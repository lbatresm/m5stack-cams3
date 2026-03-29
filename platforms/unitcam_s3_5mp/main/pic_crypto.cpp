#include "pic_crypto.hpp"
#include <cstring>
#include <esp_log.h>
#include <esp_random.h>
#include <sdkconfig.h>

#include "mbedtls/gcm.h"
#include "mbedtls/md.h"
#include "mbedtls/pkcs5.h"

static const char* TAG = "pic_crypto";

static constexpr size_t kMagicLen = 8;
static const char kMagic[kMagicLen] = {'U', 'C', 'J', 'F', '0', '0', '0', '1'};
static constexpr size_t kIvLen = 12;
static constexpr size_t kTagLen = 16;
static constexpr size_t kKeyLen = 32;
static constexpr unsigned kPbkdf2Iterations = 10000;

// Must match tools/decrypt_ucam.py (PBKDF2 salt; not secret).
static const uint8_t kPbkdf2Salt[16] = {'U', 'n', 'i', 't', 'C', 'a', 'm', 'S',
                                        'd', 'E', 'n', 'c', 'V', '1', '!', 'x'};

static bool s_enabled = false;
static uint8_t s_key[kKeyLen];
static bool s_key_ready = false;

bool pic_crypto_init()
{
    static bool s_done = false;
    if (s_done)
        return s_enabled;
    s_done = true;

    s_enabled = false;
    s_key_ready = false;

#ifdef CONFIG_PICSAVER_ENCRYPTION_PASSWORD
    const char* pw = CONFIG_PICSAVER_ENCRYPTION_PASSWORD;
#else
    const char* pw = "";
#endif
    if (pw == nullptr || pw[0] == '\0')
    {
        ESP_LOGI(TAG, "encryption off (no passphrase in menuconfig)");
        return false;
    }

    const size_t pw_len = std::strlen(pw);
    // mbedtls 3.x: mbedtls_pkcs5_pbkdf2_hmac removed when MBEDTLS_DEPRECATED_REMOVED=y
    const int ret =
        mbedtls_pkcs5_pbkdf2_hmac_ext(MBEDTLS_MD_SHA256, reinterpret_cast<const unsigned char*>(pw), pw_len, kPbkdf2Salt,
                                      sizeof(kPbkdf2Salt), kPbkdf2Iterations, kKeyLen, s_key);
    if (ret != 0)
    {
        ESP_LOGE(TAG, "PBKDF2 failed: -0x%x", static_cast<unsigned>(-ret));
        return false;
    }

    s_key_ready = true;
    s_enabled = true;
    ESP_LOGI(TAG, "encryption on (AES-256-GCM, .ucam)");
    return true;
}

bool pic_crypto_is_enabled()
{
    return s_enabled && s_key_ready;
}

bool pic_crypto_encrypt_jpeg(const uint8_t* plain, size_t plain_len, std::vector<uint8_t>& out)
{
    if (!pic_crypto_is_enabled() || plain == nullptr || plain_len == 0)
        return false;

    uint8_t iv[kIvLen];
    esp_fill_random(iv, sizeof(iv));

    std::vector<uint8_t> ct(plain_len);
    uint8_t tag[kTagLen];

    mbedtls_gcm_context ctx;
    mbedtls_gcm_init(&ctx);

    int err = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, s_key, kKeyLen * 8);
    if (err != 0)
    {
        ESP_LOGE(TAG, "gcm_setkey -0x%x", static_cast<unsigned>(-err));
        mbedtls_gcm_free(&ctx);
        return false;
    }

    err = mbedtls_gcm_crypt_and_tag(&ctx, MBEDTLS_GCM_ENCRYPT, plain_len, iv, kIvLen, nullptr, 0, plain, ct.data(),
                                    kTagLen, tag);
    mbedtls_gcm_free(&ctx);

    if (err != 0)
    {
        ESP_LOGE(TAG, "gcm_crypt -0x%x", static_cast<unsigned>(-err));
        return false;
    }

    const size_t total = kMagicLen + kIvLen + 4 + plain_len + kTagLen;
    out.clear();
    out.resize(total);
    size_t o = 0;
    std::memcpy(out.data() + o, kMagic, kMagicLen);
    o += kMagicLen;
    std::memcpy(out.data() + o, iv, kIvLen);
    o += kIvLen;
    const uint32_t len_le = static_cast<uint32_t>(plain_len);
    std::memcpy(out.data() + o, &len_le, sizeof(len_le));
    o += 4;
    std::memcpy(out.data() + o, ct.data(), plain_len);
    o += plain_len;
    std::memcpy(out.data() + o, tag, kTagLen);

    return true;
}

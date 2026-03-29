#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

/** Call once at startup. Reads CONFIG_PICSAVER_ENCRYPTION_PASSWORD. */
bool pic_crypto_init();

bool pic_crypto_is_enabled();

/** AES-256-GCM encrypt JPEG bytes into out (magic + IV + len + ciphertext + tag). */
bool pic_crypto_encrypt_jpeg(const uint8_t* plain, size_t plain_len, std::vector<uint8_t>& out);

# M5Stack UnitCam S3 5MP — SD capture

Firmware for the **M5Stack UnitCam S3 (5MP)** that saves **one JPEG per second** to the microSD card while power is on. Includes chip overheating protection.

**Where files go:** `simple_picsaver/` on the FAT volume (full path on device: `/sdcard/simple_picsaver/`). Use a **FAT32** card. You can optionally **encrypt** each JPEG (see below); files are then named `*.ucam`.

**Names:** uptime since power-on, `HHHH_MM_SS.jpg` (or `.ucam`): hours, minutes within that hour, seconds (so names stay unique at one photo per second). Resets after each reboot.

**Feedback:** the LED **toggles** each time a frame is written successfully.

**Pins:** `platforms/unitcam_s3_5mp/main/board_pins.hpp`

---

## Prerequisites

- [ESP-IDF **5.1.x**](https://docs.espressif.com/projects/esp-idf/en/v5.1/esp32s3/index.html) installed and environment set (`idf.py` works in a terminal).

---

## First-time setup

1. Clone this repository.

2. Install the Arduino core into the project (required for the build):

   ```bash
   python fetch_repos.py
   ```

   That clones the revision listed in `repos.json` into `platforms/unitcam_s3_5mp/components/arduino-esp32`.

3. The **esp32-camera** component is already under `platforms/unitcam_s3_5mp/components/esp32-camera`.

---

## Build and flash

```bash
cd platforms/unitcam_s3_5mp
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

Replace `PORT` with your serial port (for example `COM9` on Windows or `/dev/ttyUSB0` on Linux).

If you change partitions or hit odd CMake cache issues:

```bash
idf.py fullclean
idf.py build
```

---

## Encrypted JPEGs (optional)

1. **Set the passphrase in menuconfig** (same value you will use on the PC):

   ```bash
   cd platforms/unitcam_s3_5mp
   idf.py menuconfig
   ```

   Open **Picsaver (SD JPEG)** and set **Passphrase for AES-256-GCM file encryption** to a non-empty string. Leave it empty to save normal `.jpg` files.

2. **Rebuild and flash** (`idf.py build` then `idf.py -p PORT flash`).

3. On the SD card you will see files like `20260101_120000_123.ucam` instead of `.jpg`.

4. **Decrypt on your computer** (Python 3):

   ```bash
   pip install pycryptodome
   python tools/decrypt_ucam.py path/to/file.ucam -p "YOUR_PASSPHRASE"
   ```

   One `.jpg` is written next to each `.ucam` (or `python tools/decrypt_ucam.py path/to/folder -p "..."` for every `*.ucam`; optional `-o other_folder` for outputs).

The firmware uses **PBKDF2-HMAC-SHA256** (10000 iterations) and **AES-256-GCM**. The passphrase is stored in **flash and `sdkconfig`**; anyone with the firmware image or full flash dump could recover it. For stronger protection, use Espressif **flash encryption** / secure boot and treat this as protection against casual SD access only.

# UnitCam S3 5MP — SD capture

Firmware for the **M5Stack UnitCam S3 (5MP)** that saves **one JPEG per second** to the microSD card while power is on. Includes chip overheating protection.

**Where files go:** `simple_picsaver/` on the FAT volume (full path on device: `/sdcard/simple_picsaver/`). Use a **FAT32** card.

**Feedback:** the LED **toggles** each time a frame is written successfully.

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
# ESP32‑S3 Firmware Flash & Build Upload Guide

This guide documents how to flash the *isf_canbus_gateway* firmware onto an ESP32‑S3 using `esptool`. It explains every piece of the command and what to expect in the console output.

## Prerequisites

* **Hardware**: ESP32‑S3 development board with USB‑to‑UART, USB‑C cable.
* **Software**:

  * `esptool.py` ≥ 4.8 **installed on your PATH** — install with `pip install esptool` or add the Arduino core’s `esptool_py` folder to your `%PATH%`. You can still call the full `.exe` path if you prefer, but keeping it on PATH makes scripts portable.
  * Arduino IDE (or CLI) that generated the `.bin` artefacts.
  * USB driver for the board (CP210x/CH34x).
* **Host**: Windows 10/11 (paths below are from `%LOCALAPPDATA%`).

[Detailed Installation] (<https://docs.espressif.com/projects/esptool/en/latest/esp32/installation.html>)

## Build artefacts

| Address   | File                                    | Purpose                                                |
| --------- | --------------------------------------- | ------------------------------------------------------ |
| `0x0000`  | `isf_canbus_gateway.ino.bootloader.bin` | Second‑stage bootloader compiled for this project.     |
| `0x8000`  | `isf_canbus_gateway.ino.partitions.bin` | Partition table defining flash layout.                 |
| `0xE000`  | `boot_app0.bin`                         | Small stub that verifies and launches the application. |
| `0x10000` | `isf_canbus_gateway.ino.bin`            | Your application firmware image.                       |

> **Why these offsets?**
> They match the “Default 4 MB with spiffs” partition scheme used by the Arduino ESP32 core. If you changed the partition CSV, regenerate & flash at different offsets.

## Flash command

```bash
esptool.py \
  --chip esp32s3           # Target chip family  
  --port COM7              # Serial port connected to the board  
  --baud 921600            # Upload speed (auto‑reduces if unstable)  
  --before default_reset   # GPIO0 reset dance before flashing  
  --after  hard_reset      # Hard reset once flashing is done  

  write_flash              # esptool sub‑command  
  -z                       # Compress data on the fly  
  --flash_mode keep        # Keep the mode read from the module (QIO/QOUT/DIO)  
  --flash_freq keep        # Keep the detected crystal frequency (40 MHz)  
  --flash_size keep        # Autodetect flash size  

  0x0      "<bootloader.bin>"   \
  0x8000   "<partitions.bin>"   \
  0xE000   "<boot_app0.bin>"    \
  0x10000  "<app.bin>"
```

### How to run it

1. Plug the board while holding **BOOT** if it lacks auto‑boot circuitry.
2. Ensure no other program (Serial Monitor) is occupying `COM7`.
3. Paste the full command into *PowerShell* or *cmd*.
4. Observe progress bars reach **100 %** for each segment.
5. When you see `Hard resetting with RTC WDT…` press *EN/RST* or wait; the app should start.

## What the console output means

| Section                         | Explanation                                               |
| ------------------------------- | --------------------------------------------------------- |
| *Connecting…*                   | esptool toggles DTR/RTS to enter bootloader.              |
| *Chip is ESP32‑S3…*             | ID read from EFUSE; confirms you picked the right chip.   |
| *Uploading stub / Running stub* | A tiny helper stub is loaded to RAM to speed up flashing. |
| *Changing baud rate…*           | Stub switches to 921 600 bps for faster upload.           |
| *Writing at 0x…*                | Compressed chunks being written to flash.                 |
| *Hash of data verified.*        | CRC check verifies integrity of each segment.             |
| *Hard resetting…*               | esptool toggles EN pin so the new firmware boots.         |

A complete successful session looks like:

```text
(esptool.py v4.8.1)
Serial port COM7
Connecting...
Chip is ESP32-S3 (QFN56) (revision v0.2)
Features: WiFi, BLE, Embedded PSRAM 8MB (AP_3v3)
Crystal is 40MHz
MAC: 74:4d:bd:88:e9:b8
Uploading stub...
Running stub...
Stub running...
Changing baud rate to 921600
Changed.
Configuring flash size...
Flash will be erased from 0x00000000 to 0x00004fff...
Flash will be erased from 0x00008000 to 0x00008fff...
Flash will be erased from 0x0000e000 to 0x0000ffff...
Flash will be erased from 0x00010000 to 0x0007cfff...
Compressed 20208 bytes to 13058...
Writing at 0x00000000... (100 %)
...
Hard resetting with RTC WDT...
```
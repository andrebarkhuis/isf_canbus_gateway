# Deployment Guide: flashing and monitoring

## Connect

Plug the ESP32-CAN-X2 in over USB-C. The ESP32-S3's native USB Serial/JTAG port appears as a COM port
(Windows: *USB Serial Device*, VID 303A PID 1001; on the bench this was COM10). No driver install is needed.
Bluetooth serial ports (COM5, COM6 on the bench) are not the board.

## Flash

```bash
.venv\Scripts\pio run -t upload
```

PlatformIO auto-detects the port. With more than one board attached, set `upload_port` in `platformio.ini` or
pass it on the command line:

```bash
.venv\Scripts\pio run -t upload --upload-port COM10
```

If the board does not enter the bootloader by itself, hold **BOOT**, tap **RESET**, release **BOOT**, then upload.

## Monitor

```bash
.venv\Scripts\pio device monitor            # 115200 baud; Ctrl+C to leave
.venv\Scripts\pio device monitor > gateway.log
```

What the log looks like:

```
[I][initialize] TWAI (ISF bus) started on TX 7 / RX 6 at 500 kbit/s
[I][initialize] MCP2515 (GT86 bus) started at 500 kbit/s, 23 output frames
(12.345678) isf 7E0#0221000000000000
(12.410112) isf 7E8#066100BC00000100
[I][discover] Engine @ 7E0: present, 36 supported parameter address(es)
[I][interpret] Engine: map 21 01: 26/29 present
```

- `[E]`, `[W]`, `[I]`, `[D]` lines are the text log (level set in `main.cpp`, `Logger::begin`).
- `(seconds.micros) isf <id>#<data>` lines are every frame on the IS F bus in candump format, flow controls
  included. Save the log and replay it on a PC with the reference implementation:

```bash
dotnet run --project C:\projects\ToyotaLens\src\ToyotaLens -- decode gateway.log --last
```

ToyotaLens skips the text lines and decodes the frames exactly as it decodes its own recordings, so a gateway
session can be checked parameter by parameter against the car.

## Status words to look for

| Line | Meaning |
|---|---|
| `Engine @ 7E0: absent (no reply to 21 00)` | Ignition off, or the IS F bus is not connected. Probed again every 3 s. |
| `Engine @ 7E0: present, N supported parameter address(es)` | Discovery done; maps follow. |
| `21 01: no reply` | A present module went silent; it is re-discovered. |
| `ISF bus: bus off, starting recovery` | A real bus fault (wrong speed, missing termination, wiring). |
| `MCP2515 (GT86 bus) failed to start` | The SPI link to CAN 2 is not answering; check the board. |
| `GT86 bus: send 0xD1 (...) failed` | The MCP2515 could not transmit: nobody acknowledging on the GT86 bus. |

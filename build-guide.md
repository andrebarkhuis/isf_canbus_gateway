# Build Guide

The gateway is an ESP-IDF application built with PlatformIO. There is no Arduino framework and no Arduino IDE in
the loop.

## Prerequisites

| Requirement | Notes |
|---|---|
| Python 3.10 or newer | PlatformIO is installed into the repository's own virtual environment. |
| PlatformIO Core | `python -m venv .venv` then `.venv\Scripts\pip install -r requirements.txt`. The first `pio run` downloads the `espressif32` platform, ESP-IDF and the Xtensa toolchain into `%USERPROFILE%\.platformio` (several GB, once). |
| Docker Desktop (optional) | Only for the host self-test, which compiles the protocol code with gcc in a container. |

## Compile

```bash
.venv\Scripts\pio run
```

Configuration lives in three files:

- `platformio.ini` — environment `esp32-can-x2`: board `esp32-s3-devkitc-1`, `framework = espidf`, DIO flash, 4 MB.
- `sdkconfig.defaults` — console on the native USB Serial/JTAG port, 1 ms FreeRTOS tick, no C++ exceptions,
  custom partition table.
- `partitions.csv` — one factory app partition of 3 MB.

PlatformIO writes the merged configuration to `sdkconfig.esp32-can-x2` (ignored by git). To change a setting,
edit `sdkconfig.defaults`, delete `sdkconfig.esp32-can-x2` and build again.

The log level and the candump frame trace are not configuration: they are the one line
`Logger::begin(LOG_LEVEL_INFO, true)` in `src/main.cpp` (see `deployment-guide.md`, *Log level and frame trace*).

Output: `.pio/build/esp32-can-x2/firmware.bin` (plus `bootloader.bin` and `partitions.bin`).

## Regenerate the tables after editing the frame specification

```bash
python tools/generate.py
```

Reads `tools/gateway_frames.json` and `C:\projects\ToyotaLens\data\{engine,ect,abs}.json` (path in the JSON,
override with `--toyotalens <dir>`), and writes:

| File | Content |
|---|---|
| `src/uds/uds_definitions.cpp` | the parameter definitions the gateway polls and decodes |
| `src/translator_table.cpp` | the output frames and their signals |
| `gt86-bus.dbc`, `isf-bus.dbc` | one DBC per bus, for SavvyCAN and as an ADU fallback |
| `gt86-bus.canx`, `isf-bus.canx` | the same buses as ECUmaster CANX streams, what the ADU imports |
| `docs/adu-frames.md` | the frame and channel reference for the ADU, in ToyotaLens units |

It prints the frame count, the signal count and any scaling it had to approximate for the ADU's 16-bit fields.
It refuses overlapping signals, unknown uniqueIds, more than 8 channels in a frame, and signals that do not fit a
frame. None of the generated files is edited by hand; after a change `git status` shows exactly these files.

## Host self-test

```bash
bash test/host/run.sh        # add -v to see the session log and every frame
```

Compiles `src/isotp`, `src/uds`, `src/message_translator`, `src/isf_broadcast`, `src/services/isf_service` and
the generated tables with gcc 14 in a Docker container, against a scripted CAN bus (`test/host/fake_bus.*`), and
runs ToyotaLens's acceptance vectors: the specification's §1.6 frame examples, the ISO-TP channel cases (flow
control, N_Bs, N_Cr, P2, response pending, sequence errors, the id filter), the §6.8 decode examples, the session
interpreter, the translator, and an end-to-end discover-and-read of a scripted engine ECU. It ends with
`RESULT: PASS` or `RESULT: FAIL`.

With `-v` the test runs the same logger as the firmware at debug level with the frame trace on, so the lines are
the ones listed in `deployment-guide.md` and a protocol change can be read in the log before it is flashed.

## Clean build

```bash
.venv\Scripts\pio run -t clean
```

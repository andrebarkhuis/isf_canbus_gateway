# GT86-ISF CAN Gateway

A gateway between the Lexus IS F powertrain bus and the GT86 chassis bus on an Autosport Labs ESP32-CAN-X2.
It reads the IS F's Engine, ECT (transmission) and ABS/VSC/TRAC modules over ISO-TP the way Techstream does
(Toyota Mode 21), and puts what it decodes on the second bus as single CAN frames: the packets the GT86 modules
need, and dense frames for the ECUmaster ADU.

```
IS F bus (TWAI) --ISO-TP / Mode 21--> decode --> pack --> GT86 bus (MCP2515): GT86 packets + ADU frames
                                                       \--> IS F bus (TWAI): ADU frames (29-bit ids)
```

The ISF side is a file-for-file port of **ToyotaLens** (`C:\projects\ToyotaLens`), the reference implementation
verified on the car in October 2026. Nothing about the protocol is designed here; see
[docs/toyotalens-port.md](docs/toyotalens-port.md) for which file came from where. The parameter definitions are
ToyotaLens's `data/{engine,ect,abs}.json`, pulled in by a generator, never hand-edited.

## Hardware

- **Board:** [ESP32-CAN-X2](https://wiki.autosportlabs.com/ESP32-CAN-X2) (ESP32-S3-WROOM-1), USB-C for power,
  flashing and the serial log (the S3's native USB Serial/JTAG port, VID 303A).
- **CAN 1 (IS F bus):** the ESP32's TWAI controller, TX GPIO 7 / RX GPIO 6, 500 kbit/s.
- **CAN 2 (GT86 bus):** MCP2515 on SPI2, SCK 12 / MISO 13 / MOSI 11 / CS 10, 16 MHz crystal, 500 kbit/s.
- Termination is on by default on both channels (TERM1 / TERM2 pads on the back).

## What it does

1. **Discover** each IS F module (requirements R3, R4 of ToyotaLens): probe with `21 00`, walk the support bitmaps
   `21 00 / 20 / 40 …`, and fetch the response data map `A8 11 <id>` for every block the gateway polls. A silent
   module is probed again every 3 s, so the gateway can be powered before the ignition.
2. **Poll** the blocks that carry forwarded parameters: engine `21 01` and ABS `21 03` every cycle (rpm, speed,
   wheel speeds, coolant), the other blocks one per cycle in turn. In addition, the GT86 speed, wheel speed and
   rpm frames are fed from the IS F modules' own broadcast frames (0x0B0, 0x0B2, 0x0B4, 0x2C4) the moment they
   arrive, so they do not wait for a poll (`docs/output-frames.md` §2.1).
3. **Decode** every parameter of a block exactly as ToyotaLens does (data-map expansion, masked big-endian read,
   sign, shift, scale, unit map).
4. **Translate** into output frames (see [docs/output-frames.md](docs/output-frames.md)) and hand them to the
   GT86 task through a queue.
5. **Keep the GT86 bus alive**: every output frame is repeated at its interval; GT86 packets start from the bytes
   of a capture in which the cluster showed no errors and are overwritten by translated values; a frame that has
   had no fresh value for 5 s falls back to its defaults.

Every frame sent or received on the IS F bus is written to the serial log in candump format
(`(seconds.micros) isf 7E8#0661008400000100`), flow controls included, so a captured log replays with
`ToyotaLens decode <log>` and can be compared with the reference captures.

## Directory layout

```text
isf_canbus_gateway/
├── platformio.ini, sdkconfig.defaults, partitions.csv, CMakeLists.txt   PlatformIO + ESP-IDF project
├── src/
│   ├── main.cpp                 app_main: the two tasks and the queue between them
│   ├── common.h/.cpp            CANMessage, clock
│   ├── can/twai_wrapper         IS F bus: one frame in, one frame out (ToyotaLens ICanTransport)
│   ├── isotp/iso_tp             ISO 15765-2: framing, reassembly, flow control, timers (ToyotaLens IsoTp*)
│   ├── uds/uds_mapper           Mode 21 services, NRCs, bitmaps, data maps, decoder, unit map (ToyotaLens Protocol/)
│   ├── uds/uds_definitions.cpp  GENERATED: the parameters forwarded, from ToyotaLens data/*.json
│   ├── services/isf_service     the Mode 21 session: discover, read, interpret (ToyotaLens EcuSession)
│   ├── message_translator       decoded values -> output frames
│   ├── translator_table.cpp     GENERATED: output frames and signals
│   ├── services/gt86_service    GT86 bus: periodic frames, translated overrides, fallback
│   ├── mcp_can/                 MCP2515 driver on the ESP-IDF SPI master
│   └── logger/                  text log and the candump frame trace
├── tools/gateway_frames.json    the output-frame specification (edit this)
├── tools/generate.py            writes uds_definitions.cpp, translator_table.cpp, the DBC and the CANX files
├── gt86-bus.dbc                 GENERATED: what the gateway sends on the GT86 bus, for the ADU and SavvyCAN
├── isf-bus.dbc                  GENERATED: the IS F broadcasts the gateway decodes + the ADU frames repeated there
├── gt86-bus.canx                GENERATED: the GT86 bus as an ECUmaster CANX stream (what the ADU imports)
├── isf-bus.canx                 GENERATED: the IS F bus as an ECUmaster CANX stream
├── test/host/                   self-test on a host compiler (ToyotaLens's vectors), run in Docker
└── docs/                        output frames, port map, reference material
```

## Build, flash, test

See [build-guide.md](build-guide.md) and [deployment-guide.md](deployment-guide.md). In short:

```bash
python -m venv .venv && .venv\Scripts\pip install -r requirements.txt   # once
.venv\Scripts\pio run                      # compile
.venv\Scripts\pio run -t upload            # flash over USB
.venv\Scripts\pio device monitor           # serial log, 115200
bash test/host/run.sh                      # host self-test in a gcc container (needs Docker)
python tools/generate.py                   # after editing tools/gateway_frames.json
```

## IS F modules

| Module | Request id | Response id | Blocks polled |
|---|---|---|---|
| Engine (ECM) | `0x7E0` | `0x7E8` | `01` every cycle; `03 05 06 25 51` in turn |
| ECT (transmission) | `0x7E1` | `0x7E9` | `82 85` in turn |
| ABS/VSC/TRAC | `0x7B0` | `0x7B8` | `03` every cycle; `05 06 1C 1D 1F 21 5A` in turn |

The exact set follows from `tools/gateway_frames.json`: a block is polled when a forwarded parameter lives in it
and the module's support bitmap lists it. Timers: P2 700 ms for the probe, 2000 ms for data; N_Bs and N_Cr
1000 ms; P2* 5000 ms after `7F xx 78`. These are ToyotaLens's values (specification §8.1).

## References

- `C:\projects\ToyotaLens` — the reference implementation; `docs/techstream-J2534-protocol.md` is the
  specification (cited as §n in the code), `docs/requirements.md` the device requirements.
- `C:\projects\gt86-isf-swap\logs\GT86` — captures of the GT86 bus the output frames were taken from.
- `docs/Decoding Techstream DTC canbus messages.md` — notes on IS F DTC frames (not used by the gateway).

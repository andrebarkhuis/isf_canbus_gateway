# Deployment Guide: flashing, monitoring and debugging

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
The upload ends with `Hard resetting via RTS pin`; the firmware starts on its own and the first log lines appear a
second later.

## Monitor

```bash
.venv\Scripts\pio device monitor            # 115200 baud; Ctrl+C to leave
.venv\Scripts\pio device monitor > gateway.log
```

What the log looks like:

```
----------------------------------------------------
Logger initialized (level 3, frame trace on)
----------------------------------------------------
[I][app_main] Starting GT86-ISF CAN gateway
[I][isfTask] ISF task on core 0
[I][gt86Task] GT86 task on core 1
[I][initialize] TWAI (ISF bus) started on TX 7 / RX 6 at 500 kbit/s
[I][initialize] ISF session ready: 249 parameter definitions, 23 output frames
[I][initialize] MCP2515 (GT86 bus) started at 500 kbit/s, 23 output frames
(12.345678) isf 7E0#0221000000000000
(12.410112) isf 7E8#066100BC00000100
[I][discover] Engine @ 7E0: present, 36 supported parameter address(es)
[I][interpret] Engine: map 21 01: 26/29 present
[I][discover] Engine: polling 6 block(s), data maps: 6/6
(12.902331) isf 1F0005F0#1A00000000000000
```

Two kinds of line:

- `[E]`, `[W]`, `[I]`, `[D]` lines are the text log. The letter is the level, the bracketed word the function that
  wrote the line.
- `(seconds.micros) isf <id>#<data>` lines are the frame trace: every frame sent or received on the IS F bus in
  candump format, flow controls included. The timestamp is seconds since boot. An 11-bit id has 3 hex digits,
  a 29-bit id 8, so the ADU frames the gateway repeats on the IS F bus show as `isf 1F0005F0#...`. The GT86 bus
  is not traced (see *Watching the GT86 bus* below).

Save the log and replay it on a PC with the reference implementation:

```bash
dotnet run --project C:\projects\ToyotaLens\src\ToyotaLens -- decode gateway.log --last
```

ToyotaLens skips the text lines and decodes the frames exactly as it decodes its own recordings, so a gateway
session can be checked parameter by parameter against the car.

### Log level and frame trace

Both are fixed at compile time in `src/main.cpp`:

```cpp
Logger::begin(LOG_LEVEL_INFO, true);   // level, frame trace
```

| Setting | Effect |
|---|---|
| `LOG_LEVEL_ERROR` | Only faults: driver start failures, bus off, a task that died. |
| `LOG_LEVEL_WARN` | Adds timeouts, abandoned requests, bus error counters, dropped frames, failed sends. |
| `LOG_LEVEL_INFO` (default) | Adds discovery, maps, negative responses, response-pending waits, bus recovery. |
| `LOG_LEVEL_DEBUG` | Adds one line per decoded parameter (`Engine 21 01 EngineSpeed = 812 rpm (raw 3248)`), one per decoded broadcast (`broadcast 2C4 EngineSpeed = 812 rpm (raw 812)`), a summary per block (`Engine: 21 01: 29 bytes, 26/26 parameters decoded`). Several hundred lines a second; for the bench, not for recording a drive. |
| frame trace `true` | The candump lines. Needed for `ToyotaLens decode`. |
| frame trace `false` | Text lines only, for watching discovery and warnings without the flood. |

Change the line, build and flash. The host self-test uses the same logger: `bash test/host/run.sh -v` runs it at
debug level with the trace on.

## Every line the firmware can print

### Start-up

| Line | Meaning |
|---|---|
| `Failed to install TWAI driver: N` / `Failed to start TWAI driver: N` | ESP-IDF refused the CAN controller (N is an `esp_err_t`). Does not happen on a healthy board. |
| `ISF bus failed to open` then `ISF service failed to initialize; retrying` | Follows the two above; retried every second. |
| `TWAI (ISF bus) started on TX 7 / RX 6 at 500 kbit/s` | IS F side up. |
| `ISF session ready: N parameter definitions, M output frames` | The generated tables loaded; N and M are what `tools/generate.py` reported. |
| `spi_bus_initialize failed` / `spi_bus_add_device failed` / `MCP2515 init failed (no answer on SPI?)` / `MCP2515 (GT86 bus) failed to start: N` then `GT86 service failed to initialize; retrying` | The MCP2515 on CAN 2 does not answer over SPI. Hardware; retried every second. |
| `MCP2515 (GT86 bus) started at 500 kbit/s, M output frames` | GT86 side up; the M frames go out at their intervals from this moment, with their default bytes. |
| `Failed to create the message queue` | Out of memory at boot; never seen. |

### Discovery and polling (one line per module: `Engine`, `ECT`, `ABS/VSC/TRAC`)

| Line | Meaning |
|---|---|
| `Engine @ 7E0: absent (no reply to 21 00)` | No answer within 700 ms. Ignition off, module not on this bus, or the bus is not connected. Probed again every 3 s, for ever. |
| `Engine @ 7E0: present, N supported parameter address(es)` | The probe answered and the support bitmaps were walked. |
| `Engine: bitmap 21 20 -> 12 ids (more)` | One support bitmap decoded; `(more)` means the next one is fetched. |
| `Engine: map 21 01: 26/29 present` | The response data map (`A8 11 01`) of a polled block: 26 of the 29 parameters the gateway knows in that block are implemented on this car. |
| `Engine: 21 05 map expects 20 bytes, got 18; decoded raw layout` | The block reply is shorter than its data map says; the raw layout from the definitions was used. Report it if it repeats. |
| `Engine: polling N block(s), data maps: M/N` | Discovery done. M < N means a block has no data map and is decoded from the raw layout. |
| `Engine: negative response to 21: NRC 12 sub-function not supported (parameter address 1D)` | The module refused a request. `7F 21 12` on a polled block drops it from the poll list (specification §8.5); other NRCs are informational. |
| `Engine: 21 01: no reply` | A present module went silent for 2000 ms. It is marked absent and re-discovered from the probe; the car was probably switched off. |

### ISO-TP (the id printed is the module's response id)

| Line | Meaning |
|---|---|
| `7E8: response pending (7F 21 78); waiting up to P2* 5000 ms` | Normal for slow blocks; the reply follows. |
| `7E8: N response-pending frames (7F 21 78); giving up, treated as no reply` | The module never delivered. Followed by `no reply`. |
| `7E8: no flow control within N_Bs 1000 ms; request abandoned` | A multi-frame request got no flow control. Only `A8 11 xx` and longer requests can hit this. |
| `7E8: N wait flow controls; request abandoned` / `7E8: flow control status X (overflow); request abandoned` | The module asked us to wait too often or rejected the length. Not seen on the IS F. |
| `7E8: next consecutive frame did not arrive within N_Cr 1000 ms; reply abandoned` | A multi-frame reply stopped half way. A dropped frame (see *receive queue full*), bus errors, or the module was switched off mid-reply. |
| `7E8: consecutive frame out of sequence (2X); reply abandoned` | A frame of the reply was lost. Same causes. |
| `ISO-TP message too long` / `target() before request()` / `ISF bus not open` | Programming errors; never expected. |

### IS F bus health (TWAI alerts, checked every cycle)

| Line | Meaning |
|---|---|
| `twai_transmit 7E0 failed: N` | The controller refused to queue a frame. `N` 263 is `ESP_ERR_TIMEOUT`: the 16-frame transmit queue stayed full for N_As (1000 ms) because nothing on the bus acknowledges our frames. The alert lines that follow say why. |
| `ISF bus: N bus error alert(s) in the last T ms; state S tec A rec B bus_err C arb_lost D tx_fail E rx_miss F rx_ovr G tx_q H rx_q I` | At most one a second while errors happen. `state` 1 is running, 2 bus off, 3 recovering. `tec`/`rec` are the controller's transmit and receive error counters (0 on a clean bus; 128 is error passive, 256 bus off). `bus_err` counts bit, stuff, CRC, form and ack errors since start. `rx_miss` and `rx_ovr` mean frames were lost on our side. |
| `ISF bus: error passive` | `tec` or `rec` passed 127: the controller still receives but no longer actively flags errors. With nothing connected this is expected once (our probes get no acknowledgement); on a live bus it means termination, wiring or a swapped pair. |
| `ISF bus: bus off, starting recovery` then `ISF bus: recovered, restarting` | `tec` passed 255. Recovery needs 128 × 11 idle bits, so it needs a working bus; if the pair repeats, the fault is still there. |
| `ISF bus: receive queue full, frames lost` | The IS F task fell behind the bus (the controller's receive queue holds 64 frames). Followed by N_Cr or out-of-sequence warnings. Happens when the console is slow (frame trace on, a terminal that cannot keep up) or the task is blocked. |
| `ISF bus: transmit failed` | Same cause as `twai_transmit failed`, reported from the alert side. |

### GT86 bus and the hand-over between the tasks

| Line | Meaning |
|---|---|
| `GT86 bus: send 0D1 (GT86_VehicleSpeed) failed` | The MCP2515 could not get the frame out: no acknowledgement on the GT86 bus (nothing connected, wrong bit rate, termination) and its three transmit buffers are full because of that. One line per frame per interval, so the log fills quickly. |
| `message queue full, frame 1F0005F0 dropped` | The IS F task produced translations faster than the GT86 task took them (queue of 64). Only when the GT86 task is stuck in failed sends. |
| `MCP2515 SPI transfer failed: N` | SPI link lost at run time. Hardware. |

### Supervisor

| Line | Meaning |
|---|---|
| `ISF task ended; restarting it` / `GT86 task ended; restarting it` | A task returned or was deleted (a stack overflow or a crash resets the board instead and prints an ESP-IDF backtrace before the logger banner). Checked every second. |

## First power-up checklist

1. **USB only, nothing on the CAN connectors.** Expect the start-up block above, then `absent` for all three modules
   every 3 s, `GT86 bus: send ... failed` for every frame (nothing acknowledges), and on the IS F side
   `twai_transmit 7E0 failed: 263` with one `error passive`. No `bus error alert` lines beyond that: an empty bus is
   quiet.
2. **IS F bus connected, ignition on.** `Engine @ 7E0: present`, `ECT @ 7E1: present`, `ABS/VSC/TRAC @ 7B0: present`
   within a few seconds of each other, their maps, then a steady stream of `isf 7E0#...`/`isf 7E8#...` pairs with
   no `[W]` lines. The 29-bit `isf 1F0005Ex#...` lines show the ADU frames being repeated. If instead bus error
   alerts appear, read *Termination* below before anything else.
3. **GT86 bus connected.** `send ... failed` stops. The GT86 cluster shows no warning lamps with the IS F ignition off
   (default bytes) and shows live rpm and speed with it on.
4. **Replay.** Save a minute of log and run `ToyotaLens decode gateway.log --last`; every parameter the gateway
   forwards should decode to the values the car showed.

## Watching the GT86 bus

The GT86 side is not traced on the console. Watch it with a second adapter on the GT86 bus: a CANable or any
slcan device on a COM port, in SavvyCAN (connection type *Serial*, 500 kbit/s, listen only) or from the shell with
the `savvycan` capture tool used on the bench (`capture port=COM8 count=700 timeout_s=8 output_csv=...`). Load
`gt86-bus.dbc` in SavvyCAN to see signal values; `isf-bus.dbc` is the file for the IS F side.

What a correct capture shows:

- All 23 frames of `docs/output-frames.md` §2 and §3 at their intervals, nothing else from the gateway. A missing
  id means the GT86 task is not running or that frame's `interval` is 0.
- With the IS F ignition off: the default bytes from the table in §2, unchanging.
- With it on: 0x0D1, 0x0D4 and 0x141 moving with the car within one send slot of the IS F broadcasts (§2.1); the
  ADU frames (29-bit 0x1F0005E0 and up, §3) carrying the polled values.
- After the IS F goes quiet: each translated frame keeps its last value for 5 s, then snaps back to its defaults
  (`Gt86Service::TranslationHoldMs`). A value that stays frozen longer means the GT86 task has stopped taking
  translations.

Compare the raw bytes of an ADU frame with the trace of the same frame on the IS F side (`isf 1F0005F0#...`); the
two are written from the same bytes.

## Troubleshooting

**A module stays `absent` with the ignition on.** Check the trace: is `isf 7E0#0221000000000000` going out every 3 s?
If the send line is missing and `twai_transmit 7E0 failed` appears, nothing acknowledges our frame: wrong bus (the
diagnostic modules sit on the 500 kbit/s chassis/powertrain bus), CAN H/L swapped, or termination. If the probe
goes out and nothing comes back while the IS F's own broadcasts (0x0B4, 0x2C4...) are in the trace, the bus is fine
and that module is not on it, or is still booting.

**`bus error alert(s)` with growing `tec`/`rec`, `error passive`, `bus off`.** The bit rate is right (500 kbit/s is
fixed in the code and is what both cars use), so this is electrical: termination, a loose ground, CAN H/L swapped
on one of several connections, or a stub that is too long. Watch `tec` against `rec`: a high `tec` with low `rec`
means our transmissions are not acknowledged (a fault on our own drop); a high `rec` means the controller sees
corrupt frames from others (termination or a noisy bus).

**Termination.** The board terminates both channels with 120 Ω by default (TERM1 / TERM2 pads on the back). A car
bus already has its two terminators at its ends; a third drops the bus to 40 Ω and produces exactly the errors
above. Open the pad of every channel that goes into a car bus. Keep a pad closed only when the other end is a
single unterminated bench adapter (a CANable on a desk has no terminator of its own, so one of the two sides must
provide one, and not both). The pad locations are on the board's wiki page (`Readme.md`, Hardware).

**`receive queue full`, N_Cr and out-of-sequence warnings in bursts.** The task is being starved, nearly always by
the console: a terminal that blocks on a full screen buffer, or `pio device monitor` redirected to a slow disk
with the frame trace on. Turn the frame trace off (above) or use a faster terminal; if the warnings disappear with
the trace off, the console was the problem.

**`GT86 bus: send ... failed` for every frame.** Nothing acknowledges on CAN 2. Check that the GT86 bus is live
(the chassis modules broadcast on their own with the ignition on; a SavvyCAN capture must show the ids of
`docs/output-frames.md` §1), the connector pinout, and termination as above. The GT86 task drains the MCP2515
receive buffers every cycle, so a bus that is live but ignores us still shows this line.

**The board resets in a loop.** The ESP-IDF panic handler prints a `Guru Meditation Error` with a backtrace before
the logger banner; the supervisor cannot catch that. Capture the backtrace and decode it with
`.venv\Scripts\pio device monitor --filter esp32_exception_decoder`.

**Checking a fix without the car.** The host self-test (`bash test/host/run.sh -v`) runs the whole IS F session
against a scripted engine ECU and prints the same log lines, so a protocol change can be read in the log before
flashing. Bench the firmware next with the IS F bus connected and the GT86 side open, then connect the GT86 bus.

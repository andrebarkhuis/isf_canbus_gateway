# The ECUmaster ADU

What is known about the ECUmaster ADU 7 (client and firmware 0.120) as far as the gateway is concerned: how
it is wired and how the gateway's files reach it. The ADU project itself is not part of this repository; the
owner builds and maintains it in the ADU client from the files described here. The manuals are in
`Ecumaster/adu_manual_en.pdf` and `Ecumaster/PMU_Manual.pdf`.

## 1. Wiring

| ADU bus | Connected to | Project folder |
|---|---|---|
| CAN1 | ECUmaster PMU | `CAN1_IN_PMU` |
| CAN2 | the IS F chassis bus (the gateway's IS F side) | `CAN2_IN_ISF` |

The ADU therefore sees the IS F modules' own broadcasts (0x0B0, 0x0B2, 0x0B4, 0x2C4, 0x3B7, 0x3BC, 0x591,
0x224 and so on) and the gateway's ADU frames, which the IS F task repeats on that bus
(`docs/output-frames.md` §3). It does not see the GT86 bus. The file to import is therefore `isf-bus.canx`;
`gt86-bus.canx` is for an ADU on the GT86 bus.

## 2. From the gateway to the ADU

`python tools/generate.py` writes two kinds of file for the ADU from `tools/gateway_frames.json`:

- **CANX** (`isf-bus.canx`, `gt86-bus.canx`): ECUmaster's own stream format. Project tree, Add, Import
  .CANX/.DBC file. It carries the integer scaling the ADU uses (multiplier, divider, offset, decimal places)
  and which built-in channel a signal feeds (`override="ecu.clt"`). Prefer this one.
- **DBC** (`isf-bus.dbc`, `gt86-bus.dbc`): for SavvyCAN, and importable by the ADU as a fallback.

What the ADU demands, and how the generator meets it:

| ADU rule | How it is met |
|---|---|
| Channel names unique across the whole project, at most 30 characters | every signal is prefixed with its frame's source token: `ENG_`, `TRANS_`, `ABS_`, `GT86_`, `ISF_` |
| At most 8 channels per frame | the generator rejects a frame with more |
| A message object receives 1, 2, 4 or 8 consecutive ids | frames are grouped by id into objects of the smallest fitting width |
| Multiplier, divider and offset are 16-bit (-32768..32767), multiplier at most 32767 | decimal places are reduced until the offset fits; a factor is approximated as a fraction (six approximations are noted in the generator output) |
| Units recognised only by exact spelling from a fixed list (°C, °, °/s, km/h, kPa, rpm, V, %, mA, g/s, us, s, A, cm^3, bar, ms, lambda, G, L) | the DBC/CANX writers spell units that way, scale acceleration to G and leave Nm, m3/s and g/rev blank |
| The DBC importer reads Windows-1252 | the DBC files are written in cp1252 |
| At most 100 CAN inputs over both buses | import selectively; the old project's inputs count |
| One source per built-in channel (`ecu.rpm`, `ecu.clt`...) | one signal per file carries each `adu` override; remove an older input that feeds the same channel first |

A message object imported from a CANX takes the object name from the file: the frame name when the object
holds one frame (`m_ENG_Status`), the source tokens and id range when it holds several
(`m_ENG_TRANS_ABS_5F0_5F7`).

### Built-in channels the gateway feeds

`"adu": "ecu.x"` on a signal in `gateway_frames.json` makes the CANX channel an override of that built-in
channel, so the ADU's stock gauges, warnings and functions use the gateway's value. Imported overrides are
conventionally named `.` in the project, so pages reference `ecu.x`, not the input.

| Built-in channel | Unit, decimals the ADU expects | Gateway source |
|---|---|---|
| ecu.rpm | rpm | IS F 0x2C4 (GT86 0x141 in gt86-bus.canx) |
| ecu.speed | km/h, 1 | IS F 0x0B4 (GT86 0xD1 in gt86-bus.canx) |
| ecu.clt, ecu.iat, ecu.oilTemp, ecu.ambientAirTemp | °C, 1 | engine 21 01 / 21 05 temperatures |
| ecu.gearboxOilTemp | °C, 1 | ECT 21 82 |
| ecu.tps | %, 1 | engine 21 01 |
| ecu.battery | V, 2 | engine 21 01 |
| ecu.ignAngle | °BTDC, 1 | engine 21 03 ignition advance |
| ecu.lambda1, ecu.lambda2, ecu.lambda1Trgt | lambda, 3 | engine 21 04 (AFR converted) |
| ecu.fuelPress | bar, 2 | engine 21 05 (kPa converted) |
| ecu.injPW | ms, 2 | engine 21 05 (us converted) |
| ecu.gear | | ECT 21 85 shift status |
| ecu.flags4.MIL | | engine 21 06 |
| ecu.flags5.ACClutch | | engine 21 25 |

The list and the expected units live in `ECU_CHANNELS` in `tools/generate.py`.

### DTC counts

Three counts come straight from the modules, nothing is calculated:

| Channel | Frame | Source |
|---|---|---|
| ENG_EmissionDTCCount | ENG_Status 0x1F0005F0 byte 0 bits 1-7 | engine 21 06 byte 0 |
| TRANS_DTCCount | TRANS_Shift_Status 0x1F0005F3 byte 1 | ECT 21 06 byte 0 |
| ABS_DTCCount | ABS_Warning_Lights 0x1F0005FD byte 1 | ABS 21 E1 byte 0 |

The 2010 IS F ABS reported 3 (C1201, C1241, C1405) while nothing else had codes. The earlier gateway built a
0x929 "diagnostic status" frame with calculated counts; nothing sends it any more.

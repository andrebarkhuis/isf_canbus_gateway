# The ECUmaster ADU and its project

What is known about the ECUmaster ADU 7 (client and firmware 0.120) as far as the gateway is concerned: how
it is wired, how the gateway's files reach it, and what is inside a project file. The project file itself,
`Ecumaster/*.adu`, is edited only by the owner in the ADU client; nothing in this repository writes it. This
page exists so that a project can be read and a load problem diagnosed without guessing. The manuals are in
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
0x929 "diagnostic status" frame with calculated counts; nothing sends it any more and a project that still
has that object reads zeros from it.

### What the project reads from the gateway (wiring of 2026-10-23)

For reference when the project is rebuilt. Values not listed come from the ADU's own built-in channels fed
by the overrides above.

| Where | Channel |
|---|---|
| Home page speed, gear | ecu.speed, ecu.gear |
| Home page oil, intake, coolant, trans oil | ecu.oilTemp, ecu.iat, ecu.clt, ecu.gearboxOilTemp |
| Home page timing advance, KCLV | ecu.ignAngle (1 dp), ENG_KnockCorrectLearn (1 dp) |
| Home page MIL lamp | ecu.flags4.MIL |
| Home page ABS lamp | a function "ABS_DTCCount > 0" (the ABS warning light itself is ABS_ABS_Light) |
| F-SPORT text | a function "ENG_SportsMode is true" |
| Engine debug: lambda target / actual | ecu.lambda1Trgt, ecu.lambda1 (2 dp) |
| Engine debug: ignition advance, knock retard | ecu.ignAngle, ENG_KnockFeedback (1 dp) |
| Engine debug: throttle, pedal | ecu.tps, ENG_AcceleratorPosition |
| Engine debug: sensor voltages | ENG_AFS_Voltage_B1S1, ENG_AFS_Voltage_B2S1, ENG_O2S_B1S2, ENG_O2S_B2S2 (3 dp) |
| Engine debug: fuel trims | ENG_ShortFT_B1/B2, ENG_LongFT_B1/B2 (1 dp) |
| Engine debug: DTC counts ECU / ABS / TCU | ENG_EmissionDTCCount, ABS_DTCCount, TRANS_DTCCount |
| DTC page trigger | any of the three counts above zero, or ecu.flags4.MIL |
| Traction / VSC off, driver switches | TRCOFF, VSC_DISABLED, VSCOFF from the IS F 0x3B7 broadcast (the project's own inputs) |

## 3. Inside a project file

A `.adu` is a zip: `project.adu` (XML) plus the page background images. Useful when a load warning has to be
traced; the client is still the tool that edits it.

```
<project version=".." clientVersion=".." firmwareVersion=".." platform="ADU" serialNumber="..">
  <variables>
    <scope id="adu">       device settings
    <scope id="general">   logging (logToMemoryChannel, logCondition, logCond2..4Channel), CAN2 terminator, GPS
    <scope id="functions"> arrays: canbusMobs, canbusInputs, analogInputs, digitalInputs, numbers, functions,
                           enumerations, timers, tables, canbusExports, dashboardPages, dashboardAlarms, groups
  <tables>
  <loggedChannels>         one <channel id="NAME" base cond2 cond3 cond4/> per logged channel (rate per profile)
```

- Every setting is `<variable id value type/>`; arrays hold `<item>`s. Channels are referenced **by name**
  everywhere (`ecu.rpm`, an input's name, a function's name, `zero`, `one`). Message objects are referenced
  **by array index** (`mobIndex` on an input), so deleting one shifts every later index. Folders are the
  `groups` array; every item's `groupId` is the 1-based position of its folder (0 = root) and a folder's own
  `groupId` is its parent. Folders are cosmetic; names must be unique project-wide regardless.
- **Message object** (`canbusMobs`): `canbusID` decimal, with flag 0x20000000 for a 29-bit id (inferred from
  one object, not confirmed in the client); `canifChannel` 0 = CAN1, 1 = CAN2; `type` 0 normal, 0xD0
  compound; `width` 0/1/2/3 = 1/2/4/8 ids; `timeout` in hundredths of a second.
- **CAN input** (`canbusInputs`): `mobIndex`, `mobOffset`, `byteOffset`, `bitCount`, `bitPosition`;
  `dataFormat` 0 U8, 1 S8, 2 U16, 3 S16, 4 U64, 5 S64; `dataEndian` 0 little, 1 big; shown value =
  (raw × multiplier / divider + offset) / 10^decimalPlaces with all three 16-bit; `overrideChannel` the
  built-in channel fed; `quantity`/`unit` client-only unit codes: rpm (3,1), ° (9,0), lambda (11,0),
  kPa (16,1), bar (16,3), % (17,0), °C (18,0), ms (19,3), km/h (20,0), V (21,0), L (22,0), harvested from
  ECUmaster's template projects and not confirmed beyond that.
- **Functions and numbers**: a `<program>` of operation items (`type`, `flags`, `channel1-3`, `const1`,
  `time1/2` in hundredths of a second, `edgeFlags`, `multiplier1-3`). A function is true when one branch is
  true, a branch when all its operations are; a number is a sum of products. `flags` bit 1 = operation in
  use, bit 2 = last of its branch (OR / + follows), bit 4 = last operation. Logic operation codes, from the
  client's string table and confirmed against a real project: 1 is true, 2 is false, 3 =, 4 ≠, 5 <, 6 ≤,
  7 >, 8 ≥, 9 and, 10 or, 11 xor, 12 flash, 13 pulse, 14 toggle, 15 set-reset latch, 16 hysteresis,
  17 changed. `const1` is in the channel's raw units (ecu.clt > 1100 means 110.0 °C). Number operation
  65 = channel or constant is the only one seen; the rest of the manual's list (constant, choose, divide,
  modulo, add, subtract, min, max, clamp, lookup) presumably follows from 66 and is unverified.
- **Pages** (`dashboardPages`): a `<layout>` of `eUIText`, `eUIRect`, `eUIGroup`, `eUIImage`, `eUIRPMBar`,
  `eUIGrid` items. A value is an `eUIText` with `variableChannel` and `decimalPlaces`; `visibleChannel`,
  `colorChannel` and the page's `activateChannel` are channel references too.

## 4. Load warnings seen and their causes

| Client message on load | Cause |
|---|---|
| `Value -102400 for field 'offset' is out of range. Clamping.` | an input's offset × 10^decimals exceeds 16 bits; the CANX now uses fewer decimal places for such signals (knock channels, sensor currents) |
| `Channel 'm_x.timeout' not found. Skipped.` | a message object was removed but its `.timeout` channel is still listed under `loggedChannels` |
| DBC import: unit warnings | a unit spelled differently from the ADU's list, or a non-cp1252 file |
| DBC import: everything lands in one object / names clash | identical signal names in different frames; the frame-token prefix fixes it |

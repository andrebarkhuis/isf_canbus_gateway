# Output frames on the GT86 bus

What the gateway puts on CAN 2 (the GT86 chassis bus, which also carries the ECUmaster ADU), where the layouts
came from, and how to change them. The machine-readable form of this document is `tools/gateway_frames.json`;
`tools/generate.py` turns it into `src/translator_table.cpp`, `src/uds/uds_definitions.cpp` and `gateway.dbc`.
Load `gateway.dbc` into the ADU configuration and into SavvyCAN.

## 1. What is on the GT86 bus already

A two-minute drive capture of the GT86 bus (`gt86-isf-swap/logs/GT86/GVRET_202305282046_gt86_2min_drive.txt`,
116 300 frames) contains 41 identifiers:

| Band | Identifiers seen |
|---|---|
| 0x000–0x0FF | 018, 0D0, 0D1, 0D2, 0D3, 0D4 |
| 0x100–0x1FF | 140, 141, 142, 144, 148, 149, 14A, 152, 156 |
| 0x200–0x2FF | 280, 282, 284 |
| 0x300–0x3FF | 360, 361, 368, 370, 372, 374, 375, 37A, 37B |
| 0x400–0x4FF | 440, 442, 44D, 46C, 4C1, 4C2, 4C3, 4C6, 4C8, 4DC, 4DD |
| 0x500–0x5FF | nothing |
| 0x600–0x6FF | 63B, 6E1, 6E2 |
| 0x700–0x7FF | 7A1, 7A9 (cluster diagnostics; the band is reserved for diagnostics) |

Gateway-specific frames use **29-bit ids from 0x1F0005E0 upward** (CAN 2.0B extended frames). A 29-bit id can
never collide with the 11-bit ids of the GT86 modules or of the IS F modules, and the top 11 bits (0x7C0) lose
every arbitration against them, so the gateway's frames never delay a module's. The ids count up from 0x1F0005E0. Both the ECUmaster ADU and PMU accept 29-bit ids (CAN 2.0 A/B
in their specifications); tick the extended-id option when creating the message objects, or import `gateway.dbc`,
which carries the extended flag.

The same eight frames are also sent on the **IS F bus** by the ISF task, at the same 100 ms interval, so an ADU
or PMU on that bus sees them too. Only the ADU frames go there; the GT86 packets never do (their ids sit in the
band the IS F modules broadcast in).

## 2. GT86 packets

Sent whether or not the IS F answers, so the GT86 modules see a complete car. The default bytes are the frames
recorded in `gt86-isf-swap/logs/GT86/cluster_working_no_errors.txt` (the cluster showed no warnings with exactly
these), except for the motion-related frames: that capture was taken at 114 km/h, and the defaults are what the
GT86 sees before the first IS F answer and whenever the IS F stops answering, so they describe a car standing
still with the engine off. 0x0D0 is wheel straight and no G, 0x0D1 is speed 0 and foot off the brake (the real
car at rest sends `00 00 13 00`), 0x0D4 is four wheel speeds of 0, and 0x141 is 0 rpm. The remaining frames are
the same at standstill as in the capture (checked against the drive capture's standstill periods). Intervals are
the rates seen in the drive capture.

| Id | Name | Default bytes | Interval | Translated signals |
|---|---|---|---|---|
| 0x0D0 | GT86_GSensor | `00 00 01 00 00 00 00 00` | 100 ms | none (wheel straight, no G) |
| 0x0D1 | GT86_VehicleSpeed | `00 00 13 00 00 00 00 00` | 100 ms | VehicleSpeed: bytes 0–1, 0.0565 km/h per bit, from the ABS broadcast 0x0B4 (§2.1) |
| 0x0D2 | GT86_Unknown_D2 | `00 00 FF FF 00 00 00 00` | 100 ms | none |
| 0x0D3 | GT86_LightStatus | `00 06 C0 0F 00 00 32 00` | 100 ms | none |
| 0x0D4 | GT86_WheelSpeeds | `00 00 00 00 00 00 00 00` | 40 ms | FL, FR, RL, RR: 16 bits each, 0.0565 km/h per bit, from the ABS broadcasts 0x0B0 / 0x0B2 (§2.1) |
| 0x140 | GT86_EngineData1 | `00 05 00 43 00 00 00 01` | 50 ms | none (bytes 2–3 did not follow engine speed in the capture) |
| 0x141 | GT86_EngineData2 | `00 00 00 00 00 80 A7 00` | 50 ms | EngineRPM: bits 32–45, 1 rpm per bit, from the engine broadcast 0x2C4 (§2.1); bits 46–47 kept |
| 0x142 | GT86_EngineMisc | `00 00 00 00 00 00 00 00` | 10 ms | none |
| 0x144 | GT86_BrakeData1 | `C0 00 58 02 01 A4 A0 00` | 50 ms | none |
| 0x360 | GT86_Temperatures | `00 00 84 50 00 00 11 00` | 50 ms | byte 2 oil temperature, byte 3 coolant temperature, both +40, from engine `21 51` pos 9 and `21 01` pos 9 |
| 0x361 | GT86_GearData2 | `00 29 00 DD 23 00 A4 34` | 50 ms | none |
| 0x370 | GT86_SteeringTorque2 | `00 00 01 01 00 03 00 00` | 50 ms | none |
| 0x4C8 | GT86_Unknown_4C8 | `09 00 00 00 00 00 00 00` | 1 s | none |
| 0x7A1 | GT86_ClusterDiagRequest | `02 19 81 00 00 00 00 00` | 1 s | none |
| 0x7A9 | GT86_ClusterDiagResponse | `02 83 00 00 00 00 00 00` | 1 s | none |

How the layouts were verified against the drive capture:

- 0x0D1 bytes 0–1 × 0.0565 ranges 0 to 76.6 km/h over the drive and is 0 at the start; the "cluster working"
  bytes (`F0 07`) decode to 114 km/h, the speed that capture was taken at, which is why the default is `00 00`
  instead (the same for the four `F0 07` wheel speeds of 0x0D4; the drive capture starts with all four at
  `00 00`).
- 0x0D4 raw wheel values peak at 1384 while 0x0D1 reads 76.6 km/h: the same 0.0565 scale (the community DBC's
  0.0157 is wrong for this car).
- 0x141 bits 32–45 idle at about 750, rise to 2314 at 60 km/h, and the "cluster working" bytes decode to 768.
  0x140 bytes 2–3 stay at one value for the whole drive, so they are not engine speed.
- 0x360 bytes 2 and 3 minus 40 climb from 20/52 °C to 52/84 °C as the engine warms.

Every frame a decoded block or broadcast writes into is handed to the GT86 bus task each time, whether or not
its bytes changed, so a constant value (engine off, rpm 0) keeps being refreshed. A translated frame falls back
to its default bytes only when no refresh has arrived for 5 s (`Gt86Service::TranslationHoldMs`), i.e. when the
IS F has gone quiet.

### 2.1 Motion signals come from the IS F broadcasts, not from polling

Speed, wheel speeds and rpm must not lag. Polled over Mode 21 they were 60 to 180 ms old on the GT86 bus (each
fast block every 56 ms, 10 to 27 ms per request, then the frame's own send slot). The IS F ABS and ECU broadcast
the same values on their own, with no request, and the gateway decodes them the moment they arrive, so the value
is at most one GT86 send slot old:

| IS F frame | Rate | Signals (DBC bit numbering, big-endian) | Scaling | Feeds |
|---|---|---|---|---|
| 0x0B0 | 63 Hz | FR 7\|16, FL 23\|16 | 0.01 km/h per bit | 0x0D4 bytes 0–3 |
| 0x0B2 | 63 Hz | RR 7\|16, RL 23\|16 | 0.01 km/h per bit | 0x0D4 bytes 4–7 |
| 0x0B4 | 63 Hz | speed 47\|16 (bytes 5–6) | 0.01 km/h per bit | 0x0D1 bytes 0–1 |
| 0x2C4 | 30 Hz | rpm 7\|16 signed (bytes 0–1) | 0.78125 rpm per bit | 0x141 bits 32–45 |

The layouts are Toyota's (TOYOTA_LEXUS.dbc `SPEED`, `ENGINE_RPM_2`, the 0xAA wheel-speed order) and were checked
against `gt86-isf-swap/logs/ISF_SPEED_RPM_driving.csv`: 0x0B4 reads 0 to 165.6 km/h over that drive, 0x2C4 670 to
6830 rpm, and the wheel speeds agree with 0x0B4 within 0.4 km/h with no offset (newer Toyotas subtract 67.67).
They are the `broadcast` section of `tools/gateway_frames.json`; their signals are parameters of module `isf`,
referenced by an output frame like any Mode 21 parameter. The frames reach `IsfService::onFrame` from the receive
queue between requests and, during a request, through the ISO-TP channel's tap.

This is an addition, not a replacement. The Mode 21 path is unchanged: the fast blocks (engine `21 01`, ABS
`21 03` and `21 47`) are still read every cycle and every ADU frame keeps its Mode 21 source, because that is
the ECU's own view of its values with exactly known mappings, used for diagnostics and analysis, and it is the
reference against which the broadcast decode is checked (ADU `ENG_Speed_Load` rpm and speed next to GT86 0x141
and 0x0D1 must agree).

0x0D0 (steering angle, yaw, G) could come from 0x025 / 0x024 the same way once a GT86 capture confirms the GT86
scaling of that frame.

## 3. ADU frames (29-bit ids from 0x1F0005E0)

Pass-through frames for the ECUmaster ADU and PMU, grouped by meaning (Techstream has no groups of its own; the
ECU only has its Mode 21 blocks, so the groups are ours): ENG_ (engine ECU), TRANS_ (ECT) and ABS_ (ABS/VSC).
Each signal is the ECU's raw value, exactly the bytes ToyotaLens decodes, and the DBC carries ToyotaLens's scale
factor, offset, unit and state texts. All signals are little-endian (DBC `@1`). At most 8 signals per frame
(ECUmaster per-frame limit), 36 frames, sent every 100 ms (wheel speeds and dynamics 50 ms, fault flags 500 ms) on
the GT86 bus and on the IS F bus.

The frame list with every signal's position, factor, unit and source is generated into `docs/adu-frames.md`;
the layout itself is `tools/gateway_frames.json`.

## 4. Changing the frames

1. Edit `tools/gateway_frames.json`. A signal is either
   - pass-through: `{"module": "engine", "uniqueId": "vxpyKZ0Zo", "name": "EngineSpeed", "startBit": 0}` (width from
     the ToyotaLens mask, scaling into the DBC), or
   - converted: add `"bits"`, `"factor"`, `"offset"` (and `"unit"`) to re-encode the physical value, as the GT86
     packets need, or
   - a block byte: `{"blockByte": {"module": "engine", "parameterAddress": "25", "bytePosition": 4}, "frameByte": 2}`.
   The uniqueIds are in ToyotaLens's `data/{engine,ect,abs}.json` (and in its `favorites/basics.txt`).
2. Run `python tools/generate.py`. It rejects overlaps, unknown ids and signals that do not fit.
3. Run `bash test/host/run.sh`, then `.venv\Scripts\pio run`.
4. Load the new `gateway.dbc` into the ADU and SavvyCAN.

Adding a parameter from a block that is not yet polled makes the gateway poll that block; keep an eye on the cycle
time (about 100 ms per request, fast blocks every cycle, one slow block per cycle).

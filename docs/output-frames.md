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

Gateway-specific frames therefore use **0x5E0–0x5E7**: inside the empty band, above every GT86 broadcast id,
below 0x63B, and clear of the 0x501–0x57C ids the earlier ADU project's OBD DBC used.

## 2. GT86 packets

Sent whether or not the IS F answers, so the GT86 modules see a complete car. The default bytes are the frames
recorded in `gt86-isf-swap/logs/GT86/cluster_working_no_errors.txt` (the cluster showed no warnings with exactly
these); intervals are the rates seen in the drive capture.

| Id | Name | Default bytes | Interval | Translated signals |
|---|---|---|---|---|
| 0x0D0 | GT86_GSensor | `8B FB 25 00 01 08 04 01` | 100 ms | none |
| 0x0D1 | GT86_VehicleSpeed | `F0 07 00 00 B8 0F FC 08` | 100 ms | VehicleSpeed: bytes 0–1, 0.0565 km/h per bit, from engine `21 01` pos 12 |
| 0x0D2 | GT86_Unknown_D2 | `00 00 FF FF 00 00 00 00` | 100 ms | none |
| 0x0D3 | GT86_LightStatus | `00 06 C0 0F 00 00 32 00` | 100 ms | none |
| 0x0D4 | GT86_WheelSpeeds | `F0 07 F0 07 F0 07 F0 07` | 40 ms | FL, FR, RL, RR: 16 bits each, 0.0565 km/h per bit, from ABS `21 03` |
| 0x140 | GT86_EngineData1 | `00 05 00 43 00 00 00 01` | 50 ms | none (bytes 2–3 did not follow engine speed in the capture) |
| 0x141 | GT86_EngineData2 | `00 00 00 00 00 83 A7 00` | 50 ms | EngineRPM: bits 32–45, 1 rpm per bit, from engine `21 01` pos 10; bits 46–47 kept |
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
  bytes decode to 114 km/h, the speed that capture was taken at.
- 0x0D4 raw wheel values peak at 1384 while 0x0D1 reads 76.6 km/h: the same 0.0565 scale (the community DBC's
  0.0157 is wrong for this car).
- 0x141 bits 32–45 idle at about 750, rise to 2314 at 60 km/h, and the "cluster working" bytes decode to 768.
  0x140 bytes 2–3 stay at one value for the whole drive, so they are not engine speed.
- 0x360 bytes 2 and 3 minus 40 climb from 20/52 °C to 52/84 °C as the engine warms.

Every frame a decoded block writes into is handed to the GT86 bus task on every poll of that block, whether or
not its bytes changed, so a constant value (engine off, rpm 0) keeps being refreshed. A translated frame falls
back to its default bytes only when no refresh has arrived for 5 s (`Gt86Service::TranslationHoldMs`), i.e. when
the ECU has stopped answering.

## 3. ADU frames (0x5E0–0x5E7)

Dense, pass-through frames: each signal is the ECU's raw value, exactly the bytes ToyotaLens decodes, and the
DBC carries ToyotaLens's scale factor, offset, unit and state texts. All signals are little-endian (DBC `@1`).
Each frame is sent every 100 ms.

| Id | Name | Source blocks | Content |
|---|---|---|---|
| 0x5E0 | GW_EngineBasics | engine `21 01` | EngineSpeed (16), CoolantTemp, IntakeAir, VehicleSpeed, ThrottleSensorPosition, CalculateLoad, AtmospherePressure |
| 0x5E1 | GW_FuelIgnition | engine `21 03` | FuelSystemStatus1/2, ShortFT/LongFT bank 1 and 2, IgnitionAdvance, EvapPurgeVSV |
| 0x5E2 | GW_EngineExtras | engine `21 51`, `21 01`, `21 05` | EngineOilTemperature, AmbientTemp, MAF (16), BatteryVoltage (16), CatalystTemp_B1S1 (16) |
| 0x5E3 | GW_EngineSwitches | engine `21 25` | bytes 0, 1, 4, 5, 6, 8, 10, 11 of the block, every flag at its own bit (stop light, shift P/R/N/D, sport/snow/eco mode, brake switch, cruise …) |
| 0x5E4 | GW_Transmission | ECT `21 82`, `21 85` | ATOilTemperature1 (16), ShiftStatus, lock-up and solenoid flag bytes |
| 0x5E5 | GW_WheelsGear | ABS `21 03`, `21 21`, `21 1C`, `21 1D` | FR/FL/RR/RL wheel speed, wheel direction flags, AbsVehicleSpeed, gear and shift lever position, TRAC/VSC switches |
| 0x5E6 | GW_Dynamics | ABS `21 05`, `21 06` | DecelerationSensor 1 and 2, YawRateSensor 1 and 2, SteeringAngleSensor (16), RealEngineTorque (16, signed) |
| 0x5E7 | GW_StabilityStatus | ABS `21 5A`, `21 1F`, `21 05`; engine `21 06` | TRAC/VSC control and off mode, per-wheel VSC status, stop light and parking brake switches, MIL and monitor byte, AcceleratorOpeningAngle, AbsEngineRevolutions (16) |

The full signal list with bit positions, factors and value tables is `gateway.dbc`; every signal's `CM_` comment
names the ToyotaLens module, uniqueId, parameter name, block and byte position it comes from.

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

# The ToyotaLens port

The ISF side of the gateway is ToyotaLens (`C:\projects\ToyotaLens`, verified on the 2010 IS F on
2026-10-19 and 2026-10-21) ported file for file to C++. Names, constants, timers and edge-case decisions are
ToyotaLens's; the specification sections cited in the code (§n) are `docs/techstream-J2534-protocol.md` there.
When a behaviour is in question, compare the two files side by side and make the port match.

| Gateway file | ToyotaLens source | What it is |
|---|---|---|
| `src/can/twai_wrapper.h/.cpp` | `Transport/ICanTransport.cs` (the role), `Transport/SlcanSerialTransport.cs` (Send / TryReceive / Drain) | One frame in, one frame out on the IS F bus; knows nothing of ISO-TP. |
| `src/isotp/iso_tp.h` namespace `IsoTpFraming` | `Protocol/IsoTp.cs` | PCI nibbles, payload sizes, sequence numbers, STmin encoding, the send-side `Segment`. |
| `src/isotp/iso_tp.h` struct `Message_t` | `Protocol/IsoTpReassembler.cs` | Receive-side reassembly for one id (`Feed`), under this repository's historical name. |
| `src/isotp/iso_tp.h` struct `FlowControl` | `Protocol/FlowControl.cs` | Flow-control fields; `continueAll` = `30 00 00`. |
| `src/isotp/iso_tp.h` namespace `IsoTpTimers` | `Protocol/IsoTpTimers.cs` | N_As, N_Bs, N_Cr, N_Br, WaitFramesMax, P2*. |
| `src/isotp/iso_tp.h/.cpp` class `IsoTp` | `Protocol/IsoTpChannel.cs` | `Target`, `Request`: drain, send, wait for FC with block size and STmin, receive with the id filter, FC after a first frame, N_Cr, response pending, MaxResponsePending. |
| `src/uds/uds_mapper.h` namespace `Services` | `Protocol/Services.cs` | Service ids and reply layouts for Mode 21 and the data map. |
| `src/uds/uds_mapper.h` namespace `Nrc` | `Protocol/Nrc.cs` | Negative response codes and their names. |
| `src/uds/uds_mapper.h` namespace `SupportBitmap` | `Protocol/SupportBitmap.cs` | Bitmap ids, `Parse`, the "next bitmap" bit. |
| `src/uds/uds_mapper.h` struct `ResponseDataMap` | `Protocol/ResponseDataMap.cs` | `FromReply`, `Covers`, `Expand` (stale-map rule included). |
| `src/uds/uds_mapper.h` namespace `UnitMap` | `Definitions/UnitMap.cs` | Stored unit code 0..75 to unit, scale and offset. |
| `src/uds/uds_mapper.h` struct `UdsDefinition` | `Definitions/ParameterDefinition.cs` | One parameter definition (`fieldEnd` included). |
| `src/uds/uds_mapper.h` namespace `ParameterDecoder` | `Protocol/ParameterDecoder.cs` | `ReadMasked`, `SignExtend`, `Decode`. State texts are not kept on the device; they go into the DBC instead. |
| `src/uds/uds_definitions.cpp` (generated) | `data/engine.json`, `ect.json`, `abs.json` via `Definitions/ParameterFile.cs` | The parameters the gateway forwards, verbatim. |
| `src/services/isf_service.h` `ModuleDefinition`, `Modules` | `Definitions/ModuleDefinition.cs`, `Modules.cs` | The three modules and their ids. |
| `src/services/isf_service.h` `ReplyType` | `Ecu/ReplyType.cs` | Silent, Negative, Map, Bitmap, Block, Other. |
| `src/services/isf_service.cpp` `interpret`, `discover`, `read`, `decodeBlock` | `Ecu/EcuSession.cs` `Interpret`, `Discover`, `Read`; `Protocol/DecodedBlock.cs` `Decode` | The one interpreter, probing, the bitmap walk, map fetching, block reads with echo check, the `7F 21 12` drop rule. |
| `src/services/isf_service.cpp` `listen` | `Stream/LiveStream.cs` (the WAITING state) | Discovery retried every 3 s while a module is silent; a module that stops answering is re-discovered. |
| `src/logger/logger.cpp` `Logger::frame` | `Protocol/CandumpWriter.cs` | The candump trace line, so `ToyotaLens decode` replays a gateway log. |
| `test/host/fake_bus.*` | `SelfTest/FakeCanTransport.cs` | A scripted ECU on a fake bus. |
| `test/host/selftest.cpp` | `SelfTest/IsoTpVectors.cs`, `IsoTpChannelVectors.cs`, `DecoderVectors.cs`, `SessionVectors.cs` | The acceptance vectors, plus translator and end-to-end checks of this repository's own. |

## What is the gateway's own

- The polling scheme (`IsfService::fastStep`, `slowStep`): ToyotaLens reads every supported block once per cycle
  for the tab in focus; the gateway reads engine `21 01` and ABS `21 03` every cycle and one other block per
  cycle. The broadcast decode (`IsfService::onFrame`, `src/isf_broadcast.*`, the `IsoTp` frame tap) is the
  gateway's own too; ToyotaLens has no use for the modules' broadcast frames. Which blocks exist to poll comes from the forwarded parameters, intersected with the support bitmap
  (requirement R4.4).
- `src/message_translator.*` and `src/translator_table.cpp`: decoded values into output frames.
- `src/services/gt86_service.*`, `src/mcp_can/`: the second bus.
- `tools/gateway_frames.json`, `tools/generate.py`, `gateway.dbc`: the output-frame specification
  (`docs/output-frames.md`).

## Deliberate differences

- Single-frame transmission would suffice (every request is 2 or 3 bytes, R1.3), but the send side including
  flow control, block size, STmin and wait handling is ported anyway, as ToyotaLens built it, and is covered by
  the self-test.
- The receive buffer is 256 bytes (§8.3) instead of the 4095-byte protocol maximum; a first frame declaring more
  is dropped.
- The data-map layout is capped at 64 bytes (`ResponseDataMap::MaxLayoutLength`); the largest seen on the IS F is
  56.
- Service 09 identification and DTC reading (`Identify`, `ReadDtcs`, `ClearDtcs`) are not ported: the gateway
  does not need them.

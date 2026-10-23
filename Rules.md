# Project Rules for AI Automation

**Purpose**
The working rules, canonical references, build steps and verification checklist that every automated agent (and
human) follows when working on this code-base. `CLAUDE.md` is only the reading list that leads here.

## 1. Working Rules

### 1.1 The IS F side is a port of ToyotaLens, not a design of its own

Everything that talks to the IS F modules (ISO-TP, the Mode 21 session, support bitmaps, data maps, parameter
decoding, parameter lists) is `C:\projects\ToyotaLens` ported file for file: same names, constants, timers and
edge-case decisions, and its specification vectors as the self-test. ToyotaLens was verified on the car; the
gateway's first attempt was not. Before writing any IS F-side C++, open the matching ToyotaLens file (the table in
`docs/toyotalens-port.md`) and make the port match it. Do not reason from first principles and do not "improve"
the protocol code. Only the GT86/output side (frames, defaults, DBC/CANX, translator) is the gateway's own.

### 1.2 The Mode 21 path is the reference and is never removed

The ADU frames (29-bit ids from 0x1F0005E0) come from Mode 21 over ISO-TP, with exactly known mappings from
ToyotaLens's definitions. That path serves diagnostics and is the reference against which anything else is
checked. The broadcast decode (IS F 0x0B0/0x0B2/0x0B4/0x2C4 into the GT86 motion frames, `docs/output-frames.md`
§2.1) exists only for latency on the GT86 side and was added next to it. Never replace a Mode 21 source with a
broadcast one, never drop the fast-polled blocks (`FAST_BLOCKS`, `fastStep`, `slowStep`) and never slow the ADU
frames.

### 1.3 Plans before edits

Explore first and get the plan approved before changing code. Do not introduce a new domain concept (a new kind
of signal, frame, source or configuration key) without asking.

### 1.4 The ADU project is out of scope

The ECUmaster ADU project (.adu) is not part of this repository and is never edited or generated here. The
gateway's job ends at `gt86-bus.canx` / `isf-bus.canx` (and the DBC files), which the owner imports in the ADU
client. `docs/adu-project.md` records what the ADU needs from those files.

## 2. Requirements Hierarchy

| Priority | Source | Rule |
|---|---|---|
| 1 | `C:\projects\ToyotaLens` (`docs/techstream-J2534-protocol.md`, `docs/requirements.md`, `src/ToyotaLens`) | The ISF side (ISO-TP, Mode 21 session, bitmaps, data maps, decoding) is a port of ToyotaLens. Copy it; do not redesign it. When behaviour is in doubt, open the ToyotaLens file named in `docs/toyotalens-port.md` and make the port match. |
| 2 | `C:\projects\ToyotaLens\data\{engine,ect,abs}.json` | The only source of parameter definitions. They enter this repository through `tools/generate.py`; never hand-edit `src/uds/uds_definitions.cpp`. |
| 3 | `tools/gateway_frames.json` and `docs/output-frames.md` | The specification of the frames the gateway puts on the GT86 bus. Edit the JSON, run the generator; never hand-edit the generated files (`src/translator_table.cpp`, the DBC and CANX files, `docs/adu-frames.md`). |
| 4 | `Readme.md` | Overview and the module / block table. |

## 3. Code Style

- Obey every rule in `code-guide.md`.
- If you find a rule that is not followed, fix it.

## 4. Build, Flash, Test and Debug

| Step | Instruction file |
|---|---|
| Compile | `build-guide.md` (`.venv\Scripts\pio run`) |
| Regenerate the tables | `build-guide.md` (`python tools/generate.py`) |
| Host self-test | `build-guide.md` (`bash test/host/run.sh`, needs Docker) |
| Flash and monitor | `deployment-guide.md` (`pio run -t upload`, `pio device monitor`) |
| Debug | `deployment-guide.md`: log level and frame trace, every log line and its meaning, first power-up checklist, watching the GT86 bus, troubleshooting |

## 5. Directory Layout Governance

- Keep the *Directory layout* section in `Readme.md` up to date.
- Add a bullet for each new top-level folder at the moment it is introduced.

## 6. Excluded Content

Ignore everything inside `.pio`, `.venv`, `.cursor`, `.build`.

## 7. Routine Verification Checklist

1. `python tools/generate.py` leaves `git status` clean (generated files match the spec).
2. `bash test/host/run.sh` prints `RESULT: PASS` (required before a commit that touches `src/` or `tools/`).
3. `.venv\Scripts\pio run` succeeds.
4. Code conforms to `code-guide.md`.

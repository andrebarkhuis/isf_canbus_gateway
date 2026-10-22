# Project Rules for AI Automation

**Purpose**
Define the canonical references, coding standards, build steps and conventions that every automated agent (and
human) must follow when working on this code-base.

## 1. Requirements Hierarchy

| Priority | Source | Rule |
|---|---|---|
| 1 | `C:\projects\ToyotaLens` (`docs/techstream-J2534-protocol.md`, `docs/requirements.md`, `src/ToyotaLens`) | The ISF side (ISO-TP, Mode 21 session, bitmaps, data maps, decoding) is a port of ToyotaLens. Copy it; do not redesign it. When behaviour is in doubt, open the ToyotaLens file named in `docs/toyotalens-port.md` and make the port match. |
| 2 | `C:\projects\ToyotaLens\data\{engine,ect,abs}.json` | The only source of parameter definitions. They enter this repository through `tools/generate.py`; never hand-edit `src/uds/uds_definitions.cpp`. |
| 3 | `tools/gateway_frames.json` and `docs/output-frames.md` | The specification of the frames the gateway puts on the GT86 bus. Edit the JSON, run the generator; never hand-edit `src/translator_table.cpp` or `gateway.dbc`. |
| 4 | `Readme.md` | Overview and the module / block table. |

## 2. Code Style

- Obey every rule in `code-guide.md`.
- If you find a rule that is not followed, fix it.

## 3. Build, Flash and Test

| Step | Instruction file |
|---|---|
| Compile | `build-guide.md` (`.venv\Scripts\pio run`) |
| Flash and monitor | `deployment-guide.md` (`pio run -t upload`, `pio device monitor`) |
| Host self-test | `build-guide.md` (`bash test/host/run.sh`, needs Docker) |

## 4. Directory Layout Governance

- Keep the *Directory layout* section in `Readme.md` up to date.
- Add a bullet for each new top-level folder at the moment it is introduced.

## 5. Excluded Content

Ignore everything inside `.pio`, `.venv`, `.cursor`, `.build`.

## 6. Routine Verification Checklist

1. `python tools/generate.py` leaves `git status` clean (generated files match the spec).
2. `bash test/host/run.sh` prints `RESULT: PASS`.
3. `.venv\Scripts\pio run` succeeds.
4. Code conforms to `code-guide.md`.

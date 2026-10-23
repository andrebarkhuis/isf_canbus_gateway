# Working rules for this repository

Read `Readme.md` first, then `docs/toyotalens-port.md` and `docs/output-frames.md`.

## The IS F side is a port of ToyotaLens, not a design of its own

Everything that talks to the IS F modules (ISO-TP, the Mode 21 session, support bitmaps, data maps,
parameter decoding, parameter lists) is `C:\projects\ToyotaLens` ported file for file: same names, constants,
timers and edge-case decisions, and its specification vectors as the self-test. ToyotaLens was verified on the
car; the gateway's first attempt was not. Before writing any IS F-side C++, open the matching ToyotaLens file
(the table in `docs/toyotalens-port.md`) and make the port match it. Do not reason from first principles and
do not "improve" the protocol code. Only the GT86/output side (frames, defaults, DBC/CANX, translator) is the
gateway's own.

## The Mode 21 path is the reference and is never removed

The ADU frames (29-bit ids from 0x1F0005E0) come from Mode 21 over ISO-TP, with exactly known mappings from
ToyotaLens's definitions. That path serves diagnostics and is the reference against which anything else is
checked. The broadcast decode (IS F 0x0B0/0x0B2/0x0B4/0x2C4 into the GT86 motion frames) exists only for
latency on the GT86 side and was added next to it. Never replace a Mode 21 source with a broadcast one, never
drop the fast-polled blocks (`FAST_BLOCKS`, `fastStep`, `slowStep`) and never slow the ADU frames.

## Plans before edits

Explore first and get the plan approved before changing code. Do not introduce a new domain concept (a new
kind of signal, frame, source or configuration key) without asking.

## The ADU project is the user's

`Ecumaster/*.adu` is edited only by the user, in the ECUmaster ADU client. The gateway's job ends at
`gt86-bus.canx` / `isf-bus.canx` (and the DBC files), which the client imports. `docs/adu-project.md` records
what is known about the ADU and the project file for reading and diagnosing it, not for editing it.

## Commands

```bash
python -m venv .venv && .venv\Scripts\pip install -r requirements.txt   # once
python tools/generate.py        # after editing tools/gateway_frames.json: C++ tables, DBC, CANX, docs/adu-frames.md
bash test/host/run.sh           # host self-test (Docker); must pass before a commit that touches src/ or tools/
.venv\Scripts\pio run           # firmware build
.venv\Scripts\pio run -t upload # flash
```

`tools/gateway_frames.json` is the single source for frames and signals; never edit the generated files
(`src/uds/uds_definitions.cpp`, `src/translator_table.cpp`, `*.dbc`, `*.canx`, `docs/adu-frames.md`) by hand.

# Project Rules for AI Automation

**Purpose**
Define the canonical references, coding standards, build steps, and directory conventions that every automated agent (and human) must follow when working on this code‑base.

### 1. Source‑of‑Truth Hierarchy

| Priority | Folder / File                                    | Rule                                                                                                                     |
| -------- | ------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------ |
| 1        | `docs/Lexus ISF Canbus Message Definitions.json` | Defines Lexus IS‑F CAN bus messages; treat as authoritative.                                                             |
| 2        | `docs/`                                          | Always consult first for any implementation detail.                                                                      |
| 3        | `Techstream_uds_logs/`                           | Use as factual ground truth for UDS message formats. If instructions in `docs/` are ambiguous, follow the patterns here. |

**When discrepancies exist**

1. Trust `Techstream_uds_logs/`.
2. Update code and, if necessary, the markdown docs to align.

### 2. Code Style

* Obey every rule in `code-guidelines.md`.
* Reject code changes that violate these guidelines.

### 3. Build & Flash Procedures

| Target      | Instruction File                                                                  |
| ----------- | --------------------------------------------------------------------------------- |
| **Arduino** | `build-instructions.md`                                                           |
| **ESP32**   | `ESP32 Firmware Flash & Build Upload Guide.md` (flash to the configured COM port) |

Follow the exact steps; do not improvise.

### 4. Directory Layout Governance

* Keep the *Directory Layout* section in the repository `README.md` up‑to‑date.
* Add a bullet for each new top‑level folder at the moment it is introduced.

### 5. Excluded Content

Ignore everything inside the following directories. Do not read from, write to, or commit changes within them.

```
.build/
.cursor/
```

### 6. Routine Verification Checklist (for CI or pre‑commit hooks)

1. Docs reference → Logs consistency check passes.
2. Code conforms to `code-guidelines.md` (run linter).
3. Build scripts succeed for both Arduino and ESP32.
4. README directory layout matches filesystem.
5. No touched files under `.build/` or `.cursor/`.

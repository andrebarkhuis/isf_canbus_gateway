# Project Rules for AI Automation

**Purpose**
Define the canonical references, coding standards, build steps, and directory conventions that every automated agent (and human) must follow when working on this code‑base.

## 1. Requirements Hierarchy

| Priority | Folder / File     | Rule                                                                                                                |
| ---------- | ------------------- | --------------------------------------------------------------------------------------------------------------------- |
| 1        | `Readme.md`       | Defines Lexus IS‑F CAN bus messages; treat as authoritative requirements.                                          |
| 2        | `uds/uds_mapper.h | Make sure UDS ISO-TP, UDS Requests and UDS Response aligns with the `ISF CAN UDS Message Definitions.csv` file. |

## 2. Code Style

* Obey every rule in `code-guidelines.md`.
* If you find a rule that is not followed, fix it.

## 3. Build & Flash Procedures

| Target      | Instruction File                                                                  |
| ------------- | ----------------------------------------------------------------------------------- |
| **Arduino** | `build-instructions.md`                                                           |
| **ESP32**   | `ESP32 Firmware Flash & Build Upload Guide.md` (flash to the configured COM port) |

## 4. Directory Layout Governance

* Keep the *Directory Layout* section in the repository `README.md` up‑to‑date.
* Add a bullet for each new top‑level folder at the moment it is introduced.

## 5. Excluded Content

Ignore everything inside the following directories:

* .build
* .cursor

## 6. Routine Verification Checklist (for CI or pre‑commit hooks)

1. `Readme.md` reference → Logs consistency check passes.
2. Code conforms to `code-guidelines.md`.
3. Build succeeds using `build-instructions.md`.

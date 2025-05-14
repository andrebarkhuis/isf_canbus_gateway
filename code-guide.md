# Code Style Guide

This guide defines coding conventions, file‑organization rules, and clean‑code practices for the project (Arduino / C++). It is intended to be equally easy for people to read and for automated agents or linters to parse.

> **One‑pager promise:** Everything you need is on this page—no hidden rules elsewhere.

## 1 Folder & File Organization

| Area                           | Rule                                                                                         |
| -------------------------------- | ---------------------------------------------------------------------------------------------- |
| **Top‑level folders**         | Place code in purpose‑driven directories such as`common/`, `can/`, `services/`, `uds/`, `iso_tp/`, `mcp_can/`. |
| **Headers vs. sources**        | Each`.cpp` must have a matching `.h`. If a `.cpp` lacks a header, create one.                    |
| **Physical vs. logical paths** | Folder names reflect the logical component, not the hardware board name.                        |

## 2 Including Header Files

1. **Canonical path only** – Always include headers using the same *relative* path from the repo root.

   ```cpp
   #include "logger/logger.h"        // ✅ Consistent
   #include "mcp_can/mcp_canbus.h"   // ✅ Consistent
   #include "logger.h"               // ❌ Missing context
   ```

2. **Never include a `.cpp`.** If you need something from a source file, move it to a header.
3. **Quote vs. angle‑bracket syntax**

   | Use                             | Syntax  | Example                          |
   | --------------------------------- | --------- | ---------------------------------- |
   | Project headers                 | `"..."` | `#include "logger/logger.h"`     |
   | Arduino core libraries          | `<...>` | `#include <Wire.h>`              |
   | BSPs/RTOS                       | `<...>` | `#include <freertos/FreeRTOS.h>` |
   | External libs (Library Manager) | `<...>` | `#include <Adafruit_Sensor.h>`   |

🔹 **Tip for AI parsers:** Every include line starts at column 0; avoid indenting `#include`.

## 3 Commenting

* Explain **why**, not **what**.

  ```cpp
  counter++;               // ❌ “increment counter”
  counter++;               // ✅ “advance to next retry attempt”
  ```

* Use full sentences if the meaning is subtle; fragments are fine for obvious context.

🔹 **Machine‑readable tags:** Prefer `// NOTE:` / `// TODO:` / `// FIXME:` prefixes for tools that harvest tasks.

## 4 Naming & Variables

| Guideline                                   | Example                              |
| --------------------------------------------- | -------------------------------------- |
| Descriptive, no abbreviations unless common | `errorCount`, not `errCnt`           |
| Avoid “magic numbers/strings”             | `constexpr uint8_t kMaxRetries = 5;` |

Before adding a new symbol, **search** the codebase to reuse existing ones (DRY principle).

## 5 Function & Class Design

* One purpose per function or class.
* ≤ 30 – 40 lines per function; refactor beyond that.
* Prefer expressive names to inline comments.
* Keep public APIs small; hide helpers in anonymous namespaces or `detail` subfolders.

## 6 Formatting

| Setting         | Value                                           |
| ----------------- | ------------------------------------------------- |
| Indent          | 4 spaces, no tabs                              |
| Max line length | 120 chars                                      |
| Braces          | **Allman** style: opening brace on its own line |
| File encoding   | UTF‑8 w/out BOM                               |

## 7 Compile‑Time Syntax Check

After any change to a `.cpp` or `.h`:

```bash
g++ -fsyntax-only <file>
```

CI will also run this, but running it locally catches errors earlier.

## 8 Clean‑Code Checklist (🔹 quick scan before PR)

* [ ] Does each file include only what it needs?
* [ ] Any duplicated code that can be unified?
* [ ] Are all names self‑explanatory?
* [ ] Are complex sections commented with **intent**?
* [ ] Are there unresolved `TODO`/`FIXME` tags?

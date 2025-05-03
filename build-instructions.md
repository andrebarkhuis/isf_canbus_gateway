# 🛠️ Build Instructions

This document provides a detailed overview of how to build the ESP32 Arduino-based project, including required tools, settings, and the build process.

## ✅ Requirements

* **Arduino CLI**: Installed and accessible via system PATH. [Installation guide](https://arduino.github.io/arduino-cli/0.35/installation/)
* **ESP32 board support**: Installed via Arduino CLI

  ```bash
  arduino-cli core update-index
  arduino-cli core install esp32:esp32
  ```

## ⚙️ Board Configuration (FQBN)

Fully Qualified Board Name (FQBN) for this project:

```bash
esp32:esp32:aslcanx2
```

Use this when compiling the sketch with `arduino-cli`.

## 🧪 Build Process

To compile the sketch using `arduino-cli`, ensure the following:

- All source folders (`common`, `can`, `services`, `uds`, `mcp_can`, `isotp`) are now inside the `libraries/` directory.
- All `#include` statements use the Arduino library style, e.g., `#include <common/logger.h>`, `#include <can/twai_wrapper.h>`, etc. (Do NOT use the `libraries/` prefix in your includes.)
- The `--libraries` flag is NOT needed for these folders; all include paths are handled via the `-I` flags below.

Use this command from the project root:

```bash
arduino-cli compile --fqbn esp32:esp32:aslcanx2 \
  --build-property build.extra_flags="-I./libraries/common -I./libraries/can -I./libraries/services -I./libraries/uds -I./libraries/mcp_can -I./libraries/isotp" \
  --build-property "compiler.cpp.extra_flags=-I./libraries/common -I./libraries/can -I./libraries/services -I./libraries/uds -I./libraries/mcp_can -I./libraries/isotp" \
  --build-path ./.build --verbose .
```

This command:

* Compiles the `isf_canbus_gateway.ino` sketch
* Applies all relevant include paths
* Produces detailed verbose output
* Saves build artifacts in the `.build/` directory

**Troubleshooting:**
- If you see errors about missing headers (e.g., `fatal error: can/twai_wrapper.h: No such file or directory`), double-check that:
    - The folder exists inside `libraries/` (e.g., `libraries/can/twai_wrapper.h`)
    - The `#include` statement uses the correct style (e.g., `#include <can/twai_wrapper.h>`)
    - The `-I` flag for that folder is present in the build command.


## 📂 Output Artifacts

Output files include:

* `isf_canbus_gateway.ino.cpp`
* `isf_canbus_gateway.ino.elf`
* `isf_canbus_gateway.ino.bin`

All files are placed in the `.build/` directory.

---

## ⚠️ NOTE for Arduino IDE (GUI) Compatibility

The current build instructions rely on explicit include paths (`-I` flags), which **are not supported directly by the Arduino IDE graphical interface**.

If you need to compile this project using the Arduino IDE GUI, you'll need to:

* Use relative paths for your include statements, such as:

  ```cpp
  #include "../common/logger.h"
  #include "../can/twai_wrapper.h"
  ```

* Or, reorganize headers to the root directory of the sketch to avoid include path issues.

For compatibility with both Arduino CLI and Arduino IDE GUI simultaneously, relative paths are recommended.

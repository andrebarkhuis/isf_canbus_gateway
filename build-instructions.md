# 🛠️ Build Instructions – ESP32 Arduino Project

This guide walks you through building the **ISF CAN‑Bus Gateway** for ESP32 using either the Arduino CLI or the Arduino IDE.

## 📋 Table of Contents

1. [Prerequisites](#prerequisites)
2. [Board Configuration (FQBN)](#board-configuration-fqbn)
3. [Build Process](#build-process)
   1. [Arduino CLI](#arduino-cli)
   2. [Arduino IDE](#arduino-ide)
4. [Output Artifacts](#output-artifacts)

## ✅ Prerequisites<a name="prerequisites"></a>


| Requirement             | Version / Notes                                                                                                                                  |
| ------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Arduino CLI**        | v0.35 or newer installed and available on your`PATH`. <br/>➡️ [Installation guide](https://arduino.github.io/arduino-cli/0.35/installation/) |
| **ESP32 board support** | Core installed via**Arduino CLI**: `arduino-cli core install esp32:esp32`                                                                       |

## ⚙️ Board Configuration (FQBN)<a name="board-configuration-fqbn"></a>

```text
esp32:esp32:aslcanx2
```

*Board details*: [https://wiki.autosportlabs.com/ESP32-CAN-X2](https://wiki.autosportlabs.com/ESP32-CAN-X2)

## 🧪 Build Process<a name="build-process"></a>

### Arduino CLI<a name="arduino-cli"></a>

> **Heads‑up:**
> The include‑path flags **must mirror the folder names inside `/src`**.
> If you add or rename sub‑directories, remember to update the `-I` paths below.

```bash
arduino-cli compile \
  --fqbn esp32:esp32:aslcanx2 \
  --build-property build.extra_flags="-I./src/common -I./src/can -I./src/services -I./src/uds -I./src/mcp_can -I./src/iso_tp" \
  --build-property "compiler.cpp.extra_flags=-I./src/can -I./src/services -I./src/uds -I./src/mcp_can -I./src/iso_tp" \
  --build-path ./.build \
  --verbose .
```

The command above will:

1. Compile **`isf_canbus_gateway.ino`**
2. Apply all required include paths
3. Produce verbose output for easier debugging
4. Place all build artifacts in **`./.build/`**

### Arduino IDE<a name="arduino-ide"></a>

Follow the step‑by‑step instructions in the official board guide:
[https://wiki.autosportlabs.com/ESP32-CAN-X2#Step\_by\_step\_instruction\_for\_Arduino\_IDE](https://wiki.autosportlabs.com/ESP32-CAN-X2#Step_by_step_instruction_for_Arduino_IDE)

## 📂 Output Artifacts<a name="output-artifacts"></a>

After a successful build, the following files will be located in **`./.build/`**:

* `isf_canbus_gateway.ino.cpp`
* `isf_canbus_gateway.ino.elf`
* `isf_canbus_gateway.ino.bin`

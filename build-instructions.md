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

To compile the sketch using `arduino-cli`, run the following command from the project root:

```bash
arduino-cli compile \
  --fqbn esp32:esp32:aslcanx2 \
  --build-property build.extra_flags="-I./common -I./can -I./services -I./uds -I./mcp_can -I./isotp" \
  --build-path ./.build \
  --verbose \
  .
```

This command:

* Compiles the `isf_canbus_gateway.ino` sketch
* Applies all relevant include paths
* Produces detailed verbose output
* Saves build artifacts in the `.build/` directory

## 📂 Output Artifacts

Output files include:

* `isf_canbus_gateway.ino.cpp`
* `isf_canbus_gateway.ino.elf`
* `isf_canbus_gateway.ino.bin`

All files are placed in the `.build/` directory.
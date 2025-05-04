# GT86-ISF Gateway Project Requirements

## 🛠 Development Environment

- **Operating System:** Windows 11
- **IDEs Used:**
  - [Curser IDE](https://curser.dev/)
  - Visual Studio Code
  - Arduino IDE
- **Debugging Tools:**
  - [SavvyCAN](https://www.savvycan.com/) (CAN Bus logging and analysis)
- **Hardware:**
  - [ESP32-CAN-X2](https://wiki.autosportlabs.com/ESP32-CAN-X2) from Autosport Labs

## 📁 Directory Layout

```text
isf_canbus_gateway/
├── .build/              # Build output directory
├── .cursor/             # Curser IDE configuration
├── .git/                # Git repository
├── build-instructions.md # Build instructions
├── code-guidelines.md    # Code style guidelines
├── docs/                # Documentation
├── src/                 # Source code
│   ├── can/            # CAN bus interface implementation
│   ├── common_types.h  # Shared type definitions
│   ├── diagrams/       # Project diagrams
│   ├── iso_tp/         # ISO-TP protocol handling
│   ├── logger/         # Logging implementation
│   ├── mcp_can/        # MCP2515 CAN controller interface
│   ├── message_translator.h # Message translation logic
│   ├── services/       # Diagnostic services
│   └── uds/           # UDS protocol support
├── techstream_uds_logs/ # UDS logs
└── isf_canbus_gateway.ino
```

## 📚 Important Reference Documents

The `docs/` directory contains key reference materials for understanding, decoding, and mapping CAN bus and UDS messages in Toyota/Lexus vehicles. Here is an overview of the main documents:

| File Name                                           | Purpose                                                                                                                                                                                                  |
| ----------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Decoding Techstream DTC canbus messages.md**          | Explains the logic for decoding Diagnostic Trouble Codes (DTCs) for Engine and ABS ECUs using sample CAN messages and byte-by-byte breakdowns. Useful for understanding DTC extraction and status logic. |

Note: These documents are essential for anyone extending, debugging, or reverse-engineering the gateway. Always consult them before making changes to message handling or decoding logic.

## 📟 Monitoring COM Port After Flashing

After flashing the ESP32 firmware, you can monitor the COM port using pySerial's miniterm tool. This is particularly useful for debugging and monitoring CAN bus traffic.

To monitor COM7 at 115200 baud rate:

```bash
python -m serial.tools.miniterm COM7 115200 --filter hexlify --timestamp
```

The `--filter hexlify` option displays each byte as hexadecimal, which is especially useful for viewing CAN payloads. The `--timestamp` option adds timestamps to each line of output.

To capture the output to a file while still displaying it on the screen:

```bash
python -m serial.tools.miniterm COM7 115200 --logfile can_dump.bin --log raw
```

This will create a binary file `can_dump.bin` containing all the serial output, which can be useful for later analysis.

## 🔌 ECU Modules & Target Modules

### ISF CAN Bus ID Mapping

- **0x7B0**: Associated with diagnostic requests or responses related to ABS.
- **0x7B8**: This ID is the response or continuation of the 0x7B0 request for ABS.
- **0x7E8**: Commonly associated with engine ECU responses in various vehicle systems.

### ISF DTC Extraction

- **Diagnostic Trouble Codes (DTCs)**: These are embedded in the CAN messages and can be found in specific bytes of the message payload.
- For ABS system, DTCs like `C1201` and `C1429` were previously identified, decoded from bytes D3 and D4 in the CAN message.
- For the engine ECU, DTCs can be found in the payload bytes of messages with ID 0x7E8. The byte location may vary.

### ISF-Side ECUs (Connected and Operational)

- **Engine ECU** CAN ID range: `0x700`–`0x7FF` (UDS range), `0x180` (Speed/RPM)
- **ABS Module** CAN ID range: `0x284`–`0x285`
- **TCU (Transmission Control Unit)** CAN ID range: `0x3E0`–`0x3EF` (varies by region/model)

### GT86-Side ECUs (Expected Message Consumers)

- **Body ECU** CAN ID range: `0x2F0`–`0x2FF`
- **Combination Meter r** CAN ID: `0x208`, `0x209`, `0x210`, `0x211`, `0x212`, `0x322`

## Core Functional Requirements

### 1. **Handle Incoming ISF CAN Messages**

- **Check if a message needs translation**:
  - Verify whether the received message from the ISF CAN bus is required for translation.
- **Translate the message if required**:
  - Transform the ISF message into one or more GT86-compatible messages.
  - Some messages (e.g., Speed or RPM) may span multiple GT86 CAN IDs.
- **Send the translated message to the GT86 CAN bus**:
  - Replace the default GT86 message values with the translated message values.
  - Ensure timely delivery to maintain real-time operation.

### 2. **Send Periodic Messages to ISF CAN Bus**

- **Initialization and Diagnostics Messages**:
  - Continuously send the following to the ISF CAN bus:
    - **Session Initialization Requests**: Required to maintain communication with the ISF ECUs during runtime.
    - **Diagnostic Trouble Code (DTC) Requests**: Periodic queries to monitor and capture error codes.
    - **OBD-II Requests**: Requests to read standard sensor data from the ISF ECUs.
- **Timing and Frequency**:
  - Ensure these messages are sent at regular intervals (e.g., every 1 second for initialization and diagnostics).

### 3. **Fallback to Default GT86 Messages**

- **Default Messages**:
  - Pre-defined arrays of GT86 messages simulate a parked car (e.g., all values zeroed or baseline).
  - GT86 Message IDs to be sent by default: `208`, `209`, `210`, `211`, `212`, `1953`, `322`, `1961`, `1224`
- **Fallback Mechanism**:
  - If no relevant ISF CAN messages are received within a defined timeout (e.g., 50ms), send the default GT86 messages.

### 4. **Message Replacement Logic**

- Some ISF CAN messages require translation into multiple GT86 messages. For example, Speed from ISF (ID 0x180) maps to GT86 CAN IDs 209 and 212.
- The gateway must andle these 1-to-many mappings accurately and in real time to ensure compatibility.
- Ensure that transformed messages from the ISF are prioritized and replace their respective default GT86 messages during operation.
- Default messages for IDs that are replaced should not be sent.

### 5. **Minimum Required GT86 Messages**

These messages are required to be sent at all times, even if no relevant ISF messages are received.

| Hex ID  | Data (Bytes)              | Description                                               |
| --------- | --------------------------- | ----------------------------------------------------------- |
| `0xD0`  | `8B FB 25 00 01 08 04 01` | Steering Angle and Lateral/Longitudinal G-Forces (ID 208) |
| `0xD1`  | `F0 07 00 00 B8 0F FC 08` | Vehicle Speed and Brake Pressure (ID 209)                 |
| `0xD2`  | `00 00 FF FF 00 00 00 00` | Unknown State Message (ID 210)                            |
| `0xD3`  | `00 06 C0 0F 00 00 32 00` | VSC and Traction Control Lights (ID 211)                  |
| `0xD4`  | `F0 07 F0 07 F0 07 F0 07` | Wheel Speeds (ID 212)                                     |
| `0x140` | `00 05 00 43 00 00 00 01` | Clutch, Throttle, and Engine RPM (ID 320)                 |
| `0x141` | `00 00 00 00 00 83 A7 00` | Accelerator, Engine Load, RPM, Signals (ID 321)           |
| `0x360` | `00 00 84 50 00 00 11 00` | Coolant Temp, Oil Temp, Cruise Control (ID 864)           |
| `0x361` | `00 29 00 DD 23 00 A4 34` | Gear Position (ID 865)                                    |
| `0x370` | `00 00 01 01 00 03 00 00` | Unknown but required (ID 880)                             |
| `0x7A1` | `02 19 81 00 00 00 00 00` | Unknown but required (ID 1953)                            |
| `0x142` | `00 00 00 00 00 00 00 00` | Unknown but required (ID 322)                             |
| `0x7A9` | `02 83 00 00 00 00 00 00` | Unknown but required (ID 1961)                            |
| `0x4C8` | `09 00 00 00 00 00 00 00` | Unknown but required (ID 1224)                            |

### 6. **Minimum ISF Messages for Translation**

| ID  | Description  | Target GT86 IDs | Notes                                                         | Function Name |
| ----- | -------------- | ----------------- | --------------------------------------------------------------- | --------------- |
| 180 | Speed        | `209`, `212`    | Speed spans multiple GT86 messages.                           | mapSpeed      |
| 708 | RPM          | `320`, `321`    | RPM spans multiple GT86 messages.                             | mapRpm        |
| 864 | Temperatures | `864`           | Engine oil temperature (C - 40), Coolant temperature (D - 40) | mapWaterTemp  |

### 7. **Minimum ISF Messages for Translation**

The bellow is the requirements and definitions to accurately send and decode messages from the ISF CAN bus.
Each of the following isf messages should sent to the ISF CAN bus and each response should be mapped and accurately decoded so that the value can be sent to the Gt86 Canbus.

| ECU Hex ID|Parameter ID|Parameter ID Hex|UDS Data Identifier|UDS Data Identifier Hex|Is Signed|Unit Type|Byte Position|Bit Offset Position|Scaling Factor   |Offset Value|Bit Length|Parameter Value|Parameter Display Value|Parameter Name                      |
|----------|------------|----------------|-------------------|-----------------------|---------|---------|-------------|------------------|-----------------|------------|----------|---------------|-----------------------|------------------------------------|
|0x7E0     |2           |0x2             |1                  |0x1                    |0        |33       |1            |0                 |0.392156862745098|0.0         |4         |               |                       |Vehicle Load                        |
|0x7E0     |8           |0x8             |1                  |0x1                    |0        |57       |9            |0                 |1.0              |-40.0       |4         |               |                       |Coolant Temp                        |
|0x7E0     |9           |0x9             |1                  |0x1                    |0        |39       |10           |0                 |0.25             |0.0         |4         |               |                       |Engine Speed                        |
|0x7E0     |10          |0xA             |1                  |0x1                    |0        |42       |12           |0                 |1.0              |0.0         |4         |               |                       |Vehicle Speed                       |
|0x7E0     |33          |0x21            |4                  |0x4                    |0        |0        |0            |0                 |3.05e-05         |0.0         |4         |               |                       |Target Air-Fuel Ratio               |
|0x7E0     |58          |0x3A            |6                  |0x6                    |0        |0        |1            |1                 |1.0              |0.0         |4         |0              |Not Avl                |Fuel System Monitor                 |
|0x7E0     |58          |0x3A            |6                  |0x6                    |0        |0        |1            |1                 |1.0              |0.0         |4         |1              |Avail                  |Fuel System Monitor                 |
|0x7E0     |108         |0x6C            |34                 |0x22                   |0        |57       |9            |0                 |0.00390625       |-40.0       |4         |               |                       |A/T Oil Temp from ECT               |
|0x7E0     |115         |0x73            |37                 |0x25                   |0        |0        |0            |6                 |1.0              |0.0         |4         |0              |OFF                    |Engine Oil Pressure SW              |
|0x7E0     |115         |0x73            |37                 |0x25                   |0        |0        |0            |6                 |1.0              |0.0         |4         |1              |ON                     |Engine Oil Pressure SW              |
|0x7E0     |147         |0x93            |37                 |0x25                   |0        |0        |8            |2                 |1.0              |0.0         |4         |0              |OFF                    |Neutral Position SW Signal          |
|0x7E0     |147         |0x93            |37                 |0x25                   |0        |0        |8            |2                 |1.0              |0.0         |4         |1              |ON                     |Neutral Position SW Signal          |
|0x7E0     |187         |0xBB            |55                 |0x37                   |0        |57       |1            |0                 |0.625            |-40.0       |4         |               |                       |Initial Intake Air Temp             |
|0x7E0     |191         |0xBF            |55                 |0x37                   |0        |60       |8            |0                 |0.03125          |-1024.0     |4         |               |                       |Knock Correct Learn Value           |
|0x7E0     |192         |0xC0            |55                 |0x37                   |0        |60       |10           |0                 |0.03125          |-1024.0     |4         |               |                       |Knock Feedback Value                |
|0x7E0     |224         |0xE0            |57                 |0x39                   |0        |0        |3            |3                 |1.0              |0.0         |4         |0              |OFF                    |Fuel Shutoff Valve for Delivery Pipe|
|0x7E0     |224         |0xE0            |57                 |0x39                   |0        |0        |3            |3                 |1.0              |0.0         |4         |1              |ON                     |Fuel Shutoff Valve for Delivery Pipe|
|0x7E0     |227         |0xE3            |57                 |0x39                   |0        |0        |3            |0                 |1.0              |0.0         |4         |0              |OFF                    |Idle Fuel Cut Prohibit              |
|0x7E0     |227         |0xE3            |57                 |0x39                   |0        |0        |3            |0                 |1.0              |0.0         |4         |1              |ON                     |Idle Fuel Cut Prohibit              |
|0x7E0     |237         |0xED            |57                 |0x39                   |0        |0        |5            |2                 |1.0              |0.0         |4         |0              |OFF                    |Intank Fuel Pump                    |
|0x7E0     |237         |0xED            |57                 |0x39                   |0        |0        |5            |2                 |1.0              |0.0         |4         |1              |ON                     |Intank Fuel Pump                    |
|0x7E0     |285         |0x11D           |65                 |0x41                   |0        |48       |14           |0                 |0.01953125       |0.0         |4         |               |                       |Throttle Fully Close Learn          |
|0x7E0     |319         |0x13F           |69                 |0x45                   |0        |39       |1            |0                 |25.0             |0.0         |4         |               |                       |Misfire RPM                         |
|0x7E0     |439         |0x1B7           |193                |0xC1                   |0        |0        |14           |0                 |1.0              |0.0         |4         |0              |NA                     |Transmission Type                   |
|0x7E0     |439         |0x1B7           |193                |0xC1                   |0        |0        |14           |0                 |1.0              |0.0         |4         |1              |MT                     |Transmission Type                   |
|0x7E0     |439         |0x1B7           |193                |0xC1                   |0        |0        |14           |0                 |1.0              |0.0         |4         |2              |ECT 4th                |Transmission Type                   |
|0x7E0     |439         |0x1B7           |193                |0xC1                   |0        |0        |14           |0                 |1.0              |0.0         |4         |4              |ECT 5th                |Transmission Type                   |
|0x7E0     |439         |0x1B7           |193                |0xC1                   |0        |0        |14           |0                 |1.0              |0.0         |4         |8              |ECT 6th                |Transmission Type                   |
|0x7E0     |439         |0x1B7           |193                |0xC1                   |0        |0        |14           |0                 |1.0              |0.0         |4         |16             |CVT                    |Transmission Type                   |
|0x7E0     |439         |0x1B7           |193                |0xC1                   |0        |0        |14           |0                 |1.0              |0.0         |4         |32             |MMT(5th)               |Transmission Type                   |
|0x7E0     |439         |0x1B7           |193                |0xC1                   |0        |0        |14           |0                 |1.0              |0.0         |4         |64             |MMT(6th)               |Transmission Type                   |
|0x7E0     |439         |0x1B7           |193                |0xC1                   |0        |0        |14           |0                 |1.0              |0.0         |4         |128            |ECT 8th                |Transmission Type                   |
|0x7E0     |470         |0x1D6           |37                 |0x25                   |0        |0        |8            |6                 |1.0              |0.0         |4         |0              |OFF                    |Transfer Neutral                    |
|0x7E0     |470         |0x1D6           |37                 |0x25                   |0        |0        |8            |6                 |1.0              |0.0         |4         |1              |ON                     |Transfer Neutral                    |
|0x7E0     |487         |0x1E7           |81                 |0x51                   |0        |57       |9            |0                 |1.0              |-40.0       |4         |               |                       |Engine Oil Temperature              |
|0x7E0     |500         |0x1F4           |81                 |0x51                   |0        |0        |35           |6                 |1.0              |0.0         |4         |0              |OFF                    |Neutral Control                     |
|0x7E0     |500         |0x1F4           |81                 |0x51                   |0        |0        |35           |6                 |1.0              |0.0         |4         |1              |ON                     |Neutral Control                     |
|0x7E0     |505         |0x1F9           |81                 |0x51                   |0        |0        |36           |7                 |1.0              |0.0         |4         |0              |OFF                    |Immobiliser Fuel Cut                |
|0x7E0     |505         |0x1F9           |81                 |0x51                   |0        |0        |36           |7                 |1.0              |0.0         |4         |1              |ON                     |Immobiliser Fuel Cut                |
|0x7E0     |905         |0x389           |57                 |0x39                   |0        |0        |5            |6                 |1.0              |0.0         |4         |0              |OFF                    |Intank Fuel Pump                    |
|0x7E0     |905         |0x389           |57                 |0x39                   |0        |0        |5            |6                 |1.0              |0.0         |4         |1              |ON                     |Intank Fuel Pump                    |
|0x7E0     |935         |0x3A7           |1                  |0x1                    |0        |39       |10           |0                 |0.25             |0.0         |4         |               |                       |Engine Speed                        |
|0x7E0     |1157        |0x485           |193                |0xC1                   |0        |0        |18           |0                 |1.0              |0.0         |4         |0              |NA                     |Transmission Type2                  |
|0x7E0     |1157        |0x485           |193                |0xC1                   |0        |0        |18           |0                 |1.0              |0.0         |4         |128            |ASG                    |Transmission Type2                  |
|0x7E0     |1157        |0x485           |193                |0xC1                   |0        |0        |18           |0                 |1.0              |0.0         |4         |64             |ECT 10th               |Transmission Type2                  |

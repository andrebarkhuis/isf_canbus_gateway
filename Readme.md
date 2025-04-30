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

---

### 2. **Send Periodic Messages to ISF CAN Bus**

- **Initialization and Diagnostics Messages**:
  - Continuously send the following to the ISF CAN bus:
    - **Session Initialization Requests**: Required to maintain communication with the ISF ECUs during runtime.
    - **Diagnostic Trouble Code (DTC) Requests**: Periodic queries to monitor and capture error codes.
    - **OBD-II Requests**: Requests to read standard sensor data from the ISF ECUs.
- **Timing and Frequency**:
  - Ensure these messages are sent at regular intervals (e.g., every 1 second for initialization and diagnostics).

---

### 3. **Fallback to Default GT86 Messages**

- **Default Messages**:
  - Pre-defined arrays of GT86 messages simulate a parked car (e.g., all values zeroed or baseline).
  - GT86 Message IDs to be sent by default: `208`, `209`, `210`, `211`, `212`, `1953`, `322`, `1961`, `1224`
- **Fallback Mechanism**:
  - If no relevant ISF CAN messages are received within a defined timeout (e.g., 50ms), send the default GT86 messages.

---

### 4. **Message Replacement Logic**

- Some ISF CAN messages require translation into multiple GT86 messages. For example, Speed from ISF (ID 0x180) maps to GT86 CAN IDs 209 and 212.
- The gateway must andle these 1-to-many mappings accurately and in real time to ensure compatibility.
- Ensure that transformed messages from the ISF are prioritized and replace their respective default GT86 messages during operation.
- Default messages for IDs that are replaced should not be sent.

---

## Message Summary

### **ISF CAN Messages Sent**

| **ID (Hex)** | **Data (Bytes)**        | **Description**                       |
|--------------|-------------------------|---------------------------------------|
| 0x7DF        | 02 10 01 00 00 00 00 00 | Session Initialization Request         |
| 0x7E0        | 02 01 00 00 00 00 00 00 | Engine DTC Request                     |
| 0x7B0        | 02 09 00 00 00 00 00 00 | Transmission DTC Request               |
| 0x7DF        | 02 01 0C 00 00 00 00 00 | OBD-II Request (RPM)                   |
| 0x7DF        | 02 01 0D 00 00 00 00 00 | OBD-II Request (Vehicle Speed)         |
| 0x7DF        | 02 01 11 00 00 00 00 00 | OBD-II Request (Throttle Position)     |
| 0x7DF        | 02 01 05 00 00 00 00 00 | OBD-II Request (Coolant Temperature)   |
| 0x7DF        | 02 01 14 00 00 00 00 00 | OBD-II Request (Oxygen Sensor Voltage) |
| 0x7DF        | 02 01 03 00 00 00 00 00 | OBD-II Request (Fuel System Status)    |
| 0x7DF        | 02 01 0F 00 00 00 00 00 | OBD-II Request (Intake Air Temperature)|
| 0x7DF        | 02 01 3C 00 00 00 00 00 | OBD-II Request (Catalyst Temperature)  |
| 0x7DF        | 02 01 33 00 00 00 00 00 | OBD-II Request (Barometric Pressure)   |
| 0x7DF        | 02 01 23 00 00 00 00 00 | OBD-II Request (Fuel Rail Pressure)    |

---

### **Default GT86 Messages**

| Hex ID | Data (Bytes)         | Description                                                      |
|--------|----------------------|------------------------------------------------------------------|
| `0xD0` | `8B FB 25 00 01 08 04 01` | Steering Angle and Lateral/Longitudinal G-Forces (ID 208)        |
| `0xD1` | `F0 07 00 00 B8 0F FC 08` | Vehicle Speed and Brake Pressure (ID 209)                        |
| `0xD2` | `00 00 FF FF 00 00 00 00` | Unknown State Message (ID 210)                                   |
| `0xD3` | `00 06 C0 0F 00 00 32 00` | VSC and Traction Control Lights (ID 211)                         |
| `0xD4` | `F0 07 F0 07 F0 07 F0 07` | Wheel Speeds (ID 212)                                            |
| `0x140`| `00 05 00 43 00 00 00 01` | Clutch, Throttle, and Engine RPM (ID 320)                        |
| `0x141`| `00 00 00 00 00 83 A7 00` | Accelerator, Engine Load, RPM, Signals (ID 321)                  |
| `0x360`| `00 00 84 50 00 00 11 00` | Coolant Temp, Oil Temp, Cruise Control (ID 864)                  |
| `0x361`| `00 29 00 DD 23 00 A4 34` | Gear Position (ID 865)                                           |
| `0x370`| `00 00 01 01 00 03 00 00` | Unknown but required (ID 880)                                    |
| `0x7A1`| `02 19 81 00 00 00 00 00` | Unknown but required (ID 1953)                                   |
| `0x142`| `00 00 00 00 00 00 00 00` | Unknown but required (ID 322)                                    |
| `0x7A9`| `02 83 00 00 00 00 00 00` | Unknown but required (ID 1961)                                   |
| `0x4C8`| `09 00 00 00 00 00 00 00` | Unknown but required (ID 1224)                                   |

---

### **ISF Messages for Translation**

Function name:
 std::vector<CANMessage> TranslateToGt86(const twai_message_t& message)

| ID    | Description                 | Target GT86 IDs     | Notes                                                             | Function Name                |
|-------|-----------------------------|---------------------|-------------------------------------------------------------------|------------------------------|
| 180   | Speed                       | `209`, `212`        | Speed spans multiple GT86 messages.                               | mapSpeed                     |
| 708   | RPM                         | `320`, `321`        | RPM spans multiple GT86 messages.                                 | mapRpm                       |
| 864   | Temperatures                | `864`               | Engine oil temperature (C - 40), Coolant temperature (D - 40)     | mapWaterTemp                 |

---

## Implementation Requirements

1. **Real-Time Operation**:
   - Messages must adhere to their specified frequencies (e.g., 1000Hz, 500Hz, 200Hz).

2. **Maintain Modular Design**:
   - Translation logic should remain independent to facilitate future updates.

3. **Prioritization**:
   - Always prioritize transformed messages over default values.

4. **Scalability**:
   - Allow for easy addition of new translation mappings, default messages, and ISF request types.

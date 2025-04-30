
# DTC Decoding Logic

This document provides the logic for decoding Diagnostic Trouble Codes (DTCs) for both the Engine ECU and ABS ECU. The sample datasets contain unique CAN messages captured during diagnostic sessions.

## Engine ECU DTC Decoding

### Sample Dataset (Engine ECU)

```plaintext

```bash
| CAN Bus ID | Direction | D1   | D2   | D3   | D4   | D5   | D6   | D7   | D8   |
|------------|------------|------|------|------|------|------|------|------|------|
| 0x7E8      | Rx         | 0x03 | 0x41 | 0x01 | 0x81 | 0x07 | 0x21 | 0x21 | 0x00 |
| 0x7E8      | Rx         | 0x06 | 0x41 | 0x01 | 0x81 | 0x07 | 0x21 | 0x21 | 0x00 |
| 0x7E8      | Rx         | 0x04 | 0x53 | 0x01 | 0x03 | 0x35 | 0x00 | 0x00 | 0x00 |
| 0x7E8      | Rx         | 0x02 | 0x10 | 0x5F | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 |
| 0x7E8      | Rx         | 0x04 | 0x47 | 0x01 | 0x03 | 0x35 | 0x00 | 0x00 | 0x00 |

```bash

### Decoding Logic (Engine ECU)

The DTC code is determined using the following bytes:

- **D3 and D4**: These bytes represent the Diagnostic Trouble Code.

- **D2**: This byte represents the DTC status.

For example:

- **DTC Code**: Combine D3 and D4 to form the DTC code (e.g., `0x03` and `0x41` form `P0341`).

- **DTC Status**: D2 represents the status of the DTC (e.g., current, pending, history).

## ABS ECU DTC Decoding

### Sample Dataset (ABS ECU)

```plaintext

```bash
| CAN Bus ID | Direction | D1   | D2   | D3   | D4   | D5   | D6   | D7   | D8   |
|------------|------------|------|------|------|------|------|------|------|------|
| 0x7B0      | Rx         | 0x04 | 0x12 | 0x0F | 0x01 | 0x02 | 0x00 | 0x00 | 0x00 |
| 0x7B8      | Rx         | 0x06 | 0x52 | 0x0F | 0x00 | 0x00 | 0x01 | 0x02 | 0x00 |
| 0x7B0      | Rx         | 0x04 | 0x12 | 0x0E | 0x01 | 0x02 | 0x00 | 0x00 | 0x00 |
| 0x7B8      | Rx         | 0x06 | 0x52 | 0x0E | 0x00 | 0x00 | 0x01 | 0x02 | 0x00 |
| 0x7B0      | Rx         | 0x04 | 0x12 | 0x0D | 0x01 | 0x02 | 0x00 | 0x00 | 0x00 |

```bash

### Decoding Logic (ABS ECU)

The DTC code is determined using the following bytes:

- **D3 and D4**: These bytes represent the Diagnostic Trouble Code.

- **D2**: This byte represents the DTC status.

For example:

- **DTC Code**: Combine D3 and D4 to form the DTC code (e.g., `0x0F` and `0x01` form `C1201`).

- **DTC Status**: D2 represents the status of the DTC (e.g., current, pending, history).

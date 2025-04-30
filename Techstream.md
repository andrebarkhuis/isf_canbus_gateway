🧠 Toyota/Lexus ECUs & Techstream
Techstream is Toyota/Lexus's official diagnostic software. It communicates with vehicle ECUs using ISO-TP over CAN and primarily uses UDS (Unified Diagnostic Services).

Toyota ECUs use standard UDS SIDs like:

0x3E – Tester Present (keep-alive)

0x21 – ReadDataByLocalIdentifier (Toyota proprietary PIDs)

0x22 – ReadDataByIdentifier (standard UDS DIDs)

Toyota PIDs (aka Local Identifiers) are accessed via SID 0x21 and are always 2 bytes, even for 1-byte values like 0xBF → padded as 0x00BF.

Request payloads typically follow this format:
[length byte, SID, PID_high, PID_low, ...padding...]
Example: {0x03, 0x21, 0x00, 0xBF, 0x00, 0x00, 0x00, 0x00}

All PID requests are sent to a transmit CAN ID (e.g. 0x7E0) and responses are received from the response CAN ID (e.g. 0x7E8).

ECUs expect Tester Present (0x3E 00) messages every few seconds to prevent session timeout.

Toyota often reuses proprietary PID maps across models and years, but values may shift depending on firmware version.

ISO-TP is required for responses longer than 7 bytes, and flow control must be handled.


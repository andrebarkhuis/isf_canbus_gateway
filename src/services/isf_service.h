#pragma once

#include <Arduino.h>
#include "../mcp_can/mcp_can.h"
#include "../iso_tp/iso_tp.h"
#include "../uds/uds.h"
#include "../common_types.h"

namespace std
{
    template <>
    struct hash<std::tuple<uint16_t, int, int>>
    {
        size_t operator()(const std::tuple<uint16_t, int, int> &k) const
        {
            size_t h1 = std::hash<uint16_t>{}(std::get<0>(k));
            size_t h2 = std::hash<int>{}(std::get<1>(k));
            size_t h3 = std::hash<int>{}(std::get<2>(k));
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };
}

class MCP_CAN;
class IsoTp;
class UDS;

// #define DEBUG_ISF

inline UDSRequest isf_session_messages[] = {
    /* functional broadcast first – many Toyota ECUs want this         */
    {0x7DF, 0x7E8, UDS_SID_DIAGNOSTIC_SESSION_CONTROL, 0x81,   0, "Extended session (functional)"},
    /* direct to the ECM (0x7E0) in case the functional failed         */
    {0x7E0, 0x7E8, UDS_SID_DIAGNOSTIC_SESSION_CONTROL, 0x81,   0, "Extended session (ECM)"},
};

inline UDSRequest isf_messages[] = {
    /* 1‑sec keep‑alive so the ECU stays in session                    */
    {0x7E0, 0x7E8, UDS_SID_TESTER_PRESENT,         0x00, 1000, "Tester Present – keep‑alive"},
    /* === Toyota private service 0x21 (1‑byte Local IDs) ============= */
    {0x7E0, 0x7E8, UDS_SID_READ_DATA_BY_LOCAL_ID,  0x08, 1000, "Coolant Temp"},
    {0x7E0, 0x7E8, UDS_SID_READ_DATA_BY_LOCAL_ID,  0x09, 100, "Engine Speed"},
    {0x7E0, 0x7E8, UDS_SID_READ_DATA_BY_LOCAL_ID,  0x0A, 100, "Vehicle Speed"},
    {0x7E0, 0x7E8, UDS_SID_READ_DATA_BY_LOCAL_ID,  0x21, 1000, "Target Air‑Fuel Ratio"},
    {0x7E0, 0x7E8, UDS_SID_READ_DATA_BY_LOCAL_ID,  0x3A, 1000, "Fuel System Monitor"},
    {0x7E0, 0x7E8, UDS_SID_READ_DATA_BY_LOCAL_ID,  0x6C, 1000, "A/T Oil Temp (ECT)"},
    {0x7E0, 0x7E8, UDS_SID_READ_DATA_BY_LOCAL_ID,  0x73, 1000, "Engine Oil Pressure SW"},
    {0x7E0, 0x7E8, UDS_SID_READ_DATA_BY_LOCAL_ID,  0x93, 1000, "Neutral Position SW"},
    {0x7E0, 0x7E8, UDS_SID_READ_DATA_BY_LOCAL_ID,  0xBB, 1000, "Initial Intake Air Temp"},
    {0x7E0, 0x7E8, UDS_SID_READ_DATA_BY_LOCAL_ID,  0xBF, 1000, "Knock Correct Learn"},
    {0x7E0, 0x7E8, UDS_SID_READ_DATA_BY_LOCAL_ID,  0xC0, 1000, "Knock Feedback"},
    {0x7E0, 0x7E8, UDS_SID_READ_DATA_BY_LOCAL_ID,  0xE0, 1000, "Fuel Shut‑off Valve"},
    {0x7E0, 0x7E8, UDS_SID_READ_DATA_BY_LOCAL_ID,  0xE3, 1000, "Idle Fuel‑Cut Prohibit"},
    {0x7E0, 0x7E8, UDS_SID_READ_DATA_BY_LOCAL_ID,  0xED, 1000, "Intank Fuel Pump"},
    {0x7E0, 0x7E8, UDS_SID_READ_DATA_BY_LOCAL_ID,  0x1D, 1000, "Throttle Fully‑Close Learn"},
    {0x7E0, 0x7E8, UDS_SID_READ_DATA_BY_LOCAL_ID,  0x3F, 1000, "Misfire RPM"},
    {0x7E0, 0x7E8, UDS_SID_READ_DATA_BY_LOCAL_ID,  0xB7, 1000, "Transmission Type"}
};

inline const int ISF_SESSION_MESSAGE_SIZE = sizeof(isf_session_messages) / sizeof(isf_session_messages[0]);
inline const int ISF_MESSAGES_SIZE = sizeof(isf_messages) / sizeof(isf_messages[0]);

class IsfService
{
public:
    IsfService();
    ~IsfService();

    bool initialize();
    void listen();

private:
    bool initialize_diagnostic_session();
    bool beginSend();
    uint8_t buildRequestPayload(const UDSRequest &req, uint8_t *out);
    void sendPidRequest(const UDSRequest &request);
    void sendUdsRequest(const UDSRequest &request);
    bool processUdsResponse(Message_t &msg, const UDSRequest &request);
    bool transformResponse(Message_t &msg, const UDSRequest &request);

    // CAN bus interface for communication with ECUs
    MCP_CAN *mcp = nullptr;

    // ISO-TP protocol handler for multi-frame messaging
    IsoTp *isotp = nullptr;

    // UDS protocol handler for diagnostic services
    UDS *uds = nullptr;

    // Array tracking last request time for each UDS request
    unsigned long *lastUdsRequestTime = nullptr;
};
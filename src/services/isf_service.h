#pragma once

#include <Arduino.h>
#include "../mcp_can/mcp_can.h"
#include "../isotp/iso_tp.h"
#include "../uds/uds.h"
#include "../common.h"

class MCP_CAN;
class IsoTp;
class UDS;

#define DEBUG_ISF

const CANMessage isf_pid_session_requests[] = {
    {0x700, {0x02, 0x3E, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00}, 8, false, 2000, "Tester present - Keep Alive"},
    {0x7E0, {0x02, 0x3E, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00}, 8, false, 2000, "Tester present - Keep Alive"}
};

const UDSRequest isf_uds_requests[] = {

    {0x7E0, 0x7E8, 0x21, 0x01, 100, "Vehicle Load, Coolant Temp, Engine Speed, Vehicle Speed"},
    {0x7E0, 0x7E8, 0x21, 0x25, 1000, "Fuel Lid, Shift SWs, Sports/Snow Modes, A/C & Filter Signals"},
};

const int SESSION_REQUESTS_SIZE = sizeof(isf_pid_session_requests) / sizeof(isf_pid_session_requests[0]);
const int ISF_UDS_REQUESTS_SIZE = sizeof(isf_uds_requests) / sizeof(isf_uds_requests[0]);

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
    bool sendUdsRequest(uint8_t *udsMessage, uint8_t dataLength, const UDSRequest &request);
    bool processUdsResponse(uint8_t *data, uint8_t length, const UDSRequest &request);
    bool transformResponse(uint8_t *data, uint8_t length, const UDSRequest &request);

    // CAN bus interface for communication with ECUs
    MCP_CAN *mcp = nullptr;

    // ISO-TP protocol handler for multi-frame messaging
    IsoTp *isotp = nullptr;

    // UDS protocol handler for diagnostic services
    UDS *uds = nullptr;

    // Array tracking last request time for each UDS request
    unsigned long *lastUdsRequestTime = nullptr;
};
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

    //Default pid to 0 for now since we making the uds requests using the local id data identifier.
    {0x7E0, 0x7E8, 0x21, 0, 0x1, 100, ""},
    {0x7E0, 0x7E8, 0x21, 0, 0x3, 500, ""},
    {0x7E0, 0x7E8, 0x21, 0, 0x4, 500, ""},
    {0x7E0, 0x7E8, 0x21, 0, 0x6, 500, ""},
    {0x7E0, 0x7E8, 0x21, 0, 0x7, 500, ""},
    {0x7E0, 0x7E8, 0x21, 0, 0x8, 500, ""},
    {0x7E0, 0x7E8, 0x21, 0, 0x22, 500, ""},
    {0x7E0, 0x7E8, 0x21, 0, 0x25, 500, ""},
    {0x7E0, 0x7E8, 0x21, 0, 0x37, 500, ""},
    {0x7E0, 0x7E8, 0x21, 0, 0x39, 500, ""},
    {0x7E0, 0x7E8, 0x21, 0, 0x48, 500, ""},
    {0x7E0, 0x7E8, 0x21, 0, 0x49, 500, ""},
    {0x7E0, 0x7E8, 0x21, 0, 0x51, 500, ""},
    {0x7E0, 0x7E8, 0x21, 0, 0x82, 500, ""},
    {0x7E0, 0x7E8, 0x21, 0, 0x83, 500, ""},
    {0x7E0, 0x7E8, 0x21, 0, 0x85, 500, ""}
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
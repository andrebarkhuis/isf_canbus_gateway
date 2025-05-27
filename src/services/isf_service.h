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
    { .id = 0x700, .data = {0x02, 0x3E, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00}, .len = 8, .extended = false, .interval = 2400, .param_name = "Tester present - Keep Alive" },
    { .id = 0x7E0, .data = {0x02, 0x3E, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00}, .len = 8, .extended = false, .interval = 2400, .param_name = "Tester present - Keep Alive" },
    { .id = 0x7A1, .data = {0x02, 0x3E, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00}, .len = 8, .extended = false, .interval = 2400, .param_name = "Tester present - Keep Alive" },
    { .id = 0x7A2, .data = {0x02, 0x3E, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00}, .len = 8, .extended = false, .interval = 2400, .param_name = "Tester present - Keep Alive" },
    { .id = 0x7D0, .data = {0x02, 0x3E, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00}, .len = 8, .extended = false, .interval = 2400, .param_name = "Tester present - Keep Alive" }
};

const UDSRequest isf_uds_requests[] = {

    //Default pid to 0 for now since we making the uds requests using the local id data identifier.
    { .tx_id = 0x7DF, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x01, .interval = 50, .param_name = "request-1" },
    { .tx_id = 0x7DF, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x03, .interval = 50, .param_name = "request-2" },
    { .tx_id = 0x7DF, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x04, .interval = 50, .param_name = "request-3" },
    { .tx_id = 0x7DF, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x05, .interval = 50, .param_name = "request-3" },
    { .tx_id = 0x7DF, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x06, .interval = 50, .param_name = "request-4" },
    { .tx_id = 0x7DF, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x07, .interval = 50, .param_name = "request-5" },
    { .tx_id = 0x7DF, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x08, .interval = 50, .param_name = "request-6" },
    { .tx_id = 0x7DF, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x22, .interval = 50, .param_name = "request-7" },
    { .tx_id = 0x7DF, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x20, .interval = 50, .param_name = "request-7" },
    { .tx_id = 0x7DF, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x25, .interval = 50, .param_name = "request-8" },
    { .tx_id = 0x7DF, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x37, .interval = 50, .param_name = "request-9" },
    { .tx_id = 0x7DF, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x51, .interval = 50, .param_name = "request-13" },
    { .tx_id = 0x7DF, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x82, .interval = 50, .param_name = "request-14" },
    { .tx_id = 0x7DF, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x83, .interval = 50, .param_name = "request-15" },
    { .tx_id = 0x7DF, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0xE1, .interval = 50, .param_name = "request-15" },
    { .tx_id = 0x7DF, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0xC2, .interval = 150, .param_name = "engine-code" }
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
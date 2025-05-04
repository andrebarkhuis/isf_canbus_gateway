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

#define UDS_SID_READ_DATA_BY_ID 0x22
#define UDS_SID_READ_DATA_BY_LOCAL_ID 0x21
#define UDS_SID_TESTER_PRESENT 0x3E
#define OBD_MODE_SHOW_CURRENT_DATA 0x01
#define UDS_NRC_SUCCESS 0x00

extern UDSRequest isf_session_messages[];
extern UDSRequest isf_messages[];
extern const int ISF_SESSION_MESSAGE_SIZE;
extern const int ISF_MESSAGES_SIZE;
extern bool sessionActive;

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
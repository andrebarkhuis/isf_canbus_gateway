#include <Arduino.h>
#include "uds.h"
#include "../isotp/iso_tp.h"
#include "../mcp_can/mcp_can.h"
#include "../logger/logger.h"

volatile bool uds_busy = false;

UDS::UDS(IsoTp *isotp)
{
    _isotp = isotp;
}

uint16_t UDS::send(Message_t &msg)
{
    uint8_t retry = UDS_RETRY;
    uint16_t retval = 0;

    while ((retval = _isotp->send(&msg)) && retry)
    {
        retry--;
    }

    if (retval)
    {
        Logger::error("[UDS:send] Error sending message");
    }

    return retval;
}

void UDS::handlePendingResponse(Message_t &msg, boolean &isPending)
{
    Logger::debug("[UDS:handlePendingResponse] Pending response received. Retrying...");
    msg.Buffer = tmpbuf;
    isPending = true;
    delay(50); // small wait to give ECU time to respond
}

uint16_t UDS::handleNegativeResponse(Message_t &msg, Session_t *session)
{
    uint16_t retval = (uint16_t)UDS_ERROR_ID << 8 | msg.Buffer[1];

    session->Data = tmpbuf + 1;
    session->len = msg.len - 1;

    Logger::warn("[UDS:handleNegativeResponse] Negative response received, SID: 0x%02X, NRC: 0x%02X",
                 msg.Buffer[1], msg.Buffer[2]);

    return retval;
}

void UDS::handlePositiveResponse(Message_t &msg, Session_t *session)
{
    session->Data = tmpbuf + 1 + session->lenSub;
    session->len = msg.len - 1 - session->lenSub;

    Logger::info("[UDS:handlePositiveResponse] Positive response received, SID: 0x%02X, Length: %d",
        msg.Buffer[0], msg.len - 1 - session->lenSub);
}

uint16_t UDS::handleUDSResponse(Message_t &msg, Session_t *session, boolean &isPendingResponse)
{
    uint16_t retval = 0;

    int recv_result = _isotp->receive(&msg);
    if (recv_result != 0)
    {
        Logger::warn("[UDS:handleUDSResponse] Timeout after %u ms", UDS_TIMEOUT);
        return 0xDEAD;
    }

    if (msg.len < 3)
    {
        Logger::error("[UDS:handleUDSResponse] Incomplete response frame (len=%d)", msg.len);
        return 0xBEEF;
    }

    if (msg.Buffer[0] == UDS_ERROR_ID)
    {
        if (msg.Buffer[2] == UDS_NRC_RESPONSE_PENDING)
        {
            handlePendingResponse(msg, isPendingResponse);
        }
        else
        {
            retval = handleNegativeResponse(msg, session);
        }
    }
    else
    {
        handlePositiveResponse(msg, session);
    }

    return retval;
}

uint16_t UDS::Session(Session_t *session)
{
    const uint32_t startTime = millis();
    const uint32_t maxWaitTime = 2000; // 2 seconds max wait for lock
    const uint16_t waitStep = 50;

    // === Wait for uds_busy lock ===
    while (uds_busy)
    {
        if (millis() - startTime > maxWaitTime)
        {
            Logger::error("[UDS:Session] UDS: Timeout waiting for previous session to complete (SID: 0x%02X)", session->sid);
            return 0xFADE; // Custom timeout error
        }
        Logger::debug("[UDS:Session] UDS: Waiting for uds_busy lock.");
        vTaskDelay(pdMS_TO_TICKS(waitStep));
    }

    // === Lock acquired ===
    uds_busy = true;
    Message_t msg;
    uint16_t retval = 0;
    bool isPendingResponse = false;

    // Prepare buffer
    memset(tmpbuf, 0, MAX_DATA);
    tmpbuf[0] = session->sid;
    memcpy(tmpbuf + 1, session->Data, session->len);

    msg.rx_id = session->rx_id;
    msg.tx_id = session->tx_id;
    msg.len = session->len + 1;
    msg.Buffer = tmpbuf;

    retval = send(msg);

    if (!retval)
    {
        do
        {
            isPendingResponse = false;
            retval = handleUDSResponse(msg, session, isPendingResponse);
        } while (isPendingResponse);
    }

    // === Always release lock ===
    uds_busy = false;
    return retval;
}

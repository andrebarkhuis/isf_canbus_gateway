#include <Arduino.h>
#include "uds.h"
#include "../iso_tp/iso_tp.h"
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
        Logger::error("[UDS:send]. Error sending message.");
    }

    return retval;
}

void UDS::handlePendingResponse(Message_t &msg, boolean &isPending)
{
    #ifdef UDS_DEBUG
    Logger::debug("[UDS:handlePendingResponse]. Pending response received, retrying!");
    #endif
    msg.Buffer = tmpbuf;
    isPending = true;
    
    vTaskDelay(pdMS_TO_TICKS(50));  // small wait to give ECU time to respond
}

uint16_t UDS::handleNegativeResponse(Message_t *msg)
{
    uint16_t retval = (uint16_t)UDS_ERROR_ID << 8 | msg->Buffer[1];
    Logger::error("UDS:handleNegativeResponse]. Negative response SID: 0x%02X, NRC: 0x%02X", msg->Buffer[1], msg->Buffer[2]);
    
    vTaskDelay(pdMS_TO_TICKS(5)); 

    return retval;
}

void UDS::handlePositiveResponse(Message_t *msg)
{
    #ifdef UDS_DEBUG
    Logger::info("[UDS:handlePositiveResponse]. Positive response SID: 0x%02X, len: %d", msg->Buffer[0], msg->len - 1);
    #endif

    vTaskDelay(pdMS_TO_TICKS(5)); 
}

uint16_t UDS::handleUDSResponse(Message_t *msg, boolean &isPendingResponse)
{
    vTaskDelay(pdMS_TO_TICKS(10)); 

    int recv_result = _isotp->receive(msg);
    
    vTaskDelay(pdMS_TO_TICKS(5)); 

    if (recv_result != 0)
    {
        Logger::error("[UDS:handleUDSResponse]. Receive error %d", recv_result);
        return 0xBEEF; // Custom error code for receive failure
    }

    if (msg->len < 3)
    {
        Logger::error("[UDS:handleUDSResponse]. Incomplete response frame (len=%d)", msg->len);
        return 0xDEAD; // Custom error code for incomplete frame
    }

    if (msg->Buffer[0] == UDS_ERROR_ID)
    {
        if (msg->Buffer[2] == UDS_NRC_RESPONSE_PENDING)
        {
            handlePendingResponse(*msg, isPendingResponse);
            return 0;
        }
        return handleNegativeResponse(msg);
    }
   
    handlePositiveResponse(msg);

    return 0;
}

uint16_t UDS::Session(Message_t *msg)
{
    const uint32_t startTime = millis();
    const uint32_t maxWaitTime = 2000; // 2 seconds max wait for lock
    const uint16_t waitStep = 5;

    // === Wait for uds_busy lock ===
    while (uds_busy)
    {
        if (millis() - startTime > maxWaitTime)
        {
            Logger::error("[UDS:Session]. Timeout waiting for previous session to complete (SID: 0x%02X)", msg->Buffer[0]);
            return 0xFADE; // Custom timeout error
        }
        
        #ifdef UDS_DEBUG
        Logger::debug("[UDS:Session]. Waiting for uds_busy lock.");
        #endif

        vTaskDelay(pdMS_TO_TICKS(waitStep));
    }

    // === Lock acquired ===
    uds_busy = true;
    uint16_t retval = 0;
    bool isPendingResponse = false;

    // Prepare buffer
    if (msg->len > MAX_DATA)
    {
        Logger::error("[UDS:Session]. Message length %d exceeds MAX_DATA %d", msg->len, MAX_DATA);
        uds_busy = false;
        return 0xBEEF; // Error code for buffer overflow
    }

    memset(tmpbuf, 0, MAX_DATA);
    memcpy(tmpbuf, msg->Buffer, msg->len);

    msg->Buffer = tmpbuf;

    retval = send(*msg);

    if (!retval)
    {
        do
        {
            isPendingResponse = false;
            retval = handleUDSResponse(msg, isPendingResponse);
        } while (isPendingResponse);
    }

    // === Always release lock ===
    uds_busy = false;
    return retval;
}
#include <Arduino.h>
#include "isotp/iso-tp.h"
#include "mcp_can/mcp_canbus.h"
#include <common/logger.h>

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
    vTaskDelay(pdMS_TO_TICKS(10)); // small wait to give ECU time to respond
}

uint16_t UDS::handleNegativeResponse(Message_t *msg)
{
    uint16_t retval = (uint16_t)UDS_ERROR_ID << 8 | msg->Buffer[1];

#ifdef UDS_DEBUG
    Logger::logUdsMessage("[ERROR] UDS:handleNegativeResponse. Error receiving response.", msg);
#endif
    vTaskDelay(pdMS_TO_TICKS(1));

    return retval;
}

void UDS::handlePositiveResponse(Message_t *msg)
{
#ifdef UDS_DEBUG
    Logger::logUdsMessage("[OK] UDS:handlePositiveResponse. Positive response received.", msg);
#endif
    vTaskDelay(pdMS_TO_TICKS(1));
}

uint16_t UDS::handleUDSResponse(Message_t *msg, boolean &isPendingResponse)
{
    vTaskDelay(pdMS_TO_TICKS(5));

    int recv_result = _isotp->receive(msg);

    vTaskDelay(pdMS_TO_TICKS(5));

    if (recv_result != 0)
    {
        Logger::logUdsMessage("[ERROR] [UDS:handleUDSResponse] Error receiving response.", msg);

        return 0xBEEF; // Custom error code for receive failure
    }

    if (msg->len < 3)
    {
        Logger::logUdsMessage("[ERROR] [UDS:handleUDSResponse] Incomplete Frame", msg);

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

    vTaskDelay(pdMS_TO_TICKS(5));

    handlePositiveResponse(msg);

    return 0;
}

uint16_t UDS::Session(Message_t *msg)
{
    vTaskDelay(pdMS_TO_TICKS(5));

    const uint32_t startTime = millis();
    const uint32_t maxWaitTime = 5000; // 5 seconds max wait for lock

    // === Wait for uds_busy lock ===
    while (uds_busy)
    {
        if (millis() - startTime > maxWaitTime)
        {
            Logger::logUdsMessage("[ERROR] [UDS:Session] Timeout waiting for uds_busy lock, Previous session response not completed", msg);
            return 0xFADE; // Timeout error
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }

    // === Lock acquired ===
    uds_busy = true;
    uint16_t retval = 0;
    bool isPendingResponse = false;

    // Prepare buffer
    if (msg->len > MAX_DATA)
    {
        Logger::logUdsMessage("[ERROR] [UDS:Session] Message length exceeds MAX_DATA", msg);

        uds_busy = false;
        return 0xBEEF; // Buffer overflow error
    }

    // Check if msg->Buffer is allocated dynamically, or use a static temporary buffer
    uint8_t *originalBuffer = msg->Buffer;
    uint8_t localTmpBuf[MAX_DATA] = {0};

    // Ensure msg->Buffer isn't NULL
    if (msg->Buffer == nullptr)
    {
        Logger::logUdsMessage("[ERROR] [UDS:Session] msg->Buffer is NULL.", msg);
        uds_busy = false;
        return 0xDEAD; // Error code for NULL buffer
    }

    // Copy the data into a local buffer to avoid modifying the original message directly
    if (msg->Buffer != nullptr && msg->Buffer != tmpbuf)
    {
        memcpy(localTmpBuf, msg->Buffer, msg->len);
        msg->Buffer = localTmpBuf;
    }
    else
    {
        memcpy(tmpbuf, msg->Buffer, msg->len);
        msg->Buffer = tmpbuf;
    }

    vTaskDelay(pdMS_TO_TICKS(1));

    retval = send(*msg);

    if (!retval)
    {
        do
        {
            isPendingResponse = false;
            retval = handleUDSResponse(msg, isPendingResponse);
        } while (isPendingResponse);
    }

    // Restore original buffer if it was dynamically allocated
    if (originalBuffer != nullptr && originalBuffer != tmpbuf)
    {
        msg->Buffer = originalBuffer;
    }

    // === Always release lock ===
    uds_busy = false;
    return retval;
}

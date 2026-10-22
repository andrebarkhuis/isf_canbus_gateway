#pragma once

#include <cstdint>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "common.h"
#include "mcp_can/mcp_can.h"
#include "message_translator.h"

/**
 * @brief The second bus (MCP2515, CAN 2): the GT86 modules and the ECUmaster ADU.
 *
 * Keeps every output frame of the translator table on the bus at its interval. A frame starts as its default
 * bytes (the GT86 "cluster working, no errors" capture for the GT86 packets; zeros for the ADU frames). When
 * the ISF task translates fresh values, the frame's bytes are replaced through the queue; a frame that has not
 * been refreshed for TranslationHoldMs falls back to its defaults (Readme §3), so a switched-off engine does
 * not leave stale readings on the dash. Frames from the GT86 bus are received and discarded.
 */
class Gt86Service
{
public:
    static constexpr uint8_t MCP2515_CS_PIN = 10;
    /// After this long without a fresh translation a frame reverts to its default bytes. The slowest polled
    /// block is refreshed every few seconds, so the hold must be longer than one full rotation.
    static constexpr uint32_t TranslationHoldMs = 5000;
    static constexpr int MaxFrames = MessageTranslator::MaxFrames;

    explicit Gt86Service(QueueHandle_t translatedFrames);
    ~Gt86Service();

    /// Brings up the MCP2515 at 500 kbit/s.
    bool initialize();
    /// One step: take translated frames from the queue, send what is due, drain what came in.
    void listen();

private:
    void takeTranslations();
    bool sendDue();
    void handleIncomingMessages();
    int frameIndex(uint32_t id) const;

    MCP_CAN mcp_;
    QueueHandle_t queue_;
    CANMessage frames_[MaxFrames];
    uint8_t defaults_[MaxFrames][8];
    uint32_t lastSentMs_[MaxFrames];
    uint32_t lastTranslatedMs_[MaxFrames];
    int frameCount_ = 0;
};

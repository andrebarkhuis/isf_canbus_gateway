#include "services/gt86_service.h"

#include <cstring>

#include "logger/logger.h"

Gt86Service::Gt86Service(QueueHandle_t translatedFrames) : mcp_(MCP2515_CS_PIN), queue_(translatedFrames)
{
    frameCount_ = 0;
    for (size_t i = 0; i < OUTPUT_FRAMES_COUNT && frameCount_ < MaxFrames; i++)
    {
        CANMessage &m = frames_[frameCount_];
        m.id = OUTPUT_FRAMES[i].id;
        memcpy(m.data, OUTPUT_FRAMES[i].defaults, 8);
        memcpy(defaults_[frameCount_], OUTPUT_FRAMES[i].defaults, 8);
        m.len = 8;
        m.extended = OUTPUT_FRAMES[i].extended;
        m.interval = OUTPUT_FRAMES[i].intervalMs;
        m.param_name = OUTPUT_FRAMES[i].name;
        lastSentMs_[frameCount_] = 0;
        lastTranslatedMs_[frameCount_] = 0;
        frameCount_++;
    }
}

Gt86Service::~Gt86Service()
{
}

bool Gt86Service::initialize()
{
    byte res = mcp_.begin();
    if (res != CAN_OK)
    {
        LOG_ERROR("MCP2515 (GT86 bus) failed to start: %d", res);
        return false;
    }
    LOG_INFO("MCP2515 (GT86 bus) started at 500 kbit/s, %d output frames", frameCount_);
    return true;
}

void Gt86Service::listen()
{
    takeTranslations();
    sendDue();
    handleIncomingMessages();
}

int Gt86Service::frameIndex(uint32_t id) const
{
    for (int i = 0; i < frameCount_; i++)
    {
        if (frames_[i].id == id)
        {
            return i;
        }
    }
    return -1;
}

void Gt86Service::takeTranslations()
{
    if (queue_ == nullptr)
    {
        return;
    }
    CANMessage translated;
    while (xQueueReceive(queue_, &translated, 0) == pdTRUE)
    {
        int i = frameIndex(translated.id);
        if (i < 0)
        {
            continue;
        }
        memcpy(frames_[i].data, translated.data, 8);
        lastTranslatedMs_[i] = nowMs();
    }
}

bool Gt86Service::sendDue()
{
    uint32_t now = nowMs();
    bool success = true;
    for (int i = 0; i < frameCount_; i++)
    {
        CANMessage &m = frames_[i];
        if (m.interval == 0 || now - lastSentMs_[i] < m.interval)
        {
            continue;
        }
        if (lastTranslatedMs_[i] != 0 && now - lastTranslatedMs_[i] > TranslationHoldMs)
        {
            // No fresh value for a while: back to the default bytes.
            memcpy(m.data, defaults_[i], 8);
            lastTranslatedMs_[i] = 0;
        }
        lastSentMs_[i] = now;
        if (mcp_.sendMsgBuf(m.id, m.extended ? 1 : 0, m.len, m.data) != CAN_OK)
        {
            LOG_WARN("GT86 bus: send %03lX (%s) failed", static_cast<unsigned long>(m.id), m.param_name);
            success = false;
        }
    }
    return success;
}

void Gt86Service::handleIncomingMessages()
{
    // The GT86 modules' own traffic is not consumed; empty the controller so it does not overflow.
    int drained = 0;
    while (mcp_.checkReceive() == CAN_MSGAVAIL && drained < 8)
    {
        unsigned long id;
        byte len;
        byte data[8];
        mcp_.readMsgBufID(&id, &len, data);
        drained++;
    }
}

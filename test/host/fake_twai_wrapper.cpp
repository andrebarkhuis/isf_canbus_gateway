// The firmware's TwaiWrapper over the scripted FakeBus, so IsoTp and IsfService run unchanged on the host.
#include "can/twai_wrapper.h"

#include <cstring>

#include "fake_bus.h"
#include "logger/logger.h"

TwaiWrapper::TwaiWrapper()
{
}

TwaiWrapper::~TwaiWrapper()
{
}

bool TwaiWrapper::initialize()
{
    open_ = true;
    fakeBus().open = true;
    return true;
}

bool TwaiWrapper::isOpen() const
{
    return open_ && fakeBus().open;
}

bool TwaiWrapper::send(uint32_t id, const uint8_t *data, uint8_t len, bool)
{
    Frame f;
    f.id = id;
    f.len = len;
    memcpy(f.data, data, len);
    fakeBus().send(f);
    LOG_FRAME(INTERFACE, id, data, len);
    return true;
}

bool TwaiWrapper::tryReceive(uint32_t &id, uint8_t *data, uint8_t &len, uint32_t timeoutMs)
{
    Frame f;
    if (!fakeBus().tryReceive(f, static_cast<int>(timeoutMs)))
    {
        return false;
    }
    id = f.id;
    len = f.len;
    memcpy(data, f.data, f.len);
    LOG_FRAME(INTERFACE, id, data, len);
    return true;
}

void TwaiWrapper::drain()
{
    fakeBus().drain();
}

void TwaiWrapper::checkAlerts()
{
}

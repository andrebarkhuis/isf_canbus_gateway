#include "services/isf_service.h"

#include <cstring>

#include "can/twai_wrapper.h"
#include "isf_broadcast.h"
#include "isotp/iso_tp.h"
#include "logger/logger.h"

namespace
{
/// Blocks read every cycle: engine 21 01 (rpm, speed, coolant, throttle) and ABS 21 03 (wheel speeds).
struct FastBlock
{
    uint32_t requestId;
    uint8_t parameterAddress;
};
constexpr FastBlock FAST_BLOCKS[] = {{0x7E0, 0x01}, {0x7B0, 0x03}, {0x7B0, 0x47}}; // rpm/speed, wheel speeds, G/yaw/steering
} // namespace

IsfService::IsfService()
{
    for (int i = 0; i < Modules::Count; i++)
    {
        modules_[i].module = &Modules::All[i];
    }
}

IsfService::~IsfService()
{
    if (ownsChannel_)
    {
        delete isotp_;
        delete twai_;
    }
}

bool IsfService::initialize()
{
    if (twai_ == nullptr)
    {
        twai_ = new TwaiWrapper();
        isotp_ = new IsoTp(twai_);
        ownsChannel_ = true;
    }
    isotp_->setTap(tap, this);
    if (!twai_->initialize())
    {
        LOG_ERROR("ISF bus failed to open");
        return false;
    }
    LOG_INFO("ISF session ready: %u parameter definitions, %u output frames",
             static_cast<unsigned>(UDS_DEFINITIONS_COUNT), static_cast<unsigned>(OUTPUT_FRAMES_COUNT));
    return true;
}

void IsfService::attach(TwaiWrapper *twai, IsoTp *isotp)
{
    twai_ = twai;
    isotp_ = isotp;
    ownsChannel_ = false;
    isotp_->setTap(tap, this);
}

size_t IsfService::request(const ModuleDefinition &m, const uint8_t *payload, size_t length, uint32_t timeoutMs)
{
    isotp_->target(m.requestId);
    return isotp_->request(payload, length, timeoutMs, reply_, sizeof(reply_));
}

// ------------------------------------------------------------------ the single interpreter

ReplyType IsfService::interpret(ModuleState &m, const uint8_t *request, size_t requestLength, const uint8_t *reply,
                                size_t replyLength, uint8_t &nrc)
{
    nrc = 0;
    if (reply == nullptr || replyLength == 0)
    {
        return ReplyType::Silent;
    }
    if (reply[0] == Services::NegativeResponse && replyLength >= static_cast<size_t>(Services::NegativeResponseLength))
    {
        nrc = reply[2];
        if (reply[1] == Services::ReadDataByLocalId && requestLength > 1)
        {
            LOG_INFO("%s: negative response to %02X: NRC %02X %s (parameter address %02X)", m.module->name, reply[1],
                     nrc, Nrc::name(nrc), request[1]);
        }
        else
        {
            LOG_INFO("%s: negative response to %02X: NRC %02X %s", m.module->name, reply[1], nrc, Nrc::name(nrc));
        }
        return ReplyType::Negative;
    }
    if (replyLength >= static_cast<size_t>(Services::DataMapHeaderLength) && reply[0] == Services::ReadDataMapReply &&
        reply[1] == Services::ReadDataMapSubFunction)
    {
        ResponseDataMap map;
        if (!ResponseDataMap::fromReply(reply, replyLength, map))
        {
            return ReplyType::Other;
        }
        int slot = polledIndex(m, map.parameterAddress);
        if (slot >= 0)
        {
            m.maps[slot] = map;
            m.hasMap[slot] = true;
        }
        LOG_INFO("%s: map 21 %02X: %d/%d present", m.module->name, map.parameterAddress, map.presentCount(),
                 map.fullLength);
        return ReplyType::Map;
    }
    if (replyLength >= static_cast<size_t>(Services::ReadDataHeaderLength) && reply[0] == Services::ReadDataByLocalIdReply)
    {
        int parameterAddress = reply[1];
        const uint8_t *raw = reply + Services::ReadDataHeaderLength;
        int rawLength = static_cast<int>(replyLength) - Services::ReadDataHeaderLength;
        if (SupportBitmap::isBitmapId(parameterAddress) && rawLength >= SupportBitmap::Length)
        {
            uint8_t ids[SupportBitmap::MaxIds];
            bool next = false;
            int count = SupportBitmap::parse(parameterAddress, raw, rawLength, ids, next);
            for (int i = 0; i < count; i++)
            {
                if (!m.supported[ids[i]])
                {
                    m.supported[ids[i]] = true;
                    m.supportedCount++;
                }
            }
            LOG_INFO("%s: bitmap 21 %02X -> %d ids%s", m.module->name, parameterAddress, count, next ? " (more)" : "");
            (void)next;
            return ReplyType::Bitmap;
        }
        decodeBlock(m, static_cast<uint8_t>(parameterAddress), raw, rawLength);
        return ReplyType::Block;
    }
    return ReplyType::Other;
}

void IsfService::decodeBlock(ModuleState &m, uint8_t parameterAddress, const uint8_t *raw, int rawLength)
{
    // Expand through the response data map when one is known (§3.3), then decode every parameter defined for
    // the block (§6.8): ToyotaLens DecodedBlock.Decode.
    int slot = polledIndex(m, parameterAddress);
    const ResponseDataMap *map = (slot >= 0 && m.hasMap[slot]) ? &m.maps[slot] : nullptr;
    uint8_t full[ResponseDataMap::MaxLayoutLength];
    const uint8_t *block = raw;
    int blockLength = rawLength;
    if (map != nullptr)
    {
        int n = map->expand(raw, rawLength, full);
        if (n < 0)
        {
            LOG_WARN("%s: 21 %02X map expects %d bytes, got %d; decoded raw layout", m.module->name, parameterAddress,
                     map->presentCount(), rawLength);
        }
        else
        {
            block = full;
            blockLength = n;
        }
    }
    const UdsDefinition *defs[MaxValuesPerBlock];
    int defCount = definitionsOf(m.module->requestId, parameterAddress, defs, MaxValuesPerBlock);
    ParameterValue values[MaxValuesPerBlock];
    int decoded = 0;
    for (int i = 0; i < defCount; i++)
    {
        values[i] = ParameterDecoder::decode(*defs[i], block, blockLength, map);
        if (values[i].isAvailable)
        {
            decoded++;
            LOG_DEBUG("%s 21 %02X %s = %g %s (raw %lld)", m.module->name, parameterAddress, defs[i]->name,
                      values[i].value, values[i].unit, static_cast<long long>(values[i].raw));
        }
    }
    LOG_DEBUG("%s: 21 %02X: %d bytes, %d/%d parameters decoded", m.module->name, parameterAddress, rawLength, decoded,
              defCount);

    CANMessage changed[MessageTranslator::MaxFrames];
    int changedCount = translator_.apply(values, defCount, changed, MessageTranslator::MaxFrames);
    publish(changed, changedCount);
}

void IsfService::publish(const CANMessage *frames, int count)
{
    for (int i = 0; i < count; i++)
    {
        if (sink_ != nullptr)
        {
            sink_(frames[i]);
        }
        forwardToIsf(frames[i]);
    }
}

// ------------------------------------------------------------------ broadcasts (the GT86 motion frames)

void IsfService::onFrame(uint32_t id, const uint8_t *data, uint8_t len)
{
    ParameterValue values[MaxValuesPerBlock];
    int n = BroadcastDecoder::decodeFrame(id, data, len, values, MaxValuesPerBlock);
    if (n == 0)
    {
        return;
    }
    for (int i = 0; i < n; i++)
    {
        if (values[i].isAvailable)
        {
            LOG_DEBUG("broadcast %03lX %s = %g %s (raw %lld)", static_cast<unsigned long>(id),
                      values[i].parameter->name, values[i].value, values[i].unit, static_cast<long long>(values[i].raw));
        }
    }
    CANMessage changed[MessageTranslator::MaxFrames];
    int changedCount = translator_.apply(values, n, changed, MessageTranslator::MaxFrames);
    publish(changed, changedCount);
}

void IsfService::tap(void *context, uint32_t id, const uint8_t *data, uint8_t len)
{
    static_cast<IsfService *>(context)->onFrame(id, data, len);
}

void IsfService::pumpBroadcast()
{
    // Bounded, so a flood never keeps the session from its next request (the bus carries ~1100 frames/s).
    constexpr int MaxPerCall = 128;
    uint32_t id;
    uint8_t data[8];
    uint8_t len;
    for (int i = 0; i < MaxPerCall && twai_->tryReceive(id, data, len, 0); i++)
    {
        onFrame(id, data, len);
    }
}

void IsfService::forwardToIsf(const CANMessage &frame)
{
    // The ADU frames flagged in gateway_frames.json go onto the ISF bus as well (29-bit ids, so they never meet
    // a module's id and lose every arbitration to them), at the frame's own interval rather than the poll rate.
    for (size_t i = 0; i < OUTPUT_FRAMES_COUNT && i < static_cast<size_t>(MessageTranslator::MaxFrames); i++)
    {
        if (OUTPUT_FRAMES[i].id != frame.id)
        {
            continue;
        }
        if (!OUTPUT_FRAMES[i].alsoOnIsfBus || twai_ == nullptr)
        {
            return;
        }
        uint32_t now = nowMs();
        isfLastTranslatedMs_[i] = now;
        if (isfLastSentMs_[i] != 0 && now - isfLastSentMs_[i] < frame.interval)
        {
            return;
        }
        isfLastSentMs_[i] = now;
        twai_->send(frame.id, frame.data, frame.len, frame.extended);
        return;
    }
}

void IsfService::repeatIsfFrames()
{
    if (twai_ == nullptr)
    {
        return;
    }
    const CANMessage *frames = translator_.frames();
    uint32_t now = nowMs();
    for (int i = 0; i < translator_.frameCount(); i++)
    {
        if (!OUTPUT_FRAMES[i].alsoOnIsfBus || isfLastTranslatedMs_[i] == 0 ||
            now - isfLastTranslatedMs_[i] > TranslationHoldMs || now - isfLastSentMs_[i] < frames[i].interval)
        {
            continue;
        }
        isfLastSentMs_[i] = now;
        twai_->send(frames[i].id, frames[i].data, frames[i].len, frames[i].extended);
    }
}

int IsfService::polledIndex(const ModuleState &m, uint8_t parameterAddress) const
{
    for (int i = 0; i < m.polledCount; i++)
    {
        if (m.polled[i] == parameterAddress)
        {
            return i;
        }
    }
    return -1;
}

bool IsfService::isFastBlock(uint32_t requestId, uint8_t parameterAddress)
{
    for (const FastBlock &f : FAST_BLOCKS)
    {
        if (f.requestId == requestId && f.parameterAddress == parameterAddress)
        {
            return true;
        }
    }
    return false;
}

// ------------------------------------------------------------------ live

bool IsfService::discover(ModuleState &m)
{
    const ModuleDefinition &module = *m.module;
    m.lastProbeMs = nowMs();
    uint8_t probe[] = {Services::ReadDataByLocalId, Services::ProbeParameterAddress};
    size_t probeLength = request(module, probe, sizeof(probe), probeTimeoutMs);
    if (probeLength == 0)
    {
        LOG_INFO("%s @ %03lX: absent (no reply to 21 00)", module.name, static_cast<unsigned long>(module.requestId));
        m.present = false;
        return false;
    }
    memset(m.supported, 0, sizeof(m.supported));
    m.supportedCount = 0;
    m.polledCount = 0;
    memset(m.hasMap, 0, sizeof(m.hasMap));

    uint8_t nrc;
    uint8_t probeReply[MAX_UDS_PAYLOAD_LEN];
    memcpy(probeReply, reply_, probeLength);
    for (int baseId = 0; baseId <= SupportBitmap::LastBaseId; baseId += SupportBitmap::Stride)
    {
        uint8_t req[] = {Services::ReadDataByLocalId, static_cast<uint8_t>(baseId)};
        const uint8_t *r = probeReply;
        size_t n = probeLength;
        if (baseId != Services::ProbeParameterAddress)
        {
            n = request(module, req, sizeof(req), dataTimeoutMs);
            r = reply_;
        }
        // The walk stops at the first bitmap that is refused or whose last bit is clear.
        bool next = false;
        if (n > 0 && r[0] == Services::ReadDataByLocalIdReply && n >= static_cast<size_t>(Services::ReadDataHeaderLength + SupportBitmap::Length))
        {
            uint8_t ids[SupportBitmap::MaxIds];
            SupportBitmap::parse(baseId, r + Services::ReadDataHeaderLength, n - Services::ReadDataHeaderLength, ids, next);
        }
        ReplyType kind = interpret(m, req, sizeof(req), r, n, nrc);
        if (kind != ReplyType::Bitmap || !next)
        {
            break;
        }
    }
    m.present = true;
    LOG_INFO("%s @ %03lX: present, %d supported parameter address(es)", module.name,
             static_cast<unsigned long>(module.requestId), m.supportedCount);

    // The blocks this gateway polls: every block with definitions that the module supports (R4.4).
    for (size_t i = 0; i < UDS_DEFINITIONS_COUNT && m.polledCount < MaxPolledBlocks; i++)
    {
        const UdsDefinition &d = UDS_DEFINITIONS[i];
        if (d.requestId != module.requestId || !m.supported[d.parameterAddress] || polledIndex(m, d.parameterAddress) >= 0)
        {
            continue;
        }
        m.polled[m.polledCount++] = d.parameterAddress;
    }
    int maps = 0;
    for (int i = 0; i < m.polledCount; i++)
    {
        uint8_t req[] = {Services::ReadDataMap, Services::ReadDataMapSubFunction, m.polled[i]};
        size_t n = request(module, req, sizeof(req), dataTimeoutMs);
        if (interpret(m, req, sizeof(req), reply_, n, nrc) == ReplyType::Map)
        {
            maps++;
        }
    }
    LOG_INFO("%s: polling %d block(s), data maps: %d/%d", module.name, m.polledCount, maps, m.polledCount);
    return true;
}

bool IsfService::read(ModuleState &m, uint8_t parameterAddress)
{
    uint8_t req[] = {Services::ReadDataByLocalId, parameterAddress};
    size_t n = request(*m.module, req, sizeof(req), dataTimeoutMs);
    uint8_t nrc;
    ReplyType kind = interpret(m, req, sizeof(req), reply_, n, nrc);
    if (kind == ReplyType::Block && reply_[1] == parameterAddress)
    {
        return true;
    }
    if (kind == ReplyType::Silent)
    {
        LOG_WARN("%s: 21 %02X: no reply", m.module->name, parameterAddress);
        // A module that stops answering is re-discovered (the car may have been switched off).
        m.present = false;
    }
    else if (kind == ReplyType::Negative && nrc == Nrc::SubFunctionNotSupported)
    {
        // §8.5: 7F 21 12 on a local id means "not implemented"; drop that id from the poll list.
        int slot = polledIndex(m, parameterAddress);
        if (slot >= 0)
        {
            for (int i = slot; i + 1 < m.polledCount; i++)
            {
                m.polled[i] = m.polled[i + 1];
                m.maps[i] = m.maps[i + 1];
                m.hasMap[i] = m.hasMap[i + 1];
            }
            m.polledCount--;
        }
    }
    return false;
}

void IsfService::fastStep(ModuleState &m)
{
    for (int i = 0; i < m.polledCount && m.present; i++)
    {
        if (isFastBlock(m.module->requestId, m.polled[i]))
        {
            read(m, m.polled[i]);
        }
    }
}

bool IsfService::slowStep()
{
    // One slow block per cycle, rotating over every present module's slow blocks in turn, so the fast blocks
    // (rpm, speed) come round every ~300 ms and the rest within a few seconds.
    int total = 0;
    for (int i = 0; i < Modules::Count; i++)
    {
        ModuleState &m = modules_[i];
        for (int j = 0; j < m.polledCount && m.present; j++)
        {
            if (!isFastBlock(m.module->requestId, m.polled[j]))
            {
                total++;
            }
        }
    }
    if (total == 0)
    {
        return false;
    }
    int pick = slowRotation_ % total;
    slowRotation_++;
    int seen = 0;
    for (int i = 0; i < Modules::Count; i++)
    {
        ModuleState &m = modules_[i];
        for (int j = 0; j < m.polledCount && m.present; j++)
        {
            if (isFastBlock(m.module->requestId, m.polled[j]))
            {
                continue;
            }
            if (seen++ == pick)
            {
                read(m, m.polled[j]);
                return true;
            }
        }
    }
    return false;
}

void IsfService::listen()
{
    if (twai_ == nullptr)
    {
        return;
    }
    twai_->checkAlerts();
    pumpBroadcast();
    for (int i = 0; i < Modules::Count; i++)
    {
        ModuleState &m = modules_[i];
        if (!m.present)
        {
            if (m.lastProbeMs == 0 || nowMs() - m.lastProbeMs >= rediscoverMs)
            {
                discover(m);
            }
            continue;
        }
        fastStep(m);
    }
    slowStep();
    repeatIsfFrames();
}

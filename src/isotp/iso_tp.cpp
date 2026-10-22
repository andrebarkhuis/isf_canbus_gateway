#include "isotp/iso_tp.h"

#include <cstring>

#include "can/twai_wrapper.h"
#include "common.h"
#include "logger/logger.h"
#include "uds/uds_mapper.h"

// ------------------------------------------------------------------ framing (ToyotaLens IsoTp)

namespace IsoTpFraming
{
int separationTimeMicroseconds(uint8_t st)
{
    if (st <= SeparationTimeMaxMs)
    {
        return st * MicrosecondsPerMillisecond;
    }
    if (st >= SeparationTimeMicroFirst && st <= SeparationTimeMicroLast)
    {
        return (st - SeparationTimeMicroFirst + 1) * SeparationTimeMicroStep;
    }
    return SeparationTimeMaxMs * MicrosecondsPerMillisecond;
}

size_t segmentCount(size_t payloadLength)
{
    if (payloadLength > static_cast<size_t>(MaxMessageLength))
    {
        return 0;
    }
    if (payloadLength <= static_cast<size_t>(SingleFramePayload))
    {
        return 1;
    }
    size_t rest = payloadLength - FirstFramePayload;
    return 1 + (rest + ConsecutiveFramePayload - 1) / ConsecutiveFramePayload;
}

bool segment(const uint8_t *payload, size_t payloadLength, size_t index, uint8_t frame[FrameLength])
{
    size_t count = segmentCount(payloadLength);
    if (count == 0 || index >= count)
    {
        return false;
    }
    memset(frame, Padding, FrameLength);
    if (count == 1)
    {
        frame[0] = singleFramePci(static_cast<int>(payloadLength));
        memcpy(frame + 1, payload, payloadLength);
        return true;
    }
    if (index == 0)
    {
        frame[0] = firstFramePciHigh(static_cast<int>(payloadLength));
        frame[1] = firstFramePciLow(static_cast<int>(payloadLength));
        memcpy(frame + 2, payload, FirstFramePayload);
        return true;
    }
    // Consecutive frame `index`: sequence 1, 2, … F, 0, 1 …; the first one starts after the FF's 6 bytes.
    size_t sent = FirstFramePayload + (index - 1) * ConsecutiveFramePayload;
    int sequence = FirstSequence;
    for (size_t i = 1; i < index; i++)
    {
        sequence = nextSequence(sequence);
    }
    size_t take = payloadLength - sent;
    if (take > static_cast<size_t>(ConsecutiveFramePayload))
    {
        take = ConsecutiveFramePayload;
    }
    frame[0] = consecutiveFramePci(sequence);
    memcpy(frame + 1, payload + sent, take);
    return true;
}
} // namespace IsoTpFraming

// ------------------------------------------------------------------ reassembly (ToyotaLens IsoTpReassembler)

bool Message_t::feed(const uint8_t *data, uint8_t length)
{
    using namespace IsoTpFraming;
    if (length == 0)
    {
        return false;
    }
    switch (pciType(data[0]))
    {
    case SingleFrame:
    {
        int len = singleFrameLength(data[0]);
        if (len == 0 || len > length - 1)
        {
            return false;
        }
        reset();
        memcpy(buffer, data + 1, len);
        expected = static_cast<uint16_t>(len);
        return true;
    }
    case FirstFrame:
    {
        if (length < 2)
        {
            return false;
        }
        int len = firstFrameLength(data[0], data[1]);
        if (len < FirstFrameMinLength || len > static_cast<int>(MAX_UDS_PAYLOAD_LEN))
        {
            // Too short to be a first frame, or longer than anything a Toyota module sends (§8.3): dropped.
            return false;
        }
        reset();
        expected = static_cast<uint16_t>(len);
        int take = length - 2;
        if (take > len)
        {
            take = len;
        }
        memcpy(buffer, data + 2, take);
        received = static_cast<uint16_t>(take);
        nextSequence = FirstSequence;
        inProgress = true;
        return false;
    }
    case ConsecutiveFrame:
    {
        if (!inProgress)
        {
            return false; // consecutive frame without a first frame: not ours or already abandoned
        }
        int seq = sequenceNumber(data[0]);
        if (seq != nextSequence)
        {
            abandoned++;
            reset();
            return false;
        }
        nextSequence = static_cast<uint8_t>(IsoTpFraming::nextSequence(nextSequence));
        int take = length - 1;
        if (take > expected - received)
        {
            take = expected - received;
        }
        memcpy(buffer + received, data + 1, take);
        received = static_cast<uint16_t>(received + take);
        if (received >= expected)
        {
            inProgress = false;
            return true;
        }
        return false;
    }
    default:
        // FlowControl, or anything else: not part of a message on this id
        return false;
    }
}

void Message_t::reset()
{
    expected = 0;
    received = 0;
    nextSequence = 0;
    inProgress = false;
}

// ------------------------------------------------------------------ flow control (ToyotaLens FlowControl)

FlowControl FlowControl::parse(const uint8_t *data)
{
    FlowControl fc;
    fc.status = static_cast<uint8_t>(data[0] & IsoTpFraming::PciArgumentMask);
    fc.blockSize = data[1];
    fc.separationTimeUs = IsoTpFraming::separationTimeMicroseconds(data[2]);
    return fc;
}

void FlowControl::continueAll(uint8_t frame[IsoTpFraming::FrameLength])
{
    memset(frame, IsoTpFraming::Padding, IsoTpFraming::FrameLength);
    frame[0] = IsoTpFraming::flowControlPci(IsoTpFraming::FlowStatusContinueToSend);
    frame[1] = IsoTpFraming::BlockSizeAll;
    frame[2] = IsoTpFraming::SeparationTimeNone;
}

// ------------------------------------------------------------------ the channel (ToyotaLens IsoTpChannel)

IsoTp::IsoTp(TwaiWrapper *can) : can_(can)
{
}

void IsoTp::target(uint32_t requestId)
{
    txId_ = requestId;
    rxId_ = requestId + Services::ResponseIdOffset;
}

size_t IsoTp::request(const uint8_t *payload, size_t payloadLength, uint32_t timeoutMs, uint8_t *reply,
                      size_t replyCapacity)
{
    if (txId_ == 0)
    {
        LOG_ERROR("target() before request()");
        return 0;
    }
    if (!can_->isOpen())
    {
        LOG_ERROR("ISF bus not open");
        return 0;
    }
    can_->drain();
    if (!sendMessage(payload, payloadLength))
    {
        return 0;
    }
    return receiveReply(timeoutMs, reply, replyCapacity);
}

// §5.1 send

bool IsoTp::sendMessage(const uint8_t *payload, size_t payloadLength)
{
    using namespace IsoTpFraming;
    size_t frames = segmentCount(payloadLength);
    if (frames == 0)
    {
        LOG_ERROR("ISO-TP message too long: %u > %d", static_cast<unsigned>(payloadLength), MaxMessageLength);
        return false;
    }
    uint8_t frame[FrameLength];
    segment(payload, payloadLength, 0, frame);
    if (!send(frame))
    {
        return false;
    }
    if (frames == 1)
    {
        return true;
    }
    size_t next = 1;
    int waits = 0;
    uint64_t lastConsecutive = 0; // STmin is measured from the previous consecutive frame, across blocks too
    while (next < frames)
    {
        ::FlowControl fc; // the struct, not IsoTpFraming::FlowControl (the PCI type)
        if (!waitForFlowControl(fc))
        {
            LOG_WARN("%03lX: no flow control within N_Bs %lu ms; request abandoned",
                     static_cast<unsigned long>(rxId_), static_cast<unsigned long>(bsTimeoutMs));
            return false;
        }
        if (fc.wait())
        {
            if (++waits > waitFramesMax)
            {
                LOG_WARN("%03lX: %d wait flow controls; request abandoned", static_cast<unsigned long>(rxId_),
                         waits);
                return false;
            }
            continue;
        }
        if (fc.abort())
        {
            LOG_WARN("%03lX: flow control status %X (overflow); request abandoned",
                     static_cast<unsigned long>(rxId_), fc.status);
            return false;
        }
        waits = 0;
        size_t remaining = frames - next;
        size_t block = fc.blockSize == BlockSizeAll ? remaining
                                                     : (static_cast<size_t>(fc.blockSize) < remaining
                                                            ? static_cast<size_t>(fc.blockSize)
                                                            : remaining);
        for (size_t i = 0; i < block; i++)
        {
            if (lastConsecutive != 0)
            {
                pauseSince(lastConsecutive, fc.separationTimeUs); // N_Cs: the gap the ECU asked for
            }
            segment(payload, payloadLength, next++, frame);
            if (!send(frame))
            {
                return false;
            }
            lastConsecutive = nowUs();
        }
    }
    return true;
}

bool IsoTp::waitForFlowControl(FlowControl &fc)
{
    uint32_t deadline = nowMs() + bsTimeoutMs;
    while (true)
    {
        int32_t remaining = static_cast<int32_t>(deadline - nowMs());
        if (remaining <= 0)
        {
            return false;
        }
        uint32_t id;
        uint8_t data[IsoTpFraming::FrameLength];
        uint8_t len;
        if (!can_->tryReceive(id, data, len, static_cast<uint32_t>(remaining)))
        {
            continue;
        }
        if (id != rxId_)
        {
            otherFrames++;
            continue;
        }
        if (len >= FlowControl::Length && IsoTpFraming::pciType(data[0]) == IsoTpFraming::FlowControl)
        {
            fc = FlowControl::parse(data);
            return true;
        }
        LOG_DEBUG("%03lX: data frame while waiting for flow control; ignored", static_cast<unsigned long>(rxId_));
    }
}

bool IsoTp::send(const uint8_t frame[IsoTpFraming::FrameLength])
{
    return can_->send(txId_, frame, IsoTpFraming::FrameLength);
}

/// Waits until `microseconds` have passed since `sinceUs`. Sleeps for all but the last two milliseconds, then
/// spins: STmin is a minimum, so overshooting is allowed, undershooting is not.
void IsoTp::pauseSince(uint64_t sinceUs, int microseconds)
{
    constexpr int64_t SpinTailUs = 2000;
    uint64_t until = sinceUs + static_cast<uint64_t>(microseconds);
    int64_t sleepUs = static_cast<int64_t>(until - nowUs()) - SpinTailUs;
    if (sleepUs > 0)
    {
        sleepMs(static_cast<uint32_t>(sleepUs / IsoTpFraming::MicrosecondsPerMillisecond));
    }
    while (nowUs() < until)
    {
    }
}

// §5.2 receive

size_t IsoTp::receiveReply(uint32_t timeoutMs, uint8_t *reply, size_t replyCapacity)
{
    reassembler_.reset();
    reassembler_.id = rxId_;
    uint32_t deadline = nowMs() + timeoutMs; // P2: the first response frame
    int pending = 0;
    while (true)
    {
        int32_t remaining = static_cast<int32_t>(deadline - nowMs());
        if (remaining <= 0)
        {
            if (reassembler_.inProgress)
            {
                abandoned++;
                LOG_WARN("%03lX: next consecutive frame did not arrive within N_Cr %lu ms; reply abandoned",
                         static_cast<unsigned long>(rxId_), static_cast<unsigned long>(crTimeoutMs));
            }
            return 0; // silence is not an error (§8.5)
        }
        uint32_t id;
        uint8_t data[IsoTpFraming::FrameLength];
        uint8_t len;
        if (!can_->tryReceive(id, data, len, static_cast<uint32_t>(remaining)))
        {
            continue;
        }
        if (id != rxId_)
        {
            otherFrames++; // software filter: other modules, our own echo
            continue;
        }
        if (len == 0 || IsoTpFraming::pciType(data[0]) == IsoTpFraming::FlowControl)
        {
            continue; // an FC is for the ECU's own sends
        }
        bool wasInProgress = reassembler_.inProgress;
        int abandonedBefore = reassembler_.abandoned;
        bool complete = reassembler_.feed(data, len);
        if (reassembler_.abandoned > abandonedBefore)
        {
            abandoned++;
            LOG_WARN("%03lX: consecutive frame out of sequence (%02X); reply abandoned",
                     static_cast<unsigned long>(rxId_), data[0]);
            return 0;
        }
        if (complete)
        {
            if (isResponsePending(reassembler_.buffer, reassembler_.expected))
            {
                if (++pending > maxResponsePending)
                {
                    LOG_WARN("%03lX: %d response-pending frames (7F %02X 78); giving up, treated as no reply",
                             static_cast<unsigned long>(rxId_), pending, reassembler_.buffer[1]);
                    return 0;
                }
                LOG_INFO("%03lX: response pending (7F %02X 78); waiting up to P2* %lu ms",
                         static_cast<unsigned long>(rxId_), reassembler_.buffer[1],
                         static_cast<unsigned long>(responsePendingMs));
                deadline = nowMs() + responsePendingMs;
                continue;
            }
            size_t n = reassembler_.expected;
            if (n > replyCapacity)
            {
                n = replyCapacity;
            }
            memcpy(reply, reassembler_.buffer, n);
            return n;
        }
        if (!wasInProgress && reassembler_.inProgress)
        {
            // The ECU's first frame: answer with flow control at once (N_Br = 0), then each CF within N_Cr.
            uint8_t fc[IsoTpFraming::FrameLength];
            FlowControl::continueAll(fc);
            can_->send(txId_, fc, IsoTpFraming::FrameLength);
        }
        if (reassembler_.inProgress)
        {
            deadline = nowMs() + crTimeoutMs;
        }
    }
}

bool IsoTp::isResponsePending(const uint8_t *payload, size_t length)
{
    return length >= static_cast<size_t>(Services::NegativeResponseLength) && payload[0] == Services::NegativeResponse &&
           payload[2] == Nrc::ResponsePending;
}

#include "message_translator.h"

#include <cmath>
#include <cstring>

#include "logger/logger.h"

MessageTranslator::MessageTranslator()
{
    frameCount_ = 0;
    for (size_t i = 0; i < OUTPUT_FRAMES_COUNT && frameCount_ < MaxFrames; i++)
    {
        CANMessage &m = frames_[frameCount_++];
        m.id = OUTPUT_FRAMES[i].id;
        memcpy(m.data, OUTPUT_FRAMES[i].defaults, 8);
        m.len = 8;
        m.extended = OUTPUT_FRAMES[i].extended;
        m.interval = OUTPUT_FRAMES[i].intervalMs;
        m.param_name = OUTPUT_FRAMES[i].name;
    }
}

int MessageTranslator::frameIndex(uint32_t id) const
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

void MessageTranslator::packSignal(uint8_t data[8], int startBit, int bitLength, uint64_t raw)
{
    for (int i = 0; i < bitLength; i++)
    {
        int bit = startBit + i;
        if (bit >= 64)
        {
            break;
        }
        uint8_t mask = static_cast<uint8_t>(1u << (bit % 8));
        if ((raw >> i) & 1u)
        {
            data[bit / 8] |= mask;
        }
        else
        {
            data[bit / 8] &= static_cast<uint8_t>(~mask);
        }
    }
}

uint64_t MessageTranslator::unpackSignal(const uint8_t data[8], int startBit, int bitLength)
{
    uint64_t raw = 0;
    for (int i = 0; i < bitLength; i++)
    {
        int bit = startBit + i;
        if (bit >= 64)
        {
            break;
        }
        if ((data[bit / 8] >> (bit % 8)) & 1u)
        {
            raw |= 1ULL << i;
        }
    }
    return raw;
}

uint64_t MessageTranslator::encode(const TranslationSignal &s, const ParameterValue &v)
{
    uint64_t limit = s.bitLength >= 64 ? ~0ULL : ((1ULL << s.bitLength) - 1);
    if (s.passRaw)
    {
        // The field is the ECU's own value, already sized by the generator; a signed value keeps its two's
        // complement bits within the field.
        return static_cast<uint64_t>(v.raw) & limit;
    }
    double raw = std::round((v.value - s.offset) / s.factor);
    if (s.isSigned)
    {
        double min = -static_cast<double>(1ULL << (s.bitLength - 1));
        double max = static_cast<double>((1ULL << (s.bitLength - 1)) - 1);
        if (raw < min)
        {
            raw = min;
        }
        if (raw > max)
        {
            raw = max;
        }
        return static_cast<uint64_t>(static_cast<int64_t>(raw)) & limit;
    }
    if (raw < 0)
    {
        raw = 0;
    }
    if (raw > static_cast<double>(limit))
    {
        raw = static_cast<double>(limit);
    }
    return static_cast<uint64_t>(raw);
}

int MessageTranslator::apply(const ParameterValue *values, int count, CANMessage changed[], int maxChanged)
{
    bool touched[MaxFrames] = {false};
    for (int i = 0; i < count; i++)
    {
        const ParameterValue &v = values[i];
        if (!v.isAvailable || v.parameter == nullptr)
        {
            continue;
        }
        for (size_t j = 0; j < TRANSLATION_SIGNALS_COUNT; j++)
        {
            const TranslationSignal &s = TRANSLATION_SIGNALS[j];
            if (strcmp(s.uniqueId, v.parameter->uniqueId) != 0)
            {
                continue;
            }
            int f = frameIndex(s.frameId);
            if (f < 0)
            {
                continue;
            }
            // Every frame a decoded block writes into is reported, changed bytes or not, so the consumer side is
            // refreshed on every poll and its hold-time fallback only fires when the ECU really stops answering.
            packSignal(frames_[f].data, s.startBit, s.bitLength, encode(s, v));
            touched[f] = true;
        }
    }
    int n = 0;
    for (int f = 0; f < frameCount_ && n < maxChanged; f++)
    {
        if (touched[f])
        {
            changed[n++] = frames_[f];
        }
    }
    return n;
}

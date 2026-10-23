#include "isf_broadcast.h"

namespace
{
constexpr int BitsPerByte = 8;

/// In Motorola numbering the bit after position p (towards the least significant end of the field) is p - 1
/// within a byte, and bit 7 of the next byte after bit 0.
inline int nextMotorola(int position)
{
    return (position % BitsPerByte == 0) ? position + 2 * BitsPerByte - 1 : position - 1;
}
} // namespace

bool BroadcastDecoder::fits(uint8_t len, uint8_t startBit, uint8_t bitLength, bool bigEndian)
{
    int limit = len * BitsPerByte;
    if (bitLength == 0 || bitLength > 64 || startBit >= limit)
    {
        return false;
    }
    if (!bigEndian)
    {
        return startBit + bitLength <= limit;
    }
    int position = startBit;
    for (int i = 1; i < bitLength; i++)
    {
        position = nextMotorola(position);
    }
    return position < limit;
}

uint64_t BroadcastDecoder::extract(const uint8_t *data, uint8_t startBit, uint8_t bitLength, bool bigEndian)
{
    uint64_t value = 0;
    if (!bigEndian)
    {
        for (int i = 0; i < bitLength; i++)
        {
            int bit = startBit + i;
            if ((data[bit / BitsPerByte] >> (bit % BitsPerByte)) & 1u)
            {
                value |= 1ULL << i;
            }
        }
        return value;
    }
    int position = startBit;
    for (int i = 0; i < bitLength; i++)
    {
        value = (value << 1) | ((data[position / BitsPerByte] >> (position % BitsPerByte)) & 1u);
        position = nextMotorola(position);
    }
    return value;
}

ParameterValue BroadcastDecoder::decode(const BroadcastSignal &s, const uint8_t *data, uint8_t len)
{
    ParameterValue v;
    v.parameter = &s.identity;
    UnitMap::Mapping um = UnitMap::map(s.identity.unitType);
    v.unit = um.unit;
    if (!fits(len, s.startBit, s.bitLength, s.bigEndian))
    {
        v.error = "frame too short";
        return v;
    }
    uint64_t masked = extract(data, s.startBit, s.bitLength, s.bigEndian);
    int64_t raw = static_cast<int64_t>(masked);
    if (s.identity.isSigned && s.bitLength < 64 && (masked & (1ULL << (s.bitLength - 1))) != 0)
    {
        raw = static_cast<int64_t>(masked | (~0ULL << s.bitLength));
    }
    v.isAvailable = true;
    v.raw = raw;
    v.value = (raw * s.identity.scaleFactor + s.identity.offset) * um.scale + um.offset;
    return v;
}

int BroadcastDecoder::decodeFrame(uint32_t id, const uint8_t *data, uint8_t len, ParameterValue *values, int maxValues)
{
    int n = 0;
    for (size_t i = 0; i < BROADCAST_SIGNALS_COUNT && n < maxValues; i++)
    {
        const BroadcastSignal &s = BROADCAST_SIGNALS[i];
        if (s.isfId == id)
        {
            values[n++] = decode(s, data, len);
        }
    }
    return n;
}

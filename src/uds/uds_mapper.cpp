#include "uds/uds_mapper.h"

#include <cstring>

// ------------------------------------------------------------------ Nrc

const char *Nrc::name(uint8_t nrc)
{
    switch (nrc)
    {
    case GeneralReject:
        return "general reject";
    case ServiceNotSupported:
        return "service not supported";
    case SubFunctionNotSupported:
        return "sub-function not supported";
    case IncorrectMessageLength:
        return "incorrect message length";
    case BusyRepeatRequest:
        return "busy, repeat request";
    case ConditionsNotCorrect:
        return "conditions not correct";
    case RequestSequenceError:
        return "request sequence error";
    case RequestOutOfRange:
        return "request out of range";
    case SecurityAccessDenied:
        return "security access denied";
    case ResponsePending:
        return "response pending";
    default:
        return "unknown code";
    }
}

// ------------------------------------------------------------------ SupportBitmap

int SupportBitmap::parse(int baseId, const uint8_t *bitmap, size_t bitmapLength, uint8_t ids[MaxIds], bool &next)
{
    constexpr int BitsPerByte = 8;
    constexpr int HighBit = 7;
    int count = 0;
    next = false;
    for (size_t i = 0; i < bitmapLength && i < static_cast<size_t>(Length); i++)
    {
        for (int bit = HighBit; bit >= 0; bit--)
        {
            if ((bitmap[i] & (1 << bit)) == 0)
            {
                continue;
            }
            int id = baseId + static_cast<int>(i) * BitsPerByte + (HighBit - bit) + 1;
            if (id == baseId + Stride)
            {
                next = true;
            }
            else
            {
                ids[count++] = static_cast<uint8_t>(id);
            }
        }
    }
    return count;
}

// ------------------------------------------------------------------ ResponseDataMap

namespace
{
/// Reply layout: E8 11, then the parameter address, then the flag count, then the flags.
constexpr int ParameterAddressOffset = 2;
constexpr int CountOffset = 3;
} // namespace

int ResponseDataMap::presentCount() const
{
    int n = 0;
    for (int i = 0; i < fullLength; i++)
    {
        if (present[i])
        {
            n++;
        }
    }
    return n;
}

bool ResponseDataMap::fromReply(const uint8_t *payload, size_t length, ResponseDataMap &map)
{
    if (length < static_cast<size_t>(Services::DataMapHeaderLength) || payload[0] != Services::ReadDataMapReply ||
        payload[1] != Services::ReadDataMapSubFunction)
    {
        return false;
    }
    int count = payload[CountOffset];
    if (count == 0 || count > MaxLayoutLength || length < static_cast<size_t>(Services::DataMapHeaderLength + count))
    {
        return false;
    }
    map.parameterAddress = payload[ParameterAddressOffset];
    map.fullLength = static_cast<uint8_t>(count);
    for (int i = 0; i < MaxLayoutLength; i++)
    {
        map.present[i] = i < count && payload[Services::DataMapHeaderLength + i] != 0;
    }
    return true;
}

bool ResponseDataMap::covers(int position, int length) const
{
    if (length <= 0)
    {
        length = 1;
    }
    for (int i = 0; i < length; i++)
    {
        int p = position + i;
        if (p < 0 || p >= fullLength || !present[p])
        {
            return false;
        }
    }
    return true;
}

int ResponseDataMap::expand(const uint8_t *compressed, int compressedLength, uint8_t *full) const
{
    if (compressedLength >= fullLength)
    {
        int n = compressedLength < MaxLayoutLength ? compressedLength : MaxLayoutLength;
        memcpy(full, compressed, n);
        return n;
    }
    if (presentCount() != compressedLength)
    {
        return -1;
    }
    int n = 0;
    for (int i = 0; i < fullLength; i++)
    {
        full[i] = present[i] ? compressed[n++] : 0;
    }
    return fullLength;
}

// ------------------------------------------------------------------ UnitMap (§6.7)

UnitMap::Mapping UnitMap::map(int stored)
{
    constexpr double Pi = 3.14159265358979323846;
    switch (stored)
    {
    case 0:
        return {"", 1, 0};
    case 1:
        return {"m/s2", 1, 0};
    case 2:
        return {"m/s2", 0.001, 0};
    case 3:
        return {"m/s2", 5.0 / 18.0, 0};
    case 4:
        return {"m/s2", 9.81, 0};
    case 5:
        return {"deg", 1, 0};
    case 6:
        return {"deg", 180.0 / Pi, 0};
    case 7:
        return {"deg/s", 1, 0};
    case 8:
        return {"deg/s", 180.0 / Pi, 0};
    case 9:
        return {"A", 1, 0};
    case 10:
        return {"m3/h", 1, 0};
    case 11:
        return {"uA", 1, 0};
    case 12:
        return {"mA", 1, 0};
    case 13:
        return {"km", 1, 0};
    case 14:
        return {"cm", 1, 0};
    case 15:
        return {"mm", 1, 0};
    case 16:
        return {"um", 1, 0};
    case 17:
        return {"km", 1, 0};
    case 18:
        return {"Ah", 1, 0};
    case 19:
        return {"W", 1, 0};
    case 20:
        return {"W", 1000, 0};
    case 21:
        return {"kJ", 1, 0};
    case 22:
        return {"kWh", 1, 0};
    case 23:
        return {"l", 1, 0};
    case 24:
        return {"ml", 1, 0};
    case 25:
        return {"N", 1000, 0};
    case 26:
        return {"N", 1, 0};
    case 27:
        return {"m3/s", 1, 0};
    case 28:
        return {"Hz", 1, 0};
    case 29:
        return {"kHz", 1, 0};
    case 30:
        return {"lx", 1, 0};
    case 31:
        return {"us", 1, 0};
    case 32:
        return {"ppm", 1, 0};
    case 33:
        return {"%", 1, 0};
    case 34:
        return {"kPa", 1, 0};
    case 35:
        return {"kPa", 0.001, 0};
    case 36:
        return {"MPa", 1, 0};
    case 37:
        return {"MPa/s", 1, 0};
    case 38:
        return {"", 1, 0};
    case 39:
        return {"rpm", 1, 0};
    case 40:
        return {"ohm", 1, 0};
    case 41:
        return {"mohm", 1, 0};
    case 42:
        return {"km/h", 1, 0};
    case 43:
        return {"cm/s", 1, 0};
    case 44:
        return {"km/h", 3.6, 0};
    case 45:
        return {"g/s", 1.0 / 3600.0, 0};
    case 46:
        return {"g/s", 1, 0};
    case 47:
        return {"kg", 1, 0};
    case 48:
        return {"V", 1, 0};
    case 49:
        return {"mV", 1, 0};
    case 50:
        return {"ml", 1, 0};
    case 51:
        return {"kW", 1, 0};
    case 52:
        return {"s", 1, 0};
    case 53:
        return {"min", 1, 0};
    case 54:
        return {"h", 1, 0};
    case 55:
        return {"d", 1, 0};
    case 56:
        return {"ms", 1, 0};
    case 57:
        return {"degC", 1, 0};
    case 58:
        return {"Nm", 1, 0};
    case 59:
        return {"", 1, 0};
    case 60:
        return {"deg", 1, 0};
    case 61:
        return {"g/rev", 1, 0};
    case 62:
        return {"l/stroke", 1, 0};
    case 63:
        return {"kPa", 100, 0};
    case 64:
        return {"l/hr", 3.6, 0};
    case 65:
        return {"W/m2", 1, 0};
    case 66:
        return {"degC", 5.0 / 9.0, -17.77777777777778};
    case 67:
        return {"As", 1, 0};
    case 68:
        return {"kg", 0.001, 0};
    case 69:
        return {"g/s", 5.0 / 18.0, 0};
    case 70:
        return {"J", 1, 0};
    case 71:
        return {"l/hr", 1, 0};
    case 72:
        return {"kJ", 0.001, 0};
    case 73:
        return {"m/s2", 0.00981, 0};
    case 74:
        return {"kohm", 1, 0};
    case 75:
        return {"m3/s", 1.0 / 60.0, 0};
    default:
        return {"", 1, 0};
    }
}

// ------------------------------------------------------------------ ParameterDecoder (§6.8)

uint64_t ParameterDecoder::readMasked(const uint8_t *data, int offset, const uint8_t *mask, int maskLength)
{
    constexpr int BitsPerByte = 8;
    uint64_t result = 0;
    for (int i = 0; i < maskLength; i++)
    {
        result |= static_cast<uint64_t>(data[offset + i] & mask[i]) << ((maskLength - i - 1) * BitsPerByte);
    }
    return result;
}

int64_t ParameterDecoder::signExtend(uint64_t value, int byteCount)
{
    constexpr int BitsPerByte = 8;
    int bits = byteCount * BitsPerByte;
    if (bits >= static_cast<int>(sizeof(int64_t)) * BitsPerByte)
    {
        return static_cast<int64_t>(value);
    }
    uint64_t signBit = 1ULL << (bits - 1);
    return (value & signBit) != 0 ? static_cast<int64_t>(value | (~0ULL << bits)) : static_cast<int64_t>(value);
}

ParameterValue ParameterDecoder::decode(const UdsDefinition &p, const uint8_t *block, int blockLength,
                                        const ResponseDataMap *map)
{
    constexpr int MaxShift = 63; // a raw value is at most 64 bits wide
    ParameterValue v;
    v.parameter = &p;
    UnitMap::Mapping um = UnitMap::map(p.unitType);
    v.unit = um.unit;
    if (map != nullptr && !map->covers(p.bytePosition, p.maskLength))
    {
        v.isSupported = false;
        v.error = "not supported by this ECU (data map)";
        return v;
    }
    if (p.maskLength == 0)
    {
        v.error = "no mask";
        return v;
    }
    if (p.fieldEnd() > blockLength)
    {
        v.error = "block too short";
        return v;
    }
    if (p.dataType == DataType::Ascii)
    {
        // The gateway forwards no text; the bytes are available raw.
        v.isAvailable = true;
        v.raw = static_cast<int64_t>(readMasked(block, p.bytePosition, p.mask, p.maskLength));
        v.unit = "";
        return v;
    }

    uint64_t masked = readMasked(block, p.bytePosition, p.mask, p.maskLength);
    int64_t raw = p.isSigned ? signExtend(masked, p.maskLength) : static_cast<int64_t>(masked);
    int shift = p.bitOffset;
    if (shift < 0)
    {
        shift = 0;
    }
    if (shift > MaxShift)
    {
        shift = MaxShift;
    }
    raw >>= shift;

    double scaled = raw * p.scaleFactor + p.offset;
    v.isAvailable = true;
    v.raw = raw;
    v.value = scaled * um.scale + um.offset;
    return v;
}

// ------------------------------------------------------------------ the definitions table

int definitionsOf(uint32_t requestId, uint8_t parameterAddress, const UdsDefinition *out[], int maxOut)
{
    int n = 0;
    for (size_t i = 0; i < UDS_DEFINITIONS_COUNT && n < maxOut; i++)
    {
        const UdsDefinition &d = UDS_DEFINITIONS[i];
        if (d.requestId == requestId && d.parameterAddress == parameterAddress)
        {
            out[n++] = &d;
        }
    }
    return n;
}

#pragma once

#include <cstddef>
#include <cstdint>

// ---------------------------------------------------------------------------------------------------------
// The Toyota Mode 21 protocol and its decode rules, ported from ToyotaLens: Protocol/Services.cs, Nrc.cs,
// SupportBitmap.cs, ResponseDataMap.cs, ParameterDecoder.cs, Definitions/UnitMap.cs and
// ParameterDefinition.cs (specification §3, §6.6, §6.7, §6.8). The parameter definitions themselves are
// generated from ToyotaLens's data/{engine,ect,abs}.json into uds_definitions.h by tools/generate.py and are
// never edited by hand.
// ---------------------------------------------------------------------------------------------------------

/// The diagnostic services the gateway speaks and the bytes they are made of (ToyotaLens `Services`). Every
/// request is a service id followed by its arguments; a positive reply echoes the service id plus
/// PositiveResponseOffset (21 -> 61, A8 -> E8); a refusal is 7F <service> <nrc> (see `Nrc`).
namespace Services
{
/// Reply service id = request service id + 0x40 (ISO 14229 / ISO 15031).
constexpr uint8_t PositiveResponseOffset = 0x40;
/// First byte of a refusal: 7F <service> <nrc>.
constexpr uint8_t NegativeResponse = 0x7F;
constexpr int NegativeResponseLength = 3;
/// §1.1: a module answers on its request id + 8 (7E0 -> 7E8, 7B0 -> 7B8).
constexpr uint32_t ResponseIdOffset = 8;

inline uint8_t replyTo(uint8_t service)
{
    return static_cast<uint8_t>(service + PositiveResponseOffset);
}

// --- §3.2 Toyota Mode 21 (Protocol 3 and 4)

/// ReadDataByLocalIdentifier: 21 <parameter address>, reply 61 <parameter address> <data…>. The "local
/// identifier" is what toyota.all calls Parameter.Address: the block of parameters to read.
constexpr uint8_t ReadDataByLocalId = 0x21;
constexpr uint8_t ReadDataByLocalIdReply = ReadDataByLocalId + PositiveResponseOffset; // 0x61
/// A reply is 61 <parameter address> and then the block; BytePosition counts from after these two bytes.
constexpr int ReadDataHeaderLength = 2;
/// The first support bitmap, and the presence probe (§3.7): any answer to 21 00 means the module is there.
constexpr uint8_t ProbeParameterAddress = 0x00;

// --- §3.3 Toyota response data map (Protocol 4)

/// Read response data map: A8 11 <parameter address>, reply E8 11 <parameter address> <count> <flag…>.
constexpr uint8_t ReadDataMap = 0xA8;
constexpr uint8_t ReadDataMapSubFunction = 0x11;
constexpr uint8_t ReadDataMapReply = ReadDataMap + PositiveResponseOffset; // 0xE8
/// A map reply is E8 11 <parameter address> <count> and then one flag per byte of the full layout.
constexpr int DataMapHeaderLength = 4;
} // namespace Services

/// Negative response codes (ISO 14229-1 table A.1), the third byte of 7F <service> <nrc> (ToyotaLens `Nrc`).
/// Specification §3.8 lists the ones seen on Toyota modules: 0x11 for a service the generation does not have,
/// 0x12 for a parameter address a module does not implement, 0x78 from modules that need more time.
namespace Nrc
{
constexpr uint8_t GeneralReject = 0x10;
constexpr uint8_t ServiceNotSupported = 0x11;
constexpr uint8_t SubFunctionNotSupported = 0x12;
constexpr uint8_t IncorrectMessageLength = 0x13;
constexpr uint8_t BusyRepeatRequest = 0x21;
constexpr uint8_t ConditionsNotCorrect = 0x22;
constexpr uint8_t RequestSequenceError = 0x24;
constexpr uint8_t RequestOutOfRange = 0x31;
constexpr uint8_t SecurityAccessDenied = 0x33;
constexpr uint8_t ResponsePending = 0x78;

const char *name(uint8_t nrc);
} // namespace Nrc

/// Specification §3.2: support bitmaps (ToyotaLens `SupportBitmap`). Local ids 00, 20, 40, … E0 are not data;
/// each answers 61 <base> b0 b1 b2 b3, a 4-byte bitmap of the 32 ids that follow it:
///   bit 7 of byte 0 -> id base+1      bit 0 of byte 0 -> id base+8
///   bit 7 of byte 1 -> id base+9      …
///   bit 0 of byte 3 -> id base+0x20 : "the next bitmap exists", not a data id
/// The walk stops at the first bitmap that is refused or whose last bit is clear.
namespace SupportBitmap
{
/// Ids per bitmap: the distance between bitmap ids (00, 20, 40, …).
constexpr int Stride = 0x20;
/// The last bitmap id; its final bit has no "next" to point to.
constexpr int LastBaseId = 0xE0;
/// Four bytes, one bit per id.
constexpr int Length = 4;
/// The most ids one bitmap can name (the 32nd bit is "next").
constexpr int MaxIds = Stride - 1;

/// A bitmap id is a multiple of the stride: its low five bits are clear.
inline bool isBitmapId(int parameterAddress)
{
    return (parameterAddress & (Stride - 1)) == 0;
}

/// The supported ids encoded in one bitmap reply (written to `ids`, count returned) and whether the next
/// bitmap exists (`next`).
int parse(int baseId, const uint8_t *bitmap, size_t bitmapLength, uint8_t ids[MaxIds], bool &next);
} // namespace SupportBitmap

/// Toyota response data map for one Mode 21 parameter address (ToyotaLens `ResponseDataMap`, §3.3). Request
/// A8 11 id; reply E8 11 id count flag[0..count-1]. A non-zero flag means the ECU sends that byte of the full
/// layout; the 21 id reply contains only the present bytes, in layout order, so it must be expanded before
/// BytePosition can index it.
struct ResponseDataMap
{
    /// Mode 21 blocks reach 52 bytes in the definitions and 56 in the maps seen on the IS F.
    static constexpr int MaxLayoutLength = 64;

    uint8_t parameterAddress = 0;
    uint8_t fullLength = 0;
    bool present[MaxLayoutLength] = {false};

    int presentCount() const;

    /// Parses the payload of a positive A8 11 reply (E8 11 id count flags…). False when malformed.
    static bool fromReply(const uint8_t *payload, size_t length, ResponseDataMap &map);

    bool covers(int position, int length) const;

    /// Lays a compressed block out to the full layout into `full` (at least MaxLayoutLength bytes).
    /// Returns the full length; returns the input length unchanged (copied) if it is already full length;
    /// returns -1 (caller keeps the raw block) when the present count disagrees with the block length, which
    /// indicates a stale map.
    int expand(const uint8_t *compressed, int compressedLength, uint8_t *full) const;
};

/// Stored Parameter.UnitType (0..75) -> display unit plus base-unit scale/offset (ToyotaLens `UnitMap`,
/// specification §6.7). Applied after ScaleFactor/Offset: value = (raw * ScaleFactor + Offset) * scale + offset.
namespace UnitMap
{
struct Mapping
{
    const char *unit;
    double scale;
    double offset;
};

Mapping map(int stored);
} // namespace UnitMap

/// How a parameter's bytes are read (ToyotaLens ParameterDefinition.DataType).
enum class DataType : uint8_t
{
    Numeric,
    Flag,
    Hex,
    Ascii,
    Special
};

/// One parameter definition, one entry of ToyotaLens data/{engine,ect,abs}.json (ToyotaLens
/// `ParameterDefinition`, specification §6.6). Generated into uds_definitions.h.
struct UdsDefinition
{
    const char *uniqueId;
    const char *name;
    /// The module's request id: 7E0 engine, 7E1 ECT, 7B0 ABS.
    uint32_t requestId;
    /// Mode 21 parameter address (the block this field lives in).
    uint8_t parameterAddress;
    /// Offset into the block after the echoed 61 xx, in the full (map-expanded) layout.
    uint8_t bytePosition;
    uint8_t bitOffset;
    /// One mask byte per data byte; the field is maskLength bytes wide.
    uint8_t mask[4];
    uint8_t maskLength;
    bool isSigned;
    double scaleFactor;
    double offset;
    /// Stored unit code 0..75 (not the display unit; see UnitMap).
    uint8_t unitType;
    DataType dataType;

    int fieldEnd() const
    {
        return bytePosition + maskLength;
    }
};

/// Result of decoding one parameter out of a block (ToyotaLens `ParameterValue`).
struct ParameterValue
{
    const UdsDefinition *parameter = nullptr;
    bool isAvailable = false;
    bool isSupported = true;
    int64_t raw = 0;
    double value = 0.0;
    const char *unit = "";
    const char *error = "";
};

/// Specification §6.8: masked big-endian read, sign extension, shift, scale, unit map (ToyotaLens
/// `ParameterDecoder`).
namespace ParameterDecoder
{
uint64_t readMasked(const uint8_t *data, int offset, const uint8_t *mask, int maskLength);
int64_t signExtend(uint64_t value, int byteCount);

/// Decodes one parameter from a full-layout block (bytes after the echoed 61 xx, map-expanded). With a map,
/// fields with absent bytes are reported as not supported.
ParameterValue decode(const UdsDefinition &p, const uint8_t *block, int blockLength, const ResponseDataMap *map);
} // namespace ParameterDecoder

/// The generated parameter table (uds_definitions.h).
extern const UdsDefinition UDS_DEFINITIONS[];
extern const size_t UDS_DEFINITIONS_COUNT;

/// Every definition of one module's block, in table order; the count written.
int definitionsOf(uint32_t requestId, uint8_t parameterAddress, const UdsDefinition *out[], int maxOut);

#pragma once

#include <cstddef>
#include <cstdint>

#include "uds/uds_mapper.h"

// ---------------------------------------------------------------------------------------------------------
// Frames the IS F modules broadcast on their own, with no request: ABS wheel speeds 0x0B0 / 0x0B2 and vehicle
// speed 0x0B4 at about 63 Hz, engine rpm 0x2C4 at 30 Hz, yaw / steering 0x024 / 0x025 at 65 Hz. The GT86
// packets that must not lag (speed, wheel speeds, rpm) take their values from these, decoded the moment the
// frame arrives, instead of from the polled Mode 21 blocks. The signals are generated from the "broadcast"
// section of tools/gateway_frames.json into translator_table.cpp.
//
// Bit positions use the DBC convention so an entry can be copied from a Toyota DBC line: a big-endian signal
// (Motorola, "@0") names the position of its most significant bit, a little-endian one (Intel, "@1") the
// position of its least significant bit; bit 7 of byte 0 is position 7, bit 0 of byte 1 is position 8.
// ---------------------------------------------------------------------------------------------------------

struct BroadcastSignal
{
    /// The 11-bit id the IS F module sends the frame on.
    uint32_t isfId;
    /// uniqueId, name, sign, scaling and unit of the signal. A decoded value becomes a ParameterValue pointing at
    /// this, so the translator matches it by uniqueId exactly like a Mode 21 value. requestId holds the broadcast
    /// id; the block fields (parameterAddress, bytePosition, mask) are unused.
    UdsDefinition identity;
    uint8_t startBit;
    uint8_t bitLength;
    bool bigEndian;
};

extern const BroadcastSignal BROADCAST_SIGNALS[];
extern const size_t BROADCAST_SIGNALS_COUNT;

namespace BroadcastDecoder
{
/// True when the field lies inside a frame of `len` bytes.
bool fits(uint8_t len, uint8_t startBit, uint8_t bitLength, bool bigEndian);
/// The raw field, DBC bit numbering (see above). The caller checks `fits` first.
uint64_t extract(const uint8_t *data, uint8_t startBit, uint8_t bitLength, bool bigEndian);
/// One signal out of one frame; isAvailable is false when the frame is too short for the field.
ParameterValue decode(const BroadcastSignal &s, const uint8_t *data, uint8_t len);
/// Every signal of the frame with id `id`, written to `values` (at most `maxValues`); the count returned, 0 when
/// the id is not a broadcast source.
int decodeFrame(uint32_t id, const uint8_t *data, uint8_t len, ParameterValue *values, int maxValues);
} // namespace BroadcastDecoder

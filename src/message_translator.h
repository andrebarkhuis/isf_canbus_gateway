#pragma once

#include <cstddef>
#include <cstdint>

#include "common.h"
#include "uds/uds_mapper.h"

// ---------------------------------------------------------------------------------------------------------
// Decoded ISF parameters -> frames on the GT86 / ADU bus. The output frames and the signals in them are one
// table, generated from tools/gateway_frames.json into translator_table.h together with gateway.dbc, so the
// code and the DBC cannot disagree. Every signal is little-endian (Intel, DBC "@1"): start bit = the bit
// position of its least significant bit, bit 0 = byte 0 bit 0.
//
// Two kinds of signal:
//   - pass-through (the ADU frames): the ECU's raw value goes into the frame unchanged and the DBC carries
//     ToyotaLens's scale factor and offset, so nothing is lost in conversion;
//   - converted (the GT86 packets): the physical value is re-encoded with the frame's own factor and offset,
//     the way the GT86 modules expect it.
// ---------------------------------------------------------------------------------------------------------

/// One output frame: its id, the bytes it starts with (the GT86 "cluster working, no errors" capture for the
/// GT86 packets; zeros for the ADU frames), how often the GT86 side repeats it, whether the id is 29-bit, and
/// whether the ISF task also sends it on the ISF bus (the ADU frames: a 29-bit id never meets a module's id).
struct OutputFrame
{
    uint32_t id;
    const char *name;
    uint8_t defaults[8];
    uint32_t intervalMs;
    bool extended;
    bool alsoOnIsfBus;
};

/// One signal: where it comes from and where it goes.
struct TranslationSignal
{
    const char *uniqueId;   // source parameter (UDS_DEFINITIONS)
    uint32_t frameId;       // target frame
    const char *name;       // DBC signal name
    uint8_t startBit;       // LSB position, Intel
    uint8_t bitLength;
    bool isSigned;
    bool passRaw;           // true: raw value unchanged; false: raw = (physical - offset) / factor
    double factor;          // DBC factor / offset (pass-through: ToyotaLens scale and offset after the unit map)
    double offset;
};

extern const OutputFrame OUTPUT_FRAMES[];
extern const size_t OUTPUT_FRAMES_COUNT;
extern const TranslationSignal TRANSLATION_SIGNALS[];
extern const size_t TRANSLATION_SIGNALS_COUNT;

class MessageTranslator
{
public:
    static constexpr int MaxFrames = 64;

    MessageTranslator();

    /// Applies decoded values to the frames they map to. Every frame a value was written into is copied to
    /// `changed` (whether or not its bytes differ), so the consumer side is refreshed on every poll; returns how
    /// many.
    int apply(const ParameterValue *values, int count, CANMessage changed[], int maxChanged);

    /// The current bytes of every output frame (defaults until a translation touched them).
    const CANMessage *frames() const
    {
        return frames_;
    }
    int frameCount() const
    {
        return frameCount_;
    }

    /// Writes `raw` into `data` at the signal's position, little-endian; bits outside the field are kept.
    static void packSignal(uint8_t data[8], int startBit, int bitLength, uint64_t raw);
    /// Reads a signal back (for the self-test).
    static uint64_t unpackSignal(const uint8_t data[8], int startBit, int bitLength);

private:
    int frameIndex(uint32_t id) const;
    static uint64_t encode(const TranslationSignal &s, const ParameterValue &v);

    CANMessage frames_[MaxFrames];
    int frameCount_ = 0;
};

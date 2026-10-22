#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

// ---------------------------------------------------------------------------------------------------------
// Types shared by every component. Nothing here depends on ESP-IDF, so the self-test builds the same code on a
// host compiler (see test/host). The platform functions at the bottom are implemented in common.cpp (ESP-IDF)
// and in test/host/host_platform.cpp.
// ---------------------------------------------------------------------------------------------------------

/// One classic CAN frame: 11-bit identifier and up to 8 data bytes (ToyotaLens CanFrame). Plain data, so it can
/// travel through a FreeRTOS queue by value. `interval` and `param_name` serve the GT86 side's periodic tables.
struct CANMessage
{
    uint32_t id = 0;
    uint8_t data[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    uint8_t len = 8;
    bool extended = false;
    uint32_t interval = 0;         // periodic send interval in ms; 0 = sent only when translated
    const char *param_name = "";   // label for the log
};

/// Milliseconds since boot (monotonic).
uint32_t nowMs();
/// Microseconds since boot (monotonic).
uint64_t nowUs();
/// Blocks the calling task for at least `ms` milliseconds.
void sleepMs(uint32_t ms);

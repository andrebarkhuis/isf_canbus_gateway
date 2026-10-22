#pragma once

#include <cstdint>

// ---------------------------------------------------------------------------------------------------------
// Logging over the console (ESP-IDF esp_log on the device, stdout in the host self-test). Two kinds of line:
//   - text, through LOG_ERROR / LOG_WARN / LOG_INFO / LOG_DEBUG, filtered by Logger::setLevel();
//   - CAN frames in candump format, "(seconds.micros) <iface> 7E0#0221010000000000", through LOG_FRAME.
// The frame lines are the gateway's debug link (ToyotaLens requirement R8.1): every frame sent or received on
// the ISF bus is written exactly as it was on the bus, flow controls included, so a captured serial log can be
// replayed with `ToyotaLens decode <log>` and compared against the reference captures.
// ---------------------------------------------------------------------------------------------------------

enum LogLevel
{
    LOG_LEVEL_NONE = 0,
    LOG_LEVEL_ERROR = 1,
    LOG_LEVEL_WARN = 2,
    LOG_LEVEL_INFO = 3,
    LOG_LEVEL_DEBUG = 4
};

class Logger
{
public:
    static void begin(LogLevel level, bool traceFrames);
    static void setLevel(LogLevel level);
    static LogLevel level();
    /// Whether LOG_FRAME lines are written.
    static void setTraceFrames(bool on);

    static void log(LogLevel level, const char *function, const char *format, ...);
    static void frame(const char *iface, uint32_t id, const uint8_t *data, uint8_t len);

private:
    static LogLevel level_;
    static bool traceFrames_;
    static constexpr int BUFFER_SIZE = 256;
};

#define LOG_ERROR(format, ...) Logger::log(LOG_LEVEL_ERROR, __FUNCTION__, format, ##__VA_ARGS__)
#define LOG_WARN(format, ...) Logger::log(LOG_LEVEL_WARN, __FUNCTION__, format, ##__VA_ARGS__)
#define LOG_INFO(format, ...) Logger::log(LOG_LEVEL_INFO, __FUNCTION__, format, ##__VA_ARGS__)
#define LOG_DEBUG(format, ...) Logger::log(LOG_LEVEL_DEBUG, __FUNCTION__, format, ##__VA_ARGS__)
#define LOG_FRAME(iface, id, data, len) Logger::frame(iface, id, data, len)

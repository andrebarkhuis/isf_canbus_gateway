#include "logger/logger.h"

#include <cstdarg>
#include <cstdio>

#include "esp_log.h"
#include "esp_timer.h"

LogLevel Logger::level_ = LOG_LEVEL_INFO;
bool Logger::traceFrames_ = true;

namespace
{
const char *TAG = "gateway";

esp_log_level_t toEspLevel(LogLevel level)
{
    switch (level)
    {
    case LOG_LEVEL_ERROR:
        return ESP_LOG_ERROR;
    case LOG_LEVEL_WARN:
        return ESP_LOG_WARN;
    case LOG_LEVEL_INFO:
        return ESP_LOG_INFO;
    default:
        return ESP_LOG_DEBUG;
    }
}

char levelLetter(LogLevel level)
{
    switch (level)
    {
    case LOG_LEVEL_ERROR:
        return 'E';
    case LOG_LEVEL_WARN:
        return 'W';
    case LOG_LEVEL_INFO:
        return 'I';
    default:
        return 'D';
    }
}
} // namespace

void Logger::begin(LogLevel level, bool traceFrames)
{
    level_ = level;
    traceFrames_ = traceFrames;
    // esp_log filters by tag before our own level check; open it fully and let level_ decide.
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
    esp_log_write(ESP_LOG_INFO, TAG, "----------------------------------------------------\n");
    esp_log_write(ESP_LOG_INFO, TAG, "Logger initialized (level %d, frame trace %s)\n", level, traceFrames ? "on" : "off");
    esp_log_write(ESP_LOG_INFO, TAG, "----------------------------------------------------\n");
}

void Logger::setLevel(LogLevel level)
{
    level_ = level;
}

LogLevel Logger::level()
{
    return level_;
}

void Logger::setTraceFrames(bool on)
{
    traceFrames_ = on;
}

void Logger::log(LogLevel level, const char *function, const char *format, ...)
{
    if (level > level_ || level == LOG_LEVEL_NONE)
    {
        return;
    }
    char buffer[BUFFER_SIZE];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    esp_log_write(toEspLevel(level), TAG, "[%c][%s] %s\n", levelLetter(level), function, buffer);
}

void Logger::frame(const char *iface, uint32_t id, const uint8_t *data, uint8_t len)
{
    if (!traceFrames_)
    {
        return;
    }
    // candump line: "(1697700000.123456) isf 7E8#0661008400000100". The timestamp is seconds since boot;
    // ToyotaLens only needs it monotonic.
    int64_t us = esp_timer_get_time();
    char hex[2 * 8 + 1];
    int n = 0;
    for (uint8_t i = 0; i < len && i < 8; i++)
    {
        n += snprintf(hex + n, sizeof(hex) - n, "%02X", data[i]);
    }
    hex[n] = '\0';
    esp_log_write(ESP_LOG_INFO, TAG, "(%lld.%06lld) %s %03lX#%s\n", static_cast<long long>(us / 1000000),
                  static_cast<long long>(us % 1000000), iface, static_cast<unsigned long>(id), hex);
}

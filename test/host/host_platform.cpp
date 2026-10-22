// common.h and logger.h on a host compiler: steady clock, sleep, and logging to stdout.
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <thread>

#include "common.h"
#include "logger/logger.h"

namespace
{
const std::chrono::steady_clock::time_point START = std::chrono::steady_clock::now();
}

uint32_t nowMs()
{
    return static_cast<uint32_t>(nowUs() / 1000);
}

uint64_t nowUs()
{
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - START).count());
}

void sleepMs(uint32_t ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms == 0 ? 1 : ms));
}

LogLevel Logger::level_ = LOG_LEVEL_WARN;
bool Logger::traceFrames_ = false;

void Logger::begin(LogLevel level, bool traceFrames)
{
    level_ = level;
    traceFrames_ = traceFrames;
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
    printf("      [%d][%s] %s\n", level, function, buffer);
}

void Logger::frame(const char *iface, uint32_t id, const uint8_t *data, uint8_t len)
{
    if (!traceFrames_)
    {
        return;
    }
    uint64_t us = nowUs();
    printf("      (%llu.%06llu) %s %03X#", static_cast<unsigned long long>(us / 1000000),
           static_cast<unsigned long long>(us % 1000000), iface, static_cast<unsigned>(id));
    for (uint8_t i = 0; i < len; i++)
    {
        printf("%02X", data[i]);
    }
    printf("\n");
}

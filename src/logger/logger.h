#pragma once

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "../common.h"
#include <string>

#define LOG_ERROR 1
#define LOG_WARN 2
#define LOG_INFO 3
#define LOG_DEBUG 4

class Logger
{
private:
    static int logLevel;
    static constexpr size_t BUFFER_SIZE = 128; // Maximum log message size
    static SemaphoreHandle_t serialMutex;      // This should be used to protect Serial operations

public:
    static bool serialInitialized;
    static unsigned long startTime;            // Reference time for timestamp calculation

    static void setMutex(SemaphoreHandle_t mutex);
    static SemaphoreHandle_t getMutex();
    static void begin(int level = LOG_INFO, unsigned long baudRate = 115200);

    static void error(const char* func, const char *format, ...);
    static void warn(const char* func, const char *format, ...);
    static void info(const char* func, const char *format, ...);
    static void debug(const char* func, const char *format, ...);
    
    static void logCANMessage(const char *busName, uint32_t id, const uint8_t *data, uint8_t len, bool success, bool isTx, const char *description = nullptr);
    static void logUdsMessage(const char *description, const Message_t *msg);

private:
    static std::string formatData(const uint8_t *data, uint8_t len);
};

#define LOG_ERROR(format, ...) Logger::error(__FUNCTION__, format, ##__VA_ARGS__)
#define LOG_WARN(format, ...) Logger::warn(__FUNCTION__, format, ##__VA_ARGS__)
#define LOG_INFO(format, ...) Logger::info(__FUNCTION__, format, ##__VA_ARGS__)
#define LOG_DEBUG(format, ...) Logger::debug(__FUNCTION__, format, ##__VA_ARGS__)

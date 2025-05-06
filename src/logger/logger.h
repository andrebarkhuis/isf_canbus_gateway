#pragma once

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "../common.h"
#include <string>

// Define log levels
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
    static void error(const char *format, ...);
    static void warn(const char *format, ...);
    static void info(const char *format, ...);
    static void debug(const char *format, ...);
    static void logCANMessage(const char *busName, uint32_t id, const uint8_t *data, uint8_t len, bool success, bool isTx, const char *description = nullptr);
    static void logUdsMessage(const char *description, const Message_t *msg);

private:
    static std::string formatData(const uint8_t *data, uint8_t len);
};

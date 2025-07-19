#include "logger.h"

// Static member definitions
int Logger::logLevel = 4; // LOG_DEBUG
bool Logger::serialInitialized = false;
unsigned long Logger::startTime = 0;
SemaphoreHandle_t Logger::serialMutex = nullptr;

void Logger::setMutex(SemaphoreHandle_t mutex) {
    serialMutex = mutex;
}

SemaphoreHandle_t Logger::getMutex() {
    return serialMutex;
}

void Logger::begin(int level, unsigned long baudRate) {
    Serial.begin(baudRate);
    serialInitialized = true;
    logLevel = level;
    startTime = millis();
    Serial.println("----------------------------------------------------");
    Serial.println("Logger initialized");
    Serial.println("----------------------------------------------------");
}

void Logger::error(const char* func, const char *format, ...) {
    if (logLevel >= 1) { // LOG_ERROR
        if (serialMutex && xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
            char buffer[BUFFER_SIZE];
            int prefix_len = snprintf(buffer, sizeof(buffer), "[ERROR][%s] ", func);
            va_list args;
            va_start(args, format);
            vsnprintf(buffer + prefix_len, sizeof(buffer) - prefix_len, format, args);
            va_end(args);
            Serial.println(buffer);
            xSemaphoreGive(serialMutex);
        }
    }
}

void Logger::warn(const char* func, const char *format, ...) {
    if (logLevel >= 2) { // LOG_WARN
        if (serialMutex && xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
            char buffer[BUFFER_SIZE];
            int prefix_len = snprintf(buffer, sizeof(buffer), "[WARN][%s] ", func);
            va_list args;
            va_start(args, format);
            vsnprintf(buffer + prefix_len, sizeof(buffer) - prefix_len, format, args);
            va_end(args);
            Serial.println(buffer);
            xSemaphoreGive(serialMutex);
        }
    }
}

void Logger::info(const char* func, const char *format, ...) {
    if (logLevel >= 3) { // LOG_INFO
        if (serialMutex && xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
            char buffer[BUFFER_SIZE];
            int prefix_len = snprintf(buffer, sizeof(buffer), "[INFO][%s] ", func);
            va_list args;
            va_start(args, format);
            vsnprintf(buffer + prefix_len, sizeof(buffer) - prefix_len, format, args);
            va_end(args);
            Serial.println(buffer);
            xSemaphoreGive(serialMutex);
        }
    }
}

void Logger::debug(const char* func, const char *format, ...) {
    if (logLevel >= 4) { // LOG_DEBUG
        if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
            char buffer[BUFFER_SIZE];
            int prefix_len = snprintf(buffer, sizeof(buffer), "[DEBUG][%s] ", func);
            va_list args;
            va_start(args, format);
            vsnprintf(buffer + prefix_len, sizeof(buffer) - prefix_len, format, args);
            va_end(args);
            Serial.println(buffer);
            xSemaphoreGive(serialMutex);
        }
    }
}

std::string Logger::formatData(const uint8_t *data, uint8_t len) {
    std::string result;
    for (uint8_t i = 0; i < len; i++) {
        char byteStr[4];
        snprintf(byteStr, sizeof(byteStr), "%02X ", data[i]);
        result += byteStr;
    }
    return result;
}

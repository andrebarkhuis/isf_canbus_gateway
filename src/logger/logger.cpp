#include "logger.h"

// Static member definitions
int Logger::logLevel = LOG_DEBUG;
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

void Logger::error(const char *format, ...) {
    if (logLevel >= LOG_ERROR) {
        if (serialMutex && xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
            char buffer[BUFFER_SIZE];
            int prefix_len = snprintf(buffer, sizeof(buffer), "[ERROR] ");
            va_list args;
            va_start(args, format);
            vsnprintf(buffer + prefix_len, sizeof(buffer) - prefix_len, format, args);
            va_end(args);
            Serial.println(buffer);
            xSemaphoreGive(serialMutex);
        }
    }
}

void Logger::warn(const char *format, ...) {
    if (logLevel >= LOG_WARN) {
        if (serialMutex && xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
            char buffer[BUFFER_SIZE];
            int prefix_len = snprintf(buffer, sizeof(buffer), "[WARN] ");
            va_list args;
            va_start(args, format);
            vsnprintf(buffer + prefix_len, sizeof(buffer) - prefix_len, format, args);
            va_end(args);
            Serial.println(buffer);
            xSemaphoreGive(serialMutex);
        }
    }
}

void Logger::info(const char *format, ...) {
    if (logLevel >= LOG_INFO) {
        if (serialMutex && xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
            char buffer[BUFFER_SIZE];
            int prefix_len = snprintf(buffer, sizeof(buffer), "[INFO] ");
            va_list args;
            va_start(args, format);
            vsnprintf(buffer + prefix_len, sizeof(buffer) - prefix_len, format, args);
            va_end(args);
            Serial.println(buffer);
            xSemaphoreGive(serialMutex);
        }
    }
}

void Logger::debug(const char *format, ...) {
    if (logLevel >= LOG_DEBUG) {
        if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
            constexpr const char *prefix = "[DEBUG] ";
            constexpr size_t prefix_len = 8; // strlen("[DEBUG] ")
            char msgBuffer[BUFFER_SIZE - prefix_len];
            va_list args;
            va_start(args, format);
            vsnprintf(msgBuffer, sizeof(msgBuffer), format, args);
            va_end(args);
            char buffer[BUFFER_SIZE];
            snprintf(buffer, sizeof(buffer), "%s%s", prefix, msgBuffer);
            Serial.println(buffer);
            xSemaphoreGive(serialMutex);
        }
    }
}

void Logger::logCANMessage(const char *busName, uint32_t id, const uint8_t *data, uint8_t len, bool success, bool isTx, const char *description) {
    if (busName == nullptr)
        return;
    if (logLevel >= LOG_INFO) {
        if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
            char buffer[BUFFER_SIZE];
            snprintf(buffer, sizeof(buffer), "[%s] %s %s ID: 0x%lX Data: %s%s",
                     success ? "OK" : "FAIL",
                     busName,
                     isTx ? "TX" : "RX",
                     id,
                     data != nullptr ? formatData(data, len).c_str() : "",
                     description != nullptr ? description : "");
            Serial.println(buffer);
            xSemaphoreGive(serialMutex);
        }
    }
}

void Logger::logUdsMessage(const char *description, const Message_t *msg) {
    if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
        char buffer[BUFFER_SIZE];
        snprintf(buffer, sizeof(buffer), "%s state=%s tx_id=0x%lX, rx_id=0x%lX, len=%u, seq_id=%u, data=%s",
                 description,
                 msg->getStateStr().c_str(),
                 (unsigned long)msg->tx_id,
                 (unsigned long)msg->rx_id,
                 (unsigned int)msg->len,
                 (unsigned int)msg->seq_id,
                 msg->Buffer != nullptr ? formatData(msg->Buffer, msg->len).c_str() : "");
        Serial.println(buffer);
        xSemaphoreGive(serialMutex);
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

#pragma once

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <common_types.h>
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
  static bool serialInitialized;
  static unsigned long startTime;            // Reference time for timestamp calculation
  static constexpr size_t BUFFER_SIZE = 128; // Maximum log message size
  static SemaphoreHandle_t serialMutex;      // This should be used to protect Serial operations

public:
  static void setMutex(SemaphoreHandle_t mutex)
  {
    serialMutex = mutex;
  }

  static SemaphoreHandle_t getMutex()
  {
    return serialMutex;
  }

  static void
  begin(int level = LOG_INFO, unsigned long baudRate = 115200)
  {
    Serial.begin(baudRate);
    serialInitialized = true;

    logLevel = level;
    startTime = millis();

    Serial.println("----------------------------------------------------");
    Serial.println("Logger initialized");
    Serial.println("----------------------------------------------------");
  }

  static void error(const char *format, ...)
  {
    if (logLevel >= LOG_ERROR)
    {
      if (serialMutex && xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE)
      {
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

  static void warn(const char *format, ...)
  {
    if (logLevel >= LOG_WARN)
    {
      if (serialMutex && xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE)
      {
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

  static void info(const char *format, ...)
  {
    if (logLevel >= LOG_INFO)
    {
      if (serialMutex && xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE)
      {
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

  static void debug(const char *format, ...)
  {
    if (logLevel >= LOG_DEBUG)
    {
      if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE)
      {
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

  static void logCANMessage(const char *busName, uint32_t id, const uint8_t *data, uint8_t len, bool success, bool isTx, const char *description = nullptr)
  {
    if (busName == nullptr)
      return;

    if (logLevel >= LOG_INFO)
    {
      if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE)
      {
        char buffer[BUFFER_SIZE];
        snprintf(buffer, sizeof(buffer), "[%s] %s %s ID: 0x%lX Data: %s%s", // Use %lX for long unsigned int
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

  static void logUdsMessage(const char *description, const Message_t *msg)
  {
    if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE)
    {
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

  static void print_message_in(const char *method, const Message_t *msg)
  {
    if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE)
    {
      char buffer[BUFFER_SIZE];
      snprintf(buffer, sizeof(buffer),
               "[%s] tx_id=0x%lX, rx_id=0x%lX, len=%u, state=%s, seq_id=%u, data=%s",
               method,
               (unsigned long)msg->tx_id,
               (unsigned long)msg->rx_id,
               (unsigned int)msg->len,
               msg->getStateStr().c_str(),
               (unsigned int)msg->seq_id,
               msg->Buffer != nullptr ? formatData(msg->Buffer, msg->len).c_str() : "");
      Serial.println(buffer);
      xSemaphoreGive(serialMutex);
    }
  }

  static void print_message_out(const char *method, uint32_t id, uint8_t *buffer, uint16_t len)
  {
    if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE)
    {
      char formattedData[BUFFER_SIZE];
      snprintf(formattedData, sizeof(formattedData), "[%s] Buffer: 0x%lX [%d] %s",
               method,
               id,
               len,
               formatData(buffer, len).c_str());
      Serial.println(formattedData);
      xSemaphoreGive(serialMutex);
    }
  }

private:
  static std::string formatData(const uint8_t *data, uint8_t len)
  {
    std::string result;
    for (uint8_t i = 0; i < len; i++)
    {
      char byteStr[4];
      snprintf(byteStr, sizeof(byteStr), "%02X ", data[i]);
      result += byteStr;
    }
    return result;
  }
};

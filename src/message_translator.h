#pragma once

#include <vector>
#include <cstdint>
#include <common_types.h>
#include <utils/logger.h>
#include <uds_mapper.h>

class MessageTranslator {
public:
  /**
   * Translates ISF CAN messages to GT86 format
   * 
   * @param isfId The CAN ID of the ISF message
   * @param isfData Pointer to the ISF message data
   * @param dataLen Length of the data
   * @return Vector of translated GT86 messages
   */
  static std::vector<CANMessage> translateIsfToGt86(uint32_t isfId, uint8_t* isfData, uint8_t dataLen) {
    std::vector<CANMessage> translatedMessages;
  
    // Check message ID and apply appropriate translation
    switch(isfId) {
      case ISFCAN::RPM: // RPM message
        translatedMessages.push_back(translateRPM(isfData, dataLen));
        break;
        
      case ISFCAN::VEHICLE_SPEED: // Speed message
        translatedMessages.push_back(translateSpeed(isfData, dataLen));
        break;
        
      case ISFCAN::ENGINE_TEMP: // Temperature message
        translatedMessages.push_back(translateTemperature(isfData, dataLen));
        break;
    }
    
    return translatedMessages;
  }

  static CANMessage translateRPM(uint8_t* isfData, uint8_t dataLen) {
    // Check for valid data length
    if (dataLen < 2) {
      return createEmptyMessage(GT86CAN::ENGINE_DATA);
    }
    
    // Example: Extract RPM from ISF data and format for GT86
    uint16_t rpm = (isfData[0] << 8) | isfData[1];
    
    // Create GT86 format message with RPM data
    return createRPMMessage(rpm);
  }
  
  static CANMessage createRPMMessage(uint16_t rpm) {
    CANMessage gt86Msg;
    gt86Msg.id = GT86CAN::ENGINE_DATA;  // GT86 RPM message ID
    gt86Msg.len = 8;
    
    // Initialize data array
    memset(gt86Msg.data, 0, 8);
    
    // Set the RPM in GT86 format (adjust scaling as needed)
    gt86Msg.data[0] = rpm & 0xFF;
    gt86Msg.data[1] = (rpm >> 8) & 0xFF;
    
    // Log the translation
    Logger::debug("[MessageTranslator::createRPMMessage] Translated RPM: %u", rpm);
    
    return gt86Msg;
  }
  
  static CANMessage translateSpeed(uint8_t* isfData, uint8_t dataLen) {
    // Check for valid data length
    if (dataLen < 1) {
      return createEmptyMessage(GT86CAN::VEHICLE_SPEED);
    }
    
    // Extract speed from ISF data
    uint8_t speed = isfData[0];
    
    // Create GT86 speed message
    return createSpeedMessage(speed);
  }
  
  static CANMessage createSpeedMessage(uint8_t speed) {
    CANMessage gt86Msg;
    gt86Msg.id = GT86CAN::VEHICLE_SPEED;  // GT86 Speed message ID
    gt86Msg.len = 8;
    
    // Initialize data array
    memset(gt86Msg.data, 0, 8);
    
    // Set the speed in GT86 format
    gt86Msg.data[0] = speed;
    
    // Additional data might be needed for a complete GT86 message
    // For example, brake signal, etc.
    
    // Log the translation
    Logger::debug("[MessageTranslator::createSpeedMessage] Translated Speed: %u km/h", speed);
    
    return gt86Msg;
  }
  
  static CANMessage translateTemperature(uint8_t* isfData, uint8_t dataLen) {
    // Check for valid data length
    if (dataLen < 1) {
      return createEmptyMessage(GT86CAN::ENGINE_TEMP);
    }
    
    // Extract temperature from ISF data
    // Example: temperature in Celsius + 40 (common CAN protocol offset)
    int8_t temp = isfData[0] - 40;
    
    return createTemperatureMessage(temp);
  }
  
  static CANMessage createTemperatureMessage(int8_t temp) {
    CANMessage gt86Msg;
    gt86Msg.id = GT86CAN::ENGINE_TEMP;  // GT86 Temperature message ID
    gt86Msg.len = 8;
    
    // Initialize data array
    memset(gt86Msg.data, 0, 8);
    
    // Set the temperature in GT86 format
    gt86Msg.data[0] = temp + 40;  // Adding offset back for GT86 format
    
    // Log the translation
    Logger::debug("[MessageTranslator::createTemperatureMessage] Translated Temperature: %d°C", temp);
    
    return gt86Msg;
  }
  
  static CANMessage createEmptyMessage(uint32_t id) {
    CANMessage emptyMsg;
    emptyMsg.id = id;
    emptyMsg.len = 8;
    memset(emptyMsg.data, 0, 8);
    return emptyMsg;
  }
};

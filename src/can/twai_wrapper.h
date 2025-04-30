#pragma once

#include <Arduino.h>
#include <driver/twai.h>
#include "../utils/logger.h"
#include "../common_types.h"

//#ifndef DEBUG
//#define DEBUG  // Enable debug mode
//#endif

/**
 * @brief Specialized wrapper for ESP32's TWAI (Two-Wire Automotive Interface) CAN controller
 * 
 * This class provides an implementation of the CanInterface for the ESP32's built-in
 * CAN controller (TWAI). It's primarily used for communicating with the GT86 CAN bus at 500kbps.
 */
class TwaiWrapper {
    private:
    /** Bus identifier for logging purposes */
    static constexpr const char* BUS_NAME = "GT86";

    /** GPIO pin for TWAI TX (transmit) */
    static const unsigned TWAI_TX = 7;
    
    /** GPIO pin for TWAI RX (receive) */
    static const unsigned TWAI_RX = 6;

    /**
     * @brief Check for and handle any TWAI alerts
     * 
     * This method checks for bus errors, receive queue full conditions, and other alerts
     * from the TWAI driver and logs them appropriately.
     */
    void checkAlerts();

    public:
    /**
     * @brief Construct a new TwaiWrapper instance
     */
    TwaiWrapper();

    /**
     * @brief Destroy the TwaiWrapper instance and clean up resources
     */
    ~TwaiWrapper();

    /**
     * @brief Initialize the TWAI driver with appropriate settings
     * 
     * Configures and starts the ESP32 TWAI driver with 500kbps baud rate 
     * and sets up the RX/TX pins defined in the class.
     * 
     * @return true if initialization was successful, false otherwise
     */
    bool initialize() ;

    /**
     * @brief Send a CAN message to the GT86 CAN bus
     * 
     * @param msg CANMessage object containing ID, data, and length
     * @return true if message was sent successfully
     */
    bool sendMessage(const CANMessage& msg) ;

    /**
     * @brief Check if a CAN message is available and receive it
     * 
     * @param id Reference to store the received ID
     * @param data Buffer to store the received data
     * @param len Reference to store the received data length
     * @param extended Reference to store whether the message used extended ID
     * @return true if a message was received
     */
    bool receiveMessage(uint32_t& id, uint8_t* data, uint8_t& len, bool& extended) ;


};
#ifndef _TWAI_WRAPPER_H
#define _TWAI_WRAPPER_H

#include <Arduino.h>
#include <driver/twai.h>
#include "../logger/logger.h"
#include "../common.h"

// #ifndef DEBUG
// #define DEBUG  // Enable debug mode
// #endif

/**
 * @brief Specialized wrapper for ESP32's TWAI (Two-Wire Automotive Interface) CAN controller
 *
 * This class provides an implementation of the CanInterface for the ESP32's built-in
 * CAN controller (TWAI). It's primarily used for communicating with the GT86 CAN bus at 500kbps.
 */
class TwaiWrapper
{
private:
    /** Bus identifier for logging purposes */
    static constexpr const char *BUS_NAME = "ISF";

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
    bool initialize();

    /**
     * @brief Send a CAN message to the GT86 CAN bus
     *
     * @param id Reference to the ID of the message to send
     * @param data Buffer to the data to send
     * @param len Reference to the length of the data to send
     * @param extended Reference to whether the message used extended ID
     * @return true if message was sent successfully
     */
    bool sendMessage(uint32_t id, uint8_t *data, uint8_t len, bool extended);

    /**
     * @brief Check if a CAN message is available and receive it
     *
     * @param id Reference to the ID of the received message
     * @param data Buffer to the data of the received message
     * @param len Reference to the length of the data of the received message
     * @param extended Reference to whether the message used extended ID
     * @return true if a message was received
     */
    bool receiveMessage(uint32_t &id, uint8_t *data, uint8_t &len, bool &extended);
};

#endif
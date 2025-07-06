#include "twai_wrapper.h"
#include <string.h>
#include "../logger/logger.h"

// Ensure constructor is properly defined
TwaiWrapper::TwaiWrapper()
{
    // Actual initialization happens in initialize() method
}

TwaiWrapper::~TwaiWrapper()
{
    // Stop and uninstall the TWAI driver when this object is destroyed
    twai_stop();
    twai_driver_uninstall();
}

bool TwaiWrapper::initialize()
{
    // Configure TWAI driver with proper casting for GPIO pins
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)TWAI_TX, (gpio_num_t)TWAI_RX, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS(); // 500 kbps
    twai_filter_config_t filter_config = {
        .acceptance_code = (static_cast<uint32_t>(0x7E8) << 21),   // Shift into position
        .acceptance_mask = ~(static_cast<uint32_t>(0x7F0) << 21),  // Only accept 0x7E8–0x7B0 range
        .single_filter = true               // Use single filter for all RX
    };

    // Install and start TWAI driver
    esp_err_t result = twai_driver_install(&g_config, &t_config, &filter_config);
    if (result != ESP_OK)
    {
#ifdef TWAI_DEBUG
        LOG_ERROR("Failed to install TWAI driver: %d", result);
#endif
        return false;
    }

    result = twai_start();
    if (result != ESP_OK)
    {
#ifdef TWAI_DEBUG
        LOG_ERROR("Failed to start TWAI driver: %d", result);
#endif
        return false;
    }

#ifdef TWAI_INFO_PRINT
    LOG_INFO("TWAI interface initialized successfully on pins TX:%d/RX:%d at 500kbps", TWAI_TX, TWAI_RX);
#endif

    return true;
}

bool TwaiWrapper::sendMessage(uint32_t id, uint8_t *data, uint8_t len, bool extended)
{
    twai_message_t msg;
    msg.identifier = id;
    msg.extd = extended ? 1 : 0;
    msg.data_length_code = len;
    memcpy(msg.data, data, len);

    // Send message with a timeout
    esp_err_t result = twai_transmit(&msg, pdMS_TO_TICKS(10)); // 10ms timeout

#ifdef TWAI_DEBUG
    Logger::logCANMessage("TWAI", msg.identifier, msg.data, msg.data_length_code, (result == ESP_OK), true);
#endif

#ifdef TWAI_DEBUG
    checkAlerts();
#endif

    return (result == ESP_OK);
}

bool TwaiWrapper::receiveMessage(uint32_t &id, uint8_t *data, uint8_t &len, bool &extended)
{
    twai_message_t twai_msg;

    // Try to receive a message with timeout
    esp_err_t result = twai_receive(&twai_msg, pdMS_TO_TICKS(10)); // 10ms timeout
    if (result != ESP_OK)
    {
        return false; // No message received
    }

    // Fill the output parameters
    id = twai_msg.identifier;
    extended = twai_msg.extd;
    len = twai_msg.data_length_code;
    memcpy(data, twai_msg.data, len);

#ifdef TWAI_INFO_PRINT
    Logger::logCANMessage("TWAI", id, data, len, true, false);
#endif

    return true;
}

void TwaiWrapper::checkAlerts()
{
    uint32_t alerts;
    esp_err_t alert_result = twai_read_alerts(&alerts, pdMS_TO_TICKS(10));

    // Handle ESP_ERR_TIMEOUT (263) silently as it's expected when no alerts are pending
    if (alert_result == ESP_ERR_TIMEOUT)
    {
        return; // No alerts available, just return silently
    }
    else if (alert_result != ESP_OK)
    {
        // Only log other errors
        static unsigned long lastErrorLogTime = 0;
        unsigned long currentTime = millis();

        // Limit logging frequency to avoid serial spam
        if (currentTime - lastErrorLogTime > 5000)
        { // Log once every 5 seconds at most
#ifdef TWAI_DEBUG
            LOG_WARN("Failed to read TWAI alerts: %d", alert_result);
#endif
            lastErrorLogTime = currentTime;
        }
        return;
    }

    // Check for bus errors
    if (alerts & TWAI_ALERT_BUS_ERROR)
    {
#ifdef TWAI_DEBUG
        LOG_WARN("Bus error detected on TWAI bus");
#endif
    }

    // Check if receive queue is full
    if (alerts & TWAI_ALERT_RX_QUEUE_FULL)
    {
#ifdef TWAI_DEBUG
        LOG_WARN("Receive queue full on TWAI bus");
#endif
    }

    // Check for other important alerts
    if (alerts & TWAI_ALERT_TX_FAILED)
    {
#ifdef TWAI_DEBUG
        LOG_WARN("TX failed on TWAI bus");
#endif
    }

    if (alerts & TWAI_ALERT_ERR_PASS)
    {
#ifdef TWAI_DEBUG
        LOG_WARN("TWAI bus entered error passive state");
#endif
    }

    if (alerts & TWAI_ALERT_BUS_OFF)
    {
#ifdef TWAI_DEBUG
        LOG_ERROR("TWAI bus is off - attempting recovery");
#endif
        // Attempt to recover the bus
        twai_stop();
        twai_start();
    }
}

#include "twai_wrapper.h"
#include "esp_err.h"  // Required for error name helpers
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
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)TWAI_TX, (gpio_num_t)TWAI_RX, TWAI_MODE_NORMAL);
    g_config.rx_queue_len = 32;

    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    esp_err_t result = twai_driver_install(&g_config, &t_config, &f_config);
    if (result != ESP_OK)
    {
#ifdef TWAI_INFO_PRINT
        LOG_ERROR("Failed to install TWAI driver: %d", result);
#endif
        return false;
    }

    result = twai_start();
    if (result != ESP_OK)
    {
#ifdef TWAI_INFO_PRINT
        LOG_ERROR("Failed to start TWAI driver: %d", result);
#endif
        return false;
    }

#ifdef TWAI_INFO_PRINT
    LOG_INFO("TWAI interface initialized successfully on pins TX:%d/RX:%d at 500kbps", TWAI_TX, TWAI_RX);
#endif

    return true;
}

bool TwaiWrapper::sendMessage(uint32_t id, uint8_t *data, uint8_t len)
{
    twai_message_t msg = {};
    msg.identifier = id;
    msg.extd = 0;
    msg.data_length_code = len;
    memcpy(msg.data, data, len);

    esp_err_t result = twai_transmit(&msg, pdMS_TO_TICKS(5));

    return (result == ESP_OK);
}

bool TwaiWrapper::receiveMessage(uint32_t &id, uint8_t *data, uint8_t &len)
{
    twai_message_t twai_msg;

    if (twai_receive(&twai_msg, pdMS_TO_TICKS(5)) == ESP_OK) {
        id = twai_msg.identifier;
        len = twai_msg.data_length_code;
        memcpy(data, twai_msg.data, len);
        return true;
    }
    return false;
}



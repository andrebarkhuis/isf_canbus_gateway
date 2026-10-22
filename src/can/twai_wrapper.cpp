#include "can/twai_wrapper.h"

#include <cstring>

#include "driver/twai.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "isotp/iso_tp.h"
#include "logger/logger.h"

TwaiWrapper::TwaiWrapper()
{
}

TwaiWrapper::~TwaiWrapper()
{
    if (open_)
    {
        twai_stop();
        twai_driver_uninstall();
    }
}

bool TwaiWrapper::initialize()
{
    if (open_)
    {
        return true;
    }
    twai_general_config_t g_config =
        TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)TWAI_TX, (gpio_num_t)TWAI_RX, TWAI_MODE_NORMAL);
    g_config.rx_queue_len = 64;
    g_config.tx_queue_len = 16;
    g_config.alerts_enabled = TWAI_ALERT_BUS_ERROR | TWAI_ALERT_ERR_PASS | TWAI_ALERT_BUS_OFF |
                              TWAI_ALERT_BUS_RECOVERED | TWAI_ALERT_RX_QUEUE_FULL | TWAI_ALERT_TX_FAILED;
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    esp_err_t result = twai_driver_install(&g_config, &t_config, &f_config);
    if (result != ESP_OK)
    {
        LOG_ERROR("Failed to install TWAI driver: %d", result);
        return false;
    }
    result = twai_start();
    if (result != ESP_OK)
    {
        LOG_ERROR("Failed to start TWAI driver: %d", result);
        twai_driver_uninstall();
        return false;
    }
    open_ = true;
    LOG_INFO("TWAI (ISF bus) started on TX %d / RX %d at 500 kbit/s", TWAI_TX, TWAI_RX);
    return true;
}

bool TwaiWrapper::isOpen() const
{
    return open_;
}

bool TwaiWrapper::send(uint32_t id, const uint8_t *data, uint8_t len)
{
    twai_message_t msg = {};
    msg.identifier = id;
    msg.extd = 0;
    msg.data_length_code = len;
    memcpy(msg.data, data, len);

    esp_err_t result = twai_transmit(&msg, pdMS_TO_TICKS(IsoTpTimers::N_As_Ms));
    if (result != ESP_OK)
    {
        LOG_WARN("twai_transmit %03lX failed: %d", static_cast<unsigned long>(id), result);
        checkAlerts();
        return false;
    }
    LOG_FRAME(INTERFACE, id, data, len);
    return true;
}

bool TwaiWrapper::tryReceive(uint32_t &id, uint8_t *data, uint8_t &len, uint32_t timeoutMs)
{
    twai_message_t msg;
    if (twai_receive(&msg, pdMS_TO_TICKS(timeoutMs)) != ESP_OK)
    {
        return false;
    }
    id = msg.identifier;
    len = msg.data_length_code > 8 ? 8 : msg.data_length_code;
    memcpy(data, msg.data, len);
    LOG_FRAME(INTERFACE, id, data, len);
    return true;
}

void TwaiWrapper::drain()
{
    twai_message_t msg;
    while (twai_receive(&msg, 0) == ESP_OK)
    {
    }
}

void TwaiWrapper::checkAlerts()
{
    uint32_t alerts = 0;
    if (twai_read_alerts(&alerts, 0) != ESP_OK)
    {
        return;
    }
    if (alerts & TWAI_ALERT_BUS_ERROR)
    {
        LOG_WARN("ISF bus: bus error");
    }
    if (alerts & TWAI_ALERT_RX_QUEUE_FULL)
    {
        LOG_WARN("ISF bus: receive queue full, frames lost");
    }
    if (alerts & TWAI_ALERT_TX_FAILED)
    {
        LOG_WARN("ISF bus: transmit failed");
    }
    if (alerts & TWAI_ALERT_ERR_PASS)
    {
        LOG_WARN("ISF bus: error passive");
    }
    if (alerts & TWAI_ALERT_BUS_OFF)
    {
        LOG_ERROR("ISF bus: bus off, starting recovery");
        twai_initiate_recovery();
    }
    if (alerts & TWAI_ALERT_BUS_RECOVERED)
    {
        LOG_INFO("ISF bus: recovered, restarting");
        twai_start();
    }
}

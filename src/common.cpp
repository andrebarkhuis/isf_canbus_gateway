#include "common.h"

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

uint32_t nowMs()
{
    return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

uint64_t nowUs()
{
    return static_cast<uint64_t>(esp_timer_get_time());
}

void sleepMs(uint32_t ms)
{
    // A zero delay still yields, so a polling loop cannot starve the other tasks on its core.
    vTaskDelay(ms == 0 ? 1 : pdMS_TO_TICKS(ms));
}

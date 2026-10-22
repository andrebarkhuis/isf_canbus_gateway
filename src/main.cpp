/* ========================================================================
    Project Name: GT86-ISF CAN Gateway (ESP32-CAN-X2)
    Author: Andre
    Platform: ESP32-S3 (ESP32-CAN-X2 Dev Board)
    Environment: ESP-IDF (PlatformIO)

    Two tasks, one per bus: the ISF task (core 0) runs the Toyota Mode 21 session over ISO-TP and translates
    what it decodes into output frames; the GT86 task (core 1) keeps the GT86 packets and the ADU frames on
    the second bus. Translated frames travel from the first to the second through one queue.
======================================================================== */
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "common.h"
#include "logger/logger.h"
#include "services/gt86_service.h"
#include "services/isf_service.h"

namespace
{
constexpr uint32_t ISF_TASK_STACK_SIZE = 16384;
constexpr uint32_t GT86_TASK_STACK_SIZE = 8192;
constexpr UBaseType_t TASK_PRIORITY = 5;
constexpr uint32_t CPU_COOLDOWN_MS = 5;
constexpr uint32_t SUPERVISOR_PERIOD_MS = 1000;
constexpr size_t MESSAGE_QUEUE_LENGTH = 64;

QueueHandle_t messageQueue = nullptr;
TaskHandle_t isfTaskHandle = nullptr;
TaskHandle_t gt86TaskHandle = nullptr;

/// The ISF task's translated frames go here; the GT86 task takes them out.
void queueFrame(const CANMessage &frame)
{
    if (messageQueue != nullptr && xQueueSend(messageQueue, &frame, 0) != pdTRUE)
    {
        LOG_WARN("message queue full, frame %03lX dropped", static_cast<unsigned long>(frame.id));
    }
}

void isfTask(void *)
{
    LOG_INFO("ISF task on core %d", xPortGetCoreID());
    IsfService service;
    service.setFrameSink(queueFrame);
    while (!service.initialize())
    {
        LOG_ERROR("ISF service failed to initialize; retrying");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    for (;;)
    {
        service.listen();
        vTaskDelay(pdMS_TO_TICKS(CPU_COOLDOWN_MS));
    }
}

void gt86Task(void *)
{
    LOG_INFO("GT86 task on core %d", xPortGetCoreID());
    Gt86Service service(messageQueue);
    while (!service.initialize())
    {
        LOG_ERROR("GT86 service failed to initialize; retrying");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    for (;;)
    {
        service.listen();
        vTaskDelay(pdMS_TO_TICKS(CPU_COOLDOWN_MS));
    }
}

void startTasks()
{
    xTaskCreatePinnedToCore(isfTask, "ISF Task", ISF_TASK_STACK_SIZE, nullptr, TASK_PRIORITY, &isfTaskHandle, 0);
    xTaskCreatePinnedToCore(gt86Task, "GT86 Task", GT86_TASK_STACK_SIZE, nullptr, TASK_PRIORITY, &gt86TaskHandle, 1);
}
} // namespace

extern "C" void app_main(void)
{
    Logger::begin(LOG_LEVEL_INFO, true);
    LOG_INFO("Starting GT86-ISF CAN gateway");

    messageQueue = xQueueCreate(MESSAGE_QUEUE_LENGTH, sizeof(CANMessage));
    if (messageQueue == nullptr)
    {
        LOG_ERROR("Failed to create the message queue");
    }

    startTasks();

    for (;;)
    {
        if (isfTaskHandle != nullptr && eTaskGetState(isfTaskHandle) == eDeleted)
        {
            LOG_ERROR("ISF task ended; restarting it");
            xTaskCreatePinnedToCore(isfTask, "ISF Task", ISF_TASK_STACK_SIZE, nullptr, TASK_PRIORITY, &isfTaskHandle, 0);
        }
        if (gt86TaskHandle != nullptr && eTaskGetState(gt86TaskHandle) == eDeleted)
        {
            LOG_ERROR("GT86 task ended; restarting it");
            xTaskCreatePinnedToCore(gt86Task, "GT86 Task", GT86_TASK_STACK_SIZE, nullptr, TASK_PRIORITY, &gt86TaskHandle, 1);
        }
        vTaskDelay(pdMS_TO_TICKS(SUPERVISOR_PERIOD_MS));
    }
}

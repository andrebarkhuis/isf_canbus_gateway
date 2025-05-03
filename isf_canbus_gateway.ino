/* ========================================================================
    Project Name: GT86-ISF CAN Gateway (ESP32-CAN-X2)
    Author: Andre
    Date: <Insert Date>
    Platform: ESP32-S3 (ESP32-CAN-X2 Dev Board)
    Environment: Arduino Framework (ESP32 Core)
======================================================================== */
#include <Arduino.h>
#include <SPI.h>
#include "logger/logger.h"
#include "services/isf_service.h"
#include "services/gt86_service.h"

// Define LED pin if it's not already defined
#ifndef LED_BUILTIN
#define LED_BUILTIN 2 // Common LED pin on ESP32 dev boards
#endif

// Create a mutex for serial port access
SemaphoreHandle_t serialMutex = NULL;

#define ISF_TASK_STACK_SIZE 15360
#define GT86_TASK_STACK_SIZE 6144

static constexpr unsigned long CPU_COOLDOWN_TIME = 5;

// Task handles to monitor status
TaskHandle_t isfTaskHandle = NULL;
TaskHandle_t gt86TaskHandle = NULL;

// ISF Service Task (Core 0)
void isfTask(void *parameter)
{
    Logger::info("[autosport_gateway::isfTask] ISF Task running on Core: %d", xPortGetCoreID());

    IsfService *isfService = new IsfService();

    if (!isfService->initialize())
    {

        Logger::error("[autosport_gateway::isfTask] Failed to initialize ISF CAN");

        vTaskDelete(NULL); // Terminate task if init fails
    }

    for (;;)
    {
        try
        {
            // The "listen" method is the main loop that receives and sends canbus messages continuously.
            isfService->listen();
        }
        catch (...)
        {
            Logger::error("[autosport_gateway::isfTask] Exception in ISF task - recovering");
            vTaskDelay(pdMS_TO_TICKS(1000)); // Delay to recover
        }

        vTaskDelay(pdMS_TO_TICKS(CPU_COOLDOWN_TIME));
    }
}

// GT86 Service Task (Core 1)
void gt86Task(void *parameter)
{
    Logger::info("[autosport_gateway::gt86Task] GT86 Task running on Core: %d", xPortGetCoreID());

    Gt86Service *gt86Service = new Gt86Service();

    if (!gt86Service->initialize())
    {
        Logger::error("[autosport_gateway::gt86Task] Failed to initialize GT86 CAN");
        vTaskDelete(NULL);
    }

    for (;;)
    {
        try
        {
            // The "listen" method is the main loop that receives and sends canbus messages continuously.
            gt86Service->listen();
        }
        catch (...)
        {
            Logger::error("[autosport_gateway::gt86Task] Exception in GT86 task - recovering");
            vTaskDelay(pdMS_TO_TICKS(1000)); // Delay to recover
        }

        vTaskDelay(pdMS_TO_TICKS(CPU_COOLDOWN_TIME));
    }
}

void setup()
{
    Serial.begin(115200);

    vTaskDelay(pdMS_TO_TICKS(1000));

    // Add some immediate debug prints to verify Serial is working
    Serial.println("Starting setup.");

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH); // Assume active LOW

    // Initialize the serial mutex
    serialMutex = xSemaphoreCreateMutex();
    if (serialMutex == NULL)
    {
        Serial.println("Error creating serial mutex.");
    }
    else
    {
        Serial.println("Serial mutex created successfully.");
    }

    // Set the mutex in the Logger class
    Logger::setMutex(serialMutex);

    vTaskDelay(pdMS_TO_TICKS(1000));

    // Initialize the logger with DEBUG level to see all messages
    Logger::begin(LOG_DEBUG);

    // Test direct logging to verify it works
    Logger::info("[autosport_gateway::setup] Setup started.");

    // Create Tasks with increased stack size - each on a different core
    xTaskCreatePinnedToCore(isfTask, "ISF Task", ISF_TASK_STACK_SIZE, NULL, 1, &isfTaskHandle, 0);
    xTaskCreatePinnedToCore(gt86Task, "GT86 Task", GT86_TASK_STACK_SIZE, NULL, 1, &gt86TaskHandle, 1);

    vTaskDelay(pdMS_TO_TICKS(1000)); // 1 second delay before entering loop

    Logger::info("[autosport_gateway::setup] Setup complete.");
}

void loop()
{

    if (isfTaskHandle != NULL && eTaskGetState(isfTaskHandle) == eDeleted)
    {
        Logger::error("[autosport_gateway::setup] ISF task crashed - attempting restart.");
        xTaskCreatePinnedToCore(isfTask, "ISF Task", ISF_TASK_STACK_SIZE, NULL, 1, &isfTaskHandle, 0);
    }

    if (gt86TaskHandle != NULL && eTaskGetState(gt86TaskHandle) == eDeleted)
    {
        Logger::error("[autosport_gateway::setup] GT86 task crashed - attempting restart.");
        xTaskCreatePinnedToCore(gt86Task, "GT86 Task", GT86_TASK_STACK_SIZE, NULL, 1, &gt86TaskHandle, 1);
    }

    vTaskDelay(pdMS_TO_TICKS(CPU_COOLDOWN_TIME));
}

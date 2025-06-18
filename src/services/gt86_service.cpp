#include "gt86_service.h"
#include "../logger/logger.h"
#include "../message_translator.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

Gt86Service::Gt86Service()
{
    twaiWrapper = new TwaiWrapper();                                // Make sure to initialize the pointer
    lastMessageTime = new unsigned long[GT86_CAN_MESSAGES_COUNT](); // () initializes all elements to 0
}

Gt86Service::~Gt86Service()
{
    // Clean up dynamically allocated objects
    delete twaiWrapper;
    delete[] lastMessageTime;
}

bool Gt86Service::initialize()
{
    bool result = twaiWrapper->initialize();
    vTaskDelay(pdMS_TO_TICKS(10));
    return result;
}

void Gt86Service::listen()
{
    sendPidRequests();
    
    vTaskDelay(pdMS_TO_TICKS(5));

    handleIncomingMessages();
}

bool Gt86Service::sendPidRequests()
{
    unsigned long currentTime = millis();
    bool success = true;

    for (int i = 0; i < GT86_CAN_MESSAGES_COUNT; i++) // Iterate through all messages
    {
        if (currentTime - lastMessageTime[i] >= GT86_PID_MESSAGES[i].interval)
        {
            if (!twaiWrapper->sendMessage(GT86_PID_MESSAGES[i]))
            {
                success = false;
            }
            else
            {
                lastMessageTime[i] = currentTime;
            }

            // Only introduce a delay after a certain number of messages to avoid unnecessary delays
            if ((i + 1) % 2 == 0) // Introduce a delay after sending 2 messages
            {
                vTaskDelay(pdMS_TO_TICKS(10)); // You can adjust this delay if needed
            }
        }
    }

    return success;
}

bool Gt86Service::handleIncomingMessages()
{
    bool success = true;
    uint32_t id;
    uint8_t data[8];
    uint8_t len;
    bool extended;
    int messagesProcessed = 0;

    while (twaiWrapper->receiveMessage(id, data, len, extended) && messagesProcessed < 5)
    {
        messagesProcessed++;
    }
    return success;
}
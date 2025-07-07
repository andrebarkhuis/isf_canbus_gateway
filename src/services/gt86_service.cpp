#include "gt86_service.h"
#include "../logger/logger.h"
#include "../message_translator.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>


Gt86Service::Gt86Service()
{
    mcp = new MCP_CAN(10); // Initialize MCP_CAN with CS pin 10
    lastMessageTime = new unsigned long[GT86_CAN_MESSAGES_COUNT](); // initialize all elements to 0
}

Gt86Service::~Gt86Service()
{
    // Clean up dynamically allocated objects
    delete mcp;
    delete[] lastMessageTime;
}

bool Gt86Service::initialize()
{
    byte res = mcp->begin();
    #ifdef DEBUG_GT86_SERVICE
        LOG_INFO("MCP_CAN initialized with result: %d", res);
    #endif
    vTaskDelay(pdMS_TO_TICKS(10));
    return res == CAN_OK;
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
        CANMessage msg = GT86_PID_MESSAGES[i];
        
        if (currentTime - lastMessageTime[i] >= msg.interval)
        {
            
            if (mcp->sendMsgBuf(msg.id, 0, 8, reinterpret_cast<byte *>(msg.data)) != CAN_OK)
            {
                #ifdef DEBUG_GT86_SERVICE
                    LOG_ERROR("Failed to send message ID: 0x%X, %s", msg.id, msg.param_name.c_str());
                #endif
                success = false;
            }
            else
            {
                #ifdef DEBUG_GT86_SERVICE
                    LOG_INFO("Sent message ID: 0x%X, %s", msg.id, msg.param_name.c_str());
                #endif
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
    // uint32_t id;
    // uint8_t data[8];
    // uint8_t len;
    // bool extended;
    int messagesProcessed = 0;

    while (mcp->checkReceive() == CAN_MSGAVAIL && messagesProcessed < 5)
    {
        messagesProcessed++;
    }
    return success;
}
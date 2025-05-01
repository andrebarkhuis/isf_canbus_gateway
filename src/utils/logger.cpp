#include <utils/logger.h>


int Logger::logLevel = LOG_DEBUG;  // Change default to DEBUG level
bool Logger::serialInitialized = false;
unsigned long Logger::startTime = 0;
SemaphoreHandle_t Logger::serialMutex = NULL;
void print_message_in(const char *method, const Message_t *msg);
void print_message_out(const char *method, uint32_t id, uint8_t *buffer, uint16_t len);
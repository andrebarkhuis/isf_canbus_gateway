#pragma once

#include "../mcp_can/mcp_can.h"
#include "../common.h"


//#define DEBUG_GT86_SERVICE        0

// GT86 CAN messages to be sent periodically
const CANMessage GT86_PID_MESSAGES[] = {
    // CAN ID: 0xD1 (209) - Vehicle Speed & Brake Data
    {0xD1, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 8, false, 500, "Speed, brake Pedal"}, // 500ms / 50Hz - Speed, Brake Pedal

    // CAN ID: 0xD3 (211) - Light Status Data
    {0xD3, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 8, false, 500, "VSC, TCS, SCS Lights"}, // 500ms / 50Hz - VSC, TCS, SCS Lights

    // CAN ID: 0x140 (320) - Engine Data 1
    {0x140, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 8, true, 100, "Engine RPM, Throttle, Accelerator"}, // 100ms / 100Hz - Engine RPM, Throttle, Accelerator

    // CAN ID: 0x141 (321) - Engine Data 2
    {0x141, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 8, true, 0, "Engine Load, Gear Position"}, // 100ms / 100Hz - Engine Load, Gear Position

    // CAN ID: 0x142 (322) - Engine Misc Data
    {0x142, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 8, false, 100, "Unknown"}, // 100ms / 100Hz - Unknown

    // CAN ID: 0x361 (865) - Warning & Gear Data
    {0x361, {0x00, 0x29, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 8, false, 200 , "Warning Light, Gear"}, // 200ms / 5Hz - Warning Light, Gear

    // CAN ID: 0x370 (880) - Steering & EPS Status
    {0x370, {0x00, 0x00, 0x01, 0x01, 0x00, 0x03, 0x00, 0x00}, 8, false, 200, "EPS, Steering Torque"}, // 200ms / 5Hz - EPS, Steering Torque

    // CAN ID: 0x368 (872) - Misc Data
    {0x368, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 8, false, 100, "Unknown"}, // 100ms / 10Hz - Unknown

    // CAN ID: 0x4C6 (1222) - Diagnostic Response
    {0x4C6, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 8, false, 10000, "Diagnostic Response"}, // 10000ms / 0.1Hz - Diagnostic Response

    // CAN ID: 0x4C8 (1224) - Diagnostic Response 2
    {0x4C8, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 8, false, 10000, "Diagnostic Response"}, // 10000ms / 0.1Hz - Diagnostic Response

    // CAN ID: 0x4DC (1244) - Unknown Diagnostic
    {0x4DC, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 8, false, 10000, "Unknown Diagnostic"}, // 10000ms / 0.1Hz - Unknown Diagnostic

    // CAN ID: 0x4DD (1245) - Unknown Diagnostic
    {0x4DD, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 8, false, 10000, "Unknown Diagnostic"}, // 10000ms / 0.1Hz - Unknown Diagnostic

    // CAN ID: 0x63B (1595) - ABS Sensor Data
    {0x63B, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 8, false, 2000, "ABS Sensors"}, // 2000ms / 0.5Hz - ABS Sensors

    // CAN ID: 0x6E1 (1761) - EPS Diagnostic Data
    {0x6E1, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 8, false, 10000, "EPS Diagnostic"}, // 10000ms / 0.1Hz - EPS Diagnostic

    // CAN ID: 0x6E2 (1762) - EPS Diagnostic Data 2
    {0x6E2, {0xA2, 0x00, 0xCC, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE}, 8, false, 10000, "EPS Diagnostic"}, // 10000ms / 0.1Hz - EPS Diagnostic

    // CAN ID: 0x7C8 (1992) - Fuel Level
    {0x7C8, {0x03, 0x61, 0x29, 0x5A, 0x00, 0x00, 0x00, 0x00}, 8, false, 1000, "Fuel Level"} // Fuel Level: 45 liters (0x5A = 45 * 2)
};

// Calculate the size of the array
const int GT86_CAN_MESSAGES_COUNT = sizeof(GT86_PID_MESSAGES) / sizeof(GT86_PID_MESSAGES[0]);

// #define DEBUG_GT86 // Enable debug mode for GT86

class Gt86Service
{
private:
    MCP_CAN *mcp; // Using MCP_CAN library for CAN communication


    unsigned long *lastMessageTime;

    // For monitoring stack usage
    unsigned long lastStackCheck = 0;
    static constexpr unsigned long STACK_CHECK_INTERVAL = 5000; // Check stack every 5 seconds

    // Private methods
    bool sendPidRequests();
    bool handleIncomingMessages();

public:
    Gt86Service();
    ~Gt86Service();
    bool initialize(); // Initialize MCP_CAN controller
    void listen(); // Periodically send PID requests and process incoming messages
};

#pragma once

#include <Arduino.h>
#include "../common.h"
#include "../mcp_can/mcp_can.h"
#include "../isotp/iso_tp.h"


enum UnitType {
    UNIT_GENERAL = 0, // Generic / ECU Identifiers
    UNIT_ACCELERATION = 1, // Acceleration, Gradient
    UNIT_G_FORCE = 2, // G-force sensors
    UNIT_ACCEL_REQUEST = 3, // Acceleration request signals
    UNIT_DECELERATION = 4, // Deceleration sensor
    UNIT_IGNITION_FEEDBACK = 5, // Ignition timing and feedback
    UNIT_ANGLE_SENSOR = 6, // Absolute angles (Steering, Pinion)
    UNIT_YAW_RATE = 7, // Yaw rate sensors
    UNIT_CURRENT_SENSOR = 9, // Current draw sensors (Throttle, Clutch)
    UNIT_PM_SENSOR = 11, // Particulate Matter Sensors
    UNIT_DISTANCE = 13, // Distance / Mileage / Odometer-related
    UNIT_FORWARD_DISTANCE = 14, // Forward vehicle distance measurement
    UNIT_ODOMETER = 17, // Odometer history and mileage
    UNIT_BATTERY_STATUS = 18, // Battery charge, hybrid systems
    UNIT_POWER_MANAGEMENT = 19, // Request Power, Wout Control
    UNIT_HYBRID_BATTERY = 22, // Hybrid / EV Battery power levels
    UNIT_FUEL_SYSTEM = 23, // Fuel system-related parameters
    UNIT_FUEL_INJECTION = 24, // Injection volume, fuel pump parameters
    UNIT_CRUISE_CONTROL = 25, // Cruise control request forces
    UNIT_FREQUENCY_SENSOR = 29, // Frequency-based sensors (Motor, Generator)
    UNIT_ILLUMINATION_SENSOR = 30, // Light control, brightness
    UNIT_EXHAUST_SENSOR = 32, // NOx and exhaust emissions
    UNIT_LOAD_FUEL_TRIM = 33, // Load calculations, Fuel trims
    UNIT_MAP_TIRE_PRESSURE = 34, // Manifold Pressure and Tire Inflation sensors
    UNIT_ENGINE_RPM = 39, // Engine Speed, RPM
    UNIT_SPEED_SENSOR = 42, // Vehicle Speed Sensors
    UNIT_VOLTAGE_SENSOR = 48, // Oxygen Sensor, Solar Voltage, Battery Voltage
    UNIT_TEMPERATURE_SENSOR = 57, // Coolant Temperature, Intake Air Temp
    UNIT_TORQUE_SENSOR = 58, // Steering, Motor, Brake Torque
    UNIT_POSITION_SENSOR = 59, // Throttle, Clutch, ASL Gear Position
    UNIT_AMBIENT_TEMP = 66, // Outside temperature sensors
    UNIT_MASS_AIR_FLOW = 75 // MAF Sensors (filtered & raw values)
};

class MCP_CAN;
class IsoTp;

#define DEBUG_ISF

const CANMessage isf_pid_session_requests[] = {
    { .id = 0x700, .data = {0x02, 0x3E, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00}, .len = 8, .extended = false, .interval = 2400, .param_name = "Tester present - Keep Alive" },
    { .id = 0x7E0, .data = {0x02, 0x3E, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00}, .len = 8, .extended = false, .interval = 2400, .param_name = "Tester present - Keep Alive" },
    { .id = 0x7A1, .data = {0x02, 0x3E, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00}, .len = 8, .extended = false, .interval = 2400, .param_name = "Tester present - Keep Alive" },
    { .id = 0x7A2, .data = {0x02, 0x3E, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00}, .len = 8, .extended = false, .interval = 2400, .param_name = "Tester present - Keep Alive" },
    { .id = 0x7D0, .data = {0x02, 0x3E, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00}, .len = 8, .extended = false, .interval = 2400, .param_name = "Tester present - Keep Alive" }
};

const UDSRequest isf_uds_requests[] = {

    //Default pid to 0 for now since we making the uds requests using the local id data identifier.
    { .tx_id = 0x7DF, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x01, .interval = 100, .param_name = "request-1" },
    { .tx_id = 0x7DF, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x03, .interval = 100, .param_name = "request-2" },
    { .tx_id = 0x7DF, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x04, .interval = 100, .param_name = "request-3" },
    { .tx_id = 0x7DF, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x05, .interval = 100, .param_name = "request-3" },
    { .tx_id = 0x7DF, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x06, .interval = 100, .param_name = "request-4" },
    { .tx_id = 0x7DF, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x07, .interval = 100, .param_name = "request-5" },
    { .tx_id = 0x7DF, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x08, .interval = 100, .param_name = "request-6" },
    { .tx_id = 0x7DF, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x22, .interval = 100, .param_name = "request-7" },
    { .tx_id = 0x7DF, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x20, .interval = 100, .param_name = "request-7" },
    { .tx_id = 0x7DF, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x25, .interval = 100, .param_name = "request-8" },
    { .tx_id = 0x7DF, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x37, .interval = 100, .param_name = "request-9" },
    { .tx_id = 0x7DF, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x51, .interval = 100, .param_name = "request-13" },
    { .tx_id = 0x7DF, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x82, .interval = 100, .param_name = "request-14" },
    { .tx_id = 0x7DF, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x83, .interval = 100, .param_name = "request-15" },
    { .tx_id = 0x7DF, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0xE1, .interval = 100, .param_name = "request-15" },
    { .tx_id = 0x7DF, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0xC2, .interval = 150, .param_name = "engine-code" }
};

const int SESSION_REQUESTS_SIZE = sizeof(isf_pid_session_requests) / sizeof(isf_pid_session_requests[0]);
const int ISF_UDS_REQUESTS_SIZE = sizeof(isf_uds_requests) / sizeof(isf_uds_requests[0]);

class IsfService
{
public:
    IsfService();
    ~IsfService();

    bool initialize();
    void listen();

private:
    bool initialize_diagnostic_session();
    bool beginSend();
    bool sendUdsRequest(uint8_t *udsMessage, uint8_t dataLength, const UDSRequest &request);
    bool processUdsResponse(uint8_t *data, uint8_t length, const UDSRequest &request);
    bool transformResponse(uint8_t *data, uint8_t length, const UDSRequest &request);

    // CAN bus interface for communication with ECUs
    MCP_CAN *mcp = nullptr;

    // ISO-TP protocol handler for multi-frame messaging
    IsoTp *isotp = nullptr;
    
    // Response buffer for UDS communications
    uint8_t udsResponseBuffer[MAX_MSGBUF];

    // Array tracking last request time for each UDS request
    unsigned long *lastUdsRequestTime = nullptr;
};
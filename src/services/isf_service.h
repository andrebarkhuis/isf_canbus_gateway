#ifndef _ISF_SERVICE_H
#define _ISF_SERVICE_H

#include <Arduino.h>
#include "../common.h"
#include "../can/twai_wrapper.h"
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

class TwaiWrapper;
class IsoTp;

#define DEBUG_ISF

//NB: Interval is set to 0 to disable the interval timer.
const CANMessage isf_pid_session_requests[] = {
    { .id = 0x700, .data = {0x02, UDS_SID_TESTER_PRESENT, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00}, .len = 8, .extended = false, .interval = 0, .param_name = "Tester present - Keep Alive" },
    { .id = 0x7E0, .data = {0x02, UDS_SID_TESTER_PRESENT, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00}, .len = 8, .extended = false, .interval = 0, .param_name = "Tester present - Keep Alive" }
};

const UDSRequest isf_uds_requests[] = {
    { .tx_id = 0x7E0, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x01, .interval = 100, .param_name = "request-1",  .length = 4, .payload = {0x21, 0x02, 0x21, 0x01 } },
    // { .tx_id = 0x7E0, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x03, .interval = 100, .param_name = "request-2",  .length = 3, .payload = { 0x02, 0x21, 0x03 } },
    // { .tx_id = 0x7E0, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x04, .interval = 100, .param_name = "request-3",  .length = 3, .payload = { 0x02, 0x21, 0x04 } },
    // { .tx_id = 0x7E0, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x05, .interval = 100, .param_name = "request-3",  .length = 3, .payload = { 0x02, 0x21, 0x05 } },
    // { .tx_id = 0x7E0, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x06, .interval = 100, .param_name = "request-4",  .length = 3, .payload = { 0x02, 0x21, 0x06 } },
    // { .tx_id = 0x7E0, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x07, .interval = 100, .param_name = "request-5",  .length = 3, .payload = { 0x02, 0x21, 0x07 } },
    // { .tx_id = 0x7E0, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x08, .interval = 100, .param_name = "request-6",  .length = 3, .payload = { 0x02, 0x21, 0x08 } },
    // { .tx_id = 0x7E0, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x22, .interval = 100, .param_name = "request-7",  .length = 3, .payload = { 0x02, 0x21, 0x22 } },
    // { .tx_id = 0x7E0, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x20, .interval = 100, .param_name = "request-7",  .length = 3, .payload = { 0x02, 0x21, 0x20 } },
    // { .tx_id = 0x7E0, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x25, .interval = 100, .param_name = "request-8",  .length = 3, .payload = { 0x02, 0x21, 0x25 } },
    // { .tx_id = 0x7E0, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x37, .interval = 100, .param_name = "request-9",  .length = 3, .payload = { 0x02, 0x21, 0x37 } },
    // { .tx_id = 0x7E0, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x51, .interval = 100, .param_name = "request-13", .length = 3, .payload = { 0x02, 0x21, 0x51 } },
    // { .tx_id = 0x7E0, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x82, .interval = 100, .param_name = "request-14", .length = 3, .payload = { 0x02, 0x21, 0x82 } },
    // { .tx_id = 0x7E0, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x83, .interval = 100, .param_name = "request-15", .length = 3, .payload = { 0x02, 0x21, 0x83 } },
    // { .tx_id = 0x7E0, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0xE1, .interval = 100, .param_name = "request-15", .length = 3, .payload = { 0x02, 0x21, 0xE1 } },
    // { .tx_id = 0x7E0, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0xC2, .interval = 150, .param_name = "engine-code", .length = 3, .payload = { 0x02, 0x21, 0xC2 } }
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
    bool sendUdsRequest(Message_t& msg, const UDSRequest &request);
    bool processUdsResponse(uint8_t *data, uint8_t length, const UDSRequest &request);
    bool transformResponse(uint8_t *data, uint8_t length, const UDSRequest &request);

    // CAN bus interface for communication with ECUs
    TwaiWrapper *twai = nullptr;

    // ISO-TP protocol handler for multi-frame messaging
    IsoTp *isotp = nullptr;
    
    // Flag to track if a UDS request is currently in progress
    bool is_session_active = false;
    
    // Response buffer for UDS communications
    uint8_t udsResponseBuffer[MAX_MSGBUF];

    // Array tracking last request time for each UDS request
    unsigned long *lastUdsRequestTime = nullptr;

    // Timestamp for the last diagnostic session initialization
    unsigned long last_diagnostic_session_time_ = 0;
};

#endif // _ISF_SERVICE_H
#ifndef _ISF_SERVICE_H
#define _ISF_SERVICE_H

#include <Arduino.h>
#include "../common.h"
#include "../can/twai_wrapper.h"
#include "../isotp/iso_tp.h"
#include <cstdint>
#include <string_view>
#include <optional>
#include <array>

class TwaiWrapper;
class IsoTp;

#define DEBUG_ISF



// Helper enum to tag the C++ type
enum class ValueType : uint8_t {
    Float,
    UInt16,
    UInt32,
    Boolean
};

struct UnitTypeInfo {
    uint8_t                 id;
    std::string             name;
    std::string             description;
    std::optional<float>    minValue;
    std::optional<float>    maxValue;
    ValueType               valueType;
};

// Array of all infos, in increasing Id order
inline const std::array<UnitTypeInfo, 32> unitTypeInfos{{
    { 0,   "GENERAL",            "Generic / ECU Identifiers",         std::nullopt,    std::nullopt,    ValueType::Boolean },
    { 1,   "ACCELERATION",       "Acceleration, Gradient",              -10.0f,          10.0f,           ValueType::Float },
    { 2,   "G_FORCE",            "G-force sensors",                     -5.0f,           5.0f,            ValueType::Float },
    { 3,   "ACCEL_REQUEST",      "Acceleration request signals",        0.0f,            100.0f,          ValueType::Float },
    { 4,   "DECELERATION",       "Deceleration sensor",                 -10.0f,          10.0f,           ValueType::Float },
    { 5,   "IGNITION_FEEDBACK",  "Ignition timing and feedback",        -20.0f,          60.0f,           ValueType::Float },
    { 6,   "ANGLE_SENSOR",       "Absolute angles (Steering, Pinion)",  -900.0f,         900.0f,          ValueType::Float },
    { 7,   "YAW_RATE",           "Yaw rate sensors",                    -200.0f,         200.0f,          ValueType::Float },
    { 9,   "CURRENT_SENSOR",     "Current draw sensors (Throttle, Clutch)", -50.0f, 50.0f,     ValueType::Float },
    { 11,  "PM_SENSOR",          "Particulate Matter Sensors",              0.0f,            1000.0f,         ValueType::Float },
    { 13,  "DISTANCE",           "Distance / Mileage / Odometer-related",   0.0f, 999999.0f,   ValueType::Float },
    { 14,  "FORWARD_DISTANCE",   "Forward vehicle distance measurement",    0.0f,    300.0f,       ValueType::Float },
    { 17,  "ODOMETER",           "Odometer history and mileage",            0.0f,            999999.0f,       ValueType::UInt32 },
    { 18,  "BATTERY_STATUS",     "Battery charge, hybrid systems",          0.0f,            100.0f,          ValueType::Float },
    { 19,  "POWER_MANAGEMENT",   "Request Power, Wout Control",             0.0f,            100.0f,          ValueType::Float },
    { 22,  "HYBRID_BATTERY",     "Hybrid / EV Battery power levels",        0.0f,            500.0f,          ValueType::Float },
    { 23,  "FUEL_SYSTEM",        "Fuel system-related parameters",          0.0f,            100.0f,          ValueType::Float },
    { 24,  "FUEL_INJECTION",     "Injection volume, fuel pump parameters",  0.0f, 200.0f,    ValueType::Float },
    { 25,  "CRUISE_CONTROL",     "Cruise control request forces",             0.0f,            100.0f,          ValueType::Float },
    { 29,  "FREQUENCY_SENSOR",   "Frequency-based sensors (Motor, Generator)", 0.0f, 5000.0f, ValueType::UInt16 },
    { 30,  "ILLUMINATION_SENSOR","Light control, brightness",         0.0f,            100000.0f,       ValueType::UInt32 },
    { 32,  "EXHAUST_SENSOR",     "NOx and exhaust emissions",         0.0f,            1000.0f,         ValueType::Float },
    { 33,  "LOAD_FUEL_TRIM",     "Load calculations, Fuel trims",     0.0f,            100.0f,          ValueType::Float },
    { 34,  "MAP_TIRE_PRESSURE",  "Manifold Pressure and Tire Inflation sensors", 10.0f, 400.0f, ValueType::Float },
    { 39,  "ENGINE_RPM",         "Engine Speed, RPM",                 0.0f,            10000.0f,        ValueType::UInt16 },
    { 42,  "SPEED_SENSOR",       "Vehicle Speed Sensors",             0.0f,            300.0f,          ValueType::UInt16 },
    { 48,  "VOLTAGE_SENSOR",     "Oxygen Sensor, Solar Voltage, Battery Voltage", 0.0f, 18.0f, ValueType::Float },
    { 57,  "TEMPERATURE_SENSOR", "Coolant Temperature, Intake Air Temp",    -40.0f, 150.0f,   ValueType::Float },
    { 58,  "TORQUE_SENSOR",      "Steering, Motor, Brake Torque",           -500.0f,         1000.0f,         ValueType::Float },
    { 59,  "POSITION_SENSOR",    "Throttle, Clutch, ASL Gear Position",     0.0f,   100.0f,        ValueType::Float },
    { 66,  "AMBIENT_TEMP",       "Outside temperature sensors",             -50.0f,          60.0f,           ValueType::Float },
    { 75,  "MASS_AIR_FLOW",      "MAF Sensors (filtered & raw values)",     0.0f,    655.0f,        ValueType::Float }
}};

//NB: Interval is set to 0 to disable the interval timer.
const CANMessage isf_pid_session_requests[] = {
        { .id = 0x7DF, .data = {0x02, UDS_SID_TESTER_PRESENT, OBD_MODE_SHOW_CURRENT_DATA, 0x00, 0x00, 0x00, 0x00, 0x00}, .len = 8, .extended = false, .interval = 0, .param_name = "Not sure" },
        // { .id = 0x7E0, .data = {0x02, UDS_SID_TESTER_PRESENT, OBD_MODE_SHOW_CURRENT_DATA, 0x00, 0x00, 0x00, 0x00, 0x00}, .len = 8, .extended = false, .interval = 0, .param_name = "Not sure" },
        // { .id = 0x7E1, .data = {0x02, UDS_SID_TESTER_PRESENT, OBD_MODE_SHOW_CURRENT_DATA, 0x00, 0x00, 0x00, 0x00, 0x00}, .len = 8, .extended = false, .interval = 0, .param_name = "Not sure" },
        // { .id = 0x7B0, .data = {0x02, UDS_SID_TESTER_PRESENT, OBD_MODE_SHOW_CURRENT_DATA, 0x00, 0x00, 0x00, 0x00, 0x00}, .len = 8, .extended = false, .interval = 0, .param_name = "Not sure" },
};

const int ABS_MODULE_REQUEST_ID = 0x7B0;
const int ABS_MODULE_RESPONSE_ID = 0x7B8;

const int ENGINE_MODULE_REQUEST_ID = 0x7E0;
const int ENGINE_MODULE_RESPONSE_ID = 0x7E8;

const int TCM_MODULE_REQUEST_ID = 0x7E1;
const int TCM_MODULE_RESPONSE_ID = 0x7E9;

const UDSRequest isf_uds_requests[] = {
        { .tx_id = 0x7B0, .rx_id = 0x7B8, .service_id = 0x21, .pid = 0, .did = 0x03, .interval = 100, .param_name = "request-0x03",  .length = 3, .payload = {0x02, 0x21, 0x03} },
        { .tx_id = 0x7B0, .rx_id = 0x7B8, .service_id = 0x21, .pid = 0, .did = 0x04, .interval = 100, .param_name = "request-0x04",  .length = 3, .payload = {0x02, 0x21, 0x04} },
        { .tx_id = 0x7B0, .rx_id = 0x7B8, .service_id = 0x21, .pid = 0, .did = 0x05, .interval = 100, .param_name = "request-0x05",  .length = 3, .payload = {0x02, 0x21, 0x05} },
        { .tx_id = 0x7B0, .rx_id = 0x7B8, .service_id = 0x21, .pid = 0, .did = 0x06, .interval = 100, .param_name = "request-0x06",  .length = 3, .payload = {0x02, 0x21, 0x06} },
        { .tx_id = 0x7B0, .rx_id = 0x7B8, .service_id = 0x21, .pid = 0, .did = 0x21, .interval = 100, .param_name = "request-0x21",  .length = 3, .payload = {0x02, 0x21, 0x21} },
        { .tx_id = 0x7B0, .rx_id = 0x7B8, .service_id = 0x21, .pid = 0, .did = 0x3C, .interval = 100, .param_name = "request-0x3C",  .length = 3, .payload = {0x02, 0x21, 0x3C} },
        { .tx_id = 0x7B0, .rx_id = 0x7B8, .service_id = 0x21, .pid = 0, .did = 0x41, .interval = 100, .param_name = "request-0x41",  .length = 3, .payload = {0x02, 0x21, 0x41} },
        { .tx_id = 0x7B0, .rx_id = 0x7B8, .service_id = 0x21, .pid = 0, .did = 0x85, .interval = 100, .param_name = "request-0x85",  .length = 3, .payload = {0x02, 0x21, 0x85} },
        { .tx_id = 0x7B0, .rx_id = 0x7B8, .service_id = 0x21, .pid = 0, .did = 0xE1, .interval = 100, .param_name = "request-0xE1",  .length = 3, .payload = {0x02, 0x21, 0xE1} },
        { .tx_id = 0x7E0, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x01, .interval = 100, .param_name = "request-0x01", .length = 3, .payload = {0x02, 0x21, 0x01} },
        { .tx_id = 0x7E0, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x04, .interval = 100, .param_name = "request-0x04", .length = 3, .payload = {0x02, 0x21, 0x04} },
        { .tx_id = 0x7E0, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x06, .interval = 100, .param_name = "request-0x06", .length = 3, .payload = {0x02, 0x21, 0x06} },
        { .tx_id = 0x7E0, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x25, .interval = 100, .param_name = "request-0x25", .length = 3, .payload = {0x02, 0x21, 0x25} },
        { .tx_id = 0x7E0, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x37, .interval = 100, .param_name = "request-0x37", .length = 3, .payload = {0x02, 0x21, 0x37} },
        { .tx_id = 0x7E0, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x39, .interval = 100, .param_name = "request-0x39", .length = 3, .payload = {0x02, 0x21, 0x39} },
        { .tx_id = 0x7E0, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x41, .interval = 100, .param_name = "request-0x41", .length = 3, .payload = {0x02, 0x21, 0x41} },
        { .tx_id = 0x7E0, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x51, .interval = 100, .param_name = "request-0x51", .length = 3, .payload = {0x02, 0x21, 0x51} },
        { .tx_id = 0x7E0, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x52, .interval = 100, .param_name = "request-0x52", .length = 3, .payload = {0x02, 0x21, 0x52} },
        { .tx_id = 0x7E0, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x82, .interval = 100, .param_name = "request-0x82", .length = 3, .payload = {0x02, 0x21, 0x82} },
        { .tx_id = 0x7E0, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x83, .interval = 100, .param_name = "request-0x83", .length = 3, .payload = {0x02, 0x21, 0x83} },
        { .tx_id = 0x7E0, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0x85, .interval = 100, .param_name = "request-0x85", .length = 3, .payload = {0x02, 0x21, 0x85} },
        { .tx_id = 0x7E0, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0xE1, .interval = 100, .param_name = "request-0xE1", .length = 3, .payload = {0x02, 0x21, 0xE1} },
        { .tx_id = 0x7E0, .rx_id = 0x7E8, .service_id = 0x21, .pid = 0, .did = 0xE3, .interval = 100, .param_name = "request-0xE3", .length = 3, .payload = {0x02, 0x21, 0xE3} },
        { .tx_id = 0x7E1, .rx_id = 0x7E9, .service_id = 0x21, .pid = 0, .did = 0x01, .interval = 100, .param_name = "request-0x01", .length = 3, .payload = {0x02, 0x21, 0x01} },
        { .tx_id = 0x7E1, .rx_id = 0x7E9, .service_id = 0x21, .pid = 0, .did = 0x06, .interval = 100, .param_name = "request-0x06", .length = 3, .payload = {0x02, 0x21, 0x06} },
        { .tx_id = 0x7E1, .rx_id = 0x7E9, .service_id = 0x21, .pid = 0, .did = 0x25, .interval = 100, .param_name = "request-0x25", .length = 3, .payload = {0x02, 0x21, 0x25} },
        { .tx_id = 0x7E1, .rx_id = 0x7E9, .service_id = 0x21, .pid = 0, .did = 0x82, .interval = 100, .param_name = "request-0x82", .length = 3, .payload = {0x02, 0x21, 0x82} },
        { .tx_id = 0x7E1, .rx_id = 0x7E9, .service_id = 0x21, .pid = 0, .did = 0x83, .interval = 100, .param_name = "request-0x83", .length = 3, .payload = {0x02, 0x21, 0x83} },
        { .tx_id = 0x7E1, .rx_id = 0x7E9, .service_id = 0x21, .pid = 0, .did = 0xE1, .interval = 100, .param_name = "request-0xE1", .length = 3, .payload = {0x02, 0x21, 0xE1} }
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
    bool processUdsResponse(Message_t& msg, const UDSRequest &request);
    bool transformResponse(Message_t& msg, const UDSRequest &request);

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
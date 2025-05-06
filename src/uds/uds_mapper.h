#pragma once

#include <string>
#include <unordered_map>
#include <tuple>
#include <optional>

struct UdsDefinition {
    uint16_t obd2_request_id_hex;
    uint16_t parameter_id_hex;
    uint16_t uds_data_identifier_hex;
    bool is_signed;
    int unit_type;
    int byte_position;
    int bit_offset_position;
    double scaling_factor;
    double offset_value;
    int bit_length;
    std::string parameter_name;
    std::optional<double> parameter_value;
    std::optional<std::string> parameter_display_value;
};

struct SignalValue {
    uint16_t parameter_id;
    std::string parameter_name;
    double value;
    std::optional<std::string> display_value;
    std::optional<std::string> unit;

    // Constructor (without parameter_id, if you don’t need it)
    SignalValue(const std::string& name, double val,
                std::optional<std::string> display = std::nullopt,
                std::optional<std::string> unit_str = std::nullopt)
        : parameter_name(name), value(val), display_value(display), unit(unit_str) {}
};

using udsDefinition_key = std::tuple<uint16_t, uint16_t>;  // request_id and data_id

struct udsDefinition_key_hash {
    std::size_t operator()(const udsDefinition_key& key) const {
        auto h1 = std::hash<uint16_t>{}(std::get<0>(key));  // request_id
        auto h2 = std::hash<uint16_t>{}(std::get<1>(key));  // data_id
        return h1 ^ (h2 << 1);
    }
};

inline std::unordered_multimap<udsDefinition_key, UdsDefinition, udsDefinition_key_hash> udsMap;

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

inline void init_udsDefinitions() {
    udsMap.insert({ { 0x7E0, 0x1 }, { 0x7E0, 0x9, 0x1, false, 39, 10, 0, 0.25, 0.0, 4, "Engine Speed", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x1 }, { 0x7E0, 0xA, 0x1, false, 42, 12, 0, 1.0, 0.0, 4, "Vehicle Speed", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x17, 0x3, false, 0, 0, 0, 1.0, 0.0, 4, "Fuel System Status #1", std::make_optional(0.0), std::make_optional(std::string("Unused")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x17, 0x3, false, 0, 0, 0, 1.0, 0.0, 4, "Fuel System Status #1", std::make_optional(1.0), std::make_optional(std::string("OL")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x17, 0x3, false, 0, 0, 0, 1.0, 0.0, 4, "Fuel System Status #1", std::make_optional(2.0), std::make_optional(std::string("CL")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x17, 0x3, false, 0, 0, 0, 1.0, 0.0, 4, "Fuel System Status #1", std::make_optional(4.0), std::make_optional(std::string("OLDrive")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x17, 0x3, false, 0, 0, 0, 1.0, 0.0, 4, "Fuel System Status #1", std::make_optional(8.0), std::make_optional(std::string("OLFault")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x17, 0x3, false, 0, 0, 0, 1.0, 0.0, 4, "Fuel System Status #1", std::make_optional(16.0), std::make_optional(std::string("CLFault")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x18, 0x3, false, 0, 1, 0, 1.0, 0.0, 4, "Fuel System Status #2", std::make_optional(0.0), std::make_optional(std::string("Unused")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x18, 0x3, false, 0, 1, 0, 1.0, 0.0, 4, "Fuel System Status #2", std::make_optional(1.0), std::make_optional(std::string("OL")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x18, 0x3, false, 0, 1, 0, 1.0, 0.0, 4, "Fuel System Status #2", std::make_optional(2.0), std::make_optional(std::string("CL")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x18, 0x3, false, 0, 1, 0, 1.0, 0.0, 4, "Fuel System Status #2", std::make_optional(4.0), std::make_optional(std::string("OLDrive")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x18, 0x3, false, 0, 1, 0, 1.0, 0.0, 4, "Fuel System Status #2", std::make_optional(8.0), std::make_optional(std::string("OLFault")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x18, 0x3, false, 0, 1, 0, 1.0, 0.0, 4, "Fuel System Status #2", std::make_optional(16.0), std::make_optional(std::string("CLFault")) } });
    udsMap.insert({ { 0xC7, 0x2B }, { 0xC7, 0x1D, 0x2B, false, 0, 2, 0, 1.0, 0.0, 4, "Remote A/C by Smart Key", std::make_optional(0.0), std::make_optional(std::string("NOT Avail")) } });
    udsMap.insert({ { 0xC7, 0x2B }, { 0xC7, 0x1D, 0x2B, false, 0, 2, 0, 1.0, 0.0, 4, "Remote A/C by Smart Key", std::make_optional(1.0), std::make_optional(std::string("Available")) } });
    udsMap.insert({ { 0x7E0, 0x4 }, { 0x7E0, 0x21, 0x4, false, 0, 0, 0, 3.05e-05, 0.0, 4, "Target Air-Fuel Ratio", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x5 }, { 0x7E0, 0x32, 0x5, false, 34, 0, 0, 10.0, 0.0, 4, "Fuel Press", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x6 }, { 0x7E0, 0x37, 0x6, false, 0, 0, 7, 1.0, 0.0, 4, "MIL", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x6 }, { 0x7E0, 0x37, 0x6, false, 0, 0, 7, 1.0, 0.0, 4, "MIL", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x6 }, { 0x7E0, 0x3A, 0x6, false, 0, 1, 1, 1.0, 0.0, 4, "Fuel System Monitor", std::make_optional(0.0), std::make_optional(std::string("Not Avl")) } });
    udsMap.insert({ { 0x7E0, 0x6 }, { 0x7E0, 0x3A, 0x6, false, 0, 1, 1, 1.0, 0.0, 4, "Fuel System Monitor", std::make_optional(1.0), std::make_optional(std::string("Avail")) } });
    udsMap.insert({ { 0x7E0, 0x6 }, { 0x7E0, 0x3F, 0x6, false, 0, 2, 4, 1.0, 0.0, 4, "A/C Monitor", std::make_optional(0.0), std::make_optional(std::string("Not Avl")) } });
    udsMap.insert({ { 0x7E0, 0x6 }, { 0x7E0, 0x3F, 0x6, false, 0, 2, 4, 1.0, 0.0, 4, "A/C Monitor", std::make_optional(1.0), std::make_optional(std::string("Avail")) } });
    udsMap.insert({ { 0x7E0, 0x6 }, { 0x7E0, 0x47, 0x6, false, 0, 3, 4, 1.0, 0.0, 4, "A/C Monitor", std::make_optional(0.0), std::make_optional(std::string("Compl")) } });
    udsMap.insert({ { 0x7E0, 0x6 }, { 0x7E0, 0x47, 0x6, false, 0, 3, 4, 1.0, 0.0, 4, "A/C Monitor", std::make_optional(1.0), std::make_optional(std::string("Incmpl")) } });
    udsMap.insert({ { 0x7E0, 0x6 }, { 0x7E0, 0x4D, 0x6, false, 13, 6, 0, 1.0, 0.0, 4, "MIL ON Run Distance", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x6 }, { 0x7E0, 0x4F, 0x6, false, 0, 9, 5, 1.0, 0.0, 4, "Fuel System Monitor CMPL", std::make_optional(0.0), std::make_optional(std::string("Compl")) } });
    udsMap.insert({ { 0x7E0, 0x6 }, { 0x7E0, 0x4F, 0x6, false, 0, 9, 5, 1.0, 0.0, 4, "Fuel System Monitor CMPL", std::make_optional(1.0), std::make_optional(std::string("Incmpl")) } });
    udsMap.insert({ { 0x7E0, 0x6 }, { 0x7E0, 0x52, 0x6, false, 0, 9, 1, 1.0, 0.0, 4, "Fuel System Monitor ENA", std::make_optional(0.0), std::make_optional(std::string("Unable")) } });
    udsMap.insert({ { 0x7E0, 0x6 }, { 0x7E0, 0x52, 0x6, false, 0, 9, 1, 1.0, 0.0, 4, "Fuel System Monitor ENA", std::make_optional(1.0), std::make_optional(std::string("Enable")) } });
    udsMap.insert({ { 0x7E0, 0x6 }, { 0x7E0, 0x57, 0x6, false, 0, 10, 4, 1.0, 0.0, 4, "A/C Monitor ENA", std::make_optional(0.0), std::make_optional(std::string("Unable")) } });
    udsMap.insert({ { 0x7E0, 0x6 }, { 0x7E0, 0x57, 0x6, false, 0, 10, 4, 1.0, 0.0, 4, "A/C Monitor ENA", std::make_optional(1.0), std::make_optional(std::string("Enable")) } });
    udsMap.insert({ { 0x7E0, 0x6 }, { 0x7E0, 0x5F, 0x6, false, 0, 11, 4, 1.0, 0.0, 4, "A/C Monitor CMPL", std::make_optional(0.0), std::make_optional(std::string("Compl")) } });
    udsMap.insert({ { 0x7E0, 0x6 }, { 0x7E0, 0x5F, 0x6, false, 0, 11, 4, 1.0, 0.0, 4, "A/C Monitor CMPL", std::make_optional(1.0), std::make_optional(std::string("Incmpl")) } });
    udsMap.insert({ { 0x7E0, 0x6 }, { 0x7E0, 0x64, 0x6, false, 53, 12, 0, 1.0, 0.0, 4, "Running Time from MIL ON", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x22 }, { 0x7E0, 0x6B, 0x22, false, 0, 8, 0, 1.0, 0.0, 4, "Received MIL from ECT", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x22 }, { 0x7E0, 0x6B, 0x22, false, 0, 8, 0, 1.0, 0.0, 4, "Received MIL from ECT", std::make_optional(90.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x22 }, { 0x7E0, 0x6D, 0x22, false, 0, 11, 0, 1.0, 0.0, 4, "Shift Position Sig from ECT", std::make_optional(0.0), std::make_optional(std::string("1st")) } });
    udsMap.insert({ { 0x7E0, 0x22 }, { 0x7E0, 0x6D, 0x22, false, 0, 11, 0, 1.0, 0.0, 4, "Shift Position Sig from ECT", std::make_optional(1.0), std::make_optional(std::string("2nd")) } });
    udsMap.insert({ { 0x7E0, 0x22 }, { 0x7E0, 0x6D, 0x22, false, 0, 11, 0, 1.0, 0.0, 4, "Shift Position Sig from ECT", std::make_optional(2.0), std::make_optional(std::string("3rd")) } });
    udsMap.insert({ { 0x7E0, 0x22 }, { 0x7E0, 0x6D, 0x22, false, 0, 11, 0, 1.0, 0.0, 4, "Shift Position Sig from ECT", std::make_optional(3.0), std::make_optional(std::string("4th")) } });
    udsMap.insert({ { 0x7E0, 0x22 }, { 0x7E0, 0x6D, 0x22, false, 0, 11, 0, 1.0, 0.0, 4, "Shift Position Sig from ECT", std::make_optional(4.0), std::make_optional(std::string("5th")) } });
    udsMap.insert({ { 0x7E0, 0x22 }, { 0x7E0, 0x6D, 0x22, false, 0, 11, 0, 1.0, 0.0, 4, "Shift Position Sig from ECT", std::make_optional(5.0), std::make_optional(std::string("6th")) } });
    udsMap.insert({ { 0x7E0, 0x22 }, { 0x7E0, 0x6D, 0x22, false, 0, 11, 0, 1.0, 0.0, 4, "Shift Position Sig from ECT", std::make_optional(6.0), std::make_optional(std::string("7th")) } });
    udsMap.insert({ { 0x7E0, 0x22 }, { 0x7E0, 0x6D, 0x22, false, 0, 11, 0, 1.0, 0.0, 4, "Shift Position Sig from ECT", std::make_optional(7.0), std::make_optional(std::string("8th")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x78, 0x25, false, 0, 1, 2, 1.0, 0.0, 4, "Fuel Lid", std::make_optional(0.0), std::make_optional(std::string("Close")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x78, 0x25, false, 0, 1, 2, 1.0, 0.0, 4, "Fuel Lid", std::make_optional(1.0), std::make_optional(std::string("Open")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x79, 0x25, false, 0, 1, 1, 1.0, 0.0, 4, "Fuel Lid Switch", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x79, 0x25, false, 0, 1, 1, 1.0, 0.0, 4, "Fuel Lid Switch", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x7D, 0x25, false, 0, 4, 7, 1.0, 0.0, 4, "Shift SW Status (P Range)", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x7D, 0x25, false, 0, 4, 7, 1.0, 0.0, 4, "Shift SW Status (P Range)", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x7E, 0x25, false, 0, 4, 6, 1.0, 0.0, 4, "Shift SW Status (R Range)", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x7E, 0x25, false, 0, 4, 6, 1.0, 0.0, 4, "Shift SW Status (R Range)", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x7F, 0x25, false, 0, 4, 5, 1.0, 0.0, 4, "Shift SW Status (N Range)", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x7F, 0x25, false, 0, 4, 5, 1.0, 0.0, 4, "Shift SW Status (N Range)", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x80, 0x25, false, 0, 4, 4, 1.0, 0.0, 4, "Shift SW Status (D Range)", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x80, 0x25, false, 0, 4, 4, 1.0, 0.0, 4, "Shift SW Status (D Range)", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x81, 0x25, false, 0, 4, 3, 1.0, 0.0, 4, "Shift SW Status (4 Range)", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x81, 0x25, false, 0, 4, 3, 1.0, 0.0, 4, "Shift SW Status (4 Range)", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x82, 0x25, false, 0, 4, 2, 1.0, 0.0, 4, "Shift SW Status (3 Range)", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x82, 0x25, false, 0, 4, 2, 1.0, 0.0, 4, "Shift SW Status (3 Range)", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x83, 0x25, false, 0, 4, 1, 1.0, 0.0, 4, "Shift SW Status (2 Range)", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x83, 0x25, false, 0, 4, 1, 1.0, 0.0, 4, "Shift SW Status (2 Range)", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x84, 0x25, false, 0, 4, 0, 1.0, 0.0, 4, "Shift SW Status (L Range)", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x84, 0x25, false, 0, 4, 0, 1.0, 0.0, 4, "Shift SW Status (L Range)", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x85, 0x25, false, 0, 5, 7, 1.0, 0.0, 4, "Sports Mode Selection SW", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x85, 0x25, false, 0, 5, 7, 1.0, 0.0, 4, "Sports Mode Selection SW", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x86, 0x25, false, 0, 5, 6, 1.0, 0.0, 4, "Sports Shift Up SW", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x86, 0x25, false, 0, 5, 6, 1.0, 0.0, 4, "Sports Shift Up SW", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x87, 0x25, false, 0, 5, 5, 1.0, 0.0, 4, "Sports Shift Down SW", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x87, 0x25, false, 0, 5, 5, 1.0, 0.0, 4, "Sports Shift Down SW", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x88, 0x25, false, 0, 5, 3, 1.0, 0.0, 4, "Shift SW Status (B Range)", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x88, 0x25, false, 0, 5, 3, 1.0, 0.0, 4, "Shift SW Status (B Range)", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x8C, 0x25, false, 0, 6, 4, 1.0, 0.0, 4, "Snow or 2nd Start Mode", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x8C, 0x25, false, 0, 6, 4, 1.0, 0.0, 4, "Snow or 2nd Start Mode", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x96, 0x25, false, 0, 10, 7, 1.0, 0.0, 4, "Shift Indication Enable", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x96, 0x25, false, 0, 10, 7, 1.0, 0.0, 4, "Shift Indication Enable", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x98, 0x25, false, 0, 11, 7, 1.0, 0.0, 4, "A/C Signal", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x98, 0x25, false, 0, 11, 7, 1.0, 0.0, 4, "A/C Signal", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x33 }, { 0x7E0, 0xB5, 0x33, false, 34, 26, 0, 1.0, 0.0, 4, "Air Pump Pressure (Absolute)", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x33 }, { 0x7E0, 0xB6, 0x33, false, 34, 28, 0, 1.0, 0.0, 4, "Air Pump2 Pressure (Absolute)", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x37 }, { 0x7E0, 0xBA, 0x37, false, 57, 0, 0, 0.625, -40.0, 4, "Initial Engine Coolant Temp", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x37 }, { 0x7E0, 0xBF, 0x37, false, 60, 8, 0, 0.03125, -1024.0, 4, "Knock Correct Learn Value", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x37 }, { 0x7E0, 0xC0, 0x37, false, 60, 10, 0, 0.03125, -1024.0, 4, "Knock Feedback Value", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x37 }, { 0x7E0, 0xC8, 0x37, false, 0, 21, 6, 1.0, 0.0, 4, "Fuel Cut Condition", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x37 }, { 0x7E0, 0xC8, 0x37, false, 0, 21, 6, 1.0, 0.0, 4, "Fuel Cut Condition", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x37 }, { 0x7E0, 0xCC, 0x37, false, 0, 21, 1, 1.0, 0.0, 4, "Idle Fuel Cut", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x37 }, { 0x7E0, 0xCC, 0x37, false, 0, 21, 1, 1.0, 0.0, 4, "Idle Fuel Cut", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x39 }, { 0x7E0, 0xD1, 0x39, false, 0, 0, 5, 1.0, 0.0, 4, "Fuel Pump Speed Control", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x39 }, { 0x7E0, 0xD1, 0x39, false, 0, 0, 5, 1.0, 0.0, 4, "Fuel Pump Speed Control", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x39 }, { 0x7E0, 0xD4, 0x39, false, 0, 0, 2, 1.0, 0.0, 4, "Fuel Pressure Up VSV", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x39 }, { 0x7E0, 0xD4, 0x39, false, 0, 0, 2, 1.0, 0.0, 4, "Fuel Pressure Up VSV", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x39 }, { 0x7E0, 0xDA, 0x39, false, 0, 1, 3, 1.0, 0.0, 4, "A/C Magnetic Clutch Relay", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x39 }, { 0x7E0, 0xDA, 0x39, false, 0, 1, 3, 1.0, 0.0, 4, "A/C Magnetic Clutch Relay", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x39 }, { 0x7E0, 0xDC, 0x39, false, 0, 1, 1, 1.0, 0.0, 4, "Fuel Pump/Speed Status", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x39 }, { 0x7E0, 0xDC, 0x39, false, 0, 1, 1, 1.0, 0.0, 4, "Fuel Pump/Speed Status", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x39 }, { 0x7E0, 0xE0, 0x39, false, 0, 3, 3, 1.0, 0.0, 4, "Fuel Shutoff Valve for Delivery Pipe", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x39 }, { 0x7E0, 0xE0, 0x39, false, 0, 3, 3, 1.0, 0.0, 4, "Fuel Shutoff Valve for Delivery Pipe", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x39 }, { 0x7E0, 0xE3, 0x39, false, 0, 3, 0, 1.0, 0.0, 4, "Idle Fuel Cut Prohibit", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x39 }, { 0x7E0, 0xE3, 0x39, false, 0, 3, 0, 1.0, 0.0, 4, "Idle Fuel Cut Prohibit", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x39 }, { 0x7E0, 0xE6, 0x39, false, 0, 4, 5, 1.0, 0.0, 4, "Electric Fan Motor", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x39 }, { 0x7E0, 0xE6, 0x39, false, 0, 4, 5, 1.0, 0.0, 4, "Electric Fan Motor", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x39 }, { 0x7E0, 0xEC, 0x39, false, 0, 5, 3, 1.0, 0.0, 4, "Fuel Route Switching Valve", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x39 }, { 0x7E0, 0xEC, 0x39, false, 0, 5, 3, 1.0, 0.0, 4, "Fuel Route Switching Valve", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x39 }, { 0x7E0, 0xED, 0x39, false, 0, 5, 2, 1.0, 0.0, 4, "Intank Fuel Pump", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x39 }, { 0x7E0, 0xED, 0x39, false, 0, 5, 2, 1.0, 0.0, 4, "Intank Fuel Pump", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x39 }, { 0x7E0, 0xF0, 0x39, false, 0, 8, 4, 1.0, 0.0, 4, "Fuel Filler Opener", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x39 }, { 0x7E0, 0xF0, 0x39, false, 0, 8, 4, 1.0, 0.0, 4, "Fuel Filler Opener", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x39 }, { 0x7E0, 0xF1, 0x39, false, 0, 8, 3, 1.0, 0.0, 4, "Fuel Vapor-Containment Valve", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x39 }, { 0x7E0, 0xF1, 0x39, false, 0, 8, 3, 1.0, 0.0, 4, "Fuel Vapor-Containment Valve", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x3C }, { 0x7E0, 0x10A, 0x3C, false, 33, 10, 0, 0.5, 0.0, 4, "Fuel Pump Duty (D4)", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x3C }, { 0x7E0, 0x10D, 0x3C, false, 36, 16, 0, 0.001, 0.0, 4, "Fuel Pressure Target Value", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x49 }, { 0x7E0, 0x171, 0x49, false, 0, 11, 1, 1.0, 0.0, 4, "Fuel Level", std::make_optional(0.0), std::make_optional(std::string("Empty")) } });
    udsMap.insert({ { 0x7E0, 0x49 }, { 0x7E0, 0x171, 0x49, false, 0, 11, 1, 1.0, 0.0, 4, "Fuel Level", std::make_optional(1.0), std::make_optional(std::string("Not Emp")) } });
    udsMap.insert({ { 0x7E0, 0x51 }, { 0x7E0, 0x176, 0x51, false, 62, 30, 0, 0.15625, 0.0, 4, "A/C Duty Feedback Value", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x82 }, { 0x7E0, 0x196, 0x82, false, 57, 0, 0, 0.00390625, -40.0, 4, "A/T Oil Temperature 1", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x82 }, { 0x7E0, 0x197, 0x82, false, 57, 2, 0, 0.00390625, -40.0, 4, "A/T Oil Temperature 2", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x82 }, { 0x7E0, 0x198, 0x82, false, 57, 4, 0, 0.00390625, -40.0, 4, "A/T Oil Temperature 3", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x85 }, { 0x7E0, 0x1A0, 0x85, false, 0, 0, 0, 1.0, 0.0, 4, "Shift Status", std::make_optional(1.0), std::make_optional(std::string("1st")) } });
    udsMap.insert({ { 0x7E0, 0x85 }, { 0x7E0, 0x1A0, 0x85, false, 0, 0, 0, 1.0, 0.0, 4, "Shift Status", std::make_optional(2.0), std::make_optional(std::string("2nd")) } });
    udsMap.insert({ { 0x7E0, 0x85 }, { 0x7E0, 0x1A0, 0x85, false, 0, 0, 0, 1.0, 0.0, 4, "Shift Status", std::make_optional(3.0), std::make_optional(std::string("3rd")) } });
    udsMap.insert({ { 0x7E0, 0x85 }, { 0x7E0, 0x1A0, 0x85, false, 0, 0, 0, 1.0, 0.0, 4, "Shift Status", std::make_optional(4.0), std::make_optional(std::string("4th")) } });
    udsMap.insert({ { 0x7E0, 0x85 }, { 0x7E0, 0x1A0, 0x85, false, 0, 0, 0, 1.0, 0.0, 4, "Shift Status", std::make_optional(5.0), std::make_optional(std::string("5th")) } });
    udsMap.insert({ { 0x7E0, 0x85 }, { 0x7E0, 0x1A0, 0x85, false, 0, 0, 0, 1.0, 0.0, 4, "Shift Status", std::make_optional(6.0), std::make_optional(std::string("6th")) } });
    udsMap.insert({ { 0x7E0, 0x85 }, { 0x7E0, 0x1A0, 0x85, false, 0, 0, 0, 1.0, 0.0, 4, "Shift Status", std::make_optional(7.0), std::make_optional(std::string("7th")) } });
    udsMap.insert({ { 0x7E0, 0x85 }, { 0x7E0, 0x1A0, 0x85, false, 0, 0, 0, 1.0, 0.0, 4, "Shift Status", std::make_optional(8.0), std::make_optional(std::string("8th")) } });
    udsMap.insert({ { 0x7E0, 0x3C }, { 0x7E0, 0x1DB, 0x3C, false, 33, 11, 0, 0.5, 0.0, 4, "Fuel Pump2 Duty (D4)", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x39 }, { 0x7E0, 0x1DF, 0x39, false, 0, 5, 4, 1.0, 0.0, 4, "Fuel Press Switching Valve", std::make_optional(0.0), std::make_optional(std::string("Low")) } });
    udsMap.insert({ { 0x7E0, 0x39 }, { 0x7E0, 0x1DF, 0x39, false, 0, 5, 4, 1.0, 0.0, 4, "Fuel Press Switching Valve", std::make_optional(1.0), std::make_optional(std::string("High")) } });
    udsMap.insert({ { 0x7E0, 0x51 }, { 0x7E0, 0x1E7, 0x51, false, 57, 9, 0, 1.0, -40.0, 4, "Engine Oil Temperature", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x51 }, { 0x7E0, 0x1E9, 0x51, false, 57, 11, 0, 1.0, -40.0, 4, "Ambient Temp for A/C", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x51 }, { 0x7E0, 0x1F9, 0x51, false, 0, 36, 7, 1.0, 0.0, 4, "Immobiliser Fuel Cut", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x51 }, { 0x7E0, 0x1F9, 0x51, false, 0, 36, 7, 1.0, 0.0, 4, "Immobiliser Fuel Cut", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x20E, 0x25, false, 0, 6, 2, 1.0, 0.0, 4, "Snow Switch Status", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x20E, 0x25, false, 0, 6, 2, 1.0, 0.0, 4, "Snow Switch Status", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x20F, 0x25, false, 0, 10, 3, 1.0, 0.0, 4, "A/C Pressure Normal SW", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x20F, 0x25, false, 0, 10, 3, 1.0, 0.0, 4, "A/C Pressure Normal SW", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x210, 0x25, false, 0, 10, 2, 1.0, 0.0, 4, "A/C Pressure Abnormal SW", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x210, 0x25, false, 0, 10, 2, 1.0, 0.0, 4, "A/C Pressure Abnormal SW", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x39 }, { 0x7E0, 0x21C, 0x39, false, 0, 6, 0, 1.0, 0.0, 4, "Sub Fuel Tank VSV", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x39 }, { 0x7E0, 0x21C, 0x39, false, 0, 6, 0, 1.0, 0.0, 4, "Sub Fuel Tank VSV", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x3C }, { 0x7E0, 0x220, 0x3C, false, 24, 31, 0, 0.01953125, 0.0, 4, "Fuel Dilution Estimate", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x52 }, { 0x7E0, 0x221, 0x52, false, 52, 18, 0, 1.049, 0.0, 4, "Fuel Cut Elps Time", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x83 }, { 0x7E0, 0x222, 0x83, false, 0, 0, 2, 1.0, 0.0, 4, "Shift Control Mode", std::make_optional(0.0), std::make_optional(std::string("S-mode")) } });
    udsMap.insert({ { 0x7E0, 0x83 }, { 0x7E0, 0x222, 0x83, false, 0, 0, 2, 1.0, 0.0, 4, "Shift Control Mode", std::make_optional(1.0), std::make_optional(std::string("M-mode")) } });
    udsMap.insert({ { 0x7E0, 0x51 }, { 0x7E0, 0x243, 0x51, false, 0, 36, 6, 1.0, 0.0, 4, "Immobiliser Fuel Cut History", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x51 }, { 0x7E0, 0x243, 0x51, false, 0, 36, 6, 1.0, 0.0, 4, "Immobiliser Fuel Cut History", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x33 }, { 0x7E0, 0x286, 0x33, false, 34, 32, 0, 0.01, 0.0, 4, "DPR/DPNR Absolute Pressure", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x39 }, { 0x7E0, 0x292, 0x39, false, 0, 3, 6, 1.0, 0.0, 4, "Fuel Pressure Status Stratification", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x39 }, { 0x7E0, 0x292, 0x39, false, 0, 3, 6, 1.0, 0.0, 4, "Fuel Pressure Status Stratification", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x39 }, { 0x7E0, 0x293, 0x39, false, 0, 3, 5, 1.0, 0.0, 4, "Fuel Pressure Status Stoichiometric", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x39 }, { 0x7E0, 0x293, 0x39, false, 0, 3, 5, 1.0, 0.0, 4, "Fuel Pressure Status Stoichiometric", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x1 }, { 0x7E0, 0x2C6, 0x1, false, 57, 49, 0, 1.0, -40.0, 4, "Engine Oil Temperature Sensor", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x2C7, 0x3, false, 0, 12, 0, 1.0, 0.0, 4, "Current Fuel Type", std::make_optional(1.0), std::make_optional(std::string("Gasoline/petrol")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x2C7, 0x3, false, 0, 12, 0, 1.0, 0.0, 4, "Current Fuel Type", std::make_optional(2.0), std::make_optional(std::string("Methanol")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x2C7, 0x3, false, 0, 12, 0, 1.0, 0.0, 4, "Current Fuel Type", std::make_optional(3.0), std::make_optional(std::string("Ethanol")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x2C7, 0x3, false, 0, 12, 0, 1.0, 0.0, 4, "Current Fuel Type", std::make_optional(4.0), std::make_optional(std::string("Diesel")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x2C7, 0x3, false, 0, 12, 0, 1.0, 0.0, 4, "Current Fuel Type", std::make_optional(5.0), std::make_optional(std::string("Liquefied Petroleum Gas (LPG) LPG")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x2C7, 0x3, false, 0, 12, 0, 1.0, 0.0, 4, "Current Fuel Type", std::make_optional(6.0), std::make_optional(std::string("Compressed Natural Gas (CNG) CNG")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x2C7, 0x3, false, 0, 12, 0, 1.0, 0.0, 4, "Current Fuel Type", std::make_optional(7.0), std::make_optional(std::string("Propane")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x2C7, 0x3, false, 0, 12, 0, 1.0, 0.0, 4, "Current Fuel Type", std::make_optional(8.0), std::make_optional(std::string("Battery/electric")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x2C7, 0x3, false, 0, 12, 0, 1.0, 0.0, 4, "Current Fuel Type", std::make_optional(9.0), std::make_optional(std::string("Bi-fuel vehicle using gasoline")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x2C7, 0x3, false, 0, 12, 0, 1.0, 0.0, 4, "Current Fuel Type", std::make_optional(10.0), std::make_optional(std::string("Bi-fuel vehicle using methanol")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x2C7, 0x3, false, 0, 12, 0, 1.0, 0.0, 4, "Current Fuel Type", std::make_optional(11.0), std::make_optional(std::string("Bi-fuel vehicle using ethanol")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x2C7, 0x3, false, 0, 12, 0, 1.0, 0.0, 4, "Current Fuel Type", std::make_optional(12.0), std::make_optional(std::string("Bi-fuel vehicle using LPG")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x2C7, 0x3, false, 0, 12, 0, 1.0, 0.0, 4, "Current Fuel Type", std::make_optional(13.0), std::make_optional(std::string("Bi-fuel vehicle using CNG")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x2C7, 0x3, false, 0, 12, 0, 1.0, 0.0, 4, "Current Fuel Type", std::make_optional(14.0), std::make_optional(std::string("Bi-fuel vehicle using propane")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x2C7, 0x3, false, 0, 12, 0, 1.0, 0.0, 4, "Current Fuel Type", std::make_optional(15.0), std::make_optional(std::string("Bi-fuel vehicle using battery")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x2C7, 0x3, false, 0, 12, 0, 1.0, 0.0, 4, "Current Fuel Type", std::make_optional(16.0), std::make_optional(std::string("Bi-fuel vehicle using battery and combustion engine")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x2C7, 0x3, false, 0, 12, 0, 1.0, 0.0, 4, "Current Fuel Type", std::make_optional(17.0), std::make_optional(std::string("Hybrid vehicle using gasoline engine")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x2C7, 0x3, false, 0, 12, 0, 1.0, 0.0, 4, "Current Fuel Type", std::make_optional(18.0), std::make_optional(std::string("Hybrid vehicle using gasoline engine on ethanol")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x2C7, 0x3, false, 0, 12, 0, 1.0, 0.0, 4, "Current Fuel Type", std::make_optional(19.0), std::make_optional(std::string("Hybrid vehicle using diesel engine")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x2C7, 0x3, false, 0, 12, 0, 1.0, 0.0, 4, "Current Fuel Type", std::make_optional(20.0), std::make_optional(std::string("Hybrid vehicle using battery")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x2C7, 0x3, false, 0, 12, 0, 1.0, 0.0, 4, "Current Fuel Type", std::make_optional(21.0), std::make_optional(std::string("Hybrid vehicle using battery and combustion engine")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x2C7, 0x3, false, 0, 12, 0, 1.0, 0.0, 4, "Current Fuel Type", std::make_optional(22.0), std::make_optional(std::string("Hybrid vehicle in regeneration mode")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x2C7, 0x3, false, 0, 12, 0, 1.0, 0.0, 4, "Current Fuel Type", std::make_optional(24.0), std::make_optional(std::string("Bi-fuel vehicle using Natural Gas")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x2C7, 0x3, false, 0, 12, 0, 1.0, 0.0, 4, "Current Fuel Type", std::make_optional(25.0), std::make_optional(std::string("Bi-fuel vehicle using diesel")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x2C7, 0x3, false, 0, 12, 0, 1.0, 0.0, 4, "Current Fuel Type", std::make_optional(26.0), std::make_optional(std::string("Natural Gas (Compressed or Liquefied Natural Gas)")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x2C7, 0x3, false, 0, 12, 0, 1.0, 0.0, 4, "Current Fuel Type", std::make_optional(27.0), std::make_optional(std::string("Dual Fuel - Diesel and CNG")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x2C7, 0x3, false, 0, 12, 0, 1.0, 0.0, 4, "Current Fuel Type", std::make_optional(28.0), std::make_optional(std::string("Dual Fuel - Diesel and LNG")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x2C7, 0x3, false, 0, 12, 0, 1.0, 0.0, 4, "Current Fuel Type", std::make_optional(29.0), std::make_optional(std::string("Fuel Cell Utilizing Hydrogen")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x2C7, 0x3, false, 0, 12, 0, 1.0, 0.0, 4, "Current Fuel Type", std::make_optional(30.0), std::make_optional(std::string("Hydrogen Internal Combustion Engine")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x2C9, 0x25, false, 0, 9, 6, 1.0, 0.0, 4, "Sports Mode Switch", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x2C9, 0x25, false, 0, 9, 6, 1.0, 0.0, 4, "Sports Mode Switch", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x2C9, 0x25, false, 0, 9, 6, 1.0, 0.0, 4, "Sports Mode Switch", std::make_optional(2.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x2C9, 0x25, false, 0, 9, 6, 1.0, 0.0, 4, "Sports Mode Switch", std::make_optional(3.0), std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x3A }, { 0x7E0, 0x2D8, 0x3A, false, 33, 32, 0, 0.006103515625, 0.0, 4, "Fuel Pump Duty", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x51 }, { 0x7E0, 0x2D9, 0x51, false, 0, 36, 5, 1.0, 0.0, 4, "Fuel Cut Bank 2 for Idle", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x51 }, { 0x7E0, 0x2D9, 0x51, false, 0, 36, 5, 1.0, 0.0, 4, "Fuel Cut Bank 2 for Idle", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x51 }, { 0x7E0, 0x2DA, 0x51, false, 0, 37, 0, 1.0, 0.0, 4, "Fuel Cut Info Bank 2 for Idle", std::make_optional(0.0), std::make_optional(std::string("0_NG")) } });
    udsMap.insert({ { 0x7E0, 0x51 }, { 0x7E0, 0x2DA, 0x51, false, 0, 37, 0, 1.0, 0.0, 4, "Fuel Cut Info Bank 2 for Idle", std::make_optional(1.0), std::make_optional(std::string("1_No Mal")) } });
    udsMap.insert({ { 0x7E0, 0x51 }, { 0x7E0, 0x2DA, 0x51, false, 0, 37, 0, 1.0, 0.0, 4, "Fuel Cut Info Bank 2 for Idle", std::make_optional(2.0), std::make_optional(std::string("2_Auto Mode")) } });
    udsMap.insert({ { 0x7E0, 0x51 }, { 0x7E0, 0x2DA, 0x51, false, 0, 37, 0, 1.0, 0.0, 4, "Fuel Cut Info Bank 2 for Idle", std::make_optional(3.0), std::make_optional(std::string("3_Temp OK")) } });
    udsMap.insert({ { 0x7E0, 0x51 }, { 0x7E0, 0x2DA, 0x51, false, 0, 37, 0, 1.0, 0.0, 4, "Fuel Cut Info Bank 2 for Idle", std::make_optional(4.0), std::make_optional(std::string("4_Postulate")) } });
    udsMap.insert({ { 0x7E0, 0x51 }, { 0x7E0, 0x2DA, 0x51, false, 0, 37, 0, 1.0, 0.0, 4, "Fuel Cut Info Bank 2 for Idle", std::make_optional(5.0), std::make_optional(std::string("5_Other Sys OK")) } });
    udsMap.insert({ { 0x7E0, 0x51 }, { 0x7E0, 0x2DA, 0x51, false, 0, 37, 0, 1.0, 0.0, 4, "Fuel Cut Info Bank 2 for Idle", std::make_optional(6.0), std::make_optional(std::string("6_Idle OK")) } });
    udsMap.insert({ { 0x7E0, 0x51 }, { 0x7E0, 0x2DA, 0x51, false, 0, 37, 0, 1.0, 0.0, 4, "Fuel Cut Info Bank 2 for Idle", std::make_optional(7.0), std::make_optional(std::string("7_ASG OK")) } });
    udsMap.insert({ { 0x7E0, 0x51 }, { 0x7E0, 0x2DA, 0x51, false, 0, 37, 0, 1.0, 0.0, 4, "Fuel Cut Info Bank 2 for Idle", std::make_optional(8.0), std::make_optional(std::string("8_CAT OK")) } });
    udsMap.insert({ { 0x7E0, 0x51 }, { 0x7E0, 0x2DA, 0x51, false, 0, 37, 0, 1.0, 0.0, 4, "Fuel Cut Info Bank 2 for Idle", std::make_optional(9.0), std::make_optional(std::string("9_FC OK")) } });
    udsMap.insert({ { 0x7E0, 0x56 }, { 0x7E0, 0x2E3, 0x56, false, 42, 2, 0, 0.01, 0.0, 4, "Vehicle Speed for Maximum Engine Speed", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x2ED, 0x3, false, 33, 13, 0, 0.392156862745098, 0.0, 4, "Fuel Remaining Volume", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x38 }, { 0x7E0, 0x2FB, 0x38, false, 0, 23, 0, 0.01, -327.68, 4, "Exhaust Fuel Addition FB", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x52 }, { 0x7E0, 0x329, 0x52, false, 0, 21, 4, 1.0, 0.0, 4, "Engine Coolant Temp High", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x52 }, { 0x7E0, 0x329, 0x52, false, 0, 21, 4, 1.0, 0.0, 4, "Engine Coolant Temp High", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x32E, 0x3, false, 0, 14, 1, 1.0, 0.0, 4, "Shift SW Status (N,P Range) Supported", std::make_optional(0.0), std::make_optional(std::string("Unsupp")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x32E, 0x3, false, 0, 14, 1, 1.0, 0.0, 4, "Shift SW Status (N,P Range) Supported", std::make_optional(1.0), std::make_optional(std::string("Supp")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x330, 0x3, false, 0, 15, 1, 1.0, 0.0, 4, "Shift SW Status (N,P Range)", std::make_optional(0.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x330, 0x3, false, 0, 15, 1, 1.0, 0.0, 4, "Shift SW Status (N,P Range)", std::make_optional(1.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x5 }, { 0x7E0, 0x34F, 0x5, false, 0, 14, 2, 1.0, 0.0, 4, "Fuel Temperature Supported", std::make_optional(0.0), std::make_optional(std::string("Unsupp")) } });
    udsMap.insert({ { 0x7E0, 0x5 }, { 0x7E0, 0x34F, 0x5, false, 0, 14, 2, 1.0, 0.0, 4, "Fuel Temperature Supported", std::make_optional(1.0), std::make_optional(std::string("Supp")) } });
    udsMap.insert({ { 0x7E0, 0x5 }, { 0x7E0, 0x354, 0x5, false, 57, 19, 0, 1.0, -40.0, 4, "Fuel Temperature", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x38 }, { 0x7E0, 0x36A, 0x38, false, 0, 25, 0, 0.01, -327.68, 4, "Exhaust Fuel Addition FB #2", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x3A }, { 0x7E0, 0x373, 0x3A, false, 0, 37, 7, 1.0, 0.0, 4, "Exhaust Fuel Addition Injector Status", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x3A }, { 0x7E0, 0x373, 0x3A, false, 0, 37, 7, 1.0, 0.0, 4, "Exhaust Fuel Addition Injector Status", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x3A }, { 0x7E0, 0x374, 0x3A, false, 0, 37, 6, 1.0, 0.0, 4, "Exhaust Fuel Addition Injector Status #2", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x3A }, { 0x7E0, 0x374, 0x3A, false, 0, 37, 6, 1.0, 0.0, 4, "Exhaust Fuel Addition Injector Status #2", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x39 }, { 0x7E0, 0x385, 0x39, false, 0, 10, 2, 1.0, 0.0, 4, "Fuel Return Pipe Valve", std::make_optional(0.0), std::make_optional(std::string("CLOSE")) } });
    udsMap.insert({ { 0x7E0, 0x39 }, { 0x7E0, 0x385, 0x39, false, 0, 10, 2, 1.0, 0.0, 4, "Fuel Return Pipe Valve", std::make_optional(1.0), std::make_optional(std::string("OPEN")) } });
    udsMap.insert({ { 0x7E0, 0x39 }, { 0x7E0, 0x388, 0x39, false, 0, 5, 7, 1.0, 0.0, 4, "Fuel Route Switching Valve", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x39 }, { 0x7E0, 0x388, 0x39, false, 0, 5, 7, 1.0, 0.0, 4, "Fuel Route Switching Valve", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x39 }, { 0x7E0, 0x389, 0x39, false, 0, 5, 6, 1.0, 0.0, 4, "Intank Fuel Pump", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x39 }, { 0x7E0, 0x389, 0x39, false, 0, 5, 6, 1.0, 0.0, 4, "Intank Fuel Pump", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x52 }, { 0x7E0, 0x390, 0x52, false, 0, 34, 6, 1.0, 0.0, 4, "Throttle Air Flow Learning Prohibit(Air Fuel Ratio Malfunction)", std::make_optional(0.0), std::make_optional(std::string("OK")) } });
    udsMap.insert({ { 0x7E0, 0x52 }, { 0x7E0, 0x390, 0x52, false, 0, 34, 6, 1.0, 0.0, 4, "Throttle Air Flow Learning Prohibit(Air Fuel Ratio Malfunction)", std::make_optional(1.0), std::make_optional(std::string("NG")) } });
    udsMap.insert({ { 0x7E0, 0x3C }, { 0x7E0, 0x392, 0x3C, false, 0, 21, 6, 1.0, 0.0, 4, "Fuel Dilution Status", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x3C }, { 0x7E0, 0x392, 0x3C, false, 0, 21, 6, 1.0, 0.0, 4, "Fuel Dilution Status", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x3C }, { 0x7E0, 0x394, 0x3C, false, 0, 35, 7, 1.0, 0.0, 4, "Sub Fuel Tank Pump Relay", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x3C }, { 0x7E0, 0x394, 0x3C, false, 0, 35, 7, 1.0, 0.0, 4, "Sub Fuel Tank Pump Relay", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x1 }, { 0x7E0, 0x3A7, 0x1, false, 39, 10, 0, 0.25, 0.0, 4, "Engine Speed", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x3B4, 0x3, false, 0, 0, 0, 1.0, 0.0, 4, "Fuel System Status #1", std::make_optional(0.0), std::make_optional(std::string("Unused")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x3B4, 0x3, false, 0, 0, 0, 1.0, 0.0, 4, "Fuel System Status #1", std::make_optional(1.0), std::make_optional(std::string("OL")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x3B4, 0x3, false, 0, 0, 0, 1.0, 0.0, 4, "Fuel System Status #1", std::make_optional(2.0), std::make_optional(std::string("CL")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x3B4, 0x3, false, 0, 0, 0, 1.0, 0.0, 4, "Fuel System Status #1", std::make_optional(4.0), std::make_optional(std::string("OLDrive")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x3B4, 0x3, false, 0, 0, 0, 1.0, 0.0, 4, "Fuel System Status #1", std::make_optional(8.0), std::make_optional(std::string("OLFault")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x3B4, 0x3, false, 0, 0, 0, 1.0, 0.0, 4, "Fuel System Status #1", std::make_optional(16.0), std::make_optional(std::string("CLFault")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x3B5, 0x3, false, 0, 1, 0, 1.0, 0.0, 4, "Fuel System Status #2", std::make_optional(0.0), std::make_optional(std::string("Unused")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x3B5, 0x3, false, 0, 1, 0, 1.0, 0.0, 4, "Fuel System Status #2", std::make_optional(1.0), std::make_optional(std::string("OL")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x3B5, 0x3, false, 0, 1, 0, 1.0, 0.0, 4, "Fuel System Status #2", std::make_optional(2.0), std::make_optional(std::string("CL")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x3B5, 0x3, false, 0, 1, 0, 1.0, 0.0, 4, "Fuel System Status #2", std::make_optional(4.0), std::make_optional(std::string("OLDrive")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x3B5, 0x3, false, 0, 1, 0, 1.0, 0.0, 4, "Fuel System Status #2", std::make_optional(8.0), std::make_optional(std::string("OLFault")) } });
    udsMap.insert({ { 0x7E0, 0x3 }, { 0x7E0, 0x3B5, 0x3, false, 0, 1, 0, 1.0, 0.0, 4, "Fuel System Status #2", std::make_optional(16.0), std::make_optional(std::string("CLFault")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x3B6, 0x25, false, 0, 1, 0, 1.0, 0.0, 4, "Fuel Select Switch", std::make_optional(0.0), std::make_optional(std::string("Gasoline")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x3B6, 0x25, false, 0, 1, 0, 1.0, 0.0, 4, "Fuel Select Switch", std::make_optional(1.0), std::make_optional(std::string("CNG")) } });
    udsMap.insert({ { 0x7E0, 0x48 }, { 0x7E0, 0x3B9, 0x48, false, 36, 0, 0, 0.0002337646484375, 0.0, 4, "Tank Fuel Pressure", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x48 }, { 0x7E0, 0x3BA, 0x48, false, 36, 2, 0, 0.0002337646484375, 0.0, 4, "Delivery Fuel Pressure", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x48 }, { 0x7E0, 0x3BB, 0x48, false, 57, 4, 0, 1.0, -40.0, 4, "Tank Fuel Temperature", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x48 }, { 0x7E0, 0x3BC, 0x48, false, 57, 5, 0, 1.0, -40.0, 4, "Delivery Fuel Temperature", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x48 }, { 0x7E0, 0x3C9, 0x48, false, 0, 15, 4, 1.0, 0.0, 4, "Fuel Pump2 Speed Control", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x48 }, { 0x7E0, 0x3C9, 0x48, false, 0, 15, 4, 1.0, 0.0, 4, "Fuel Pump2 Speed Control", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x48 }, { 0x7E0, 0x3CB, 0x48, false, 48, 18, 0, 7.62939453125e-05, 0.0, 4, "Tank Fuel Pressure Sensor Voltage", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x48 }, { 0x7E0, 0x3CC, 0x48, false, 48, 20, 0, 7.62939453125e-05, 0.0, 4, "Delivery Fuel Pressure Sensor Voltage", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x48 }, { 0x7E0, 0x3CD, 0x48, false, 48, 22, 0, 7.62939453125e-05, 0.0, 4, "Delivery Fuel Temperature Sensor Voltage", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x48 }, { 0x7E0, 0x3D2, 0x48, false, 36, 27, 0, 0.001953125, 0.0, 4, "Tank Fuel Pressure", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x37 }, { 0x7E0, 0x3D4, 0x37, false, 60, 31, 0, 0.03125, 0.0, 4, "Knock Sensor Lowest Learning Value", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x3C }, { 0x7E0, 0x3DC, 0x3C, false, 36, 36, 0, 0.001953125, -64.0, 4, "High Fuel Pressure Sensor", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x3C }, { 0x7E0, 0x441, 0x3C, false, 34, 38, 0, 0.022, -720.896, 4, "Low Fuel Pressure Sensor", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x3D }, { 0x7E0, 0x446, 0x3D, false, 57, 49, 0, 1.0, -40.0, 4, "Fuel Return Temperature", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x3E }, { 0x7E0, 0x44D, 0x3E, false, 34, 10, 0, 10.0, 0.0, 4, "Fuel Pressure #1", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x3E }, { 0x7E0, 0x44E, 0x3E, false, 34, 12, 0, 10.0, 0.0, 4, "Fuel Pressure #2", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x3E }, { 0x7E0, 0x44F, 0x3E, false, 34, 14, 0, 10.0, 0.0, 4, "Fuel Pressure #3", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x3E }, { 0x7E0, 0x450, 0x3E, false, 34, 16, 0, 10.0, 0.0, 4, "Fuel Pressure #4", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x3E }, { 0x7E0, 0x451, 0x3E, false, 57, 18, 0, 1.0, -40.0, 4, "Fuel Temperature #1", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x3E }, { 0x7E0, 0x452, 0x3E, false, 57, 19, 0, 1.0, -40.0, 4, "Fuel Temperature #2", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x3E }, { 0x7E0, 0x453, 0x3E, false, 57, 20, 0, 1.0, -40.0, 4, "Fuel Temperature #3", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x3E }, { 0x7E0, 0x454, 0x3E, false, 57, 21, 0, 1.0, -40.0, 4, "Fuel Temperature #4", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x47C, 0x25, false, 0, 6, 3, 1.0, 0.0, 4, "Sports Drive Switch", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x47C, 0x25, false, 0, 6, 3, 1.0, 0.0, 4, "Sports Drive Switch", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x486, 0x25, false, 0, 2, 7, 1.0, 0.0, 4, "Fuel Cooler SW", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x486, 0x25, false, 0, 2, 7, 1.0, 0.0, 4, "Fuel Cooler SW", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x4AA, 0x25, false, 0, 2, 5, 1.0, 0.0, 4, "Clogged Fuel Filter Switch", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x25 }, { 0x7E0, 0x4AA, 0x25, false, 0, 2, 5, 1.0, 0.0, 4, "Clogged Fuel Filter Switch", std::make_optional(1.0), std::make_optional(std::string("ON")) } });
    udsMap.insert({ { 0x7E0, 0x48 }, { 0x7E0, 0x4B0, 0x48, false, 0, 14, 5, 1.0, 0.0, 4, "Fuel Cooler VSV", std::make_optional(0.0), std::make_optional(std::string("Close")) } });
    udsMap.insert({ { 0x7E0, 0x48 }, { 0x7E0, 0x4B0, 0x48, false, 0, 14, 5, 1.0, 0.0, 4, "Fuel Cooler VSV", std::make_optional(1.0), std::make_optional(std::string("Open")) } });
    udsMap.insert({ { 0x7E0, 0x83 }, { 0x7E0, 0x4D3, 0x83, false, 0, 16, 0, 1.0, 0.0, 4, "Shift Range Indicator", std::make_optional(0.0), std::make_optional(std::string("OFF")) } });
    udsMap.insert({ { 0x7E0, 0x83 }, { 0x7E0, 0x4D3, 0x83, false, 0, 16, 0, 1.0, 0.0, 4, "Shift Range Indicator", std::make_optional(1.0), std::make_optional(std::string("1st")) } });
    udsMap.insert({ { 0x7E0, 0x83 }, { 0x7E0, 0x4D3, 0x83, false, 0, 16, 0, 1.0, 0.0, 4, "Shift Range Indicator", std::make_optional(2.0), std::make_optional(std::string("2nd")) } });
    udsMap.insert({ { 0x7E0, 0x83 }, { 0x7E0, 0x4D3, 0x83, false, 0, 16, 0, 1.0, 0.0, 4, "Shift Range Indicator", std::make_optional(3.0), std::make_optional(std::string("3rd")) } });
    udsMap.insert({ { 0x7E0, 0x83 }, { 0x7E0, 0x4D3, 0x83, false, 0, 16, 0, 1.0, 0.0, 4, "Shift Range Indicator", std::make_optional(4.0), std::make_optional(std::string("4th")) } });
    udsMap.insert({ { 0x7E0, 0x83 }, { 0x7E0, 0x4D3, 0x83, false, 0, 16, 0, 1.0, 0.0, 4, "Shift Range Indicator", std::make_optional(5.0), std::make_optional(std::string("5th")) } });
    udsMap.insert({ { 0x7E0, 0x83 }, { 0x7E0, 0x4D3, 0x83, false, 0, 16, 0, 1.0, 0.0, 4, "Shift Range Indicator", std::make_optional(6.0), std::make_optional(std::string("6th")) } });
    udsMap.insert({ { 0x7E0, 0x83 }, { 0x7E0, 0x4D3, 0x83, false, 0, 16, 0, 1.0, 0.0, 4, "Shift Range Indicator", std::make_optional(7.0), std::make_optional(std::string("7th")) } });
    udsMap.insert({ { 0x7E0, 0x83 }, { 0x7E0, 0x4D3, 0x83, false, 0, 16, 0, 1.0, 0.0, 4, "Shift Range Indicator", std::make_optional(8.0), std::make_optional(std::string("8th")) } });
    udsMap.insert({ { 0x7E0, 0x83 }, { 0x7E0, 0x4D3, 0x83, false, 0, 16, 0, 1.0, 0.0, 4, "Shift Range Indicator", std::make_optional(9.0), std::make_optional(std::string("9th")) } });
    udsMap.insert({ { 0x7E0, 0x83 }, { 0x7E0, 0x4D3, 0x83, false, 0, 16, 0, 1.0, 0.0, 4, "Shift Range Indicator", std::make_optional(10.0), std::make_optional(std::string("10th")) } });
    udsMap.insert({ { 0x7E0, 0x8 }, { 0x7E0, 0x4FC, 0x8, false, 46, 4, 0, 0.02, 0.0, 4, "Engine Fuel Rate", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x8 }, { 0x7E0, 0x4FD, 0x8, false, 46, 6, 0, 0.02, 0.0, 4, "Vehicle Fuel Rate", std::nullopt, std::nullopt } });
    udsMap.insert({ { 0x7E0, 0x6 }, { 0x7E0, 0x558, 0x6, false, 0, 1, 5, 1.0, 0.0, 4, "Fuel System Monitor Result", std::make_optional(0.0), std::make_optional(std::string("Compl")) } });
    udsMap.insert({ { 0x7E0, 0x6 }, { 0x7E0, 0x558, 0x6, false, 0, 1, 5, 1.0, 0.0, 4, "Fuel System Monitor Result", std::make_optional(1.0), std::make_optional(std::string("Incmpl")) } });
}

const int UDS_MAP_SIZE = udsMap.size();

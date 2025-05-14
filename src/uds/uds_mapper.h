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
    uint16_t ecu_id;         // Also known as obd2_request_id_hex or request_id
    uint16_t parameter_id;   // parameter_id_hex
    uint16_t data_id;        // uds_data_identifier_hex
    std::string parameter_name;
    double value;
    std::optional<std::string> display_value;
    std::optional<std::string> unit;

    // Constructor
    SignalValue(uint16_t ecu_id_, uint16_t parameter_id_, uint16_t data_id_,
                const std::string& name, double val,
                std::optional<std::string> display = std::nullopt,
                std::optional<std::string> unit_str = std::nullopt)
        : ecu_id(ecu_id_), parameter_id(parameter_id_), data_id(data_id_),
          parameter_name(name), value(val), display_value(display), unit(unit_str) {}
};

// udsDefinition_key: tuple of (ecu_id, parameter_id, data_id)
using udsDefinition_key = std::tuple<uint16_t, uint16_t, uint16_t>;

struct udsDefinition_key_hash {
    std::size_t operator()(const udsDefinition_key& key) const {
        auto h1 = std::hash<uint16_t>{}(std::get<0>(key));  // ecu_id
        auto h2 = std::hash<uint16_t>{}(std::get<1>(key));  // parameter_id
        auto h3 = std::hash<uint16_t>{}(std::get<2>(key));  // data_id
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

inline std::unordered_multimap<udsDefinition_key, UdsDefinition, udsDefinition_key_hash> udsMap;
inline void init_udsDefinitions() {
}    // Call the split initialization functions to avoid ESP32 l32r relocation issues
  
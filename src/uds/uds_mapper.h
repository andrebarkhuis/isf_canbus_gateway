#pragma once
#include <cstdint>
#include <string>
#include <tuple>
#include <unordered_map>
#include <optional>

struct UdsDefinition
{
    uint16_t                   request_id     = 0;
    uint16_t                   did            = 0;
    int8_t                     unit           = 0;
    int8_t                     byte_position  = 0;
    int8_t                     bit_offset_position = 0;
    double                     scaling_factor  = 1.0;
    double                     offset_value    = 0.0;
    bool                       is_calculated   = false;
    int8_t                     bit_length      = 0;
    std::string                name;
    std::optional<uint8_t>     value           = std::nullopt;
    std::optional<std::string> display_value   = std::nullopt;
};

using uds_key = std::tuple<uint16_t, uint16_t>; // {tx_id, data_id}

struct uds_key_hash {
    size_t operator()(const uds_key& k) const noexcept {
        auto h1 = std::hash<uint16_t>{}(std::get<0>(k));
        auto h2 = std::hash<uint16_t>{}(std::get<1>(k));
        return h1 ^ (h2 << 1);
    }
};

inline std::unordered_multimap<uds_key, UdsDefinition, uds_key_hash> udsMap;

void init_udsDefinitions();

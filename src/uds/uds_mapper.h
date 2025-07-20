#pragma once
#include <cstdint>
#include <string>
#include <tuple>
#include <unordered_map>
#include <optional>

/* ── UdsDefinition ─────────────────────────────────────────────── */
struct UdsDefinition
{
    // raw metadata
    uint16_t obd2_request_id_hex;
    uint16_t uds_data_identifier_hex;
    uint8_t  unit_type;
    uint8_t  byte_position;
    uint8_t  bit_offset_position;
    double   scaling_factor;
    double   offset_value;
    bool     is_calculated_value;               // set by the c’tor
    uint8_t  bit_length;
    std::string   parameter_name;
    std::optional<double>        parameter_value;
    std::optional<std::string>   parameter_display_value;

    /* ── ctor #1 : calculated signal ──────────────────────────── */
    constexpr UdsDefinition(
        uint16_t obd2_id,
        uint16_t data_id,
        uint8_t  unit,
        uint8_t  byte_pos,
        uint8_t  bit_offs,
        double   scale,
        double   offs,
        uint8_t  bits,
        std::string   name)
        : obd2_request_id_hex   { obd2_id }
        , uds_data_identifier_hex{ data_id }
        , unit_type             { unit }
        , byte_position         { byte_pos }
        , bit_offset_position   { bit_offs }
        , scaling_factor        { scale }
        , offset_value          { offs }
        , is_calculated_value   { true }            // <- automatic
        , bit_length            { bits }
        , parameter_name        { std::move(name) }
        , parameter_value       { std::nullopt }
        , parameter_display_value{ std::nullopt }
    {}

    /* ── ctor #2 : enumerated / fixed value ───────────────────── */
    /*  enumerated / fixed value  (11 args — no scale)          *
     *  scale and offset fall back to 1.0 / 0.0 by default      */
    constexpr UdsDefinition(
        uint16_t obd2_id,
        uint16_t data_id,
        uint8_t  unit,
        uint8_t  byte_pos,
        uint8_t  bit_offs,
        uint8_t  bits,
        std::string   name,
        uint8_t  enum_value,
        std::string   display,
        double   scale = 1.0,
        double   offs  = 0.0)
        : obd2_request_id_hex   { obd2_id }
        , uds_data_identifier_hex{ data_id }
        , unit_type             { unit }
        , byte_position         { byte_pos }
        , bit_offset_position   { bit_offs }
        , scaling_factor        { scale }
        , offset_value          { offs }
        , is_calculated_value   { false }
        , bit_length            { bits }
        , parameter_name        { std::move(name) }
        , parameter_value       { std::make_optional(enum_value) }
        , parameter_display_value{ std::make_optional<std::string>(std::move(display)) }
    {}
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

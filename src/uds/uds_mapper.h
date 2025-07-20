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
    std::uint16_t obd2_request_id_hex;
    std::uint16_t parameter_id_hex;
    std::uint16_t uds_data_identifier_hex;
    std::uint8_t  unit_type;
    std::uint8_t  byte_position;
    std::uint8_t  bit_offset_position;
    float         scaling_factor;
    float         offset_value;
    bool          is_calculated_value;               // set by the c’tor
    std::uint8_t  bit_length;
    std::string   parameter_name;
    std::optional<float>        parameter_value;
    std::optional<std::string>   parameter_display_value;

    /* ── ctor #1 : calculated signal ──────────────────────────── */
    constexpr UdsDefinition(std::uint16_t obd2_id,
        std::uint16_t param_id,
        std::uint16_t data_id,
        std::uint8_t  unit,
        std::uint8_t  byte_pos,
        std::uint8_t  bit_offs,
        float         scale,
        float         offs,
        std::uint8_t  bits,
        std::string   name)
        : obd2_request_id_hex   { obd2_id }
        , parameter_id_hex      { param_id }
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
    constexpr UdsDefinition(std::uint16_t obd2_id,
        std::uint16_t param_id,
        std::uint16_t data_id,
        std::uint8_t  unit,
        std::uint8_t  byte_pos,
        std::uint8_t  bit_offs,
        std::uint8_t  bits,
        std::string   name,
        float         enum_value,
        std::string   display,
        float         scale = 1.0,
        float         offs  = 0.0)
        : obd2_request_id_hex   { obd2_id }
        , parameter_id_hex      { param_id }
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

/* ── map setup (unchanged) ─────────────────────────────────────── */
using uds_key = std::tuple<std::uint16_t, std::uint16_t>;

#if __cpp_lib_tuple >= 201907L
struct uds_key_hash : std::hash<uds_key> {};
#else
struct uds_key_hash {
    std::size_t operator()(const uds_key& k) const noexcept {
        auto h1 = std::hash<std::uint16_t>{}(std::get<0>(k));
        auto h2 = std::hash<std::uint16_t>{}(std::get<1>(k));
        return h1 ^ (h2 << 1);
    }
};
#endif

inline std::unordered_multimap<uds_key, UdsDefinition, uds_key_hash> udsMap;
void init_udsDefinitions();     // defined in a .cpp

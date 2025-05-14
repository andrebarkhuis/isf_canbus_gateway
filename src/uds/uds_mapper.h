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
    bool          is_signed;
    int           unit_type;
    int           byte_position;
    int           bit_offset_position;
    double        scaling_factor;
    double        offset_value;
    bool          is_calculated_value;               // set by the c’tor
    int           bit_length;
    std::string   parameter_name;

    std::optional<double>        parameter_value;
    std::optional<std::string>   parameter_display_value;

    /* ── ctor #1 : calculated signal ──────────────────────────── */
    constexpr UdsDefinition(std::uint16_t obd2_id,
                            std::uint16_t param_id,
                            std::uint16_t data_id,
                            bool          signed_flag,
                            int           unit,
                            int           byte_pos,
                            int           bit_offs,
                            double        scale,
                            double        offs,
                            int           bits,
                            std::string   name)
        : obd2_request_id_hex   { obd2_id }
        , parameter_id_hex      { param_id }
        , uds_data_identifier_hex{ data_id }
        , is_signed             { signed_flag }
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
        bool          signed_flag,
        int           unit,
        int           byte_pos,
        int           bit_offs,
        int           bits,
        std::string   name,
        double        enum_value,
        std::string   display,
        double        scale = 1.0,
        double        offs  = 0.0)
        : obd2_request_id_hex   { obd2_id }
        , parameter_id_hex      { param_id }
        , uds_data_identifier_hex{ data_id }
        , is_signed             { signed_flag }
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
using uds_key = std::tuple<std::uint16_t, std::uint16_t, std::uint16_t>;

#if __cpp_lib_tuple >= 201907L
struct uds_key_hash : std::hash<uds_key> {};
#else
struct uds_key_hash {
    std::size_t operator()(const uds_key& k) const noexcept {
        auto h1 = std::hash<std::uint16_t>{}(std::get<0>(k));
        auto h2 = std::hash<std::uint16_t>{}(std::get<1>(k));
        auto h3 = std::hash<std::uint16_t>{}(std::get<2>(k));
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};
#endif

inline std::unordered_multimap<uds_key, UdsDefinition, uds_key_hash> udsMap;
void init_udsDefinitions();     // defined in a .cpp

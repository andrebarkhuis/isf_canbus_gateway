#include "isf_service.h"
#include "../mcp_can/mcp_can.h"
#include "../logger/logger.h"
#include "../uds/uds_mapper.h"
#include "../uds/uds.h"
#include "../isotp/iso_tp.h"
#include <algorithm>
#include <string>
#include <unordered_map>
#include <tuple>
#include <optional>

IsfService::IsfService()
{
    // Initialize the array with the correct size
    lastUdsRequestTime = new unsigned long[ISF_UDS_REQUESTS_SIZE]();
}

IsfService::~IsfService()
{
    delete uds;
    delete isotp;
    delete mcp;
    delete[] lastUdsRequestTime; // Clean up the array
}

/**
 * @brief Initializes the ISF service and CAN interface
 *
 * Sets up the CAN interface and validates that the UDS and ISO-TP objects
 * are properly initialized.
 *
 * @return true if initialization was successful, false otherwise
 */
bool IsfService::initialize()
{
    // Initialize UDS response mappings
    init_udsDefinitions();

    // Create MCP_CAN with specific CS pin
    mcp = new MCP_CAN();
    if (mcp == nullptr)
    {
        Logger::error("[IsfService:initialize] Failed to create MCP_CAN instance.");
        return false;
    }
    else
    {
        Logger::info("[IsfService:initialize] MCP_CAN instance created successfully.");
    }

    // Initialize the MCP_CAN directly
    byte initResult = mcp->begin();
    if (initResult != CAN_OK)
    {
        Logger::error("[IsfService:initialize] MCP_CAN initialization failed. Error code: %d", initResult);
        return false;
    }
    else
    {
        Logger::info("[IsfService:initialize] MCP_CAN initialized successfully. Code: %d", initResult);
    }

    // Create IsoTp instance
    isotp = new IsoTp(mcp);
    if (isotp == nullptr)
    {
        Logger::error("[IsfService:initialize] Failed to create IsoTp instance.");
        return false;
    }
    else
    {
        Logger::info("[IsfService:initialize] IsoTp instance created successfully.");
    }

    // Create UDS instance
    uds = new UDS(isotp);
    if (uds == nullptr)
    {
        Logger::error("[IsfService:initialize] Failed to create UDS instance.");
        return false;
    }
    else
    {
        Logger::info("[IsfService:initialize] UDS instance created successfully.");
    }

#ifdef DEBUG_ISF
    Logger::debug("[IsfService:initialize] Running on core %d", xPortGetCoreID());
#endif
    return true;
}

/**
 * @brief Sends diagnostic session initialization messages
 *
 * Sends all predefined session initialization messages from isf_pid_session_requests
 * to prepare the ECUs for diagnostic communication.
 *
 * @return true if all session messages were sent successfully, false otherwise
 */
bool IsfService::initialize_diagnostic_session()
{
    const int SESSION_REQUESTS_SIZE = sizeof(isf_pid_session_requests) / sizeof(CANMessage);

    for (int i = 0; i < SESSION_REQUESTS_SIZE; i++)
    {
        bool failed = mcp->sendMsgBuf(isf_pid_session_requests[i].id,
                                      isf_pid_session_requests[i].extended ? 1 : 0,
                                      isf_pid_session_requests[i].len,
                                      const_cast<byte *>(isf_pid_session_requests[i].data)); // Cast to match byte*
        if (failed)
        {
            Logger::error("[IsfService:initialize_diagnostic_session] Failed to send session request for ID: %d", isf_pid_session_requests[i].id);
            return false;
        }
    }

    return true;
}

/**
 * @brief Main processing function that sends requests and receives messages
 *
 * This method should be called periodically from a task loop to:
 * 1. Send scheduled UDS requests to the ECU
 * 2. Process incoming CAN messages
 * 3. Blink activity LED to indicate operation
 */
void IsfService::listen()
{
#ifdef LED_BUILTIN
    digitalWrite(LED_BUILTIN, HIGH);
#endif

    if (initialize_diagnostic_session())
    {
        beginSend();
    }

#ifdef LED_BUILTIN
    digitalWrite(LED_BUILTIN, LOW);
#endif
    vTaskDelay(pdMS_TO_TICKS(5)); // Delay to avoid flooding the bus
}

bool IsfService::beginSend()
{
    unsigned long currentTime = millis();

    for (int i = 0; i < ISF_UDS_REQUESTS_SIZE; i++)
    {
        if (currentTime - lastUdsRequestTime[i] >= isf_uds_requests[i].interval)
        {
            uint8_t udsMessage[8] = {0};
            uint8_t dataLength = 0;

            switch (isf_uds_requests[i].service_id)
            {
            case UDS_SID_READ_DATA_BY_ID:
                udsMessage[0] = (isf_uds_requests[i].did >> 8) & 0xFF; // DID high byte
                udsMessage[1] = isf_uds_requests[i].did & 0xFF;        // DID low byte
                dataLength = 2;
                break;

            case OBD_MODE_SHOW_CURRENT_DATA:
                udsMessage[0] = isf_uds_requests[i].did & 0xFF; // Single-byte PID
                dataLength = 1;
                break;

            case UDS_SID_TESTER_PRESENT:
                udsMessage[0] = 0x00; // Sub-function for Tester Present
                dataLength = 1;
                break;

            case UDS_SID_READ_DATA_BY_LOCAL_ID:                 // Techstream SID for Local Identifier requests
                udsMessage[0] = 0x02;                           // Length of the remaining bytes
                udsMessage[1] = UDS_SID_READ_DATA_BY_LOCAL_ID;  // Use defined SID instead of hardcoded value
                udsMessage[2] = isf_uds_requests[i].did & 0xFF; // Identifier (0x01, 0xE1, etc.)
                dataLength = 3;                                 // SID + Identifier + length byte
                break;

            default:
                Logger::error("[IsfService:beginSend] Unsupported UDS service ID: %02X", isf_uds_requests[i].service_id);
                continue; // Skip unknown services
            }

            sendUdsRequest(udsMessage, dataLength, isf_uds_requests[i]);

            lastUdsRequestTime[i] = currentTime;

            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }

    return true;
}

bool IsfService::sendUdsRequest(uint8_t *udsMessage, uint8_t dataLength, const UDSRequest &request)
{
    Session_t session;
    session.tx_id = request.tx_id;
    session.rx_id = request.rx_id;
    session.sid = request.service_id;
    session.len = dataLength;
    session.Data = udsMessage;

    uint16_t retval = uds->Session(&session);

    if (retval == UDS_NRC_SUCCESS)
    {
#ifdef DEBUG_ISF
        Logger::info("[IsfService:sendUdsRequest] UDS request successful. %s", request.param_name);
#endif
        // Read ISO-TP response directly here
        if (processUdsResponse(session.Data, session.len, request))
        {
#ifdef DEBUG_ISF
            Logger::info("[IsfService:sendUdsRequest] UDS response parsed successfully. %s", request.param_name);
#endif
            return true;
        }
        else
        {
#ifdef DEBUG_ISF
            Logger::warn("[IsfService:sendUdsRequest] Failed to parse UDS response data. %s", request.param_name);
#endif
            return false;
        }
    }
    else
    {
#ifdef DEBUG_ISF
        Logger::error("[IsfService:sendUdsRequest] UDS session failed with retval: %04X", retval);
#endif
        return false;
    }
}

bool IsfService::processUdsResponse(uint8_t *data, uint8_t length, const UDSRequest &request)
{
    if (data == nullptr || length == 0)
    {
        Logger::error("[IsfService:processUdsResponse] Invalid UDS response data");
        return false;
    }

    switch (request.service_id)
    {
    case UDS_SID_READ_DATA_BY_LOCAL_ID: // Local Identifier (Techstream)
    case UDS_SID_READ_DATA_BY_ID:
        return transformResponse(data, length, request);
    default:
        Logger::error("[IsfService:processUdsResponse] Unsupported response SID: %02X", request.service_id);
        return false;
    }
}

namespace std
{
    template <>
    struct hash<std::tuple<uint16_t, int, int>>
    {
        size_t operator()(const std::tuple<uint16_t, int, int> &k) const
        {
            size_t h1 = std::hash<uint16_t>{}(std::get<0>(k));
            size_t h2 = std::hash<int>{}(std::get<1>(k));
            size_t h3 = std::hash<int>{}(std::get<2>(k));
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };
}

std::vector<UdsDefinition> get_uds_definitions(uint16_t request_id, uint16_t data_id)
{
    std::vector<UdsDefinition> results;

    auto range = udsMap.equal_range(std::make_tuple(request_id, data_id));
    for (auto it = range.first; it != range.second; ++it)
    {
        results.push_back(it->second);
    }

    return results;
}

/**
 * @brief Safely extracts raw data bits from a CAN data buffer.
 *
 * Ensures bit boundaries are valid before extracting, and logs the specific
 * parameter name if an issue is found.
 *
 * @param data            Raw data buffer (must be non-null)
 * @param byte_pos        Starting byte position to extract from
 * @param bit_offset      Bit offset within starting byte
 * @param bit_length      Total number of bits to extract
 * @param data_len        Length of the data buffer
 * @param parameter_name  Signal name (for debug/error context)
 * @return Extracted raw value (uint32_t), or 0 on error
 */
uint32_t extract_raw_data(
    const uint8_t *data,
    int byte_pos,
    int bit_offset,
    int bit_length,
    int data_len,
    const std::string &parameter_name)
{
    if (!data)
    {
        Logger::error("[extract_raw_data] Null data pointer for parameter: %s", parameter_name.c_str());
        return 0;
    }

    if (byte_pos < 0 || byte_pos >= data_len)
    {
        Logger::error("[extract_raw_data] Byte position out of range: %d (needs 1 byte, buffer len = %d) for parameter: %s",
                      byte_pos, data_len, parameter_name.c_str());
        return 0;
    }

    int max_bytes = (bit_offset + bit_length + 7) / 8;
    if (byte_pos + max_bytes > data_len)
    {
        Logger::error("[extract_raw_data] Byte position out of range: %d (needs %d bytes, buffer len = %d) for parameter: %s",
                      byte_pos, max_bytes, data_len, parameter_name.c_str());
        return 0;
    }

    uint32_t raw = 0;
    memcpy(&raw, &data[byte_pos], 4); // Safe: already checked bounds
    raw >>= bit_offset;

    uint32_t mask = (1ULL << bit_length) - 1;
    return raw & mask;
}

std::optional<const char *> get_unit_name(int unit_type)
{
    switch (unit_type)
    {
    case UNIT_GENERAL:
        return std::nullopt;
    case UNIT_ACCELERATION:
        return "m/s²";
    case UNIT_G_FORCE:
        return "G";
    case UNIT_ACCEL_REQUEST:
        return "%";
    case UNIT_DECELERATION:
        return "m/s²";
    case UNIT_IGNITION_FEEDBACK:
        return "°";
    case UNIT_ANGLE_SENSOR:
        return "°";
    case UNIT_YAW_RATE:
        return "°/s";
    case UNIT_CURRENT_SENSOR:
        return "A";
    case UNIT_PM_SENSOR:
        return "mg/m³";
    case UNIT_DISTANCE:
        return "m";
    case UNIT_FORWARD_DISTANCE:
        return "m";
    case UNIT_ODOMETER:
        return "km";
    case UNIT_BATTERY_STATUS:
        return "%";
    case UNIT_POWER_MANAGEMENT:
        return "W";
    case UNIT_HYBRID_BATTERY:
        return "V";
    case UNIT_FUEL_SYSTEM:
        return "%";
    case UNIT_FUEL_INJECTION:
        return "ms";
    case UNIT_CRUISE_CONTROL:
        return "%";
    case UNIT_FREQUENCY_SENSOR:
        return "Hz";
    case UNIT_ILLUMINATION_SENSOR:
        return "lx";
    case UNIT_EXHAUST_SENSOR:
        return "ppm";
    case UNIT_LOAD_FUEL_TRIM:
        return "%";
    case UNIT_MAP_TIRE_PRESSURE:
        return "kPa";
    case UNIT_ENGINE_RPM:
        return "RPM";
    case UNIT_SPEED_SENSOR:
        return "km/h";
    case UNIT_VOLTAGE_SENSOR:
        return "V";
    case UNIT_TEMPERATURE_SENSOR:
        return "°C";
    case UNIT_TORQUE_SENSOR:
        return "Nm";
    case UNIT_POSITION_SENSOR:
        return "%";
    case UNIT_AMBIENT_TEMP:
        return "°C";
    case UNIT_MASS_AIR_FLOW:
        return "g/s";
    default:
        return std::nullopt;
    }
}

std::optional<SignalValue> get_signal_value(
    const UdsDefinition &definition,
    const uint8_t *data,
    int data_len,
    const std::vector<UdsDefinition> &candidates)
{
    if (!data)
    {
        Logger::error("[get_signal_value] Null data pointer for signal: %s", definition.parameter_name.c_str());
        return std::nullopt;
    }

    uint32_t raw_value = extract_raw_data(
        data,
        definition.byte_position,
        definition.bit_offset_position,
        definition.bit_length,
        data_len,
        definition.parameter_name);

    // If extraction failed, raw_value will be zero, but this might be valid — so add a flag
    if (definition.byte_position < 0 ||
        definition.byte_position >= data_len ||
        (definition.byte_position + ((definition.bit_offset_position + definition.bit_length + 7) / 8)) > data_len)
    {
        return std::nullopt; // Extraction failed due to range check
    }

    double decoded_value = raw_value * definition.scaling_factor + definition.offset_value;

    for (const auto &candidate : candidates)
    {
        if (candidate.parameter_value.has_value() &&
            std::abs(candidate.parameter_value.value() - decoded_value) < 0.0001 &&
            candidate.parameter_display_value.has_value())
        {
            return SignalValue(
                candidate.parameter_name,
                decoded_value,
                candidate.parameter_display_value,
                get_unit_name(candidate.unit_type));
        }
    }

    return SignalValue(
        definition.parameter_name,
        decoded_value,
        std::nullopt,
        get_unit_name(definition.unit_type));
}

int get_max_required_payload_size(const std::unordered_map<std::tuple<uint16_t, int, int>, std::vector<UdsDefinition>> &grouped_defs)
{
    int max_required = 0;

    for (const auto &[key, defs] : grouped_defs)
    {
        const auto &def = defs.front(); // all in group share position
        int bit_end = def.bit_offset_position + def.bit_length;
        int byte_span = (bit_end + 7) / 8; // ceil
        int end_pos = def.byte_position + byte_span;

        if (end_pos > max_required)
            max_required = end_pos;
    }

    return max_required;
}

/**
 * @brief Extracts all unique signal values from a UDS response message.
 *
 * Groups definitions by a composite key (parameter_id_hex, byte_position, bit_offset_position)
 * and extracts one signal per group.
 *
 * @param data      Raw CAN data buffer (already reassembled via ISO-TP)
 * @param data_len  Length of the CAN data buffer
 * @param groups    Map of (parameter_id_hex, byte_position, bit_offset_position) → list of UdsDefinitions
 * @return          Vector of extracted SignalValue objects
 */
std::vector<SignalValue> get_signal_values(
    const uint8_t *data,
    int data_len,
    const std::unordered_map<std::tuple<uint16_t, int, int>, std::vector<UdsDefinition>> &groups)
{
    std::vector<SignalValue> signals;

    for (const auto &[key, defs] : groups)
    {
        // Use the first definition as base for decoding
        const auto &base_def = defs.front();

        auto sig = get_signal_value(base_def, data, data_len, defs);

        if (sig.has_value())
            signals.push_back(*sig);
    }

    return signals;
}

/**
 * Pretty-logs a list of SignalValues along with the original raw CAN message data.
 *
 * @param signals   List of decoded signal values
 * @param data      Original CAN frame data (from UDS response)
 * @param len       Length of the data buffer
 * @param can_id    CAN ID of the sender (e.g. ECU response ID)
 */
void log_signals(const std::vector<SignalValue> &signals, const uint8_t *data, size_t len, uint32_t can_id)
{
    // Format the CAN data as a hex string
    char data_hex[64] = {0};
    size_t pos = 0;
    for (size_t i = 0; i < len && pos < sizeof(data_hex) - 3; ++i)
        pos += snprintf(&data_hex[pos], sizeof(data_hex) - pos, "%02X ", data[i]);
    if (pos > 0 && data_hex[pos - 1] == ' ')
        data_hex[pos - 1] = '\0';

    // Header line with CAN ID and raw data
    Logger::debug("[log_signals] [CAN: %03X] Data: %s", can_id, data_hex);

    // Individual decoded signals
    for (const auto &sig : signals)
    {
        if (sig.display_value.has_value())
            Logger::debug("  %s = %s", sig.parameter_name.c_str(), sig.display_value->c_str());
        else if (sig.unit.has_value())
            Logger::debug("  %s = %.2f %s", sig.parameter_name.c_str(), sig.value, sig.unit.value());
        else
            Logger::debug("  %s = %.2f", sig.parameter_name.c_str(), sig.value);
    }
}

bool IsfService::transformResponse(uint8_t *data, uint8_t length, const UDSRequest &request)
{
    // === Lookup all UDS definitions for this (tx_id, did) pair ===
    auto range = udsMap.equal_range(std::make_tuple(request.tx_id, request.did));

    using GroupKey = std::tuple<uint16_t, int, int>;
    std::unordered_map<GroupKey, std::vector<UdsDefinition>> grouped;

    for (auto it = range.first; it != range.second; ++it)
    {
        const auto &def = it->second;
        GroupKey key = std::make_tuple(def.parameter_id_hex, def.byte_position, def.bit_offset_position);
        grouped[key].push_back(def);
    }

    if (grouped.empty())
    {
        Logger::warn("[IsfService:transformResponse] No UDS definitions for TX: %04X, DID: %04X", request.tx_id, request.did);
        return false;
    }

    // === Payload size validation ===
    int required_len = get_max_required_payload_size(grouped);
    if (length < required_len)
    {
        Logger::warn("[IsfService:transformResponse] Buffer too short. TX: %04X, DID: %04X → Needed: %d bytes, Got: %d", request.tx_id, request.did, required_len, length);
        return false;
    }

    // === Extract one signal per bitfield group ===
    std::vector<SignalValue> signals = get_signal_values(data, length, grouped);

    if (signals.empty())
    {
        Logger::warn("[IsfService:transformResponse] No signal values extracted for TX: %04X, DID: %04X", request.tx_id, request.did);
        return false;
    }

    // === Log each signal with raw CAN data for traceability ===
    log_signals(signals, data, length, request.rx_id);

    return true;
}

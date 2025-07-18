#include "isf_service.h"
#include "../can/twai_wrapper.h"
#include "../logger/logger.h"
#include "../uds/uds_mapper.h"
#include "../isotp/iso_tp.h"
#include <algorithm>
#include <cstdint>          // <-- NEW
#include <string>
#include <unordered_map>
#include <tuple>
#include <optional>

/**
 * @brief Represents a decoded signal value from a CAN message
 * 
 * Contains all the metadata about the signal as well as the decoded value
 * and formatted display value if available.
 */
struct SignalValue {
    std::uint16_t obd2_request_id_hex;
    std::uint16_t parameter_id_hex;
    std::uint16_t uds_data_identifier_hex;
    std::string parameter_name;
    double value;
    std::optional<std::string> display_value;
    std::optional<const char*> unit_name;

    // Constructor for signal values
    SignalValue(
        std::uint16_t obd2_id,
        std::uint16_t param_id,
        std::uint16_t data_id,
        std::string name,
        double val,
        std::optional<std::string> display,
        std::optional<const char*> unit
    ) : obd2_request_id_hex(obd2_id),
        parameter_id_hex(param_id),
        uds_data_identifier_hex(data_id),
        parameter_name(std::move(name)),
        value(val),
        display_value(display),
        unit_name(unit)
    {}
};

// Custom hash function for std::tuple<std::uint16_t, int, int>
struct SignalLocationKeyHash {
    std::size_t operator()(const std::tuple<std::uint16_t, int, int>& k) const {
        auto h1 = std::hash<std::uint16_t>{}(std::get<0>(k));
        auto h2 = std::hash<int>{}(std::get<1>(k));
        auto h3 = std::hash<int>{}(std::get<2>(k));
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

IsfService::IsfService()
{
    // Initialize the array with the correct size
    lastUdsRequestTime = new unsigned long[ISF_UDS_REQUESTS_SIZE]();
}

IsfService::~IsfService()
{
    delete isotp;
    delete twai;
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

    // Create TwaiWrapper instance
    twai = new TwaiWrapper();
    if (twai == nullptr)
    {
        LOG_ERROR("Failed to create TwaiWrapper instance.");
        return false;
    }
    else
    {
        LOG_INFO("TwaiWrapper instance created successfully.");
    }

    // Initialize the TwaiWrapper directly
    bool initResult = twai->initialize();
    if (!initResult)
    {
        LOG_ERROR("TwaiWrapper initialization failed.");
        return false;
    }
    else
    {
        LOG_INFO("TwaiWrapper initialized successfully.");
    }

    // Create IsoTp instance
    isotp = new IsoTp(twai);
    if (isotp == nullptr)
    {
        LOG_ERROR("Failed to create IsoTp instance.");
        return false;
    }
    else
    {
        LOG_INFO("IsoTp instance created successfully.");
    }

    // ISO-TP already initialized

#ifdef DEBUG_ISF
    LOG_DEBUG("Running on core %d", xPortGetCoreID());
#endif
    return true;
}

/**
 * @brief Sends diagnostic session initialization messages
 *
 * Sends all predefined session initialization messages from isf_pid_session_requests
 * to prepare the ECUs for diagnostic communication.
 * Includes retry logic and appropriate delays between messages.
 *
 * @return true if all session messages were sent successfully, false otherwise
 */
bool IsfService::initialize_diagnostic_session()
{
    const int SESSION_REQUESTS_SIZE = sizeof(isf_pid_session_requests) / sizeof(CANMessage);
    
    for (int i = 0; i < SESSION_REQUESTS_SIZE; i++)
    {
        CANMessage msg = isf_pid_session_requests[i];

        LOG_INFO("Sending diagnostic session message to ID: 0x%X", msg.id);
        
        bool failed = !twai->sendMessage(msg.id, msg.data, (uint8_t)msg.len, msg.extended);
        if (failed)
        {
            LOG_ERROR("Failed to send session request for ID: 0x%X", msg.id);

            return false;
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }

    return true;
}

/**
 * @brief Main processing function that sends requests and receives messages
 *
 * This method should be called periodically from a task loop to:
 * 1. Send scheduled UDS requests to the ECU
 * 2. Process incoming CAN messages
 */
void IsfService::listen()
{
    //unsigned long current_time = millis();

    // if (current_time - last_diagnostic_session_time_ >= 3000) 
    // {
    //     if (initialize_diagnostic_session())
    //     {
    //         // Optionally, add logic here if the session initialization is successful
    //     }
    //     last_diagnostic_session_time_ = current_time;
    // }
        
    beginSend();

    vTaskDelay(pdMS_TO_TICKS(5)); 
}

bool IsfService::beginSend()
{
    if (is_session_active) {
        LOG_DEBUG("UDS request already in progress, skipping new send");
        return false;
    }

    for (int i = 0; i < ISF_UDS_REQUESTS_SIZE; i++)
    {
        UDSRequest request = isf_uds_requests[i];

        Message_t msg_to_send;
        msg_to_send.tx_id = request.tx_id;
        msg_to_send.rx_id = request.rx_id;
        msg_to_send.service_id = request.service_id;
        msg_to_send.data_id = request.did;

        uint8_t payload[8] = {0};

        switch (request.service_id)
        {
            case UDS_SID_READ_DATA_BY_ID:
                //Not Supported
                continue;

            case OBD_MODE_SHOW_CURRENT_DATA:
                //Not Supported
                continue;

            case UDS_SID_TESTER_PRESENT:
                //Not Supported
                continue;

            case UDS_SID_READ_DATA_BY_LOCAL_ID:              // Techstream SID for Local Identifier requests
                payload[0] = 0x02;                           // Length of the remaining bytes
                payload[1] = UDS_SID_READ_DATA_BY_LOCAL_ID;  // Use defined SID instead of hardcoded value
                payload[2] = isf_uds_requests[i].did & 0xFF; // Identifier (0x01, 0xE1, etc.)
                request.dataLength = 3;                      // SID + Identifier + length byte
                break;

            default:
                LOG_ERROR("Unsupported UDS service ID for ISO-TP: %02X", request.service_id);
                vTaskDelay(pdMS_TO_TICKS(1));
                continue;
        }

        // Prepare the final UDS message. Length is 1 byte for SID + data length.
        msg_to_send.length = 1 + request.dataLength;
        uint8_t uds_frame_payload[msg_to_send.length];
        uds_frame_payload[0] = request.service_id;
        memcpy(uds_frame_payload + 1, payload, request.dataLength);
        msg_to_send.Buffer = uds_frame_payload;

        sendUdsRequest(msg_to_send, request);
        
        //Delay to avoid flooding the bus
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    return true;
}

bool IsfService::sendUdsRequest(Message_t& msg, const UDSRequest &request)
{
    is_session_active = true;
    
    #ifdef DEBUG_ISF
        Logger::logUdsMessage("[sendUdsRequest] BEGIN Sending message.", &msg);
    #endif

    if (!isotp->send(&msg))
    {
        #ifdef DEBUG_ISF
            Logger::logUdsMessage("[sendUdsRequest] Error sending message.", &msg);
        #endif
        is_session_active = false;
        msg.reset();
        return false;
    }
     
    msg.reset();

    if (!isotp->receive(&msg))
    {
        #ifdef DEBUG_ISF
            Logger::logUdsMessage("[sendUdsRequest] Error receiving message.", &msg);
        #endif
        is_session_active = false;
        msg.reset();    
        return false;
    }
    
    // if (msg.length < 3) 
    // {
    //     Logger::logUdsMessage("[sendUdsRequest] Incomplete response frame.", &msg);
    //     is_session_active = false;
    //     msg.reset();
    //     return false;
    // }
    
    // Logger::logUdsMessage("[sendUdsRequest] Received response", &msg);
    // if (processUdsResponse(msg.Buffer, msg.length, request))
    // {
    //     LOG_INFO("UDS response parsed successfully. %s", request.param_name);
    //     return true;
    // }
    // else
    // {
    //     LOG_WARN("Failed to parse UDS response data. %s", request.param_name);
    //     return false;
    // }

    is_session_active = false;
    
    return true;
}

bool IsfService::processUdsResponse(uint8_t *data, uint8_t length, const UDSRequest &request)
{
    if (data == nullptr || length == 0)
    {
        LOG_ERROR("Invalid UDS response data");
        return false;
    }

    //For now we only support Read Data By Local ID and Read Data By ID
    switch (request.service_id)
    {
        case UDS_SID_READ_DATA_BY_LOCAL_ID: // Local Identifier (Techstream)
        case UDS_SID_READ_DATA_BY_ID:
            return transformResponse(data, length, request);
        default:
            LOG_ERROR("Unsupported response SID: %02X", request.service_id);
            return false;
    }
}

std::vector<UdsDefinition> get_uds_definitions(std::uint16_t request_id,
                                               std::uint16_t pid,
                                               std::uint16_t data_id)
{
    std::vector<UdsDefinition> results;

    auto range = udsMap.equal_range(std::make_tuple(request_id, pid, data_id));
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
        #ifdef DEBUG_ISF
            LOG_ERROR("Null data pointer for parameter: %s", parameter_name.c_str());
        #endif
        return 0;
    }

    if (byte_pos < 0 || byte_pos >= data_len)
    {
        #ifdef DEBUG_ISF
            LOG_ERROR("Byte position out of range: %d (needs 1 byte, buffer len = %d) for parameter: %s", byte_pos, data_len, parameter_name.c_str());
        #endif
        return 0;
    }

    int max_bytes = (bit_offset + bit_length + 7) / 8;
    if (byte_pos + max_bytes > data_len)
    {
        #ifdef DEBUG_ISF
            LOG_ERROR("Byte position out of range: %d (needs %d bytes, buffer len = %d) for parameter: %s", byte_pos, max_bytes, data_len, parameter_name.c_str());
        #endif
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
        LOG_ERROR("Null data pointer for signal: %s", definition.parameter_name.c_str());
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
                definition.obd2_request_id_hex,
                candidate.parameter_id_hex,
                definition.uds_data_identifier_hex,
                candidate.parameter_name,
                decoded_value,
                candidate.parameter_display_value,
                get_unit_name(candidate.unit_type));
        }
    }

    return SignalValue(
        definition.obd2_request_id_hex,
        definition.parameter_id_hex,
        definition.uds_data_identifier_hex,
        definition.parameter_name,
        decoded_value,
        std::nullopt,
        get_unit_name(definition.unit_type));
}

int get_max_required_payload_size(const std::unordered_map<std::tuple<std::uint16_t,int,int>,
                                                             std::vector<UdsDefinition>> &grouped_defs)
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
 * Groups definitions by a composite key (data_id, byte_position, bit_offset_position)
 * and extracts one signal per group. Handles partial responses by checking each signal
 * individually rather than requiring the full expected response length.
 *
 * @param responseData      Raw CAN data buffer (already reassembled via ISO-TP)
 * @param responseLength    Length of the CAN data buffer in bytes
 * @param signalGroups      Map of signal location keys → lists of UdsDefinitions
 * @return                  Vector of successfully extracted SignalValue objects
 */

std::vector<SignalValue> get_signal_values(
    const uint8_t *responseData,
    int responseLength,
    const std::unordered_map<std::tuple<uint16_t, int, int>, std::vector<UdsDefinition>, SignalLocationKeyHash> &signalGroups)
{
    std::vector<SignalValue> extractedSignals;

    for (const auto &[locationKey, definitionGroup] : signalGroups)
    {
        // Skip processing if the definition requires data beyond the response length
        const auto &baseDefinition = definitionGroup.front();
        int bitEnd = baseDefinition.bit_offset_position + baseDefinition.bit_length;
        int byteSpan = (bitEnd + 7) / 8; // ceil
        int endPos = baseDefinition.byte_position + byteSpan;
        
        if (endPos > responseLength) {
            LOG_DEBUG("Skipping out-of-range signal: %s (needs %d bytes, got %d)", 
                baseDefinition.parameter_name.c_str(), endPos, responseLength);
            continue; // Skip this signal group and try the next one
        }
        
        // Try to extract the signal value
        auto signalValue = get_signal_value(baseDefinition, responseData, responseLength, definitionGroup);

        if (signalValue.has_value())
        {
            extractedSignals.push_back(*signalValue);
        }
    }

    return extractedSignals;
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
    LOG_DEBUG("[CAN: %03X] Data: %s", can_id, data_hex);

    // Individual decoded signals
    for (const auto &sig : signals)
    {
        if (sig.display_value.has_value())
            LOG_DEBUG("  %s = %s", sig.parameter_name.c_str(), sig.display_value->c_str());
        else if (sig.unit_name.has_value())
            LOG_DEBUG("  %s = %.2f %s", sig.parameter_name.c_str(), sig.value, sig.unit_name.value());
        else
            LOG_DEBUG("  %s = %.2f", sig.parameter_name.c_str(), sig.value);
    }
}

/**
 * @brief Transforms a UDS response into signal values
 *
 * This method processes raw diagnostic response data from vehicle ECUs by:
 *
 * 1. Finding all signal definitions that match the request parameters
 *    - Uses (tx_id, pid, did) tuple as the lookup key in udsMap
 *    - Each definition specifies where (byte/bit position) a signal exists in the data
 *
 * 2. Grouping definitions by their physical location in the message
 *    - Groups by (data_id, byte_position, bit_offset_position)
 *    - Each group represents one physical signal that may have multiple interpretations
 *    - For example: same field could be engine temperature AND a warning level
 *
 * 3. Extracting values for each signal group
 *    - Decodes raw bytes according to position, length, scaling factors
 *    - Handles signals even if response is shorter than expected (partial response)
 *    - Some ECUs may respond with only a subset of values
 *
 * 4. Logging results for diagnostics and debugging
 *    - Logs both raw data and decoded signals
 *    - Provides units and display values where available
 *
 * This implementation is resilient to partial responses, allowing extraction
 * of signals that fit within the available data, even if some expected signals
 * are missing from the response.
 *
 * @param data      Pointer to the UDS response data buffer
 * @param length    Length of the UDS response data in bytes
 * @param request   UDS request parameters that triggered this response
 * @return true     if at least one signal was successfully extracted and processed
 * @return false    if no signals could be extracted
 */
bool IsfService::transformResponse(uint8_t *data, uint8_t length, const UDSRequest &request)
{
    // Extract DID from the response if possible
    uint16_t responseDID = request.did; // Default to requested DID
    
    // Check if we have enough data to extract the response DID
    if (length >= 3 && data[0] == (request.service_id + 0x40)) {
        
        // Standard UDS response format: [service_id+0x40][DID_MSB][DID_LSB][data...]
        if (request.service_id == UDS_SID_READ_DATA_BY_ID && length >= 4) {
            // For service 0x22 (ReadDataByID), the response has the DID in bytes 1-2
            responseDID = (data[1] << 8) | data[2];
            // Adjust response data pointer and length to skip header
            data += 3;  // Skip service ID and DID bytes
            length -= 3;
        } 
        else if (request.service_id == UDS_SID_READ_DATA_BY_LOCAL_ID) {
            // For Toyota-specific local ID (service 0x21)
            // Response format is [0x61][LocalID][data...]
            responseDID = data[1];
            // Adjust response data pointer and length to skip header
            data += 2;  // Skip service ID and local ID byte
            length -= 2;
        }
    }
    
    // Now look up definitions using the extracted response DID if possible
    auto definitionsRange = udsMap.equal_range(std::make_tuple(request.tx_id, request.pid, responseDID));

    // Group the definitions by their physical location in the data
    using SignalLocationKey = std::tuple<std::uint16_t, int, int>; // data_id, byte_pos, bit_offset
    std::unordered_map<SignalLocationKey, std::vector<UdsDefinition>, SignalLocationKeyHash> signalGroups;

    for (auto it = definitionsRange.first; it != definitionsRange.second; ++it)
    {
        const auto &signalDefinition = it->second;
        SignalLocationKey locationKey = std::make_tuple(
            signalDefinition.uds_data_identifier_hex, 
            signalDefinition.byte_position, 
            signalDefinition.bit_offset_position);
        signalGroups[locationKey].push_back(signalDefinition);
    }

    if (signalGroups.empty())
    {
        LOG_WARN("No UDS definitions for TX: %04X, DID: %04X (Response DID: %04X)", 
            request.tx_id, request.did, responseDID);
        return false;
    }

    // Skip global length validation and handle partial responses
    // Instead of requiring the entire expected payload, we'll extract what we can
    std::vector<SignalValue> extractedSignals = get_signal_values(data, length, signalGroups);

    if (extractedSignals.empty())
    {
        LOG_WARN("No signal values extracted for TX: %04X, DID: %04X", request.tx_id, request.did);
        return false;
    }

    // Log the extracted signals with the raw data for traceability
    log_signals(extractedSignals, data, length, request.rx_id);

    return true;
}

#include "isf_service.h"
#include "../can/twai_wrapper.h"
#include "../logger/logger.h"
#include "../uds/uds_mapper.h"
#include "../isotp/iso_tp.h"
#include <algorithm>
#include <cstdint> // <-- NEW
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
struct SignalValue
{
    std::uint16_t obd2_request_id_hex;
    std::uint16_t parameter_id_hex;
    std::uint16_t uds_data_identifier_hex;
    std::string parameter_name;
    double value;
    std::optional<std::string> display_value;
    std::optional<const char *> unit_name;

    // Constructor for signal values
    SignalValue(
        std::uint16_t obd2_id,
        std::uint16_t param_id,
        std::uint16_t data_id,
        std::string name,
        double val,
        std::optional<std::string> display,
        std::optional<const char *> unit) : obd2_request_id_hex(obd2_id),
                                            parameter_id_hex(param_id),
                                            uds_data_identifier_hex(data_id),
                                            parameter_name(std::move(name)),
                                            value(val),
                                            display_value(display),
                                            unit_name(unit)
    {
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

        bool failed = !twai->sendMessage(msg.id, msg.data, (uint8_t)msg.len);
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
    // unsigned long current_time = millis();

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
    if (is_session_active)
    {
        LOG_DEBUG("UDS request already in progress, skipping new send");
        return false;
    }

    for (int i = 0; i < ISF_UDS_REQUESTS_SIZE; i++)
    {
        const UDSRequest &request = isf_uds_requests[i];

        Message_t msg_to_send;
        msg_to_send.tx_id = request.tx_id;
        msg_to_send.rx_id = request.rx_id;
        msg_to_send.service_id = request.service_id;
        msg_to_send.data_id = request.did;
        msg_to_send.length = request.length;

        memcpy(msg_to_send.Buffer, request.payload, request.length);

        sendUdsRequest(msg_to_send, request);

        vTaskDelay(pdMS_TO_TICKS(100));
    }

    return true;
}

bool IsfService::sendUdsRequest(Message_t &msg, const UDSRequest &request)
{
    is_session_active = true;

    if (!isotp->send(&msg))
    {
        is_session_active = false;
        msg.reset();
        return false;
    }

    if (!isotp->receive(&msg))
    {
        is_session_active = false;
        msg.reset();
        return false;
    }

    processUdsResponse(msg, request);

    msg.reset();
    is_session_active = false;
    return true;
}

bool IsfService::processUdsResponse(Message_t &msg, const UDSRequest &request)
{
    // For now we only support Read Data By Local ID and Read Data By ID
    switch (request.service_id)
    {
    case UDS_SID_READ_DATA_BY_LOCAL_ID: // Local Identifier (Techstream)
    case UDS_SID_READ_DATA_BY_ID:
        return transformResponse(msg, request);
    default:
        LOG_ERROR("Unsupported response SID: %02X", request.service_id);
        return false;
    }
}

std::vector<UdsDefinition> get_uds_definitions(std::uint16_t request_id, std::uint16_t data_id)
{
    std::vector<UdsDefinition> results;

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
    // Validate bit length range
    if (bit_length <= 0 || bit_length > 32)
    {
#ifdef DEBUG_ISF
        LOG_ERROR("[extract_raw_data] Invalid bit_length=%d for parameter: %s",
                  bit_length, parameter_name.c_str());
#endif
        return 0;
    }

    // Validate starting byte position
    if (byte_pos < 0 || byte_pos >= data_len)
    {
#ifdef DEBUG_ISF
        LOG_ERROR("[extract_raw_data] byte_pos=%d is out of bounds (buffer length = %d) for parameter: %s",
                  byte_pos, data_len, parameter_name.c_str());
#endif
        return 0;
    }

    // Calculate how many bytes are needed
    int total_bit_offset = bit_offset + bit_length;
    int required_bytes = (total_bit_offset + 7) / 8;
    int end_byte = byte_pos + required_bytes;

    if (end_byte > data_len)
    {
#ifdef DEBUG_ISF
        LOG_ERROR("[extract_raw_data] extracting %d bits starting at bit offset %d from byte %d "
                  "requires up to byte %d, but buffer length is only %d. Parameter: %s",
                  bit_length, bit_offset, byte_pos, end_byte - 1, data_len, parameter_name.c_str());
#endif
        return 0;
    }

    // Copy exactly the bytes needed for this bitfield
    uint32_t raw = 0;
    memcpy(&raw, &data[byte_pos], required_bytes); // No more than needed

    raw >>= bit_offset;

    uint32_t mask = (bit_length == 32) ? 0xFFFFFFFF : ((1U << bit_length) - 1);
    return raw & mask;
}

std::optional<const char *> get_unit_name(uint8_t unit_type)
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

/**
 * @brief Extracts and decodes a signal value from raw UDS response data using the given UdsDefinition.
 *
 * This function interprets the raw byte buffer (`data`) based on the position, bit offset, length,
 * scaling factor, and offset specified in the `definition`. If `candidates` are provided with
 * matching decoded values and display strings (e.g., for enumerated values), the corresponding
 * display label is included in the result.
 *
 * @param definition     The UdsDefinition that specifies how to interpret the raw data.
 * @param data           Pointer to the raw byte array from the UDS response.
 * @param data_len       Length of the byte array.
 * @param candidates     A vector of possible matching UdsDefinitions (e.g., with enumerated values).
 *
 * @return std::optional<SignalValue>
 *         - A populated SignalValue if decoding is successful.
 *         - std::nullopt if decoding fails or input is invalid.
 *
 * @note The function includes range checks and logs an error if the `data` pointer is null.
 *       A fallback `SignalValue` without a display label is returned if no enumerated match is found.
 */
std::optional<SignalValue> get_signal_value(
    Message_t &msg,
    const UdsDefinition &definition)
{

    return std::nullopt;
}

/**
 * @brief Extracts all unique signal values from a UDS response message.
 *
 * Groups definitions by a composite key (data_id, byte_position, bit_offset_position)
 * and extracts one signal per group. Handles partial responses by checking each signal
 * individually rather than requiring the full expected response length.
 *
 * @return                  Vector of successfully extracted SignalValue objects
 */

std::vector<SignalValue> get_signal_values(Message_t &msg)
{
    std::vector<SignalValue> results;

    return results;
}

/**
 * Pretty-logs a list of SignalValues.
 *
 * @param signals   List of decoded signal values
 */
void log_signals(const std::vector<SignalValue> &signals)
{
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
 * @return true     if at least one signal was successfully extracted and processed
 * @return false    if no signals could be extracted
 */
bool IsfService::transformResponse(Message_t &msg, const UDSRequest &request)
{
    auto matchingDefinitions = udsMap.equal_range(std::make_tuple(msg.tx_id, msg.data_id));
    
    for (auto it = matchingDefinitions.first; it != matchingDefinitions.second; ++it)
    {
        const UdsDefinition &def = it->second;
        const uint8_t* payload = &msg.Buffer[2]; //Skip UDS Header, SID and DID
        int payload_length = msg.length - 2;
        
        uint32_t raw_value = extract_raw_data(payload, def.byte_position, def.bit_offset_position, def.bit_length, payload_length, def.name.c_str());

        if (def.is_calculated)
        {
            double calculated_value = raw_value * def.scaling_factor + def.offset_value;
            LOG_DEBUG("%s raw: %u value: %.2f", def.name.c_str(), raw_value, calculated_value);
        }
        else
        {
            auto enumRange = udsMap.equal_range(uds_key{msg.tx_id, msg.data_id});
                     
            for (auto enumIt = enumRange.first; enumIt != enumRange.second; ++enumIt)
            {
                const UdsDefinition &enumDef = enumIt->second;
                
                if (enumDef.name == def.name &&
                    enumDef.byte_position == def.byte_position &&
                    enumDef.bit_offset_position == def.bit_offset_position &&
                    enumDef.value.has_value() &&
                    enumDef.value.value() == raw_value &&
                    enumDef.display_value.has_value())
                {
                    LOG_DEBUG("%s raw: %u value: %s", def.name.c_str(), raw_value, enumDef.display_value.value().c_str());
                }
            }
        }
    }

    return true;
}

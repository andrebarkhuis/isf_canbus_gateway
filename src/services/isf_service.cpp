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
#include <set>

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
    // Use the generic sender for session init array
    return send_obd2_requests(isf_pid_session_requests, SESSION_REQUESTS_SIZE);
}

bool IsfService::send_obd2_requests(const CANMessage* requests, int count)
{
    for (int i = 0; i < count; ++i)
    {
        CANMessage msg = requests[i];

        if (!twai->sendMessage(msg.id, msg.data, msg.len))
        {
            return false;
        }

        vTaskDelay(pdMS_TO_TICKS(50));
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
    unsigned long current_time = millis();

    if (current_time - last_diagnostic_session_time_ >= 2000)
    {
        if (initialize_diagnostic_session())
        {
            last_diagnostic_session_time_ = current_time;
        }
    }

    //send_obd2_requests(isf_pid_requests, PID_REQUESTS_SIZE);

    beginSend();

    vTaskDelay(pdMS_TO_TICKS(5));
}

bool IsfService::beginSend()
{
    if (is_session_active)
    {
        return false;
    }

    is_session_active = true; // Set session active for the entire batch

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

    is_session_active = false; // Clear session active after all requests
    return true;
}

bool IsfService::sendUdsRequest(Message_t &msg, const UDSRequest &request)
{
    if (!isotp->send(&msg))
    {
        msg.reset();
        return false;
    }

    if (!isotp->receive(&msg, request.param_name))
    {
        msg.reset();
        return false;
    }

    processUdsResponse(msg, request);

    msg.reset();
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

/**
 * @brief Safely extracts raw data bits from a CAN data buffer.
 *
 * Ensures bit boundaries are valid before extracting.
 *
 * @param data            Raw data buffer (must be non-null)
 * @param byte_pos        Starting byte position to extract from
 * @param bit_offset      Bit offset within starting byte
 * @param bit_length      Total number of bits to extract
 * @param data_len        Length of the data buffer
 * @param raw_value       Output parameter - extracted raw value
 * @return true if extraction was successful, false on error
 */
bool get_raw_value(
    const uint8_t *data,
    int8_t byte_pos,
    int8_t bit_pos,
    int8_t bit_length,
    int8_t data_len,
    uint32_t &raw_value)
{
    const int MAX_BIT_LENGTH = 32; 
        
    // Validate bit length range
    if (bit_length <= 0 || bit_length > MAX_BIT_LENGTH)
    {
        LOG_ERROR("Invalid bit length: %d", bit_length);
        return false;
    }

    // Validate starting byte position
    if (byte_pos < 0 || byte_pos >= data_len)
    {
        LOG_ERROR("Invalid byte position: %d", byte_pos);
        return false;
    }

    // Calculate how many bytes are needed
    int8_t total_bit_offset = bit_pos + bit_length;
    int8_t required_bytes = (total_bit_offset + 7) / 8;
    int8_t end_byte = byte_pos + required_bytes;

    if (end_byte > data_len)
    {
        LOG_ERROR("Invalid end byte: %d", end_byte);
        return false;
    }

    // Copy exactly the bytes needed for this bitfield
    uint32_t raw = 0;
    memcpy(&raw, &data[byte_pos], required_bytes); // No more than needed

    raw >>= bit_pos;

    uint32_t mask = (bit_length == MAX_BIT_LENGTH) ? 0xFFFFFFFF : ((1U << bit_length) - 1);
    raw_value = raw & mask;
    
    return true;
}

/**
 * @brief Safely extracts a single bit from a CAN data buffer.
 *
 * This function is optimized for single-bit extraction (like MIL status, warning flags, etc.)
 * and always extracts exactly 1 bit.
 *
 * @param data            Raw data buffer (must be non-null)
 * @param byte_pos        Starting byte position to extract from
 * @param bit_offset      Bit offset within starting byte (0-7)
 * @param data_len        Length of the data buffer
 * @param bit_value       Output parameter - extracted bit value (0 or 1)
 * @param param_name      Optional parameter name for logging context
 * @return true if extraction was successful, false on error
 */
bool get_single_bit(
    const uint8_t *data,
    int8_t byte_pos,
    int8_t bit_pos,
    int8_t data_len,
    uint8_t &bit_value,
    const char *param_name)
{
    // Validate starting byte position
    if (byte_pos < 0 || byte_pos >= data_len)
    {
        LOG_ERROR("Invalid byte position: %d (param: %s)", byte_pos, (param_name ? param_name : ""));
        return false;
    }

    // Validate bit position within byte
    if (bit_pos < 0 || bit_pos > 7)
    {
        LOG_ERROR("Invalid bit position: %d (param: %s)", bit_pos, (param_name ? param_name : ""));
        return false;
    }

    // Extract the single bit
    uint8_t byte_value = data[byte_pos];
    bit_value = (byte_value >> bit_pos) & 0x01;
    
    return true;
}

/**
 * @brief Finds UnitTypeInfo by unit ID
 *
 * Searches through the unitTypeInfos array to find the entry with matching unit ID.
 * This is necessary because unit IDs are not consecutive and cannot be used as direct array indices.
 *
 * @param unit_id The unit ID to search for
 * @return Pointer to UnitTypeInfo if found, nullptr if not found
 */
const UnitTypeInfo* findUnitTypeInfo(uint8_t unit_id)
{
    for (const auto &info : unitTypeInfos) {
        if (info.id == unit_id) {
            return &info;
        }
    }
    return nullptr;
}

/**
 * @brief Retrieves the matching enum definition for a given raw value from UDS definitions.
 *
 * Searches through the UDS map for matching enum definitions that correspond to
 * the given raw value and returns the complete matching UdsDefinition.
 *
 * @param tx_id           CAN transmit ID
 * @param data_id         UDS data identifier
 * @param def             UDS definition containing position and name info
 * @param raw_value       Raw extracted value to match against enum definitions
 * @return Optional UdsDefinition containing the matching enum definition, or nullopt if not found
 */
std::optional<UdsDefinition> get_enum_value(
    uint32_t tx_id,
    uint16_t data_id,
    const UdsDefinition &def,
    uint32_t raw_value)
{
    auto enumRange = udsMap.equal_range(uds_key{tx_id, data_id});
                     
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
            return enumDef;
        }
    }
    
    return std::nullopt;
}

void logBufferHex(int byte_pos, int bit_pos, const uint8_t* buffer, size_t buffer_length) {
    
    const size_t maxOutputLen = 64 + buffer_length * 5; // generous headroom
    char output[maxOutputLen];
    size_t pos = snprintf(output, sizeof(output), "byte_pos: %d, bit_pos: %d, buffer_length: %zu |", byte_pos, bit_pos, buffer_length);
  
    for (size_t i = 0; i < buffer_length && pos < sizeof(output) - 5; ++i) {
      pos += snprintf(output + pos, sizeof(output) - pos, " 0x%02X", buffer[i]);
    }
  
    // Optional newline
    if (pos < sizeof(output) - 2) {
      output[pos++] = '\n';
      output[pos] = '\0';
    }
  
    LOG_DEBUG("%s", output);
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
    bool at_least_one_success = false;
    
    // Track processed (byte_position, bit_offset_position) pairs to avoid duplicates
    std::set<std::pair<int8_t, int8_t>> processedPositions;
    
    for (auto it = matchingDefinitions.first; it != matchingDefinitions.second; ++it)
    {
        const UdsDefinition &def = it->second;
        const uint8_t* payload = &msg.Buffer[2]; //Skip UDS Header, SID and DID
                
        //logBufferHex(def.byte_position, def.bit_offset_position, payload, msg.length);

        // Check if this byte/bit position has already been processed
        std::pair<int8_t, int8_t> position = {def.byte_position, def.bit_offset_position};
        if (processedPositions.find(position) != processedPositions.end())
        {
            continue; // Skip this definition as we've already processed this position
        }
        
        // Mark this position as processed
        processedPositions.insert(position);

        const UnitTypeInfo* unit_info = findUnitTypeInfo(def.unit);
        if(unit_info == nullptr)
        {
            LOG_ERROR("Unit type not found for unit %d", def.unit);
            continue;
        }

        switch (unit_info->valueType)
        {
            case ValueType::UInt16:
            case ValueType::UInt32:
            case ValueType::Float:
            {
                uint32_t raw_value;
                if (!get_raw_value(payload, def.byte_position, def.bit_offset_position,4, msg.length, raw_value))
                {
                    continue; // Skip this definition if extraction failed
                }
                // Has decimals

                float calculated_value = (float)raw_value * def.scaling_factor + def.offset_value;

                if (unit_info->minValue.has_value() && calculated_value < unit_info->minValue.value())
                {
                    calculated_value = unit_info->minValue.value();
                }
                if (unit_info->maxValue.has_value() && calculated_value > unit_info->maxValue.value())
                {
                    calculated_value = unit_info->maxValue.value();
                }

                LOG_DEBUG("%s raw: %u value: %f", def.name.c_str(), raw_value, calculated_value);

                at_least_one_success = true;
                break;
            }
            case ValueType::Boolean:
            {
                uint8_t bit_value;
                if (!get_single_bit(payload, def.byte_position, def.bit_offset_position, msg.length, bit_value, request.param_name))
                {
                    continue; // Skip this definition if bit extraction failed
                }

                auto udsDefinitionValue = get_enum_value(msg.tx_id, msg.data_id, def, bit_value);

                if (udsDefinitionValue.has_value())
                {
                     LOG_DEBUG("%s raw: %u value: %s", def.name.c_str(), bit_value, udsDefinitionValue.value().display_value.value().c_str());
                }
                
                at_least_one_success = true;
                break;
            }
            default:
            {
                LOG_ERROR("%s Unknown value type: %d", def.name.c_str(), unit_info->valueType);
                break;
            }
        }

    }

    return at_least_one_success;
}

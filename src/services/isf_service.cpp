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
    int8_t byte_pos,
    int8_t bit_pos,
    int8_t bit_length,
    int8_t data_len,
    const std::string &parameter_name)
{
    const int MAX_BIT_LENGTH = 32; 
    
    // Validate bit length range
    if (bit_length <= 0 || bit_length > MAX_BIT_LENGTH)
    {
#ifdef DEBUG_ISF
        LOG_ERROR("[extract_raw_data] Invalid bit_length=%d for parameter: %s", bit_length, parameter_name.c_str());
#endif
        return 0;
    }

    // Validate starting byte position
    if (byte_pos < 0 || byte_pos >= data_len)
    {
#ifdef DEBUG_ISF
        LOG_ERROR("[extract_raw_data] byte_pos=%d is out of bounds (buffer length = %d) for parameter: %s", byte_pos, data_len, parameter_name.c_str());
#endif
        return 0;
    }

    // Calculate how many bytes are needed
    int8_t total_bit_offset = bit_pos + bit_length;
    int8_t required_bytes = (total_bit_offset + 7) / 8;
    int8_t end_byte = byte_pos + required_bytes;

    if (end_byte > data_len)
    {
#ifdef DEBUG_ISF
        LOG_ERROR("[extract_raw_data] extracting %d bits starting at bit offset %d from byte %d requires up to byte %d, but buffer length is only %d. Parameter: %s", bit_length, bit_pos, byte_pos, end_byte - 1, data_len, parameter_name.c_str());
#endif
        return 0;
    }

    // Copy exactly the bytes needed for this bitfield
    uint32_t raw = 0;
    memcpy(&raw, &data[byte_pos], required_bytes); // No more than needed

    raw >>= bit_pos;

    uint32_t mask = (bit_length == MAX_BIT_LENGTH) ? 0xFFFFFFFF : ((1U << bit_length) - 1);
    return raw & mask;
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
    
    for (auto it = matchingDefinitions.first; it != matchingDefinitions.second; ++it)
    {
        const UdsDefinition &def = it->second;
        const uint8_t* payload = &msg.Buffer[2]; //Skip UDS Header, SID and DID
                
        //logBufferHex(def.byte_position, def.bit_offset_position, payload, msg.length);

        const UnitTypeInfo &unit_info = unitTypeInfos.at(def.unit);

        switch (unit_info.valueType)
        {
            case ValueType::Float:
            {
                //TODO: Implement float calculation
                break;
            }
            case ValueType::UInt16:
            {
                // int multivalue like gear position

                uint16_t calculated_value = extract_uint16_data(payload, def.byte_position, def.bit_offset_position, def.bit_length, msg.length, def.name.c_str());
                break;
            }
            case ValueType::UInt32:
            {
                //Calculated value, speed, temp, etc
                uint32_t calculated_value = extract_uint32_data(payload, def.byte_position, def.bit_offset_position, def.bit_length, msg.length, def.name.c_str());
                break;
            }
            case ValueType::Boolean:
            {
                //Single bit value, MIL, etc
                bool calculated_value = extract_single_bit(payload, def.byte_position, def.bit_offset_position, def.bit_length, msg.length, def.name.c_str());
                break;
            }
        }

        if (def.is_calculated)
        {
          
            //LOG_DEBUG("%s raw: %u value: %.2f", def.name.c_str(), raw_value, calculated_value);
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
                    //LOG_DEBUG("%s raw: %u value: %s", def.name.c_str(), raw_value, enumDef.display_value.value().c_str());
                }
            }
        }
    }

    return true;
}

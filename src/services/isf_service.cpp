#include <algorithm>
#include "isf_service.h"
#include "../mcp_can/mcp_can.h"
#include "../logger/logger.h"
#include "../uds/uds_mapper.h"
#include "../iso_tp/iso_tp.h"
#include "../uds/uds.h"

#include "../common_types.h"
#include "../message_translator.h"

#define BUFFER_SIZE 256 // Define BUFFER_SIZE for log_signals function

bool sessionActive = false;

IsfService::IsfService()
{
    lastUdsRequestTime = nullptr;
}

IsfService::~IsfService()
{
    delete uds;
    delete isotp;
    delete mcp;
    if (lastUdsRequestTime)
    {
        delete[] lastUdsRequestTime;
        lastUdsRequestTime = nullptr;
    }
}

bool IsfService::initialize()
{
    // Initialize UDS response mappings
    init_udsDefinitions();

    // Create MCP_CAN with specific CS pin
    mcp = new MCP_CAN();
    if (mcp == nullptr)
    {
        Logger::error("[IsfService::initialize] Failed to create MCP_CAN instance.");
        return false;
    }
    else
    {
        Logger::info("[IsfService::initialize] MCP_CAN instance created successfully.");
    }

    // Initialize the MCP_CAN directly
    byte initResult = mcp->begin();
    if (initResult != CAN_OK)
    {
        Logger::error("[IsfService::initialize] MCP_CAN initialization failed. Error code: %d", initResult);
        return false;
    }
    else
    {
        Logger::info("[IsfService::initialize] MCP_CAN initialized successfully. Code: %d", initResult);
    }

    // Create IsoTp instance
    isotp = new IsoTp(mcp);
    if (isotp == nullptr)
    {
        Logger::error("[IsfService::initialize] Failed to create IsoTp instance.");
        return false;
    }
    else
    {
        Logger::info("[IsfService::initialize] IsoTp instance created successfully.");
    }

    // Create UDS instance
    uds = new UDS(isotp);
    if (uds == nullptr)
    {
        Logger::error("[IsfService::initialize] Failed to create UDS instance.");
        return false;
    }
    else
    {
        Logger::info("[IsfService::initialize] UDS instance created successfully.");
    }

    if (lastUdsRequestTime)
        delete[] lastUdsRequestTime;
    lastUdsRequestTime = new (std::nothrow) unsigned long[ISF_MESSAGES_SIZE]();
    if (!lastUdsRequestTime)
    {
        Logger::error("[IsfService::initialize] Failed to allocate lastUdsRequestTime array.");
        return false;
    }

#ifdef DEBUG_ISF
    Logger::debug("[IsfService::initialize] Running on core %d", xPortGetCoreID());
#endif
    return true;
}

bool IsfService::initialize_diagnostic_session()
{
    for (size_t i = 0; i < ISF_SESSION_MESSAGE_SIZE; ++i)
    {
        /* ---------- build & send the session‑control request -------- */
        const UDSRequest &req = isf_session_messages[i];

        Message_t msg{};
        msg.tx_id = req.tx_id;                   // 0x7DF or 0x7E0
        msg.rx_id = req.rx_id;                   // 0x7E8 (first ECU you expect)

        uint8_t buffer[8] = {0};
        msg.len    = buildRequestPayload(req, buffer);   // makes 02 10 81
        msg.Buffer = buffer;

        Logger::info("[IsfService] Session request -> 0x%03X (%s)", msg.tx_id, req.param_name);

        uint16_t retval = uds->Session(&msg);    // TX + wait for reply

        /* ---------- look for a positive response 0x50 xx xx --------- */
        const uint8_t POSITIVE_SID = req.service_id | 0x40; // 0x10 → 0x50

        if (retval == UDS_NRC_SUCCESS &&
            msg.len >= 3 &&
            msg.Buffer[1] == POSITIVE_SID)
        {
            uint8_t session_id = msg.Buffer[2];          // e.g. 0x81
            Logger::info("[IsfService] ECU 0x%03X accepted session 0x%02X (%s)", msg.rx_id, session_id, req.param_name);

            /* ---- patch functional requests to the now‑known IDs ---- */
            for (size_t j = 0; j < ISF_MESSAGES_SIZE; ++j)
            {
                if (isf_messages[j].tx_id == req.tx_id &&
                    (isf_messages[j].rx_id == 0x000 ||  // functional
                     isf_messages[j].rx_id == req.rx_id))
                {
                    isf_messages[j].tx_id = msg.rx_id - 8; // 0x7E0 / 7E1 …
                    isf_messages[j].rx_id = msg.rx_id;     // 0x7E8 / 7E9 …
                }
            }

            sessionActive = true;
            return true;
        }
        else
        {
            Logger::warn("[IsfService] ECU 0x%03X rejected session (retval=0x%04X, SID=0x%02X)", req.rx_id, retval, (msg.len ? msg.Buffer[1] : 0xFF));
        }

        vTaskDelay(pdMS_TO_TICKS(20));                    // small gap
    }

    Logger::error("[IsfService] No ECU accepted a diagnostic session");
    return false;
}


void IsfService::listen()
{
#ifdef LED_BUILTIN
    digitalWrite(LED_BUILTIN, HIGH);
#endif

    if (!sessionActive)
    {
        initialize_diagnostic_session();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    else
    {
        beginSend();
    }

#ifdef LED_BUILTIN
    digitalWrite(LED_BUILTIN, LOW);
#endif

    vTaskDelay(pdMS_TO_TICKS(5));
}

bool IsfService::beginSend()
{
    unsigned long currentTime = millis();
    for (int i = 0; i < ISF_MESSAGES_SIZE; i++)
    {
        if (currentTime - lastUdsRequestTime[i] >= isf_messages[i].interval)
        {
            const UDSRequest &req = isf_messages[i];
            switch (req.service_id)
            {
            case UDS_SID_READ_DATA_BY_ID:
            case UDS_SID_READ_DATA_BY_LOCAL_ID:
            case UDS_SID_TESTER_PRESENT:
                sendUdsRequest(req);
                break;
            case OBD_MODE_SHOW_CURRENT_DATA:
                sendPidRequest(req);
                break;
            default:
                continue;
            }
            lastUdsRequestTime[i] = currentTime;
        }
    }
    return true;
}

uint8_t IsfService::buildRequestPayload(const UDSRequest &req, uint8_t *out)
{
    switch (req.service_id)
    {
    /* ----------- ISO‑14229 Diagnostic‑Session‑Control (0x10) -------- */
    case UDS_SID_DIAGNOSTIC_SESSION_CONTROL:      // 0x10
        out[0] = 0x02;                    // length
        out[1] = 0x10;                    // SID
        out[2] = req.did & 0xFF;          // sub‑function, e.g. 0x81
        memset(out + 3, 0, 5);
        return 8;

    /* ----------------- Toyota private Read‑Data‑By‑LocalID ---------- */
    case UDS_SID_READ_DATA_BY_LOCAL_ID:           // 0x21
        out[0] = 0x02;                    // **NOT 0x03** – only 1‑byte ID
        out[1] = 0x21;
        out[2] = req.did & 0xFF;          // Local ID
        memset(out + 3, 0, 5);
        return 8;

    /* -------------- Standard Read‑Data‑By‑Identifier (0x22) --------- */
    case UDS_SID_READ_DATA_BY_ID:                 // 0x22
        out[0] = 0x03;                    // two‑byte identifier
        out[1] = 0x22;
        out[2] = (req.did >> 8) & 0xFF;   // high
        out[3] = req.did & 0xFF;          // low
        memset(out + 4, 0, 4);
        return 8;

    /* ------------------------ Tester‑Present ------------------------ */
    case UDS_SID_TESTER_PRESENT:                  // 0x3E
        out[0] = 0x02;
        out[1] = 0x3E;
        out[2] = 0x00;                   // “alive” sub‑function
        memset(out + 3, 0, 5);
        return 8;

    /* -------------------------- OBD‑II PID -------------------------- */
    case OBD_MODE_SHOW_CURRENT_DATA:              // 0x01
        out[0] = 0x02;
        out[1] = 0x01;
        out[2] = req.did & 0xFF;          // 1‑byte PID
        memset(out + 3, 0, 5);
        return 8;

    default:
        return 0;
    }
}

void IsfService::sendUdsRequest(const UDSRequest &request)
{
    Message_t msg;
    msg.rx_id = request.rx_id;
    msg.tx_id = request.tx_id;

    uint8_t buffer[8];
    msg.len = buildRequestPayload(request, buffer);
    msg.Buffer = buffer;

    vTaskDelay(pdMS_TO_TICKS(1));

    uint16_t retval = uds->Session(&msg);

    if (retval == UDS_NRC_SUCCESS)
    {
        processUdsResponse(msg, request);
    }
    else
    {
        Logger::logUdsMessage("[ERROR] IsfService::sendUdsRequest. UDS response", &msg);
    }
}

void IsfService::sendPidRequest(const UDSRequest &request)
{
    uint8_t payload[8];
    uint8_t len = buildRequestPayload(request, payload);

    bool failed = mcp->sendMsgBuf(request.tx_id, 0, len, payload);

    Logger::logCANMessage("[IsfService::sendPidRequest]", request.tx_id, payload, len, !failed, true);
}

bool IsfService::processUdsResponse(Message_t &msg, const UDSRequest &request)
{
    vTaskDelay(pdMS_TO_TICKS(1));

    if (msg.Buffer == nullptr || msg.len == 0)
    {
        Logger::error("[IsfService::processUdsResponse] Invalid UDS response data");
        return false;
    }

    switch (request.service_id)
    {
    case UDS_SID_READ_DATA_BY_LOCAL_ID: // Local Identifier (Techstream)
    case UDS_SID_READ_DATA_BY_ID:
        return transformResponse(msg, request);
    default:
        Logger::error("[IsfService::processUdsResponse] Unsupported response SID: %02X", request.service_id);
        return false;
    }
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
        Logger::error("[IsfService::extract_raw_data] Null data pointer for parameter: %s", parameter_name.c_str());
        return 0;
    }

    if (byte_pos < 0 || byte_pos >= data_len)
    {
        Logger::error("[IsfService::extract_raw_data] Byte position out of range: %d (needs 1 byte, buffer len = %d) for parameter: %s",
                      byte_pos, data_len, parameter_name.c_str());
        return 0;
    }

    int max_bytes = (bit_offset + bit_length + 7) / 8;
    if (byte_pos + max_bytes > data_len)
    {
        Logger::error("[IsfService::extract_raw_data] Byte position out of range: %d (needs %d bytes, buffer len = %d) for parameter: %s",
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
        Logger::error("[IsfService::get_signal_value] Null data pointer for signal: %s", definition.parameter_name.c_str());
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
                raw_value,
                candidate.parameter_display_value,
                get_unit_name(candidate.unit_type));
        }
    }

    return SignalValue(
        definition.parameter_name,
        decoded_value,
        raw_value,
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
    SemaphoreHandle_t serialMutex = Logger::getMutex();

    if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE)
    {
        for (const auto &sig : signals)
        {
            char buffer[BUFFER_SIZE];
            snprintf(buffer, sizeof(buffer), "%s, Val: %.2f, RawVal: %lu%s%s", // Use %lu for long unsigned int
                     sig.parameter_name.c_str(),
                     sig.value,
                     sig.raw_value,
                     sig.display_value.has_value() ? (", DisplyVal: " + *sig.display_value).c_str() : "",
                     sig.unit.has_value() ? (", Unit: " + *sig.unit).c_str() : "");
            Serial.println(buffer);
        }
        xSemaphoreGive(serialMutex);
    }
}

bool IsfService::transformResponse(Message_t &msg, const UDSRequest &request)
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
        Logger::warn("[IsfService::transformResponse] No UDS definitions for TX: %04X, DID: %04X", request.tx_id, request.did);
        return false;
    }

    // === Payload size validation ===
    int required_len = get_max_required_payload_size(grouped);
    if (msg.len < required_len)
    {
        return false;
    }

    // === Extract one signal per bitfield group ===
    std::vector<SignalValue> signals = get_signal_values(msg.Buffer, msg.len, grouped);

    if (signals.empty())
    {
        Logger::warn("[IsfService::transformResponse] No signal values extracted for TX: %04X, DID: %04X", request.tx_id, request.did);
        return false;
    }

    // === Log each signal with raw CAN data for traceability ===
    log_signals(signals, msg.Buffer, msg.len, request.rx_id);

    return true;
}

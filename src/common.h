#pragma once

#include <cstdint>
#include <string>

struct CANMessage
{
  uint32_t id;
  uint8_t data[8];
  uint8_t len;
  bool extended = false;
  uint32_t interval = 0;
  std::string param_name; // optional-like, default is empty string
};

typedef enum
{
  ISOTP_IDLE = 0,
  ISOTP_SEND,
  ISOTP_SEND_FF,
  ISOTP_SEND_CF,
  ISOTP_WAIT_FIRST_FC,
  ISOTP_WAIT_FC,
  ISOTP_WAIT_DATA,
  ISOTP_FINISHED,
  ISOTP_ERROR
} isotp_states_t;

struct Message_t
{
  uint16_t len = 0;
  isotp_states_t tp_state = ISOTP_IDLE;
  uint16_t seq_id = 1;
  uint8_t fc_status = 0; // Clear to send
  uint8_t blocksize = 0;
  uint8_t min_sep_time = 0;
  uint32_t tx_id = 0; // Changed INT32U to uint32_t
  uint32_t rx_id = 0; // Changed INT32U to uint32_t
  uint8_t *Buffer;

  std::string getStateStr() const
  {
    switch (tp_state)
    {
    case ISOTP_IDLE:
      return "ISOTP_IDLE";
    case ISOTP_SEND:
      return "ISOTP_SEND";
    case ISOTP_SEND_FF:
      return "ISOTP_SEND_FF";
    case ISOTP_SEND_CF:
      return "ISOTP_SEND_CF";
    case ISOTP_WAIT_FIRST_FC:
      return "ISOTP_WAIT_FIRST_FC";
    case ISOTP_WAIT_FC:
      return "ISOTP_WAIT_FC";
    case ISOTP_WAIT_DATA:
      return "ISOTP_WAIT_DATA";
    case ISOTP_FINISHED:
      return "ISOTP_FINISHED";
    case ISOTP_ERROR:
      return "ISOTP_ERROR";
    default:
      return "UNKNOWN_STATE";
    }
  }
};

struct UDSRequest
{
  uint32_t tx_id;
  uint32_t rx_id;
  uint8_t service_id;
  uint16_t did;
  unsigned long interval;
  const char *param_name;
};

namespace ISFCAN
{
  // Standard ISF CAN IDs
  constexpr uint32_t RPM = 0x2C4;
  constexpr uint32_t VEHICLE_SPEED = 0xB4;
  constexpr uint32_t ENGINE_TEMP = 0x360;
  constexpr uint32_t THROTTLE_POSITION = 0x288;
  constexpr uint32_t TRANSMISSION_DATA = 0x340;

  // OBD-II Diagnostic IDs
  constexpr uint32_t OBD_ECU_REQUEST_ID = 0x7E0;
  constexpr uint32_t OBD_ECU_RESPONSE_ID = 0x7E8;
}

namespace GT86CAN
{
  // Engine & Powertrain
  constexpr uint32_t ENGINE_DATA = 0x140;
  constexpr uint32_t ENGINE_TEMP = 0x141;
  constexpr uint32_t GEAR_POSITION = 0x142;

  // Vehicle Dynamics
  constexpr uint32_t VEHICLE_SPEED = 0xD1;
  constexpr uint32_t WHEEL_SPEEDS = 0xD4;

  // Vehicle Systems
  constexpr uint32_t HVAC_STATUS = 0x220;
  constexpr uint32_t LIGHT_STATUS = 0x280;
}
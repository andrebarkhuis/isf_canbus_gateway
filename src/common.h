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
  //Expected length of the complete uds message with all its Consecutive Frames included.
  uint16_t length = 0;
  //The current Sequence number of the consecutive frame received.
  uint8_t sequence_number = 1;
  //The next expected sequence number for consecutive frames
  uint8_t next_sequence = 1;
  //Tracks how many bytes have been received so far
  uint16_t bytes_received = 0;
  //Tracks how many bytes still need to be received
  uint16_t remaining_bytes = 0;
  //The block size of the consecutive frame received.
  uint8_t blocksize = 0;
  //The tx_id of the message.
  uint32_t tx_id = 0;
  //The rx_id of the message.
  uint32_t rx_id = 0;
  //The service_id of the message.
  uint8_t service_id = 0;
  //The data_id of the message.
  uint16_t data_id = 0;
  //The buffer of the message.
  uint8_t *Buffer;
  //The state of the message.
  isotp_states_t tp_state = ISOTP_IDLE;

  void reset()
  {
    length = 0;
    sequence_number = 1;
    next_sequence = 1;
    bytes_received = 0;
    remaining_bytes = 0;
    blocksize = 0;
    tx_id = 0;
    rx_id = 0;
    service_id = 0;
    data_id = 0;
    Buffer = nullptr;
    tp_state = ISOTP_IDLE;
  }


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
  uint32_t tx_id; // ecu_id
  uint32_t rx_id;
  uint8_t service_id; 
  uint16_t pid = 0; // using 0 will match all pids in the map
  uint16_t did = 0; // using 0 will match all dids in the map
  unsigned long interval;
  const char *param_name;
  uint8_t dataLength; // Length of the UDS message payload (e.g., DID, PID, etc.)
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
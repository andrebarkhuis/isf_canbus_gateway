#include "common_types.h"
#include <string>

// Implementation for Message_t::getStateStr()
std::string Message_t::getStateStr() const {
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
      return "UNKNOWN";
    }
}

#include <SPI.h>
#include <Arduino.h>
#include "iso_tp.h"
#include "../can/twai_wrapper.h"
#include "../logger/logger.h"


IsoTp::IsoTp(TwaiWrapper *bus)
{
  _twaiWrapper = bus;
}


// Convert UDS error code to string representation
const char* getUdsErrorString(uint8_t errorCode) {
  switch(errorCode) {
    case UDS_NRC_SUCCESS: return "UDS_NRC_SUCCESS";
    case UDS_NRC_SERVICE_NOT_SUPPORTED: return "UDS_NRC_SERVICE_NOT_SUPPORTED";
    case UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED: return "UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED";
    case UDS_NRC_INCORRECT_LENGTH_OR_FORMAT: return "UDS_NRC_INCORRECT_LENGTH_OR_FORMAT";
    case UDS_NRC_CONDITIONS_NOT_CORRECT: return "UDS_NRC_CONDITIONS_NOT_CORRECT";
    case UDS_NRC_REQUEST_OUT_OF_RANGE: return "UDS_NRC_REQUEST_OUT_OF_RANGE";
    case UDS_NRC_SECURITY_ACCESS_DENIED: return "UDS_NRC_SECURITY_ACCESS_DENIED";
    case UDS_NRC_INVALID_KEY: return "UDS_NRC_INVALID_KEY";
    case UDS_NRC_TOO_MANY_ATTEMPS: return "UDS_NRC_TOO_MANY_ATTEMPS";
    case UDS_NRC_TIME_DELAY_NOT_EXPIRED: return "UDS_NRC_TIME_DELAY_NOT_EXPIRED";
    case UDS_NRC_RESPONSE_PENDING: return "UDS_NRC_RESPONSE_PENDING";
    default: return "Unknown Error";
  } 
}

bool IsoTp::is_next_consecutive_frame(Message_t *msg, uint8_t actual_seq_num)
{
  return actual_seq_num == msg->next_sequence;
}

bool IsoTp::handle_first_frame(Message_t *msg, uint8_t rxBuffer[])
{ 
  // Correct ISO-TP length calculation: 12 bits from first frame
  // First 4 bits from rxBuffer[0] & 0x0F (high nibble)
  // Last 8 bits from rxBuffer[1] (low byte)
  uint16_t expected_length = ((rxBuffer[0] & 0x0F) << 8) | rxBuffer[1];
  
  //8,10,30,61,21,00,00,00,00

  msg->length = expected_length;
  msg->tp_state = ISOTP_WAIT_FC;
  msg->bytes_received = 6; // First frame already contains 6 bytes
  msg->remaining_bytes = expected_length - 6; // Remaining bytes to receive
  msg->sequence_number = 0; // First CF will have sequence number 1;
  msg->next_sequence = 1;

  /* copy the first received data bytes */
  memcpy(msg->Buffer, rxBuffer + 2, 6);  // Copy first data chunk (after PCI)

  #ifdef ISO_TP_DEBUG
    uint8_t ff_service_id = rxBuffer[2];
    uint16_t ff_data_id = rxBuffer[3];  
    LOG_DEBUG("First-frame received: iso_tp_state=%s, tx_id=0x%lX, rx_id=0x%lX, length=%u, bytes_received=%u, remaining=%u, service_id=0x%X, data_id=0x%X", 
      msg->getStateStr().c_str(), msg->tx_id, msg->rx_id, expected_length, 
      msg->bytes_received, msg->remaining_bytes, ff_service_id, ff_data_id);
  #endif

  if (!send_flow_control(msg->tx_id))
  {
    msg->tp_state = ISOTP_ERROR;

    LOG_ERROR("Flow-Control Frame: tx_id=0x%lX", msg->tx_id);
    return false;
  }
  
  return true;
}

bool IsoTp::handle_single_frame(Message_t *msg, uint8_t rxBuffer[])
{
  uint8_t service_id = rxBuffer[1];
  uint16_t data_id = 0;
  
  msg->length = rxBuffer[0] & 0x0F;
  msg->service_id = service_id;
  msg->data_id = data_id;
  msg->tp_state = ISOTP_FINISHED;
  
  if (msg->length >= 3) {
    data_id = (rxBuffer[2] << 8) | (msg->length >= 4 ? rxBuffer[3] : 0);
  }
  
  //Read the data into the buffer
  memcpy(msg->Buffer, rxBuffer + 1, msg->length); // Skip PCI, SF uses len bytes
    
  #ifdef ISO_TP_DEBUG
    LOG_DEBUG("Single frame received: iso_tp_state=%s, tx_id=0x%lX, rx_id=0x%lX, length=%u, service_id=0x%X, data_id=0x%X", msg->getStateStr().c_str(), msg->tx_id, msg->rx_id, msg->length, service_id, data_id);
  #endif

  return true;
}

bool IsoTp::send_flow_control(uint32_t rx_id)
{
  // FC example: 30,00,00,00,00,00,00,00

  #ifdef ISO_TP_DEBUG
    LOG_DEBUG("Flow-Control Frame: rx_id=0x%lX", rx_id);
  #endif

  uint8_t TxBuf[8] = {0};

  TxBuf[0] = (N_PCI_FC | ISOTP_FC_CTS); // Explicitly Clear To Send (0x30)
  TxBuf[1] = 0x00;                      // No block size limit, send all data
  TxBuf[2] = 0x01;                      // 1ms separation time

  bool result = _twaiWrapper->sendMessage(rx_id, TxBuf, 8);
  if (!result) 
  {
    #ifdef ISO_TP_DEBUG
    LOG_ERROR("Failed to send Flow-Control Frame: rx_id: 0x%lX", rx_id);
    #endif

    return false;
  }

  return true;
}

bool IsoTp::send(Message_t *msg)
{
    #ifdef ISO_TP_DEBUG
      LOG_DEBUG("Sending UDS message: tx_id: 0x%lX, rx_id: 0x%lX, service_id: 0x%02X, length: %d", msg->tx_id, msg->rx_id, msg->service_id, msg->length);
    #endif

    if (msg->length > 7) 
    {
      msg->tp_state = ISOTP_ERROR;
      #ifdef ISO_TP_DEBUG
        LOG_ERROR("Message too long for single frame: tx_id: 0x%lX, rx_id: 0x%lX, length: %d (max: 7), state: %s", msg->tx_id, msg->rx_id, msg->length, msg->getStateStr().c_str());
      #endif
      return false; // Error, too much data for single frame
    }

    bool result = _twaiWrapper->sendMessage(msg->tx_id, msg->Buffer, 8); 
    if (!result) 
    {
      msg->tp_state = ISOTP_ERROR;
      #ifdef ISO_TP_DEBUG
        LOG_ERROR("Failed to send single-frame: tx_id: 0x%lX, rx_id: 0x%lX, service_id: 0x%02X, state: %s", msg->tx_id, msg->rx_id, msg->service_id, msg->getStateStr().c_str());
      #endif
      return false;
    }
  
    msg->tp_state = ISOTP_FINISHED;
  
    #ifdef ISO_TP_DEBUG
    Logger::logUdsMessage("IsoTp:send_single_frame", msg);
    #endif

    return true;
}

void IsoTp::handle_udsError(uint8_t serviceId, uint8_t nrc_code, const char* param_name)
{
  LOG_ERROR("UDS Negative Response for Service ID 0x%X: %s (0x%X) | param: %s", serviceId, getUdsErrorString(nrc_code), nrc_code, (param_name ? param_name : ""));
}

bool IsoTp::isSupportedDiagnosticId(uint32_t rxId) {
  return rxId >= 0x700 && rxId < 0x800;
}

bool IsoTp::handle_consecutive_frame(Message_t *msg, const uint8_t *rxBuffer, uint8_t rxLen)
{
    if (rxLen < 2) {
        // Not enough data to process a CF
        #ifdef ISO_TP_DEBUG
            LOG_WARN("CF frame too short: rxLen = %u", rxLen);
        #endif
        return false;
    }

    uint8_t sequence_num = rxBuffer[0] & 0x0F;

    if (sequence_num != msg->next_sequence) {
        #ifdef ISO_TP_DEBUG
            LOG_WARN("CF sequence mismatch: got %u, expected %u", sequence_num, msg->next_sequence);
        #endif
        return false;
    }

    uint16_t bytes_to_copy = (msg->remaining_bytes > 7) ? 7 : msg->remaining_bytes;
    uint16_t offset = msg->bytes_received;

    if (offset + bytes_to_copy > MAX_UDS_PAYLOAD_LEN) {
        #ifdef ISO_TP_DEBUG
            LOG_ERROR("Buffer overflow prevented: offset + bytes_to_copy = %u > %u", offset + bytes_to_copy, MAX_UDS_PAYLOAD_LEN);
        #endif
        return false;
    }

    memcpy(msg->Buffer + offset, rxBuffer + 1, bytes_to_copy);
    msg->bytes_received += bytes_to_copy;
    msg->remaining_bytes -= bytes_to_copy;

    msg->sequence_number = sequence_num;
    msg->next_sequence = (sequence_num + 1) & 0x0F;

    #ifdef ISO_TP_INFO_PRINT
        LOG_DEBUG("Appended CF: seq=%u, copied=%u, total_received=%u, remaining=%u", 
                  sequence_num, bytes_to_copy, msg->bytes_received, msg->remaining_bytes);
    #endif

    if (msg->remaining_bytes == 0) {
        msg->tp_state = ISOTP_FINISHED;
        return true;
    } else {
        msg->tp_state = ISOTP_WAIT_DATA;
        return false;
    }
}

bool IsoTp::receive(Message_t *msg, const char* param_name)
{
  uint32_t rxId;
  uint8_t rxLen = 0;
  uint8_t rxBuffer[8] = {0};
  uint32_t startTime = millis();

  while (_twaiWrapper->receiveMessage(rxId, rxBuffer, rxLen) && (millis() - startTime) < UDS_TIMEOUT)
  {
      if (!isSupportedDiagnosticId(rxId)) {
        continue;
      }

      // Safety: Cap rxLen to 8 to prevent buffer overflow
      if (rxLen > 8) rxLen = 8;

      // Handle UDS Negative Response: [0x03] [0x7F] [original SID] [NRC]
      if ((rxBuffer[0] & 0xF0) == N_PCI_SF && rxLen >= 4 && rxBuffer[1] == UDS_NEGATIVE_RESPONSE) 
      {
        msg->tp_state = ISOTP_ERROR;
        uint8_t nrc_code = rxBuffer[3];
        handle_udsError(msg->service_id, nrc_code, param_name);
        return false;
      }

      //Skip messages that are not for this message
      if(msg->rx_id != rxId) {
        continue;
      }
     
      uint8_t pciType = rxBuffer[0] & 0xF0;

      if(pciType == N_PCI_SF) // Single Frame
      {
        #ifdef ISO_TP_INFO_PRINT
            LOG_DEBUG("Single-Frame received");
        #endif

        msg->length = rxLen;
        return handle_single_frame(msg, rxBuffer);
      }
      else if(pciType == N_PCI_FF) // First Frame
      {
        //FF example:
        //8,10,30,61,21,00,00,00,00

        if (handle_first_frame(msg, rxBuffer))
        {
          #ifdef ISO_TP_INFO_PRINT
            LOG_DEBUG("First-Frame handled successfully");
          #endif
          continue;
        }
        else
        {
          msg->reset();
          return false;
        }
      }
      else if(pciType == N_PCI_CF) // Consecutive Frame
      {
        //CF example:
        // 21,80,02,00,80,00,00,00
        // 22,00,00,00,00,00,00,00
        // 23,00,01,00,00,00,00,51
        // 24,42,65,3A,00,00,00,00
        // 25,00,00,2C,7E,29,55,2C
        // 26,01,00,00,0C,8F,34,1B

        if(handle_consecutive_frame(msg, rxBuffer, rxLen))
        {
          #ifdef ISO_TP_INFO_PRINT
            LOG_DEBUG("Consecutive-Frame handled successfully. Len %d, received %d, remaining %d", msg->length, msg->bytes_received, msg->remaining_bytes);
          #endif
          return true;
        }
        else
        {
          startTime = millis();
          continue;
        }
      }

      vTaskDelay(pdMS_TO_TICKS(1));
  }

  msg->reset();
  return false;
}

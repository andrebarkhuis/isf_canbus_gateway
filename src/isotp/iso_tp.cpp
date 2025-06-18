#include <SPI.h>
#include <Arduino.h>
#include "iso_tp.h"
#include "../mcp_can/mcp_can.h"
#include "../logger/logger.h"


IsoTp::IsoTp(MCP_CAN *bus)
{
  _bus = bus;
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

bool IsoTp::is_next_consecutive_frame(Message_t *msg, unsigned long actual_rx_id, uint8_t actual_seq_num, uint8_t actual_serviceId, uint16_t actual_data_id)
{
    // Use transaction fingerprinting to validate if this is the next frame in our current transaction
    // Check if rx_id, service_id, and data_id match our expected transaction
    
    // Match the IDs (CAN addressing)
    bool idsMatch = (actual_rx_id == msg->rx_id);
    
    // Match the service ID - typically the response service ID = request service ID + 0x40
    bool serviceIdMatch = (actual_serviceId == msg->service_id);
    
    // Match the data ID - should be the same in request and response
    bool dataIdMatch = (actual_data_id == msg->data_id);
    
    // All criteria must match for a valid transaction
    if (idsMatch && serviceIdMatch && dataIdMatch) {
      // LOG_DEBUG("CF received: sequence number received: %u, expected sequence number: %u, received service ID: 0x%X, expected service ID: 0x%X, received data ID: 0x%X, expected data ID: 0x%X", 
      //   actual_seq_num, msg->next_sequence, actual_serviceId, msg->service_id, actual_data_id, msg->data_id);
      return true;
    }

    // LOG_WARN("Unexpected CF received: sequence number received: %u, expected sequence number: %u, received service ID: 0x%X, expected service ID: 0x%X, received data ID: 0x%X, expected data ID: 0x%X", 
    //   actual_seq_num, msg->next_sequence, actual_serviceId, msg->service_id, actual_data_id, msg->data_id);

    return false;
}

bool IsoTp::handle_first_frame(Message_t *msg, uint8_t rxBuffer[])
{ 
  // Correct ISO-TP length calculation: 12 bits from first frame
  // First 4 bits from rxBuffer[0] & 0x0F (high nibble)
  // Last 8 bits from rxBuffer[1] (low byte)
  uint16_t expected_length = ((rxBuffer[0] & 0x0F) << 8) | rxBuffer[1];
  uint8_t ff_service_id = rxBuffer[2];
  uint16_t ff_data_id = rxBuffer[3];  
  
  //8,10,30,61,21,00,00,00,00

  msg->length = expected_length;
  msg->tp_state = ISOTP_WAIT_FC;
  msg->bytes_received = 6; // First frame already contains 6 bytes
  msg->remaining_bytes = expected_length - 6; // Remaining bytes to receive
  msg->sequence_number = 1; // First CF will have sequence number 1;
  msg->next_sequence = 2;

   /* copy the first received data bytes */
   memcpy(msg->Buffer,rxBuffer+2,6); // Skip 2 bytes PCI, FF must have 6 bytes!

  LOG_DEBUG("First-frame received: iso_tp_state=%s, tx_id=0x%lX, rx_id=0x%lX, length=%u, bytes_received=%u, remaining=%u, service_id=0x%X, data_id=0x%X", 
           msg->getStateStr().c_str(), msg->tx_id, msg->rx_id, expected_length, 
           msg->bytes_received, msg->remaining_bytes, ff_service_id, ff_data_id);

  if (!send_flow_control(msg))
  {
    msg->tp_state = ISOTP_ERROR;

    LOG_ERROR("Failed to send Flow-Control after First-Frame: iso_tp_state=%s, tx_id=0x%lX, rx_id=0x%lX, length=%u, service_id=0x%X, data_id=0x%X", msg->getStateStr().c_str(), 
              msg->tx_id, msg->rx_id, expected_length, ff_service_id, ff_data_id);
    
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
  
  memcpy(msg->Buffer, rxBuffer + 1, msg->length); // Skip PCI, SF uses len bytes
    
  LOG_DEBUG("Single frame received: iso_tp_state=%s, tx_id=0x%lX, rx_id=0x%lX, length=%u, service_id=0x%X, data_id=0x%X", msg->getStateStr().c_str(), msg->tx_id, msg->rx_id, msg->length, service_id, data_id);

  return true;
}

bool IsoTp::send_flow_control(struct Message_t *msg)
{
  // FC example: 30,00,00,00,00,00,00,00

  uint8_t TxBuf[8] = {0};

  TxBuf[0] = (N_PCI_FC | ISOTP_FC_CTS); // Explicitly Clear To Send (0x30)
  TxBuf[1] = 0x00;                      // No block size limit
  TxBuf[2] = 0x00;                      // No separation time required

  uint8_t result = _bus->sendMsgBuf(msg->tx_id, 0, 8, reinterpret_cast<byte *>(TxBuf)); // Cast to byte*
  if (result != 0) 
  {
    msg->tp_state = ISOTP_ERROR;
    LOG_ERROR("Failed to send flow-control: tx_id: 0x%lX, rx_id: 0x%lX, service_id: 0x%02X, state: %s", msg->tx_id, msg->rx_id, msg->service_id, msg->getStateStr().c_str());
    return false;
  }

  LOG_DEBUG("Flow-control sent: tx_id: 0x%lX, rx_id: 0x%lX, service_id: 0x%02X", msg->tx_id, msg->rx_id, msg->service_id);

  msg->tp_state = ISOTP_WAIT_DATA;

  return true;
}

bool IsoTp::send_single_frame(struct Message_t *msg)
{
  if (msg->length > 7) 
  {
    msg->tp_state = ISOTP_ERROR;
    LOG_ERROR("Message too long for single frame: tx_id: 0x%lX, rx_id: 0x%lX, length: %d (max: 7), state: %s", msg->tx_id, msg->rx_id, msg->length, msg->getStateStr().c_str());
    return false; // Error, too much data for single frame
  }

  uint8_t TxBuf[8] = {0};

  TxBuf[0] = N_PCI_SF | (msg->length & 0x0F);

  memcpy(TxBuf + 1, msg->Buffer, msg->length);

  uint8_t result = _bus->sendMsgBuf(msg->tx_id, 0, 8, reinterpret_cast<byte *>(TxBuf)); // Cast to byte*
  if (result != 0) 
  {
    msg->tp_state = ISOTP_ERROR;
    LOG_ERROR("Failed to send single-frame: tx_id: 0x%lX, rx_id: 0x%lX, service_id: 0x%02X, state: %s", msg->tx_id, msg->rx_id, msg->service_id, msg->getStateStr().c_str());
    return false;
  }

  msg->tp_state = ISOTP_FINISHED;

  LOG_DEBUG("Single-frame sent: tx_id: 0x%lX, rx_id: 0x%lX, length: %d, service_id: 0x%02X", msg->tx_id, msg->rx_id, msg->length, msg->service_id);
  
  return true;
}

bool IsoTp::send(Message_t *msg)
{
    LOG_DEBUG("Sending UDS message: tx_id: 0x%lX, rx_id: 0x%lX, service_id: 0x%02X, length: %d", msg->tx_id, msg->rx_id, msg->service_id, msg->length);

    return send_single_frame(msg); 
}

void IsoTp::handle_udsError(uint8_t serviceId, uint8_t nrc_code)
{
  LOG_ERROR("UDS Negative Response for Service ID 0x%X: %s (0x%X)", serviceId, getUdsErrorString(nrc_code), nrc_code);
}

bool IsoTp::can_read_message(unsigned long &rxId, uint8_t &rxLen, uint8_t *rxBuffer)
{
  if (_bus->checkReceive() == CAN_MSGAVAIL) {
    
    memset(rxBuffer, 0, 8);

    _bus->readMsgBufID(&rxId, &rxLen, rxBuffer);
    
    return true;
  }
  
  return false;
}

bool IsoTp::receive(Message_t *msg)
{
  while(msg->tp_state!=ISOTP_FINISHED && msg->tp_state!=ISOTP_ERROR)
  {
      uint32_t rxId;
      uint8_t rxLen;
      uint8_t rxBuffer[8] = {0};
      uint8_t rxServiceId = rxBuffer[2];
      
      if (can_read_message(rxId, rxLen, rxBuffer)==false)
      {
        vTaskDelay(pdMS_TO_TICKS(1));
        LOG_DEBUG("No message available");
        continue;
      }
    
      if (rxLen >= 4 && rxBuffer[1] == UDS_NEGATIVE_RESPONSE) 
      {
        msg->tp_state = ISOTP_ERROR;
        
        uint8_t nrc_code = rxBuffer[3];
        handle_udsError(rxServiceId, nrc_code);
        
        vTaskDelay(pdMS_TO_TICKS(1));

        return false;
      }

      uint8_t pciType = rxBuffer[0] & 0xF0;

      if(pciType == N_PCI_SF) // Single Frame
      {
        LOG_DEBUG("Single Frame received");

        vTaskDelay(pdMS_TO_TICKS(1));

        return handle_single_frame(msg, rxBuffer);
      }
      else if( pciType == N_PCI_FF) // First Frame
      {
        vTaskDelay(pdMS_TO_TICKS(1));
        
        //FF example:
        //8,10,30,61,21,00,00,00,00

        LOG_DEBUG("First Frame received");

        if (handle_first_frame(msg, rxBuffer))
        {
          LOG_DEBUG("First Frame handled successfully, tx_id: 0x%lX, rx_id: 0x%lX, service_id: 0x%02X, data_id: 0x%02X, sequence: %d", 
            msg->tx_id, msg->rx_id, msg->service_id, msg->data_id, msg->sequence_number);
          vTaskDelay(pdMS_TO_TICKS(1));
          continue;
        }
        else
        {
          return false;
        }

      }
      else if( pciType == N_PCI_CF) // Consecutive Frame
      {
        LOG_DEBUG("Consecutive Frame received");

        vTaskDelay(pdMS_TO_TICKS(1));

        //CF example:
        // 21,80,02,00,80,00,00,00
        // 22,00,00,00,00,00,00,00
        // 23,00,01,00,00,00,00,51
        // 24,42,65,3A,00,00,00,00
        // 25,00,00,2C,7E,29,55,2C
        // 26,01,00,00,0C,8F,34,1B

        uint8_t uds_seq_num = rxBuffer[0] & 0x0F;
          
        // Check if this is the expected consecutive frame with the correct sequence number
        if(is_next_consecutive_frame(msg, rxId, uds_seq_num, msg->service_id, msg->data_id) && uds_seq_num == msg->next_sequence)
        {
            LOG_DEBUG("Matching CF received with expected sequence %u", uds_seq_num);
                    
            uint16_t bytes_to_copy = (msg->remaining_bytes > 7) ? 7 : msg->remaining_bytes;
            uint16_t offset = msg->bytes_received;
            
            LOG_DEBUG("Consecutive Frame received: bytes_to_copy: %u, offset: %u, before[received: %u, remaining: %u]", bytes_to_copy, offset, msg->bytes_received, msg->remaining_bytes);
            
            memcpy(msg->Buffer + offset, rxBuffer + 1, bytes_to_copy);

            msg->bytes_received += bytes_to_copy;
            
            if (bytes_to_copy > msg->remaining_bytes) {
                LOG_ERROR("Remaining bytes underflow prevented: bytes_to_copy %u > remaining %u", bytes_to_copy, msg->remaining_bytes);
                msg->remaining_bytes = 0;
            } else {
                msg->remaining_bytes -= bytes_to_copy;
            }
            
            msg->next_sequence = (msg->next_sequence + 1) & 0x0F;
            
            LOG_DEBUG("After CF processing: received: %u, remaining: %u", msg->bytes_received, msg->remaining_bytes);

            if (msg->remaining_bytes == 0) 
            {
              msg->tp_state = ISOTP_FINISHED;
              return true;
            } 
            else 
            { 
              msg->tp_state = ISOTP_WAIT_DATA;
              vTaskDelay(pdMS_TO_TICKS(1));
              continue;
            }
        }
      }

      vTaskDelay(pdMS_TO_TICKS(1));
    }

  return false;
}

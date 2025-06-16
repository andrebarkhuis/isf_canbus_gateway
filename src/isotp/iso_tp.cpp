#include <SPI.h>
#include <Arduino.h>
#include "iso_tp.h"
#include "../mcp_can/mcp_can.h"
#include "../logger/logger.h"


IsoTp::IsoTp(MCP_CAN *bus)
{
  _bus = bus;
  reset_state(); // Initialize buffer on construction
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

bool IsoTp::is_can_message_available()
{
  // Poll for new messages
  return (_bus->checkReceive() == CAN_MSGAVAIL);
}

bool IsoTp::is_next_consecutive_frame(Message_t *msg, unsigned long actual_rx_id, unsigned actual_tx_id, uint8_t actual_seq_num, uint8_t actual_serviceId, uint16_t actual_data_id)
{
    // Use transaction fingerprinting to validate if this is the next frame in our current transaction
    // Check if rx_id, tx_id, service_id, and data_id match our expected transaction
    
    // Match the IDs (CAN addressing)
    bool idsMatch = (actual_rx_id == msg->rx_id && actual_tx_id == msg->tx_id);
    
    // Match the service ID - typically the response service ID = request service ID + 0x40
    bool serviceIdMatch = (actual_serviceId == msg->service_id);
    
    // Match the data ID - should be the same in request and response
    bool dataIdMatch = (actual_data_id == msg->data_id);
    
    // All criteria must match for a valid transaction
    if (idsMatch && serviceIdMatch && dataIdMatch) {
      return true;
    }

    LOG_WARN("Unexpected CF received: sequence number received: %u, expected sequence number: %u, received service ID: 0x%X, expected service ID: 0x%X, received data ID: 0x%X, expected data ID: 0x%X", 
      actual_seq_num, msg->next_sequence, actual_serviceId, msg->service_id, actual_data_id, msg->data_id);

    return false;
}

void IsoTp::reset_state()
{
  LOG_DEBUG("Resetting ISO-TP state");

  memset(rxBuffer, 0, sizeof(rxBuffer));
  first_frame_received = false;
  rxLen = 0;
}

uint8_t IsoTp::handle_first_frame(Message_t *msg)
{
  // Get the length from the first frame (12 bits)
  uint16_t expected_length = ((rxBuffer[0] & 0x0F) << 8) | rxBuffer[1];
  
  // Extract the service ID (typically the first data byte after length)
  uint8_t ff_service_id = rxBuffer[2];
  
  // Extract the data ID. If the service ID is 0x61 (positive response for UDS_SID_READ_DATA_BY_LOCAL_ID 0x21),
  // the DID (LocalID) is 1 byte at rxBuffer[3]. Otherwise, assume 2 bytes for other services for now.
  uint16_t ff_data_id = (ff_service_id == 0x61 && expected_length >= 1) ? rxBuffer[3] : ((expected_length >= 2) ? (rxBuffer[3] << 8) | rxBuffer[4] : 0); 
  // Added expected_length checks to prevent reading past buffer if PDU is too short for DID.
  
  msg->length = expected_length;
  msg->service_id = ff_service_id;
  msg->data_id = ff_data_id;
  msg->tp_state = ISOTP_WAIT_DATA;
  msg->bytes_received = 6; // First frame already contains 6 bytes
  msg->remaining_bytes = expected_length - 6; // Remaining bytes to receive
  msg->next_sequence = 1; // First CF will have sequence number 1;

  // Copy the first 6 bytes of the first frame to the buffer
  memcpy(msg->Buffer, rxBuffer + 2, 6);

  LOG_DEBUG("First-frame received: iso_tp_state=%s, tx_id=0x%lX, rx_id=0x%lX, length=%u, service_id=0x%X, data_id=0x%X", msg->getStateStr().c_str(), msg->tx_id, msg->rx_id, expected_length, ff_service_id, ff_data_id);

  uint8_t result = send_flow_control(msg);
  if (result != 0) {
    msg->tp_state = ISOTP_ERROR;
    LOG_ERROR("Failed to send Flow-Control after First-Frame: iso_tp_state=%s, tx_id=0x%lX, rx_id=0x%lX, length=%u, service_id=0x%X, data_id=0x%X", msg->getStateStr().c_str(), 
              msg->tx_id, msg->rx_id, expected_length, ff_service_id, ff_data_id);
  }
  return result;
}

uint8_t IsoTp::handle_consecutive_frame(struct Message_t *msg)
{
    // Extract sequence number from the CF frame
    uint8_t actual_seq_num = rxBuffer[0] & 0x0F;
    
    // Verify sequence number matches what we expect
    if (actual_seq_num != msg->next_sequence) {
      msg->tp_state = ISOTP_ERROR;
      LOG_WARN("Received wrong CF sequence: %u, expected: %u", actual_seq_num, msg->next_sequence);
      return 1;
    }
    
    // Calculate how many bytes to copy from this frame
    uint16_t bytes_to_copy = (msg->remaining_bytes > 7) ? 7 : msg->remaining_bytes;
    
    // Calculate the offset in the buffer where to copy data
    uint16_t offset = msg->bytes_received;
    
    // Copy the data from the CF (skipping the first byte which is the PCI)
    memcpy(msg->Buffer + offset, rxBuffer + 1, bytes_to_copy);
    
    msg->bytes_received += bytes_to_copy;
    msg->remaining_bytes -= bytes_to_copy;
    msg->next_sequence = (msg->next_sequence + 1) & 0x0F;
    
    if (msg->remaining_bytes == 0) {
      msg->tp_state = ISOTP_FINISHED;
      LOG_DEBUG("CF sequence complete all data received, iso_tp_state=%s, %u bytes received", msg->getStateStr().c_str(), msg->length);
    } 
    else { 
      msg->tp_state = ISOTP_WAIT_DATA;
      LOG_DEBUG("waiting for CF with sequence %u, service ID: 0x%X, data ID: 0x%X, %u bytes remaining", msg->next_sequence, msg->service_id, msg->data_id, msg->remaining_bytes);
    }
    
    return 0;
}

uint8_t IsoTp::handle_single_frame(Message_t *msg)
{
  msg->length = rxBuffer[0] & 0x0F;
  
  // Extract the service ID (typically the first data byte)
  uint8_t service_id = rxBuffer[1];
  
  // Extract the data ID if present (typically 2 bytes after service ID)
  uint16_t data_id = 0;

  if (msg->length >= 3) {
    data_id = (rxBuffer[2] << 8) | (msg->length >= 4 ? rxBuffer[3] : 0);
  }
  
  LOG_DEBUG("Single-frame received: iso_tp_state=%s, tx_id=0x%lX, rx_id=0x%lX, length=%u, service_id=0x%X, data_id=0x%X", msg->getStateStr().c_str(), msg->tx_id, msg->rx_id, msg->length, service_id, data_id);
  
  if (msg->length > 7) {
    LOG_WARN("Correcting single frame with invalid length: iso_tp_state=%s, tx_id=0x%lX, rx_id=0x%lX, length=%u (max: 7), service_id=0x%X, data_id=0x%X", msg->getStateStr().c_str(), msg->tx_id, msg->rx_id, msg->length, service_id, data_id);
    msg->length = 7; // Correct the length
  }
  
  memcpy(msg->Buffer, rxBuffer + 1, msg->length); // Skip PCI, SF uses len bytes
  
  // Store transaction fingerprint
  msg->service_id = service_id;
  msg->data_id = data_id;
  
  LOG_DEBUG("Single frame received: iso_tp_state=%s, tx_id=0x%lX, rx_id=0x%lX, length=%u, service_id=0x%X, data_id=0x%X", msg->getStateStr().c_str(), msg->tx_id, msg->rx_id, msg->length, service_id, data_id);

  return 0;
}

uint8_t IsoTp::send_flow_control(struct Message_t *msg)
{
  uint8_t TxBuf[8] = {0};

  TxBuf[0] = (N_PCI_FC | ISOTP_FC_CTS); // Explicitly Clear To Send (0x30)
  TxBuf[1] = 0x00;                      // No block size limit
  TxBuf[2] = 0x00;                      // No separation time required

  uint8_t result = _bus->sendMsgBuf(msg->tx_id, 0, 8, reinterpret_cast<byte *>(TxBuf)); // Cast to byte*
  if (result != 0) {
    msg->tp_state = ISOTP_ERROR;
    LOG_ERROR("Failed to send flow-control: tx_id: 0x%lX, rx_id: 0x%lX, service_id: 0x%02X, state: %s", msg->tx_id, msg->rx_id, msg->service_id, msg->getStateStr().c_str());
    return 1;
  }

  LOG_DEBUG("Flow-control sent: tx_id: 0x%lX, rx_id: 0x%lX, service_id: 0x%02X", msg->tx_id, msg->rx_id, msg->service_id);

  return 0;
}

uint8_t IsoTp::send_single_frame(struct Message_t *msg)
{
  if (msg->length > 7) {
    msg->tp_state = ISOTP_ERROR;
    LOG_ERROR("Message too long for single frame: tx_id: 0x%lX, rx_id: 0x%lX, length: %d (max: 7), state: %s", msg->tx_id, msg->rx_id, msg->length, msg->getStateStr().c_str());
    return 1; // Error, too much data for single frame
  }

  uint8_t TxBuf[8] = {0};

  TxBuf[0] = N_PCI_SF | (msg->length & 0x0F);

  memcpy(TxBuf + 1, msg->Buffer, msg->length);

  uint8_t result = _bus->sendMsgBuf(msg->tx_id, 0, 8, reinterpret_cast<byte *>(TxBuf)); // Cast to byte*
  if (result != 0) {
    msg->tp_state = ISOTP_ERROR;
    LOG_ERROR("Failed to send single-frame: tx_id: 0x%lX, rx_id: 0x%lX, service_id: 0x%02X, state: %s", msg->tx_id, msg->rx_id, msg->service_id, msg->getStateStr().c_str());
    return 1;
  }

  LOG_DEBUG("Single-frame sent: tx_id: 0x%lX, rx_id: 0x%lX, length: %d, service_id: 0x%02X", msg->tx_id, msg->rx_id, msg->length, msg->service_id);
  
  return 0;
}

uint8_t IsoTp::send(Message_t *msg)
{
  reset_state();

  LOG_DEBUG("Sending UDS message: tx_id: 0x%lX, rx_id: 0x%lX, service_id: 0x%02X, length: %d", msg->tx_id, msg->rx_id, msg->service_id, msg->length);

  uint8_t retval = 0;

  // Since we only support single-frame sending as a tester
  if (msg->length > 7)
  {
    msg->tp_state = ISOTP_ERROR;

    Logger::logUdsMessage("[iso_tp::send] Error: Message too long for single frame", msg);
    
    LOG_ERROR("Message too long for single frame: tx_id: 0x%lX, rx_id: 0x%lX, service_id: 0x%02X, length: %d (max: 7), state: %s", msg->tx_id, msg->rx_id, msg->service_id, msg->length, msg->getStateStr().c_str());
    
    return 1;
  }

  // Send as single frame
  retval = send_single_frame(msg);
  if (retval == 0) {
    msg->tp_state = ISOTP_FINISHED;
    
    Logger::logUdsMessage("[iso_tp::send] Single frame sent successfully", msg);
  }
  else {
    msg->tp_state = ISOTP_ERROR;
    
    Logger::logUdsMessage("[iso_tp::send] Error: Failed to send single frame", msg);
  }

  return retval;
}

void IsoTp::handle_udsError(uint8_t serviceId, uint8_t nrc_code)
{
  LOG_ERROR("UDS Negative Response for Service ID 0x%X: %s (0x%X)", serviceId, getUdsErrorString(nrc_code), nrc_code);
}

void IsoTp::receive_all_consecutive_frames(uint8_t seq_num, Message_t *msg)
{
    LOG_DEBUG("Receiving consecutive frames: tx_id: 0x%lX, rx_id: 0x%lX, service_id: 0x%02X, seq_num: %u", msg->tx_id, msg->rx_id, msg->service_id, seq_num);
    
    while (is_can_message_available())
    {
        unsigned long actual_rx_id;
        uint8_t rxLen;
        
        _bus->readMsgBufID(&actual_rx_id, &rxLen, rxBuffer);

        if(is_next_consecutive_frame(msg, actual_rx_id, msg->tx_id, seq_num, msg->service_id, msg->data_id))
        {
            if(handle_consecutive_frame(msg) == 0)
            {
                if(msg->tp_state == ISOTP_FINISHED)
                {
                  return 0;
                }
                else if(msg->tp_state == ISOTP_ERROR)
                {
                  return 1;
                }
                else if(msg->tp_state == ISOTP_WAIT_DATA)
                {
                  continue;
                }
            }
        }
        else 
        {
          LOG_WARN("Received unexpected consecutive frame: rx_id=0x%lX, tx_id=0x%lX, seq_num=%u", actual_rx_id, msg->tx_id, seq_num);
        }
    }
}

uint8_t IsoTp::receive(Message_t *msg)
{
  reset_state();

  uint32_t startTime = millis();

  while ((millis() - startTime) < TIMEOUT_SESSION)
  {
    while (is_can_message_available())
    {
      unsigned long actual_rx_id;
            
      _bus->readMsgBufID(&actual_rx_id, &rxLen, rxBuffer);

      if (rxLen >= 4 && rxBuffer[1] == UDS_NEGATIVE_RESPONSE) {
        
        msg->tp_state = ISOTP_ERROR;
        uint8_t original_sid = rxBuffer[2];
        uint8_t nrc_code = rxBuffer[3];
        handle_udsError(original_sid, nrc_code);
        
        return 1;
      }

      if (actual_rx_id != msg->rx_id) {
        continue; 
      }

      uint8_t pciType = rxBuffer[0] & 0xF0;

      if(pciType == N_PCI_FF) // First Frame
      {
        first_frame_received = true;  

        if(handle_first_frame(msg) == 0)
        {
          continue;
        }
      }
      else if(pciType == N_PCI_CF) // Consecutive Frame
      {
        //NB: Extract sequence number from the CF frame to check if it is the next expected frame
        uint8_t seq_num = rxBuffer[0] & 0x0F;
        
        LOG_DEBUG("Received consecutive frame: rx_id=0x%lX, tx_id=0x%lX, seq_num=%u", actual_rx_id, msg->tx_id, seq_num);
        
        receive_all_consecutive_frames(seq_num, msg);

        continue;
      }
      else if(pciType == N_PCI_SF) // Single Frame
      {
        if(handle_single_frame(msg) == 0)
        {
          if(msg->tp_state == ISOTP_FINISHED)
          {
            //NB: Message received and processed successfully, happpy day.
            return 0;
          }
          else if(msg->tp_state == ISOTP_ERROR)
          {
            //NB: Message received and processed failed, sad day.
            return 1;
          }
        }
      }

      delay(5);
    }
  }
 
  if(msg->tp_state == ISOTP_FINISHED)
  {
    Logger::logUdsMessage("[IsoTp::receive] completed successfully", msg);
    return 0;
  }

  Logger::logUdsMessage("[IsoTp::receive] failed or timed out", msg);

  return 1;
}

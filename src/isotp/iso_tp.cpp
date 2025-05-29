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

bool IsoTp::is_can_message_available()
{
  // Poll for new messages
  return (_bus->checkReceive() == CAN_MSGAVAIL);
}

uint8_t IsoTp::send_flow_control(struct Message_t *msg)
{
  LOG_DEBUG("send_flow_control: tx_id: 0x%lX, rx_id: 0x%lX", msg->tx_id, msg->rx_id);

  uint8_t TxBuf[8] = {0};

  TxBuf[0] = (N_PCI_FC | ISOTP_FC_CTS); // Explicitly Clear To Send (0x30)
  TxBuf[1] = 0x00;                      // No block size limit
  TxBuf[2] = 0x00;                      // No separation time required

  msg->tp_state = ISOTP_WAIT_DATA;

  uint8_t result = _bus->sendMsgBuf(msg->tx_id, 0, 8, reinterpret_cast<byte *>(TxBuf)); // Cast to byte*
  if (result != 0) {
    LOG_ERROR("Failed to send flow control frame: tx_id: 0x%lX, rx_id: 0x%lX, error: %d", msg->tx_id, msg->rx_id, result);
  }
  return result;
}

uint8_t IsoTp::send_single_frame(struct Message_t *msg)
{
  if (msg->length > 7) {
    LOG_ERROR("Message too long for single frame: tx_id: 0x%lX, rx_id: 0x%lX, length: %d (max: 7)", msg->tx_id, msg->rx_id, msg->length);
    return 1; // Error, too much data for single frame
  }

  uint8_t TxBuf[8] = {0};

  TxBuf[0] = N_PCI_SF | (msg->length & 0x0F);

  memcpy(TxBuf + 1, msg->Buffer, msg->length);

  uint8_t result = _bus->sendMsgBuf(msg->tx_id, 0, 8, reinterpret_cast<byte *>(TxBuf)); // Cast to byte*
  if (result != 0) {
    LOG_ERROR("Failed to send single-frame: tx_id: 0x%lX, rx_id: 0x%lX, length: %d, error: %d", msg->tx_id, msg->rx_id, msg->length, result);
    return 1;
  }

  LOG_DEBUG("Single-frame sent: tx_id: 0x%lX, rx_id: 0x%lX, length: %d", msg->tx_id, msg->rx_id, msg->length);
  
  return 0;
}

uint8_t IsoTp::receive_consecutive_frame(struct Message_t *msg)
{
    // Extract sequence number from the CF frame
    uint8_t actual_seq_num = rxBuffer[0] & 0x0F;
    
    // Verify sequence number matches what we expect
    if (actual_seq_num != msg->next_sequence) {
        LOG_WARN("Received wrong CF sequence: %u, expected: %u", actual_seq_num, msg->next_sequence);
        msg->tp_state = ISOTP_ERROR;
        return 1;
    }
    
    // Calculate how many bytes to copy from this frame
    uint16_t bytes_to_copy = (msg->remaining_bytes > 7) ? 7 : msg->remaining_bytes;
    
    // Calculate the offset in the buffer where to copy data
    uint16_t offset = msg->bytes_received;
    
    // Copy the data from the CF (skipping the first byte which is the PCI)
    memcpy(msg->Buffer + offset, rxBuffer + 1, bytes_to_copy);
    
    // Update tracking counters
    msg->bytes_received += bytes_to_copy;
    msg->remaining_bytes -= bytes_to_copy;
    
    // Update sequence number for next CF, with wrap-around
    msg->next_sequence = (msg->next_sequence + 1) & 0x0F;
    if (msg->next_sequence == 0) msg->next_sequence = 1; // Reset to 1 if it wraps to 0
    
    // Update message state if we've received all the data
    if (msg->remaining_bytes == 0) {
        msg->tp_state = ISOTP_FINISHED;
        LOG_DEBUG("CF sequence complete all data received, iso_tp_state=%d, %u bytes received", msg->tp_state, msg->length);
    } else { 
      msg->tp_state = ISOTP_WAIT_DATA;
      LOG_DEBUG("waiting for CF with sequence %u, service ID: 0x%X, data ID: 0x%X, %u bytes remaining", msg->next_sequence, msg->service_id, msg->data_id, msg->remaining_bytes);
    }
    
    return 0;
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

// Reset internal state and buffer, include variables that needs to be reset before a new transaction
void IsoTp::reset_state()
{
  LOG_DEBUG("Resetting ISO-TP state");

  memset(rxBuffer, 0, sizeof(rxBuffer));
  first_frame_received = false;
  rxLen = 0;
}

uint8_t IsoTp::send(Message_t *msg)
{
  reset_state();

  LOG_DEBUG("Sending UDS message: tx_id: 0x%lX, rx_id: 0x%lX, length: %d", msg->tx_id, msg->rx_id, msg->length);

  uint8_t retval = 0;

  // Since we only support single-frame sending as a tester
  if (msg->length > 7)
  {
    Logger::logUdsMessage("[iso_tp::send] Error: Message too long for single frame", msg);
    LOG_ERROR("Message too long for single frame: tx_id: 0x%lX, rx_id: 0x%lX, length: %d (max: 7)", msg->tx_id, msg->rx_id, msg->length);
    msg->tp_state = ISOTP_ERROR;
    return 1;
  }

  // Send as single frame
  retval = send_single_frame(msg);
  msg->tp_state = ISOTP_FINISHED;
  Logger::logUdsMessage("[iso_tp::send] Single frame", msg);

  return retval;
}

uint8_t IsoTp::handle_first_frame(Message_t *msg)
{
  // Get the length from the first frame (12 bits)
  uint16_t expected_length = ((rxBuffer[0] & 0x0F) << 8) | rxBuffer[1];
  
  // Extract the service ID (typically the first data byte after length)
  uint8_t ff_service_id = rxBuffer[2];
  
  // Extract the data ID (typically 2 bytes after service ID in most UDS messages)
  uint16_t ff_data_id = (rxBuffer[3] << 8) | rxBuffer[4];
  
  // Initialize message tracking variables
  msg->length = expected_length;
  msg->service_id = ff_service_id;
  msg->data_id = ff_data_id;
  msg->tp_state = ISOTP_WAIT_DATA;
  
  // Initialize CF tracking variables
  msg->bytes_received = 6; // First frame already contains 6 bytes
  msg->remaining_bytes = expected_length - 6; // Remaining bytes to receive
  msg->next_sequence = 1; // First CF will have sequence number 1;

  // Copy the first 6 bytes of the first frame to the buffer
  memcpy(msg->Buffer, rxBuffer + 2, 6);

  LOG_DEBUG("First-frame received: iso_tp_state=%d, tx_id=0x%lX, rx_id=0x%lX, length=%u, service_id=0x%X, data_id=0x%X", msg->tp_state, msg->tx_id, msg->rx_id, expected_length, ff_service_id, ff_data_id);

  uint8_t result = send_flow_control(msg);
  if (result != 0) {
    msg->tp_state = ISOTP_ERROR;
    LOG_ERROR("Failed to send Flow-Control after First-Frame: iso_tp_state=%d, tx_id=0x%lX, rx_id=0x%lX, length=%u, service_id=0x%X, data_id=0x%X", msg->tp_state, msg->tx_id, msg->rx_id, expected_length, ff_service_id, ff_data_id);
  }
  return result;
}

uint8_t IsoTp::handle_single_frame(Message_t *msg)
{
  msg->length = rxBuffer[0] & 0x0F;
  msg->tp_state = ISOTP_FINISHED;
  
  // Extract the service ID (typically the first data byte)
  uint8_t service_id = rxBuffer[1];
  
  // Extract the data ID if present (typically 2 bytes after service ID)
  uint16_t data_id = 0;
  if (msg->length >= 3) {
    data_id = (rxBuffer[2] << 8) | (msg->length >= 4 ? rxBuffer[3] : 0);
  }
  
  LOG_DEBUG("Single-frame received: iso_tp_state=%d, tx_id=0x%lX, rx_id=0x%lX, length=%u, service_id=0x%X, data_id=0x%X", msg->tp_state, msg->tx_id, msg->rx_id, msg->length, service_id, data_id);
  
  if (msg->length > 7) {
    LOG_WARN("Correcting single frame with invalid length: iso_tp_state=%d, tx_id=0x%lX, rx_id=0x%lX, length=%u (max: 7), service_id=0x%X, data_id=0x%X", msg->tp_state, msg->tx_id, msg->rx_id, msg->length, service_id, data_id);
    msg->length = 7; // Correct the length
  }
  
  memcpy(msg->Buffer, rxBuffer + 1, msg->length); // Skip PCI, SF uses len bytes
  
  // Store transaction fingerprint
  msg->service_id = service_id;
  msg->data_id = data_id;
  
  LOG_DEBUG("Single frame received: iso_tp_state=%d, tx_id=0x%lX, rx_id=0x%lX, length=%u, service_id=0x%X, data_id=0x%X", msg->tp_state, msg->tx_id, msg->rx_id, msg->length, service_id, data_id);

  return 0;
}

uint8_t IsoTp::receive(Message_t *msg)
{
  reset_state();

  uint32_t startTime = millis();

  while ((millis() - startTime) < TIMEOUT_SESSION)
  {
    // Keep reading all available frames
    while (is_can_message_available())
    {
      unsigned long actual_rx_id;
      byte ext;
      _bus->peekMsgId(&actual_rx_id, &ext);

      if(actual_rx_id != msg->rx_id)
      {
        //NB: Read the message from the receive buffer so its removed from the stack
        _bus->readMsgBufID(&actual_rx_id, &rxLen, rxBuffer);
        LOG_DEBUG("Skipping irrelevant frame with ID: 0x%lX (expected: 0x%lX), tx: 0x%lX", actual_rx_id, msg->rx_id, msg->tx_id);
        continue;
      }

      // Read the message data
      _bus->readMsgBufID(&actual_rx_id, &rxLen, rxBuffer);

      uint8_t pciType = rxBuffer[0] & 0xF0;

      if(pciType == N_PCI_FF) // First Frame
      {
        handle_first_frame(msg);

        first_frame_received = true;
      }
      else if(pciType == N_PCI_CF) // Consecutive Frame
      {
        //NB: Extract sequence number from the CF frame to check if it is the next expected frame
        uint8_t seq_num = rxBuffer[0] & 0x0F;
        
        //NB: Check if the received frame is the next expected frame, this is done to prevent out of order frames
        if(first_frame_received && is_next_consecutive_frame(msg, actual_rx_id, msg->tx_id, seq_num, msg->service_id, msg->data_id))
        {
          receive_consecutive_frame(msg);
        }
        else {
          LOG_WARN("Received unexpected consecutive frame: rx_id=0x%lX, tx_id=0x%lX, seq_num=%u", actual_rx_id, msg->tx_id, seq_num);
        }
      }
      else if(pciType == N_PCI_SF) // Single Frame
      {
        handle_single_frame(msg);
      }
      delay(1); // small wait to give ECU time to respond
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

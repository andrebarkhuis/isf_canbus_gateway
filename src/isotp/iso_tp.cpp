#include <SPI.h>
#include <Arduino.h>
#include "iso_tp.h"
#include "../mcp_can/mcp_can.h"
#include "../logger/logger.h"

IsoTp::IsoTp(MCP_CAN *bus)
{
  _bus = bus;
}


bool IsoTp::is_can_message_available()
{
  // Poll for new messages
  return (_bus->checkReceive() == CAN_MSGAVAIL);
}

uint8_t IsoTp::send_flow_control(struct Message_t *msg)
{
#ifdef ISO_TP_DEBUG
  Logger::logUdsMessage("send_flow_control", msg);
#endif

  uint8_t TxBuf[8] = {0};

  TxBuf[0] = (N_PCI_FC | ISOTP_FC_CTS); // Explicitly Clear To Send (0x30)
  TxBuf[1] = 0x00;                      // No block size limit
  TxBuf[2] = 0x00;                      // No separation time required

  return _bus->sendMsgBuf(msg->tx_id, 0, 8, reinterpret_cast<byte *>(TxBuf)); // Cast to byte*
}

uint8_t IsoTp::send_single_frame(struct Message_t *msg)
{
#ifdef ISO_TP_DEBUG
  Logger::logUdsMessage("send_single_frame", msg);
#endif

  if (msg->len > 7)
    return 1; // Error, too much data for single frame

  uint8_t TxBuf[8] = {0};
  TxBuf[0] = N_PCI_SF | (msg->len & 0x0F);
  memcpy(TxBuf + 1, msg->Buffer, msg->len);


  return _bus->sendMsgBuf(msg->tx_id, 0, 8, reinterpret_cast<byte *>(TxBuf)); // Cast to byte*
}

uint8_t IsoTp::send_first_frame(struct Message_t *msg)
{
#ifdef ISO_TP_DEBUG
  Logger::logUdsMessage("send_first_frame", msg);
#endif

  if (msg->len <= 7)
    return 1; // Error, use SF for 7 or fewer bytes

  uint8_t TxBuf[8] = {0};
  msg->seq_id = 1;

  TxBuf[0] = N_PCI_FF | ((msg->len >> 8) & 0x0F);
  TxBuf[1] = msg->len & 0xFF;
  memcpy(TxBuf + 2, msg->Buffer, 6); // First 6 bytes payload

  return _bus->sendMsgBuf(msg->tx_id, 0, 8, reinterpret_cast<byte *>(TxBuf)); // Cast to byte*
}

uint8_t IsoTp::send_consecutive_frame(struct Message_t *msg)
{
  uint8_t TxBuf[8] = {0};
  uint8_t payload_len = (msg->len > 7) ? 7 : msg->len;

  TxBuf[0] = N_PCI_CF | (msg->seq_id & 0x0F);
  memcpy(TxBuf + 1, msg->Buffer, payload_len);

  return _bus->sendMsgBuf(msg->tx_id, 0, 8, reinterpret_cast<byte *>(TxBuf)); // Cast to byte*
}

void IsoTp::flow_control_delay(uint8_t sep_time)
{
  /*
   * 0x00 - 0x7F: 0 - 127ms
   * 0x80 - 0xF0: reserved
   * 0xF1 - 0xF9: 100us - 900us
   * 0xFA - 0xFF: reserved
   * default 0x7F, 127ms
   */
  if (sep_time <= 0x7F)
    delay(sep_time);
  else if ((sep_time >= 0xF1) && (sep_time <= 0xF9))
    delayMicroseconds((sep_time - 0xF0) * 100);
  else
    delay(0x7F);
}

uint8_t IsoTp::receive_single_frame(struct Message_t *msg)
{
  /* get the SF_DL from the N_PCI byte */
  msg->len = rxBuffer[0] & 0x0F;
  /* copy the received data bytes */
  memcpy(msg->Buffer, rxBuffer + 1, msg->len); // Skip PCI, SF uses len bytes
  msg->tp_state = ISOTP_FINISHED;

  Logger::logUdsMessage("receive_single_frame", msg);

  return 0;
}

uint8_t IsoTp::receive_first_frame(struct Message_t *msg)
{
  msg->seq_id = 1; // Reset expected sequence number explicitly after receiving first frame

  /* get the FF_DL */
  msg->len = (rxBuffer[0] & 0x0F) << 8;
  msg->len += rxBuffer[1];
  rest = msg->len;

  /* copy the first received data bytes */
  memcpy(msg->Buffer, rxBuffer + 2, 6); // Skip 2 bytes PCI, FF must have 6 bytes!
  rest -= 6;                            // Rest length

  msg->tp_state = ISOTP_WAIT_DATA;

  Logger::logUdsMessage("receive_first_frame", msg);
  /* send our first FC frame with Target Address */
  struct Message_t fc;
  fc.tx_id = msg->tx_id;
  fc.fc_status = ISOTP_FC_CTS;
  fc.blocksize = 0;
  fc.min_sep_time = 0;

  return send_flow_control(&fc);
}

uint8_t IsoTp::receive_consecutive_frame(struct Message_t *msg)
{
  uint32_t delta = millis() - wait_cf; // (move logging after message update below)

  if ((delta >= TIMEOUT_FC) && msg->seq_id > 1)
  {
    Logger::warn("[receive_consecutive_frame] Timeout waiting for CF. wait_cf=%lu delta=%lu", wait_cf, delta);
    msg->tp_state = ISOTP_IDLE;
    return 1;
  }
  wait_cf = millis();

  uint8_t receivedSeqId = rxBuffer[0] & 0x0F;
  uint8_t expectedSeqId = msg->seq_id & 0x0F;

  if (receivedSeqId != expectedSeqId)
  {
    Logger::warn("[receive_consecutive_frame] iso_tp sequence mismatch. Got: %u, Expected: %u", receivedSeqId, expectedSeqId);
    msg->tp_state = ISOTP_ERROR;
    return 1;
  }

  uint8_t bytesToCopy = (rest <= 7) ? rest : 7;
  memcpy(msg->Buffer + msg->len - rest, rxBuffer + 1, bytesToCopy);
  rest -= bytesToCopy;

  if (rest == 0)
  {
    msg->tp_state = ISOTP_FINISHED;
    Logger::logUdsMessage("receive_consecutive_frame", msg);
    Logger::debug("[receive_consecutive_frame] Last CF received with seq. ID: %u", msg->seq_id);
  }
  else
  {
    Logger::logUdsMessage("receive_consecutive_frame", msg);
    Logger::debug("[receive_consecutive_frame] CF received with seq. ID: %u", msg->seq_id);
    Logger::debug("[receive_consecutive_frame] Remaining bytes: %u", rest);
  }

  msg->seq_id = (msg->seq_id + 1) & 0x0F; // Wrap sequence number

  return 0;
}

uint8_t IsoTp::receive_flow_control(struct Message_t *msg)
{

  uint8_t retval = 0;

  if (msg->tp_state != ISOTP_WAIT_FC && msg->tp_state != ISOTP_WAIT_FIRST_FC)
    return 0;

  /* get communication parameters only from the first FC frame */
  if (msg->tp_state == ISOTP_WAIT_FIRST_FC)
  {
    msg->blocksize = rxBuffer[1];
    msg->min_sep_time = rxBuffer[2];

    /* fix wrong separation time values according spec */
    if ((msg->min_sep_time > 0x7F) && ((msg->min_sep_time < 0xF1) || (msg->min_sep_time > 0xF9)))
      msg->min_sep_time = 0x7F;
  }

  Logger::debug("[receive_flow_control] FC frame: FS %u, Blocksize %u, Min. separation Time %u", rxBuffer[0] & 0x0F, msg->blocksize, msg->min_sep_time);

  switch (rxBuffer[0] & 0x0F)
  {
    case ISOTP_FC_CTS:
      msg->tp_state = ISOTP_SEND_CF;
      break;
    case ISOTP_FC_WT:
      fc_wait_frames++;
      if (fc_wait_frames >= MAX_FCWAIT_FRAME)
      {
        Logger::warn("[receive_flow_control] FC wait frames exceeded.");
        fc_wait_frames = 0;
        msg->tp_state = ISOTP_IDLE;
        retval = 1;
      }
      Logger::debug("[receive_flow_control] Start waiting for next FC");
      break;
    case ISOTP_FC_OVFLW:
      Logger::warn("[receive_flow_control] Overflow in receiver side");
      msg->tp_state = ISOTP_IDLE;
      retval = 1;
      break;
  }
  return retval;
}

uint8_t IsoTp::send(Message_t *msg)
{
  uint8_t bs = false;
  uint32_t delta = 0;
  uint8_t retval = 0;

  msg->tp_state = ISOTP_SEND;

  while (msg->tp_state != ISOTP_IDLE && msg->tp_state != ISOTP_ERROR && msg->tp_state != ISOTP_FINISHED)
  {
    bs = false;

    switch (msg->tp_state)
    {
      case ISOTP_SEND:
        if (msg->len <= 7)
        {
          retval = send_single_frame(msg);

          msg->tp_state = ISOTP_FINISHED;

          Logger::logUdsMessage("[send] Single frame", msg);
        }
        else
        {
          if (!(retval = send_first_frame(msg)))
          {
            msg->Buffer += 6;
            msg->len -= 6;
            msg->tp_state = ISOTP_WAIT_FIRST_FC;
            fc_wait_frames = 0;
            wait_fc = millis();

            Logger::logUdsMessage("[send] First frame", msg);
          }
        }
      break;

    case ISOTP_WAIT_FIRST_FC:
      delta = millis() - wait_fc;
      if (delta >= TIMEOUT_FC)
      {
        msg->tp_state = ISOTP_ERROR;

        Logger::logUdsMessage("[send] Timeout waiting for first flow control", msg);

        return 1;
      }
      else if (is_can_message_available())
      {
        unsigned long rx_id;
        byte ext;
        _bus->peekMsgId(&rx_id, &ext);

        if (rx_id == msg->rx_id)
        {
          _bus->readMsgBufID(&rx_id, &rxLen, rxBuffer);
          
          retval = receive_flow_control(msg);

          memset(rxBuffer, 0, sizeof(rxBuffer));
        }
      }
      break;

    case ISOTP_SEND_CF:
      while (msg->len > 7 && !bs)
      {
        flow_control_delay(msg->min_sep_time);

        if (!(retval = send_consecutive_frame(msg)))
        {
          if (msg->blocksize > 0 && !(msg->seq_id % msg->blocksize))
          {
            bs = true;
            msg->tp_state = ISOTP_WAIT_FC;
          }

          msg->seq_id = (msg->seq_id + 1) % 0x10;
          msg->Buffer += 7;
          msg->len -= 7;
        }
      }
      if (!bs && msg->len > 0)
      {
        flow_control_delay(msg->min_sep_time);

        retval = send_consecutive_frame(msg);

        msg->tp_state = ISOTP_FINISHED;
      }
      break;

    case ISOTP_WAIT_FC:
      delta = millis() - wait_fc;
      if (delta >= TIMEOUT_FC)
      {
        msg->tp_state = ISOTP_ERROR;

        Logger::logUdsMessage("[send] Timeout waiting for next flow control frame", msg);

        return 1;
      }
      else if (is_can_message_available())
      {
        unsigned long rx_id;
        byte ext;
        _bus->peekMsgId(&rx_id, &ext);

        if (rx_id == msg->rx_id)
        {
          _bus->readMsgBufID(&rx_id, &rxLen, rxBuffer);

          retval = receive_flow_control(msg);

          memset(rxBuffer, 0, sizeof(rxBuffer));
        }
      }
      break;

    default:
      msg->tp_state = ISOTP_ERROR;
      Logger::logUdsMessage("[send] Unknown or unhandled state", msg);
      return 1;
    }

    // Yield to avoid watchdog reset
    vTaskDelay(pdMS_TO_TICKS(5));
  }

  return retval;
}

uint8_t IsoTp::receive(Message_t *msg)
{

  uint32_t startTime = millis();
  uint8_t expected_seq = 1;
  uint16_t totalLength = 0;
  uint16_t copiedBytes = 0;

  msg->len = 0;
  msg->tp_state = ISOTP_IDLE;

  while ((millis() - startTime) < TIMEOUT_SESSION)
  {
    // Keep reading all available frames
    while (is_can_message_available())
    {
      unsigned long rx_id;
      byte ext;
      _bus->peekMsgId(&rx_id, &ext);

      // Skip irrelevant messages
      if (rx_id != msg->rx_id)
      {
        _bus->readMsgBufID(&rx_id, &rxLen, rxBuffer); // flush
        
        Logger::debug("[receive] Skipping frame with ID: 0x%lX", rx_id);

        continue;
      }

      // Read matching message
      _bus->readMsgBufID(&rx_id, &rxLen, rxBuffer);

      uint8_t pciType = rxBuffer[0] & 0xF0;

      switch (pciType)
      {
      case N_PCI_SF:
      {
        msg->len = rxBuffer[0] & 0x0F;
        memcpy(msg->Buffer, &rxBuffer[1], msg->len);
        msg->tp_state = ISOTP_FINISHED;

        Logger::logUdsMessage("[receive] receive_single_frame", msg);

        return 0;
      }

      case N_PCI_FF:
      {
        totalLength = ((rxBuffer[0] & 0x0F) << 8) | rxBuffer[1];
        copiedBytes = rxLen - 2;
        memcpy(msg->Buffer, &rxBuffer[2], copiedBytes);

        uint8_t expectedCFs = (totalLength - copiedBytes + 6) / 7;

        Logger::debug("[receive] First Frame: totalLength = %u, initial copied = %u, expected CFs = %u", totalLength, copiedBytes, expectedCFs);

        // Send Flow Control
        Message_t fc;
        fc.tx_id = msg->tx_id;
        fc.fc_status = ISOTP_FC_CTS;
        fc.blocksize = 0;
        fc.min_sep_time = 0;

        send_flow_control(&fc);

        expected_seq = 1;
        msg->tp_state = ISOTP_WAIT_DATA;
        startTime = millis(); // Reset timeout for CFs
        break;
      }

      case N_PCI_CF:
      {
        if (msg->tp_state != ISOTP_WAIT_DATA)
        {
          Logger::warn("[receive] Unexpected CF before FF");
          continue;
        }

        uint8_t seqNum = rxBuffer[0] & 0x0F;

        if (seqNum != expected_seq)
        {
          Logger::warn("[receive] Sequence mismatch: got %u, expected %u", seqNum, expected_seq);
          return 1;
        }

        uint8_t bytes_to_copy = min(rxLen - 1, totalLength - copiedBytes);
        memcpy(msg->Buffer + copiedBytes, &rxBuffer[1], bytes_to_copy);
        copiedBytes += bytes_to_copy;

        Logger::debug("[receive] CF %u received: copiedBytes = %u / %u (CF %u of %u)", seqNum, copiedBytes, totalLength, seqNum, (totalLength - 6 + 6) / 7);

        if (copiedBytes >= totalLength)
        {
          msg->len = totalLength;
          msg->tp_state = ISOTP_FINISHED;
          Logger::debug("[receive] All frames received, total length = %u", msg->len);
          return 0;
        }

        expected_seq = (expected_seq + 1) & 0x0F;
        startTime = millis(); // reset timeout after valid CF
        break;
      }

      default:
      {
        Logger::logUdsMessage("[receive] unknown_pci_type", msg);
        return 1;
      }
      }

      memset(rxBuffer, 0, sizeof(rxBuffer));
    }

    // Yield after consuming all messages
    vTaskDelay(pdMS_TO_TICKS(2));
  }

  if (msg->tp_state == ISOTP_WAIT_DATA)
  {
    Logger::logUdsMessage("[receive] incomplete_message", msg);
  }

  Logger::logUdsMessage("[receive] timeout", msg);
  return 1;
}

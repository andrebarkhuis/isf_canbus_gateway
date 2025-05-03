#include <SPI.h>
#include <Arduino.h>
#include "isotp/iso-tp.h"
#include "mcp_can/mcp_canbus.h"
#include "mcp_can/mcp_canbus_dfs.h"
#include "logger/logger.h"

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
  Logger::print_message_in("[IsoTp::send_flow_control]", msg);
#endif

  uint8_t TxBuf[8] = {0};

  TxBuf[0] = (N_PCI_FC | ISOTP_FC_CTS); // Explicitly Clear To Send (0x30)
  TxBuf[1] = 0x00;                      // No block size limit
  TxBuf[2] = 0x00;                      // No separation time required

#ifdef ISO_TP_DEBUG
  Logger::print_message_out("[IsoTp::send_flow_control] ", msg->tx_id, TxBuf, 8);
#endif

  return _bus->sendMsgBuf(msg->tx_id, 0, 8, reinterpret_cast<byte *>(TxBuf)); // Cast to byte*
}

uint8_t IsoTp::send_single_frame(struct Message_t *msg)
{
#ifdef ISO_TP_DEBUG
  Logger::print_message_in("[IsoTp::send_single_frame] ", msg);
#endif

  if (msg->len > 7)
    return RESULT_ERROR; // Error, too much data for single frame

  uint8_t TxBuf[8] = {0};
  TxBuf[0] = N_PCI_SF | (msg->len & 0x0F);
  memcpy(TxBuf + 1, msg->Buffer, msg->len);

#ifdef ISO_TP_DEBUG
  Logger::print_message_out("[IsoTp::send_single_frame] ", msg->tx_id, TxBuf, 8);
#endif

  return _bus->sendMsgBuf(msg->tx_id, 0, 8, reinterpret_cast<byte *>(TxBuf)); // Cast to byte*
}

uint8_t IsoTp::send_first_frame(struct Message_t *msg)
{
#ifdef ISO_TP_DEBUG
  Logger::print_message_in("[IsoTp::send_first_frame] ", msg);
#endif

  if (msg->len <= 7)
    return RESULT_ERROR; // Error, use SF for 7 or fewer bytes

  uint8_t TxBuf[8] = {0};
  msg->seq_id = 1;

  TxBuf[0] = N_PCI_FF | ((msg->len >> 8) & 0x0F);
  TxBuf[1] = msg->len & 0xFF;
  memcpy(TxBuf + 2, msg->Buffer, 6); // First 6 bytes payload

#ifdef ISO_TP_DEBUG
  Logger::print_message_out("[IsoTp::send_first_frame]", msg->tx_id, TxBuf, 8);
#endif

  return _bus->sendMsgBuf(msg->tx_id, 0, 8, reinterpret_cast<byte *>(TxBuf)); // Cast to byte*
}

uint8_t IsoTp::send_consecutive_frame(struct Message_t *msg)
{
#ifdef ISO_TP_DEBUG
  Logger::print_message_in("[IsoTp::send_consecutive_frame]", msg);
#endif

  uint8_t TxBuf[8] = {0};
  uint8_t payload_len = (msg->len > 7) ? 7 : msg->len;

  TxBuf[0] = N_PCI_CF | (msg->seq_id & 0x0F);
  memcpy(TxBuf + 1, msg->Buffer, payload_len);

#ifdef ISO_TP_DEBUG
  Logger::print_message_out("[IsoTp::send_consecutive_frame]", msg->tx_id, TxBuf, 8);
#endif

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
#ifdef ISO_TP_DEBUG
  Logger::print_message_in("[IsoTp::receive_single_frame]", msg);
#endif

  /* get the SF_DL from the N_PCI byte */
  msg->len = rxBuffer[0] & 0x0F;
  /* copy the received data bytes */
  memcpy(msg->Buffer, rxBuffer + 1, msg->len); // Skip PCI, SF uses len bytes
  msg->tp_state = ISOTP_FINISHED;

  return RESULT_OK;
}

uint8_t IsoTp::receive_first_frame(struct Message_t *msg)
{
#ifdef ISO_TP_DEBUG
  Logger::print_message_in("[IsoTp::receive_first_frame]", msg);
#endif

  msg->seq_id = 1; // Reset expected sequence number explicitly after receiving first frame

  /* get the FF_DL */
  msg->len = (rxBuffer[0] & 0x0F) << 8;
  msg->len += rxBuffer[1];
  rest = msg->len;

  /* copy the first received data bytes */
  memcpy(msg->Buffer, rxBuffer + 2, 6); // Skip 2 bytes PCI, FF must have 6 bytes!
  rest -= 6;                            // Rest length

  msg->tp_state = ISOTP_WAIT_DATA;

#ifdef ISO_TP_INFO_PRINT
  Logger::debug("[IsoTp::receive_first_frame] First Frame received. Total Length: %u", msg->len);
  Logger::debug("[IsoTp::receive_first_frame] Initial bytes copied: 6, Remaining: %u", rest);
  Logger::debug("[IsoTp::receive_first_frame] Sending Flow Control Frame.");
#endif

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
#ifdef ISO_TP_DEBUG
  Logger::print_message_in("[IsoTp::receive_consecutive_frame]", msg);
#endif

  uint32_t delta = millis() - wait_cf;

  if ((delta >= TIMEOUT_FC) && msg->seq_id > 1)
  {
#ifdef ISO_TP_INFO_PRINT
    Logger::debug("[IsoTp::receive_consecutive_frame] Timeout waiting for CF. wait_cf=%lu delta=%lu", wait_cf, delta);
#endif
    msg->tp_state = ISOTP_IDLE;
    return RESULT_ERROR;
  }
  wait_cf = millis();

  uint8_t receivedSeqId = rxBuffer[0] & 0x0F;
  uint8_t expectedSeqId = msg->seq_id & 0x0F;

  if (receivedSeqId != expectedSeqId)
  {
#ifdef ISO_TP_INFO_PRINT
    Logger::warn("[IsoTp::receive_consecutive_frame] Sequence mismatch. Got Seq (%u), Expected Seq (%u)", receivedSeqId, expectedSeqId);
#endif
    msg->tp_state = ISOTP_ERROR; // Use enum value, not macro
    return RESULT_ERROR;
  }

  uint8_t bytesToCopy = (rest <= 7) ? rest : 7;
  memcpy(msg->Buffer + msg->len - rest, rxBuffer + 1, bytesToCopy);
  rest -= bytesToCopy;

  if (rest == 0)
  {
#ifdef ISO_TP_INFO_PRINT
    Logger::debug("[IsoTp::receive_consecutive_frame] Last CF received with seq. ID: %u", msg->seq_id);
#endif
    msg->tp_state = ISOTP_FINISHED;
  }
  else
  {
#ifdef ISO_TP_INFO_PRINT
    Logger::debug("[IsoTp::receive_consecutive_frame] CF received with seq. ID: %u, remaining bytes: %u", msg->seq_id, rest);
#endif
  }

  msg->seq_id = (msg->seq_id + 1) & 0x0F; // Wrap sequence number

  return RESULT_OK;
}

uint8_t IsoTp::receive_flow_control(struct Message_t *msg)
{
#ifdef ISO_TP_DEBUG
  Logger::print_message_in("[IsoTp::receive_flow_control]", msg);
#endif

  uint8_t retval = RESULT_OK;

  if (msg->tp_state != ISOTP_WAIT_FC && msg->tp_state != ISOTP_WAIT_FIRST_FC)
    return RESULT_OK;

  /* get communication parameters only from the first FC frame */
  if (msg->tp_state == ISOTP_WAIT_FIRST_FC)
  {
    msg->blocksize = rxBuffer[1];
    msg->min_sep_time = rxBuffer[2];

    /* fix wrong separation time values according spec */
    if ((msg->min_sep_time > 0x7F) && ((msg->min_sep_time < 0xF1) || (msg->min_sep_time > 0xF9)))
      msg->min_sep_time = 0x7F;
  }

#ifdef ISO_TP_DEBUG
  Logger::debug("[IsoTp::receive_flow_control] Received Flow Control frame %u, blocksize %u, Min. separation Time %u", (rxBuffer[0] & 0x0F), msg->blocksize, msg->min_sep_time);
#endif

  switch (rxBuffer[0] & 0x0F)
  {
  case ISOTP_FC_CTS:
    msg->tp_state = ISOTP_SEND_CF;
    break;
  case ISOTP_FC_WT:
    fc_wait_frames++;
    if (fc_wait_frames >= MAX_FCWAIT_FRAME)
    {
#ifdef ISO_TP_DEBUG
      Logger::debug("[IsoTp::receive_flow_control] FC wait frames exceeded.");
#endif
      fc_wait_frames = 0;
      msg->tp_state = ISOTP_IDLE;
      retval = RESULT_ERROR;
    }
#ifdef ISO_TP_DEBUG
    Logger::debug("[IsoTp::receive_flow_control] Start waiting for next FC.");
#endif
    break;
  case ISOTP_FC_OVFLW:
#ifdef ISO_TP_DEBUG
    Logger::debug("[IsoTp::receive_flow_control] FC Overflow in receiver side.");
#endif
    msg->tp_state = ISOTP_IDLE;
    retval = RESULT_ERROR;
    break;
  }
  return retval;
}

uint8_t IsoTp::send(Message_t *msg)
{
#ifdef ISO_TP_DEBUG
  Logger::print_message_in("[IsoTp::send]", msg);
#endif

  uint8_t bs = false;
  uint32_t delta = 0;
  uint8_t retval = RESULT_OK;
  msg->tp_state = ISOTP_SEND;

  uint8_t *localBuffer = msg->Buffer;
  uint16_t localLen = msg->len;

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

#ifdef ISO_TP_DEBUG
        Logger::logUdsMessage("IsoTp::send. Sending single frame.", msg);
#endif
      }
      else
      {
        if (!(retval = send_first_frame(msg)))
        {
          localBuffer += 6;
          localLen -= 6;
          msg->tp_state = ISOTP_WAIT_FIRST_FC;
          fc_wait_frames = 0;
          wait_fc = millis();
        }
      }
      break;

    case ISOTP_WAIT_FIRST_FC:
      delta = millis() - wait_fc;
      if (delta >= TIMEOUT_FC)
      {
        msg->tp_state = ISOTP_ERROR; // Use enum value, not macro

#ifdef ISO_TP_DEBUG
        Logger::logUdsMessage("WARN::IsoTp::send. Timeout waiting for first flow control.", msg);
#endif
        return RESULT_ERROR;
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
      while (localLen > 7 && !bs)
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
          localBuffer += 7;
          localLen -= 7;
        }
      }
      if (!bs && localLen > 0)
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
        msg->tp_state = ISOTP_ERROR; // Use enum value, not macro

#ifdef ISO_TP_DEBUG
        Logger::logUdsMessage("WARN::IsoTp::send. Timeout waiting for first flow control.", msg);
#endif

        return RESULT_ERROR;
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
      msg->tp_state = ISOTP_ERROR; // Use enum value, not macro
#ifdef ISO_TP_DEBUG
      Logger::error("[IsoTp::send] Unknown or unhandled state.");
#endif
      return RESULT_ERROR;
    }

    // Yield to avoid watchdog reset
    vTaskDelay(pdMS_TO_TICKS(5));
  }

  return retval;
}

uint8_t IsoTp::receive(Message_t *msg)
{

#ifdef ISO_TP_DEBUG
  Logger::print_message_in("[IsoTp::receive]", msg);
#endif

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

#ifdef ISO_TP_DEBUG
      Logger::logUdsMessage("IsoTp::receive", msg);
#endif

      // Skip irrelevant messages
      if (rx_id != msg->rx_id)
      {
        _bus->readMsgBufID(&rx_id, &rxLen, rxBuffer); // flush
#ifdef ISO_TP_DEBUG
        Logger::debug("[IsoTp::receive] Skipping irrelevant frame with ID 0x%lX", rx_id);
#endif
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

#ifdef ISO_TP_DEBUG
        Logger::logUdsMessage("IsoTp::receive. Single Frame", msg);
#endif
        return RESULT_OK;
      }

      case N_PCI_FF:
      {
        totalLength = ((rxBuffer[0] & 0x0F) << 8) | rxBuffer[1];
        copiedBytes = rxLen - 2;
        memcpy(msg->Buffer, &rxBuffer[2], copiedBytes);

#ifdef ISO_TP_DEBUG
        uint8_t expectedCFs = (totalLength - copiedBytes + 6) / 7;
        Logger::debug("[IsoTp::receive] First Frame. Total byte length %u. Bytes copied %u. Expect %u CFs", totalLength, copiedBytes, expectedCFs);
#endif

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
#ifdef ISO_TP_DEBUG
          Logger::warn("[IsoTp::receive] Unexpected CF before FF");
#endif
          continue;
        }

        uint8_t seqNum = rxBuffer[0] & 0x0F;

        if (seqNum != expected_seq)
        {
#ifdef ISO_TP_DEBUG
          Logger::warn("[IsoTp::receive] Sequence mismatch. (Got seq %u, Expected seq %u)", seqNum, expected_seq);
#endif
          return RESULT_ERROR;
        }

        uint8_t bytes_to_copy = min(rxLen - 1, totalLength - copiedBytes);
        memcpy(msg->Buffer + copiedBytes, &rxBuffer[1], bytes_to_copy);
        copiedBytes += bytes_to_copy;

#ifdef ISO_TP_DEBUG
        Logger::debug("[IsoTp::receive] CF %u received. Bytes copied %u of %u (CF seq %u of %u)", seqNum, copiedBytes, totalLength, seqNum, bytes_to_copy);
#endif

        if (copiedBytes >= totalLength)
        {
#ifdef ISO_TP_DEBUG
          Logger::debug("[IsoTp::receive] All frames received, total length %u", msg->len);
#endif

          msg->len = totalLength;
          msg->tp_state = ISOTP_FINISHED;
          return RESULT_OK;
        }

        expected_seq = (expected_seq + 1) & 0x0F;
        startTime = millis(); // reset timeout after valid CF
        break;
      }

      default:
      {
        Logger::warn("[IsoTp::receive] Unknown PCI type: 0x%02X", pciType);
        return RESULT_ERROR;
      }
      }

      memset(rxBuffer, 0, sizeof(rxBuffer));
    }

    // Yield after consuming all messages
    vTaskDelay(pdMS_TO_TICKS(2));
  }

  if (msg->tp_state == ISOTP_WAIT_DATA)
  {
    Logger::error("[IsoTp::receive] Incomplete ISO-TP message. Expected more CFs");
  }

  Logger::warn("[IsoTp::receive] Timeout after %u ms", TIMEOUT_SESSION);
  return RESULT_ERROR;
}

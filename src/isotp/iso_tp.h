#pragma once

#include "../mcp_can/mcp_can.h"
#include "../common.h"
#include <stdint.h> // Add explicit include for standard integer types

// #define ISO_TP_DEBUG
// #define ISO_TP_INFO_PRINT

#define CAN_MAX_DLEN 8 // Not extended CAN

/* N_PCI type values in bits 7-4 of N_PCI bytes */
#define N_PCI_SF 0x00 /* single frame */
#define N_PCI_FF 0x10 /* first frame */
#define N_PCI_CF 0x20 /* consecutive frame */
#define N_PCI_FC 0x30 /* flow control */

#define FC_CONTENT_SZ 3 /* flow control content size in byte (FS/BS/STmin) */

/* Flow Status given in FC frame */
#define ISOTP_FC_CTS 0   /* clear to send */
#define ISOTP_FC_WT 1    /* wait */
#define ISOTP_FC_OVFLW 2 /* overflow */
/* Timeout values - adjusted to align with ISO 15765-2 */
#define TIMEOUT_SESSION 1000 /* Timeout between successful send and receive (N_As) */
#define TIMEOUT_FC 1000      /* Timeout between FF and FC or Block CF and FC (N_Bs) */
#define TIMEOUT_CF 1000      /* Timeout between CFs (N_Cr) */
#define MAX_FCWAIT_FRAME 128

#define MAX_MSGBUF 128 /* Received Message Buffer. Depends on uC ressources! Should be enough for our needs */


class IsoTp
{
public:
  IsoTp(MCP_CAN *bus);
  uint8_t send(Message_t *msg);
  uint8_t receive(Message_t *msg);
  
private:
  MCP_CAN *_bus;
  uint8_t rxLen;
  uint8_t rxBuffer[8];
  uint16_t remaining_bytes_to_copy;
  uint8_t fc_wait_frames = 0;
  uint32_t wait_fc = 0;
  uint32_t wait_cf = 0;
  uint32_t wait_session = 0;
  uint8_t can_send(uint32_t id, uint8_t len, uint8_t *data);
  bool is_can_message_available(void);
  uint8_t send_flow_control(struct Message_t *msg);
  uint8_t send_single_frame(struct Message_t *msg);
  uint8_t send_first_frame(struct Message_t *msg);
  uint8_t send_consecutive_frame(struct Message_t *msg);
  uint8_t receive_single_frame(struct Message_t *msg);
  uint8_t receive_first_frame(struct Message_t *msg);
  uint8_t receive_consecutive_frame(struct Message_t *msg);
  uint8_t receive_flow_control(struct Message_t *msg);
  void flow_control_delay(uint8_t sep_time);
  void reset_state();  // Reset buffer and internal state
};

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

#define MAX_DATA (MAX_MSGBUF - 1)
#define UDS_RETRY 3
#define UDS_TIMEOUT 1000 // 500 -> 5000
#define UDS_KEEPALIVE 3000

/* OBD-II Modes */
#define OBD_MODE_SHOW_CURRENT_DATA 0x01
#define OBD_MODE_SHOW_FREEZE_FRAME 0x02
#define OBD_MODE_READ_DTC 0x03
#define OBD_MODE_CLEAR_DTC 0x04
#define OBD_MODE_TEST_RESULTS_NON_CAN 0x05
#define OBD_MODE_TEST_RESULTS_CAN 0x06
#define OBD_MODE_READ_PENDING_DTC 0x07
#define OBD_MODE_CONTROL_OPERATIONS 0x08
#define OBD_MODE_VEHICLE_INFORMATION 0x09
#define OBD_MODE_READ_PERM_DTC 0x0A

//* UDS Service Identifiers (ISO 14229-1) */
#define UDS_SID_DIAGNOSTIC_SESSION_CONTROL 0x10
#define UDS_SID_ECU_RESET 0x11
#define UDS_SID_CLEAR_DTC 0x14
#define UDS_SID_READ_DTC 0x19
#define UDS_SID_READ_DATA_BY_LOCAL_ID 0x21 // Techstream
#define UDS_SID_READ_DATA_BY_ID 0x22
#define UDS_SID_READ_MEM_BY_ADDRESS 0x23
#define UDS_SID_READ_SCALING_BY_ID 0x24
#define UDS_SID_SECURITY_ACCESS_REQUEST_SEED 0x27
#define UDS_SID_SECURITY_ACCESS_SEND_KEY 0x27
#define UDS_SID_READ_DATA_BY_ID_PERIODIC 0x2A
#define UDS_SID_DEFINE_DATA_ID 0x2C
#define UDS_SID_WRITE_DATA_BY_ID 0x2E
#define UDS_SID_IO_CONTROL_BY_ID 0x2F
#define UDS_SID_ROUTINE_CONTROL 0x31
#define UDS_SID_REQUEST_DOWNLOAD 0x34
#define UDS_SID_REQUEST_UPLOAD 0x35
#define UDS_SID_TRANSFER_DATA 0x36
#define UDS_SID_REQUEST_XFER_EXIT 0x37
#define UDS_SID_WRITE_MEM_BY_ADDRESS 0x3D
#define UDS_SID_TESTER_PRESENT 0x3E
#define UDS_SID_ACCESS_TIMING 0x83
#define UDS_SID_SECURED_DATA_TRANS 0x84
#define UDS_SID_CONTROL_DTC_SETTINGS 0x85
#define UDS_SID_RESPONSE_ON_EVENT 0x86
#define UDS_SID_LINK_CONTROL 0x87

/* UDS Response Codes */
#define UDS_POSITIVE_RESPONSE(SID) ((SID) + 0x40)
#define UDS_NEGATIVE_RESPONSE 0x7F

// Negative response codes
#define UDS_NRC_INVALID_KEY 0x35
#define UDS_NRC_EXCEEDED_NUMBER_OF_ATTEMPTS 0x36
#define UDS_NRC_REQUIRED_TIME_DELAY_NOT_EXPIRED 0x37

// Security access subfunctions
#define UDS_REQUEST_SEED 0x01
#define UDS_SEND_KEY 0x02

/* UDS Error Codes */
#define UDS_ERROR_ID 0x7F
#define UDS_NRC_SUCCESS 0x00
#define UDS_NRC_SERVICE_NOT_SUPPORTED 0x11
#define UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED 0x12
#define UDS_NRC_INCORRECT_LENGTH_OR_FORMAT 0x13
#define UDS_NRC_CONDITIONS_NOT_CORRECT 0x22
#define UDS_NRC_REQUEST_OUT_OF_RANGE 0x31
#define UDS_NRC_SECURITY_ACCESS_DENIED 0x33
#define UDS_NRC_INVALID_KEY 0x35
#define UDS_NRC_TOO_MANY_ATTEMPS 0x36
#define UDS_NRC_TIME_DELAY_NOT_EXPIRED 0x37
#define UDS_NRC_RESPONSE_PENDING 0x78


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
  bool first_frame_received;
  uint8_t can_send(uint32_t id, uint8_t len, uint8_t *data);
  bool is_can_message_available(void);
  uint8_t send_flow_control(struct Message_t *msg);
  uint8_t send_single_frame(struct Message_t *msg);
  uint8_t receive_single_frame(struct Message_t *msg);
  uint8_t receive_consecutive_frame(struct Message_t *msg);
  uint8_t receive_flow_control(struct Message_t *msg);
  bool is_next_consecutive_frame(Message_t *msg, unsigned long actual_rx_id, unsigned actual_tx_id, uint8_t actual_seq_num, uint8_t actual_serviceId, uint16_t actual_data_id);
  void reset_state();  // Reset buffer and internal state
  
  // Frame handler functions
  uint8_t handle_first_frame(Message_t *msg);
  uint8_t handle_consecutive_frame(Message_t *msg);
  uint8_t handle_single_frame(Message_t *msg);
};

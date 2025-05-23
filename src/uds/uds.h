#pragma once

#include "../isotp/iso_tp.h"
#include "../mcp_can/mcp_can.h"

// #define UDS_DEBUG

#define MAX_DATA (MAX_MSGBUF - 1)
#define UDS_RETRY 2
#define UDS_TIMEOUT 5000 // 500 -> 5000
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

extern volatile bool uds_busy;

enum UnitType {
  UNIT_GENERAL = 0, // Generic / ECU Identifiers
  UNIT_ACCELERATION = 1, // Acceleration, Gradient
  UNIT_G_FORCE = 2, // G-force sensors
  UNIT_ACCEL_REQUEST = 3, // Acceleration request signals
  UNIT_DECELERATION = 4, // Deceleration sensor
  UNIT_IGNITION_FEEDBACK = 5, // Ignition timing and feedback
  UNIT_ANGLE_SENSOR = 6, // Absolute angles (Steering, Pinion)
  UNIT_YAW_RATE = 7, // Yaw rate sensors
  UNIT_CURRENT_SENSOR = 9, // Current draw sensors (Throttle, Clutch)
  UNIT_PM_SENSOR = 11, // Particulate Matter Sensors
  UNIT_DISTANCE = 13, // Distance / Mileage / Odometer-related
  UNIT_FORWARD_DISTANCE = 14, // Forward vehicle distance measurement
  UNIT_ODOMETER = 17, // Odometer history and mileage
  UNIT_BATTERY_STATUS = 18, // Battery charge, hybrid systems
  UNIT_POWER_MANAGEMENT = 19, // Request Power, Wout Control
  UNIT_HYBRID_BATTERY = 22, // Hybrid / EV Battery power levels
  UNIT_FUEL_SYSTEM = 23, // Fuel system-related parameters
  UNIT_FUEL_INJECTION = 24, // Injection volume, fuel pump parameters
  UNIT_CRUISE_CONTROL = 25, // Cruise control request forces
  UNIT_FREQUENCY_SENSOR = 29, // Frequency-based sensors (Motor, Generator)
  UNIT_ILLUMINATION_SENSOR = 30, // Light control, brightness
  UNIT_EXHAUST_SENSOR = 32, // NOx and exhaust emissions
  UNIT_LOAD_FUEL_TRIM = 33, // Load calculations, Fuel trims
  UNIT_MAP_TIRE_PRESSURE = 34, // Manifold Pressure and Tire Inflation sensors
  UNIT_ENGINE_RPM = 39, // Engine Speed, RPM
  UNIT_SPEED_SENSOR = 42, // Vehicle Speed Sensors
  UNIT_VOLTAGE_SENSOR = 48, // Oxygen Sensor, Solar Voltage, Battery Voltage
  UNIT_TEMPERATURE_SENSOR = 57, // Coolant Temperature, Intake Air Temp
  UNIT_TORQUE_SENSOR = 58, // Steering, Motor, Brake Torque
  UNIT_POSITION_SENSOR = 59, // Throttle, Clutch, ASL Gear Position
  UNIT_AMBIENT_TEMP = 66, // Outside temperature sensors
  UNIT_MASS_AIR_FLOW = 75 // MAF Sensors (filtered & raw values)
};

struct Session_t
{
  uint32_t tx_id = 0;
  uint32_t rx_id = 0;
  uint8_t sid = 0;
  uint8_t *Data;
  uint8_t len = 0;
  uint8_t lenSub = 0; // Length of Sub-function and PID(DID or RID)
};

class UDS
{
private:
  IsoTp *_isotp;

  uint8_t tmpbuf[MAX_DATA];

  uint16_t send(Message_t &msg);

  void handlePendingResponse(Message_t &msg, boolean &isPending);

  uint16_t handleNegativeResponse(Message_t &msg, Session_t *session);

  void handlePositiveResponse(Message_t &msg, Session_t *session);

  uint16_t handleUDSResponse(Message_t &msg, Session_t *session, boolean &isPendingResponse);

public:
  UDS(IsoTp *isotp);
  uint16_t Session(Session_t *msg);
};

/*
 * COMMS.h
 *
 * Created on: Feb 17, 2026
 * Author: J2050 TT&C
 * Description: Header file for the RedPill Satellite Telecommunication Subsystem.
 * Contains configuration, packet structures, and API prototypes.
 */

#ifndef COMMS_H
#define COMMS_H

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// C-COMPATIBLE INCLUDES & API (For main.c)
// -----------------------------------------------------------------------------
#include "main.h"
#include "cmsis_os.h"

// Entry point for the OBC Mission Loop (Called from main.c)
void app_OBC_loop(void *argument);

#ifdef __cplusplus
}
#endif

// -----------------------------------------------------------------------------
// C++ INCLUDES & DEFINITIONS
// -----------------------------------------------------------------------------
#ifdef __cplusplus

// Standard Libraries
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <stdint.h>

// FreeRTOS
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

// Hardware & Drivers
#include <RadioLib.h>
#include "STM32Hal.h"
#include "sha256.h"

extern "C" {
    #include "ecc.h"
}

// =============================================================================
// SECTION 1: HARDWARE & RADIO CONFIGURATION
// =============================================================================

// RadioLib Virtual Pin IDs (Mapped to CubeMX pins in COMMS.cpp)
#define RLIB_NSS   0
#define RLIB_RESET 1
#define RLIB_DIO1  2
#define RLIB_BUSY  3

// LoRa Modulation Parameters
#define LORA_FREQ           436.0f   // Frequency [MHz]
#define LORA_BW             125.0f   // Bandwidth [kHz]
#define LORA_SF             10       // Spreading Factor
#define LORA_CR             5        // Coding Rate (4/5)
#define LORA_SYNC           0x12     // Sync Word (Private Network)
#define LORA_PWR            22       // Output Power [dBm]
#define LORA_PREAMBLE       8        // Preamble Length [Symbols]
#define LORA_TCXO_V         0.0f     // TCXO Voltage (0.0 = Not present)
#define LORA_USE_LDO        false    // Regulator (false = DCDC, true = LDO)

// =============================================================================
// SECTION 2: SYSTEM STATES & TIMING
// =============================================================================

// State Machine States
#define COMMS_IDLE          0   // Waiting for events
#define COMMS_TX            1   // Transmitting packet
#define COMMS_TX_ERROR      2   // Transmission failed
#define COMMS_RX            3   // Processing received packet
#define COMMS_RX_ERROR      4   // Reception error (CRC/ECC)
#define COMMS_CMD           5   // Executing telecommand
#define COMMS_ERROR         6   // General error state

// Timing Configuration
#define IDLE_TIMEOUT        500 // [ms] Time to wait in IDLE before checking queues
#define TX_QUEUE_SIZE       6   // [Packets] Max TX queue depth
#define CMD_QUEUE_SIZE      2   // [Packets] Max Command queue depth

// Subsystem States (TX / Crystal)
#define TX_OFF              0x00
#define TX_ON               0x01
#define TX_NOBEACON         0x02

#define CRY_OFF             0x00
#define CRY_LIGHT           0x01
#define CRY_DARK            0x02

// =============================================================================
// SECTION 3: PACKET STRUCTURE & ERROR CODES
// =============================================================================

// Packet Limits
#define PACKET_SIZE_MAX         128
#define PACKET_HEADER_LENGTH    12
#define PACKET_PAYLOAD_MAX      98

// Error Codes
#define PACKET_ERR_NONE         0
#define PACKET_ERR_RS           -1  // Reed-Solomon Error
#define PACKET_ERR_DECODE       -2  // Decoding Failed
#define PACKET_ERR_LENGTH       -3  // Invalid Length
#define PACKET_ERR_MAC          -4  // Invalid MAC/Auth
#define PACKET_ERR_CMD_FULL     -5  // Command Queue Full
#define PACKET_ERR_CMD_POINTER  -6  // Null Pointer
#define PACKET_ERR_CMD_UNKNOWN  -7  // Unknown Command ID
#define PACKET_ERR_CMD_PAYLOAD  -8  // Invalid Command Payload
#define PACKET_ERR_CMD_MEMORY   -9  // Memory Allocation Fail

// Reed-Solomon Config
#define RS_OFF                  0x55
#define RS_ON                   0xAA
#define RS_PADDING              0x00
#define RS_BLOCK_SIZE           16
#ifndef NPAR
#define NPAR 2
#endif
#define DATA_BLOCK_SIZE (RS_BLOCK_SIZE - NPAR)

// -----------------------------------------------------------------------------
// STRUCT: Packet
// Represents a telecommunication packet in memory (decoded)
// -----------------------------------------------------------------------------
struct Packet {
    bool ecc;           // Reed-Solomon Enabled
    bool mac_valid;     // MAC Verification Status
    int8_t state;       // Internal Packet State/Error Code

    uint8_t station;    // Ground Station ID
    uint8_t command;    // Command ID (TEC/TER)
    uint8_t payload_length;
    uint32_t time_unix; // Timestamp
    uint32_t MAC;       // Message Authentication Code
    uint8_t payload[PACKET_PAYLOAD_MAX];

    // Methods
    void init(bool rs_enabled, uint8_t cmd);
    int8_t setPayload(const uint8_t* data, uint8_t length);
    int8_t seal(); // Calculate MAC and finalize packet
};

// Struct for Delayed Command Timer
struct TimerPayload {
    Packet* packet;
    TimerHandle_t* global_handle;
};

// =============================================================================
// SECTION 4: COMMAND DEFINITIONS (TEC & TER)
// =============================================================================

#define MISSION_ID          0x01

// Telecommands (TEC) - Uplink
#define TEC_OBC_REBOOT      0x01
#define TEC_EXIT_STATE      0x02
#define TEC_VAR_CHANGE      0x03
#define TEC_SET_TIME        0x04
#define TEC_EPS_REBOOT      0x08
#define TEC_ADCS_REBOOT     0x10
#define TEC_ADCS_TLE        0x11
#define TEC_LORA_STATE      0x18
#define TEC_LORA_CONFIG     0x19
#define TEC_LORA_PING       0x1A // Ping/Link Check
#define TEC_CRY_EXP         0x80

// Telemetry Responses (TER) - Downlink
#define TER_BEACON          0x30
#define TER_ACK             0x31
#define TER_NACK            0x32
#define TER_LORA_PONG       0x33

// =============================================================================
// SECTION 5: GLOBAL VARIABLES & HANDLES
// =============================================================================

// Radio Hardware
extern STM32Hal radioHal;
extern SX1268 radio;

// RTOS Handles
extern TaskHandle_t COMMSTaskHandle;
extern QueueHandle_t RTOS_queue_TX;
extern QueueHandle_t RTOS_queue_cmd;

// Timers
extern TimerHandle_t RTOS_timer_lora_state;
extern TimerHandle_t RTOS_timer_lora_config;
extern TimerHandle_t RTOS_timer_cry_state;

// System Status
extern uint8_t tx_state;
extern bool rs_enabled;
extern uint8_t cry_state;

// =============================================================================
// SECTION 6: FUNCTION PROTOTYPES
// =============================================================================

// State Machine
void COMMS_stateMachine(void *parameter);
void PowerUp_COMMS(void);
void PowerDown_COMMS(void);

// Radio Operations
void Init_Radio_Hardware(void);
void startReception(void);
void startTransmission(uint8_t *tx_packet, uint8_t packet_size);
void printRadioStatus(int16_t state, bool blocking);

// Utilities
void printStartupMessage(const char* device);
void printData(const char* prefix, const uint8_t* packet, uint8_t length);
void packetEvent(void); // Legacy stub

// Data Conversion & ECC
bool isDataECCEnabled(const uint8_t* data, uint8_t length);
void encodeECC(uint8_t* data, uint8_t& data_len);
int8_t decodeECC(uint8_t* data, uint8_t& data_len);
void dataToPacket(const uint8_t* data, uint8_t length, Packet* packet);
uint8_t packetToData(const Packet* packet, uint8_t* data);

// Command Execution
int8_t executeTEC(const Packet* cmd);
bool isTEC(uint8_t command);
bool isACKNeeded(const Packet* packet);
bool isACKNeededBefore(const Packet* packet);
void sendACK(bool ecc, uint8_t TEC);
void sendNACK(bool ecc, uint8_t TEC, int8_t error);

// Delayed Commands (Timers)
void createDelayedCommand(const Packet* cmd, void (*callback)(TimerHandle_t), uint32_t delay_seconds, void (*editDelayedCommand)(Packet*), TimerHandle_t* global_timer_handle, const char* timer_name);
void queueDelayedPacket(TimerHandle_t xTimer);
void deleteTimerIfExists(TimerHandle_t* timer_handle, const char* timer_name);
void editDelayedLoraState(Packet* packet);
void editDelayedLoraConfig(Packet* packet);

#endif // C++
#endif // COMMS_H

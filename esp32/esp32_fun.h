#ifndef ESP32_H

#define ESP32_H
// #define GS_MODE 0 // define GS_MODE to enable serial input mode

// Arduino library
#include <Arduino.h>

// RadioLib library for communication management
#include <RadioLib.h>

// Reed-Salomon library for error correction
// #define NPAR 2 // number of parity bytes for Reed-Salomon encoding must be defined in library
extern "C"
{
	#include "rscode-1.3/ecc.h"
}

// Time library for time management
#include <time.h>
#include <sys/time.h>

// Library for message authentication code
#include "mbedtls/md.h"
#include <stdio.h>
#include <string.h>

// ---------------------------------
// LORA CONFIGURATION
// ---------------------------------

// SX1278 LoRa module pins
#define CS_PIN 18
#define DIO0_PIN 26
#define RESET_PIN 23
#define DIO1_PIN 33

// SX1278 LoRa module configuration
#define F 436.0 // frequency [MHz]
#define BW 125.0 // bandwidth [kHz]
#define SF 10 // spreading factor
#define CR 5 // coding rate
#define SYNC_WORD 0x12 // standard for private communications
#define OUTPUT_POWER 10 // 10 dBm is set for testing phase (ideal value to use: 22 dBm)
#define PREAMBLE_LENGTH 8 // standard
#define GAIN 1 // set automatic gain control

// Define LoRa module
extern SX1278 radio;

// ---------------------------------
// RTOS CONFIGURATION
// ---------------------------------

// Define RTOS handles
extern TaskHandle_t RTOS_handle_COMMS_StateMachine;
extern QueueHandle_t RTOS_queue_TX; // queue for TX packets
extern QueueHandle_t RTOS_queue_cmd; // queue for command packets


// ---------------------------------
// COMMS STATE MACHINE FUNCTION
// ---------------------------------

// States configuration
#define COMMS_IDLE 0 // default idle state
#define COMMS_TX 1 // tx state to encode and send packets
#define COMMS_TX_ERROR 2 // tx error state
#define COMMS_RX 3 // rx state to decode and process received packets
#define COMMS_RX_ERROR 4 // rx error state
#define COMMS_CMD 5 // command state to process received packets
#define COMMS_ERROR 6 // error state
#define COMMS_SERIAL 7 // serial state to input packets manually (GS mode)

// States timing
#define IDLE_TIMEOUT 500 // [ms] idle timeout to wait before checking if a packet needs to be sent
#define RX_TIMEOUT 1000 // [ms] rx timeout to wait before going back to idle state

// Main COMMS loop
void COMMS_stateMachine(void *parameter);

// ---------------------------------
// UTILITY FUNCTIONS
// ---------------------------------

// Print boot message on serial
void printStartupMessage(const char*);

// Print radio status on serial
void printRadioStatus(int8_t state, bool blocking = false);

// Print received packet on serial with optional prefix
void printPacket(const char* prefix, const uint8_t* packet, uint8_t length);


// ---------------------------------
// RADIO FUNCTIONS
// ---------------------------------

// Packet configuration
#define TX_QUEUE_SIZE 6 // [packets] size of tx queue
#define PACKET_SIZE_MAX 128 // [bytes] max size of packets
#define PACKET_HEADER_LENGTH 12 // [bytes] length of the header in the packet
#define PACKET_PAYLOAD_MAX 98 // [bytes] max size of payload in the packet

// Command configuration
#define CMD_QUEUE_SIZE 2 // [packets] size of cmd queue

// MAC configuration
const uint8_t SECRET_KEY[] = { 0xA1, 0xB2, 0xC3, 0xD4 }; // secret key for MAC generation

// RS encoding configuration
extern bool rs_enabled;
#define RS_BLOCK_SIZE 16 // [bytes] size of RS block
constexpr uint8_t DATA_BLOCK_SIZE = RS_BLOCK_SIZE - NPAR; // [bytes] size of data block (data + parity)
#define BYTE_RS_OFF 0x55
#define BYTE_RS_ON 0xAA
#define BYTE_RS_PADDING 0x00

// TX state configuration
extern uint8_t tx_state;
#define BYTE_TX_OFF 0x00 // TX off
#define BYTE_TX_ON 0x01 // TX on
#define BYTE_TX_NOBEACON 0x02 // TX no beacon

// Error codes
#define PACKET_ERR_NONE 0 // no error in packet
#define PACKET_ERR_RS -1 // error in RS encoding bytes
#define PACKET_ERR_DECODE -2 // error in packet state
#define PACKET_ERR_LENGTH -3 // error in packet length
#define PACKET_ERR_MAC -4 // error in MAC
#define PACKET_ERR_CMD_FULL -5 // error in command queue full
#define PACKET_ERR_CMD_GENERIC -6 // error in command generic
#define PACKET_ERR_CMD_UNKNOWN -7 // error in command unknown
#define PACKET_ERR_CMD_PAYLOAD -8 // error in command payload

// TEC codes
#define TEC_OBC_REBOOT 0x01 // reboot OBC command
#define TEC_EXIT_STATE 0x02 // exit state command
#define TEC_VAR_CHANGE 0x03 // variable change command
#define TEC_SET_TIME 0x04 // set time command
#define TEC_EPS_REBOOT 0x08 // reboot EPS command
#define TEC_ADCS_REBOOT 0x10 // reboot ADCS command
#define TEC_ADCS_TLE 0x11 // send TLE to ADCS command
#define TEC_LORA_STATE 0x18 // send LoRa state command
#define TEC_LORA_CONFIG 0x19 // send LoRa link status command
#define TEC_LORA_PING 0x1A // send LoRa link status command

// TER codes
#define TER_BEACON 0x30 // telemetry beacon reply
#define TER_ACK 0x31 // ACK reply
#define TER_NACK 0x32 // NACK reply
#define TER_LORA_LINK 0x33 // LoRa link state reply

// TER lengths
// #define TER_BEACON_LENGTH 8 // [bytes] length of the telemetry beacon reply

// TER header
#define MISSION_ID 0x01 // mission ID

// ---------------------------------
// PRINT FUNCTIONS
// ---------------------------------

// Print boot message on serial
void printStartupMessage(const char* device);

// Print radio status on serial
void printRadioStatus(int8_t state, bool blocking);

// Print received packet on serial with optional prefix
void printPacket(const char* prefix, const uint8_t* packet, uint8_t length);


// ---------------------------------
// HELPER FUNCTIONS
// ---------------------------------

// Struct to hold packet information
struct Packet
{
	uint8_t station;
	bool ecc; // flag to indicate if RS ECC is used
	uint8_t command;
	uint8_t payload_length;
	uint32_t time_unix;
	uint32_t MAC;
	uint8_t payload[PACKET_PAYLOAD_MAX];
	int8_t state;

	void init(bool rs_enabled, uint8_t cmd);
	int8_t setPayload(const uint8_t* data, uint8_t length);
	int8_t seal(); // seal packet by calculating MAC and setting time
};

// Initialize system time to a specific UNIX timestamp, or default to Jan 1, 2025 if 0
void setUNIX(uint32_t unixTime = 0);

// Get current UNIX time
uint32_t getUNIX();

// MAC function to generate a message authentication code
// uint32_t makeMAC(uint32_t timestamp, uint32_t secret_key);
int8_t makeMAC(const Packet* packet, uint32_t* out_mac);

// Write float as 4 byte big endian
void writeFloatToBytes(float value, uint8_t* buffer);

// Process commands in serial input
void handleSerialInput();


// ---------------------------------
// RADIO FUNCTIONS
// ---------------------------------

// Notify COMMS task of a radio event (called when packet sent or received)
void packetEvent(void);

// Start LoRa reception
void startReception(void);

// Start LoRa transmission
void startTransmission(uint8_t *tx_packet, uint8_t packet_size);

// Convert received raw data to packet struct
void dataToPacket(const uint8_t* data, uint8_t length, Packet* packet);

// Convert packet struct to raw data, return length
uint8_t packetToData(const Packet* packet, uint8_t* data);


// ---------------------------------
// ECC FUNCTIONS
// ---------------------------------

// Check if RS ECC is enabled in the packet
bool isDataECCEnabled(const uint8_t* data, uint8_t length);

// Encode data using RS ECC and interleave the output
void encodeECC(uint8_t* data, uint8_t& data_len);

// Recover true data, deinterleaving and decoding if RS ECC is enabled
int8_t decodeECC(uint8_t* data, uint8_t& data_len);


// ---------------------------------
// COMMAND FUNCTIONS
// ---------------------------------

// Execute TEC from valid packets
int8_t executeTEC(const Packet* cmd);

// Check if packet is a TEC to be executed
bool isPacketTEC(const Packet* packet);

// Check if ACK is needed for the command
bool isACKNeeded(const Packet* packet);

// Check if ACK is needed for the command before execution
bool isACKNeededBefore(const Packet* packet);

// Send ACK packet to report valid command received
void sendACK(bool ecc, uint8_t TEC);

// Send NACK packet to report invalid command received
void sendNACK(bool ecc, uint8_t TEC, int8_t error);


// ---------------------------------
// TIMER FUNCTIONS
// ---------------------------------

extern TimerHandle_t RTOS_tx_timer; // timer to reset TX state

void vTxStateReset(TimerHandle_t xTimer);

#endif
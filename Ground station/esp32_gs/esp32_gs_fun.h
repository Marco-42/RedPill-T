// USE TTGO LORA32-OLED IN ARDUINO IDE FOR CORRECT PIN DEFINITION

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
// GS STATE MACHINE FUNCTION
// ---------------------------------

// States configuration
#define GS_IDLE 0 // default idle state
#define GS_TX 1 // tx state to encode and send packets
#define GS_RX 2 // rx state to decode and process received packets
#define GS_SERIAL 3 // serial state to input packets manually (GS mode)

// States timing
#define IDLE_TIMEOUT 500 // [ms] idle timeout to wait before checking if a packet needs to be sent
#define RX_TIMEOUT 1000 // [ms] rx timeout to wait before going back to idle state

// Packet configuration
#define TX_QUEUE_SIZE 6 // [packets] size of tx queue
#define PACKET_SIZE_MAX 128 // [bytes] max size of packets
#define PACKET_HEADER_LENGTH 12 // [bytes] length of the header in the packet
#define PACKET_PAYLOAD_MAX 98 // [bytes] max size of payload in the packet

// Main GS loop
void GS_stateMachine(void *parameter);


// ---------------------------------
// STRUCTS
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


// ---------------------------------
// UTILITY FUNCTIONS
// ---------------------------------

// Print boot message on serial
void printStartupMessage(const char* device);

// Print radio status on serial
void printRadioStatus(int8_t state, bool blocking = false);

// Print received packet on serial with optional prefix
void printData(const char* prefix, const uint8_t* packet, uint8_t length);

// Print packet on serial with optional prefix
void printPacket(const char* prefix, const Packet* packet);


// ---------------------------------
// HELPER FUNCTIONS
// ---------------------------------

// Process commands in serial input
void handleSerialInput();


// ---------------------------------
// RADIO FUNCTIONS
// ---------------------------------

// RS encoding configuration
extern bool rs_enabled;
#define RS_BLOCK_SIZE 16 // [bytes] size of RS block
constexpr uint8_t DATA_BLOCK_SIZE = RS_BLOCK_SIZE - NPAR; // [bytes] size of data block (data + parity)
#define RS_OFF 0x55
#define RS_ON 0xAA
#define RS_PADDING 0x00

// Error codes
#define PACKET_ERR_NONE 0 // no error in packet
#define PACKET_ERR_RS -1 // error in RS encoding bytes
#define PACKET_ERR_DECODE -2 // error in packet state
#define PACKET_ERR_LENGTH -3 // error in packet length
#define PACKET_ERR_MAC -4 // error in MAC
#define PACKET_ERR_CMD_FULL -5 // error in command queue full
#define PACKET_ERR_CMD_POINTER -6 // error in command pointer
#define PACKET_ERR_CMD_UNKNOWN -7 // requested command not found
#define PACKET_ERR_CMD_PAYLOAD -8 // payload of command is invalid
#define PACKET_ERR_CMD_MEMORY -9 // memory allocation error during command execution

// Notify GS task of a radio event (called when packet sent or received)
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
// PACKET FUNCTIONS
// ---------------------------------



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



// ---------------------------------
// TIMERS AND RTOS CONFIGURATION
// ---------------------------------

// RTOS handles
extern TaskHandle_t RTOS_handle_GS_StateMachine;

extern QueueHandle_t RTOS_queue_TX; // queue for TX packets

#endif
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

// ---------------------------------
// LORA CONFIGURATION
// ---------------------------------

// SX1278 LoRa module pins
#define CS_PIN 18
#define DIO0_PIN 26
#define RESET_PIN 23
#define DIO1_PIN 33

// SX1278 LoRa module configuration
#define F 433.0 // frequency [MHz]
#define BW 125.0 // bandwidth [kHz]
#define SF 10 // spreading factor
#define CR 8 // coding rate
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


// TX queue configuration
#define TX_QUEUE_SIZE 5 // [packets] size of tx queue


// ---------------------------------
// COMMS STATE MACHINE FUNCTION
// ---------------------------------

// States configuration
#define COMMS_IDLE 0 // default idle state
#define COMMS_TX 1 // tx state
#define COMMS_TX_ERROR 2 // tx error state
#define COMMS_RX 3 // rx state
#define COMMS_RX_ERROR 4 // rx error state
#define COMMS_ERROR 5 // error state
#define COMMS_SERIAL 6 // serial state to input packets manually (GS mode)

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
#define PACKET_SIZE_MAX 128 // [bytes] max size of packets
#define PACKET_HEADER_LENGTH 13 // [bytes] length of the header in the packet
#define PACKET_PAYLOAD_MAX 98 // [bytes] max size of payload in the packet

// Command configuration
#define CMD_PACKETS_MAX 2 // [packets] max number of packets in a command
constexpr uint8_t CMD_SIZE_MAX = (PACKET_SIZE_MAX * CMD_PACKETS_MAX); // [bytes] max size of a command

// MAC configuration
#define SECRET_KEY 0xA1B2C3D4 // secret key for MAC generation

// RS encoding configuration
#define RS_BLOCK_SIZE 16 // [bytes] size of RS block
constexpr uint8_t DATA_BLOCK_SIZE = RS_BLOCK_SIZE - NPAR; // [bytes] size of data block (data + parity)
#define BYTE_RS_OFF 0x55
#define BYTE_RS_ON 0xAA
#define BYTE_RS_PADDING 0x00 

// Error codes
#define PACKET_ERR_NONE 0 // no error in packet
#define PACKET_ERR_RS -1 // error in RS encoding bytes
#define PACKET_ERR_LENGTH -2 // error in packet length
#define PACKET_ERR_ID -3 // error in packet ID (out of range)
#define PACKET_ERR_MAC -4 // error in MAC

#define CMD_ERR_NONE 0 // no error in command
#define CMD_ERR_PACKET -1 // error in packet state
#define CMD_ERR_HEADER -2 // error in packet header
#define CMD_ERR_ID -3 // error in packet ID (double or out of range)
#define CMD_ERR_MISSING -4 // missing packet in command

// TEC codes
#define TEC_OBC_REBOOT 0x01 // reboot OBC command
#define TEC_EXIT_STATE 0x02 // exit state command
#define TEC_VAR_CHANGE 0x03 // variable change command
#define TEC_SET_TIME 0x04 // set time command
#define TEC_EPS_REBOOT 0x08 // reboot EPS command
#define TEC_ADCS_REBOOT 0x10 // reboot ADCS command
#define TEC_ADCS_TLE 0x11 // send TLE to ADCS command

// TRC codes
#define TRC_BEACON 0x30 // telemetry beacon reply
#define TRC_ACK 0x31 // ACK reply
#define TRC_NACK 0x32 // NACK reply

// TRC lengths
#define TRC_BEACON_LENGTH 8 // [bytes] length of the telemetry beacon reply
#define TRC_ACK_LENGTH 8 // [bytes] length of the ACK reply
#define TRC_NACK_LENGTH 8 // [bytes] length of the NACK reply

// TRC header
#define MISSION_ID 0x01 // mission ID

// ---------------------------------
// PRINT FUNCTIONS
// ---------------------------------

// Print boot message on serial
void printStartupMessage(const char* device);

// Print radio status on serial
void printRadioStatus(int8_t state, bool blocking);


// ---------------------------------
// HELPER FUNCTIONS
// ---------------------------------

// Initialize system time to a specific UNIX timestamp, or default to Jan 1, 2025 if 0
void setUNIX(uint32_t unixTime = 0);

// Get current UNIX time
uint32_t getUNIX();

// MAC function to generate a message authentication code
uint32_t makeMAC(uint32_t timestamp, uint32_t secret_key);

// ---------------------------------
// RADIO FUNCTIONS
// ---------------------------------

// Notify COMMS task of a radio event (called when packet sent or received)
void packetEvent(void);

// Start LoRa reception
void startReception(void);

// Start LoRa transmission
void startTransmission(uint8_t *tx_packet, uint8_t packet_size);

// Struct to hold packet information
struct Packet
{
	int8_t state;
	bool ecc; // flag to indicate if RS ECC is used
	uint8_t station;
	uint8_t command;
	uint8_t ID_total;
	uint8_t ID;
	uint32_t MAC;
	uint32_t time_unix;
	uint8_t payload_length;
	uint8_t payload[PACKET_PAYLOAD_MAX];

	void init();
	void setPayload(const uint8_t* data, uint8_t length);

};

// Convert received raw data to packet struct
void dataToPacket(const uint8_t* data, uint8_t length, Packet* packet);

// Convert packet struct to raw data, return length
uint8_t packetToData(const Packet* packet, uint8_t* data);

// Process commands in serial input
void handleSerialInput();

// Encode data using RS ECC and interleave the output
void encodeECC(uint8_t* data, uint8_t& data_len);

// Recover true data, deinterleaving and decoding if RS ECC is enabled
bool decodeECC(uint8_t* data, uint8_t& data_len);

// Validate packet
int8_t validatePacket(const Packet* packet);

// ---------------------------------
// COMMAND FUNCTIONS
// ---------------------------------

// Check if packets form a valid command
int8_t checkPackets(const Packet* packets, uint8_t packets_total);

// Assemble and execute command from valid packets
bool executeCommand(const Packet* packets, uint8_t packets_total);

// Send ACK packet to report valid command received
void sendACK(uint8_t TEC);

// Send NACK packet to report invalid command received
void sendNACK(uint8_t TEC);

#endif
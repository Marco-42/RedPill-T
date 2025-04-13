#ifndef COMMSFUN_H
#define COMMSFUN_H


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


// ---------------------------------
// RTOS CONFIGURATION
// ---------------------------------

// TX queue configuration
#define TX_QUEUE_SIZE 5 // [packets] size of tx queue
#define TX_QUEUE_PACKET_SIZE 64 // [bytes] max size of packets to be sent


// ---------------------------------
// COMMS STATE MACHINE CONFIGURATION
// ---------------------------------

// States configuration
#define COMMS_IDLE 0 // default idle state
#define COMMS_TX 1 // tx state
#define COMMS_TX_ERROR 2 // tx error state
#define COMMS_RX 3 // rx state
#define COMMS_RX_ERROR 4 // rx error state
#define COMMS_ERROR 5 // error state

// States timing
#define IDLE_TIMEOUT 500 // [ms] idle timeout to wait before checking if a packet needs to be sent
#define RX_TIMEOUT 3000 // [ms] rx timeout to wait before going back to idle state


// ---------------------------------
// PACKET CONFIGURATION
// ---------------------------------

// Packet size configuration
#define RX_PACKET_SIZE_MAX 64 // maximum size of the RX packet
#define RX_PACKET_NUMBER_MAX 4 // maximum number of packets in single command
#define RX_PACKET_HEADER_LENGTH 13

// Error codes
#define RX_PACKET_ERR_NONE 0
#define RK_PACKET_ERR_END 1


// ---------------------------------
// UTILITY FUNCTIONS
// ---------------------------------

// Print boot message on serial
void printBootMessage();

// Print radio status on serial
void printRadioStatus(int8_t state, bool blocking = false);

// Print received packet on serial
void printPacket(const uint8_t* packet, uint8_t length)

#endif
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

// States timing
#define IDLE_TIMEOUT 500 // [ms] idle timeout to wait before checking if a packet needs to be sent
#define RX_TIMEOUT 3000 // [ms] rx timeout to wait before going back to idle state

// Main COMMS loop
void COMMS_stateMachine(void *parameter);

// ---------------------------------
// UTILITY FUNCTIONS
// ---------------------------------

// Print boot message on serial
void printStartupMessage(const char*);

// Print radio status on serial
void printRadioStatus(int8_t state, bool blocking = false);

// Print received packet on serial
void printPacket(const uint8_t* packet, uint8_t length);


// ---------------------------------
// RADIO FUNCTIONS
// ---------------------------------

// Packet size configuration
#define RX_PACKET_SIZE_MAX 64 // [bytes] max size of RX packets
#define RX_PACKET_NUMBER_MAX 4 // [bytes] max number of packets in single command
#define RX_PACKET_HEADER_LENGTH 13 // [bytes] length of the header in the RX packet
#define TX_PACKET_SIZE_MAX 128 // [bytes] max size of TX packets

// Error codes
#define RX_PACKET_ERR_NONE 0
#define RX_PACKET_ERR_END 1

// TEC codes
#define TEC_OBC_REBOOT 0x00 // reboot OBC command
#define TEC_TLM_BEACON 0x02 // send telemetry beacon

// TEC lengths

// TRC codes
#define TRC_BEACON 0x00 // telemetry beacon reply
#define TRC_ACK 0x01 // ACK reply
#define TRC_NACK 0x02 // NACK reply

// TRC lengths
#define TRC_BEACON_LENGTH 8 // [bytes] length of the telemetry beacon reply
#define TRC_ACK_LENGTH 8 // [bytes] length of the ACK reply
#define TRC_NACK_LENGTH 8 // [bytes] length of the NACK reply

// TRC header
#define MISSION_ID 0x01 // mission ID
#define POCKETQUBE_ID 0x01 // pocketqube ID


// Notify COMMS task of a radio event (called when packet sent or received)
void packetEvent(void);

// Start LoRa reception
void startReception(void);

// Start LoRa transmission
void startTransmision(uint8_t *tx_packet, uint8_t packet_size);

// Struct to hold packet information
struct PacketRX
{
	uint8_t state;
	uint8_t station;
	uint8_t TEC;
	uint8_t ID_total;
	uint8_t ID;
	uint32_t time_unix;
	uint8_t payload_length;
	uint8_t payload[RX_PACKET_SIZE_MAX];

};

// Struct to hold TX packet information
struct PacketTX
{
	uint8_t TRC;
	uint8_t ID_total;
	uint8_t ID;
	uint32_t time_unix;
	uint8_t payload_length;
	uint8_t payload[TX_PACKET_SIZE_MAX];
};

// Convert received packet to struct
PacketRX dataToPacketRX(uint8_t* data, uint8_t length);


// ---------------------------------
// COMMAND FUNCTIONS
// ---------------------------------

// Make TEC and payload from packets
bool processCommand(PacketRX* packets, uint8_t packets_total);

#endif
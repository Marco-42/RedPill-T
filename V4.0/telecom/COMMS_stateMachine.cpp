#include <Arduino.h>
#include "comms_fun.h"


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
// COMMS STATE MACHINE
// ---------------------------------

// States configuration
#define COMMS_IDLE 0 // default idle state
#define COMMS_TX 1 // tx state
#define COMMS_TX_ERROR 2 // tx error state
#define COMMS_RX 3 // rx state
#define COMMS_RX_ERROR 4 // rx error state
#define COMMS_ERROR 5 // error state

// Syayes timing
#define IDLE_TIMEOUT 500 // [ms] tx timeout
#define RX_TIMEOUT 3000 // [ms] tx timeout

// Main COMMS loop
void COMMS_stateMachine(void)
{
	// Start serial communication
	Serial.begin(9600);
	printBootMessage();
	
	// Create queue for TX packets
	RTOS_queue_TX = xQueueCreate(TX_QUEUE_SIZE, TX_QUEUE_PACKET_SIZE);

	// Initialize variables
	COMMS_state = COMMS_IDLE; // initial state
	
	// Start LoRa module
	SX1278 radio = new Module(CS_PIN, DIO0_PIN, RESET_PIN, DIO1_PIN);

	Serial.print("[SX1278] Initializing ... ");
	int8_t state = radio.begin(F, BW, SF, CR, SYNC_WORD, OUTPUT_POWER, PREAMBLE_LENGTH, GAIN);
	printRadioStatus(state, true); // block program if LoRa cannot be initialized

	// Set ISR to be called when packets are sent or received
	radio.setPacketSentAction(packetEvent);
	radio.setPacketReceivedAction(packetEvent);

	// Start listening
	startReception();

	for(;;) // only way to end state machine is killing COMMS thread (by the OBC)
	{
		
		// State machine
		switch (COMMS_state)
		{
			// Nothing to process
			case COMMS_IDLE:
			{
				// Check if a packet has been received
				if (!ulTaskNotifyTake(pdTRUE, IDLE_TIMEOUT) == 0) // if a packet is received
				{
					// Switch to RX state
					COMMS_state = COMMS_RX;
				}

				// Check if there are packets waiting in queue to be transmitted
				else if (uxQueueMessagesWaiting(RTOS_queue_TX) > 0)
				{
					// Switch to TX state
					COMMS_state = COMMS_TX;
				}

				// No packet received before timeout, no packet to send
				else
				{
					// Do nothing, repeat loop	
				}
		
				break;
			}

			// Incoming packet to be processed
			case COMMS_RX:
			{
				do
				{
					// Read packet
					uint8_t rx_packet_size = radio.getPacketLength();
					uint8_t rx_packet[rx_packet_size];
					int8_t rx_state = radio.readData(rx_packet, rx_packet_size);

					// Successful reception
					if (rx_state == RADIOLIB_ERR_NONE)
					{
						// Print received packet
						printPacket(rx_packet, rx_packet_size);

						//TODO: decode Reed Salomon packet
						//TODO: deinterleave packet

						Packet incoming = packetTransform(rx_packet, rx_packet_size)
					}

				} while (!ulTaskNotifyTake(pdTRUE, RX_TIMEOUT) == 0); // repeat if another packet is received before timeout

				break;
			}
			
			default:
				// TODO: bitflip protection
				COMMS_state = COMMS_ERROR;
				break;
		}
	}
	
	vTaskDelete(NULL);
}
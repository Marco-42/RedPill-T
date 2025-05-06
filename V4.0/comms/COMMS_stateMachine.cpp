#include <Arduino.h>
#include "comms_fun.h"


// Main COMMS loop
void COMMS_stateMachine(void)
{
	// Start serial communication
	Serial.begin(9600);
	printStartupMessage('COMMS');
	
	// Create queue for TX packets
	RTOS_queue_TX = xQueueCreate(TX_QUEUE_SIZE, sizeof(PacketTX));

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
			case COMMS_IDLE: //--> non fa nulla
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
				// Initialize command variables
				uint8_t command_packets_processed = 0
				PacketRX command_packets[RX_PACKET_NUMBER_MAX];

				do
				{
					// Read packet
					uint8_t rx_packet_size = radio.getPacketLength();
					uint8_t rx_packet[rx_packet_size];
					int8_t rx_state = radio.readData(rx_packet, rx_packet_size);

					// Reception successfull
					if (rx_state == RADIOLIB_ERR_NONE)
					{
						// Print received packet
						printPacket(rx_packet, rx_packet_size);

						//TODO: decode Reed Salomon packet
						//TODO: deinterleave packet

						// Parse packet
						PacketRX incoming = dataToPacketRX(rx_packet, rx_packet_size)

						// Store packet in command array
						command_packets[command_packets_processed] = incoming; // store packet in command array
						command_packets_processed += 1; // increment number of packets processed

						// Next code has been deprecated as check is done in processCommand
						// // Successfully decoded packet
						// if (incoming.state == PACKET_ERR_NONE)
						// {
							
						// }

						// // Packet can not be decoded
						// else
						// {
						// 	//	TODO: handle error
						// }
					}

					// Reception error
					{
						// TODO: handle error
					}


				} while (!ulTaskNotifyTake(pdTRUE, RX_TIMEOUT) == 0); // repeat if another packet is received before timeout (command is multipacket)

				// Check if all packets have been received
				bool command_valid = processCommand(command_packets, command_packets_processed);

				//
				if (!command_valid)
				{
					// TODO: send NACK
				}

				break;
			}
			
			case COMMS_TX:
			{
				do
				{
					// Get packet from queue
					uint8_t tx_packet[TX_QUEUE_PACKET_SIZE];
					xQueueReceive(RTOS_queue_TX, &tx_packet, portMAX_DELAY);

					// Transform packet to data based on TRC
					switch (tx_packet.TRC)
					{
					case TRC_BEACON:
					{
						// Send telemetry beacon without coding or interleaving
						Serial.println("Sending telemetry beacon...");
						break;
					}
					
					default:
					{
						// TODO: interleave
						// TODO: encode Reed Salomon packet
						Serial.println("Sending data packet...");
						break;
					}

					// Start transmission
					startTransmision(tx_packet, TX_PACKET_SIZE);

					// Wait for transmission to end
					ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

					// Report transmission status
					printRadioStatus(tx_state);

				} while (uxQueueMessagesWaiting(RTOS_queue_TX) > 0); // repeat if there are multiple packets to be sent
				
			}

			default:
				// TODO: bitflip protection
				COMMS_state = COMMS_ERROR;
				break;
		}
	}
	
	vTaskDelete(NULL);
}

// Functions and libraries
#include "esp32_fun.h"

// Define LoRa module
SX1278 radio = new Module(CS_PIN, DIO0_PIN, RESET_PIN, DIO1_PIN);

// Define the queue handle
QueueHandle_t RTOS_queue_TX;

// General comunication state configuration 
uint8_t COMMS_state = 0;

// Main COMMS loop
void COMMS_stateMachine(void *parameter)
{	
	printStartupMessage("COMMS");

	// Create queue for TX packets
	RTOS_queue_TX = xQueueCreate(TX_QUEUE_SIZE, sizeof(Packet));

	// Initial state
	COMMS_state = COMMS_IDLE;
	
	// Start LoRa module
	Serial.print("[SX1278] Initializing ... ");
	int8_t state = radio.begin(F, BW, SF, CR, SYNC_WORD, OUTPUT_POWER, PREAMBLE_LENGTH, GAIN);
	printRadioStatus(state, true); // block program if LoRa cannot be initialized

	// Set ISR to be called when packets are sent or received
	radio.setPacketSentAction(packetEvent);
	radio.setPacketReceivedAction(packetEvent);

	// Initialize Reed-Solomon error correction
	initialize_ecc();
	bool rs_enabled = true; // flag to check if Reed-Solomon is enabled
	bool rs_should_encode = true; // flag to check if Reed-Solomon encoding should be done

	for(;;) // only way to end state machine is killing COMMS thread (by the OBC)
	{
		
		// State machine
		switch (COMMS_state)
		{
			// NOTHING TO PROCESS
			// Check if there is packet waiting in the queue, in case it switch to COMMS_TX
			case COMMS_IDLE:
			{
				Serial.println("COMMS_IDLE: Waiting for packets ...");

				bool first_run = true; // flag to check if this is the first run of the loop

				do
				{
					// Check if there are packets waiting in queue to be transmitted
					if (uxQueueMessagesWaiting(RTOS_queue_TX) > 0)
					{
						// Switch to TX state
						COMMS_state = COMMS_TX;
					}

					// Check if there is serial data to be transmitted
					else if (Serial.available() > 0)
					{
						// Switch to serial state
						COMMS_state = COMMS_SERIAL;
					}
					
					// Handle reception
					else
					{
						// Enable RX if first run
						if (first_run)
						{
							startReception();
							first_run = false; // disable RX for next runs
						}

						// Check if a packet has been received
						// IDLE_TIMEOUT is the time to wait for a packet to be received before checking if there are packets to be sent
						else if (!ulTaskNotifyTake(pdTRUE, IDLE_TIMEOUT) == 0) // if a packet is received before timeout
						{
							// Switch to RX state
							COMMS_state = COMMS_RX;
						}

						// No packet received before timeout, no packet to send
						else
						{
							// Do nothing, repeat loop	
						}
					}
				} while (COMMS_state == COMMS_IDLE); // repeat until state changes
		
				break;
			}

			// PACKET RECEPTION
			case COMMS_RX:
			{
				Serial.println("COMMS_RX: starting packet reception ...");
				
				// Initialize command variables
				uint8_t command_packets_processed = 0;
				Packet command_packets[PACKET_CMD_MAX]; // array to store received packets in command

				do
				{
					// Read packet
					uint8_t rx_packet_size = radio.getPacketLength();
					uint8_t rx_packet[PACKET_SIZE_MAX];
					int8_t rx_state = radio.readData(rx_packet, rx_packet_size);

					// Reception successfull
					if (rx_state == RADIOLIB_ERR_NONE)
					{
						// Print received packet
						printPacket(rx_packet, rx_packet_size);

						//TODO: decode Reed Salomon packet
						//TODO: deinterleave packet
						decode_data(rx_packet, rx_packet_size); // decode Reed-Solomon

						// Parse packet
						Packet incoming = dataToPacket(rx_packet, rx_packet_size);

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
				uint8_t command_valid = processCommand(command_packets, command_packets_processed);

				//
				if (command_valid != CMD_ERR_NONE)
				{
					// TODO: send NACK
				}


				COMMS_state = COMMS_IDLE; // go back to idle state after processing command
				break;
			}
			
			// PACKET TRANSMITION
			case COMMS_TX:
			{
				Serial.println("COMMS_TX: starting packet transmission ...");

				do
				{
					// Get packet data from queue
					Packet tx_packet_struct;
					xQueueReceive(RTOS_queue_TX, &tx_packet_struct, portMAX_DELAY);
					
					// Prepare packet
					uint8_t tx_packet[PACKET_SIZE_MAX];
					uint8_t tx_packet_size = packetToData(&tx_packet_struct, tx_packet);

					// Transform packet to data based on command
					switch (tx_packet_struct.command)
					{
						case TRC_BEACON:
						{
							rs_should_encode = false;
							break;
						}
						
						default:
						{
							
							// TODO: interleave
							// TODO: encode Reed Salomon packet
							// Serial.println("Sending data packet...");
							break;
						}
					}

					// If Reed-Solomon encoding is enabled, encode the packet
					if (rs_enabled && rs_should_encode)
					{
						// Encode Reed-Solomon codeword to tx_packet
						uint8_t tx_packet_encoded[PACKET_SIZE_MAX];
						encode_data(tx_packet, tx_packet_size, tx_packet_encoded);
						Serial.printf("Before RS encoding:");
						for (uint8_t i = 0; i < tx_packet_size; ++i) {
							Serial.printf("%02X ", tx_packet[i]);
						}
						Serial.println();

						// Copy encoded data to tx_packet
						tx_packet_size += NPAR; // increase size by number of parity bytes
						memcpy(tx_packet, tx_packet_encoded, tx_packet_size);
						Serial.printf("After RS encoding:");
						for (uint8_t i = 0; i < tx_packet_size; ++i) {
							Serial.printf("%02X ", tx_packet[i]);
						}
						Serial.println();
					}
					
					// Start transmission
					startTransmission(tx_packet, tx_packet_size);

					// Wait for transmission to end
					ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

					// Report transmission status
					printRadioStatus(state);

				} while (uxQueueMessagesWaiting(RTOS_queue_TX) > 0); // repeat if there are multiple packets to be sent
				
				COMMS_state = COMMS_IDLE; // go back to idle state after processing all packets
				break;
			}

			// SERIAL INPUT
			case COMMS_SERIAL:
			{
				Serial.println("COMMS_SERIAL: enter packets -> 'go' to send, 'end' to discard");


				handleSerialInput();

				COMMS_state = COMMS_IDLE;
				break;
			}


			default:
			{
				Serial.println("Unknown COMMS state! Resetting to IDLE.");
				COMMS_state = COMMS_IDLE; // reset to idle state if unknown state
				break;
			}
		}
	}
	
	vTaskDelete(NULL);
}

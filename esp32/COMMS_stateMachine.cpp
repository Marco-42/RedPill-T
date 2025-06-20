// Functions and libraries
#include "esp32_fun.h"

// Define LoRa module
SX1278 radio = new Module(CS_PIN, DIO0_PIN, RESET_PIN, DIO1_PIN);

// General comunication state configuration 
uint8_t COMMS_state = 0;

// Main COMMS loop
void COMMS_stateMachine(void *parameter)
{	
	printStartupMessage("COMMS");
	
	// Defining the Queue Handle
	QueueHandle_t RTOS_queue_TX;

	// Create queue for TX packets
	RTOS_queue_TX = xQueueCreate(TX_QUEUE_SIZE, sizeof(PacketTX));

	// Initialize variables 
	// initial state --> for default setting COMMS_IDLE = 0
	COMMS_state = COMMS_IDLE;
	
	// Start LoRa module
	Serial.print("[SX1278] Initializing ... ");
	int8_t state = radio.begin(F, BW, SF, CR, SYNC_WORD, OUTPUT_POWER, PREAMBLE_LENGTH, GAIN);
	printRadioStatus(state, true); // block program if LoRa cannot be initialized

	// Set ISR to be called when packets are sent or received
	radio.setPacketSentAction(packetEvent);
	radio.setPacketReceivedAction(packetEvent);

	// Start listening
	startReception();


	// only way to end state machine is killing COMMS thread (by the OBC)
	for(;;)
	{
		
		// State machine
		switch (COMMS_state)
		{
			// NOTHING TO PROCESS
			// Check if there is packet waiting in the queue, in case it switch to COMMS_TX
			case COMMS_IDLE: //COMMS_IDLE = 0
			{
				Serial.println("COMMS_IDLE: Waiting for packets...");

				// Start listening for packets
				startReception();
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
				Serial.println("COMMS_RX: starting packet reception...");
				
				// Initialize command variables
				uint8_t command_packets_processed = 0;
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
						PacketRX incoming = dataToPacketRX(rx_packet, rx_packet_size);

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
				Serial.println("COMMS_TX: starting packet transmission...");

				do
				{
					// Get packet data from queue
					PacketTX tx_packet_struct;
					xQueueReceive(RTOS_queue_TX, &tx_packet_struct, portMAX_DELAY);
					
					// Prepare packet
					uint8_t tx_buffer[TX_PACKET_SIZE_MAX];
					uint8_t tx_packet_size = packetTXtoData(&tx_packet_struct, tx_buffer);
					uint8_t tx_packet[tx_packet_size];
					memcpy(tx_packet, tx_buffer, tx_packet_size);

					// Transform packet to data based on TRC
					switch (tx_packet_struct.TRC)
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

			case COMMS_SERIAL:
			{

				// Serial input listener
				Serial.println("COMMS_SERIAL: enter payloads -> 'go' to send, 'end' to discard");

				std::vector<String> lines_raw; // ChatGPT magic
				String line_incoming;

				while (true)
				{
					// Throttle execution
					vTaskDelay(100);

					// Read incoming characters
					if (Serial.available())
					{
						char c = Serial.read();

						// Build the input line
						if (c == '\n' || c == '\r') // line is terminated
						{
							line_incoming.trim();  // Remove any stray whitespace

							// Skip empty lines
							if (line_incoming.length() == 0)
							{
								continue;  // this skips the rest of the loop
							}

							if (line_incoming.equalsIgnoreCase("go"))
							{
								Serial.println("Processing packets...");
								uint8_t lines_total = lines_raw.size();

								for (uint8_t i = 0; i < lines_total; ++i)
								{
									const String& line = lines_raw[i];

									// Create a PacketTX from the current line
									PacketTX packet = serialToPacketTX(line, i + 1, lines_total);

									// Send packet to the TX queue
									xQueueSend(RTOS_queue_TX, &packet, portMAX_DELAY);
								}
								break;  // Exit the listener
							}

							else if (line_incoming.equalsIgnoreCase("end"))
							{
								Serial.println("Cancelled! Packets discarded, resuming operation");
								break;  // Exit and discard
							}

							else
							{
								lines_raw.push_back(line_incoming);
								Serial.println("Packet: " + line_incoming);
							}

							// Reset for next line
							line_incoming = "";
						}

						// Add character to the current line
						else
						{
							line_incoming += c;
						}
					}
				}

				COMMS_state = COMMS_IDLE;
				break;
			}

			default:
				// TODO: bitflip protection etc
				COMMS_state = COMMS_ERROR;
				break;
		}
	}
	
	vTaskDelete(NULL);
}

// Functions and libraries
#include "esp32_fun.h"

// Define LoRa module
SX1278 radio = new Module(CS_PIN, DIO0_PIN, RESET_PIN, DIO1_PIN);

// Define the queue handles
QueueHandle_t RTOS_queue_TX;
QueueHandle_t RTOS_queue_cmd;

// General comunication state configuration 
uint8_t COMMS_state = COMMS_IDLE;

// Transmission state
uint8_t tx_state = BYTE_TX_ON;
TimerHandle_t RTOS_tx_timer;

// Flag to check if Reed-Solomon ECC is enabled
bool rs_enabled = false;

// Main COMMS loop
void COMMS_stateMachine(void *parameter)
{	
	printStartupMessage("COMMS");

	// Create queues
	RTOS_queue_TX = xQueueCreate(TX_QUEUE_SIZE, sizeof(Packet));
	RTOS_queue_cmd = xQueueCreate(CMD_QUEUE_SIZE, sizeof(Packet));

	// Initial state
	COMMS_state = COMMS_IDLE;

	// Initialize Reed-Solomon error correction
	initialize_ecc();

	Serial.println("ok");
	
	// Start LoRa module
	Serial.print("[SX1278] Initializing ... ");
	int8_t state = radio.begin(F, BW, SF, CR, SYNC_WORD, OUTPUT_POWER, PREAMBLE_LENGTH, GAIN);
	printRadioStatus(state, true); // block program if LoRa cannot be initialized

	// Set ISR to be called when packets are sent or received
	radio.setPacketSentAction(packetEvent);
	radio.setPacketReceivedAction(packetEvent);


	for(;;) // only way to end state machine is killing COMMS thread (by the OBC)
	{
		
		// State machine
		switch (COMMS_state)
		{
			// NOTHING TO PROCESS
			// Check if there is packet waiting in the queue, in case it switch to COMMS_TX
			case COMMS_IDLE:
			{
				Serial.println("COMMS_IDLE: Waiting for events ... ");

				bool first_run = true; // flag to check if this is the first run of the loop

				// Keep checking for events until state changes
				while (COMMS_state == COMMS_IDLE)
				{
					// Check if there are packets waiting in queue to be transmitted
					if (uxQueueMessagesWaiting(RTOS_queue_TX) > 0)
					{
						// Switch to TX state
						COMMS_state = COMMS_TX;
						break;
					}

					// Check if there are commands waiting in queue to be processed
					if (uxQueueMessagesWaiting(RTOS_queue_cmd) > 0)
					{
						// Switch to CMD state
						COMMS_state = COMMS_CMD;
						break;
					}
					// Check if there is serial data to be transmitted
					if (Serial.available() > 0)
					{
						// Switch to serial state
						COMMS_state = COMMS_SERIAL;
						break;
					}

					
					// Enable RX if first run
					if (first_run)
					{
						startReception();
						first_run = false; // do not activate RX for next runs
					}

					// Check if a packet has been received
					// IDLE_TIMEOUT is the time to wait for a packet to be received before checking if there are other things to process
					if (ulTaskNotifyTake(pdFALSE, IDLE_TIMEOUT) != 0) // if a packet is received before timeout
					{
						// Switch to RX state
						COMMS_state = COMMS_RX;
						break;
					}

					// Nothing to do, repeat checks
				}
		
				break;
			}

			// PACKET RECEPTION
			case COMMS_RX:
			{
				Serial.println("COMMS_RX: Starting packet reception ... ");

				// Read packet
				uint8_t rx_data[PACKET_SIZE_MAX];
				uint8_t rx_data_size = radio.getPacketLength();
				int8_t rx_state = radio.readData(rx_data, rx_data_size);

				bool ecc = isDataECCEnabled(rx_data, rx_data_size);
				bool is_TEC = false; // flag to check if packet is a TEC
				
				Packet rx_packet; // create packet struct to store received data
				rx_packet.init(ecc, 0); // initialize packet with default values

				// Reception successfull
				if (rx_state == RADIOLIB_ERR_NONE)
				{
					// Decode ECC data if enabled
					if (ecc)
					{
						printPacket("Before decoding: ", rx_data, rx_data_size);
						rx_packet.state = decodeECC(rx_data, rx_data_size);
					}
				}
				// Reception error
				else
				{
					// Always try to recover the packet, content can not be trusted otherwise
					ecc = true;
					printPacket("Before decoding: ", rx_data, rx_data_size);
					rx_packet.state = decodeECC(rx_data, rx_data_size);
				}
				printPacket("Decoded: ", rx_data, rx_data_size);

				// Store packet if no decode error
				if (rx_packet.state == PACKET_ERR_NONE)
				{
					// Validate and decode packet into struct
					dataToPacket(rx_data, rx_data_size, &rx_packet);

					// Check if packet is a TEC and send it to command queue
					is_TEC = isPacketTEC(&rx_packet); // check if packet is a tec
					if (is_TEC)
					{
						if (xQueueSend(RTOS_queue_cmd, &rx_packet, 0) != pdPASS)
						{
							rx_packet.state = PACKET_ERR_CMD_FULL; // command queue is full, cannot process packet
						}
					}
					else
					{
						Serial.printf("Received non-TEC packet with command %d\n", rx_packet.command);
					}
				}

				// Serial.printf("COMMS_RX: Command %d has state %d, it is %s\n", rx_packet.command, rx_packet.state, is_TEC ? "TEC" : "non-TEC");
				if (is_TEC)
				{
					rs_enabled = rx_packet.ecc; // update RS encoding flag based on received packet
					if (rx_packet.state == PACKET_ERR_NONE)
					{
						if (isACKNeededBefore(&rx_packet))
						{
							// Send early ACK packet to confirm valid command executed
							sendACK(rs_enabled, rx_packet.command);
						}
					}
					else
					{
						// Send NACK packet to confirm invalid command received
						sendNACK(rs_enabled, rx_packet.command, rx_packet.state);
					}
				}

				COMMS_state = COMMS_IDLE; // go back to idle state after processing command
				break;
			}
			
			// PACKET TRANSMITION
			case COMMS_TX:
			{
				Serial.println("COMMS_TX: Starting packet transmission ... ");

				// Keep processing packets until queue is empty
				while (uxQueueMessagesWaiting(RTOS_queue_TX) > 0)
				{
					// Get packet data from queue
					Packet tx_packet_struct;
					xQueueReceive(RTOS_queue_TX, &tx_packet_struct, portMAX_DELAY);
				
					if (tx_state == BYTE_TX_OFF || 
						(tx_state == BYTE_TX_NOBEACON && tx_packet_struct.command == TER_BEACON))
					{
						Serial.println("COMMS_TX: Transmission is off, skipping packet.");
						continue; // skip transmission if TX is off
					}

					// Prepare packet
					uint8_t tx_packet[PACKET_SIZE_MAX];
					uint8_t tx_packet_size = packetToData(&tx_packet_struct, tx_packet);

					// If Reed-Solomon encoding is enabled, encode the packet
					if (rs_enabled && tx_packet_struct.ecc)
					{
						// Encode data using Reed-Solomon ECC
						printPacket("Before encoding: ", tx_packet, tx_packet_size);

						encodeECC(tx_packet, tx_packet_size);
					}
					
					// Start transmission
					startTransmission(tx_packet, tx_packet_size);

					// Wait for transmission to end
					ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
				}
				
				COMMS_state = COMMS_IDLE; // go back to idle state after processing all packets
				break;
			}

			// COMMAND PROCESSING
			case COMMS_CMD:
			{
				Serial.println("COMMS_CMD: Processing command packets ... ");

				// Keep processing command packets until queue is empty
				while (uxQueueMessagesWaiting(RTOS_queue_cmd) > 0)
				{
					// Get command packet from queue
					Packet cmd_packet;
					xQueueReceive(RTOS_queue_cmd, &cmd_packet, portMAX_DELAY);

					// Execute command
					int8_t cmd_state = executeTEC(&cmd_packet);
					if (cmd_state == PACKET_ERR_NONE)
					{
						// Command executed successfully, send ACK if needed
						if (isACKNeeded(&cmd_packet))
						{
							sendACK(cmd_packet.ecc, cmd_packet.command);
						}
					}
					else
					{
						// Command execution failed, send NACK with error code
						sendNACK(cmd_packet.ecc, cmd_packet.command, cmd_state);
					}
				}

				COMMS_state = COMMS_IDLE; // go back to idle state after processing all commands
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

// Functions and libraries
#include "esp32_gs_fun.h"

// Define LoRa module
SX1268 radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY);
// SX1268 radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, RADIOLIB_NC);

// Define the queue handles
QueueHandle_t RTOS_queue_TX;

// General comunication state configuration 
uint8_t GS_state = GS_IDLE;

// Main GS loop
void GS_stateMachine(void *parameter)
{	
	printStartupMessage("GS");

	// Create queues
	RTOS_queue_TX = xQueueCreate(TX_QUEUE_SIZE, sizeof(Packet));

	// Initial state
	GS_state = GS_IDLE;

	// Initialize Reed-Solomon error correction
	initialize_ecc();

	Serial.println("ok");
	
	// Start LoRa module
	Serial.print("[SX1268] Initializing ... ");
	int16_t radio_state = radio.begin(F, BW, SF, CR, RADIOLIB_SX126X_SYNC_WORD_PRIVATE, OUTPUT_POWER, PREAMBLE_LENGTH, TCXO_V, USE_LDO);
	printRadioStatus(radio_state, true); // block program if LoRa cannot be initialized

	// Set ISR to be called when packets are sent or received
	radio.setPacketSentAction(packetEvent);
	radio.setPacketReceivedAction(packetEvent);

	//radio.setBufferAction(packetEvent);
	
	// only way to end state machine is killing COMMS thread (by the OBC)
	for(;;)
	{
		
		// State machine
		switch (GS_state)
		{
			// NOTHING TO PROCESS
			// Check if there is packet waiting in the queue, in case it switch to GS_TX
			case GS_IDLE:
			{
				Serial.println("GS_IDLE: Waiting for events ... ");

				// Enable RX
				startReception();

				// Keep checking for events until state changes
				while (GS_state == GS_IDLE)
				{
					// Check if there are packets waiting in queue to be transmitted
					if (uxQueueMessagesWaiting(RTOS_queue_TX) > 0)
					{
						// Switch to TX state
						GS_state = GS_TX;
						break;
					}

					// Check if there is serial data to be transmitted
					if (Serial.available() > 0)
					{
						// Switch to serial state
						GS_state = GS_SERIAL;
						break;
					}

					// Check if a packet has been received
					// IDLE_TIMEOUT is the time to wait for a packet to be received before checking if there are other things to process
					if (ulTaskNotifyTake(pdFALSE, IDLE_TIMEOUT) != 0) // if a packet is received before timeout
					{
						// Switch to RX state
						GS_state = GS_RX;
						break;
					}

					// Nothing to do, repeat checks
				}
		
				break;
			}

			// PACKET RECEPTION
			case GS_RX:
			{
				Serial.println("GS_RX: Starting packet reception ... ");

				// Read packet
				uint8_t rx_data[PACKET_SIZE_MAX];
				uint8_t rx_data_size = radio.getPacketLength();
				int8_t rx_state = radio.readData(rx_data, rx_data_size);

				bool ecc = isDataECCEnabled(rx_data, rx_data_size);
				
				// Packet rx_packet; // create packet struct to store received data
				// rx_packet.init(ecc, 0); // initialize packet with default values

				// Reception successfull
				if (rx_state == RADIOLIB_ERR_NONE)
				{
					// Decode ECC data if enabled
					if (ecc)
					{
						printData("PACKET RAW: ", rx_data, rx_data_size);
						decodeECC(rx_data, rx_data_size);
					}
				}
				// Reception error
				else
				{
					// Always try to recover the packet, content can not be trusted otherwise
					ecc = true;
					Serial.printf("Reception error: %d\n", rx_state);
					printData("PACKET RAW: ", rx_data, rx_data_size);
					decodeECC(rx_data, rx_data_size);
				}

				// Print received data to serial
				printData("PACKET: ", rx_data, rx_data_size);

				// Report RSSI and SNR if available
				float RSSI = radio.getRSSI();
				float SNR = radio.getSNR();
				float freq_shift = radio.getFrequencyError();
				Serial.printf("RSSI: %.2f SNR: %.2f dF: %.2f\n", RSSI, SNR, freq_shift);

				// Reset to idle state after processing packet
				GS_state = GS_IDLE;
				break;
			}
			
			// PACKET TRANSMITION
			case GS_TX:
			{
				Serial.println("GS_TX: Starting packet transmission ... ");

				// Keep processing packets until queue is empty
				while (uxQueueMessagesWaiting(RTOS_queue_TX) > 0)
				{
					// Get packet data from queue
					Packet tx_packet_struct;
					xQueueReceive(RTOS_queue_TX, &tx_packet_struct, portMAX_DELAY);

					// Prepare packet
					uint8_t tx_packet[PACKET_SIZE_MAX];
					uint8_t tx_packet_size = packetToData(&tx_packet_struct, tx_packet);

					// If Reed-Solomon encoding is enabled, encode the packet
					if (tx_packet_struct.ecc)
					{
						// Encode data using Reed-Solomon ECC
						printData("Before encoding: ", tx_packet, tx_packet_size);

						encodeECC(tx_packet, tx_packet_size);
					}

					// Inject random errors for testing
					// This is for testing purposes only, remove in production code
					// With 50% probability, randomly change a byte in tx_packet
					// if (tx_packet_size > 0 && (esp_random() % 100) < 50)
					// {
					// 	uint8_t error_length = (esp_random() % 2) + 1; // Randomly choose 1 to 2 bytes to flip
					// 	uint8_t random_index = esp_random() % tx_packet_size;
					// 	for (uint8_t i = 0; i < error_length && random_index + i < tx_packet_size; ++i)
					// 	{
					// 		// Randomly flip bits in the selected byte
					// 		tx_packet[random_index + i] ^= (1 << (esp_random() % 8)); // Flip a random bit
					// 	}
						
					// 	Serial.printf("Injected error in packet at index %d, length %d\n", random_index, error_length);
					// }
					
					// Start transmission
					startTransmission(tx_packet, tx_packet_size);

					// Wait for transmission to end
					ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
				}
				
				GS_state = GS_IDLE; // go back to idle state after processing all packets
				break;
			}

			// SERIAL INPUT
			case GS_SERIAL:
			{
				Serial.println("GS_SERIAL: processing serial input ...");

				handleSerialInput();

				GS_state = GS_IDLE;
				break;
			}

			default:
			{
				Serial.println("Unknown GS state! Resetting to IDLE.");
				GS_state = GS_IDLE; // reset to idle state if unknown state
				break;
			}
		}
	}
	
	vTaskDelete(NULL);
}

// Functions and libraries
#include "esp32_fun.h"


// ---------------------------------
// PRINT FUNCTIONS
// ---------------------------------


// Print boot message on serial
void printStartupMessage(const char* device)
{
	vTaskDelay(500);
	Serial.print(device);
	Serial.print(" starting ... ");
	vTaskDelay(100);
	// Serial.println("ok");
	// Serial.println("");
	// vTaskDelay(100);
}

// Print radio status on serial
void printRadioStatus(int8_t state, bool blocking)
{

	// No error reported
	if (state == RADIOLIB_ERR_NONE)
	{
		Serial.println("ok");
	} else
	{
		Serial.println((String) "failed! --> Code: " + state);
		if (blocking)
		{
			Serial.println("Blocking program until user forces restart!");
			while (true)
			{
				delay(10000);
				Serial.println("Program blocked, please restart ...");
			}
		}
	}
}

// Print received packet on serial
// void printPacket(const uint8_t* packet, uint8_t length)
// {
// 	Serial.print("DATA: ");
// 	for (uint8_t i = 0; i < length; i++)
// 	{
// 		Serial.print((char)packet[i]);
// 		Serial.print(" ");
// 	}
// 	Serial.println();
// }


// ---------------------------------
// HELPER FUNCTIONS
// ---------------------------------

// Get current UNIX time
uint32_t getUNIX()
{
	// TODO: fix this with time variable
	return millis() / 1000; // convert milliseconds to seconds
}

// MAC function to generate a message authentication code
uint32_t makeMAC(uint32_t timestamp, uint32_t secret_key)
{
    uint32_t x = timestamp ^ secret_key;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = (x >> 16) ^ x;
    return x;
}

// ---------------------------------
// RADIO FUNCTIONS
// ---------------------------------

// Notify COMMS task of a radio event (called when packet sent or received)
ICACHE_RAM_ATTR void packetEvent(void)
{
	// Define task to be notified
	BaseType_t higher_priority_task_woken = pdFALSE;
	TaskHandle_t task_to_notify = RTOS_handle_COMMS_StateMachine; // always notify COMMS task

	// Notify task
	vTaskNotifyGiveFromISR(task_to_notify, &higher_priority_task_woken); // notify task
	portYIELD_FROM_ISR(higher_priority_task_woken);
}

// Start LoRa reception
void startReception(void)
{
	// Start listening
	Serial.print("Listening ... ");
	int8_t rx_state = radio.startReceive();

	// Report listening status
	printRadioStatus(rx_state);
}

// Start LoRa transmission
void startTransmission(uint8_t* tx_packet, uint8_t packet_size)
{
	// Start transmission
	Serial.print("Transmitting: ");
	// Serial.print("Transmitting: " + String((char*) tx_packet) + " ... ");
	for (uint8_t i = 0; i < packet_size; i++)
	{
		Serial.print(tx_packet[i], HEX);
		// Serial.print("=");
		// Serial.print(tx_packet[i]);
		Serial.print(" ");
	}
	Serial.print("... ");

	int8_t tx_state = radio.startTransmit(tx_packet, packet_size);

	// Set task to notify by packetEvent
	//xTaskNotify = RTOS_TX_manager_handle; //NOT CLEAR
	//POSSIBLE CORRECTION:
	//xTaskNotifyGive(RTOS_TX_manager_handle);
	
	// TODO: should transmissions tatus be reported there or after full transmission?
}

// ---------------------------------
// PACKET FUNCTIONS
// ---------------------------------


// Initialize packet with default values
Packet::Packet()
{
	state = PACKET_ERR_NONE; // no error
	ecc = false; // RS encoding off by default
	station = MISSION_ID; // default station ID
	command = 0; // default command
	ID_total = 1; // total number of IDs in command
	ID = 1; // ID of this packet
	time_unix = getUNIX(); // default UNIX time
	MAC = makeMAC(time_unix, SECRET_KEY); // default MAC
	payload_length = 0; // no payload by default
	memset(payload, 0, PACKET_PAYLOAD_MAX); // clear payload
}

// Convert received packet to struct
Packet dataToPacket(const uint8_t* data, uint8_t length)
{
	// Create output struct
	Packet packet;

	// Byte 0: station ID
	packet.station = data[0];

	// Byte 1: RS flag
	if (data[1] == BYTE_RS_ON) {
		packet.ecc = true;
	} else if (data[1] == BYTE_RS_OFF) {
		packet.ecc = false;
	} else {
		packet.state = PACKET_ERR_RS;
		return packet;
	}

	// Byte 2: command (TEC type + Task value)
	packet.command = data[2];

	// Byte 3: ID_total and ID
	packet.ID_total = (data[3] >> 4) & 0x0F;
	packet.ID = data[3] & 0x0F;

	// Byte 4: Payload length
	packet.payload_length = data[4];

	// Check if packet length is valid
	if (length != PACKET_HEADER_LENGTH + packet.payload_length)
	{
		packet.state = PACKET_ERR_LENGTH; // packet too short
		return packet;
	}

	// Byte 5–8: MAC
	packet.MAC = ((uint32_t)data[5] << 24) | ((uint32_t)data[6] << 16) | ((uint32_t)data[7] << 8) | (uint32_t)data[8];

	// Byte 9–12: UNIX time
	packet.time_unix = ((uint32_t)data[9] << 24) | ((uint32_t)data[10] << 16) | ((uint32_t)data[11] << 8) | (uint32_t)data[12];

	// Check if MAC is valid
	uint32_t expected_MAC = makeMAC(packet.time_unix, SECRET_KEY); // SECRET_KEY is a predefined constant
	if (packet.MAC != expected_MAC)
	{
		packet.state = PACKET_ERR_MAC; // MAC error
		return packet;
	}

	// Parse payload
	for (uint8_t i = 0; i < packet.payload_length; i++)
	{
		packet.payload[i] = data[PACKET_HEADER_LENGTH + i];
	}

	// Packet successfully decoded
	packet.state = PACKET_ERR_NONE;
	return packet;
}

// Convert struct to packet to be sent
uint8_t packetToData(const Packet* packet, uint8_t* data)
{
	// Byte 0: station ID
	data[0] = packet->station;

	// Byte 1: RS encoding flag
	if (packet->ecc)
	{
		data[1] = BYTE_RS_ON; // RS encoding on
	}
	else
	{
		data[1] = BYTE_RS_OFF; // RS encoding off
	}

	// Byte 2: command
	data[2] = packet->command;

	// Byte 3: ID_total and ID
	data[3] = ((packet->ID_total & 0x0F) << 4) | (packet->ID & 0x0F); // first 4 bits ID_total, last 4 bits ID

	// Byte 4: payload length
	data[4] = packet->payload_length;

	// Byte 5–8: MAC
	data[5] = (packet->MAC >> 24) & 0xFF;
	data[6] = (packet->MAC >> 16) & 0xFF;
	data[7] = (packet->MAC >> 8) & 0xFF;
	data[8] = packet->MAC & 0xFF;

	// Byte 9–12: UNIX time
	data[9] = (packet->time_unix >> 24) & 0xFF;
	data[10] = (packet->time_unix >> 16) & 0xFF;
	data[11] = (packet->time_unix >> 8) & 0xFF;
	data[12] = packet->time_unix & 0xFF;

	// Payload bytes
	for (uint8_t i = 0; i < packet->payload_length; i++)
	{
		data[PACKET_HEADER_LENGTH + i] = packet->payload[i];
	}

	Serial.println("TX packet length: " + String(PACKET_HEADER_LENGTH + packet->payload_length));
	// Return total length of data
	return PACKET_HEADER_LENGTH + packet->payload_length;
}

// Process commands in serial input TODO: maybe this should not be part of comms state machine
void handleSerialInput()
{
	uint8_t packet_buffers[PACKET_CMD_MAX][PACKET_SIZE_MAX];
	uint8_t packet_lengths[PACKET_CMD_MAX];
	uint8_t packet_count = 0;

	uint8_t temp_buffer[PACKET_SIZE_MAX];
	uint8_t temp_length = 0;

	while (true)
	{
		vTaskDelay(100);  // Throttle

		while (Serial.available())
		{
			uint8_t c = Serial.read();

			if (c == '\n' || c == '\r')
			{
				// Check for "go" or "end" commands by comparing raw buffer
				if ((temp_length == 2 &&
					 (temp_buffer[0] == 'g' || temp_buffer[0] == 'G') &&
					 (temp_buffer[1] == 'o' || temp_buffer[1] == 'O')))
				{
					Serial.printf("Processing %d packet(s)...\n", packet_count);
					
					for (uint8_t i = 0; i < packet_count; ++i)
					{
						Packet packet = dataToPacket(packet_buffers[i], packet_lengths[i]);
						if (packet.state == PACKET_ERR_NONE)
						{
							xQueueSend(RTOS_queue_TX, &packet, portMAX_DELAY);
							// Serial.printf("Packet %d sent.\n", i);
						}
						else
						{
							Serial.printf("Packet %d invalid. Skipped. Error: %d\n", i, packet.state);
						}
					}
					return;
				}
				else if ((temp_length == 3 &&
						  (temp_buffer[0] == 'e' || temp_buffer[0] == 'E') &&
						  (temp_buffer[1] == 'n' || temp_buffer[1] == 'N') &&
						  (temp_buffer[2] == 'd' || temp_buffer[2] == 'D')))
				{
					Serial.println("Cancelled. Packets discarded.");
					return;
				}
				else if (temp_length > 0 && packet_count < PACKET_CMD_MAX)
				{
					memcpy(packet_buffers[packet_count], temp_buffer, temp_length);
					packet_lengths[packet_count] = temp_length;
					packet_count++;
					Serial.printf("Stored packet %d (%d bytes): ", packet_count, temp_length);
					for (uint8_t i = 0; i < temp_length; ++i) {
						Serial.printf("%02X ", temp_buffer[i]);
					}
					Serial.println();
				}
				else
				{
					Serial.println("Error or packet limit reached. Skipping.");
				}

				temp_length = 0; // reset for next line
			}
			else
			{
				if (temp_length < PACKET_SIZE_MAX)
				{
					temp_buffer[temp_length++] = c;
				}
				else
				{
					Serial.println("Packet too long. Ignoring rest.");
				}
			}
		}
	}
}

// ---------------------------------
// COMMAND FUNCTIONS
// ---------------------------------

// Make command from packets and execute it
int8_t processCommand(const Packet* packets, uint8_t packets_total)
{
	// Initialize state
	uint8_t command_state = CMD_ERR_NONE;

	// Extract variables from first packet
	uint8_t station = packets[0].station;
	uint8_t TEC = packets[0].command;
	uint8_t ID_total = packets[0].ID_total;

	// Packets variables
	uint8_t packets_ID[ID_total]; // array to store IDs of packets in command
	
	// Check all packets in command for errors
	for (uint8_t i = 0; i < packets_total; i++)
	{
		packets_ID[i] = packets[i].ID;
		
		// Check if packet has error
		if (packets[i].state != PACKET_ERR_NONE)
		{
			Serial.println("Packet " + String(i + 1) + " has error: " + String(packets[i].state));
			command_state = CMD_ERR_PACKET; // mark command as invalid
		}

		// Check if packet header matches
		// TODO: if first packet is wrong, all packets are wrong (can be optimized)
		if (packets[i].station != station || packets[i].ID_total != ID_total || packets[i].command != TEC)
		{
			Serial.println("Packet " + String(i + 1) + " has different header!");
			command_state = CMD_ERR_HEADER; // mark command as invalid
		}

		// Check if packet ID is in range
		if (packets[i].ID > ID_total)
		{
			Serial.println("Packet " + String(i + 1) + " has invalid ID: " + String(packets[i].ID));
			command_state = CMD_ERR_ID; // mark command as invalid
		}
	}

	// Check for missing or double packets
	for (uint8_t expected_id = 1; expected_id <= packets_total; expected_id++)
	{
		uint8_t count = 0;
		for (uint8_t i = 0; i < packets_total; i++)
		{
			if (packets_ID[i] == expected_id)
			{
				count++;
			}
		}
		if (count == 0)
		{
			Serial.println("Missing packet with ID: " + String(expected_id));
			command_state = CMD_ERR_MISSING; // mark command as invalid 
			break;
		}
		else if (count > 1)
		{
			Serial.println("Duplicate packets found with ID: " + String(expected_id));
			command_state = CMD_ERR_ID; // mark command as invalid
		}
	}

	// Check if command is valid
	if (command_state != CMD_ERR_NONE)
	{
		Serial.println("Command is invalid!");
		return command_state; // command is invalid
	}
	
	// Now all packets are validated, we can start parsing command

	// Calculate total payload length
	uint8_t command_length = 0;
	for (uint8_t i = 0; i < packets_total; i++) {
		command_length += packets[i].payload_length;
	}

	// Combine payloads from all packets
	uint8_t command[command_length];
	uint8_t command_processed = 0;
	for (uint8_t i = 0; i < packets_total; i++)
	{
		for (uint8_t j = 0; j < packets[i].payload_length; j++)
		{
			command[command_processed] = packets[i].payload[j];
			command_processed++;
		}
	}
	
	// Execute command
	switch(TEC)
	{
		case TEC_OBC_REBOOT:
			// TODO execute command
			Serial.println("TEC: OBC_REBOOT!");
			break;
		case TEC_TLM_BEACON:
			// TODO process data
			Serial.println("TEC: TLM_BEACON!");
			break;
		default:
			Serial.println("Unknown TEC!");
			return false; // invalid TEC
	}

	return true; // command processed
}

// Send ACK packet to confirm last command received
void sendACK(uint8_t TEC)
{
	// Create ACK packet
	Packet ack_packet;

	ack_packet.command = TRC_ACK; // set command to ACK
	ack_packet.payload_length = 1; // payload is executed TEC
	ack_packet.payload[0] = TEC; // set payload to executed TEC

	// Send ACK packet to queue
	xQueueSend(RTOS_queue_TX, &ack_packet, portMAX_DELAY);
}

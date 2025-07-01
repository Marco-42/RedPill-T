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
void printPacket(const uint8_t* packet, uint8_t length)
{
	Serial.print("DATA: ");
	for (uint8_t i = 0; i < length; ++i)
	{
		Serial.printf("%02X ", packet[i]);
	}
	Serial.println();
}


// ---------------------------------
// HELPER FUNCTIONS
// ---------------------------------

// Initialize system time to a specific UNIX timestamp, or default to Jan 1, 2025 if 0
void setUNIX(uint32_t unixTime)
{
    time_t t;

    if (unixTime == 0)
    {
        // Default to Jan 1, 2025 at 00:00:00
        struct tm tm_time = {};
        tm_time.tm_year = 2025 - 1900;
        tm_time.tm_mon = 0;
        tm_time.tm_mday = 1;
        tm_time.tm_hour = 0;
        tm_time.tm_min = 0;
        tm_time.tm_sec = 0;

        t = mktime(&tm_time);
    }
    else
    {
        t = static_cast<time_t>(unixTime);
    }

    struct timeval now = { .tv_sec = t, .tv_usec = 0 };
    settimeofday(&now, nullptr);
}

// Get current UNIX time
uint32_t getUNIX()
{
	time_t now = time(nullptr);
    return static_cast<uint32_t>(now);
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
void Packet::init()
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
		Serial.printf("MAC error: expected %08X, got %08X\n", expected_MAC, packet.MAC);
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
	uint8_t packet_buffers[CMD_PACKETS_MAX][PACKET_SIZE_MAX];
	uint8_t packet_lengths[CMD_PACKETS_MAX];
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
				else if (temp_length > 0 && packet_count < CMD_PACKETS_MAX)
				{
					memcpy(packet_buffers[packet_count], temp_buffer, temp_length);
					packet_lengths[packet_count] = temp_length;
					packet_count++;
					Serial.printf("Stored packet %d (%d bytes): ", packet_count, temp_length);
					for (uint8_t i = 0; i < temp_length; ++i)
					{
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

// Check if packets form a valid command
int8_t checkPackets(const Packet* packets, uint8_t packets_total)
{
	// if (packets_total == 0) {
	// 	Serial.println("checkPackets: No packets received.");
	// 	return CMD_ERR_PACKET;
	// }

	uint8_t command_state = CMD_ERR_NONE;
	uint8_t station = packets[0].station;
	uint8_t TEC = packets[0].command;
	uint8_t ID_total = packets[0].ID_total;

	bool packet_received_flags[CMD_PACKETS_MAX + 1] = { false };
	uint8_t total_payload_length = 0;

	for (uint8_t i = 0; i < packets_total; i++)
	{
		uint8_t id = packets[i].ID;

		// Check state
		if (packets[i].state != PACKET_ERR_NONE)
		{
			Serial.printf("Packet %d has error: %d\n", i + 1, packets[i].state);
			command_state = CMD_ERR_PACKET;
		}

		// Header consistency
		if (packets[i].station != station || packets[i].ID_total != ID_total || packets[i].command != TEC)
		{
			Serial.printf("Packet %d header mismatch.\n", i + 1);
			command_state = CMD_ERR_HEADER;
		}

		// ID validity
		if (id == 0 || id > ID_total)
		{
			Serial.printf("Packet %d has invalid ID: %d\n", i + 1, id);
			command_state = CMD_ERR_ID;
		}

		// Check for duplicates
		if (packet_received_flags[id])
		{
			Serial.printf("Duplicate packet ID: %d\n", id);
			command_state = CMD_ERR_ID;
		}
		packet_received_flags[id] = true;

		total_payload_length += packets[i].payload_length;
	}

	// Check for missing packets
	for (uint8_t id = 1; id <= ID_total; id++)
	{
		if (!packet_received_flags[id]) {
			Serial.printf("Missing packet with ID: %d\n", id);
			command_state = CMD_ERR_MISSING;
			break;
		}
	}

	// TEC-specific validation
	// switch (TEC)
	// {
	// 	case TEC_SET_TIME:
	// 		if (total_payload_length != 4) {
	// 			Serial.printf("SET_TIME requires 4-byte payload, got %d\n", total_payload_length);
	// 			command_state = CMD_ERR_LENGTH;
	// 		}
	// 		break;
	// 	// Add future TEC-specific checks here
	// 	default:
	// 		break;
	// }

	if (command_state != CMD_ERR_NONE) {
		Serial.println("verifyCommand: Command is invalid.");
	}
	return command_state;
}

// Assemble and execute command from valid packets
bool executeCommand(const Packet* packets, uint8_t packets_total)
{
	if (packets_total == 0) return false;

	uint8_t TEC = packets[0].command;

	// Merge payloads
	uint8_t command_length = 0;
	for (uint8_t i = 0; i < packets_total; i++)
	{
		command_length += packets[i].payload_length;
	}
	uint8_t command[command_length];
	uint8_t offset = 0;
	for (uint8_t i = 0; i < packets_total; i++)
	{
		memcpy(command + offset, packets[i].payload, packets[i].payload_length);
		offset += packets[i].payload_length;
	}

	// Execute (assumes payloads are correct!)
	switch (TEC)
	{
		case TEC_OBC_REBOOT:
			Serial.println("TEC: OBC_REBOOT");
			ESP.restart();
			break;

		case TEC_EXIT_STATE:
			Serial.println("TEC: EXIT_STATE");
			break;

		case TEC_VAR_CHANGE:
			Serial.println("TEC: VAR_CHANGE");
			break;

		case TEC_SET_TIME:
		{
			Serial.println("TEC: SET_TIME");
			uint32_t time_new = ((uint32_t)command[0] << 24) | ((uint32_t)command[1] << 16) | ((uint32_t)command[2] << 8) | (uint32_t)command[3];
			setUNIX(time_new);
			Serial.printf("Time set to: %u\n", time_new);
			break;
		}

		case TEC_EPS_REBOOT:
			Serial.println("TEC: EPS_REBOOT");
			break;

		case TEC_ADCS_REBOOT:
			Serial.println("TEC: ADCS_REBOOT");
			break;

		case TEC_ADCS_TLE:
			Serial.println("TEC: ADCS_TLE");
			break;

		default:
			Serial.println("Unknown TEC!");
			return false;
	}

	return true;
}


// Send ACK packet to confirm last command received
void sendACK(uint8_t TEC)
{
	// Create ACK packet
	Packet ack_packet;
	ack_packet.init(); // initialize packet with default values

	ack_packet.command = TRC_ACK; // set command to ACK
	ack_packet.payload_length = 1; // payload is executed TEC
	ack_packet.payload[0] = TEC; // set payload to executed TEC

	// Send ACK packet to queue
	xQueueSend(RTOS_queue_TX, &ack_packet, portMAX_DELAY);
}

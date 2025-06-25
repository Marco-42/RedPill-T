// Functions and libraries
#include "esp32_fun.h"


// ---------------------------------
// PRINT FUNCTIONS
// ---------------------------------


// Print boot message on serial
void printStartupMessage(const char* device)
{
	delay(1000);
	Serial.print(device);
	Serial.print(" starting ... ");
	delay(500);
	Serial.println("ok");
	Serial.println("");
	delay(500);
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
	for (uint8_t i = 0; i < length; i++)
	{
		Serial.print((char)packet[i]);
		Serial.print(" ");
	}
	Serial.println();
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

// Convert received packet to struct
Packet dataToPacket(const uint8_t* data, uint8_t length)
{
	// Create output struct
	Packet packet;

	// Parse header
	if (data[0] == RS_ON_BYTE_1 && data[1] == RS_ON_BYTE_2)
	{
		packet.rs_encode = true; // RS encoding is used
	} else if (data[0] == RS_OFF_BYTE_1 && data[1] == RS_OFF_BYTE_2)
	{
		packet.rs_encode = false; // plain encoding is used
	} else
	{
		// Serial.println("Packet header is wrong!");
		packet.state = PACKET_ERR_RS; // TODO handle error
		return packet;
	}
	packet.station = data[2]; // station is byte 3
	packet.command = data[3]; // command is byte 4
	packet.ID_total = (data[4] & 0xF0) >> 4; // ID_total is the first 4 bits of byte 5
	packet.ID = data[4] & 0x0F; // ID is the last 4 bits of byte 5
	packet.time_unix = ((uint32_t)(data[5]) << 24) | ((uint32_t)(data[6]) << 16) | ((uint32_t)(data[7]) << 8) | (uint32_t)(data[8]); // time_unix is bytes 6-9

	// Parse payload (from byte 9 to byte 9 + payload_length)
	packet.payload_length = data[9]; // payload_length is byte 10
	uint8_t payload_start_byte = PACKET_HEADER_LENGTH + 1; // +1 because payload_length is byte 10, so payload starts from byte 11
	for (uint8_t i = 0; i < packet.payload_length; i++)
	{
		packet.payload[i] = data[payload_start_byte + i];
	}

	// Check end byte
	uint8_t end_byte = PACKET_HEADER_LENGTH + packet.payload_length; // array starts from 0 so no need to add 1
	if (data[end_byte] != BYTE_END)
	{
		// Serial.println("End byte value wrong!");
		packet.state = PACKET_ERR_END; // TODO handle error
		return packet;
	}

	// Check padding TODO?

	// Packet successfully decoded
	packet.state = PACKET_ERR_NONE;
	return packet;
}

// // Convert serial line to TX packet, GS only
// Packet serialToPacket(const String& line)
// {
// 	// Create output struct
// 	Packet packet;

// 	// Parse header
// 	packet.station = line[0]; // station is byte 1
// 	packet.TRC = line[1]; // TRC is byte 2
// 	packet.ID_total = (line[2] & 0xF0) >> 4; // ID_total is the first 4 bits of byte 3
// 	packet.ID = line[2] & 0x0F; // ID is the last 4 bits of byte 3
// 	packet.time_unix = ((uint32_t)(line[3]) << 24) | ((uint32_t)(line[4]) << 16) | ((uint32_t)(line[5]) << 8) | (uint32_t)(line[6]); // time_unix is bytes 4-7

// 	// Parse payload (from byte 8 to byte 8 + payload_length)
// 	packet.payload_length = line[7]; // payload_length is byte 8
// 	uint8_t payload_start_byte = PACKET_HEADER_LENGTH + 1; // +1 because payload_length is the 8th byte, so payload starts from 9th byte
// 	for (uint8_t i = 0; i < packet.payload_length && i < sizeof(packet.payload); i++)
// 	{
// 		packet.payload[i] = (uint8_t)line[i];
// 		// Serial.println((char)packet.payload[i]); // print payload for debug
// 	}

// 	return packet;
// }

// Convert struct to packet to be sent
uint8_t packetToData(const Packet* packet, uint8_t* data)
{
	// Fill data with packet information
	if (packet->rs_encode)
	{
		data[0] = RS_ON_BYTE_1; // RS encoding on byte 1
		data[1] = RS_ON_BYTE_2; // RS encoding on byte 2
	} else
	{
		data[0] = RS_OFF_BYTE_1; // RS encoding off byte 1
		data[1] = RS_OFF_BYTE_2; // RS encoding off byte 2
	}
	data[2] = MISSION_ID; // station
	data[3] = packet->command; // command
	data[4] = ((packet->ID_total & 0x0F) << 4) | (packet->ID & 0x0F); // fist 4 bits ID_total, last 4 bits ID
	data[5] = (packet->time_unix >> 24) & 0xFF; // time_unix byte 1
	data[6] = (packet->time_unix >> 16) & 0xFF; // time_unix byte 2
	data[7] = (packet->time_unix >> 8) & 0xFF; // time_unix byte 3
	data[8] = packet->time_unix & 0xFF; // time_unix byte 4

	// TODO: should RS parity bits be added here?

	// Copy payload to data
	for (uint8_t i = 0; i < packet->payload_length; i++)
	{
		data[PACKET_HEADER_LENGTH + i] = packet->payload[i];
	}

	// Add end byte
	data[PACKET_HEADER_LENGTH + packet->payload_length] = BYTE_END;

	Serial.println("TX packet length: " + String(PACKET_HEADER_LENGTH + packet->payload_length + 1)); // +1 for end byte
	// Return total length of data
	return PACKET_HEADER_LENGTH + packet->payload_length + 1; // +1 for end byte
}

// Process commands in serial input
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
                      queuePackets(packet_buffers, packet_lengths, packet_count);
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
                    Serial.printf("Stored packet %d (%d bytes): %s\n", packet_count, temp_length, temp_buffer);
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

// Queue packets for transmission
void queuePackets(uint8_t buffers[][PACKET_SIZE_MAX], const uint8_t* lengths, uint8_t count)
{
    for (uint8_t i = 0; i < count; ++i)
    {
        Packet packet = dataToPacket(buffers[i], lengths[i]);
        if (packet.state == PACKET_ERR_NONE)
        {
            xQueueSend(RTOS_queue_TX, &packet, portMAX_DELAY);
            Serial.printf("Packet %d sent.\n", i);
        }
        else
        {
            Serial.printf("Packet %d invalid. Skipped.\n", i);
        }
    }
}

// ---------------------------------
// COMMAND FUNCTIONS
// ---------------------------------


// Make TEC and payload from packets
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

// Arduino library
#include <Arduino.h>

// Custom functions
#include "comms_fun.h"


// ---------------------------------
// PRINT FUNCTIONS
// ---------------------------------


// Print boot message on serial
void printStartupMessage(const char* device)
{
	delay(1000);
	Serial.print(device);
	Serial.print(" starting... ");
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
				Serial.println("Program blocked, please restart...");
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
	Serial.print("Listening... ");
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
PacketRX dataToPacketRX(const uint8_t* data, uint8_t length)
{
	// Create output struct
	PacketRX packet;
	
	// Parse header
	packet.station = data[0];
	packet.TEC = data[1];
	packet.ID_total = (data[2] & 0xF0) >> 4; // ID_total is the first 4 bits of byte 3
	packet.ID = data[2] & 0x0F; // ID is the last 4 bits of byte 2 --> NON E' IL BYTE 3?
	packet.time_unix = (data[3] << 24) | (data[4] << 16) | (data[5] << 8) | data[6];
	//packet.payload_length = getPayloadLength(packet.TEC, packet.ID); //--> NOT DEFINE
	packet.payload_length = 3; //--> ONLY FOR DEBUG OPERATIONS TODO change this

	// Parse payload (from byte 14 to byte 14 + payload_length)
	uint8_t payload_start_byte = RX_PACKET_HEADER_LENGTH + 1;
	for (uint8_t i = 0; i < packet.payload_length; i++)
	{
		packet.payload[i] = data[payload_start_byte + i];
	}

	// Check end byte
	uint8_t end_byte = RX_PACKET_HEADER_LENGTH + packet.payload_length; // array starts from 0 so need to add 1
	if (data[end_byte] != BYTE_END)
	{
		Serial.println("End byte value wrong!");
		packet.state = PACKET_ERR_END; // TODO handle error
		return packet;
	}

	// Check padding TODO?

	// Packet successfully decoded
	packet.state = PACKET_ERR_NONE;

	return packet;
}


// Convert serial line to TX packet
PacketTX serialToPacketTX(const String& line, uint8_t ID, uint8_t ID_total)
{
	PacketTX packet;

	// TODO: change all of this
	packet.TRC = 1;
	packet.ID_total = ID_total; // total number of packets in command
	packet.ID = ID; // ID of this packet
	packet.time_unix = millis(); // millis since boot

	packet.payload_length = line.length();
	for (uint8_t i = 0; i < packet.payload_length && i < sizeof(packet.payload); i++)
	{
		packet.payload[i] = (uint8_t)line[i];
		// Serial.println((char)packet.payload[i]); // print payload for debug
	}

	return packet;
}

// Convert struct to packet to be sent
uint8_t packetTXtoData(const PacketTX* packet, uint8_t* data)
{
	// Fill data with packet information
	data[0] = MISSION_ID; // station
	data[1] = packet->TRC; // TRC
	data[2] = ((packet->ID_total & 0x0F) << 4) | (packet->ID & 0x0F); // fist 4 bits ID_total, last 4 bits ID
	data[3] = (packet->time_unix >> 24) & 0xFF; // time_unix byte 1
	data[4] = (packet->time_unix >> 16) & 0xFF; // time_unix byte 2
	data[5] = (packet->time_unix >> 8) & 0xFF; // time_unix byte 3
	data[6] = packet->time_unix & 0xFF; // time_unix byte 4

	// TODO: should RS parity bits be added here?

	// Copy payload to data
	for (uint8_t i = 0; i < packet->payload_length; i++)
	{
		data[TX_PACKET_HEADER_LENGTH + i] = packet->payload[i];
	}

	// Add end byte
	data[TX_PACKET_HEADER_LENGTH + packet->payload_length] = BYTE_END;

	// Return total length of data
	return TX_PACKET_HEADER_LENGTH + packet->payload_length + 1; // +1 for end byte
}


// ---------------------------------
// COMMAND FUNCTIONS
// ---------------------------------


// Make TEC and payload from packets
int8_t processCommand(const PacketRX* packets, uint8_t packets_total)
{
	// Initialize state
	uint8_t command_state = CMD_ERR_NONE;

	// Extract variables from first packet
	uint8_t station = packets[0].station;
	uint8_t TEC = packets[0].TEC;
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
		if (packets[i].station != station || packets[i].ID_total != ID_total || packets[i].TEC != TEC)
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

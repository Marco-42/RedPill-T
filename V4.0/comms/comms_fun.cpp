#include <Arduino.h>
#include "comms_fun.h"


// ---------------------------------
// PRINT FUNCTIONS
// ---------------------------------


// Print boot message on serial
void printBootMessage()
{
	Serial.println(" ");
	delay(1000);
	Serial.println("Booting...");
	delay(500);
	Serial.println("Boot complete.");
	delay(500);
}


// Print radio status on serial
void printRadioStatus(int8_t state, bool blocking = false)
{
	if (state == RADIOLIB_ERR_NONE)
	{
		Serial.println("ok");
	} else
	{
		Serial.println((String) "failed, code " + state);
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
	Serial.print("Data: ");
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


// ---------------------------------
// PACKET FUNCTIONS
// ---------------------------------

// Struct to hold packet information
struct PacketRX
{
	uint8_t state;
	uint8_t station;
	uint8_t TEC;
	uint8_t ID_total;
	uint8_t ID;
	uint32_t time_unix;
	uint8_t payload_length;
	uint8_t payload[RX_PACKET_SIZE_MAX];

};


// Convert received packet to struct
PacketRX makePacketRX(uint8_t* data, uint8_t length)
{
	// Create output struct
	PacketRX packet;
	
	// Parse header
	packet.station = data[0];
	packet.TEC = data[1];
	packet.ID_total = (data[2] & 0xF0) >> 4; // ID_total is the first 4 bits of byte 2
	packet.ID = data[2] & 0x0F; // ID is the last 4 bits of byte 2
	packet.time_unix = (data[3] << 24) | (data[4] << 16) | (data[5] << 8) | data[6];
	packet.payload_length = getPayloadLength(packet.TEC, packet.ID);

	// Parse payload (from byte 14 to byte 14 + payload_length)
	payload_start_byte = RX_PACKET_HEADER_LENGTH + 1
	for (uint8_t i = 0; i < payload_length; i++)
	{
		packet.payload[i] = data[payload_start_byte + i];
	}

	// Check end byte
	end_byte = RX_PACKET_HEADER_LENGTH + payload_length + 1;
	if (data[end_byte] != 0xFF)
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


// ---------------------------------
// COMMAND FUNCTIONS
// ---------------------------------


// Check if received telecommand is valid
bool checkCommandPackets(PacketRX* packets, uint8_t packets_total)
{
	

	// Return true if all checks passed
	return true; // no error
}


// Make TEC and payload from packets
bool processCommand(PacketRX* packets, uint8_t packets_total)
{
	// Extract variables from first packet
	uint8_t station = packets[0].station;
	uint8_t TEC = packets[0].TEC;
	uint8_t ID_total = packets[0].ID_total;

	// Check if single packet command is actually a single packet
	if(packets_total == 1)
	{
		if (packets[0].ID != ID_total)
		{
			Serial.println("Single packet command error!");
			return false; // invalid packet
		}
	}

	// Check if all packets in command do not have errors and have same header
	for (uint8_t i = 0; i < packets_total; i++)
	{
		if (packets[i].state != RX_PACKET_ERR_NONE)
		{
			Serial.println("Packet state error!");
			return false; // invalid packets
		}
		if (packets[i].station != station || packets[i].ID_total != ID_total || packets[i].TEC != TEC)
		{
			Serial.println("Packet header mismatch!");
			return false; // invalid packets
		}
	}

	// Check if packets are in order
	for (uint8_t i = 0; i < packets_total; i++)
	{
		if (packets[i].ID != i + 1) // array starts from 0, but ID from 1
		{
			Serial.println("Packet ID not consecutive!");
			return false; // invalid packets
		}
	}
	
	// Now all packets are validated, we can start parsing command

	// Calculate total payload length
	command_length = 0;
	for (uint8_t i = 0; i < packets_total; i++) {
		command_length += packets[i].payload_length;
	}

	// Combine payloads from all packets
	uint8_t command[command_length];
	for (uint8_t i = 0; i < packets_total; i++)
	{
		// TODO THIS NEEDS TO BE FIXED
		memcpy(command + i * packets[i].payload_length, packets[i].payload, packets[i].payload_length);
	}
	

	return true; // command processed
}

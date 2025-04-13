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
// RADIO STRUCTS
// ---------------------------------
struct PacketInfo
{
	uint8_t station;
	uint8_t TEC;
	uint8_t ID;
	uint32_t time_unix;
	uint8_t payload_length;
};

PacketInfo::init(uint8_t station, uint8_t TEC, uint8_t ID, uint32_t time_unix, uint8_t payload_length)
{
	this->station = station;
	this->TEC = TEC;
	this->ID = ID;
	this->time_unix = time_unix;
	this->payload_length = payload_length;
}

PacketInfo getPacketInfo(uint8_t* packet, uint8_t length)
{
	// Create output struct
	PacketInfo packet_info;
	

	packet_info.station = packet[0];
	packet_info.TEC = packet[1];
	packet_info.ID = packet[2];
	packet_info.time_unix = (packet[3] << 24) | (packet[4] << 16) | (packet[5] << 8) | packet[6];
	packet_info.payload_length = length - sizeof(PacketInfo);
	return packet_info;
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

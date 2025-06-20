/*
-------------------------------------------
	Telecom software - RedPill by J2050 
-------------------------------------------

Management software for the comunication beetwen two Lora module
Used module: TTGO LoRa32 T3_V1.6.1 433 MHz

	Default setting end other informations about the lora module: 
	https://github.com/jgromes/RadioLib/wiki/Default-configuration

	Used library:
	- Radiolib --> https://github.com/jgromes/RadioLib/tree/master

	Used board manager: 
	- Esp
	https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
*/

// Functions and libraries
#include "esp32_fun.h"


// ---------------------------------
// SETUP
// ---------------------------------

// FreeRTOS task handles
TaskHandle_t RTOS_handle_COMMS_StateMachine;

void setup()
{
	// Initialize serial port
	Serial.begin(9600);
	Serial.println("");
	printStartupMessage("SETUP");

	// FreeRTOS task creation

	// xTaskCreate(
	// 		time_display_update,	// Function that should be called
	// 		"Time display update",	 // Name of the task (for debugging)
	// 		1000,			// Stack size (bytes)
	// 		NULL,			// Parameter to pass
	// 		1,				 // Task priority, higher number = higher priority
	// 		&time_display_update_handle			 // Task handle
	// 		);

	// 	uxTaskGetStackHighWaterMark() to get maximum stack size for task

	// 	RTOS_semaphore = xSemaphoreCreateMutex();
	// 	RTOS_queue = xQueueCreate(1, sizeof(uint8_t));


	// COMMS_stateMachine task
	xTaskCreate(COMMS_stateMachine, "COMMS_stateMachine", 2000, NULL, 2, &RTOS_handle_COMMS_StateMachine);
}

// ---------------------------------
// LOOP (leaving empty for freeRTOS)
// ---------------------------------

void loop()
{
}
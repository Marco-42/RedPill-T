/*
	Programma di trasmissione/ricezione per il modulo SX1278 usando la libreria RadioLib 
	Un modulo manda in trasmissione un messaggio passato dall'utente in modalità seriale
	Il modulo ricevente ritrasmette poi all'utente una copia di tale messaggio
	Scheda usata: TTGO LoRa32 T3_V1.6.1 433 MHz

	Per le impostazioni di default per il modulo visualizzare il seguente link:
	https://github.com/jgromes/RadioLib/wiki/Default-configuration

	Per informazioni sulla libreria:
	https://github.com/jgromes/RadioLib/tree/master

	Librerie necessarie:
	- Esp32
	- Radiolib

	Link del board manager per l'esp32:
	https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
*/

#include <Arduino.h>

// ---------------------------------
// LORA CONFIGURATION
// ---------------------------------

// RadioLib library for communication management
#include <RadioLib.h>

// Initialize SX1278 LoRa module pins
#define CS_PIN 18
#define DIO0_PIN 26
#define RESET_PIN 23
#define DIO1_PIN 33
SX1278 radio = new Module(CS_PIN, DIO0_PIN, RESET_PIN, DIO1_PIN);
// SX1278 radio1 = new Module(18, 26, 23, 33);

// SX1278 configuration
#define F 433.0 // frequency [MHz]
#define BW 125.0 // bandwidth [kHz]
#define SF 10 // spreading factor
#define CR 8 // coding rate
#define SYNC_WORD 0x12 // standard for private communications
#define OUTPUT_POWER 10 // 10 dBm is set for testing phase (ideal value to use: 22 dBm)
#define PREAMBLE_LENGTH 8 // standard
#define GAIN 1 // set automatic gain control


// ---------------------------------
// FUNCTIONS
// ---------------------------------

// Print radio status
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

// ---------------------------------
// FreeRTOS CONFIGURATION
// ---------------------------------

// #include <FreeRTOS.h>

// Handles
static TaskHandle_t xTaskToNotify = NULL; // task to notify


// RX MANAGER TASK

// Handles
TaskHandle_t RTOS_RX_manager_handle;

int8_t rx_state;

// Helper function to start reception
void startReception(void)
{
	// Start listening
	Serial.print("Listening ... ");
	rx_state = radio.startReceive();

	// Set task to notify by packetEvent
	xTaskToNotify = RTOS_RX_manager_handle;

	// Report listening status
	printRadioStatus(rx_state);
}

// Main task
void RX_manager(void *parameter)
{
	uint8_t rx_packet_size;

	// uint8_t packet[] = {0x01, 0x23, 0x45, 0x67,
	//                   0x89, 0xAB, 0xCD, 0xEF};

	// Start listening
	startReception();

	for(;;)
	{
		// Wait for packet to be received
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
		Serial.println("Packet received...");

		// Read packet
		rx_packet_size = radio.getPacketLength();
    	uint8_t rx_packet[rx_packet_size];
		rx_state = radio.readData(rx_packet, rx_packet_size);
		

		// Parse packet
		if (rx_state == RADIOLIB_ERR_NONE) // successfull reception
		{
			// Print received packet
			Serial.print("Data: ");
			
			for (uint8_t i = 0; i < rx_packet_size; i++)
			{
				Serial.print((char)rx_packet[i]);
				Serial.print(" ");
			}
			Serial.println();

			// Print packet info
			Serial.println((String) "Length: " + rx_packet_size);
			Serial.println((String) "RSSI: " + radio.getRSSI());
			Serial.println((String) "SNR: " + radio.getSNR());
			Serial.println((String) "Frequency error: " + radio.getFrequencyError());
		}
		else if (rx_state == RADIOLIB_ERR_CRC_MISMATCH) // CRC error
		{
			Serial.println("CRC error!");
		}
		else // other error
		{
			Serial.println((String) "Error receiving packet: " + rx_state);
		}
	}

	// Clean task if there is error in execution
	vTaskDelete(NULL);
}

// Notify correct task of a radio event
ICACHE_RAM_ATTR void packetEvent(void)
{
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	configASSERT(xTaskToNotify != NULL); // Task to notify must be set

	vTaskNotifyGiveFromISR(xTaskToNotify, &xHigherPriorityTaskWoken); // notify task
	// xTaskToNotify = NULL; // Reset task to notify

	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}


// TX MANAGER TASK

#define TX_PACKET_SIZE 8

// Handles
TaskHandle_t RTOS_TX_manager_handle;
QueueHandle_t RTOS_TX_queue;

int8_t tx_state;

// Helper function to start transmission
void startTransmision(uint8_t *tx_packet, uint8_t packet_size)
{
	// Start transmission
	Serial.print("Transmitting: " + String((char*) tx_packet) + " ... ");
	tx_state = radio.startTransmit(tx_packet, packet_size);

	// Set task to notify by packetEvent
	xTaskToNotify = RTOS_TX_manager_handle;

	// TODO: should transmissions tatus be reported there or after full transmission?
}

// Main task
void TX_manager(void *parameter)
{
	uint8_t tx_packet[TX_PACKET_SIZE];
	uint16_t tx_start_tick;
	uint16_t tx_end_tick;
	uint16_t tx_time = 0;

	// uint8_t packet[] = {0x01, 0x23, 0x45, 0x67,
	//                   0x89, 0xAB, 0xCD, 0xEF};
	for(;;)
		{
		// Wait for packet to be queued
		xQueueReceive(RTOS_TX_queue, &tx_packet, portMAX_DELAY);
		tx_start_tick = xTaskGetTickCount();

		// Start transmission
		startTransmision(tx_packet, TX_PACKET_SIZE);

		// Wait for transmission to end
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

		// Report transmission status
		printRadioStatus(tx_state);

		// Calculate transmission time
		tx_end_tick = xTaskGetTickCount();
		tx_time = tx_time + (tx_end_tick - tx_start_tick) / pdMS_TO_TICKS(1);

		// Resume listening if no more packets are in the queue
		if (uxQueueMessagesWaiting(RTOS_TX_queue) == 0)
		{
			// Print transmission time
			Serial.println("Transmission time: " + String(tx_time) + " ms");
			tx_time = 0;

			// Start listening
			startReception();
		}
	}

	// Clean task if there is error in execution
	vTaskDelete(NULL);
}


// SERIAL MANAGER TASK

#define SERIAL_BUFFER_SIZE 64

// Handles
TaskHandle_t RTOS_serial_handle;

// Main task
void serial_manager(void *parameter)
{
	const TickType_t xDelay = pdMS_TO_TICKS(100); // 100 ms delay
	
	for(;;)
	{
		// Wait for data to be available on the serial port
		while (Serial.available() == 0)
		{
			vTaskDelay(xDelay);
		}
		
		// Debug
		// Serial.println("Data available on serial port");

		// Read the data from the serial port
		String data = Serial.readStringUntil('\n');
		
		// Reboot the device if the user sends "reboot"
		if (data == "reboot")
		{
			Serial.println("Rebooting ...");
			delay(1000);
			ESP.restart();
		}

		// Create packet(s) from user input
		uint8_t packets_needed = ceil((float) data.length() / TX_PACKET_SIZE); // calculate the number of packets needed
		
		// Debug
		Serial.println("Data read: " + data);
		Serial.println("Data length: " + String(data.length()));
		Serial.println("Packets needed: " + String(packets_needed));

		for (uint8_t i = 0; i < packets_needed; i++)
		{
			// Calculate the size of the current packet
			// uint8_t packet_size = min(TX_PACKET_SIZE, (int) data.length() - i * TX_PACKET_SIZE);

			// Initialize packet with 0 padding
			uint8_t packet[TX_PACKET_SIZE] = {0};

			// Copy data to the packet
			// data.getBytes(packet, packet_size + 1, i * TX_PACKET_SIZE);
			data.getBytes(packet, TX_PACKET_SIZE + 1, i * TX_PACKET_SIZE);

			// Debug
			// Serial.println("Packet " + String(i) + ": " + String((char*) packet));

			// Send packet to TX queue
			xQueueSend(RTOS_TX_queue, &packet, portMAX_DELAY);
		}
	}
}



// ---------------------------------
// SETUP
// ---------------------------------

void setup()
{
	// Initialize serial port
	Serial.begin(9600);
	Serial.println(" ");
	delay(1000);

	Serial.println("Booting...");
	delay(1000);

	// Initialize SX1278 with default settings
	Serial.print("[SX1278] Initializing ... ");
	int8_t state = radio.begin(F, BW, SF, CR, SYNC_WORD, OUTPUT_POWER, PREAMBLE_LENGTH, GAIN);
	// int8_t state = radio1.begin(433.0, 125.0, 10, 8, 0x12, 10, 8, 1);

	// Report radio status
	printRadioStatus(state, true);

	// Set ISRs to be called when packets are sent or received
	// radio.setPacketSentAction(packetSent);
	// radio.setPacketReceivedAction(packetReceived);
	radio.setPacketSentAction(packetEvent);
	radio.setPacketReceivedAction(packetEvent);

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


	// RX manager task
	xTaskCreate(RX_manager, "RX manager", 2000, NULL, 2, &RTOS_RX_manager_handle);

	// TX manager task
	RTOS_TX_queue = xQueueCreate(5, TX_PACKET_SIZE); // 5 packets of TX_PACKET_SIZE bytes max
	xTaskCreate(TX_manager, "TX manager", 2000, NULL, 3, &RTOS_TX_manager_handle);

 	// Serial manager task
	xTaskCreate(serial_manager, "Serial manager", 4000, NULL, 1, &RTOS_serial_handle);

}

// ---------------------------------
// LOOP (leaving empty for freeRTOS)
// ---------------------------------

void loop()
{
	
	// Serial.println("Waiting...");
	// delay(10000);
	// // Main loop
	// // Wait for user input
	// Serial.println("Enter message to send: ");
	// while (Serial.available() == 0)
	// {
	// 	delay(10);
	// }

	// // Read user input
	// String user_input = Serial.readStringUntil('\n');
	// user_input.toCharArray(packet, TX_PACKET_SIZE);

	// // Send packet
	// xQueueSend(RTOS_TX_queue, &packet, portMAX_DELAY);
}
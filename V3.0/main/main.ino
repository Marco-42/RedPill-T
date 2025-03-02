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
// SX1278 radio = new Module(CS_PIN, DIO0_PIN, RESET_PIN, DIO1_PIN);
SX1278 radio1 = new Module(18, 26, 23, 33);

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
		Serial.println("success!");
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

// Custom tick conversion
// #define pdTICKS_TO_MS(xTicks) (((TickType_t) (xTicks) * 1000u) / configTICK_RATE_HZ)

// ISR notification values
// #define TX_STARTING_BIT 0x01
// #define TX_ENDING_BIT 0x02
// #define RX_STARTING_BIT 0x03
// #define RX_ENDING_BIT 0x04

// Handles

// SemaphoreHandle_t RTOS_LoRa_semaphore;
// static TaskHandle_t xTaskToNotify = NULL; // Task to notify


// RX MANAGER TASK

// #define RX_START_IDX 0
// #define RX_STOP_IDX 1
// #define RX_PACKET_START_IDX 2
// #define RX_PACKET_END_IDX 3

/*

// Handles
TaskHandle_t RTOS_radio_manager_handle;

// Main task

void radio_manager(void *parameter)
{
	const TickType_t xMaxBlockTime = pdMS_TO_TICKS(1000);
	// const TickType_t xMaxBlockTime = portMAX_DELAY;
	BaseType_t xResult;

	uint8_t rx_packet[TX_PACKET_SIZE];
	int8_t rx_state;

	// uint8_t packet[] = {0x01, 0x23, 0x45, 0x67,
	//                   0x89, 0xAB, 0xCD, 0xEF};
	for(;;)
	{
		xResult = xTaskNotifyWait(pdFALSE,          // don't clear bits on entry
									ULONG_MAX,        // clear all bits on exit
									&ulNotifiedValue, // stores the notified value
									xMaxBlockTime );

		if(xResult == pdPASS) // Notification received, see which bits were set
		{
			// TX packet outgoing
			if((ulNotifiedValue & TX_STARTING_BIT) != 0)
			{
			// Nothing yet
			}

			// TX packet sent
			if((ulNotifiedValue & TX_ENDING_BIT) != 0)
			{
			// Resume reception
			Serial.println("Listening ... ");
			rx_state = radio1.startReceive(rx_packet);
			
			// Report listening status
			printRadioStatus(rx_state);
			}

			// RX packet incoming
			if((ulNotifiedValue & RX_STARTING_BIT) != 0) 
			{
			// Nothing yet
			}

			// RX packet received
			if((ulNotifiedValue & RX_ENDING_BIT) != 0)
			{
			// Nothing yet
			}

		}
		else // Notification not received in time
		{
			Serial.println((String) "Nothing happened since " + pdTICKS_TO_MS(xMaxBlockTime) + " ms");
			
		}
	}

	// Clean task if there is error in execution
	vTaskDelete(NULL);
}
*/


// RX MANAGER TASK

// Handles
TaskHandle_t RTOS_RX_manager_handle;

// Main task
void RX_manager(void *parameter)
{
	uint8_t rx_packet_size;
	int8_t rx_state;

	// uint8_t packet[] = {0x01, 0x23, 0x45, 0x67,
	//                   0x89, 0xAB, 0xCD, 0xEF};

	// Start listening
	Serial.print("Listening ... ");
	rx_state = radio1.startReceive();

	// Report listening status
	printRadioStatus(rx_state);

	for(;;)
	{
		// Wait for packet to be received
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
		Serial.println("Packet received...");

		// Read packet
		rx_packet_size = radio1.getPacketLength();
    	uint8_t rx_packet[rx_packet_size];
		rx_state = radio1.readData(rx_packet, rx_packet_size);
		

		// Parse packet
		if (rx_state == RADIOLIB_ERR_NONE) // successfull reception
		{
			// Print received packet
			Serial.println("Data: ");
			
			for (uint8_t i = 0; i < rx_packet_size; i++)
			{
				Serial.print((char)rx_packet[i]);
				Serial.print(" ");
			}

			// Print packet info
			Serial.println((String) "Length: " + rx_packet_size);
			Serial.println((String) "RSSI: " + radio1.getRSSI());
			Serial.println((String) "SNR: " + radio1.getSNR());
			Serial.println((String) "Frequency error: " + radio1.getFrequencyError());
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

// Notify radio task that packet is incoming
ICACHE_RAM_ATTR void packetIncoming(void)
{
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	// configASSERT(xTaskToNotify != NULL); // Task to notify must be set

	// prvClearInterrupt();

	// xTaskNotifyFromISR(radio_manager, RX_STARTING_BIT, eSetBits, &xHigherPriorityTaskWoken); // Notify task
	// vTaskNotifyGiveIndexedFromISR(radio_manager, RX_PACKET_START_IDX, &xHigherPriorityTaskWoken); // Notify task
	// vTaskNotifyGiveIndexedFromISR(xTaskToNotify, RX_PACKET_START_IDX, &xHigherPriorityTaskWoken); // Notify task
	// xTaskToNotify = NULL; // Reset task to notify

	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// Notify radio task that packet has been received
ICACHE_RAM_ATTR void packetReceived(void)
{
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	// configASSERT(xTaskToNotify != NULL); // Task to notify must be set

	// ;

	// xTaskNotifyFromISR(radio_manager, RX_ENDING_BIT, eSetBits, &xHigherPriorityTaskWoken); // notify task
	vTaskNotifyGiveFromISR(RTOS_RX_manager_handle, &xHigherPriorityTaskWoken); // notify task
	// vTaskNotifyGiveIndexedFromISR(xTaskToNotify, RX_PACKET_START_IDX, &xHigherPriorityTaskWoken); // notify task
	// xTaskToNotify = NULL; // Reset task to notify

	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}


// TX MANAGER TASK

#define TX_PACKET_SIZE 8

// Handles
TaskHandle_t RTOS_TX_manager_handle;
QueueHandle_t RTOS_TX_queue;

// Main task
void TX_manager(void *parameter)
{
	uint8_t tx_packet[TX_PACKET_SIZE];
	int8_t tx_state;

	// uint8_t packet[] = {0x01, 0x23, 0x45, 0x67,
	//                   0x89, 0xAB, 0xCD, 0xEF};
	for(;;)
		{
		// Wait for packet to be queued
		xQueueReceive(RTOS_TX_queue, &tx_packet, portMAX_DELAY);

		// Notify manager task that transmission is starting
		// xTaskNotify(radio_manager, TX_STARTING_BIT, eSetBits);

		// Start transmission
		Serial.print("Transmitting ... ");
		tx_state = radio1.startTransmit(tx_packet, TX_PACKET_SIZE);

		// Wait for transmission to end
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

		// Report transmission status
		printRadioStatus(tx_state);

		// Notify manager task that transmission is over
		// xTaskNotify(radio_manager, TX_ENDING_BIT, eSetBits);

		// Resume listening
		Serial.print("Listening ... ");
		int8_t rx_state = radio1.startReceive();

		// Report listening status
		printRadioStatus(rx_state);
	}

	// Clean task if there is error in execution
	vTaskDelete(NULL);
}

// Notify TX task that packet has been sent
ICACHE_RAM_ATTR void packetSent(void)
{
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	// configASSERT(xTaskToNotify != NULL); // Task to notify must be set

	vTaskNotifyGiveFromISR(RTOS_TX_manager_handle, &xHigherPriorityTaskWoken); // Notify task
	// vTaskNotifyGiveFromISR(xTaskToNotify, &xHigherPriorityTaskWoken); // Notify task
	// xTaskToNotify = NULL; // Reset task to notify

	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}


// SERIAL MANAGER TASK

// Handles
TaskHandle_t RTOS_serial_handle;

// Main task
void serial_manager(void *parameter)
{
	const TickType_t xDelay = 100 / portTICK_PERIOD_MS; // 100 ms delay

	String data;
	uint8_t packet[TX_PACKET_SIZE];
	
	for(;;)
	{
		// Wait for data to be available on the serial port
		while (Serial.available() == 0)
		{
			vTaskDelay(100);
		}
		
		// Read the data from the serial port
		data = Serial.readStringUntil('\n');
		
		// Create packet(s) from user input
		uint8_t packets_needed = ceil(data.length() / TX_PACKET_SIZE); // calculate the number of packets needed

		for (uint8_t i = 0; i < packets_needed; i++)
		{
			// Initialize packet with 0 padding
			uint8_t packet[TX_PACKET_SIZE] = {0};

			// Calculate the size of the current packet
			uint8_t packet_size = min(TX_PACKET_SIZE, (int) data.length() - i * TX_PACKET_SIZE);

			// Copy data to the packet
			data.getBytes(packet, packet_size, i * TX_PACKET_SIZE);

			xQueueSend(RTOS_TX_queue, &packet, portMAX_DELAY); // Send the packet to the TX queue
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
  delay(2000);

	Serial.println("Booting...");
  delay(2000);

	// Initialize SX1278 with default settings
	Serial.print("[SX1278] Initializing ... ");
	// int8_t state = radio1.begin(F, BW, SF, CR, SYNC_WORD, OUTPUT_POWER, PREAMBLE_LENGTH, GAIN);
	int8_t state = radio1.begin(433.0, 125.0, 10, 8, 0x12, 10, 8, 1);

	// Report radio status
	printRadioStatus(state, true);

	// // Set ISRs to be called when packets are sent or received
	// radio1.setPacketSentAction(packetSent);
	// // radio1.setDio0Action(setFlagTimeout, RISING); // LoRa preamble not detected
	// // radio1.setDio1Action(setFlagDetected, RISING); // LoRa preamble detected
	// radio1.setPacketReceivedAction(packetReceived);

	// FreeRTOS task creation

	// xTaskCreate(
		//	time_display_update,	// Function that should be called
		//	"Time display update",	 // Name of the task (for debugging)
		//	1000,			// Stack size (bytes)
		//	NULL,			// Parameter to pass
		//	1,				 // Task priority, higher number = higher priority
		//	&time_display_update_handle			 // Task handle
		//	);

		// uxTaskGetStackHighWaterMark() to get maximum stack size for task

		// RTOS_semaphore = xSemaphoreCreateMutex();
		// RTOS_queue = xQueueCreate(1, sizeof(uint8_t));


	// RX manager task
	// xTaskCreate(RX_manager, "RX manager", 2000, NULL, 1, &RTOS_RX_manager_handle);

	// // TX manager task
	// RTOS_TX_queue = xQueueCreate(5, 64); // 5 packets of 64 bytes max
	// xTaskCreate(TX_manager, "TX manager", 2000, NULL, 2, &RTOS_TX_manager_handle);

  // // Serial manager task
	// xTaskCreate(serial_manager, "Serial manager", 2000, NULL, 0, &RTOS_serial_handle);

}

// ---------------------------------
// LOOP
// ---------------------------------

void loop()
{
	
	Serial.println("Still alive!");
	delay(10000);
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

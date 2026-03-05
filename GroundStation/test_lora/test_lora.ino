// RadioLib library for communication management
#include <RadioLib.h>

#define LORA_CS 17
#define LORA_RST 32
#define LORA_DIO1 35
#define LORA_BUSY 33
// MOSI 23
// MISO 19
// SCK 18

// SX1268 LoRa module configuration
#define F 436.0 // frequency [MHz]
#define BW 125.0 // bandwidth [kHz]
#define SF 10 // spreading factor
#define CR 5 // coding rate
#define OUTPUT_POWER 10 // 10 dBm is set for testing phase (ideal value to use: 22 dBm)
#define PREAMBLE_LENGTH 8 // standard
#define TCXO_V 0 // no TCXO present in LoRa1268F30 module
#define USE_LDO false // external DC-DC in LoRa1268F30 module

void setup()
{
	// Initialize serial port
	Serial.begin(9600);
	Serial.println("");
  Serial.println("Booting...");

	Serial.print("MOSI: ");
	Serial.println(MOSI);
	Serial.print("MISO: ");
	Serial.println(MISO);
	Serial.print("SCK: ");
	Serial.println(SCK);
	Serial.print("SS: ");
	Serial.println(SS);

  Serial.println("Initializing module...");
  SX1268 radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY);
  int16_t radio_state = radio.begin(F, BW, SF, CR, RADIOLIB_SX126X_SYNC_WORD_PRIVATE, OUTPUT_POWER, PREAMBLE_LENGTH, TCXO_V, USE_LDO);
  Serial.print("Code: ");
  Serial.println(radio_state);

}

void loop()
{

}
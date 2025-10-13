// Functions and libraries
#include "esp32_gs_fun.h"


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
		// Serial.println("ok");
	}
	else
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

// Print received data on serial with optional prefix
void printData(const char* prefix, const uint8_t* data, uint8_t length)
{
    if (prefix != nullptr)
    {
        Serial.print(prefix);
    }
	
	for (uint8_t i = 0; i < length; ++i)
	{
		Serial.printf("%02X ", data[i]);
	}
	Serial.println();
}

// Print packet on serial with optional prefix
void printPacket(const char* prefix, const Packet* packet)
{
	if (prefix != nullptr) {
		Serial.print(prefix);
	}
	Serial.printf("%02X ", packet->station);
	Serial.printf("%02X ", packet->ecc ? RS_ON : RS_OFF);
	Serial.printf("%02X ", packet->command);
	Serial.printf("%02X ", packet->payload_length);
	// Print time_unix as 4 bytes, each as two hex digits, space-separated
	for (int i = 0; i < 4; ++i) {
		Serial.printf("%02X ", (packet->time_unix >> (8 * (3 - i))) & 0xFF);
	}
	// Print MAC as 4 bytes, each as two hex digits, space-separated
	for (int i = 0; i < 4; ++i) {
		Serial.printf("%02X ", (packet->MAC >> (8 * (3 - i))) & 0xFF);
	}
	for (uint8_t i = 0; i < packet->payload_length; ++i) {
		Serial.printf("%02X ", packet->payload[i]);
	}
	Serial.println();
}


// ---------------------------------
// HELPER FUNCTIONS
// ---------------------------------

// Process commands in serial input TODO: maybe this should not be part of GS state machine
void handleSerialInput()
{
	const uint16_t timeout_ms = 20; // max silence threshold between bytes
	const uint16_t max_wait_ms = 200; // max wait time for full line

	uint8_t buffer[PACKET_SIZE_MAX + 1]; // null-terminated buffer
	uint8_t length = 0;

	// Wait until something is available
	if (!Serial.available())
		return;

	uint32_t start_time = millis();

	// Read data until silence or timeout
	while ((millis() - start_time < max_wait_ms) && length < PACKET_SIZE_MAX)
	{
		while (Serial.available() && length < PACKET_SIZE_MAX)
		{
			buffer[length++] = Serial.read();
			start_time = millis(); // reset timeout on new byte
		}
		vTaskDelay(pdMS_TO_TICKS(timeout_ms)); // brief silence wait
	}

	buffer[length] = '\0';  // null-terminate for safety
	String line = String((char*)buffer);
    line.trim();  // remove leading/trailing whitespace, newlines

	// Handle radio settings line
	if (line.startsWith("RADIO:"))
	{
		line = line.substring(6);  // Remove "RADIO:"
		
		float freq_mhz = 0;
		float bw_khz = 0;
		uint8_t sf = 0;
		uint8_t cr = 0;
		int8_t power = 0;

		String params[5];
		uint8_t param_index = 0;
		uint8_t index_last = 0;
		uint8_t index_space = 0;

		while (param_index < 5 && (index_space = line.indexOf(' ', index_last)) != -1)
		{
			params[param_index++] = line.substring(index_last, index_space);
			index_last = index_space + 1;
		}

		if (param_index < 5 && index_last < line.length())
		{
			params[param_index++] = line.substring(index_last);
		}

		if (param_index == 5)
		{
			freq_mhz = params[0].toFloat();
			bw_khz = params[1].toFloat();
			sf = (uint8_t)params[2].toInt();
			cr = (uint8_t)params[3].toInt();
			power = (int8_t)params[4].toInt();

			int8_t state = 0;
			state += radio.setFrequency(freq_mhz);
			state += radio.setBandwidth(bw_khz);
			state += radio.setSpreadingFactor(sf);
			state += radio.setCodingRate(cr);
			state += radio.setOutputPower(power);

			if (state == 0)
			{
				Serial.printf("RADIO settings: F %.2f MHz, BW %.2f kHz, SF %u, CR %u, Power %d dBm\n",
							freq_mhz, bw_khz, sf, cr, power);
			}
			else
			{
				Serial.printf("RADIO error: failed to apply settings (errors: %d)\n", state);
			}
		}
		else
		{
			Serial.println("RADIO error: expected 5 parameters (F, BW, SF, CR, Power)");
		}
	}
	// Handle command line
	else if (line.startsWith("TEC:"))
	{
		line = line.substring(4); // remove "TEC:"

		uint8_t data[PACKET_SIZE_MAX];
		uint8_t data_len = 0;

		int index_last = 0;
		int index_space = 0;
		String token_string;

		while (data_len < PACKET_SIZE_MAX)
		{
			index_space = line.indexOf(' ', index_last);

			if (index_space == -1)
			{
				// Last token (or only token)
				if (index_last < line.length())
				{
					token_string = line.substring(index_last);
					if (token_string.length() > 0)
					{
						data[data_len++] = (uint8_t) strtoul(token_string.c_str(), nullptr, 16);
					}
				}
				break;
			}
			else
			{
				token_string = line.substring(index_last, index_space);
				token_string.trim(); // handle extra spaces safely
				if (token_string.length() > 0)
				{
					data[data_len++] = (uint8_t) strtoul(token_string.c_str(), nullptr, 16);
				}
				index_last = index_space + 1;
			}
		}

		if (data_len > 0)
		{
			Packet packet;
			dataToPacket(data, data_len, &packet);

			if (packet.state == PACKET_ERR_NONE)
			{
				xQueueSend(RTOS_queue_TX, &packet, 0);
				Serial.printf("Packet queued (%d bytes): ", data_len);
				printData("", data, data_len);
			}
			else
			{
				Serial.printf("Invalid packet. Error: %d\n", packet.state);
			}
		}
		else
		{
			Serial.println("PACKET error: No valid data");
		}
	}

	// Display error
	else
	{
		Serial.println("Unrecognized line format");
	}
}


// ---------------------------------
// RADIO FUNCTIONS
// ---------------------------------

// Notify GS task of a radio event (called when packet sent or received)
ICACHE_RAM_ATTR void packetEvent(void)
{
	// Define task to be notified
	BaseType_t higher_priority_task_woken = pdFALSE;
	TaskHandle_t task_to_notify = RTOS_handle_GS_StateMachine; // always notify GS task

	// Notify task
	vTaskNotifyGiveFromISR(task_to_notify, &higher_priority_task_woken); // notify task
	portYIELD_FROM_ISR(higher_priority_task_woken);
}

// Start LoRa reception
void startReception(void)
{
	// Start listening
	// Serial.print("Listening ... ");
	int8_t rx_state = radio.startReceive();

	// Report listening status
	printRadioStatus(rx_state);
}

// Start LoRa transmission
void startTransmission(uint8_t* tx_packet, uint8_t packet_size)
{
	// Start transmission
	printData("Transmitting: ", tx_packet, packet_size);
	int8_t tx_state = radio.startTransmit(tx_packet, packet_size);
	
	// Report transmission status
	printRadioStatus(tx_state);
}


// ---------------------------------
// PACKET FUNCTIONS
// ---------------------------------

int8_t Packet::setPayload(const uint8_t* data, uint8_t length)
{
	if (length > PACKET_PAYLOAD_MAX)
	{
		return PACKET_ERR_LENGTH;
	}

	memcpy(payload, data, length);
	payload_length = length;

	return PACKET_ERR_NONE;
}

// Convert received data to packet with validation
void dataToPacket(const uint8_t* data, uint8_t length, Packet* packet)
{
	// Check if data size can be decoded
	if (length < PACKET_HEADER_LENGTH || length > PACKET_SIZE_MAX)
	{
		packet->state = PACKET_ERR_LENGTH; // Invalid packet length
		return; 
	}

	// Byte 0: station ID
	packet->station = data[0];

	// Byte 1: RS flag
	if (data[1] == RS_ON)
	{
		packet->ecc = true;
	}
	else if (data[1] == RS_OFF)
	{
		packet->ecc = false;
	}
	else
	{
		packet->state = PACKET_ERR_RS; // invalid RS flag
		return;
	}

	// Byte 2: command (TEC type + Task value)
	packet->command = data[2];

	// Byte 3: Payload length
	packet->payload_length = data[3];

	// Remove padding bytes
	if (length > PACKET_HEADER_LENGTH + packet->payload_length)
	{
		// Remove all trailing padding bytes
		while (length > PACKET_HEADER_LENGTH + packet->payload_length)
		{
			if (data[length - 1] == RS_PADDING) // Check if last byte is padding
			{
				// Remove padding byte
				length--;
			}
			else
			{
				// A padding byte was expected but not found
				packet->state = PACKET_ERR_LENGTH; // Invalid packet length
				return;
			}
		}
	}

	if (packet->payload_length > PACKET_PAYLOAD_MAX || length != PACKET_HEADER_LENGTH + packet->payload_length)
	{
		packet->state = PACKET_ERR_LENGTH; // invalid payload length
		return;
	}

	// Byte 4–7: UNIX time
	packet->time_unix = ((uint32_t)data[4] << 24) | ((uint32_t)data[5] << 16) | ((uint32_t)data[6] << 8) | (uint32_t)data[7];

	// Byte 8–11: MAC
	packet->MAC = ((uint32_t)data[8] << 24) | ((uint32_t)data[9] << 16) | ((uint32_t)data[10] << 8) | (uint32_t)data[11];

	// Parse payload
	packet->setPayload(data + PACKET_HEADER_LENGTH, packet->payload_length);

	// Packet successfully decoded
	packet->state = PACKET_ERR_NONE;
}

// Convert struct to packet to be sent, return length
uint8_t packetToData(const Packet* packet, uint8_t* data)
{
	// Byte 0: station ID
	data[0] = packet->station;

	// Byte 1: RS encoding flag
	if (packet->ecc)
	{
		data[1] = RS_ON; // RS encoding on
	}
	else
	{
		data[1] = RS_OFF; // RS encoding off
	}

	// Byte 2: command
	data[2] = packet->command;

	// Byte 3: payload length
	data[3] = packet->payload_length;

	// Byte 4–7: UNIX time
	data[4] = (packet->time_unix >> 24) & 0xFF;
	data[5] = (packet->time_unix >> 16) & 0xFF;
	data[6] = (packet->time_unix >> 8) & 0xFF;
	data[7] = packet->time_unix & 0xFF;

	// Byte 8–11: MAC
	data[8] = (packet->MAC >> 24) & 0xFF;
	data[9] = (packet->MAC >> 16) & 0xFF;
	data[10] = (packet->MAC >> 8) & 0xFF;
	data[11] = packet->MAC & 0xFF;

	// Payload bytes
	for (uint8_t i = 0; i < packet->payload_length; i++)
	{
		data[PACKET_HEADER_LENGTH + i] = packet->payload[i];
	}

	// Return total length of data
	return PACKET_HEADER_LENGTH + packet->payload_length;
}



// ---------------------------------
// ECC FUNCTIONS
// ---------------------------------

// Check if RS ECC is enabled in the packet
bool isDataECCEnabled(const uint8_t* data, uint8_t length)
{
	// Check if RS ECC is enabled in the packet
	if ((length % RS_BLOCK_SIZE == 0) && (data[1] != RS_OFF))
	{
		// If the data length is a multiple of RS_BLOCK_SIZE and RS ECC is not disabled, we can infer ECC is enabled
		// This also handles correctly the case of a corrupted data[1] byte (but not if it corrupts to RS_OFF)
		return true;
	}
	return false;
}

// Encode data using RS ECC and interleave the output
void encodeECC(uint8_t* data, uint8_t& data_len)
{
	uint8_t num_blocks = (data_len + DATA_BLOCK_SIZE - 1) / DATA_BLOCK_SIZE;

	// Temporary storage for codewords before interleaving
	uint8_t codewords[num_blocks][RS_BLOCK_SIZE];

	// Encode each block with padding as needed
	for (uint8_t i = 0; i < num_blocks; ++i)
	{
		uint8_t block[DATA_BLOCK_SIZE] = {RS_PADDING}; // Initialize with padding byte
		uint8_t remaining = data_len - i * DATA_BLOCK_SIZE;
		uint8_t copy_len = remaining >= DATA_BLOCK_SIZE ? DATA_BLOCK_SIZE : remaining;

		memcpy(block, data + i * DATA_BLOCK_SIZE, copy_len);
		encode_data(block, DATA_BLOCK_SIZE, codewords[i]);
	}

	// Interleave column-wise into a temporary buffer
	uint8_t interleaved[num_blocks * RS_BLOCK_SIZE];
	for (uint8_t col = 0; col < RS_BLOCK_SIZE; ++col) {
		for (uint8_t row = 0; row < num_blocks; ++row) {
			interleaved[col * num_blocks + row] = codewords[row][col];
		}
	}

	// Copy interleaved codewords back to original data buffer
	memcpy(data, interleaved, num_blocks * RS_BLOCK_SIZE);

	// Update data length to encoded size (with parity)
	data_len = num_blocks * RS_BLOCK_SIZE;
}

// Decode data by deinterleaving and correcting errors
int8_t decodeECC(uint8_t* data, uint8_t& data_len)
{
	int8_t error = PACKET_ERR_NONE;
	uint8_t num_blocks = data_len / RS_BLOCK_SIZE;

	// Temporary storage for deinterleaved codewords
	uint8_t codewords[num_blocks][RS_BLOCK_SIZE];

	// Deinterleave: undo column-wise interleaving
	for (uint8_t col = 0; col < RS_BLOCK_SIZE; ++col)
	{
		for (uint8_t row = 0; row < num_blocks; ++row)
		{
			codewords[row][col] = data[col * num_blocks + row];
		}
	}

	// Decode each codeword and write back only the data portion (without parity) in-place into data buffer
	uint8_t write_pos = 0;
	for (uint8_t i = 0; i < num_blocks; ++i)
	{
		uint8_t decoded[RS_BLOCK_SIZE] = {0};

		// decode_data takes encoded codeword and outputs decoded codeword (data + parity)
		decode_data(codewords[i], RS_BLOCK_SIZE);

		if (check_syndrome() != 0)
		{
			int8_t result = correct_errors_erasures(codewords[i], RS_BLOCK_SIZE, 0, nullptr);
			if (result == 0)
			{
				Serial.printf("RS decode failed on block %d\n", i);
				error = PACKET_ERR_DECODE; // set error code for RS decode failure
			}
			else
			{
				Serial.printf("RS decode corrected errors in block %d\n", i);
			}
		}
		// Copy only the data bytes (without parity) back to the original data buffer
		memcpy(data + write_pos, codewords[i], DATA_BLOCK_SIZE);
		write_pos += DATA_BLOCK_SIZE;
	}

	// Update data_len to new decoded length (without parity)
	data_len = num_blocks * DATA_BLOCK_SIZE;

	return error; // return error code
}


// ---------------------------------
// COMMAND FUNCTIONS
// ---------------------------------



// ---------------------------------
// TIMERS AND RTOS CONFIGURATION
// ---------------------------------



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
// uint32_t makeMAC(uint32_t timestamp, uint32_t secret_key)
// {
//     uint32_t x = timestamp ^ secret_key;
//     x = ((x >> 16) ^ x) * 0x45d9f3b;
//     x = ((x >> 16) ^ x) * 0x45d9f3b;
//     x = (x >> 16) ^ x;
//     return x;
// }
int8_t makeMAC(const Packet* packet, uint32_t* out_mac)
{
    uint8_t buffer[PACKET_HEADER_LENGTH + PACKET_PAYLOAD_MAX]; // header + max payload
    uint8_t full_output[32]; // full HMAC-SHA256 output
    int ret = 0;

    const mbedtls_md_info_t* md_info;
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);

    // === Construct header ===
    buffer[0] = packet->station;
    buffer[1] = packet->ecc ? RS_ON : RS_OFF;
    buffer[2] = packet->command;
    buffer[3] = packet->payload_length;

    // === Timestamp: bytes 4–7 ===
    buffer[4] = (packet->time_unix >> 24) & 0xFF;
    buffer[5] = (packet->time_unix >> 16) & 0xFF;
    buffer[6] = (packet->time_unix >> 8)  & 0xFF;
    buffer[7] = (packet->time_unix)       & 0xFF;

    // === MAC placeholder: bytes 8–11 ===
    buffer[8]  = 0;
    buffer[9]  = 0;
    buffer[10] = 0;
    buffer[11] = 0;

    // === Copy payload ===
    memcpy(buffer + PACKET_HEADER_LENGTH, packet->payload, packet->payload_length);

    // === Compute HMAC-SHA256 ===
    md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (!md_info) goto fail;

    ret = mbedtls_md_setup(&ctx, md_info, 1); // HMAC enabled
    if (ret != 0) goto fail;

    ret = mbedtls_md_hmac_starts(&ctx, SECRET_KEY, sizeof(SECRET_KEY));
    if (ret != 0) goto fail;

    ret = mbedtls_md_hmac_update(&ctx, buffer, PACKET_HEADER_LENGTH + packet->payload_length);
    if (ret != 0) goto fail;

    ret = mbedtls_md_hmac_finish(&ctx, full_output);
    if (ret != 0) goto fail;

    *out_mac = ((uint32_t)full_output[0] << 24) |
               ((uint32_t)full_output[1] << 16) |
               ((uint32_t)full_output[2] << 8)  |
               ((uint32_t)full_output[3]);

    mbedtls_md_free(&ctx);
    return PACKET_ERR_NONE;

fail:
    mbedtls_md_free(&ctx);
    return PACKET_ERR_MAC;
}

// Write float as 4 byte big endian
void writeFloatToBytes(float value, uint8_t* buffer)
{
	uint32_t value_int;
	memcpy(&value_int, &value, sizeof(float));  // interpret float bits as uint32_t

	buffer[0] = (value_int >> 24) & 0xFF;
	buffer[1] = (value_int >> 16) & 0xFF;
	buffer[2] = (value_int >> 8) & 0xFF;
	buffer[3] = value_int & 0xFF;
}

// Process commands in serial input TODO: maybe this should not be part of comms state machine
void handleSerialInput()
{
	uint8_t packet_buffers[CMD_QUEUE_SIZE][PACKET_SIZE_MAX];
	uint8_t packet_lengths[CMD_QUEUE_SIZE];
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
					// Serial.printf("Processing %d packet(s)...\n", packet_count);
					
					for (uint8_t i = 0; i < packet_count; ++i)
					{
						Packet packet;
						dataToPacket(packet_buffers[i], packet_lengths[i], &packet);
						if (packet.state == PACKET_ERR_NONE)
						{
							xQueueSend(RTOS_queue_TX, &packet, 0);
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
				else if (temp_length > 0 && packet_count < CMD_QUEUE_SIZE)
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

// Initialize packet with default values
void Packet::init(bool rs_enabled, uint8_t cmd)
{
	state = PACKET_ERR_NONE; // no error
	station = MISSION_ID; // default station ID
	command = cmd; // set command type
	
	if (rs_enabled)
	{
		switch (command)
		{
			case TER_BEACON:
				ecc = false; // disable RS encoding only for TER_BEACON
				break;
			default:
				ecc = true; // enable RS encoding for all other commands
				break;
		}
	}
	else
	{
		ecc = false; // disable RS encoding by default
	}

	time_unix = 0; // default UNIX time
	MAC = 0; // default MAC

	payload_length = 0; // no payload by default
	memset(payload, 0, PACKET_PAYLOAD_MAX); // clear payload
}

// Set packet payload data and length
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

// Seal packet by calculating MAC and setting time
int8_t Packet::seal()
{
	time_unix = getUNIX();

	uint32_t mac_calculated;
	int8_t mac_state = makeMAC(this, &mac_calculated);

	if (mac_state != PACKET_ERR_NONE)
		return mac_state;

	MAC = mac_calculated;
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
	
	if (!isTEC(packet->command)) // Check if command is a valid TEC
	{
		packet->state = PACKET_ERR_CMD_UNKNOWN; // unknown command
		return;
	}

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

	// Check if MAC is valid
	uint32_t mac_calculated;
	int8_t mac_state = makeMAC(packet, &mac_calculated);

	if (mac_state != PACKET_ERR_NONE)
	{
		packet->state = mac_state; // MAC error
		return;
	}
	if (mac_calculated != packet->MAC)
	{
		packet->state = PACKET_ERR_MAC; // MAC mismatch
		return;
	}
	// Serial.printf("Packet MAC: %08X, Calculated MAC: %08X\n", packet->MAC, mac_calculated);

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

// Execute TEC from valid packets
int8_t executeTEC(const Packet* cmd)
{
	if (cmd == nullptr)
	{
		return PACKET_ERR_CMD_POINTER;
	}

	// Execute (assumes payloads are correct!)
	switch (cmd->command)
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

			uint32_t time_new = ((uint32_t)cmd->payload[0] << 24) | ((uint32_t)cmd->payload[1] << 16) | ((uint32_t)cmd->payload[2] << 8) | (uint32_t)cmd->payload[3];
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
		
		case TEC_LORA_STATE:
		{
			Serial.println("TEC: LORA_STATE");

			// Unpack LoRa state from payload
			uint8_t tx_state_new = cmd->payload[0];
			uint8_t val0 = (tx_state_new >> 0) & 0b1111;
			uint8_t val1 = (tx_state_new >> 4) & 0b1111;
			Serial.printf("LoRa TX State new: %d, values: %d, %d\n", tx_state_new, val0, val1);

			if (val0 == val1)
			{
				tx_state_new = val0; // use common value
			}
			else
			{
				Serial.println("LoRa TX State values are not all the same!");
				return PACKET_ERR_CMD_PAYLOAD; // payload error if not all same
			}

			// Unpack duration from payload
			uint32_t duration = ((uint32_t)cmd->payload[1] << 16) | ((uint32_t)cmd->payload[2] << 8) | (uint32_t)cmd->payload[3];

			// Set LoRa TX state
			tx_state = tx_state_new;
			Serial.printf("LoRa TX State set to %d for %d s\n", tx_state, duration);

			// Cancel previous timer if still active
			if (RTOS_timer_lora_state != NULL)
			{
				xTimerStop(RTOS_timer_lora_state, 0);
				Packet* old_cmd = (Packet*)pvTimerGetTimerID(RTOS_timer_lora_state);
				xTimerDelete(RTOS_timer_lora_state, 0);
				vPortFree(old_cmd); // free memory of old command
				RTOS_timer_lora_state = NULL;
				Serial.println("Cancelled previous LoRa state timer");
			}
			
			// Handle duration-based reset
			if (duration > 0)
			{
				// Create packet with no delay to trigger the state change when timer expires
				Packet* delayed_cmd = (Packet*)pvPortMalloc(sizeof(Packet));
				if (delayed_cmd == nullptr)
				{
					Serial.println("Failed to allocate memory for delayed packet");
					return PACKET_ERR_CMD_MEMORY;
				}
				memcpy(delayed_cmd, cmd, sizeof(Packet));

				// Set default state and delay to 0 in payload
				printPacket("Packet before editing: ", delayed_cmd);
				delayed_cmd->payload[0] = ((TX_ON & 0b1111) << 4) | (TX_ON & 0b1111);
				delayed_cmd->payload[1] = 0x00;
				delayed_cmd->payload[2] = 0x00;
				delayed_cmd->payload[3] = 0x00;
				delayed_cmd->seal(); // seal the packet
				printPacket("Packet after editing: ", delayed_cmd);

				// Duration is in seconds, convert to ticks
				TickType_t ticks = pdMS_TO_TICKS(duration * 1000UL);

				// Create one-shot timer, no need to track globally
				RTOS_timer_lora_state = xTimerCreate("LoRa State Timer",
													ticks,
													pdFALSE,
													(void*)delayed_cmd,
													vQueueDelayedPacket);
				if (RTOS_timer_lora_state != NULL)
				{
					xTimerStart(RTOS_timer_lora_state, 0);
				}
				else
				{
					vPortFree(delayed_cmd);
				}
			}
			else
			{
				// Permanent state, nothing else to do
			}
			break;
		}

		case TEC_LORA_CONFIG:
		{
			Serial.println("TEC: LORA_CONFIG");

			// Byte 0–2: frequency in MHz
			uint32_t freq_mhz = (((uint32_t)cmd->payload[0] << 16) |
								((uint32_t)cmd->payload[1] << 8) |
								(uint32_t)cmd->payload[2]) / 1000;

			// Byte 3
			uint8_t b3 = cmd->payload[3];
			uint8_t bandwidth = (b3 >> 6) & 0b11; // 2 bits
			float bw_khz = 0.0;
			switch (bandwidth)
			{
			case 0:
				bw_khz = 62.5;
				break;
			case 1:
				bw_khz = 125.0;
				break;
			case 2:
				bw_khz = 250.0;
				break;
			case 3:
				bw_khz = 500.0;
				break;
			default:
				break;
			}
			uint8_t sf = ((b3 >> 3) & 0b111) + 6; // 3 bits, add 6
			uint8_t cr = (b3 & 0b111) + 5; // 3 bits, add 5

			// Byte 4
			uint8_t b4 = cmd->payload[4];
			int8_t power = ((b4 >> 3) & 0b11111) - 9; // 5 bits
			uint8_t reserved = b4 & 0b111; // 3 bits

			// Validate parameters
			Serial.printf("LoRa Config: Freq: %u kHz, BW: %.1f kHz, SF: %d, CR: %d, Power: %d dBm\n", freq_mhz, bw_khz, sf, cr, power);
			if (freq_mhz < 400 || freq_mhz > 500 || bw_khz < 62.5 || bw_khz > 500 ||
				sf < 6 || sf > 12 || cr < 5 || cr > 8 || power < -4 || power > 17) // TODO update power range for SX1268, -9 to +22 dBm
			{
				Serial.println("Invalid LoRa configuration parameters!");
				return PACKET_ERR_CMD_PAYLOAD; // payload error
			}

			// Apply LoRa configuration
			int8_t state = radio.setFrequency(freq_mhz);
			if (state != RADIOLIB_ERR_NONE)
			{
				Serial.println("Failed to set frequency");
				return PACKET_ERR_CMD_PAYLOAD; // payload error
			}

			state = radio.setBandwidth(bw_khz);
			if (state != RADIOLIB_ERR_NONE)
			{
				Serial.println("Failed to set bandwidth");
				return PACKET_ERR_CMD_PAYLOAD; // payload error
			}

			state = radio.setSpreadingFactor(sf);
			if (state != RADIOLIB_ERR_NONE)
			{
				Serial.println("Failed to set spreading factor");
				return PACKET_ERR_CMD_PAYLOAD; // payload error
			}

			state = radio.setCodingRate(cr);
			if (state != RADIOLIB_ERR_NONE)
			{
				Serial.println("Failed to set coding rate");
				return PACKET_ERR_CMD_PAYLOAD; // payload error
			}

			state = radio.setOutputPower(power);
			if (state != RADIOLIB_ERR_NONE)
			{
				Serial.println("Failed to set output power");
				return PACKET_ERR_CMD_PAYLOAD; // payload error
			}
		}

		case TEC_LORA_PING:
		{
			Serial.println("TEC: LORA_LINK");

			float RSSI = radio.getRSSI();
			float SNR = radio.getSNR();
			float freq_shift = radio.getFrequencyError();

			uint8_t payload[12]; // 12 bytes for RSSI, SNR, and frequency shift
			writeFloatToBytes(RSSI, payload); // first 4 bytes for RSSI
			writeFloatToBytes(SNR, payload + 4); // next 4 bytes for SNR
			writeFloatToBytes(freq_shift, payload + 8); // last 4 bytes for frequency shift

			// Create packet with LoRa link status
			Packet lora_packet;
			lora_packet.init(rs_enabled, TER_LORA_LINK); // initialize packet
			lora_packet.setPayload(payload, sizeof(payload)); // set payload to LoRa link status
			lora_packet.seal(); // seal packet with current time and MAC

			// Send packet to queue
			xQueueSend(RTOS_queue_TX, &lora_packet, 0);
			break;
		}

		case TEC_CRY_EXP:
		{
			Serial.println("TEC: CRY_EXP");

			// First 3 bytes: glass state (6 bits) + activation delay (18 bits)
			uint32_t glass_and_delay =
				((uint32_t)cmd->payload[0] << 16) |
				((uint32_t)cmd->payload[1] << 8) |
				((uint32_t)cmd->payload[2]);

			uint8_t glass_bits = (glass_and_delay >> 18) & 0b111111;
			uint32_t activation_delay = glass_and_delay & 0x3FFFF;  // 18 bits

			uint8_t glass = (glass_bits >> 3) & 0b111;
			uint8_t val0 = glass_bits & 0b111;

			// Validate repeated glass value
			if (glass != val0)
			{
				Serial.println("Glass value mismatch!");
				return PACKET_ERR_CMD_PAYLOAD;
			}

			Serial.printf("Glass state: %d (activation in %d s)\n", glass, activation_delay);
			
			// Cancel previous timer if still active
			if (RTOS_timer_cry_state != NULL)
			{
				xTimerStop(RTOS_timer_cry_state, 0);
				Packet* old_cmd = (Packet*)pvTimerGetTimerID(RTOS_timer_cry_state);
				xTimerDelete(RTOS_timer_cry_state, 0);
				vPortFree(old_cmd); // free memory of old command
				RTOS_timer_cry_state = NULL;
				Serial.println("Cancelled previous Crystals state timer");
			}
			
			// Delayed activation
			if (activation_delay > 0)
			{
				// Create packet with no delay and no pictures to trigger the state change when timer expires
				Packet* delayed_cmd = (Packet*)pvPortMalloc(sizeof(Packet));
				if (delayed_cmd == nullptr)
				{
					Serial.println("Failed to allocate memory for delayed packet");
					return PACKET_ERR_CMD_MEMORY;
				}
				memcpy(delayed_cmd, cmd, sizeof(Packet));

				// Set wanted state and delay to 0 in payload
				printPacket("Packet before editing: ", delayed_cmd);
				memset(delayed_cmd->payload, 0, delayed_cmd->payload_length); // clear payload
				delayed_cmd->payload[0] = glass_bits << 2;
				delayed_cmd->seal(); // seal the packet
				printPacket("Packet after editing: ", delayed_cmd);

				// Duration is in seconds, convert to ticks
				TickType_t ticks = pdMS_TO_TICKS(activation_delay * 1000UL);

				// Create one-shot timer, no need to track globally
				RTOS_timer_cry_state = xTimerCreate("Crystals State Timer",
													ticks,
													pdFALSE,
													(void*)delayed_cmd,
													vQueueDelayedPacket);
				if (RTOS_timer_cry_state != NULL)
				{
					xTimerStart(RTOS_timer_cry_state, 0);
				}
				else
				{
					vPortFree(delayed_cmd);
				}
			}
			else
			{
				// TODO: set crystals state
			}

			// Next 3 bytes: diode (3 bits), picture (3 bits) + acquisition delay (18 bits)
			uint32_t diode_and_delay =
				((uint32_t)cmd->payload[3] << 16) |
				((uint32_t)cmd->payload[4] << 8) |
				((uint32_t)cmd->payload[5]);

			uint8_t state_bits = (diode_and_delay >> 18) & 0b111111;
			uint32_t acquisition_delay = diode_and_delay & 0x3FFFF;
			uint32_t acquisition_delay_total = activation_delay + acquisition_delay;

			uint8_t diode = (state_bits >> 3) & 0b111;
			uint8_t picture = state_bits & 0b111;

			Serial.printf("Photodiode: %d, Picture: %d (acquisition in %d s after activation)\n", diode, picture, acquisition_delay_total);

			// TODO: always create photodiode acquisition packet and picture acquisition packet, with delay

			break;
		}

		// TODO: deprecate these commands when splitting code for PQ/GS
		case TER_ACK:
			Serial.println("TER: ACK");
			break;

		case TER_NACK:
			Serial.println("TER: NACK");
			break;

		default:
			Serial.println("Unknown TEC!");
			return PACKET_ERR_CMD_UNKNOWN;
	}

	return PACKET_ERR_NONE; // return no error
}

// Check if packet is a TEC to be executed
bool isTEC(uint8_t command)
{
	switch (command)
	{
		case TEC_OBC_REBOOT:
		case TEC_EXIT_STATE:
		case TEC_VAR_CHANGE:
		case TEC_SET_TIME:
		case TEC_EPS_REBOOT:
		case TEC_ADCS_REBOOT:
		case TEC_ADCS_TLE:
		case TEC_LORA_STATE:
		case TEC_LORA_CONFIG:
		case TEC_LORA_PING:
		case TEC_CRY_EXP:
			return true;
	
		default:
			return false; // not a TEC
	}
}

// Check if ACK is needed for the command
bool isACKNeeded(const Packet* packet)
{
	switch (packet->command)
	{
		case TEC_LORA_PING:
			return false; // ACK not needed for these commands
		
		default:
			return true; // ACK needed
	}
}

// Check if ACK is needed for the command before execution
bool isACKNeededBefore(const Packet* packet)
{
	switch (packet->command)
	{
		case TEC_OBC_REBOOT:
			return true; // ACK needs to be sent before rebooting
		default:
			return false; // ACK not needed
	}
}

// Send ACK packet to report valid command received
void sendACK(bool ecc, uint8_t TEC)
{
	// Create ACK packet
	Packet ack_packet;
	ack_packet.init(ecc, TER_ACK); // initialize packet

	ack_packet.setPayload(&TEC, 1); // set payload to executed TEC
	ack_packet.seal(); // seal packet with current time and MAC

	// Send ACK packet to queue
	xQueueSend(RTOS_queue_TX, &ack_packet, 0);
}

// Send NACK packet to report invalid command received
void sendNACK(bool ecc, uint8_t TEC, int8_t error)
{
	// Create NACK packet
	Packet nack_packet;
	nack_packet.init(ecc, TER_NACK); // initialize packet

	uint8_t payload[2] = {TEC, static_cast<uint8_t>(error)}; // payload contains TEC and error state
	nack_packet.setPayload(payload, sizeof(payload)); // set payload to executed TEC
	nack_packet.seal(); // seal packet with current time and MAC

	// Send NACK packet to queue
	xQueueSend(RTOS_queue_TX, &nack_packet, 0);
}


// ---------------------------------
// TIMER FUNCTIONS
// ---------------------------------

void vQueueDelayedPacket(TimerHandle_t xTimer)
{
	Packet* delayed_cmd = (Packet*)pvTimerGetTimerID(xTimer);
	if (delayed_cmd != nullptr)
	{
		BaseType_t sent = xQueueSend(RTOS_queue_cmd, delayed_cmd, 0);
		if (sent != pdPASS)
		{
			Serial.println("Error: RTOS_queue_cmd is full, delayed command not queued");
		}
		else
		{
			Serial.println("Delayed command queued successfully");
		}
		printPacket("Delayed packet: ", delayed_cmd); // print the delayed command for debugging
		vPortFree(delayed_cmd);
	}

	// Delete timer to clean up resources
	xTimerDelete(xTimer, 0);

	// Reset global timer handle using if-else instead of switch
	if (xTimer == RTOS_timer_lora_state)
	{
		RTOS_timer_lora_state = NULL;
	}
	else if (xTimer == RTOS_timer_cry_state)
	{
		RTOS_timer_cry_state = NULL;
	}
	else
	{
		Serial.println("Warning: Unknown timer deleted, not resetting global handle");
	}
}
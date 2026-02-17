/* Core/Src/app_logic.cpp */
#include "app.h"
#include <cstring> // <--- AGGIUNGI QUESTO!
#include <cstdio>  // <--- E ANCHE QUESTO PER PRINTF

// Global time offset variable (simulating Unix timestamp)
static uint32_t system_time_offset = 1735689600; // Default: 1 Jan 2025

// MAC configuration
const uint8_t SECRET_KEY[] = { 0xA1, 0xB2, 0xC3, 0xD4 }; // secret key for MAC generation

// ---------------------------------
// PRINT FUNCTIONS
// ---------------------------------

// Print boot message on serial
void printStartupMessage(const char* device)
{
	osDelay(500);
	printf("%s starting ... ", device);
	osDelay(100);
	// printf("ok\r\n");
}

// Print radio status on serial
void printRadioStatus(int16_t state, bool blocking)
{
	// No error reported
	if (state == RADIOLIB_ERR_NONE)
	{
		printf("ok\r\n");
	}
	else
	{
		printf("failed! --> Code: %d\r\n", state);
		if (blocking)
		{
			printf("Blocking program until user forces restart!\r\n");
			while (true)
			{
				osDelay(10000);
				printf("Program blocked, please restart ...\r\n");
			}
		}
	}
}

// Print received data on serial with optional prefix
void printData(const char* prefix, const uint8_t* data, uint8_t length)
{
    if (prefix != NULL)
    {
        printf("%s", prefix);
    }

	for (uint8_t i = 0; i < length; ++i)
	{
		printf("%02X ", data[i]);
	}
	printf("\r\n");
}

// Print packet on serial with optional prefix
void printPacket(const char* prefix, const Packet* packet)
{
	if (prefix != NULL) {
		printf("%s", prefix);
	}
	printf("%02X ", packet->station);
	printf("%02X ", packet->ecc ? RS_ON : RS_OFF);
	printf("%02X ", packet->command);
	printf("%02X ", packet->payload_length);
	// Print time_unix as 4 bytes
	for (int i = 0; i < 4; ++i) {
			printf("%02X ", (unsigned int)((packet->time_unix >> (8 * (3 - i))) & 0xFF));
		}
		// Print MAC as 4 bytes (CORRETTO CON CAST)
		for (int i = 0; i < 4; ++i) {
			printf("%02X ", (unsigned int)((packet->MAC >> (8 * (3 - i))) & 0xFF));
		}
		for (uint8_t i = 0; i < packet->payload_length; ++i) {
			printf("%02X ", packet->payload[i]);
		}
		printf("\r\n");
}


// ---------------------------------
// HELPER FUNCTIONS
// ---------------------------------

// Initialize system time to a specific UNIX timestamp, or default to Jan 1, 2025 if 0
void setUNIX(uint32_t unixTime)
{
    if (unixTime == 0)
    {
        unixTime = 1735689600; // Default Jan 1 2025
    }
    // Set offset based on current HAL Tick (ms)
    system_time_offset = unixTime - (HAL_GetTick() / 1000);
}

// Get current UNIX time
uint32_t getUNIX()
{
	return system_time_offset + (HAL_GetTick() / 1000);
}

// MAC function to generate a message authentication code (HMAC-SHA256)
int8_t makeMAC(const Packet* packet, uint32_t* out_mac)
{
    uint8_t buffer[PACKET_HEADER_LENGTH + PACKET_PAYLOAD_MAX]; // header + max payload
    uint8_t full_output[32]; // full HMAC-SHA256 output

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

    // === Compute HMAC-SHA256 using local library ===
    hmac_sha256(SECRET_KEY, sizeof(SECRET_KEY), buffer, PACKET_HEADER_LENGTH + packet->payload_length, full_output);

    // Take first 4 bytes
    *out_mac = ((uint32_t)full_output[0] << 24) |
               ((uint32_t)full_output[1] << 16) |
               ((uint32_t)full_output[2] << 8)  |
               ((uint32_t)full_output[3]);

    return PACKET_ERR_NONE;
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


// ---------------------------------
// RADIO FUNCTIONS
// ---------------------------------

// Notify COMMS task of a radio event (called when packet sent or received)
void packetEvent(void)
{
	// Define task to be notified
	BaseType_t higher_priority_task_woken = pdFALSE;

    if(COMMSTaskHandle != NULL) {
	    // Notify task
	    vTaskNotifyGiveFromISR(COMMSTaskHandle, &higher_priority_task_woken); // notify task
	    portYIELD_FROM_ISR(higher_priority_task_woken);
    }
}

// Start LoRa reception
void startReception(void)
{
	// Start listening
	int8_t rx_state = radio.startReceive();

	// Report listening status
	printf("[SX1268] Listening ...");
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
	if (mac_calculated == packet->MAC)
	{
		packet->mac_valid = true; // MAC valid
	}
	else
	{
		packet->mac_valid = false; // MAC invalid
	}

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
    // Note: Variable Length Array (VLA) or malloc should be used here if stack size is issue
    // For small blocks, stack is usually fine on STM32
	uint8_t codewords[num_blocks][RS_BLOCK_SIZE];

	// Encode each block with padding as needed
	for (uint8_t i = 0; i < num_blocks; ++i)
	{
		uint8_t block[DATA_BLOCK_SIZE];
        memset(block, RS_PADDING, DATA_BLOCK_SIZE); // Initialize with padding

		uint8_t remaining = data_len - i * DATA_BLOCK_SIZE;
		uint8_t copy_len = remaining >= DATA_BLOCK_SIZE ? DATA_BLOCK_SIZE : remaining;

		memcpy(block, data + i * DATA_BLOCK_SIZE, copy_len);
		encode_data(block, DATA_BLOCK_SIZE, codewords[i]);
	}

	// Interleave column-wise into a temporary buffer
    uint8_t* interleaved = (uint8_t*)malloc(num_blocks * RS_BLOCK_SIZE);
    if (!interleaved) return; // Malloc fail check

	for (uint8_t col = 0; col < RS_BLOCK_SIZE; ++col) {
		for (uint8_t row = 0; row < num_blocks; ++row) {
			interleaved[col * num_blocks + row] = codewords[row][col];
		}
	}

	// Copy interleaved codewords back to original data buffer
	memcpy(data, interleaved, num_blocks * RS_BLOCK_SIZE);
    free(interleaved);

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
		// decode_data takes encoded codeword and outputs decoded codeword (data + parity)
		decode_data(codewords[i], RS_BLOCK_SIZE);

		if (check_syndrome() != 0)
		{
			int8_t result = correct_errors_erasures(codewords[i], RS_BLOCK_SIZE, 0, NULL);
			if (result == 0)
			{
				printf("RS decode failed on block %d\r\n", i);
				error = PACKET_ERR_DECODE; // set error code for RS decode failure
			}
			else
			{
				printf("RS decode corrected errors in block %d\r\n", i);
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
	if (cmd == NULL)
	{
		return PACKET_ERR_CMD_POINTER;
	}

	// Execute (assumes payloads are correct!)
	switch (cmd->command)
	{
		case TEC_OBC_REBOOT:
			printf("TEC: OBC_REBOOT\r\n");
			HAL_NVIC_SystemReset();
			break;

		case TEC_EXIT_STATE:
			printf("TEC: EXIT_STATE\r\n");
			break;

		case TEC_VAR_CHANGE:
			printf("TEC: VAR_CHANGE\r\n");
			break;

		case TEC_SET_TIME:
		{
			printf("TEC: SET_TIME\r\n");

			uint32_t time_new = ((uint32_t)cmd->payload[0] << 24) | ((uint32_t)cmd->payload[1] << 16) | ((uint32_t)cmd->payload[2] << 8) | (uint32_t)cmd->payload[3];
			setUNIX(time_new);
			printf("Time set to: %u\r\n", (unsigned int)time_new);
			break;
		}

		case TEC_EPS_REBOOT:
			printf("TEC: EPS_REBOOT\r\n");
			break;

		case TEC_ADCS_REBOOT:
			printf("TEC: ADCS_REBOOT\r\n");
			break;

		case TEC_ADCS_TLE:
			printf("TEC: ADCS_TLE\r\n");
			break;

		case TEC_LORA_STATE:
		{
			printf("TEC: LORA_STATE\r\n");

			// Unpack LoRa state from payload
			uint8_t tx_state_new = cmd->payload[0];
			uint8_t val0 = (tx_state_new >> 0) & 0b1111;
			uint8_t val1 = (tx_state_new >> 4) & 0b1111;
			// printf("LoRa TX State new: %d, values: %d, %d\r\n", tx_state_new, val0, val1);

			// Check if both nibbles are the same
			if (val0 != val1)
			{
				printf("LoRa TX State values are not all the same!\r\n");
				return PACKET_ERR_CMD_PAYLOAD;
			}
			tx_state_new = val0;

			// Unpack duration from payload
			uint32_t duration = ((uint32_t)cmd->payload[1] << 16) | ((uint32_t)cmd->payload[2] << 8) | (uint32_t)cmd->payload[3];

			// Set LoRa TX state
			tx_state = tx_state_new;
			printf("LoRa TX State set to %d for %lu s\r\n", tx_state, (unsigned long)duration);

			// Cancel previous timer if still active
			deleteTimerIfExists(&RTOS_timer_lora_state, "LoRa State Timer");

			// Schedule delayed revert if needed
			if (duration > 0)
			{
				// Create packet with no delay to trigger the state change when timer expires
				createDelayedCommand(cmd,
							queueDelayedPacket,
							duration,
							editDelayedLoraState,
							&RTOS_timer_lora_state,
							"LoRa State Timer");
			}
			break;
		}

		case TEC_LORA_CONFIG:
		{
			printf("TEC: LORA_CONFIG\r\n");

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
			case 0: bw_khz = 62.5; break;
			case 1: bw_khz = 125.0; break;
			case 2: bw_khz = 250.0; break;
			case 3: bw_khz = 500.0; break;
			}
			uint8_t sf = ((b3 >> 3) & 0b111) + 6; // 3 bits, add 6
			uint8_t cr = (b3 & 0b111) + 5; // 3 bits, add 5

			// Byte 4
			uint8_t b4 = cmd->payload[4];
			int8_t power = ((b4 >> 3) & 0b11111) - 9; // 5 bits
			// uint8_t reserved = b4 & 0b111; // 3 bits

			// Byte 5: duration in seconds
			uint8_t duration = cmd->payload[5];

			// Validate parameters
			printf("LoRa Config: Freq: %u kHz, BW: %d.%d kHz, SF: %d, CR: %d, Power: %d dBm\r\n",
			       (unsigned int)freq_mhz,
			       (int)bw_khz,               // Parte intera (es. 62)
			       (int)(bw_khz * 10) % 10,   // Parte decimale (es. 5)
			       sf, cr, power);

			// Apply LoRa configuration
			int16_t state = radio.setFrequency(freq_mhz);
			state += radio.setBandwidth(bw_khz);
			state += radio.setSpreadingFactor(sf);
			state += radio.setCodingRate(cr);
			state += radio.setOutputPower(power);

			if (state != RADIOLIB_ERR_NONE)
			{
				return PACKET_ERR_CMD_PAYLOAD; // payload error
			}

			// Cancel previous timer if still active
			deleteTimerIfExists(&RTOS_timer_lora_config, "LoRa Config Timer");

			// Handle duration-based reset
			if (duration > 0)
			{
				createDelayedCommand(cmd,
							queueDelayedPacket,
							duration,
							editDelayedLoraConfig,
							&RTOS_timer_lora_config,
							"LoRa Config Timer");
			}
			break;
		}

		case TEC_LORA_PING:
		{
			printf("TEC: LORA_LINK\r\n");

			float RSSI = radio.getRSSI();
			float SNR = radio.getSNR();
			float freq_shift = radio.getFrequencyError();

			uint8_t payload[12]; // 12 bytes for RSSI, SNR, and frequency shift
			writeFloatToBytes(RSSI, payload);
			writeFloatToBytes(SNR, payload + 4);
			writeFloatToBytes(freq_shift, payload + 8);

			// Create packet with LoRa link status
			Packet lora_packet;
			lora_packet.init(rs_enabled, TER_LORA_PONG); // initialize packet
			lora_packet.setPayload(payload, sizeof(payload)); // set payload to LoRa link status
			lora_packet.seal(); // seal packet with current time and MAC

			// Send packet to queue
			xQueueSend(RTOS_queue_TX, &lora_packet, 0);
			break;
		}

		case TEC_CRY_EXP:
		{
			printf("TEC: CRY_EXP\r\n");
			// TODO: Adapt Cry Logic logic
			break;
		}

		case TER_ACK:
			printf("TER: ACK\r\n");
			break;

		case TER_NACK:
			printf("TER: NACK\r\n");
			break;

		default:
			printf("Unknown TEC!\r\n");
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
		case TEC_OBC_REBOOT: // before rebooting OBC
		case TEC_LORA_CONFIG: // before changing LoRa config
			return true;
		default:
			return false; // ACK not needed early
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

// Timer callback to queue delayed command packet
void queueDelayedPacket(TimerHandle_t xTimer)
{
	TimerPayload* payload = (TimerPayload*)pvTimerGetTimerID(xTimer);
	if (payload != NULL)
	{
		Packet* delayed_cmd = payload->packet;
		if (xQueueSend(RTOS_queue_cmd, delayed_cmd, 0) != pdPASS)
		{
			printf("Error: RTOS_queue_cmd is full, delayed command not queued\r\n");
		}
		else
		{
			printf("Delayed command queued successfully\r\n");
		}

		printPacket("Delayed packet: ", delayed_cmd);

		vPortFree(delayed_cmd);

		// Clear the timer handle
		if (payload->global_handle != NULL)
		{
			*(payload->global_handle) = NULL;
		}

		vPortFree(payload);
	}

	xTimerDelete(xTimer, 0);
}

// Create a one-shot timer to execute a command after a delay, with optional packet editing
void createDelayedCommand(const Packet* cmd,
									void (*callback)(TimerHandle_t),
									uint32_t delay_seconds,
									void (*editDelayedCommand)(Packet*),
									TimerHandle_t* global_timer_handle,
									const char* timer_name)
{
	// Allocate a copy of the packet
    Packet* delayed_cmd = (Packet*)pvPortMalloc(sizeof(Packet));
    if (delayed_cmd == NULL)
    {
        printf("Failed to allocate memory for delayed packet\r\n");
        return;
    }

    memcpy(delayed_cmd, cmd, sizeof(Packet));
    editDelayedCommand(delayed_cmd); // Call the specific packet editor for this TEC command
    printPacket("Packet after editing: ", delayed_cmd);

    TimerPayload* payload = (TimerPayload*)pvPortMalloc(sizeof(TimerPayload));
    if (payload == NULL)
    {
        printf("Failed to allocate timer payload\r\n");
        vPortFree(delayed_cmd);
        return;
    }

    payload->packet = delayed_cmd;
    payload->global_handle = global_timer_handle;

    TickType_t ticks = pdMS_TO_TICKS(delay_seconds * 1000UL);

    *global_timer_handle = xTimerCreate(timer_name,
                                        ticks,
                                        pdFALSE,
                                        (void*)payload,
                                        callback);

    if (*global_timer_handle != NULL)
    {
        xTimerStart(*global_timer_handle, 0);
    }
    else
    {
        vPortFree(delayed_cmd);
        vPortFree(payload);
        printf("Failed to create timer\r\n");
    }
}

// Delete timer if active
void deleteTimerIfExists(TimerHandle_t* timer_handle, const char* timer_name)
{
	if (*timer_handle != NULL)
	{
		xTimerStop(*timer_handle, 0);

		TimerPayload* payload = (TimerPayload*)pvTimerGetTimerID(*timer_handle);
		if (payload != NULL)
		{
			if (payload->packet != NULL)
			{
				vPortFree(payload->packet);
			}
			vPortFree(payload);
		}

		xTimerDelete(*timer_handle, 0);
		*timer_handle = NULL;

		printf("Cancelled previous %s timer\r\n", timer_name);
	}
}

// Edit function to set delay to 0 (generic 4-byte delay at payload[0..3])
void editDelayedGeneric4(Packet* packet)
{
	if (packet != NULL)
	{
		// Set the first 4 bytes of the payload to 0
		memset(packet->payload, 0, 4);

		// Re-seal the packet to update MAC
		packet->seal();
	}
}

// Edit function to set delay to 0 (generic 1-byte delay at payload[0])
void editDelayedGeneric1(Packet* packet)
{
	if (packet != NULL)
	{
		// Set the first byte of the payload to 0
		packet->payload[0] = 0;

		// Re-seal the packet to update MAC
		packet->seal();
	}
}

// Edit function to set LoRa default state with 0 delay (3-byte duration at payload[1..3])
void editDelayedLoraState(Packet* packet)
{
	 if (packet == NULL) return;

	// Set default state and delay to 0 in payload
	packet->payload[0] = ((TX_ON & 0b1111) << 4) | (TX_ON & 0b1111);
	packet->payload[1] = 0x00;
	packet->payload[2] = 0x00;
	packet->payload[3] = 0x00;

	packet->seal(); // seal the packet
}

// Edit function to set LoRa default config with 0 duration (1-byte duration at payload[5])
void editDelayedLoraConfig(Packet* packet)
{
	if (packet == NULL) return;

	uint32_t freq_khz = F * 1000;
	uint32_t bw_hz = BW * 1000;
	uint8_t bandwidth_code = 1; // default to 125 kHz
	switch (bw_hz)
	{
		case 62500: bandwidth_code = 0; break;
		case 125000: bandwidth_code = 1; break;
		case 250000: bandwidth_code = 2; break;
		case 500000: bandwidth_code = 3; break;
	}

	uint8_t reserved = 0b000; // reserved bits set to 0
	packet->payload[0] = (freq_khz >> 16) & 0xFF;
	packet->payload[1] = (freq_khz >> 8) & 0xFF;
	packet->payload[2] = freq_khz & 0xFF;
	packet->payload[3] = (bandwidth_code << 6) | ((SF - 6) << 3) | (CR - 5); // 2 bits BW, 3 bits SF, 3 bits CR
	packet->payload[4] = ((OUTPUT_POWER + 9) << 3) | reserved; // 5 bits power, 3 bits reserved
	packet->payload[5] = 0x00; // duration in seconds

	packet->seal(); // seal the packet
}

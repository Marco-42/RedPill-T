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
				Serial.println("Program blocked, please restart ...");
			}
		}
	}
}

// Print received packet on serial with optional prefix
void printPacket(const char* prefix, const uint8_t* packet, uint8_t length)
{
    if (prefix != nullptr)
    {
        Serial.print(prefix);
    }
	
	for (uint8_t i = 0; i < length; ++i)
	{
		Serial.printf("%02X ", packet[i]);
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
uint32_t makeMAC(uint32_t timestamp, uint32_t secret_key)
{
    uint32_t x = timestamp ^ secret_key;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = (x >> 16) ^ x;
    return x;
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
					Serial.printf("Processing %d packet(s)...\n", packet_count);
					
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
	Serial.print("Listening ... ");
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
void Packet::setPayload(const uint8_t* data, uint8_t length)
{
	if (length > PACKET_PAYLOAD_MAX)
	{
		length = PACKET_PAYLOAD_MAX;  // Prevent overflow
	}

	memcpy(payload, data, length);
	payload_length = length;
}

// Seal packet by calculating MAC and setting time
void Packet::seal()
{
	time_unix = getUNIX();
	MAC = makeMAC(time_unix, SECRET_KEY);
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
	if (data[1] == BYTE_RS_ON)
	{
		packet->ecc = true;
	}
	else if (data[1] == BYTE_RS_OFF)
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
			if (data[length - 1] == BYTE_RS_PADDING) // Check if last byte is padding
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

	// Byte 8–11: UNIX time
	packet->time_unix = ((uint32_t)data[8] << 24) | ((uint32_t)data[9] << 16) | ((uint32_t)data[10] << 8) | (uint32_t)data[11];

	// Byte 4–7: MAC
	packet->MAC = ((uint32_t)data[4] << 24) | ((uint32_t)data[5] << 16) | ((uint32_t)data[6] << 8) | (uint32_t)data[7];

	uint32_t expected_MAC = makeMAC(packet->time_unix, SECRET_KEY); // SECRET_KEY is a predefined constant
	if (packet->MAC != expected_MAC)
	{
		packet->state = PACKET_ERR_MAC; // MAC error
		return;
	}

	// Parse payload
	packet->setPayload(data + PACKET_HEADER_LENGTH, packet->payload_length);
	// for (uint8_t i = 0; i < packet->payload_length; i++)
	// {
	// 	packet->payload[i] = data[PACKET_HEADER_LENGTH + i];
	// }

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
		data[1] = BYTE_RS_ON; // RS encoding on
	}
	else
	{
		data[1] = BYTE_RS_OFF; // RS encoding off
	}

	// Byte 2: command
	data[2] = packet->command;

	// Byte 3: payload length
	data[3] = packet->payload_length;

	// Byte 4–7: MAC
	data[4] = (packet->MAC >> 24) & 0xFF;
	data[5] = (packet->MAC >> 16) & 0xFF;
	data[6] = (packet->MAC >> 8) & 0xFF;
	data[7] = packet->MAC & 0xFF;

	// Byte 8–11: UNIX time
	data[8] = (packet->time_unix >> 24) & 0xFF;
	data[9] = (packet->time_unix >> 16) & 0xFF;
	data[10] = (packet->time_unix >> 8) & 0xFF;
	data[11] = packet->time_unix & 0xFF;

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
	if ((length % RS_BLOCK_SIZE == 0) && (data[1] != BYTE_RS_OFF))
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
		uint8_t block[DATA_BLOCK_SIZE] = {BYTE_RS_PADDING}; // Initialize with padding byte
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
			if (result != 0)
			{
				Serial.printf("RS decode failed on block %d\n", i);
				error = PACKET_ERR_DECODE; // set error code for RS decode failure
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
bool executeTEC(const Packet* cmd)
{
	if (cmd == nullptr)
	{
		return false;
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

		case TEC_LORA_LINK:
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

		// TODO: deprecate these commands when splitting code for PQ/GS
		case TER_ACK:
			Serial.println("TER: ACK");
			break;

		case TER_NACK:
			Serial.println("TER: NACK");
			break;

		default:
			Serial.println("Unknown TEC!");
			return false;
	}

	return true;
}

// Check if packet is a TEC to be executed
bool isPacketTEC(const Packet* packet)
{
	switch (packet->command)
	{
		case TEC_OBC_REBOOT:
		case TEC_EXIT_STATE:
		case TEC_VAR_CHANGE:
		case TEC_SET_TIME:
		case TEC_EPS_REBOOT:
		case TEC_ADCS_REBOOT:
		case TEC_ADCS_TLE:
		case TEC_LORA_LINK:
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
		case TEC_LORA_LINK:
			return false; // ACK not needed for these commands
		
		default:
			return true; // ACK needed
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

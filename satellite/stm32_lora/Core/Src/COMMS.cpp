/*
 * COMMS.cpp
 *
 * Created on: Feb 17, 2026
 * Author: J2050 TT&C
 * Description: Implementation of the RedPill Telecommunication Subsystem.
 * Handles RadioLib integration, Packet Encoding/Decoding,
 * ECC, and the Main Communications State Machine.
 */

#include "COMMS.h"
#include <cstring>
#include <cstdio>

// =============================================================================
// HARDWARE & GLOBAL OBJECTS
// =============================================================================

// External Handles (Defined in main.c)
extern SPI_HandleTypeDef hspi1;
extern osThreadId_t OBCTaskHandle;

// RadioLib Hardware Abstraction & Module
STM32Hal radioHal(&hspi1);
// SX1268(Module(hal, CS, DIO1, RST, BUSY))
SX1268 radio = new Module(&radioHal, RLIB_NSS, RLIB_DIO1, RLIB_RESET, RLIB_BUSY);

// Hardware state tracking
volatile HwRadioMode current_radio_mode = RADIO_MODE_STANDBY;

// RTOS Objects
TaskHandle_t COMMS_task_handle = NULL;
QueueHandle_t COMMS_queue_events = NULL;

// Timers
TimerHandle_t COMMS_timer_lora_state = NULL;
TimerHandle_t COMMS_timer_lora_config = NULL;
TimerHandle_t COMMS_timer_cry_state = NULL;

// System State Variables
uint8_t tx_state = TX_ON;
uint8_t cry_state = CRY_OFF;
bool rs_enabled = false;

// Time & Security
static uint32_t system_time_offset = 1735689600; // Default: Jan 1 2025
const uint8_t SECRET_KEY[] = { 0xA1, 0xB2, 0xC3, 0xD4 }; // HMAC Secret Key

// =============================================================================
// INTERRUPT SERVICE ROUTINES
// =============================================================================

// Callback for GPIO External Interrupts
extern "C" void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	if (GPIO_Pin == LORA_DIO1_Pin)
	{
		printf("ISR!\r\n");
		if(COMMS_queue_events != NULL)
		{
			BaseType_t xHigherPriorityTaskWoken = pdFALSE;
			CommsEvent irq_event;
			
			// Contextually determine the event type based on hardware state
			if (current_radio_mode == RADIO_MODE_TX)
			{
				irq_event.type = EVENT_LORA_TX_DONE;
			}
			else if (current_radio_mode == RADIO_MODE_RX)
			{
				irq_event.type = EVENT_LORA_RX_DONE;
			}
			else
			{
				return; // Ignore spurious interrupts
			}

			xQueueSendToFrontFromISR(COMMS_queue_events, &irq_event, &xHigherPriorityTaskWoken);
			portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
		}
	}
}


// =============================================================================
// DEBUG PRINT
// =============================================================================

// Print boot message on serial
void printStartupMessage(const char* device)
{
	osDelay(500);
	printf("%s starting ... ", device);
	osDelay(100);
}

// Print radio status on serial
void printRadioStatus(int16_t state, bool blocking)
{
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
				printf("Program blocked, please restart ...");
			}
		}
	}
}

// Print data byte array with optional prefix on serial
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


// =============================================================================
// UTILITIES
// =============================================================================


// Set UNIX time from external source or default value
void setUNIX(uint32_t unixTime)
{
	if (unixTime == 0)
	{
		unixTime = 1735689600;
	}
	// Calculate offset based on current system tick
	system_time_offset = unixTime - (HAL_GetTick() / 1000);
}

// Return current UNIX time
uint32_t getUNIX()
{
	return system_time_offset + (HAL_GetTick() / 1000);
}

// Write float into 4 byte array (big endian)
void writeFloatToBytes(float value, uint8_t* buffer)
{
	uint32_t value_int;
	memcpy(&value_int, &value, sizeof(float));
	buffer[0] = (value_int >> 24) & 0xFF;
	buffer[1] = (value_int >> 16) & 0xFF;
	buffer[2] = (value_int >> 8) & 0xFF;
	buffer[3] = value_int & 0xFF;
}


// =============================================================================
// RADIO HARDWARE CONTROL
// =============================================================================

// Start radio with correct pin assignment
void initRadioHardware()
{
	printf("[COMMS] Binding Radio Hardware...\n");

	// Map virtual pins to real CubeMX pins
	radioHal.addPin(RLIB_NSS,   LORA_CS_GPIO_Port,    LORA_CS_Pin);
	radioHal.addPin(RLIB_RESET, LORA_RST_GPIO_Port,   LORA_RST_Pin);
	radioHal.addPin(RLIB_DIO1,  LORA_DIO1_GPIO_Port,  LORA_DIO1_Pin);
	radioHal.addPin(RLIB_BUSY,  LORA_BUSY_GPIO_Port,  LORA_BUSY_Pin);

	// Ensure CS is high (inactive) before init
	HAL_GPIO_WritePin(LORA_CS_GPIO_Port, LORA_CS_Pin, GPIO_PIN_SET);

	printf("[COMMS] RadioLib Begin...\n");
	int16_t state = radio.begin(LORA_FREQ, LORA_BW, LORA_SF, LORA_CR, LORA_SYNC, LORA_PWR, LORA_PREAMBLE, LORA_TCXO_V, LORA_USE_LDO);

	printRadioStatus(state, false);
}

// Start radio reception
void startReception()
{
	printf("[SX1268] Listening ...");
	int16_t rx_state = radio.startReceive();
	if (rx_state == RADIOLIB_ERR_NONE)
    {
        current_radio_mode = RADIO_MODE_RX; // track hardware state
    }
	printRadioStatus(rx_state, false);
}

// Start radio transmission
void startTransmission(uint8_t* tx_packet, uint8_t packet_size)
{
	printData("Transmitting: ", tx_packet, packet_size);
	int16_t tx_state = radio.startTransmit(tx_packet, packet_size);
	if (tx_state == RADIOLIB_ERR_NONE)
    {
        current_radio_mode = RADIO_MODE_TX; // Track hardware state
    }
	printRadioStatus(tx_state, false);
}


// =============================================================================
// PACKET CLASS HANDLING
// =============================================================================

// Initialize packet with default values
void Packet::init(bool rs_enabled, uint8_t cmd)
{
	state = PACKET_ERR_NONE;
	station = MISSION_ID;
	command = cmd;
	// Override ECC settings for selected packets
	ecc = rs_enabled;
	switch (command)
	{
		case TER_BEACON:
			ecc = false;
	}
	time_unix = 0;
	MAC = 0;
	payload_length = 0;
	memset(payload, 0, PACKET_PAYLOAD_MAX);
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

// Seal packet by setting time and calculating MAC
int8_t Packet::seal()
{
	time_unix = getUNIX();
	uint32_t mac_calculated;
	if (makeMAC(this, &mac_calculated) != PACKET_ERR_NONE)
	{
		return PACKET_ERR_MAC;
	}
	MAC = mac_calculated;
	return PACKET_ERR_NONE;
}

// Convert byte array to packet with validation
void dataToPacket(const uint8_t* data, uint8_t length, Packet* packet)
{
	if (length < PACKET_HEADER_LENGTH || length > PACKET_SIZE_MAX)
	{
		packet->state = PACKET_ERR_LENGTH;
		return;
	}

	packet->station = data[0];

	// Check RS Flag
	switch (data[1])
	{
		case RS_ON:
			packet->ecc = true;
			break;
		case RS_OFF:
			packet->ecc = false;
			break;
		default:
			packet->state = PACKET_ERR_RS;
			return;
	}

	// Check command
	packet->command = data[2];
	if (!isTEC(packet->command))
	{
		packet->state = PACKET_ERR_TEC_UNKNOWN;
		return;
	}

	packet->payload_length = data[3];

	// Deserialize time and MAC
	packet->time_unix = ((uint32_t)data[4] << 24) | ((uint32_t)data[5] << 16) | ((uint32_t)data[6] << 8) | (uint32_t)data[7];
	packet->MAC = ((uint32_t)data[8] << 24) | ((uint32_t)data[9] << 16) | ((uint32_t)data[10] << 8) | (uint32_t)data[11];

	packet->setPayload(data + PACKET_HEADER_LENGTH, packet->payload_length);

	// Verify MAC
	uint32_t mac_calc;
	if (makeMAC(packet, &mac_calc) != PACKET_ERR_NONE)
	{
		packet->state = PACKET_ERR_MAC;
		return;
	}
	// If MAC is invalid do not raise PACKET_ERR_MAC but set flag
	// This allows processing packets that do not require MAC (radioamateur payloads)
	packet->mac_valid = (mac_calc == packet->MAC);
	packet->state = PACKET_ERR_NONE;
}

// Convert packet to byte array and return length
uint8_t packetToData(const Packet* packet, uint8_t* data)
{
	data[0] = packet->station;
	data[1] = packet->ecc ? RS_ON : RS_OFF;
	data[2] = packet->command;
	data[3] = packet->payload_length;
	data[4] = (packet->time_unix >> 24) & 0xFF;
	data[5] = (packet->time_unix >> 16) & 0xFF;
	data[6] = (packet->time_unix >> 8) & 0xFF;
	data[7] = packet->time_unix & 0xFF;
	data[8] = (packet->MAC >> 24) & 0xFF;
	data[9] = (packet->MAC >> 16) & 0xFF;
	data[10] = (packet->MAC >> 8) & 0xFF;
	data[11] = packet->MAC & 0xFF;
	memcpy(data + PACKET_HEADER_LENGTH, packet->payload, packet->payload_length);
	return PACKET_HEADER_LENGTH + packet->payload_length;
}

// =============================================================================
// DATA ENCRYPTION AND ECC
// =============================================================================

// Generate HMAC-SHA256 for packet authentication
int8_t makeMAC(const Packet* packet, uint32_t* out_mac)
{
	uint8_t buffer[PACKET_HEADER_LENGTH + PACKET_PAYLOAD_MAX];
	uint8_t full_output[32];

	// Construct header for hashing
	buffer[0] = packet->station;
	buffer[1] = packet->ecc ? RS_ON : RS_OFF;
	buffer[2] = packet->command;
	buffer[3] = packet->payload_length;
	buffer[4] = (packet->time_unix >> 24) & 0xFF;
	buffer[5] = (packet->time_unix >> 16) & 0xFF;
	buffer[6] = (packet->time_unix >> 8)  & 0xFF;
	buffer[7] = (packet->time_unix)       & 0xFF;
	memset(&buffer[8], 0, 4); // zero out MAC field for calculation
	memcpy(buffer + PACKET_HEADER_LENGTH, packet->payload, packet->payload_length);

	// Compute hash
	hmac_sha256(SECRET_KEY, sizeof(SECRET_KEY), buffer, PACKET_HEADER_LENGTH + packet->payload_length, full_output);

	// Truncate to 4 bytes
	*out_mac = ((uint32_t)full_output[0] << 24) |
			   ((uint32_t)full_output[1] << 16) |
			   ((uint32_t)full_output[2] << 8) |
			   ((uint32_t)full_output[3]);
	return PACKET_ERR_NONE;
}

// Check if RS ECC is enabled in the packet
bool isDataECCEnabled(const uint8_t* data, uint8_t length)
{
	// If length aligns with RS blocks and RS flag is not off
	// we can infer ECC is enabled
	return ((length % RS_BLOCK_SIZE == 0) && (data[1] != RS_OFF));
}

// Encode data using RS ECC and interleave the output, changing data length
void encodeECC(uint8_t* data, uint8_t& data_len)
{
	uint8_t num_blocks = (data_len + DATA_BLOCK_SIZE - 1) / DATA_BLOCK_SIZE;
	uint8_t codewords[num_blocks][RS_BLOCK_SIZE];

	// Encode blocks
	for (uint8_t i = 0; i < num_blocks; ++i)
	{
		uint8_t block[DATA_BLOCK_SIZE];
		memset(block, RS_PADDING, DATA_BLOCK_SIZE);
		uint8_t remaining = data_len - i * DATA_BLOCK_SIZE;
		memcpy(block, data + i * DATA_BLOCK_SIZE, (remaining >= DATA_BLOCK_SIZE ? DATA_BLOCK_SIZE : remaining));
		encode_data(block, DATA_BLOCK_SIZE, codewords[i]);
	}

	// Interleave column-wise
	uint8_t* interleaved = (uint8_t*)malloc(num_blocks * RS_BLOCK_SIZE);
	if (!interleaved) return;

	for (uint8_t col = 0; col < RS_BLOCK_SIZE; ++col)
	{
		for (uint8_t row = 0; row < num_blocks; ++row)
		{
			interleaved[col * num_blocks + row] = codewords[row][col];
		}
	}
	memcpy(data, interleaved, num_blocks * RS_BLOCK_SIZE);
	free(interleaved);
	data_len = num_blocks * RS_BLOCK_SIZE;
}

// Decode data by deinterleaving and correcting errors using RS ECC
int8_t decodeECC(uint8_t* data, uint8_t& data_len)
{
    int8_t error = PACKET_ERR_NONE;
    uint8_t num_blocks = data_len / RS_BLOCK_SIZE;
    uint8_t codewords[num_blocks][RS_BLOCK_SIZE];

    // Deinterleave
    for (uint8_t col = 0; col < RS_BLOCK_SIZE; ++col)
    {
        for (uint8_t row = 0; row < num_blocks; ++row)
        {
            codewords[row][col] = data[col * num_blocks + row];
        }
    }

    // Decode blocks
    uint8_t write_pos = 0;
    for (uint8_t i = 0; i < num_blocks; ++i)
    {
        decode_data(codewords[i], RS_BLOCK_SIZE);
        if (check_syndrome() != 0)
        {
            if (correct_errors_erasures(codewords[i], RS_BLOCK_SIZE, 0, NULL) == 0)
            {
                printf("RS decode failed on block %d\r\n", i);
                error = PACKET_ERR_DECODE;
            }
            else
            {
                printf("RS decode corrected block %d\r\n", i);
            }
        }
        memcpy(data + write_pos, codewords[i], DATA_BLOCK_SIZE);
        write_pos += DATA_BLOCK_SIZE;
    }
    data_len = num_blocks * DATA_BLOCK_SIZE;
    return error;
}

// =============================================================================
// ACK MANAGEMENT
// =============================================================================

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
    // For dangerous commands (reboot), ACK before execution
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
	Packet ack;
	ack.init(ecc, TER_ACK);
	ack.setPayload(&TEC, 1);
	ack.seal();
    scheduleAction(EVENT_TX, &ack, true);
}

// Send NACK packet to report invalid command received
void sendNACK(bool ecc, uint8_t TEC, int8_t error)
{
    Packet nack;
    nack.init(ecc, TER_NACK);
    uint8_t pl[2] = {TEC, (uint8_t)error};
    nack.setPayload(pl, 2);
    nack.seal();
    scheduleAction(EVENT_TX, &nack, true);
}

// =============================================================================
// TELECOMMAND PROCESSING AND TIMERS
// =============================================================================

// Add action to COMMS queue
BaseType_t scheduleAction(CommsEventType action, Packet* payload, bool priority)
{
	// Reserve slot strictly for TX operations (ACKs/NACKs)
	if (action == EVENT_CMD)
	{
		if (uxQueueSpacesAvailable(COMMS_queue_events) <= COMMS_QUEUE_RESERVED)
		{
			// Reject the command so we don't consume the reserved slots
			return pdFAIL;
		}
	}

    CommsEvent event;
    event.type = action;
    event.packet = *payload;

    if (priority)
    {
    	return xQueueSendToFront(COMMS_queue_events, &event, 0);
    }
    else
    {
    	return xQueueSend(COMMS_queue_events, &event, 0);
    }
}

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

// Execute telecommand logic
int8_t executeTEC(const Packet* cmd)
{
    if (!cmd)
	{
    	return PACKET_ERR_CMD_POINTER;
	}

    switch (cmd->command)
    {
        case TEC_OBC_REBOOT:
        {
            printf("TEC: OBC REBOOT\r\n");
            HAL_NVIC_SystemReset();
            break;
        }

		case TEC_EXIT_STATE:
        {
			printf("TEC: EXIT_STATE\r\n");
			break;
        }

		case TEC_VAR_CHANGE:
        {
			printf("TEC: VAR_CHANGE\r\n");
			break;
        }

		case TEC_SET_TIME:
		{
			printf("TEC: SET_TIME\r\n");
			uint32_t time_new = ((uint32_t)cmd->payload[0] << 24) | ((uint32_t)cmd->payload[1] << 16) | ((uint32_t)cmd->payload[2] << 8) | (uint32_t)cmd->payload[3];
			setUNIX(time_new);
			printf("Time set to: %lu\r\n", time_new);
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

        case TEC_LORA_PING:
        {
            printf("TEC: LORA_PING\r\n");
            float rssi = radio.getRSSI();
            float snr = radio.getSNR();
            float err = radio.getFrequencyError();
            uint8_t pl[12];
            writeFloatToBytes(rssi, pl);
            writeFloatToBytes(snr, pl+4);
            writeFloatToBytes(err, pl+8);

            Packet pong;
            pong.init(rs_enabled, TER_LORA_PONG);
            pong.setPayload(pl, 12);
            pong.seal();
            scheduleAction(EVENT_TX, &pong, false);
            break;
        }

        case TEC_LORA_STATE:
        {
             printf("TEC: LORA_STATE\r\n");
             uint8_t newState = cmd->payload[0] & 0x0F;
             // Check both nibbles
//			 if (newState != (cmd->payload[0] & 0xF0))
//			 {
//				 return PACKET_ERR_CMD_PAYLOAD; // payload error
//			 }
             uint32_t duration = ((uint32_t)cmd->payload[1] << 16) | ((uint32_t)cmd->payload[2] << 8) | cmd->payload[3];

             tx_state = newState;
             printf("Set TX State: %d for %lu s\r\n", tx_state, (unsigned long)duration);

             deleteTimerIfExists(&COMMS_timer_lora_state, "LoRa State");
             if (duration > 0)
             {
                 createDelayedCommand(cmd, queueDelayedPacket, duration, editDelayedLoraState, &COMMS_timer_lora_state, "LoRa State");
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
			printf("LoRa Config: Freq: %lu kHz, BW: %.1f kHz, SF: %d, CR: %d, Power: %d dBm\r\n", freq_mhz, bw_khz, sf, cr, power);
			if (freq_mhz < 400 || freq_mhz > 500 || bw_khz < 62.5 || bw_khz > 500 ||
				sf < 6 || sf > 12 || cr < 5 || cr > 8 || power < -9 || power > 22) // TODO check these limits
			{
				printf("Invalid LoRa configuration parameters!\r\n");
				return PACKET_ERR_CMD_PAYLOAD; // payload error
			}

			// Apply LoRa configuration
			int16_t state = radio.setFrequency(freq_mhz);
			state = state + radio.setBandwidth(bw_khz);
			state = state + radio.setSpreadingFactor(sf);
			state = state + radio.setCodingRate(cr);
			state = state + radio.setOutputPower(power);
			if (state != RADIOLIB_ERR_NONE)
			{
				return PACKET_ERR_CMD_PAYLOAD; // payload error
			}

			// Cancel previous timer if still active
			deleteTimerIfExists(&COMMS_timer_lora_config, "LoRa Config");
			if (duration > 0)
			{
			 createDelayedCommand(cmd, queueDelayedPacket, duration, editDelayedLoraConfig, &COMMS_timer_lora_config, "LoRa Config");
			}
			break;
		}

		default:
			printf("Unknown command: %02X\r\n", cmd->command);
			return PACKET_ERR_TEC_UNKNOWN;
	}
	return PACKET_ERR_NONE;
}

// Delete timer if active
void deleteTimerIfExists(TimerHandle_t* timer_handle, const char* timer_name)
{
	if (*timer_handle != NULL)
	{
		xTimerStop(*timer_handle, 0);
		TimerPayload* payload = (TimerPayload*)pvTimerGetTimerID(*timer_handle);
		if (payload)
		{
			if (payload->packet)
			{
				vPortFree(payload->packet);
			}
			vPortFree(payload);
		}
		xTimerDelete(*timer_handle, 0);
		*timer_handle = NULL;
		printf("Cancelled timer: %s\r\n", timer_name);
	}
}

// Timer callback to queue delayed command packet
void queueDelayedPacket(TimerHandle_t xTimer)
{
    TimerPayload* payload = (TimerPayload*)pvTimerGetTimerID(xTimer);
    if (payload)
    {
        if (scheduleAction(EVENT_CMD, payload->packet, false) == pdPASS)
        {
            printf("Delayed command queued\r\n");
        }
        else
        {
        	printf("Error: RTOS_queue_cmd is full, delayed command not queued\r\n");
        }
        // Clear timer handle
        if (payload->global_handle)
		{
        	*(payload->global_handle) = NULL;
		}
        vPortFree(payload->packet);
        vPortFree(payload);
    }
    xTimerDelete(xTimer, 0);
}

// Create a one-shot timer to execute a command after a delay, with optional packet editing
void createDelayedCommand(const Packet* cmd, void (*callback)(TimerHandle_t), uint32_t delay_seconds, void (*editDelayedCommand)(Packet*), TimerHandle_t* global_timer_handle, const char* timer_name)
{
	Packet* delayed_cmd = (Packet*)pvPortMalloc(sizeof(Packet));
	if (!delayed_cmd)
	{
		printf("Failed to allocate memory for delayed packet\r\n");
		return;
	}
	memcpy(delayed_cmd, cmd, sizeof(Packet));
	editDelayedCommand(delayed_cmd);

	TimerPayload* payload = (TimerPayload*)pvPortMalloc(sizeof(TimerPayload));
	if (!payload)
	{
		vPortFree(delayed_cmd);
		return;
	}
	payload->packet = delayed_cmd;
	payload->global_handle = global_timer_handle;

	*global_timer_handle = xTimerCreate(timer_name, pdMS_TO_TICKS(delay_seconds * 1000UL), pdFALSE, (void*)payload, callback);
	if (*global_timer_handle)
	{
		xTimerStart(*global_timer_handle, 0);
	}
	else
	{
		vPortFree(delayed_cmd);
		vPortFree(payload);
	}
}

// Edit function to set delay to 0 (generic 4-byte delay at payload[0..3])
void editDelayedGeneric4(Packet* packet)
{
	if (!packet)
	{
		return;
	}
	else
	{
		memset(packet->payload, 0, 4);
		packet->seal();
	}
}

// Edit function to set delay to 0 (generic 1-byte delay at payload[0])
void editDelayedGeneric1(Packet* packet)
{
	if (!packet)
	{
		return;
	}
	else
	{
		packet->payload[0] = 0;
		packet->seal();
	}
}

// Edit function to set LoRa default state with 0 delay (3-byte duration at payload[1..3])
void editDelayedLoraState(Packet* packet)
{
	if (!packet)
	{
		return;
	}
	// Set to default state (TX_ON)
	packet->payload[0] = ((TX_ON & 0b1111) << 4) | (TX_ON & 0b1111);
	memset(&packet->payload[1], 0, 3);
	packet->seal();
}

// Edit function to set LoRa default config with 0 duration (1-byte duration at payload[5])
void editDelayedLoraConfig(Packet* packet)
{
	if (!packet)
	{
		return;
	}
	// Revert to default config (436 MHz, etc)
	uint32_t freq = (uint32_t)(LORA_FREQ * 1000);
	packet->payload[0] = (freq >> 16) & 0xFF;
	packet->payload[1] = (freq >> 8) & 0xFF;
	packet->payload[2] = freq & 0xFF;
	// 125kHz (1), SF10, CR 4/5
	packet->payload[3] = (1 << 6) | ((LORA_SF - 6) << 3) | (LORA_CR - 5);
	packet->payload[4] = ((LORA_PWR + 9) << 3);
	packet->payload[5] = 0;
	packet->seal();
}

// =============================================================================
// COMMS STATE MACHINE TASK
// =============================================================================

void COMMS_stateMachine(void *parameter) {
    printStartupMessage("COMMS");

    // Initialize Hardware
    initRadioHardware();

    // Initialize Queues
//    if (RTOS_queue_TX == NULL) RTOS_queue_TX = xQueueCreate(TX_QUEUE_SIZE, sizeof(Packet));
//    if (RTOS_queue_cmd == NULL) RTOS_queue_cmd = xQueueCreate(CMD_QUEUE_SIZE, sizeof(Packet));
    if (COMMS_queue_events == NULL) COMMS_queue_events = xQueueCreate(COMMS_QUEUE_SIZE, sizeof(CommsEvent));

    // Initialize Logic
    initialize_ecc();
    uint8_t COMMS_state = COMMS_IDLE;

    // Only way to end state machine is killing COMMS thread (by the OBC)
    for(;;)
    {
    	switch (COMMS_state)
    	{
    		// IDLE STATE: wait for events and route them
    		// case COMMS_IDLE:
    		// {
    		// 	printf("COMMS_IDLE: Waiting for events ...\r\n");
    		// 	CommsEvent event;
    		// 	startReception();

    		// 	if (xQueuePeek(COMMS_queue_events, &event, portMAX_DELAY) == pdTRUE)
    		// 	{
    		// 		switch (event.type)
			// 		{
    		// 			case EVENT_LORA_DIO1_IRQ:
    		// 				// Switch to RX state
			// 				COMMS_state = COMMS_RX;
			// 				break;

    		// 			case EVENT_TX:
    		// 				// Switch to TX state
			// 				COMMS_state = COMMS_TX;
			// 				break;

    		// 			case EVENT_CMD:
    		// 				// Switch to CMD state
			// 				COMMS_state = COMMS_CMD;
			// 				break;
			// 		}
					
    		// 	}
    		// 	break;
    		// }
			// IDLE STATE: wait for events and route them
            case COMMS_IDLE:
            {
                printf("COMMS_IDLE: Waiting for events ...\r\n");
                CommsEvent event;
                
                // Only start reception if we aren't currently doing something else
                if (current_radio_mode == RADIO_MODE_STANDBY)
				{
                    startReception();
                }

                if (xQueuePeek(COMMS_queue_events, &event, portMAX_DELAY) == pdTRUE)
                {
                    switch (event.type)
                    {
                        case EVENT_LORA_RX_DONE:
                            // Switch to RX state to process the payload
                            COMMS_state = COMMS_RX;
                            break;

                        case EVENT_LORA_TX_DONE:
                            // The radio finished transmitting in the background.
                            // Consume the event, reset hardware state to standby, and stay in IDLE
                            xQueueReceive(COMMS_queue_events, &event, 0); 
                            printf("COMMS_IDLE: Background TX Complete.\r\n");
                            current_radio_mode = RADIO_MODE_STANDBY;
                            break;

                        case EVENT_TX:
                            COMMS_state = COMMS_TX;
                            break;

                        case EVENT_CMD:
                            COMMS_state = COMMS_CMD;
                            break;
                    }
                }
                break;
            }

    		// RX STATE: process incoming packet
    		case COMMS_RX:
    		{
    			printf("COMMS_RX: Processing incoming packet ...\r\n");
				CommsEvent rx_event;
				if (xQueueReceive(COMMS_queue_events, &rx_event, 0) == pdTRUE)
				{
					if (rx_event.type == EVENT_LORA_RX_DONE)
					{
						uint8_t rx_data[PACKET_SIZE_MAX];
						uint8_t len = radio.getPacketLength();
						int16_t state = radio.readData(rx_data, len);
						printData("Received: ", rx_data, len);

						// Reuse rx_event.packet instead of allocating rx_packet to save memory
						bool ecc = isDataECCEnabled(rx_data, len);
						rx_event.packet.init(ecc, 0);

						// Decode
						if (state == RADIOLIB_ERR_NONE)
						{
							if (ecc)
							{
								rx_event.packet.state = decodeECC(rx_data, len);
							}
						}
						else
						{
							// CRC Error: Try to recover via ECC anyway
							ecc = true;
							rx_event.packet.state = decodeECC(rx_data, len);
						}

						// Parse and validate
						dataToPacket(rx_data, len, &rx_event.packet);
						rs_enabled = rx_event.packet.ecc;

						// Route valid packet
						if (rx_event.packet.state == PACKET_ERR_NONE)
						{
							if (scheduleAction(EVENT_CMD, &rx_event.packet, false) != pdPASS)
							{
								rx_event.packet.state = PACKET_ERR_QUEUE_FULL;
							}
						}

						// Acknowledge / Nack
						if (rx_event.packet.state != PACKET_ERR_NONE)
						{
							sendNACK(rs_enabled, rx_event.packet.command, rx_event.packet.state);
						}
						else if (isACKNeededBefore(&rx_event.packet))
						{
							sendACK(rs_enabled, rx_event.packet.command);
						}
					}
				}

				// Return to idle
				current_radio_mode = RADIO_MODE_STANDBY; 
                COMMS_state = COMMS_IDLE;
				break;
    		}

    		// TX STATE: process outgoing packet
    		case COMMS_TX:
    		{
    			printf("COMMS_TX: Processing outgoing packet ...\r\n");
				CommsEvent tx_event;
				if (xQueueReceive(COMMS_queue_events, &tx_event, 0) == pdTRUE)
				{
					if (tx_event.type == EVENT_TX)
					{
						if (tx_state == TX_OFF || (tx_state == TX_NOBEACON && tx_event.packet.command == TER_BEACON))
						{
							printf("Transmission is off, skipping packet\r\n");
							COMMS_state = COMMS_IDLE;
							break;
						}

						// Prepare packet buffer
						uint8_t tx_data[PACKET_SIZE_MAX];
						uint8_t tx_data_size = packetToData(&tx_event.packet, tx_data);

						// If Reed-Solomon encoding is enabled, encode the packet
						if (rs_enabled && tx_event.packet.ecc)
						{
							printData("Before encoding: ", tx_data, tx_data_size);
							encodeECC(tx_data, tx_data_size);
						}

						// Start transmission (Non-blocking)
						startTransmission(tx_data, tx_data_size);

						// We do NOT wait here.
						// The SX1268 is transmitting in the background.
						// We return to IDLE. When TX is done, DIO1 will fire and push an EVENT_LORA_DIO1_IRQ.
					}
				}

				// Return to idle
				COMMS_state = COMMS_IDLE;
				break;
    		}

			// CMD STATE: process command packet
            case COMMS_CMD:
			{
				printf("COMMS_CMD: Processing command packet ...\r\n");
				CommsEvent cmd_event;

				// Pull the event from the unified queue (no blocking)
				if (xQueueReceive(COMMS_queue_events, &cmd_event, 0) == pdTRUE)
				{
					// Safety check: ensure the event routed here is actually a command
					if (cmd_event.type == EVENT_CMD)
					{
						// Execute command payload
						int8_t cmd_state = executeTEC(&cmd_event.packet);

						if (cmd_state == PACKET_ERR_NONE)
						{
							// Command executed successfully, send ACK if needed
							if (isACKNeeded(&cmd_event.packet))
							{
								sendACK(cmd_event.packet.ecc, cmd_event.packet.command);
							}
						}
						else
						{
							// Command execution failed, send NACK with error code
							sendNACK(cmd_event.packet.ecc, cmd_event.packet.command, cmd_state);
						}
					}
				}

				// Return to IDLE immediately. 
				// If another command is waiting, IDLE will route us back here on the next cycle.
				COMMS_state = COMMS_IDLE; 
				break;
			}

			default:
			{
				printf("Unknown COMMS state! Resetting to IDLE\r\n");
				COMMS_state = COMMS_IDLE; // reset to idle state if unknown state
				break;
			}

        }

        // Prevent watchdog starvation
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// =============================================================================
// SECTION 8: ENTRY POINT (Called from main.c)
// =============================================================================

void PowerUp_COMMS() {
    if (COMMS_task_handle == NULL) {
        printf("[OBC] Spawning COMMS Task...\n");
        xTaskCreate(COMMS_stateMachine, "COMMSTask", 4096, NULL, osPriorityNormal, &COMMS_task_handle);
    }
}

void PowerDown_COMMS() {
    if (COMMS_task_handle != NULL) {
        printf("[OBC] Stopping COMMS Task...\n");
        radio.sleep();
        vTaskDelete(COMMS_task_handle);
        COMMS_task_handle = NULL;
    }
}

extern "C" void app_OBC_loop(void *argument) {
    printf("\n--- J2050 OBC BOOT SEQUENCE ---\n");

    // Start Radio Task
    PowerUp_COMMS();

    // Mission Heartbeat
    for(;;) {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

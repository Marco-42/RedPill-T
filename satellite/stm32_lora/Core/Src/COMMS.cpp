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
// SECTION 1: HARDWARE & GLOBAL OBJECTS
// =============================================================================

// External Handles (Defined in main.c)
extern SPI_HandleTypeDef hspi1;
extern osThreadId_t OBCTaskHandle;

// RadioLib Hardware Abstraction & Module
STM32Hal radioHal(&hspi1);
// SX1268(Module(hal, CS, DIO1, RST, BUSY))
SX1268 radio = new Module(&radioHal, RLIB_NSS, RLIB_DIO1, RLIB_RESET, RLIB_BUSY);

// RTOS Objects
TaskHandle_t COMMSTaskHandle = NULL;
QueueHandle_t RTOS_queue_TX = NULL;
QueueHandle_t RTOS_queue_cmd = NULL;

// Timers
TimerHandle_t RTOS_timer_lora_state = NULL;
TimerHandle_t RTOS_timer_lora_config = NULL;
TimerHandle_t RTOS_timer_cry_state = NULL;

// System State Variables
volatile bool packetReceived = false;
uint8_t tx_state = TX_ON;
uint8_t cry_state = CRY_OFF;
bool rs_enabled = false;

// Time & Security
static uint32_t system_time_offset = 1735689600; // Default: Jan 1 2025
const uint8_t SECRET_KEY[] = { 0xA1, 0xB2, 0xC3, 0xD4 }; // HMAC Secret Key

// =============================================================================
// SECTION 2: INTERRUPT SERVICE ROUTINES
// =============================================================================

// Callback for GPIO External Interrupts (Overriding Weak symbol from HAL)
extern "C" void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == LORA_DIO1_Pin)
    {
        packetReceived = true;

        // Wake up the COMMS task immediately if it is sleeping
        if(COMMSTaskHandle != NULL) {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            vTaskNotifyGiveFromISR(COMMSTaskHandle, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    }
}

// =============================================================================
// SECTION 3: UTILITY & HELPER FUNCTIONS
// =============================================================================

void printStartupMessage(const char* device) {
    osDelay(500);
    printf("%s starting ... ", device);
    osDelay(100);
}

void printRadioStatus(int16_t state, bool blocking) {
    if (state == RADIOLIB_ERR_NONE) {
        printf("ok\r\n");
    } else {
        printf("failed! --> Code: %d\r\n", state);
        if (blocking) {
            printf("Blocking program until user forces restart!\r\n");
            while (true) { osDelay(10000); }
        }
    }
}

void printData(const char* prefix, const uint8_t* data, uint8_t length) {
    if (prefix != NULL) printf("%s", prefix);
    for (uint8_t i = 0; i < length; ++i) printf("%02X ", data[i]);
    printf("\r\n");
}

// Helper to set Unix time from external source
void setUNIX(uint32_t unixTime) {
    if (unixTime == 0) unixTime = 1735689600;
    // Calculate offset based on current system tick
    system_time_offset = unixTime - (HAL_GetTick() / 1000);
}

uint32_t getUNIX() {
    return system_time_offset + (HAL_GetTick() / 1000);
}

// Helper to write float into byte array (Big Endian)
void writeFloatToBytes(float value, uint8_t* buffer) {
    uint32_t value_int;
    memcpy(&value_int, &value, sizeof(float));
    buffer[0] = (value_int >> 24) & 0xFF;
    buffer[1] = (value_int >> 16) & 0xFF;
    buffer[2] = (value_int >> 8) & 0xFF;
    buffer[3] = value_int & 0xFF;
}

// Generate HMAC-SHA256 for Packet Authentication
int8_t makeMAC(const Packet* packet, uint32_t* out_mac) {
    uint8_t buffer[PACKET_HEADER_LENGTH + PACKET_PAYLOAD_MAX];
    uint8_t full_output[32];

    // Construct Header for Hashing
    buffer[0] = packet->station;
    buffer[1] = packet->ecc ? RS_ON : RS_OFF;
    buffer[2] = packet->command;
    buffer[3] = packet->payload_length;
    buffer[4] = (packet->time_unix >> 24) & 0xFF;
    buffer[5] = (packet->time_unix >> 16) & 0xFF;
    buffer[6] = (packet->time_unix >> 8)  & 0xFF;
    buffer[7] = (packet->time_unix)       & 0xFF;
    memset(&buffer[8], 0, 4); // Zero out MAC field for calculation
    memcpy(buffer + PACKET_HEADER_LENGTH, packet->payload, packet->payload_length);

    // Compute Hash
    hmac_sha256(SECRET_KEY, sizeof(SECRET_KEY), buffer, PACKET_HEADER_LENGTH + packet->payload_length, full_output);

    // Truncate to 4 bytes
    *out_mac = ((uint32_t)full_output[0] << 24) |
               ((uint32_t)full_output[1] << 16) |
               ((uint32_t)full_output[2] << 8) |
               ((uint32_t)full_output[3]);
    return PACKET_ERR_NONE;
}

// =============================================================================
// SECTION 4: RADIO HARDWARE CONTROL
// =============================================================================

void Init_Radio_Hardware() {
    printf("[COMMS] Binding Radio Hardware...\n");

    // Map Virtual Pins to Real CubeMX Pins
    radioHal.addPin(RLIB_NSS,   LORA_CS_GPIO_Port,    LORA_CS_Pin);
    radioHal.addPin(RLIB_RESET, LORA_RST_GPIO_Port,   LORA_RST_Pin);
    radioHal.addPin(RLIB_DIO1,  LORA_DIO1_GPIO_Port,  LORA_DIO1_Pin);
    radioHal.addPin(RLIB_BUSY,  LORA_BUSY_GPIO_Port,  LORA_BUSY_Pin);

    // Ensure CS is High (Inactive) before Init
    HAL_GPIO_WritePin(LORA_CS_GPIO_Port, LORA_CS_Pin, GPIO_PIN_SET);

    printf("[COMMS] RadioLib Begin...\n");
    int16_t state = radio.begin(LORA_FREQ, LORA_BW, LORA_SF, LORA_CR, LORA_SYNC, LORA_PWR, LORA_PREAMBLE, LORA_TCXO_V, LORA_USE_LDO);

    printRadioStatus(state, false);
}

void startReception() {
	int16_t rx_state = radio.startReceive();
    printf("[SX1268] Listening ...");
    printRadioStatus(rx_state, false);
}

void startTransmission(uint8_t* tx_packet, uint8_t packet_size) {
    printData("Transmitting: ", tx_packet, packet_size);
    int16_t tx_state = radio.startTransmit(tx_packet, packet_size);
    printRadioStatus(tx_state, false);
}

void packetEvent(void) {
    // Stub: Event handled by HAL_GPIO_EXTI_Callback
}

// =============================================================================
// SECTION 5: PACKET & ECC LOGIC
// =============================================================================

// Packet Class Methods
void Packet::init(bool rs_enabled, uint8_t cmd) {
    state = PACKET_ERR_NONE;
    station = MISSION_ID;
    command = cmd;
    // Disable ECC for Beacons to save power/overhead
    ecc = (rs_enabled && command != TER_BEACON);
    time_unix = 0;
    MAC = 0;
    payload_length = 0;
    memset(payload, 0, PACKET_PAYLOAD_MAX);
}

int8_t Packet::setPayload(const uint8_t* data, uint8_t length) {
    if (length > PACKET_PAYLOAD_MAX) return PACKET_ERR_LENGTH;
    memcpy(payload, data, length);
    payload_length = length;
    return PACKET_ERR_NONE;
}

int8_t Packet::seal() {
    time_unix = getUNIX();
    uint32_t mac_calculated;
    if (makeMAC(this, &mac_calculated) != PACKET_ERR_NONE) return PACKET_ERR_MAC;
    MAC = mac_calculated;
    return PACKET_ERR_NONE;
}

// Data Conversion & Validation
void dataToPacket(const uint8_t* data, uint8_t length, Packet* packet) {
    if (length < PACKET_HEADER_LENGTH || length > PACKET_SIZE_MAX) {
        packet->state = PACKET_ERR_LENGTH;
        return;
    }

    packet->station = data[0];

    // Check RS Flag
    if (data[1] == RS_ON) packet->ecc = true;
    else if (data[1] == RS_OFF) packet->ecc = false;
    else { packet->state = PACKET_ERR_RS; return; }

    // Check Command
    packet->command = data[2];
    if (!isTEC(packet->command)) { packet->state = PACKET_ERR_CMD_UNKNOWN; return; }

    packet->payload_length = data[3];

    // Deserialize Time and MAC
    packet->time_unix = ((uint32_t)data[4] << 24) | ((uint32_t)data[5] << 16) | ((uint32_t)data[6] << 8) | (uint32_t)data[7];
    packet->MAC = ((uint32_t)data[8] << 24) | ((uint32_t)data[9] << 16) | ((uint32_t)data[10] << 8) | (uint32_t)data[11];

    packet->setPayload(data + PACKET_HEADER_LENGTH, packet->payload_length);

    // Verify MAC
    uint32_t mac_calc;
    if (makeMAC(packet, &mac_calc) != PACKET_ERR_NONE) { packet->state = PACKET_ERR_MAC; return; }

    packet->mac_valid = (mac_calc == packet->MAC);
    packet->state = PACKET_ERR_NONE;
}

uint8_t packetToData(const Packet* packet, uint8_t* data) {
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

// ECC Helpers
bool isDataECCEnabled(const uint8_t* data, uint8_t length) {
    // If length aligns with RS blocks and RS flag is present
    return ((length % RS_BLOCK_SIZE == 0) && (data[1] != RS_OFF));
}

void encodeECC(uint8_t* data, uint8_t& data_len) {
    uint8_t num_blocks = (data_len + DATA_BLOCK_SIZE - 1) / DATA_BLOCK_SIZE;
    uint8_t codewords[num_blocks][RS_BLOCK_SIZE];

    // Encode Blocks
    for (uint8_t i = 0; i < num_blocks; ++i) {
        uint8_t block[DATA_BLOCK_SIZE];
        memset(block, RS_PADDING, DATA_BLOCK_SIZE);
        uint8_t remaining = data_len - i * DATA_BLOCK_SIZE;
        memcpy(block, data + i * DATA_BLOCK_SIZE, (remaining >= DATA_BLOCK_SIZE ? DATA_BLOCK_SIZE : remaining));
        encode_data(block, DATA_BLOCK_SIZE, codewords[i]);
    }

    // Interleave
    uint8_t* interleaved = (uint8_t*)malloc(num_blocks * RS_BLOCK_SIZE);
    if (!interleaved) return;

    for (uint8_t col = 0; col < RS_BLOCK_SIZE; ++col) {
        for (uint8_t row = 0; row < num_blocks; ++row) {
            interleaved[col * num_blocks + row] = codewords[row][col];
        }
    }
    memcpy(data, interleaved, num_blocks * RS_BLOCK_SIZE);
    free(interleaved);
    data_len = num_blocks * RS_BLOCK_SIZE;
}

int8_t decodeECC(uint8_t* data, uint8_t& data_len) {
    int8_t error = PACKET_ERR_NONE;
    uint8_t num_blocks = data_len / RS_BLOCK_SIZE;
    uint8_t codewords[num_blocks][RS_BLOCK_SIZE];

    // Deinterleave
    for (uint8_t col = 0; col < RS_BLOCK_SIZE; ++col) {
        for (uint8_t row = 0; row < num_blocks; ++row) {
            codewords[row][col] = data[col * num_blocks + row];
        }
    }

    // Decode Blocks
    uint8_t write_pos = 0;
    for (uint8_t i = 0; i < num_blocks; ++i) {
        decode_data(codewords[i], RS_BLOCK_SIZE);
        if (check_syndrome() != 0) {
            if (correct_errors_erasures(codewords[i], RS_BLOCK_SIZE, 0, NULL) == 0) {
                printf("RS decode failed on block %d\r\n", i);
                error = PACKET_ERR_DECODE;
            } else {
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
// SECTION 6: COMMAND PROCESSING & TIMERS
// =============================================================================

bool isTEC(uint8_t command) {
    // Check if command ID is within valid range
    // (Simplified check, add specific cases if needed)
    return true;
}

bool isACKNeeded(const Packet* packet) {
    // Ping usually doesn't need an ACK packet, just a PONG response
    return (packet->command != TEC_LORA_PING);
}

bool isACKNeededBefore(const Packet* packet) {
    // For dangerous commands (Reboot), ACK before execution
    return (packet->command == TEC_OBC_REBOOT || packet->command == TEC_LORA_CONFIG);
}

void sendACK(bool ecc, uint8_t TEC) {
    Packet ack;
    ack.init(ecc, TER_ACK);
    ack.setPayload(&TEC, 1);
    ack.seal();
    xQueueSend(RTOS_queue_TX, &ack, 0);
}

void sendNACK(bool ecc, uint8_t TEC, int8_t error) {
    Packet nack;
    nack.init(ecc, TER_NACK);
    uint8_t pl[2] = {TEC, (uint8_t)error};
    nack.setPayload(pl, 2);
    nack.seal();
    xQueueSend(RTOS_queue_TX, &nack, 0);
}

// Execute Telecommand Logic
int8_t executeTEC(const Packet* cmd) {
    if (!cmd) return PACKET_ERR_CMD_POINTER;

    switch (cmd->command) {
        case TEC_OBC_REBOOT:
            printf("TEC: OBC REBOOT\r\n");
            HAL_NVIC_SystemReset();
            break;

        case TEC_LORA_PING: {
            printf("TEC: PING\r\n");
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
            xQueueSend(RTOS_queue_TX, &pong, 0);
            break;
        }

        case TEC_LORA_STATE: {
             printf("TEC: LORA STATE\r\n");
             uint8_t newState = cmd->payload[0] & 0x0F;
             uint32_t duration = ((uint32_t)cmd->payload[1] << 16) | ((uint32_t)cmd->payload[2] << 8) | cmd->payload[3];

             tx_state = newState;
             printf("Set TX State: %d for %lu s\r\n", tx_state, (unsigned long)duration);

             deleteTimerIfExists(&RTOS_timer_lora_state, "LoRa State");
             if (duration > 0) {
                 createDelayedCommand(cmd, queueDelayedPacket, duration, editDelayedLoraState, &RTOS_timer_lora_state, "LoRa State");
             }
             break;
        }

        case TEC_LORA_CONFIG: {
             printf("TEC: LORA CONFIG\r\n");
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
			uint8_t reserved = b4 & 0b111; // 3 bits

			// Byte 5: duration in seconds
			uint8_t duration = cmd->payload[5];

			// Validate parameters
			printf("LoRa Config: Freq: %u kHz, BW: %.1f kHz, SF: %d, CR: %d, Power: %d dBm\r\n", freq_mhz, bw_khz, sf, cr, power);
			if (freq_mhz < 400 || freq_mhz > 500 || bw_khz < 62.5 || bw_khz > 500 ||
				sf < 6 || sf > 12 || cr < 5 || cr > 8 || power < -4 || power > 17) // TODO update power range for SX1268, -9 to +22 dBm
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
			deleteTimerIfExists(&RTOS_timer_lora_config, "LoRa Config");
			if (duration > 0) {
			 createDelayedCommand(cmd, queueDelayedPacket, duration, editDelayedLoraConfig, &RTOS_timer_lora_config, "LoRa Config");
			}
			break;
        }

        default:
            printf("Unknown Command: %02X\r\n", cmd->command);
            return PACKET_ERR_CMD_UNKNOWN;
    }
    return PACKET_ERR_NONE;
}

// Timer Helpers
void deleteTimerIfExists(TimerHandle_t* timer_handle, const char* timer_name) {
    if (*timer_handle != NULL) {
        xTimerStop(*timer_handle, 0);
        TimerPayload* payload = (TimerPayload*)pvTimerGetTimerID(*timer_handle);
        if (payload) {
            if (payload->packet) vPortFree(payload->packet);
            vPortFree(payload);
        }
        xTimerDelete(*timer_handle, 0);
        *timer_handle = NULL;
        printf("Cancelled Timer: %s\r\n", timer_name);
    }
}

void queueDelayedPacket(TimerHandle_t xTimer) {
    TimerPayload* payload = (TimerPayload*)pvTimerGetTimerID(xTimer);
    if (payload) {
        if (xQueueSend(RTOS_queue_cmd, payload->packet, 0) == pdPASS) {
            printf("Delayed command queued.\r\n");
        }
        if (payload->global_handle) *(payload->global_handle) = NULL;
        vPortFree(payload->packet);
        vPortFree(payload);
    }
    xTimerDelete(xTimer, 0);
}

void createDelayedCommand(const Packet* cmd, void (*callback)(TimerHandle_t), uint32_t delay_seconds, void (*editDelayedCommand)(Packet*), TimerHandle_t* global_timer_handle, const char* timer_name) {
    Packet* delayed_cmd = (Packet*)pvPortMalloc(sizeof(Packet));
    if (!delayed_cmd) return;
    memcpy(delayed_cmd, cmd, sizeof(Packet));
    editDelayedCommand(delayed_cmd);

    TimerPayload* payload = (TimerPayload*)pvPortMalloc(sizeof(TimerPayload));
    if (!payload) { vPortFree(delayed_cmd); return; }
    payload->packet = delayed_cmd;
    payload->global_handle = global_timer_handle;

    *global_timer_handle = xTimerCreate(timer_name, pdMS_TO_TICKS(delay_seconds * 1000UL), pdFALSE, (void*)payload, callback);
    if (*global_timer_handle) xTimerStart(*global_timer_handle, 0);
    else { vPortFree(delayed_cmd); vPortFree(payload); }
}

void editDelayedLoraState(Packet* packet) {
    if (!packet) return;
    // Set to default state (TX_ON)
    packet->payload[0] = ((TX_ON & 0b1111) << 4) | (TX_ON & 0b1111);
    memset(&packet->payload[1], 0, 3);
    packet->seal();
}

void editDelayedLoraConfig(Packet* packet) {
    if (!packet) return;
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
// SECTION 7: COMMS STATE MACHINE
// =============================================================================

void COMMS_stateMachine(void *parameter) {
    printStartupMessage("COMMS");

    // 1. Initialize Hardware
    Init_Radio_Hardware();

    // 2. Initialize Queues
    if (RTOS_queue_TX == NULL) RTOS_queue_TX = xQueueCreate(TX_QUEUE_SIZE, sizeof(Packet));
    if (RTOS_queue_cmd == NULL) RTOS_queue_cmd = xQueueCreate(CMD_QUEUE_SIZE, sizeof(Packet));

    // 3. Initialize Logic
    initialize_ecc();
    startReception();

    uint8_t COMMS_state = COMMS_IDLE;

    for(;;) {
        // --- IDLE STATE: Wait for Events ---
        if (COMMS_state == COMMS_IDLE) {
            if (uxQueueMessagesWaiting(RTOS_queue_TX) > 0) {
                COMMS_state = COMMS_TX;
            } else if (uxQueueMessagesWaiting(RTOS_queue_cmd) > 0) {
                COMMS_state = COMMS_CMD;
            } else if (packetReceived) {
                packetReceived = false; // Clear flag
                COMMS_state = COMMS_RX;
            } else if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(IDLE_TIMEOUT)) != 0) {
                // Notified by ISR directly
                packetReceived = false;
                COMMS_state = COMMS_RX;
            }
        }

        // --- RX STATE: Process Incoming Packet ---
        if (COMMS_state == COMMS_RX) {
            uint8_t rx_data[PACKET_SIZE_MAX];
            uint8_t len = radio.getPacketLength();
            int16_t state = radio.readData(rx_data, len);
            printData("Received: ", rx_data, len);

            Packet rx_packet;
            bool ecc = isDataECCEnabled(rx_data, len);
            rx_packet.init(ecc, 0);

            // Decode
            if (state == RADIOLIB_ERR_NONE) {
                if (ecc) rx_packet.state = decodeECC(rx_data, len);
            } else {
                // CRC Error: Try to recover via ECC anyway
                ecc = true;
                rx_packet.state = decodeECC(rx_data, len);
            }

            // Parse & Validate
            dataToPacket(rx_data, len, &rx_packet);
            rs_enabled = rx_packet.ecc;

            // Route Valid Packet
            if (rx_packet.state == PACKET_ERR_NONE) {
                if (xQueueSend(RTOS_queue_cmd, &rx_packet, 0) != pdPASS) {
                    rx_packet.state = PACKET_ERR_CMD_FULL;
                }
            }

            // Acknowledge / Nack
            if (rx_packet.state != PACKET_ERR_NONE) {
                sendNACK(rs_enabled, rx_packet.command, rx_packet.state);
            } else if (isACKNeededBefore(&rx_packet)) {
                sendACK(rs_enabled, rx_packet.command);
            }

            // Return to Idle
            startReception();
            COMMS_state = COMMS_IDLE;
        }

        // --- TX STATE: Send Packets ---
        if (COMMS_state == COMMS_TX) {
            while (uxQueueMessagesWaiting(RTOS_queue_TX) > 0) {
                Packet tx_pkt;
                xQueueReceive(RTOS_queue_TX, &tx_pkt, 0);

                uint8_t raw[PACKET_SIZE_MAX];
                uint8_t size = packetToData(&tx_pkt, raw);

                if (rs_enabled && tx_pkt.ecc) encodeECC(raw, size);

                startTransmission(raw, size);
                osDelay(100); // Wait for TX to complete (Simple blocking)
            }
            startReception();
            COMMS_state = COMMS_IDLE;
        }

        // --- CMD STATE: Execute Telecommands ---
        if (COMMS_state == COMMS_CMD) {
            while (uxQueueMessagesWaiting(RTOS_queue_cmd) > 0) {
                Packet cmd;
                xQueueReceive(RTOS_queue_cmd, &cmd, 0);

                int8_t st = executeTEC(&cmd);

                if (st == PACKET_ERR_NONE) {
                    if (isACKNeeded(&cmd)) sendACK(cmd.ecc, cmd.command);
                } else {
                    sendNACK(cmd.ecc, cmd.command, st);
                }
            }
            COMMS_state = COMMS_IDLE;
        }

        // Prevent Watchdog Starvation
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// =============================================================================
// SECTION 8: ENTRY POINT (Called from main.c)
// =============================================================================

void PowerUp_COMMS() {
    if (COMMSTaskHandle == NULL) {
        printf("[OBC] Spawning COMMS Task...\n");
        xTaskCreate(COMMS_stateMachine, "COMMSTask", 4096, NULL, osPriorityNormal, &COMMSTaskHandle);
    }
}

void PowerDown_COMMS() {
    if (COMMSTaskHandle != NULL) {
        printf("[OBC] Stopping COMMS Task...\n");
        radio.sleep();
        vTaskDelete(COMMSTaskHandle);
        COMMSTaskHandle = NULL;
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

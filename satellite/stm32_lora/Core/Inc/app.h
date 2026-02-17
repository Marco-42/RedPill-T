/* Core/Inc/app.h */
#ifndef APP_H
#define APP_H

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------
// SEZIONE COMPATIBILE C e C++
// ---------------------------------
#include "main.h"
#include "cmsis_os.h" // Per i tipi di FreeRTOS usati in C

// API per main.c (Ponte C -> C++)
void app_OBC_loop(void *argument);

#ifdef __cplusplus
}
#endif

// ---------------------------------
// SEZIONE ESCLUSIVA C++ (Invisibile a main.c)
// ---------------------------------
#ifdef __cplusplus

// ---------------------------------
// INCLUDE STANDARD & FREERTOS
// ---------------------------------
#include <cstring>  // FONDAMENTALE per memcpy, memset
#include <cstdio>   // FONDAMENTALE per printf
#include <cstdlib>  // per abs, rand
#include <vector>
#include <string>
#include <stdint.h>

// Include FreeRTOS C++ (Tipi Handle)
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

// Include Specifici Progetto
#include "SX1268.h"
#include "sha256.h"

// Include per rscode (avvolto in extern C perché è C puro)
extern "C" {
    #include "ecc.h"
}

// ---------------------------------
// LORA CONFIGURATION
// ---------------------------------
// Mappatura Pin STM32 (Generati da CubeMX)
#define LORA_SPI_HANDLE     &hspi1
#define LORA_CS_PORT        LORA_CS_GPIO_Port
#define LORA_CS_PIN         LORA_CS_Pin
#define LORA_BUSY_PORT      LORA_BUSY_GPIO_Port
#define LORA_BUSY_PIN       LORA_BUSY_Pin
#define LORA_RST_PORT       LORA_RST_GPIO_Port
#define LORA_RST_PIN        LORA_RST_Pin
#define LORA_DIO1_PORT      LORA_DIO1_GPIO_Port
#define LORA_DIO1_PIN       LORA_DIO1_Pin

// Parametri Radio
#define F 436.0
#define BW 125.0
#define SF 10
#define CR 5
#define OUTPUT_POWER 1
#define PREAMBLE_LENGTH 8
#define TCXO_V 0
#define USE_LDO false

// Istanza globale radio
extern SX1268 radio;

// ---------------------------------
// DEFINIZIONI COMMS (Stati Macchina a Stati)
// ---------------------------------
#define COMMS_IDLE 0
#define COMMS_TX 1
#define COMMS_TX_ERROR 2
#define COMMS_RX 3
#define COMMS_RX_ERROR 4
#define COMMS_CMD 5
#define COMMS_ERROR 6
#define COMMS_SERIAL 7

#define IDLE_TIMEOUT 500
#define RX_TIMEOUT 1000

#define TX_QUEUE_SIZE 6
#define PACKET_SIZE_MAX 128
#define PACKET_HEADER_LENGTH 12
#define PACKET_PAYLOAD_MAX 98
#define CMD_QUEUE_SIZE 2

// Entry point della macchina a stati
void COMMS_stateMachine(void *parameter);

// ---------------------------------
// STRUCTS
// ---------------------------------
struct Packet
{
    bool ecc;
    bool mac_valid;
    int8_t state;
    uint8_t station;
    uint8_t command;
    uint8_t payload_length;
    uint32_t time_unix;
    uint32_t MAC;
    uint8_t payload[PACKET_PAYLOAD_MAX];

    void init(bool rs_enabled, uint8_t cmd);
    int8_t setPayload(const uint8_t* data, uint8_t length);
    int8_t seal();
};

struct TimerPayload
{
    Packet* packet;
    TimerHandle_t* global_handle;
};

// ---------------------------------
// COSTANTI GLOBALI & HELPER
// ---------------------------------

// Crystal State Definitions (Mancavano nel vecchio codice)
#define CRY_OFF 0x00
#define CRY_LIGHT 0x01
#define CRY_DARK 0x02

extern uint8_t cry_state;
extern bool rs_enabled;
extern uint8_t tx_state;

// Funzioni di utilità
void printStartupMessage(const char* device);
void printRadioStatus(int16_t state, bool blocking = false);
void printData(const char* prefix, const uint8_t* packet, uint8_t length);
void printPacket(const char* prefix, const Packet* packet);
void setUNIX(uint32_t unixTime = 0);
uint32_t getUNIX();
int8_t makeMAC(const Packet* packet, uint32_t* out_mac);
void writeFloatToBytes(float value, uint8_t* buffer);

// ---------------------------------
// RADIO & ECC CONFIGURATION
// ---------------------------------
#ifndef NPAR
#define NPAR 2
#endif

#define RS_BLOCK_SIZE 16
#define DATA_BLOCK_SIZE (RS_BLOCK_SIZE - NPAR)

#define RS_OFF 0x55
#define RS_ON 0xAA
#define RS_PADDING 0x00

#define TX_OFF 0x00
#define TX_ON 0x01
#define TX_NOBEACON 0x02

// Codici Errore
#define PACKET_ERR_NONE 0
#define PACKET_ERR_RS -1
#define PACKET_ERR_DECODE -2
#define PACKET_ERR_LENGTH -3
#define PACKET_ERR_MAC -4
#define PACKET_ERR_CMD_FULL -5
#define PACKET_ERR_CMD_POINTER -6
#define PACKET_ERR_CMD_UNKNOWN -7
#define PACKET_ERR_CMD_PAYLOAD -8
#define PACKET_ERR_CMD_MEMORY -9

// TEC Codes (Telecommand)
#define TEC_OBC_REBOOT 0x01
#define TEC_EXIT_STATE 0x02
#define TEC_VAR_CHANGE 0x03
#define TEC_SET_TIME 0x04
#define TEC_EPS_REBOOT 0x08
#define TEC_ADCS_REBOOT 0x10
#define TEC_ADCS_TLE 0x11
#define TEC_LORA_STATE 0x18
#define TEC_LORA_CONFIG 0x19
#define TEC_LORA_PING 0x1A
#define TEC_CRY_EXP 0x80

// TER Codes (Telemetry Response)
#define TER_BEACON 0x30
#define TER_ACK 0x31
#define TER_NACK 0x32
#define TER_LORA_PONG 0x33
#define MISSION_ID 0x01

// Funzioni Radio
void packetEvent(void);
void startReception(void);
void startTransmission(uint8_t *tx_packet, uint8_t packet_size);
void dataToPacket(const uint8_t* data, uint8_t length, Packet* packet);
uint8_t packetToData(const Packet* packet, uint8_t* data);

// Funzioni ECC
bool isDataECCEnabled(const uint8_t* data, uint8_t length);
void encodeECC(uint8_t* data, uint8_t& data_len);
int8_t decodeECC(uint8_t* data, uint8_t& data_len);

// ---------------------------------
// COMMAND FUNCTIONS
// ---------------------------------
int8_t executeTEC(const Packet* cmd);
bool isTEC(uint8_t command);
bool isACKNeeded(const Packet* packet);
bool isACKNeededBefore(const Packet* packet);
void sendACK(bool ecc, uint8_t TEC);
void sendNACK(bool ecc, uint8_t TEC, int8_t error);

// ---------------------------------
// RTOS HANDLES (Extern)
// ---------------------------------
extern TaskHandle_t COMMSTaskHandle;
extern QueueHandle_t RTOS_queue_TX;
extern QueueHandle_t RTOS_queue_cmd;
extern TimerHandle_t RTOS_timer_lora_state;
extern TimerHandle_t RTOS_timer_lora_config;
extern TimerHandle_t RTOS_timer_cry_state;

// ---------------------------------
// TIMER HELPERS
// ---------------------------------
void queueDelayedPacket(TimerHandle_t xTimer);
void createDelayedCommand(const Packet* cmd,
                          void (*callback)(TimerHandle_t),
                          uint32_t delay_seconds,
                          void (*editDelayedCommand)(Packet*),
                          TimerHandle_t* global_timer_handle,
                          const char* timer_name);

void deleteTimerIfExists(TimerHandle_t* timer_handle, const char* timer_name);
void editDelayedGeneric4(Packet* packet);
void editDelayedGeneric1(Packet* packet);
void editDelayedLoraState(Packet* packet);
void editDelayedLoraConfig(Packet* packet);

#endif // Fine sezione C++

#endif /* APP_H */

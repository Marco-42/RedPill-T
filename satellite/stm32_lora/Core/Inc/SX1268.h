/*
 * SX1268.h
 *
 *  Created on: 16 feb 2026
 *      Author: aless
 */

/* Core/Inc/SX1268.h */
#ifndef INC_SX1268_H_
#define INC_SX1268_H_

#include "main.h"
#include "cmsis_os.h" // Per osDelay

// Codici di errore compatibili con RadioLib
#define RADIOLIB_ERR_NONE     0
#define RADIOLIB_ERR_CHIP_NOT_FOUND -1
#define RADIOLIB_ERR_PACKET_TOO_LONG -2
#define RADIOLIB_ERR_TX_TIMEOUT -3
#define RADIOLIB_ERR_RX_TIMEOUT -4
#define RADIOLIB_ERR_CRC_MISMATCH -5
#define RADIOLIB_ERR_INVALID_HEADER -6
#define RADIOLIB_ERR_SPI_WRITE_FAILED -7

// Comandi SX126x
#define SX126X_CMD_SET_SLEEP 0x84
#define SX126X_CMD_SET_STANDBY 0x80
#define SX126X_CMD_SET_FS 0xC1
#define SX126X_CMD_SET_TX 0x83
#define SX126X_CMD_SET_RX 0x82
#define SX126X_CMD_SET_MODULATION_PARAMS 0x8B
#define SX126X_CMD_SET_PACKET_PARAMS 0x8C
#define SX126X_CMD_WRITE_BUFFER 0x0E
#define SX126X_CMD_READ_BUFFER 0x1E
#define SX126X_CMD_SET_DIO_IRQ_PARAMS 0x08
#define SX126X_CMD_GET_IRQ_STATUS 0x12
#define SX126X_CMD_CLEAR_IRQ_STATUS 0x02
#define SX126X_CMD_SET_RF_FREQUENCY 0x86
#define SX126X_CMD_SET_PA_CONFIG 0x95
#define SX126X_CMD_SET_TX_PARAMS 0x8E
#define SX126X_CMD_SET_BUFFER_BASE_ADDRESS 0x8F
#define SX126X_CMD_SET_PACKET_TYPE 0x8A
#define SX126X_CMD_GET_PACKET_STATUS 0x14
#define SX126X_CMD_GET_RX_BUFFER_STATUS 0x13

// Sync Word
#define RADIOLIB_SX126X_SYNC_WORD_PRIVATE 0x12

class SX1268 {
public:
    // Costruttore
    SX1268(SPI_HandleTypeDef* hspi,
           GPIO_TypeDef* csPort, uint16_t csPin,
           GPIO_TypeDef* busyPort, uint16_t busyPin,
           GPIO_TypeDef* resetPort, uint16_t resetPin,
           GPIO_TypeDef* dio1Port, uint16_t dio1Pin);

    // Metodi principali (API simil-RadioLib)
    int16_t begin(float freq, float bw, uint8_t sf, uint8_t cr, uint8_t syncWord, int8_t power, uint16_t preambleLength, float tcxoVoltage, bool useRegulatorLDO);

    int8_t startReceive();
    int8_t startTransmit(uint8_t* data, uint8_t length);
    int8_t readData(uint8_t* data, uint8_t length);

    // Metodi di configurazione al volo
    int16_t setFrequency(float freq);
    int16_t setBandwidth(float bw);
    int16_t setSpreadingFactor(uint8_t sf);
    int16_t setCodingRate(uint8_t cr);
    int16_t setOutputPower(int8_t power);

    // Telemetria
    uint8_t getPacketLength();
    float getRSSI();
    float getSNR();
    float getFrequencyError();

    // Callback e Interrupt
    void setPacketReceivedAction(void (*func)(void));
    void setPacketSentAction(void (*func)(void));

    // Da chiamare dentro la HAL_GPIO_EXTI_Callback
    void handleDio1Irq();

private:
    SPI_HandleTypeDef* _hspi;
    GPIO_TypeDef* _csPort;      uint16_t _csPin;
    GPIO_TypeDef* _busyPort;    uint16_t _busyPin;
    GPIO_TypeDef* _resetPort;   uint16_t _resetPin;
    GPIO_TypeDef* _dio1Port;    uint16_t _dio1Pin;

    // Cache dei parametri correnti (per riconfigurazioni rapide)
    float _freq;
    float _bw;
    uint8_t _sf;
    uint8_t _cr;
    int8_t _power;
    uint16_t _preambleLength;

    // Puntatori alle funzioni di callback (Bridge verso RTOS)
    void (*_onPacketReceived)(void) = nullptr;
    void (*_onPacketSent)(void) = nullptr;

    // Metodi Low-Level
    void Reset();
    void Wakeup();
    void WaitForBusy();
    void WriteCommand(uint8_t opcode, uint8_t* params, uint8_t paramsLen);
    void ReadCommand(uint8_t opcode, uint8_t* status, uint8_t* buffer, uint8_t bufferLen);
    void WriteBuffer(uint8_t offset, uint8_t* data, uint8_t length);
    void ReadBuffer(uint8_t offset, uint8_t* data, uint8_t length);

    void SetStandby(uint8_t mode = 0x00); // 0=STDBY_RC
    void SetPacketParams(uint16_t preambleLength, uint8_t crcType, uint8_t payloadLength, uint8_t headerType, uint8_t invertIQ);
    void SetModulationParams(uint8_t sf, uint8_t bw, uint8_t cr, uint8_t ldro);
    void FixPaClamping(); // Workaround hardware SX126x
};

#endif /* INC_SX1268_H_ */

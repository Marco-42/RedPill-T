/**
 * @file    SX1268.h
 * @brief   SX1268 Driver - RadioLib Compatible API
 */

#ifndef SX1268_H
#define SX1268_H

#include "stm32l4xx_hal.h"
#include <cstdint>
#include <cstring>
#include <cmath>

// ==========================================
// RADIOLIB COMPATIBILITY DEFINITIONS
// ==========================================
#define RADIOLIB_ERR_NONE     0
#define RADIOLIB_ERR_CHIP_NOT_FOUND -1
#define RADIOLIB_ERR_PACKET_TOO_LONG -2
#define RADIOLIB_ERR_TX_TIMEOUT -3
#define RADIOLIB_ERR_RX_TIMEOUT -4
#define RADIOLIB_ERR_CRC_MISMATCH -5
#define RADIOLIB_ERR_INVALID_HEADER -6
#define RADIOLIB_ERR_SPI_WRITE_FAILED -7

// Sync Words
#define RADIOLIB_SX126X_SYNC_WORD_PRIVATE 0x1424
#define RADIOLIB_SX126X_SYNC_WORD_PUBLIC  0x3444

/* ============================================================================
 * SX1268 Class (RadioLib API)
 * ============================================================================ */
class SX1268 {
public:
    SX1268();

    // Costruttore "Arduino Style" (per compatibilità con app.cpp)
    SX1268(SPI_HandleTypeDef* hspi,
           GPIO_TypeDef* csPort, uint16_t csPin,
           GPIO_TypeDef* busyPort, uint16_t busyPin,
           GPIO_TypeDef* resetPort, uint16_t resetPin,
           GPIO_TypeDef* dio1Port, uint16_t dio1Pin);

    // --- API PRINCIPALI (Esattamente come RadioLib) ---

    // Inizializzazione completa
    int16_t begin(float freq, float bw, uint8_t sf, uint8_t cr, uint16_t syncWord, int8_t power, uint16_t preambleLength, float tcxoVoltage, bool useRegulatorLDO);

    // Ricezione
    int8_t startReceive(); // Continuous mode
    int8_t readData(uint8_t* data, uint8_t length);
    uint8_t getPacketLength();

    // Trasmissione
    int8_t startTransmit(uint8_t* data, uint8_t length);
    int8_t transmit(uint8_t* data, uint8_t length, uint32_t timeoutMs); // Blocking opzionale

    // Configurazione al volo
    int16_t setFrequency(float freq);
    int16_t setBandwidth(float bw);
    int16_t setSpreadingFactor(uint8_t sf);
    int16_t setCodingRate(uint8_t cr);
    int16_t setOutputPower(int8_t power);

    // Telemetria
    float getRSSI();
    float getSNR();
    float getFrequencyError();

    // Callback e Interrupt
    void setPacketReceivedAction(void (*func)(void));
    void setPacketSentAction(void (*func)(void));
    void handleDio1Irq(); // Da chiamare in HAL_GPIO_EXTI_Callback

private:
    // Struttura Pin interna
    struct {
        SPI_HandleTypeDef* hspi;
        GPIO_TypeDef* nssPort;   uint16_t nssPin;
        GPIO_TypeDef* resetPort; uint16_t resetPin;
        GPIO_TypeDef* busyPort;  uint16_t busyPin;
        GPIO_TypeDef* dio1Port;  uint16_t dio1Pin;
    } _pins;

    // Stato interno
    float _freqMHz;
    float _bw;
    uint8_t _sf;
    uint8_t _cr;
    bool _useTCXO;
    uint8_t _tcxoVoltage; // Raw register value for TCXO

    // Callback pointers
    void (*_onRxCallback)(void) = nullptr;
    void (*_onTxCallback)(void) = nullptr;

    // --- METODI LOW-LEVEL (Nascosti) ---
    void hwReset();
    int waitBusy(uint32_t timeoutMs = 1000);
    void nssLow();
    void nssHigh();

    void spiWrite(uint8_t cmd, const uint8_t* data, uint16_t len);
    void spiRead(uint8_t cmd, uint8_t* data, uint16_t len);
    void writeRegister(uint16_t addr, uint8_t data);
    void writeBuffer(uint8_t offset, const uint8_t* data, uint8_t len);
    void readBuffer(uint8_t offset, uint8_t* data, uint8_t len);

    // Comandi SX126x
    void setStandby(uint8_t mode); // 0=RC, 1=XOSC
    void setPacketParams(uint16_t preamble, uint8_t header, uint8_t len, uint8_t crc, uint8_t iq);
    void setModulationParams(uint8_t sf, uint8_t bw, uint8_t cr, uint8_t ldro);
    void setDioIrqParams(uint16_t irqMask, uint16_t dio1Mask, uint16_t dio2Mask, uint16_t dio3Mask);
    void clearIrqStatus(uint16_t flags);
    uint16_t getIrqStatus();
    void setBufferBaseAddress(uint8_t tx, uint8_t rx);
    void getRxBufferStatus(uint8_t& len, uint8_t& start);
    void getPacketStatus(float &rssi, float &snr);

    // Helpers
    uint8_t mapBandwidth(float bw);
    uint8_t mapCodingRate(uint8_t cr);
    bool needsLDRO(uint8_t sf, uint8_t bwReg);
    void workaround_TxClampConfig();
};

#endif // SX1268_H

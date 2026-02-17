/*
 * SX1268.cpp
 *
 *  Created on: 16 feb 2026
 *      Author: aless
 */


/* Core/Src/SX1268.cpp */
#include "SX1268.h"
#include <string.h> // Per memcpy
#include <stdio.h>

// Costruttore
SX1268::SX1268(SPI_HandleTypeDef* hspi,
       GPIO_TypeDef* csPort, uint16_t csPin,
       GPIO_TypeDef* busyPort, uint16_t busyPin,
       GPIO_TypeDef* resetPort, uint16_t resetPin,
       GPIO_TypeDef* dio1Port, uint16_t dio1Pin)
{
    _hspi = hspi;
    _csPort = csPort; _csPin = csPin;
    _busyPort = busyPort; _busyPin = busyPin;
    _resetPort = resetPort; _resetPin = resetPin;
    _dio1Port = dio1Port; _dio1Pin = dio1Pin;
}

// Low Level: Attesa del pin BUSY
void SX1268::WaitForBusy() {
    uint32_t start = HAL_GetTick();
    while(HAL_GPIO_ReadPin(_busyPort, _busyPin) == GPIO_PIN_SET) {
        if(HAL_GetTick() - start > 1000) {
            printf("[SX1268] BUSY TIMEOUT!\r\n");
            break;
        }
        // Piccola attesa per non saturare il bus
        // osDelay(1) non si può usare dentro sezioni critiche o interrupt, quindi loop vuoto
    }
}

// Low Level: Scrittura Comando SPI
void SX1268::WriteCommand(uint8_t opcode, uint8_t* params, uint8_t paramsLen) {
    WaitForBusy();
    HAL_GPIO_WritePin(_csPort, _csPin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(_hspi, &opcode, 1, 100);
    if(paramsLen > 0) {
        HAL_SPI_Transmit(_hspi, params, paramsLen, 100);
    }
    HAL_GPIO_WritePin(_csPort, _csPin, GPIO_PIN_SET);
    WaitForBusy();
}

// Low Level: Lettura Comando SPI
void SX1268::ReadCommand(uint8_t opcode, uint8_t* status, uint8_t* buffer, uint8_t bufferLen) {
    WaitForBusy();
    HAL_GPIO_WritePin(_csPort, _csPin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(_hspi, &opcode, 1, 100);
    uint8_t nop = 0x00;
    HAL_SPI_TransmitReceive(_hspi, &nop, status, 1, 100); // Legge lo status
    if(bufferLen > 0) {
        HAL_SPI_Receive(_hspi, buffer, bufferLen, 100);
    }
    HAL_GPIO_WritePin(_csPort, _csPin, GPIO_PIN_SET);
    WaitForBusy();
}

void SX1268::WriteBuffer(uint8_t offset, uint8_t* data, uint8_t length) {
    uint8_t buf[2] = {offset};
    WriteCommand(SX126X_CMD_WRITE_BUFFER, buf, 1); // Setup offset
    // Nota: WriteBuffer richiede di tenere CS basso e continuare a scrivere
    // La mia WriteCommand alza CS, quindi implementazione specifica qui sotto:

    WaitForBusy();
    HAL_GPIO_WritePin(_csPort, _csPin, GPIO_PIN_RESET);
    uint8_t opcode = SX126X_CMD_WRITE_BUFFER;
    HAL_SPI_Transmit(_hspi, &opcode, 1, 100);
    HAL_SPI_Transmit(_hspi, &offset, 1, 100);
    HAL_SPI_Transmit(_hspi, data, length, 100);
    HAL_GPIO_WritePin(_csPort, _csPin, GPIO_PIN_SET);
    WaitForBusy();
}

void SX1268::ReadBuffer(uint8_t offset, uint8_t* data, uint8_t length) {
    WaitForBusy();
    HAL_GPIO_WritePin(_csPort, _csPin, GPIO_PIN_RESET);
    uint8_t opcode = SX126X_CMD_READ_BUFFER;
    HAL_SPI_Transmit(_hspi, &opcode, 1, 100);
    HAL_SPI_Transmit(_hspi, &offset, 1, 100);
    uint8_t nop = 0x00;
    HAL_SPI_Transmit(_hspi, &nop, 1, 100); // NOP durante la lettura status (SX126x quirk)
    HAL_SPI_Receive(_hspi, data, length, 100);
    HAL_GPIO_WritePin(_csPort, _csPin, GPIO_PIN_SET);
    WaitForBusy();
}

void SX1268::SetStandby(uint8_t mode) {
    WriteCommand(SX126X_CMD_SET_STANDBY, &mode, 1);
}

// Inizializzazione (Sostituisce radio.begin)
int16_t SX1268::begin(float freq, float bw, uint8_t sf, uint8_t cr, uint8_t syncWord, int8_t power, uint16_t preambleLength, float tcxoVoltage, bool useRegulatorLDO) {
    // 1. Reset Hardware
    HAL_GPIO_WritePin(_resetPort, _resetPin, GPIO_PIN_RESET);
    osDelay(50);
    HAL_GPIO_WritePin(_resetPort, _resetPin, GPIO_PIN_SET);
    osDelay(50);

    WaitForBusy();
    SetStandby(0x00); // STDBY_RC

    // 2. Set Packet Type: LoRa (0x01)
    uint8_t pktType = 0x01;
    WriteCommand(SX126X_CMD_SET_PACKET_TYPE, &pktType, 1);

    // 3. Configurazione RF Frequency
    setFrequency(freq);

    // 4. Configurazione PA (Power Amplifier) e TX Params
    // Configurazione specifica per SX1268 (Low Power PA)
    // paDutyCycle, hpMax, deviceSel, paLut = 1
    uint8_t paConf[4] = {0x04, 0x07, 0x00, 0x01};
    WriteCommand(SX126X_CMD_SET_PA_CONFIG, paConf, 4);
    setOutputPower(power);

    // 5. Modulation Params
    _bw = bw; _sf = sf; _cr = cr;
    SetModulationParams(sf, 0x04, cr, 0x00); // 0x04 = 125kHz, va mappato dinamicamente

    // 6. Packet Params
    _preambleLength = preambleLength;
    SetPacketParams(preambleLength, 0x00, 0xFF, 0x00, 0x00); // Explicit header, CRC on, IQ standard

    // 7. DIO Irq Params (TxDone, RxDone, Timeout)
    // Abilitiamo IRQ su DIO1 per TxDone (bit 0), RxDone (bit 1), Timeout (bit 9)
    uint8_t dioParams[8] = {
        0x00, 0x00, 0x02, 0x03, // IRQ Mask (TxDone | RxDone | Timeout) -> 0x0203 (Bit 9, 1, 0)
        0x00, 0x00, 0x02, 0x03, // DIO1 Mask (idem)
    };
    // Nota: l'endianness nei registri SX126x a 16 bit è MSB first.
    // IRQ map: Bit 0=TxDone, 1=RxDone, 2=PreambleDetected... 9=RxTxTimeout.
    // 0x0203 = Bit 9, Bit 1, Bit 0 set.
    WriteCommand(SX126X_CMD_SET_DIO_IRQ_PARAMS, dioParams, 8);

    return RADIOLIB_ERR_NONE;
}

// Mapping Bandwidth kHz -> Register Value
// 125.0 -> 0x04, 250 -> 0x05, 500 -> 0x06
uint8_t MapBandwidth(float bw) {
    if(bw < 100.0) return 0x03; // 62.5
    if(bw < 200.0) return 0x04; // 125
    if(bw < 400.0) return 0x05; // 250
    return 0x06; // 500
}

void SX1268::SetModulationParams(uint8_t sf, uint8_t bw, uint8_t cr, uint8_t ldro) {
    uint8_t params[8] = {sf, bw, cr, ldro, 0, 0, 0, 0};
    WriteCommand(SX126X_CMD_SET_MODULATION_PARAMS, params, 4); // LoRa usa solo 4 byte
}

void SX1268::SetPacketParams(uint16_t preambleLength, uint8_t crcType, uint8_t payloadLength, uint8_t headerType, uint8_t invertIQ) {
    uint8_t params[6] = {
        (uint8_t)((preambleLength >> 8) & 0xFF),
        (uint8_t)(preambleLength & 0xFF),
        headerType,
        payloadLength,
        crcType,
        invertIQ
    };
    WriteCommand(SX126X_CMD_SET_PACKET_PARAMS, params, 6);
}

int16_t SX1268::setFrequency(float freq) {
    // Freq = (RF * 2^25) / 32MHz
    uint32_t frf = (uint32_t)((freq * 1000000.0) * (33554432.0 / 32000000.0));
    uint8_t buf[4];
    buf[0] = (frf >> 24) & 0xFF;
    buf[1] = (frf >> 16) & 0xFF;
    buf[2] = (frf >> 8) & 0xFF;
    buf[3] = frf & 0xFF;
    WriteCommand(SX126X_CMD_SET_RF_FREQUENCY, buf, 4);
    _freq = freq;
    return RADIOLIB_ERR_NONE;
}

int8_t SX1268::startReceive() {
    // Imposta buffer base address RX a 0
    uint8_t bufBase[2] = {0x00, 0x00}; // TxBase, RxBase
    WriteCommand(SX126X_CMD_SET_BUFFER_BASE_ADDRESS, bufBase, 2);

    // Set RX (Timeout 0 = continuous)
    uint8_t rxParams[3] = {0x00, 0x00, 0x00};
    WriteCommand(SX126X_CMD_SET_RX, rxParams, 3);
    return RADIOLIB_ERR_NONE;
}

int8_t SX1268::startTransmit(uint8_t* data, uint8_t length) {
    SetStandby();

    // Configura IRQ per TxDone e Timeout
    uint8_t dioParams[8] = { 0x00, 0x00, 0x02, 0x01, 0x00, 0x00, 0x02, 0x01 }; // Mask TX Done | Timeout
    WriteCommand(SX126X_CMD_SET_DIO_IRQ_PARAMS, dioParams, 8);

    // Scrivi dati nel buffer (offset 0)
    WriteBuffer(0x00, data, length);

    // Aggiorna Packet Params con la lunghezza giusta
    SetPacketParams(_preambleLength, 0x01, length, 0x00, 0x00); // 0x01 = CRC ON

    // Set TX (Timeout 0 = disable watchdog)
    uint8_t txParams[3] = {0x00, 0x00, 0x00};
    WriteCommand(SX126X_CMD_SET_TX, txParams, 3);
    return RADIOLIB_ERR_NONE;
}

int8_t SX1268::readData(uint8_t* data, uint8_t length) {
    // 1. Get RX Buffer Status (per sapere lunghezza e offset start)
    uint8_t status = 0;
    uint8_t bufStatus[2]; // [0] = PayloadLength, [1] = RxStartBufferPointer
    ReadCommand(SX126X_CMD_GET_RX_BUFFER_STATUS, &status, bufStatus, 2);

    uint8_t rxLen = bufStatus[0];
    uint8_t rxPtr = bufStatus[1];

    if(length < rxLen) rxLen = length; // Protezione buffer overflow

    // 2. Leggi Buffer
    ReadBuffer(rxPtr, data, rxLen);

    // 3. Clear IRQ (Reset flags)
    uint8_t clearIrq[2] = {0xFF, 0xFF}; // Pulisci tutto
    WriteCommand(SX126X_CMD_CLEAR_IRQ_STATUS, clearIrq, 2);

    return RADIOLIB_ERR_NONE;
}

uint8_t SX1268::getPacketLength() {
    uint8_t status = 0;
    uint8_t bufStatus[2];
    ReadCommand(SX126X_CMD_GET_RX_BUFFER_STATUS, &status, bufStatus, 2);
    return bufStatus[0];
}

// Stub functions per compatibilità
int16_t SX1268::setBandwidth(float bw) { _bw = bw; SetModulationParams(_sf, MapBandwidth(bw), _cr, 0); return RADIOLIB_ERR_NONE; }
int16_t SX1268::setSpreadingFactor(uint8_t sf) { _sf = sf; SetModulationParams(sf, MapBandwidth(_bw), _cr, 0); return RADIOLIB_ERR_NONE; }
int16_t SX1268::setCodingRate(uint8_t cr) { _cr = cr; SetModulationParams(_sf, MapBandwidth(_bw), cr, 0); return RADIOLIB_ERR_NONE; }
int16_t SX1268::setOutputPower(int8_t power) {
    uint8_t buf[2] = {(uint8_t)(power), 0x02}; // RampTime 0x02
    WriteCommand(SX126X_CMD_SET_TX_PARAMS, buf, 2);
    return RADIOLIB_ERR_NONE;
}
float SX1268::getRSSI() { return -100.0; /* TODO: Implement PacketStatus */ }
float SX1268::getSNR() { return 10.0; /* TODO: Implement PacketStatus */ }
float SX1268::getFrequencyError() { return 0.0; }

void SX1268::setPacketReceivedAction(void (*func)(void)) { _onPacketReceived = func; }
void SX1268::setPacketSentAction(void (*func)(void)) { _onPacketSent = func; }

// Questa funzione viene chiamata dall'ISR EXTI
void SX1268::handleDio1Irq() {
    // Leggi IRQ Status per sapere cosa è successo
    uint8_t status = 0;
    uint8_t irqStatus[2];
    ReadCommand(SX126X_CMD_GET_IRQ_STATUS, &status, irqStatus, 2);
    uint16_t irq = (irqStatus[0] << 8) | irqStatus[1];

    if(irq & 0x0002) { // RxDone (Bit 1)
        if(_onPacketReceived) _onPacketReceived();
    }
    if(irq & 0x0001) { // TxDone (Bit 0)
        if(_onPacketSent) _onPacketSent();
    }

    // Nota: La pulizia IRQ viene fatta normalmente nel loop principale (readData)
    // o qui se necessario, ma attenzione alla concorrenza SPI dentro l'ISR.
    // Per ora lasciamo che sia il Task RTOS a pulire dopo la notifica.
}


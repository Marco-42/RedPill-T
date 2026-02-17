/**
 * @file    SX1268.cpp
 * @brief   SX1268 Driver Implementation (RadioLib Compatible API)
 */

#include "SX1268.h"

// Costruttore Vuoto
SX1268::SX1268() {
    memset(&_pins, 0, sizeof(_pins));
}

// Costruttore Completo (usato in app.cpp)
SX1268::SX1268(SPI_HandleTypeDef* hspi,
       GPIO_TypeDef* csPort, uint16_t csPin,
       GPIO_TypeDef* busyPort, uint16_t busyPin,
       GPIO_TypeDef* resetPort, uint16_t resetPin,
       GPIO_TypeDef* dio1Port, uint16_t dio1Pin)
{
    _pins.hspi = hspi;
    _pins.nssPort = csPort; _pins.nssPin = csPin;
    _pins.busyPort = busyPort; _pins.busyPin = busyPin;
    _pins.resetPort = resetPort; _pins.resetPin = resetPin;
    _pins.dio1Port = dio1Port; _pins.dio1Pin = dio1Pin;
}

// ==========================================
// PUBLIC API (RADIOLIB STYLE)
// ==========================================

int16_t SX1268::begin(float freq, float bw, uint8_t sf, uint8_t cr, uint16_t syncWord, int8_t power, uint16_t preambleLength, float tcxoVoltage, bool useRegulatorLDO) {
    // 1. Reset Hardware
    hwReset();

    // 2. Wait for BUSY (Critical check)
    if (waitBusy(1500) != 0) return RADIOLIB_ERR_CHIP_NOT_FOUND;

    // 3. Standby RC
    setStandby(0x00); // RC Mode
    if (waitBusy() != 0) return RADIOLIB_ERR_CHIP_NOT_FOUND;

    // 4. TCXO Config
    if (tcxoVoltage > 0.0f) {
        _useTCXO = true;
        // Map voltage (1.6V=0 ... 3.3V=7)
        if (tcxoVoltage < 1.7f) _tcxoVoltage = 0x00; // 1.6V
        else if (tcxoVoltage < 2.0f) _tcxoVoltage = 0x02; // 1.8V
        else _tcxoVoltage = 0x07; // 3.3V

        // Opcode 0x97: SetDIO3AsTcxoCtrl
        uint32_t delay = 5000 / 15.625; // 5ms delay
        uint8_t buf[] = { _tcxoVoltage, (uint8_t)(delay >> 16), (uint8_t)(delay >> 8), (uint8_t)delay };
        spiWrite(0x97, buf, 4);

        // Calibrate (0x89) - Calibrate all (0x7F)
        uint8_t cal = 0x7F;
        spiWrite(0x89, &cal, 1);
        waitBusy(100);
    }

    // 5. Regulator Mode (0x96)
    uint8_t regMode = useRegulatorLDO ? 0x00 : 0x01; // 0=LDO, 1=DCDC
    spiWrite(0x96, &regMode, 1);

    // 6. Packet Type LoRa (0x8A -> 0x01)
    uint8_t pktType = 0x01;
    spiWrite(0x8A, &pktType, 1);

    // 7. Frequency (0x86)
    setFrequency(freq);

    // 8. PA Config & TX Power
    // Per SX1268 +22dBm: paDuty=0x04, hpMax=0x07, devSel=0x00, paLut=0x01
    uint8_t paBuf[] = {0x04, 0x07, 0x00, 0x01};
    spiWrite(0x95, paBuf, 4);
    setOutputPower(power);

    // 9. Modulation Params
    _bw = bw; _sf = sf; _cr = cr;
    uint8_t bwReg = mapBandwidth(bw);
    uint8_t crReg = mapCodingRate(cr);
    bool ldro = needsLDRO(sf, bwReg);
    setModulationParams(sf, bwReg, crReg, ldro ? 1 : 0);

    // 10. Packet Params
    // Preamble, Header(0=Explicit), Payload(255 max), CRC(1=On), IQ(0=Std)
    setPacketParams(preambleLength, 0x00, 255, 0x01, 0x00);

    // 11. Sync Word
    uint8_t swBuf[2] = { (uint8_t)(syncWord >> 8), (uint8_t)(syncWord & 0xFF) };
    writeRegister(0x0740, swBuf[0]);
    writeRegister(0x0741, swBuf[1]);

    // 12. Workarounds
    workaround_TxClampConfig();

    // 13. IRQ Setup (Enable All)
    setDioIrqParams(0x03FF, 0x03FF, 0, 0);

    return RADIOLIB_ERR_NONE;
}

int8_t SX1268::startReceive() {
    // Set Buffer Base
    setBufferBaseAddress(0, 0);

    // Config IRQ for RX
    // RX_DONE, TIMEOUT, CRC_ERR
    setDioIrqParams(0x0262, 0x0262, 0, 0);
    clearIrqStatus(0x03FF);

    // Set RX Continuous (0x82 -> 0xFFFFFF)
    uint8_t buf[] = {0xFF, 0xFF, 0xFF};
    spiWrite(0x82, buf, 3);

    return RADIOLIB_ERR_NONE;
}

int8_t SX1268::readData(uint8_t* data, uint8_t length) {
    // Check Flags
    uint16_t irq = getIrqStatus();

    if (!(irq & 0x0002)) { // RX_DONE not set
        return RADIOLIB_ERR_RX_TIMEOUT;
    }

    if (irq & 0x0040) { // CRC_ERR
        clearIrqStatus(0x03FF);
        return RADIOLIB_ERR_CRC_MISMATCH;
    }

    // Get Payload Length & Start Offset
    uint8_t len = 0;
    uint8_t start = 0;
    getRxBufferStatus(len, start);

    // Safety cap
    if (length < len) len = length;

    // Read Buffer
    readBuffer(start, data, len);

    // Clear Flags
    clearIrqStatus(0x03FF);

    return RADIOLIB_ERR_NONE;
}

int8_t SX1268::startTransmit(uint8_t* data, uint8_t length) {
    if (length > 255) return RADIOLIB_ERR_PACKET_TOO_LONG;

    setStandby(0x00); // Standby RC
    waitBusy();

    // Reset Buffer Base
    setBufferBaseAddress(0, 0);

    // Write Data
    writeBuffer(0, data, length);

    // Update Packet Params (Length)
    // Preamb, Explicit, Len, CRC On, IQ Std
    setPacketParams(8, 0x00, length, 0x01, 0x00);

    // IRQ for TX
    setDioIrqParams(0x0201, 0x0201, 0, 0); // TX_DONE | TIMEOUT
    clearIrqStatus(0x03FF);

    // Set TX (0x83 -> 0 = No Timeout)
    uint8_t buf[] = {0, 0, 0};
    spiWrite(0x83, buf, 3);

    return RADIOLIB_ERR_NONE;
}

int8_t SX1268::transmit(uint8_t* data, uint8_t length, uint32_t timeoutMs) {
    startTransmit(data, length);

    uint32_t start = HAL_GetTick();
    while (true) {
        uint16_t irq = getIrqStatus();
        if (irq & 0x0001) { // TX_DONE
            clearIrqStatus(0x03FF);
            return RADIOLIB_ERR_NONE;
        }
        if (irq & 0x0200) { // TIMEOUT
            return RADIOLIB_ERR_TX_TIMEOUT;
        }
        if (HAL_GetTick() - start > timeoutMs) {
            return RADIOLIB_ERR_TX_TIMEOUT;
        }
    }
}

uint8_t SX1268::getPacketLength() {
    uint8_t len = 0;
    uint8_t start = 0;
    getRxBufferStatus(len, start);
    return len;
}

// --- Setters ---
int16_t SX1268::setFrequency(float freq) {
    _freqMHz = freq;
    uint32_t frf = (uint32_t)(freq * 1000000.0 * 33554432.0 / 32000000.0);
    uint8_t buf[] = { (uint8_t)(frf>>24), (uint8_t)(frf>>16), (uint8_t)(frf>>8), (uint8_t)frf };
    spiWrite(0x86, buf, 4);
    return RADIOLIB_ERR_NONE;
}

int16_t SX1268::setBandwidth(float bw) {
    _bw = bw;
    // Note: This needs SetModulationParams to take effect.
    // Simplified: Just set stored value, user must re-init or we assume they call begin() again?
    // RadioLib setBandwidth re-applies modulation params instantly.
    uint8_t bwReg = mapBandwidth(bw);
    uint8_t crReg = mapCodingRate(_cr);
    bool ldro = needsLDRO(_sf, bwReg);
    setModulationParams(_sf, bwReg, crReg, ldro ? 1 : 0);
    return RADIOLIB_ERR_NONE;
}

int16_t SX1268::setSpreadingFactor(uint8_t sf) {
    _sf = sf;
    uint8_t bwReg = mapBandwidth(_bw);
    uint8_t crReg = mapCodingRate(_cr);
    bool ldro = needsLDRO(sf, bwReg);
    setModulationParams(sf, bwReg, crReg, ldro ? 1 : 0);
    return RADIOLIB_ERR_NONE;
}

int16_t SX1268::setCodingRate(uint8_t cr) {
    _cr = cr;
    uint8_t bwReg = mapBandwidth(_bw);
    uint8_t crReg = mapCodingRate(cr);
    bool ldro = needsLDRO(_sf, bwReg);
    setModulationParams(_sf, bwReg, crReg, ldro ? 1 : 0);
    return RADIOLIB_ERR_NONE;
}

int16_t SX1268::setOutputPower(int8_t power) {
    // 0x8E SetTxParams
    uint8_t buf[] = { (uint8_t)power, 0x04 }; // 0x04 = 200us ramp
    spiWrite(0x8E, buf, 2);
    return RADIOLIB_ERR_NONE;
}

// --- Telemetry ---
float SX1268::getRSSI() {
    float rssi = 0;
    float snr = 0;
    getPacketStatus(rssi, snr);
    return rssi;
}

float SX1268::getSNR() {
    float rssi = 0;
    float snr = 0;
    getPacketStatus(rssi, snr);
    return snr;
}

float SX1268::getFrequencyError() { return 0.0; } // Not implemented

// --- Callbacks ---
void SX1268::setPacketReceivedAction(void (*func)(void)) { _onRxCallback = func; }
void SX1268::setPacketSentAction(void (*func)(void)) { _onTxCallback = func; }

void SX1268::handleDio1Irq() {
    uint16_t flags = getIrqStatus();
    if (flags & 0x0002) { // RX_DONE
        if (_onRxCallback) _onRxCallback();
    }
    if (flags & 0x0001) { // TX_DONE
        if (_onTxCallback) _onTxCallback();
    }
}

// ==========================================
// PRIVATE HELPERS
// ==========================================

void SX1268::hwReset() {
    HAL_GPIO_WritePin(_pins.resetPort, _pins.resetPin, GPIO_PIN_RESET);
    HAL_Delay(20);
    HAL_GPIO_WritePin(_pins.resetPort, _pins.resetPin, GPIO_PIN_SET);
    HAL_Delay(50);
}

int SX1268::waitBusy(uint32_t timeoutMs) {
    uint32_t start = HAL_GetTick();
    while (HAL_GPIO_ReadPin(_pins.busyPort, _pins.busyPin) == GPIO_PIN_SET) {
        if (HAL_GetTick() - start > timeoutMs) return -1;
    }
    return 0;
}

void SX1268::nssLow() { HAL_GPIO_WritePin(_pins.nssPort, _pins.nssPin, GPIO_PIN_RESET); }
void SX1268::nssHigh() { HAL_GPIO_WritePin(_pins.nssPort, _pins.nssPin, GPIO_PIN_SET); }

void SX1268::spiWrite(uint8_t cmd, const uint8_t* data, uint16_t len) {
    waitBusy(); nssLow();
    HAL_SPI_Transmit(_pins.hspi, &cmd, 1, 100);
    if (len > 0) HAL_SPI_Transmit(_pins.hspi, (uint8_t*)data, len, 100);
    nssHigh();
}

void SX1268::spiRead(uint8_t cmd, uint8_t* data, uint16_t len) {
    waitBusy(); nssLow();
    HAL_SPI_Transmit(_pins.hspi, &cmd, 1, 100);
    uint8_t nop = 0;
    HAL_SPI_Transmit(_pins.hspi, &nop, 1, 100); // Status byte
    if (len > 0) HAL_SPI_Receive(_pins.hspi, data, len, 100);
    nssHigh();
}

void SX1268::writeRegister(uint16_t addr, uint8_t data) {
    uint8_t buf[] = { (uint8_t)(addr >> 8), (uint8_t)(addr & 0xFF), data };
    uint8_t op = 0x0D;
    waitBusy(); nssLow();
    HAL_SPI_Transmit(_pins.hspi, &op, 1, 100);
    HAL_SPI_Transmit(_pins.hspi, buf, 3, 100);
    nssHigh();
}

void SX1268::writeBuffer(uint8_t offset, const uint8_t* data, uint8_t len) {
    uint8_t buf[] = { offset };
    uint8_t op = 0x0E;
    waitBusy(); nssLow();
    HAL_SPI_Transmit(_pins.hspi, &op, 1, 100);
    HAL_SPI_Transmit(_pins.hspi, buf, 1, 100);
    HAL_SPI_Transmit(_pins.hspi, (uint8_t*)data, len, 100);
    nssHigh();
}

void SX1268::readBuffer(uint8_t offset, uint8_t* data, uint8_t len) {
    uint8_t buf[] = { offset };
    uint8_t op = 0x1E;
    waitBusy(); nssLow();
    HAL_SPI_Transmit(_pins.hspi, &op, 1, 100);
    HAL_SPI_Transmit(_pins.hspi, buf, 1, 100);
    uint8_t nop = 0;
    HAL_SPI_Transmit(_pins.hspi, &nop, 1, 100); // Status
    HAL_SPI_Receive(_pins.hspi, data, len, 100);
    nssHigh();
}

// --- Commands ---
void SX1268::setStandby(uint8_t mode) { spiWrite(0x80, &mode, 1); }
void SX1268::setPacketParams(uint16_t pre, uint8_t head, uint8_t len, uint8_t crc, uint8_t iq) {
    uint8_t buf[] = { (uint8_t)(pre >> 8), (uint8_t)pre, head, len, crc, iq, 0, 0, 0 };
    spiWrite(0x8C, buf, 9);
}
void SX1268::setModulationParams(uint8_t sf, uint8_t bw, uint8_t cr, uint8_t ldro) {
    uint8_t buf[] = { sf, bw, cr, ldro, 0, 0, 0, 0 };
    spiWrite(0x8B, buf, 8);
}
void SX1268::setDioIrqParams(uint16_t irq, uint16_t d1, uint16_t d2, uint16_t d3) {
    uint8_t buf[] = { (uint8_t)(irq >> 8), (uint8_t)irq, (uint8_t)(d1 >> 8), (uint8_t)d1, 0, 0, 0, 0 };
    spiWrite(0x08, buf, 8);
}
void SX1268::clearIrqStatus(uint16_t flags) {
    uint8_t buf[] = { (uint8_t)(flags >> 8), (uint8_t)flags };
    spiWrite(0x02, buf, 2);
}
uint16_t SX1268::getIrqStatus() {
    uint8_t buf[2] = {0, 0};
    spiRead(0x12, buf, 2);
    return (buf[0] << 8) | buf[1];
}
void SX1268::setBufferBaseAddress(uint8_t tx, uint8_t rx) {
    uint8_t buf[] = { tx, rx };
    spiWrite(0x8F, buf, 2);
}
void SX1268::getRxBufferStatus(uint8_t& len, uint8_t& start) {
    uint8_t buf[2] = {0, 0};
    spiRead(0x13, buf, 2);
    len = buf[0];
    start = buf[1];
}
void SX1268::getPacketStatus(float &rssi, float &snr) {
    uint8_t buf[3];
    spiRead(0x14, buf, 3);
    rssi = -buf[0] / 2.0f;
    snr = (int8_t)buf[1] / 4.0f;
}

// --- Helpers ---
uint8_t SX1268::mapBandwidth(float bw) {
    if (bw < 10.0) return 0x00; // 7.8
    if (bw < 13.0) return 0x08; // 10.4
    if (bw < 20.0) return 0x01; // 15.6
    if (bw < 30.0) return 0x09; // 20.8
    if (bw < 40.0) return 0x02; // 31.25
    if (bw < 60.0) return 0x0A; // 41.7
    if (bw < 100.0) return 0x03; // 62.5
    if (bw < 200.0) return 0x04; // 125
    if (bw < 400.0) return 0x05; // 250
    return 0x06; // 500
}

uint8_t SX1268::mapCodingRate(uint8_t cr) {
    if (cr >= 5) return cr - 4; // RadioLib 5->1, 6->2...
    return 0x01; // Default 4/5
}

bool SX1268::needsLDRO(uint8_t sf, uint8_t bwReg) {
    // Semplificazione: Low Data Rate Optimization consigliata per SF11/12 @ 125k
    if (sf >= 11 && bwReg == 0x04) return true;
    if (sf == 12 && bwReg == 0x05) return true;
    return false;
}

void SX1268::workaround_TxClampConfig() {
    writeRegister(0x08D8, 0x1E);
}

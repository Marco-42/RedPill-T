#include "STM32Hal.h"
#include <cstring>
#include <cstdio> // Added for printf debug

// Arduino Compat Macros
#define INPUT 0
#define OUTPUT 1
#define LOW 0
#define HIGH 1
#define RISING 0
#define FALLING 1

STM32Hal::STM32Hal(SPI_HandleTypeDef* spiHandle)
    : RadioLibHal(INPUT, OUTPUT, LOW, HIGH, RISING, FALLING), _spi(spiHandle) {
    memset(_pinMap, 0, sizeof(_pinMap));
}

void STM32Hal::addPin(uint32_t pinId, GPIO_TypeDef* port, uint16_t pin) {
    if (pinId < MAX_PINS) {
        _pinMap[pinId].port = port;
        _pinMap[pinId].pin = pin;
    }
}

Stm32Pin* STM32Hal::getStmPin(uint32_t pinId) {
    if (pinId < MAX_PINS && _pinMap[pinId].port != NULL) return &_pinMap[pinId];
    return NULL;
}

void STM32Hal::pinMode(uint32_t pin, uint32_t mode) {}

void STM32Hal::digitalWrite(uint32_t pin, uint32_t value) {
    Stm32Pin* p = getStmPin(pin);
    if (p) HAL_GPIO_WritePin(p->port, p->pin, (value == HIGH) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

uint32_t STM32Hal::digitalRead(uint32_t pin) {
    Stm32Pin* p = getStmPin(pin);
    if (p) return (HAL_GPIO_ReadPin(p->port, p->pin) == GPIO_PIN_SET) ? HIGH : LOW;
    return LOW;
}

void STM32Hal::spiBegin() {}
void STM32Hal::spiBeginTransaction() {}

// --- DEBUG SPI TRANSFER ---
void STM32Hal::spiTransfer(uint8_t* out, size_t len, uint8_t* in) {
    uint8_t tempTx[len]; // Using local buffer to avoid corrupting source if out==in
    uint8_t tempRx[len];

    // Prepare Data
    if (out != NULL) memcpy(tempTx, out, len);
    else memset(tempTx, 0x00, len); // Send dummy 0s for reads

    // Perform Transfer
    HAL_StatusTypeDef state = HAL_SPI_TransmitReceive(_spi, tempTx, tempRx, len, 1000);

    // DEBUG PRINT (WARNING: SLOWS DOWN EXECUTION)
    // Only enable this if initialization is failing!
    /*
    printf("[SPI] TX: ");
    for(size_t i=0; i<len; i++) printf("%02X ", tempTx[i]);
    printf(" | RX: ");
    for(size_t i=0; i<len; i++) printf("%02X ", tempRx[i]);
    printf("\r\n");
	*/

    if (state != HAL_OK) {
        printf("[HAL] SPI Error: %d\r\n", state);
    }

    // Copy Result
    if (in != NULL) memcpy(in, tempRx, len);
}

void STM32Hal::spiEndTransaction() {}
void STM32Hal::spiEnd() {}

void STM32Hal::delay(unsigned long ms) { HAL_Delay(ms); }

void STM32Hal::delayMicroseconds(unsigned long us) {
    uint32_t start = HAL_GetTick();
    // Simple blocking delay to avoid overhead of creating a hardware timer for microsecond precision
    while((HAL_GetTick() - start) < (us / 1000 + 1));
}

unsigned long STM32Hal::millis() { return HAL_GetTick(); }
unsigned long STM32Hal::micros() { return HAL_GetTick() * 1000; }
long STM32Hal::pulseIn(uint32_t pin, uint32_t state, unsigned long timeout) { return 0; }

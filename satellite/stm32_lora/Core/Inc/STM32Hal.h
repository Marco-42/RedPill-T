#ifndef STM32HAL_H
#define STM32HAL_H

#include <RadioLib.h>
#include "main.h"

// Struct to map a "RadioLib Pin ID" to an actual STM32 Port/Pin
struct Stm32Pin {
    GPIO_TypeDef* port;
    uint16_t pin;
};

class STM32Hal : public RadioLibHal {
public:
    STM32Hal(SPI_HandleTypeDef* spiHandle);

    // Setup method to register your pins
    void addPin(uint32_t pinId, GPIO_TypeDef* port, uint16_t pin);

    // --- GPIO ---
    void pinMode(uint32_t pin, uint32_t mode) override;
    void digitalWrite(uint32_t pin, uint32_t value) override;
    uint32_t digitalRead(uint32_t pin) override;

    // --- SPI ---
    void spiBegin() override;
    void spiBeginTransaction() override;
    void spiTransfer(uint8_t* out, size_t len, uint8_t* in) override;
    void spiEndTransaction() override;
    void spiEnd() override;

    // --- Time ---
    void delay(unsigned long ms) override;
    void delayMicroseconds(unsigned long us) override;
    unsigned long millis() override;
    unsigned long micros() override;

    // --- Interrupts (REQUIRED for RadioLib v7.5+) ---
    // We leave these empty because we handle interrupts via HAL_GPIO_EXTI_Callback in app.cpp
    void attachInterrupt(uint32_t interruptNum, void (*interruptCb)(void), uint32_t mode) override {};
    void detachInterrupt(uint32_t interruptNum) override {};
    void yield() override {}; // Required for some internal loops

    // --- Pulse (Optional) ---
    long pulseIn(uint32_t pin, uint32_t state, unsigned long timeout) override;

private:
    SPI_HandleTypeDef* _spi;
    static const int MAX_PINS = 8;
    Stm32Pin _pinMap[MAX_PINS];
    Stm32Pin* getStmPin(uint32_t pinId);
};

#endif // STM32HAL_H

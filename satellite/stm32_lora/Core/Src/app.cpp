/* Core/Src/app.cpp */
#include "app.h"

extern SPI_HandleTypeDef hspi1;

// Define LoRa module with STM32 Pins
SX1268 radio(LORA_SPI_HANDLE,
             LORA_CS_PORT,   LORA_CS_PIN,
             LORA_BUSY_PORT, LORA_BUSY_PIN,
             LORA_RST_PORT,  LORA_RST_PIN,
             LORA_DIO1_PORT, LORA_DIO1_PIN
             );

// ---------------------------------------------------------
// INTERRUPT CALLBACKS (Chiamate dal driver HAL in C)
// ---------------------------------------------------------
extern "C" void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	printf("?");
    // Usiamo la macro definita in app.h per coerenza
    if (GPIO_Pin == LORA_DIO1_PIN)
    {
    	printf("!");
        radio.handleDio1Irq();
    }
}

// ---------------------------------------------------------
// GLOBAL HANDLES
// ---------------------------------------------------------
extern osThreadId_t OBCTaskHandle; // Definita in main.c (CubeMX)
TaskHandle_t COMMSTaskHandle = NULL; // Definita qui (Manuale)

// ---------------------------------------------------------
// HELPER FUNCTIONS (Interne C++, niente extern "C")
// ---------------------------------------------------------
void PowerUp_COMMS() {
    if (COMMSTaskHandle != NULL) return;

    printf("[OBC] Avvio task Radio...\n");
    // Creazione manuale della task Radio
    xTaskCreate(COMMS_stateMachine, "COMMSTask", 2048, NULL, osPriorityNormal, &COMMSTaskHandle);
}

void PowerDown_COMMS() {
    if (COMMSTaskHandle == NULL) return;

    printf("[OBC] Spegnimento task Radio...\n");
    vTaskDelete(COMMSTaskHandle);
    COMMSTaskHandle = NULL;
    // Qui andrebbe anche radio.sleep(); per risparmiare corrente
}

// ---------------------------------------------------------
// IL CERVELLO DEL SATELLITE (OBC)
// Chiamata dal main.c -> DEVE avere extern "C"
// ---------------------------------------------------------
extern "C" void app_OBC_loop(void *argument) {

    // === FASE 1: INIZIALIZZAZIONE SISTEMA ===
    printf("\n--- J2050 OBC BOOT SEQUENCE ---\n");

    // 1.1 Creazione Code (Prima degli interrupt!)
    if (RTOS_queue_TX == NULL) {
        RTOS_queue_TX = xQueueCreate(TX_QUEUE_SIZE, sizeof(Packet));
    }
    if (RTOS_queue_cmd == NULL) {
        RTOS_queue_cmd = xQueueCreate(CMD_QUEUE_SIZE, sizeof(Packet));
    }
    printf("[OBC] Memory Structures: OK\n");

    // 1.2 Inizializzazione Hardware Critico
    HAL_GPIO_WritePin(LORA_CS_PORT, LORA_CS_PIN, GPIO_PIN_SET);

    // Check Hardware
    printf("[OBC] Hardware Init: OK\n");

    // === FASE 2: LOOP DI MISSIONE ===
    printf("[OBC] Entering Mission Loop.\n");

    // Boot: Accendiamo la radio
    PowerUp_COMMS();

    for(;;) {
        // --- LOGICA DI CONTROLLO ---

        // Esempio: Heartbeat
        printf("[OBC] Status: Nominal. Radio: %s\n", (COMMSTaskHandle != NULL) ? "ON" : "OFF");

        // Qui in futuro metterai la logica "Se batteria bassa -> PowerDown_COMMS()"

        // Sleep OBC (10 secondi)
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

/*************************************************
ALIMENTAZIONE NECESSARIA A MENO DI 3.3 V 
PROGRAMMA GESTIONE RICEZIONE SLAVE
LORA1262 + ARDUINO NANO
VERSIONE 2.0 AGGIORNATO AL 01 - 02 -2025 
**************************************************

  CONNESSIONI 
  
    Arduino NANO      LoRa1262
       D10(SS)   <------->    NSS
       D13(SCK)      <------->    SCK
       D11(MOSI)     <------->    MOSI
       D12(MISO)     <------->    MISO
       D9      <------->    NRESET
       D8      <------->    BUSY
       D7      <------->    DIO1
       3V3     <------->    VCC
       GND     <------->    GND
     
**************************************************

Il codice seguente fa riferimento all'utilizzo di un sensore DS18B20 per il rilevamento della temperatura e per la ricezione del dato tramite modulo Lora

**************************************************/

#include <SX1262.h>
#include <SPI.h>
#include <SoftwareSerial.h>

 
#define	SLAVE

SX1262 LoRa1262;  //--> Definisco l'oggetto che identifica il modulo
loRa_Para_t lora_para;  //Struct dei parametri--> Parametri di comunciazione(da settare)

uint8_t rx_buf[6];
uint16_t rx_cnt = 0;
uint16_t rx_size = 6;

uint8_t state;


void setup(void) 
{
	bool temp;
	Serial.begin(9600);	

  //Imposto i parametri di comunicazione del modulo Lora
  lora_para.rf_freq    = 432000000; //--> Imposto la frequenza(espressa in Hz), nel nostro caso variabile tra 432 e 438 MHz
  lora_para.tx_power   = 22;  //-9~22 --> Imposto la potenza di trasmissione espressa in dBm(decibel milliwatt) --> Nel nostro caso imposto 22 dBm --> ??
  lora_para.lora_sf    = LORA_SF10; //--> Imposto lo spreading factor 
  lora_para.code_rate  = LORA_CR_4_8; //--> Imposto il code rate bit_effettivi/bit_trasmessi
  lora_para.payload_size = 11;
//  lora_para.setBandwidth =  LORA_BW_125 --> Imposto la lunghezza di banda (QUESTO COMANDO E' DA VERIFICARE) 

  temp = LoRa1262.Init(&lora_para); //--> CARICO I COMANDI SULLA SCHEDA(e verifico l'effettivo caricamento)
  
  if(0 == temp)
  {
    Serial.println("ERRORE CARICAMENTO SCHEDA");
  }
    Serial.println("SLAVE - RICEZIONE");

//Inizializzo il modulo Lora alla ricezione dei dati
	#ifdef SLAVE
		LoRa1262.RxBufferInit(rx_buf,&rx_size);
    LoRa1262.RxInit();   
	#endif
}

void loop(void) 
{
#ifdef SLAVE

  //Attendo ricezione --> se è avvenuta correttamente mando a schermo il valore della temperatura, altrimenti un messaggio di errore
        state = LoRa1262.WaitForIRQ_RxDone();
        if(state)    
        {
	    ++rx_cnt;
	    Serial.print(" ");
            Serial.print(rx_cnt);
            Serial.print(" - Temperature: ");
            Serial.write(rx_buf,rx_size);    //Mando a schermo i dati
            Serial.println();
        }else{
          Serial.println("ERROR")
          }
#endif
}

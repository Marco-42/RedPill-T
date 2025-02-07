/*************************************************
ALIMENTAZIONE NECESSARIA A MENO DI 3.3 V 
PROGRAMMA GESTIONE TRASMISSIONE MASTER 
LORA1262 + ARDUINO NANO
VERSIONE 2.0 AGGIORNATO AL 01 - 02 -2025 
**************************************************
AAAAAAAAAAAAAAAAAAAAAAAAAAAA
  CONNESSIONI 
	
		Arduino NANO			LoRa1262
		   D10(SS)	 <------->    NSS
		   D13(SCK)      <------->    SCK
		   D11(MOSI)     <------->    MOSI
		   D12(MISO)     <------->    MISO
		   D9   	 <------->    NRESET
		   D8   	 <------->    BUSY
		   D7   	 <------->    DIO1
		   3V3   	 <------->    VCC
		   GND   	 <------->    GND
	   
**************************************************

Il codice seguente fa riferimento all'utilizzo di un sensore DS18B20 per il rilevamento della temperatura e per la trasmissione del dato tramite modulo Lora

**************************************************/

//Librerie per il modulo Lora
#include <SX1262.h>
#include <SPI.h>
#include <SoftwareSerial.h>
//Libreria per il sensore di temperatura
#include <OneWire.h>
#include <DallasTemperature.h>

//Definisco il pin e l'oggetto che identificano il sensore
#define ONE_WIRE_BUS 3 //Il sensore viene collegato al pin D3 
OneWire oneWire(ONE_WIRE_BUS); 
//passo la referenza onewire al sensore
DallasTemperature sensors(&oneWire);

SX1262 LoRa1262;	//--> Definisco l'oggetto che identifica il modulo
loRa_Para_t	lora_para;	//Struct dei parametri--> Parametri di comunciazione(da settare)

uint8_t state;
int conta = 0; 
int verifica = 0;
float temperatura = 0;
uint8_t tx[] = "00000";
String vettore;

//CON SCHEDA COLLEGATA AL COMPUTER
void setup() 
{
	bool temp;

  //Inizializzo la comunicazione seriale e la libreria del sensore
	Serial.begin(9600);
  sensors.begin();

   
  //Imposto i parametri di comunicazione del modulo Lora
	lora_para.rf_freq    = 432000000; //--> Imposto la frequenza(espressa in Hz), nel nostro caso variabile tra 432 e 438 MHz
	lora_para.tx_power   = 22;	//-9~22 --> Imposto la potenza di trasmissione espressa in dBm(decibel milliwatt) --> Nel nostro caso imposto 22 dBm --> ??
	lora_para.lora_sf    = LORA_SF10; //--> Imposto lo spreading factor 
	lora_para.code_rate  = LORA_CR_4_8; //--> Imposto il code rate bit_effettivi/bit_trasmessi
	lora_para.payload_size = 11;
//  lora_para.setBandwidth =  LORA_BW_125 --> Imposto la lunghezza di banda (QUESTO COMANDO E' DA VERIFICARE) 

	temp = LoRa1262.Init(&lora_para); //--> CARICO I COMANDI SULLA SCHEDA(e verifico l'effettivo caricamento)
	
	if(0 == temp)
	{
		Serial.println("ERRORE CARICAMENTO SCHEDA");
	}
		Serial.println("MASTER - TRASMISSIONE");
}

//CON SCHEDA SCOLLEGATA DAL COMPUTER
void loop() 
{
  //Acquisisco i dati di temperatura dal sensore(temperatura in Celsius)
  //Il programma ritenta l'acquisizione(per un numero massimo di 5 volte) se il sensore resistituisce valori anomali
  //Altrimenti trasmette al modulo lora 0 come temperatura standard
  while(conta <= 5 && verifica == 0){
  sensors.requestTemperatures(); 
  
  //Se volessimo mettere più sensori in serie servirà cambiare l'indice corrispondente
  temperatura = sensors.getTempCByIndex(0)
  ++conta; 
  
  //Controllo dell'avvenuta presa dati dal sensore [0 - dati non ricavati, 1 - dati ricavati]
  if (isnan(temperatura)) //--> Restituisce "true" se l'argomento contiene elementi NaN
    verifica = 0;
  else
  verifica = 1; 

  delay(10);
  }

  if(verifica == 0)
  temperatura = 0;
  
  
  //Il pacchetto di dati conterrà una stringa con la temperatura misurata con 2 cifre significative dopo la virgola
  vettore += temperatura;
  vettore = vettore.substring(0, 5);

  //Riempio il pacchetto
  for(int i = 0; i < 5; ++i)
  tx[i] = (uint8_t)vettore[i];
  
  verifica = 0;
  //Invio il pacchetto di dati(tx)
  //Pacchetto - Dimensione pacchetto
	LoRa1262.TxPacket(tx,sizeof(tx));

 //LA PARTE SEGUENTE DEL PROGRAMMA ANDREBBE DISCUSSA
 //Ricevo conferma dell'avvenuta trasmissione
 //Se la trasmissione è avvenuta con successo attendo un minuto prima della nuova trasmissione
 //Se la trasmissione è fallita ritento dopo 1 secondo
	state = LoRa1262.WaitForIRQ_TxDone();
	if(state)
	{
	 verifica = 1;
	}

        if(verifica == 1)
        delay(60000);
        else
        delay(1000);
  
}
  

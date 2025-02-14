/*
  Programma di ricezione per il modulo SX1278 usando la libreria RadioLib 
  Scheda usata: TTGO LoRa32 T3_V1.6.1 433 MHz

  Per le impostazioni di default per il modulo visualizzare il seguente link:
  https://github.com/jgromes/RadioLib/wiki/Default-configuration

  Per informazioni sulla libreria:
  https://github.com/jgromes/RadioLib/tree/master

  Librerie necessarie:
  - Esp32
  - Radiolib

  Link del board manager per l'esp32:
  https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
*/

// includo la libreria Radiolib per la gestione della comunicazione
#include <RadioLib.h>

// imposto i pin del modulo lora SX1278
// CS pin:   18
// DIO0 pin:  26
// RESET pin: 23
// DIO1 pin: 33 RADIOLIB_NC
SX1278 radio1 = new Module(18, 26, 23, 33);

// definisco alcune variabili utili per la gestione della comunicazione in modalità interrupt
// la ricezione o la trasmissione dei pacchetti non blocca l'esecuzione del programma

// check_com indica se la ricezione è terminata 
// viene impostata su false se la ricezione deve ancora essere ultimata 
volatile bool check_com = false;

// la seguente riga di codice ottimizza l'esecuzione del programma per l'impiego di esp32
#if defined(ESP8266) || defined(ESP32)
  ICACHE_RAM_ATTR
#endif

// la funzione setFlag imposta come true la variabile check_com quando viene richiamata
void setFlag(void) {
  check_com = true;
}

void setup() {

  // inizializzo la porta seriale
  Serial.begin(9600);

  // inizializzo il modulo SX1278 con i parametri di comunicazione noti
  Serial.print(F("[SX1278] Starting: "));

  // frequency:                   433.0 MHz
  // bandwidth:                   125.0 kHz
  // spreading factor:            10
  // coding rate:                 4/8
  // sync word:                   0x12 --> standard per comunicazioni private
  // output power:                10 dBm --> 10 dBm è impostato per la fase di test(valore da impiegare idealmente: 22 dBm)
  // preamble length:             8 symbols --> stardard
  // amplifier gain:              0 (imposto controllo automatico del gain)
  //frequency - bandwidth - spreading factor - coding rate - sync word - output power -  preamble length - gain
  int state = radio1.begin(433.0, 125.0, 10, 8, 0x12, 10, 8, 1);

  // verifico se l'inizializzazione del modulo è avvenuta correttamente(state memorizza lo stato dell'inizializzazione)
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("Starting procedure COMPLETE"));
  } else {
    Serial.print(F("Starting procedure FAILED"));
    // mando a schermo lo stato di errore dell'inizializzazione
    Serial.println(state);

  // blocco il programma nel caso di fallimento nell'inizializzazione del modulo(fino a quando l'utente non forza il riavvio)
    while (true) { delay(10); }
  }

  // imposto il limite di sicurezza per la corrente a 60 mA --> impostare a 0 mA per annullare il limite di sicurezza
  // verifico se l'impostazione è avvenuta correttamente
  if (radio1.setCurrentLimit(60) == RADIOLIB_ERR_INVALID_CURRENT_LIMIT) {
    Serial.println(F("SETTING FAILED: valore della corrente limite non compatibile con il modello impiegato"));

  //  blocco il programma nel caso di fallimento fino a quando l'utente non forza il riavvio
    while (true) { delay(10); }
  }

  // imposto la funzione che verrà richiamata quando la ricezione finisce
  // se la ricezione si conclude correttamente verrà poi eseguita la funzione setFlag
  radio1.setPacketReceivedAction(setFlag);

  // effettuo la prima ricezione
  Serial.print(F("[SX1278] RICEZIONE: "));
  state = radio1.startReceive();

  // verifico lo stato della ricezione
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("Ricezione avviata con successo"));
  } else {
    Serial.print(F("Ricezione avviata fallita --> Errore: "));
    Serial.println(state);
    // blocco il programma nel caso di fallimento fino a quando l'utente non forza il riavvio
    while (true) { delay(10); }
  }
}

// Eseguo una ricezione(ricevo un pacchetto dati alla volta senza bloccare l'esecuzione del programma)
void loop() {

// leggo il pacchetto ottenuto solo se la ricezione si è conclusa
  if(check_com) {
    // resetto il valore di check_com
    check_com = false;

// mando i dati ricevuti in una stringa 
  String str;
  int state = radio1.readData(str);

// se c'è la necessità di leggere i singoli bit del pacchetto: 
    /*
      byte byteArr[8];
      int numBytes = radio1.getPacketLength();
      int state = radio1.readData(byteArr, numBytes);
    */

// se la ricezione è avvenuta correttamente mando a schermo il pacchetto
  if (state == RADIOLIB_ERR_NONE) {
      // se il pacchetto è stato ricevuto con successo
      Serial.println(F("[SX1278] Ricezione avvenuta"));

      // mando a schermo i dati ricevuti
      Serial.print(F("[SX1278] Dati: "));
      Serial.println(str);

 }else if(state == RADIOLIB_ERR_CRC_MISMATCH){
  // se la ricezione è fallita per la manomissione/corruzione del pacchetto
  Serial.println(F("[SX1278] Corruzione del pacchetto - Ricezione fallita - CRC error"));
 
 }else{
  // se la ricezione è fallita per altri motivi mando a schermo il messaggio d'errore
  Serial.print(F("[SX1278] RICEZIONE FALLITA --> Errore: "));
  Serial.println(state);

 }
}
}
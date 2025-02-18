/*
  Programma di trasmissione/ricezione per il modulo SX1278 usando la libreria RadioLib 
  Non si effettua distinzione tra modulo master e slave
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

// la riga successiva deve essere decomentata solo in uno dei due moduli
// la comunicazione deve iniziare da un solo modulo, altrimenti entrambi comincerebbero in modalità di ricezione
//#define INITIATING_NODE

// imposto i pin del modulo lora SX1278
// CS pin:   18
// DIO0 pin:  26
// RESET pin: 23
// DIO1 pin: 33 RADIOLIB_NC
SX1278 radio1 = new Module(18, 26, 23, 33);


// stato_trasmissione salva lo stato della comunicazione in modalità di trasmissione
int stato_trasmissione = RADIOLIB_ERR_NONE;

// check_com indica lo stato della trasmissione/ricezione
// viene impostata su false nel caso di ricezione e su true nel caso di trasmissione
volatile bool check_com = false;

// check_packet indica se un pacchetto è stato ricevuto/trasmesso
volatile bool check_packet = false;

// la seguente riga di codice ottimizza l'esecuzione del programma per l'impiego di esp32
#if defined(ESP8266) || defined(ESP32)
  ICACHE_RAM_ATTR
#endif

// la funzione setFlag imposta come true la variabile check_packet quando viene richiamata
// ovvero quando viene ultimata la trasmissione/ricezione di un paccheto
void setFlag(void) {
  check_packet = true;
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
    // mando a schermo lo stato di errore della trasmissione
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

  // imposto la funzione che verrà richiamata nella ricezione del pacchetto
  // se la ricezione si conclude correttamente verrà poi eseguita la funzione setFlag
  radio1.setDio0Action(setFlag, RISING);

  // PROCEDURA INIZIALE
  // se il modulo viene definito come INITIATING_NODE allora questo inizia la comunicazione e viene trasmesso il primo pacchetto
  #if defined(INITIATING_NODE)
    Serial.print(F("[SX1278] TRASMISSIONE"));
    
    // Trasmetto il primo pacchetto
    stato_trasmissione = radio1.startTransmit("Hello World!");

    // Quando la trasmissione viene ultimata check_com va impostato su true
    check_com = true;
  
  // se il modulo non viene definito come INITIATING_NODE allora inizio la ricezione
  #else

    Serial.print(F("[SX1278] RICEZIONE"));
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
  #endif
}

void loop() {
  // verifico se la comunicazione precedente è conclusa
  // check_packet viene impostato su true dalla funzione setFlag se il pacchetto è stato ricevuto/trasmesso con successo
  if(check_packet) {
    // resetto check_packet per la comunicazione successiva
    check_packet = false;

    if(check_com) {

      // check_com viene impostato su true nel caso di trasmissione
      // se la comunicazione precedente era una trasmissione e si è conclusa,
      // allora inizio la ricezione e mando a schermo il pacchetto ricevuto

      // verifico lo stato della trasmissione precedente
      if (stato_trasmissione == RADIOLIB_ERR_NONE) {
        // se la trasmissione è avvenuta correttamente
        Serial.println(F("Trasmissione completata"));

      // se la trasmissione è fallita mando a schermo il messaggio d'errore
      } else {
        Serial.print(F("TRASMISSIONE FALLITA - Stato: "));
        Serial.println(stato_trasmissione);

      }

      // inizio la ricezione ed imposto su check_com su false(nel caso di ricezione)
      radio1.startReceive();
      check_com = false;

    } else {
      // se la comunicazione precedente era una ricezione e si è conclusa,
      // allora mando a schermo il pacchetto ricevuto ed inizio la nuova trasmissione
      String str;
      int state = radio1.readData(str);

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

      // attendo 5 secondi da una ricezione alla trasmissione successiva
      delay(5000);

      // effettuo la trasmissione
      Serial.print(F("[SX1278] TRASMISSIONE"));
    
      // Trasmetto il pacchetto
      stato_trasmissione = radio1.startTransmit("Hello World!");
      check_com = true;
    }
  }
}
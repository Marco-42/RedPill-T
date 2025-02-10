/*
  Programma di trasmissione per il modulo SX1278 usando la libreria RadioLib 
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
// NSS pin:   18
// DIO0 pin:  26
// RESET pin: 23
// DIO1 pin: RADIOLIB_NC --> non dato
SX1278 radio1 = new Module(18, 26, 23, RADIOLIB_NC);

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

  // verifico se l'inizializzazione del modulo è avvenuta correttamente
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("Starting procedure COMPLETE"));
  } else {
    Serial.print(F("Starting procedure FAILED"));

  // blocco il programma nel caso di fallimento nell'inizializzazione del modulo(fino a quando l'utente non forza il riavvio)
    while (true) { delay(10); }
  }

  // imposto il limite di sicurezza per la corrente a 60 mA --> impostare a 0 mA per annullare il limite di sicurezza
  // verifico se l'impostazione è avvenuta correttamente
  if (radio1.setCurrentLimit(60) == RADIOLIB_ERR_INVALID_CURRENT_LIMIT) {
    Serial.println(F("Selected current limit is invalid for this module!"));

  //  blocco il programma nel caso di fallimento fino a quando l'utente non forza il riavvio
    while (true) { delay(10); }
  }

}

// contatore pacchetti trasmessi
int conta = 0;

// Eseguo una trasmissione in blocking(trasmetto un pacchetto dati alla volta ed attendo la fine della trasmissione)
void loop() {
  Serial.print(F("[SX1278] Trasmissione: "));

  // creo una stringa contenente un messaggio testuale ed il numero della trasmissione
  String str = "Hello World! " + String(conta++);

  // trasmetto il pacchetto 
  int state = radio1.transmit(str);

  // per gestire il messaggio a partire dai singoli bit: 
  /*
    byte byteArr[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
    int state = radio.transmit(byteArr, 8);
  */
  
  // verifico lo stato della trasmissione del pacchetto
  // durante questa fase il programma si blocca ed aspetta l'avvenuta trasmissione
  if (state == RADIOLIB_ERR_NONE) {
    // se la trasmissione è avvenuta correttamente
    Serial.println(F("Trasmissione completata: "));

    // Mando a schermo le informazioni sulla velocità della trasmissione
    Serial.print(F("[SX1278] Velocità trasmissione:\t"));
    Serial.print(radio1.getDataRate());
    Serial.println(F(" bps"));

   // se la trasmissione è fallita mando a schermo il messaggio d'errore
  } else if (state == RADIOLIB_ERR_PACKET_TOO_LONG) {
    // Se il pacchetto dati è troppo lungo(supera i 256 bit)
    Serial.println(F("Trasmissione fallita(>256bit)"));

  } else if (state == RADIOLIB_ERR_TX_TIMEOUT) {
    // Se la trasmissione non si verifica correttamente avviene il timeout della trasmissione
    Serial.println(F("Trasmissione fallita(TIMEOUT)"));

  } else {
    // se si sono verificati altri errori durante la trasmissione
    Serial.print(F("TRASMISSIONE FALLITA - Stato: "));
    Serial.println(state);

  }

  // Tra una trasmissione e l'altra attendo 5 secondi
  delay(5000);



}

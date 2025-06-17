#include <SPI.h>
#include <SD.h>



// ---- SD-Karte (SPI) ----
#define SD_CS_PIN    5  // CS→GPIO5, MOSI→23, MISO→19, SCK→18
#define MOSI_PIN    23
#define MISO_PIN    19
#define SCK_PIN     18





void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n>> Starte ESP32 mit SD-Card Logging");


  // ---- SD-Karte mounten ----
  Serial.print("SD-Karte initialisieren...");
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println(" FEHLER!");
    delay(1000);
  }
  else {
    Serial.println(" OK");
  }


  Serial.print("SD-Karte Typ: ");
  switch (SD.cardType()) {
    case CARD_NONE:
      Serial.println(" Keine Karte");
      break;
    case CARD_MMC:
      Serial.println(" MMC");
      break;
    case CARD_SD:
      Serial.println(" SDSC");
      break;
    case CARD_SDHC:
      Serial.println(" SDHC");
      break;
    case CARD_UNKNOWN:
      Serial.println(" unbekannt");
      break;
  }
  Serial.print("SD-Karte Größe: ");
  uint64_t cardSize = SD.cardSize() / (1024 * 1024); // Size in MB
  Serial.print(cardSize);
  Serial.println(" MB");

  Serial.print("SD-Karte verfügbarer Speicher: ");
  uint64_t freeSpace = SD.totalBytes() - SD.usedBytes();
  freeSpace /= (1024 * 1024); // Convert to MB
  Serial.print(freeSpace);
  Serial.println(" MB");

}


void loop() {
  
  
}

#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>
#include <Wire.h>

// WLAN
#define SSID "UniFi"
#define PASSPHRASE "lassmichbitterein"

// Webserver
#define START_SIDE "/index.html"

// Pinbelegung SPI
#define MISO 19 // Data In
#define MOSI 23 // Data Out
#define SCK 18  // Clock

#define SD_CS 5 // Chip Select (CS) für die SD-Karte

// Pinbelegung I2C
#define SDA 21 // Data Line
#define SCL 22 // Clock Line

#define BOOT_BUTTON 0 // Boot Button Pin
volatile bool bootPressed = false;
volatile uint32_t lastInterrupt = 0;

#define LED_BUILTIN 13 // Onboard LED Pin

AsyncWebServer server(80);

bool recording = false;

void setupWebServer();
void mountSDCard();
void connectWifi();
void listFilesRecursiveJson(File dir, File &outFile, const String &path);
bool saveJsonListing(const char *outputPath);

void IRAM_ATTR onBootPress() {
  uint32_t now = millis();
  if (now - lastInterrupt < 500)
    return;
  lastInterrupt = now;
  bootPressed = true;
}

void setup() {
  Serial.begin(115200);

  // ---- I2C initialisieren ----
  Wire.begin(SDA, SCL, 400000); // Initialize I2C with custom pins
  Serial.println("I2C initialized");

  // ---- SPI initialisieren ----
  SPI.begin(SCK, MISO, MOSI, SD_CS); // Initialize SPI with custom pins
  Serial.println("SPI initialized");

  connectWifi();                // WLAN verbinden
  mountSDCard();                // SD-Karte mounten
  setupWebServer();             // Webserver einrichten
  pinMode(LED_BUILTIN, OUTPUT); // LED Pin als Output
  pinMode(BOOT_BUTTON,
          INPUT_PULLUP); // Boot Button Pin als Input mit Pull-Up Widerstand
  attachInterrupt(digitalPinToInterrupt(BOOT_BUTTON), onBootPress,
                  FALLING); // Interrupt für Boot Button
}

void connectWifi() {
  // Initialize WiFi
  WiFi.begin(SSID, PASSPHRASE);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }

  Serial.println("Connected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void mountSDCard() {
  Serial.print("SD-Karte initialisieren...");
  if (!SD.begin(SD_CS, SPI, 4000000)) {
    Serial.println(" FEHLER!");
    delay(1000);
  } else {
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

  saveJsonListing("/list.json");
}

void listFilesRecursiveJson(File dir, File &outFile, const String &path) {
  bool firstEntry = true;

  // Stelle sicher, dass wir am Verzeichnisanfang lesen
  dir.rewindDirectory();

  while (true) {
    File entry = dir.openNextFile();
    if (!entry)
      break;

    // Relativen Namen extrahieren
    String name = String(entry.name()).substring(path.length());

    // Skip hidden files/folders (Name beginnt mit '.')
    if (name.length() > 0 && name.charAt(0) == '.') {
      entry.close();
      continue;
    }

    // Nur zwischen den Einträgen Komma drucken
    if (!firstEntry) {
      outFile.print(",");
    }
    firstEntry = false;

    // JSON-Objekt starten
    outFile.print("{\"name\":\"");
    outFile.print(name);
    outFile.print("\",\"type\":\"");
    outFile.print(entry.isDirectory() ? "folder" : "file");
    outFile.print("\"");

    // Bei Ordnern rekursiv in children
    if (entry.isDirectory()) {
      outFile.print(",\"children\":[");
      String newPath = String(entry.name()) + "/";

      // Unterordner frisch öffnen, damit openNextFile() neu startet
      File subDir = SD.open(entry.name());
      if (subDir && subDir.isDirectory()) {
        listFilesRecursiveJson(subDir, outFile, newPath);
        subDir.close();
      }

      outFile.print("]");
    }

    // Objekt beenden
    outFile.print("}");
    entry.close();
  }
}

// Erstellt das JSON-Haupt-Array ohne versteckte Einträge
bool saveJsonListing(const char *outputPath) {
  File root = SD.open("/");
  if (!root || !root.isDirectory()) {
    return false;
  }

  File jsonFile = SD.open(outputPath, FILE_WRITE);
  if (!jsonFile) {
    root.close();
    return false;
  }

  jsonFile.print("[");
  listFilesRecursiveJson(root, jsonFile, "");
  jsonFile.print("]");

  jsonFile.close();
  root.close();
  return true;
}

void setupWebServer() {
  server.onNotFound([](AsyncWebServerRequest *request) {
    String path = request->url();
    printf("Anfrage für Pfad: %s\n", path.c_str());
    if (path == "/") {
      request->redirect(START_SIDE);
      return;
    }

    if (path.indexOf('.') == 0) {
      request->send(404, "text/plain", "404: File Not Found");
      return;
    }

    if (!SD.exists(path)) {
      request->send(404, "text/plain", "404: File Not Found");
      return;
    }

    File file = SD.open(path, FILE_READ);
    if (file) {

      String contentType = "text/plain";
      if (path.endsWith(".html"))
        contentType = "text/html";
      else if (path.endsWith(".css"))
        contentType = "text/css";
      else if (path.endsWith(".js"))
        contentType = "application/javascript";
      else if (path.endsWith(".png"))
        contentType = "image/png";
      else if (path.endsWith(".jpg") || path.endsWith(".jpeg"))
        contentType = "image/jpeg";
      else if (path.endsWith(".gif"))
        contentType = "image/gif";
      else if (path.endsWith(".ico"))
        contentType = "image/x-icon";
      else if (path.endsWith(".xml"))
        contentType = "text/xml";
      else if (path.endsWith(".pdf"))
        contentType = "application/pdf";

      AsyncWebServerResponse *response =
          request->beginResponse(SD, path, contentType);
      request->send(response);
      file.close();
    } else {
      request->send(500, "text/plain", "500: Fehler beim Öffnen der Datei");
    }
  });

  server.on("/list.json", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SD, "/list.json", "application/json");
  });

  server.on(
      "/upload", HTTP_POST,
      [](AsyncWebServerRequest *request) {
        saveJsonListing("/list.json");
        request->send(200, "text/plain", "Upload abgeschlossen");
      },
      [](AsyncWebServerRequest *request, String filename, size_t index,
         uint8_t *data, size_t len, bool final) {
        static File uploadFile;

        if (index == 0) {
          String path = "/" + filename;
          if (SD.exists(path))
            SD.remove(path);
          uploadFile = SD.open(path, FILE_WRITE);
        }

        if (uploadFile) {
          uploadFile.write(data, len);
        }

        if (final && uploadFile) {
          uploadFile.close();
        }
      });

  server.begin();
  Serial.println("Webserver gestartet");
}

void loop() {

  if (bootPressed) {
    Serial.println("BOOT-Button gedrückt!");
    recording = !recording; // Toggle recording state
    if (recording) {
      Serial.println("Aufnahme gestartet");
      digitalWrite(LED_BUILTIN, HIGH); // LED einschalten
    } else {
      Serial.println("Aufnahme gestoppt");
      digitalWrite(LED_BUILTIN, LOW); // LED einschalten
    }
    delay(50);
    bootPressed = false;
  }
}
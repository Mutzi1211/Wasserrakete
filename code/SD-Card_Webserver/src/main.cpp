#include "SparkFun_BNO08x_Arduino_Library.h"
#include <Arduino.h>
#include <AsyncEventSource.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <SparkFunBME280.h>
#include <SparkFun_MAX1704x_Fuel_Gauge_Arduino_Library.h>
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

File csvFile;

// Pinbelegung I2C
#define SDA 21 // Data Line
#define SCL 22 // Clock Line

#define INT_PIN 32
#define RST_PIN 14

#define BNO08x_EASY_MODE true
#define BNO08X_ADDR 0x4B

#define BOOT_BUTTON 0 // Boot Button Pin
volatile bool bootPressed = false;
volatile uint32_t lastInterrupt = 0;

#define SMALL_LED 13 // Onboard LED Pin

BME280 bme280;
bool bme280_active = false;

BNO08x bno086;
bool bno086_active = false;

SFE_MAX1704X lipo;
bool lipo_active = false;

AsyncWebServer server(80);
AsyncEventSource events("/events");

bool recording = false;
#define PUBLISH_INTERVAL 100 // Zeitintervall für die Veröffentlichung von Daten in Millisekunden
uint32_t nextMicros;

struct LogEntry {
  uint32_t timestamp;                    // Zeitstempel der Aufnahme
  float temp, hum, pres;                 // BME280 Messwerte
  float yaw, pitch, roll;                // BNO08x Euler-Winkel in Grad
  float linAccelX, linAccelY, linAccelZ; // BNO08x lineare Beschleunigung
  float accelX, accelY, accelZ;          // BNO08x Beschleunigung
  float soc, voltage;                    // MAX17048 Ladezustand und Spannung
};

void setupWebServer();
void mountSD();
void connectWifi();
void listFilesRecursiveJson(File dir, File &outFile, const String &path);
bool saveJsonListing(const char *outputPath);

bool bme280Init() {
  if (!bme280.begin()) {
    Serial.println("BME280 initialisierung fehlgeschlagen!");
    return false;
  }
  Serial.println("BME280 initialisiert");
  return true;
}

bool bno086Init() {

  if (BNO08x_EASY_MODE) {
    Serial.print("Initialisiere BNO08x im easy Mode ...");
    if (!bno086.begin()) {

      Serial.println("BNO08x initialisierung fehlgeschlagen!");
      return false;
    }
    Serial.println("BNO08x initialisiert im easy Mode");
  } else {
    Serial.print("Initialisiere BNO08x im advanced Mode ...");
    pinMode(RST_PIN, OUTPUT);
    digitalWrite(RST_PIN, LOW);
    delay(10);
    digitalWrite(RST_PIN, HIGH);
    delay(500);

    if (!bno086.begin(BNO08X_ADDR, Wire, INT_PIN, RST_PIN)) {
      Serial.println("BNO08x initialisierung fehlgeschlagen!");
      return false;
    }
  }

  Serial.println("BNO08x initialisiert");

  bno086.enableRotationVector(PUBLISH_INTERVAL);
  Serial.println(" OK – RotationVector aktiviert");
  bno086.enableAccelerometer(PUBLISH_INTERVAL);
  Serial.println(" OK – Accelerometer aktiviert");
  bno086.enableLinearAccelerometer(PUBLISH_INTERVAL);
  Serial.println(" OK – LinearAccelerometer aktiviert");

  return true;
}

bool lipoInit() {
  if (!lipo.begin()) {
    Serial.println("MAX17048 initialisierung fehlgeschlagen!");
    return false;
  }
  Serial.println("MAX17048 initialisiert");

  // Optional: QuickStart-Reset (force full recalibration)
  lipo.quickStart();
  // Read and print the reset indicator
  Serial.print(F("Reset Indicator was: "));
  bool RI = lipo.isReset(true); // Read the RI flag and clear it automatically if it is set
  Serial.println(RI); // Print the RI
  // If RI was set, check it is now clear
  if (RI)
  {
    Serial.print(F("Reset Indicator is now: "));
    RI = lipo.isReset(); // Read the RI flag
    Serial.println(RI); // Print the RI    
  }

  // To quick-start or not to quick-start? That is the question!
  // Read the following and then decide if you do want to quick-start the fuel gauge.
  // "Most systems should not use quick-start because the ICs handle most startup problems transparently,
  //  such as intermittent battery-terminal connection during insertion. If battery voltage stabilizes
  //  faster than 17ms then do not use quick-start. The quick-start command restarts fuel-gauge calculations
  //  in the same manner as initial power-up of the IC. If the system power-up sequence is so noisy that the
  //  initial estimate of SOC has unacceptable error, the system microcontroller might be able to reduce the
  //  error by using quick-start."
  // If you still want to try a quick-start then uncomment the next line:
	//lipo.quickStart();

  // Read and print the device ID
  Serial.print(F("Device ID: 0x"));
  uint8_t id = lipo.getID(); // Read the device ID
  if (id < 0x10) Serial.print(F("0")); // Print the leading zero if required
  Serial.println(id, HEX); // Print the ID as hexadecimal

  // Read and print the device version
  Serial.print(F("Device version: 0x"));
  uint8_t ver = lipo.getVersion(); // Read the device version
  if (ver < 0x10) Serial.print(F("0")); // Print the leading zero if required
  Serial.println(ver, HEX); // Print the version as hexadecimal

  // Read and print the battery threshold
  Serial.print(F("Battery empty threshold is currently: "));
  Serial.print(lipo.getThreshold());
  Serial.println(F("%"));

	// We can set an interrupt to alert when the battery SoC gets too low.
	// We can alert at anywhere between 1% and 32%:
	lipo.setThreshold(20); // Set alert threshold to 20%.

  // Read and print the battery empty threshold
  Serial.print(F("Battery empty threshold is now: "));
  Serial.print(lipo.getThreshold());
  Serial.println(F("%"));

  // Read and print the high voltage threshold
  Serial.print(F("High voltage threshold is currently: "));
  float highVoltage = ((float)lipo.getVALRTMax()) * 0.02; // 1 LSb is 20mV. Convert to Volts.
  Serial.print(highVoltage, 2);
  Serial.println(F("V"));

  // Set the high voltage threshold
  lipo.setVALRTMax((float)4.1); // Set high voltage threshold (Volts)

  // Read and print the high voltage threshold
  Serial.print(F("High voltage threshold is now: "));
  highVoltage = ((float)lipo.getVALRTMax()) * 0.02; // 1 LSb is 20mV. Convert to Volts.
  Serial.print(highVoltage, 2);
  Serial.println(F("V"));

  // Read and print the low voltage threshold
  Serial.print(F("Low voltage threshold is currently: "));
  float lowVoltage = ((float)lipo.getVALRTMin()) * 0.02; // 1 LSb is 20mV. Convert to Volts.
  Serial.print(lowVoltage, 2);
  Serial.println(F("V"));

  // Set the low voltage threshold
  lipo.setVALRTMin((float)3.9); // Set low voltage threshold (Volts)

  // Read and print the low voltage threshold
  Serial.print(F("Low voltage threshold is now: "));
  lowVoltage = ((float)lipo.getVALRTMin()) * 0.02; // 1 LSb is 20mV. Convert to Volts.
  Serial.print(lowVoltage, 2);
  Serial.println(F("V"));

  // Enable the State Of Change alert
  Serial.print(F("Enabling the 1% State Of Change alert: "));
  if (lipo.enableSOCAlert())
  {
    Serial.println(F("success."));
  }
  else
  {
    Serial.println(F("FAILED!"));
  }
  
  // Read and print the HIBRT Active Threshold
  Serial.print(F("Hibernate active threshold is: "));
  float actThr = ((float)lipo.getHIBRTActThr()) * 0.00125; // 1 LSb is 1.25mV. Convert to Volts.
  Serial.print(actThr, 5);
  Serial.println(F("V"));

  // Read and print the HIBRT Hibernate Threshold
  Serial.print(F("Hibernate hibernate threshold is: "));
  float hibThr = ((float)lipo.getHIBRTHibThr()) * 0.208; // 1 LSb is 0.208%/hr. Convert to %/hr.
  Serial.print(hibThr, 3);
  Serial.println(F("%/h"));

  return true;
}

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
  Wire.begin(SDA, SCL);
  Serial.println("I2C initialized");

  // ---- SPI initialisieren ----
  SPI.begin(SCK, MISO, MOSI, SD_CS);
  Serial.println("SPI initialized");

  connectWifi();
  mountSD();

  bme280_active = bme280Init();
  bno086_active = bno086Init();
  lipo_active = lipoInit();

  setupWebServer();

  pinMode(SMALL_LED, OUTPUT);
  pinMode(BOOT_BUTTON, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BOOT_BUTTON), onBootPress, FALLING);
}

void connectWifi() {
  // Initialize WiFi
  WiFi.begin(SSID, PASSPHRASE);
  Serial.print("\nConnecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }

  Serial.println("\nConnected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void mountSD() {
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

  server.addHandler(&events);

  server.on("/list.json", HTTP_GET, [](AsyncWebServerRequest *request) {
    saveJsonListing("/list.json");
    request->send(SD, "/list.json", "application/json");
  });

  // ganz unten in setupWebServer():
  server.on("/battery", HTTP_GET, [](AsyncWebServerRequest *request) {
    // JSON zusammenbauen
    String payload = "{";
    payload += "\"soc\":" + String(lipo.getSOC(), 1) + ",";
    payload += "\"voltage\":" + String(lipo.getVoltage(), 3) + ",";
    payload += "\"changeRate\":" + String(lipo.getChangeRate(), 2) + ",";
    payload += "\"alert\":" + String(lipo.getAlert()) + ",";
    payload += "\"voltageHighAlert\":" + String(lipo.isVoltageHigh()) + ",";
    payload += "\"voltageLowAlert\":" + String(lipo.isVoltageLow()) + ",";
    payload += "\"emptyAlert\":" + String(lipo.isLow()) + ",";
    payload += "\"soc1PercentChangeAlert\":" + String(lipo.isChange()) + ",";
    payload += "\"hibernating\":" + String(lipo.isHibernating());
    payload += "}";

    File jsonFile = SD.open("/battery.json", FILE_WRITE);
    if (jsonFile) {
      jsonFile.print(payload);
      jsonFile.close();
    }

    request->send(200, "application/json", payload);
  });

  server.on(
      "/upload", HTTP_POST,
      [](AsyncWebServerRequest *request) {
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

        saveJsonListing("/list.json");
      });

  server.on("/delete", HTTP_DELETE, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("path")) {
      return request->send(400, "text/plain", "Fehlender Parameter: path");
    }
    String path = request->getParam("path")->value();
    if (!path.startsWith("/")) {
      path = "/" + path;
    }

    if (SD.exists(path)) {
      if (SD.remove(path)) {
        saveJsonListing("/list.json");
        request->send(200, "text/plain", "Datei gelöscht");
      } else {
        request->send(500, "text/plain", "Löschen fehlgeschlagen");
      }
    } else {
      request->send(404, "text/plain", "Datei nicht gefunden");
    }
  });

  server.onNotFound([](AsyncWebServerRequest *request) {
    String path = request->url();
    printf("Anfrage für Pfad: %s\n", path.c_str());

    saveJsonListing("/list.json");

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

  server.begin();
  Serial.println("Webserver gestartet");
}

void record() {

  LogEntry entry;
  entry.timestamp = micros();
  if (bme280_active) {
    entry.temp = bme280.readTempC();
    entry.hum = bme280.readFloatHumidity();
    entry.pres = bme280.readFloatPressure() / 100.0; // Convert to hPa
  } else {
    entry.temp = entry.hum = entry.pres = 0.0;
  }

  entry.yaw = entry.pitch = entry.roll = 0.0f;
  entry.accelX = entry.accelY = entry.accelZ = 0.0f;
  entry.linAccelX = entry.linAccelY = entry.linAccelZ = 0.0f;

  if (bno086_active) {
    if (bno086.getSensorEvent()) {
      uint8_t id = bno086.getSensorEventID();
      switch (id) {

      // Rotation
      case SENSOR_REPORTID_ROTATION_VECTOR:
      case SENSOR_REPORTID_GAME_ROTATION_VECTOR:
        entry.yaw = bno086.getYaw() * RAD_TO_DEG;
        entry.pitch = bno086.getPitch() * RAD_TO_DEG;
        entry.roll = bno086.getRoll() * RAD_TO_DEG;
        break;

      // Roh-Beschleunigung (inkl. Gravitation)
      case SENSOR_REPORTID_ACCELEROMETER:
        entry.accelX = bno086.getAccelX();
        entry.accelY = bno086.getAccelY();
        entry.accelZ = bno086.getAccelZ();
        break;

      // Lineare Beschleunigung (ohne Gravitation)
      case SENSOR_REPORTID_LINEAR_ACCELERATION:
        entry.linAccelX = bno086.getLinAccelX();
        entry.linAccelY = bno086.getLinAccelY();
        entry.linAccelZ = bno086.getLinAccelZ();
        break;

      default:
        // für andere Sensor-Typen ggf. ignorieren
        break;
      }
    } else {
      // Keine neuen Daten
      entry.yaw = entry.pitch = entry.roll = 0.0f;
      entry.accelX = entry.accelY = entry.accelZ = 0.0f;
      entry.linAccelX = entry.linAccelY = entry.linAccelZ = 0.0f;
    }
  } else {
    // Sensor inaktiv
    entry.yaw = entry.pitch = entry.roll = 0.0f;
    entry.accelX = entry.accelY = entry.accelZ = 0.0f;
    entry.linAccelX = entry.linAccelY = entry.linAccelZ = 0.0f;
  }

  entry.yaw = entry.pitch = entry.roll = 0.0; // Keine Daten verfügbar

  if (lipo_active) {
    entry.soc = lipo.getSOC();
    entry.voltage = lipo.getVoltage();
  } else {
    entry.soc = entry.voltage = 0.0;
  }

  if (recording) {
    if (csvFile) {
      csvFile.print(entry.timestamp);
      csvFile.print(",");
      csvFile.print(entry.temp);
      csvFile.print(",");
      csvFile.print(entry.hum);
      csvFile.print(",");
      csvFile.print(entry.pres);
      csvFile.print(",");
      csvFile.print(entry.yaw);
      csvFile.print(",");
      csvFile.print(entry.pitch);
      csvFile.print(",");
      csvFile.print(entry.roll);
      csvFile.print(",");
      csvFile.print(entry.linAccelX);
      csvFile.print(",");
      csvFile.print(entry.linAccelY);
      csvFile.print(",");
      csvFile.print(entry.linAccelZ);
      csvFile.print(",");
      csvFile.print(entry.accelX);
      csvFile.print(",");
      csvFile.print(entry.accelY);
      csvFile.print(",");
      csvFile.print(entry.accelZ);
      csvFile.print(",");
      csvFile.print(entry.soc);
      csvFile.print(",");
      csvFile.print(entry.voltage);
      csvFile.println();
      csvFile.flush(); // Daten sofort auf die SD-Karte schreiben
    }
  }
}

bool check_parachute() {

  if (bme280_active) {
    static float lastAltitude = 0.0;
    float currentAltitude = bme280.readFloatAltitudeMeters();

    if (lastAltitude == 0.0) {
      lastAltitude = currentAltitude;
    }

    if (currentAltitude < lastAltitude - 1.0) {
      lastAltitude = currentAltitude;
      return true;
    }

    lastAltitude = currentAltitude;
    return false;
  }

  return true;
}

void deploy_parachute() {
  Serial.println("Fallschirm wird ausgelöst!");

  // hier soll code für den Solanoid hin
}

void loop() {
  if (bootPressed) {
    Serial.println("BOOT-Button gedrückt!");
    recording = !recording; // Toggle recording state
    if (recording) {
      Serial.println("Aufnahme gestartet");
      digitalWrite(SMALL_LED, HIGH); // LED einschalten

      // irgendeine Magie die dafür sorgt, dass die neuste Datei den größten
      // Index hat
      uint16_t maxIndex = 0;
      File root = SD.open("/");
      while (true) {
        File entry = root.openNextFile();
        if (!entry)
          break;

        if (!entry.isDirectory()) {
          const char *fullName = entry.name();
          const char *fn = fullName[0] == '/' ? fullName + 1 : fullName;
          if (strncmp(fn, "log_", 4) == 0 && strlen(fn) == 4 + 4 + 4 &&
              fn[8] == '.' && tolower(fn[9]) == 'c' && tolower(fn[10]) == 's' &&
              tolower(fn[11]) == 'v') {
            char num[5] = {fn[4], fn[5], fn[6], fn[7], 0};
            uint16_t idx = atoi(num);
            if (idx > maxIndex)
              maxIndex = idx;
          }
        }

        entry.close();
      }
      root.close();

      uint16_t newIndex = maxIndex + 1;
      char filename[16];
      sprintf(filename, "/log_%04d.csv", newIndex);
      Serial.print("Erstelle neue Datei: ");
      Serial.println(filename);

      csvFile = SD.open(filename, FILE_WRITE);

      csvFile.println("time_us,temp_C,hum_pct,press_hPa,yaw_deg,pitch_deg,roll_deg,linAccelX,linAccelY,linAccelZ,accelX,accelY,accelZ,soc_pct,voltage_V");
      csvFile.flush();

      // erstes Timing
      nextMicros = micros() + PUBLISH_INTERVAL * 1000;

    } else {
      Serial.println("Aufnahme gestoppt");
      digitalWrite(SMALL_LED, LOW); // LED einschalten

      events.send("update", "file-changed", millis());
      if (csvFile) {
        csvFile.close();
        csvFile = File(); // Reset file handle
      }
    }
    delay(50);
    bootPressed = false;
  }

  if (recording) {
    uint32_t now = micros();
    // prüfen, ob wir den nächsten 100 Hz-Zeitpunkt erreicht haben
    if ((int32_t)(now - nextMicros) >= 0) {
      // nächstes Zeitfenster buchen
      nextMicros += PUBLISH_INTERVAL * 1000;

      // Daten aufnehmen
      record();

      // Falls Schirm auslösen …
      if (check_parachute()) {
        deploy_parachute();
      }
    }
  }
}
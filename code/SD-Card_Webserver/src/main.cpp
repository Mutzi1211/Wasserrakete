// #include "SparkFun_BNO08x_Arduino_Library.h"
#include <Adafruit_BNO08x.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
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

#define BNO08X_ADDR 0x4B

// OLED Display
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define OLED_RESET -1    // Reset pin # (or -1 if sharing Arduino reset pin)

#define BOOT_BUTTON 0 // Boot Button Pin
volatile bool bootPressed = false;
volatile uint32_t lastInterrupt = 0;

#define SMALL_LED 13 // Onboard LED Pin

BME280 bme280;
bool bme280_active = false;

Adafruit_BNO08x bno086;
bool bno086_active = false;

SFE_MAX1704X lipo;
bool lipo_active = false;

AsyncWebServer server(80);
AsyncEventSource events("/events");

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool oled_active = false;

// 8×8 WLAN-Icon (monochrom)
static const unsigned char wifi_icon[] PROGMEM = {
    0b00011000, //
    0b01111110, //
    0b11000011, //
    0b00111100, //
    0b11000011, //
    0b00111100, //
    0b00011000, //
};
// 8×8 REC-Icon (monochrom)
static const unsigned char rec_icon[] PROGMEM = {0x00, 0x3C, 0x7E, 0xDB,
                                                 0xDB, 0x7E, 0x3C, 0x00};

bool recording = false;
#define PUBLISH_INTERVAL                                                       \
  100 // Zeitintervall für die Veröffentlichung von Daten in Millisekunden
uint32_t nextMicros;

float lastAltitude = 0.0;

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
  Serial.print("Initialisiere BNO08x ...");

  // I²C-Init (alternativ: begin_SPI(...))
  if (!bno086.begin_I2C(BNO08X_ADDR, &Wire)) {
    Serial.println("FEHLER: BNO08x init auf I2C fehlgeschlagen!");
    return false;
  }
  Serial.println("OK – BNO08x initialisiert");

 uint32_t iu = PUBLISH_INTERVAL * 1000UL; // 100 ms → 100000 µs

  // Nur Game Rotation Vector (0x08)
  if (!bno086.enableReport(SH2_GAME_ROTATION_VECTOR, iu))
    Serial.println("FEHLER: GameRotVec nicht aktivierbar");
  else
    Serial.println(" OK – GameRotationVector aktiviert");

  // Roh-Beschl. (0x01)
  if (!bno086.enableReport(SH2_ACCELEROMETER,       iu))
    Serial.println("FEHLER: Accelerometer nicht aktivierbar");
  else
    Serial.println(" OK – Accelerometer aktiviert");

  // Lineare Beschl. (0x04)
  if (!bno086.enableReport(SH2_LINEAR_ACCELERATION, iu))
    Serial.println("FEHLER: LinearAccel nicht aktivierbar");
  else
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

  return true;
}

bool oledInit() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");
    return false;
  }
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Init");
  display.println("Success!");
  display.display();
  return true;
}

void show_display(String text) {

  if (!oled_active)
    return;

  display.clearDisplay();
  // WLAN-Icon oben rechts
  if (WiFi.status() == WL_CONNECTED) {
    display.drawBitmap(SCREEN_WIDTH - 8, 0, wifi_icon, 8, 8, SSD1306_WHITE);
  }
  // REC-Icon unten links
  if (recording) {
    display.drawBitmap(SCREEN_WIDTH - 20, 0, rec_icon, 8, 8, SSD1306_WHITE);
  }
  // Text oder Akkustand/Höhe
  display.setTextSize(2);
  display.setCursor(0, 0);
  if (!WiFi.status() == WL_CONNECTED && !recording) {
    // Nur Text anzeigen
    display.print(text);
  } else if (recording) {
    // im Recording-Modus zusätzlich Text
    display.print(text);
    display.print("Recording \n");
  } else {
    // ansonsten Akku und Höhe abwechselnd
    static unsigned long lastSwitch = 0;
    static bool showBattery = true;
    if (millis() - lastSwitch > 2000) {
      showBattery = !showBattery;
      lastSwitch = millis();
    }
    if (showBattery)
      display.print("Bat: \n" + String(lipo.getSOC()) + "%");
    else
      display.printf("Alt: %dm", lastAltitude);
  }
  display.display();
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

  oled_active = oledInit();

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

  // BME280
  if (bme280_active) {
    entry.temp = bme280.readTempC();
    entry.hum = bme280.readFloatHumidity();
    entry.pres = bme280.readFloatPressure() / 100.0; // hPa
  } else {
    entry.temp = entry.hum = entry.pres = 0.0f;
  }

  // Default-Werte, falls nichts kommt
  entry.yaw = entry.pitch = entry.roll = 0.0f;
  entry.accelX = entry.accelY = entry.accelZ = 0.0f;
  entry.linAccelX = entry.linAccelY = entry.linAccelZ = 0.0f;

  if (bno086_active) {
    sh2_SensorValue_t sv;
    // FIFO‐Leeren und Daten übernehmen
    while (bno086.getSensorEvent(&sv)) {
      switch (sv.sensorId) {
        case SH2_GAME_ROTATION_VECTOR: {
          // Quaternion → Euler
          float qi = sv.un.gameRotationVector.i;
          float qj = sv.un.gameRotationVector.j;
          float qk = sv.un.gameRotationVector.k;
          float qw = sv.un.gameRotationVector.real;
          entry.yaw   = atan2f(2*(qw*qk + qi*qj),
                               1 - 2*(qj*qj + qk*qk)) * 180.0f / M_PI;
          entry.pitch = asinf (2*(qw*qj - qk*qi))       * 180.0f / M_PI;
          entry.roll  = atan2f(2*(qw*qi + qj*qk),
                               1 - 2*(qi*qi + qj*qj)) * 180.0f / M_PI;
          break;
        }
        case SH2_ACCELEROMETER:
          entry.accelX = sv.un.accelerometer.x;
          entry.accelY = sv.un.accelerometer.y;
          entry.accelZ = sv.un.accelerometer.z;
          break;
        case SH2_LINEAR_ACCELERATION:
          entry.linAccelX = sv.un.linearAcceleration.x;
          entry.linAccelY = sv.un.linearAcceleration.y;
          entry.linAccelZ = sv.un.linearAcceleration.z;
          break;
        default:
          break;
      }
    }
  }

  // LiPo
  if (lipo_active) {
    entry.soc = lipo.getSOC();
    entry.voltage = lipo.getVoltage();
  } else {
    entry.soc = entry.voltage = 0.0f;
  }

  // Ins CSV schreiben
  if (recording && csvFile) {
    csvFile.print(entry.timestamp);
    csvFile.print(',');
    csvFile.print(entry.temp);
    csvFile.print(',');
    csvFile.print(entry.hum);
    csvFile.print(',');
    csvFile.print(entry.pres);
    csvFile.print(',');
    csvFile.print(entry.yaw);
    csvFile.print(',');
    csvFile.print(entry.pitch);
    csvFile.print(',');
    csvFile.print(entry.roll);
    csvFile.print(',');
    csvFile.print(entry.linAccelX);
    csvFile.print(',');
    csvFile.print(entry.linAccelY);
    csvFile.print(',');
    csvFile.print(entry.linAccelZ);
    csvFile.print(',');
    csvFile.print(entry.accelX);
    csvFile.print(',');
    csvFile.print(entry.accelY);
    csvFile.print(',');
    csvFile.print(entry.accelZ);
    csvFile.print(',');
    csvFile.print(entry.soc);
    csvFile.print(',');
    csvFile.print(entry.voltage);
    csvFile.println();
    csvFile.flush();
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

  if (lipo_active) {
    show_display("");
  }

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

      csvFile.println("time_us,temp_C,hum_pct,press_hPa,yaw_deg,pitch_deg,roll_"
                      "deg,linAccelX,linAccelY,linAccelZ,accelX,accelY,accelZ,"
                      "soc_pct,voltage_V");
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
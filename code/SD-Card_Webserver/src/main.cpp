// #include "SparkFun_BNO08x_Arduino_Library.h"
#include <Adafruit_BNO08x.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>

#include "ICM_20948.h"
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

// // WLAN
// #define SSID "UniFi"
// #define PASSPHRASE "lassmichbitterein"

// WLAN
#define SSID "THAir"
#define PASSPHRASE "RoboterUSG1!"

// WLAN AP
#define SSID_AP "ESP32"
#define PASSPHRASE_AP "12345678"

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
float max_altitude = 0.0f;
float min_altitude = 3000.0f;

ICM_20948_I2C icm; // I2C Objekt
bool icm_active = false;

SFE_MAX1704X lipo;
bool lipo_active = false;

AsyncWebServer server(80);
AsyncEventSource events("/events");

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool oled_active = false;

bool parachute_deployed = false;
int last_parachute = 0;

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

struct LogEntry {
  uint32_t timestamp;                    // Zeitstempel der Aufnahme
  float temp, hum, pres, altitude;       // BME280 Messwerte
  float yaw, pitch, roll;                // BNO08x Euler-Winkel in Grad
  float linAccelX, linAccelY, linAccelZ; // BNO08x lineare Beschleunigung
  float accelX, accelY, accelZ;          // BNO08x Beschleunigung
  float soc, voltage;                    // MAX17048 Ladezustand und Spannung
};

// Servo parameters and LEDC configuration
const int servoPin = 15;     // GPIO15
const int freq = 50;         // 50 Hz for servos
const int channel = 0;       // LEDC channel 0
const int resolution = 16;   // 16-bit resolution
const int minPulseUs = 500;  // Minimum pulse width in microseconds
const int maxPulseUs = 2400; // Maximum pulse width in microseconds

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

bool initICM() {
  if (icm.begin()) {

    return false;
  }
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
  if (text != "") {
    display.print(text);
  } else {
    display.print("Bat: \n" + String(lipo.getSOC()) + "%");
  }

  display.display();
}

uint32_t pulseUsToDuty(int pulse_us) {
  // duty = (pulse_us / (1e6 / freq)) * (2^resolution)
  float periodUs = 1e6 / freq;
  float dutyFraction = pulse_us / periodUs;
  return (uint32_t)(dutyFraction * ((1 << resolution) - 1));
}

void initServo() {
  ledcSetup(channel, freq, resolution);
  ledcAttachPin(servoPin, channel);

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

  bme280_active = bme280Init();

  icm_active = initICM();

  lipo_active = lipoInit();

  show_display("");

  initServo();

  // connectWifi();

  mountSD();

  // setupWebServer();

  pinMode(SMALL_LED, OUTPUT);
  pinMode(BOOT_BUTTON, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BOOT_BUTTON), onBootPress, FALLING);

  bootPressed = true;
  show_display("Launch -> ");
}

void connectWifi() {
  // Initialize WiFi
  WiFi.begin(SSID, PASSPHRASE);
  Serial.print("\nConnecting to WiFi...");

  for (int i = 0; i < 5; i++) {
    delay(1000);
    Serial.print(".");
  }

  if (WiFi.status() != WL_CONNECTED) {

    Serial.println("FEHLER: WiFi nicht verbunden");

    WiFi.softAP(SSID_AP, PASSPHRASE_AP);
    Serial.println("Own AP started");

    return;

  } else {

    Serial.println("\nConnected to WiFi");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  }
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

void setServoAngle(int angle) {
  // Constrain angle 0-180
  angle = constrain(angle, 0, 180);
  // Map angle to pulse width
  int pulse = map(angle, 0, 180, minPulseUs, maxPulseUs);
  uint32_t duty = pulseUsToDuty(pulse);
  ledcWrite(channel, duty);
}

void quaternionToEuler(float q0, float q1, float q2, float q3, float &roll,
                       float &pitch, float &yaw) {
  roll = atan2(2.0f * (q0 * q1 + q2 * q3), 1.0f - 2.0f * (q1 * q1 + q2 * q2));
  pitch = asin(2.0f * (q0 * q2 - q3 * q1));
  yaw = atan2(2.0f * (q0 * q3 + q1 * q2), 1.0f - 2.0f * (q2 * q2 + q3 * q3));

  // Umrechnen in Grad
  roll *= 180.0f / PI;
  pitch *= 180.0f / PI;
  yaw *= 180.0f / PI;
}

void record() {
  LogEntry entry;
  entry.timestamp = micros();

  // BME280
  if (bme280_active) {
    entry.temp = bme280.readTempC();
    entry.hum = bme280.readFloatHumidity();
    entry.pres = bme280.readFloatPressure() / 100.0; // hPa
    entry.altitude = bme280.readFloatAltitudeMeters();
  } else {
    entry.temp = entry.hum = entry.pres = 0.0f;
  }

  // ICM-20948
  if (icm_active && icm.dataReady()) {

    icm.getAGMT(); // wichtige Funktion!
    entry.accelX = icm.accX();
    entry.accelY = icm.accY();
    entry.accelZ = icm.accZ();
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
    csvFile.print(entry.altitude);
    csvFile.print(',');
    csvFile.print(entry.accelX);
    csvFile.print(',');
    csvFile.print(entry.accelY);
    csvFile.print(',');
    csvFile.print(entry.accelZ);
    csvFile.print(',');
    csvFile.print(entry.voltage);
    csvFile.println();
    csvFile.flush();
  }
}

bool check_parachute() {

  bool zeroG = false;

  if (icm_active) {
    icm.getAGMT(); // wichtige Funktion!
    float x, y, z;

    x = icm.accX();
    y = icm.accY();
    z = icm.accZ();


    if (z < 200) {
      zeroG = true;
    } else {
      zeroG = false;
    }
  }

  if (bme280_active) {

    float pressure = bme280.readFloatPressure();
    float altitude = bme280.readFloatAltitudeMeters();

    // Serial.println("Altitude: " + String(altitude) + "m");
    // Serial.println("Max Altitude: " + String(max_altitude) + "m");

    if (altitude < min_altitude) {
      min_altitude = altitude;
    }

    if (altitude > max_altitude) {
      max_altitude = altitude;
      return false;
    }

    if (zeroG && altitude < max_altitude - 0.1) {
      return true;
    }
  }
  return false;
}

void deploy_parachute() {
  Serial.println("Fallschirm wird ausgelöst!");
  // Center
  setServoAngle(90);
  Serial.println("Angle: 90°");
  delay(1000);
  // Left
  setServoAngle(30);
  Serial.println("Angle: 0°");
  delay(1000);
  // Back to center

  parachute_deployed = true;
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

      csvFile.println("time_us,temp_C,hum_pct,press_hPa,altitude_m,accelX,"
                      "accelY,accelZ,voltage_V");
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

  if (parachute_deployed) {
    if (millis() - last_parachute > 10000) {
      parachute_deployed = false;
      max_altitude = 0;
      min_altitude = 1000;
      Serial.println("Fallschirm zurückgesetzt");
      show_display("Launch ->");
    }
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
        if (!parachute_deployed) {
          deploy_parachute();
          last_parachute = millis();
          float height = max_altitude - min_altitude;
          show_display("Hoehe: \n" + String(height));
        }
      }
    }
  }
}
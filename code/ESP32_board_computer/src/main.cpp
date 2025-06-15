#include <Arduino.h>
#include <WiFi.h>
#include <SPI.h>
#include <SD.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

// WLAN-Zugangsdaten
const char* ssid = "UniFi";
const char* password = "lassmichbitterein";

// Definiere die Pins für den benutzerdefinierten SPI-Bus
#define PIN_SPI_MISO 19  // Data In
#define PIN_SPI_MOSI 23  // Data Out
#define PIN_SPI_SCK 18   // Clock
#define SD_CS_PIN 5      // Chip Select (CS) für die SD-Karte

SPIClass mySPI(VSPI);

// Webserver-Port
AsyncWebServer server(80);

// ======= Beispiel-Funktionen =======
void ledBlink(int duration) {
  Serial.printf("ledBlink(%d)\n", duration);
  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);
  delay(duration);
  digitalWrite(13, LOW);
  delay(duration);
}

void relaisOn(int duration) {
  Serial.printf("relaisOn(%d)\n", duration);
  pinMode(6, OUTPUT);
  digitalWrite(6, HIGH);
  delay(duration);
  digitalWrite(6, LOW);
}

void sensorRead(int analogPin) {
  Serial.printf("sensorRead(%d)\n", analogPin);
  int value = analogRead(analogPin);
  Serial.println("Sensorwert: " + String(value));
  delay(200);
}

void setupWebServer() {
  server.onNotFound([](AsyncWebServerRequest* request) {
    String path = request->url();
    if (path == "/") {
      path = "/dashboard.html";
    }

    if (!SD.exists(path)) {
      request->send(404, "text/plain", "404: Datei nicht gefunden");
      return;
    }

    File file = SD.open(path, FILE_READ);
    if (file) {
      String contentType = "text/plain";
      if (path.endsWith(".html")) contentType = "text/html";
      else if (path.endsWith(".css")) contentType = "text/css";
      else if (path.endsWith(".js")) contentType = "application/javascript";
      else if (path.endsWith(".png")) contentType = "image/png";
      else if (path.endsWith(".jpg") || path.endsWith(".jpeg")) contentType = "image/jpeg";
      else if (path.endsWith(".gif")) contentType = "image/gif";
      else if (path.endsWith(".ico")) contentType = "image/x-icon";
      else if (path.endsWith(".xml")) contentType = "text/xml";
      else if (path.endsWith(".pdf")) contentType = "application/pdf";

      AsyncWebServerResponse* response = request->beginResponse(SD, path, contentType);
      request->send(response);
      file.close();
    } else {
      request->send(500, "text/plain", "500: Fehler beim Öffnen der Datei");
    }
  });

  // Sequenz empfangen und ausführen
server.on("/run_sequence", HTTP_POST, [](AsyncWebServerRequest *request) {},
  NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, data, len);
    if (error) {
      request->send(400, "text/plain", "Ungültiges JSON");
      return;
    }

    JsonArray setup = doc["setup"];
    JsonArray loop = doc["loop"];

    for (JsonObject step : setup) {
      String name = step["name"];
      JsonObject params = step["params"];

    Serial.println("name: " + name);
    Serial.println("params: " + params);

      if (name == "ledBlink") {
        ledBlink(params["duration"]);
      } else if (name == "relaisOn") {
        relaisOn(params["duration"]);
      } else if (name == "sensorRead") {
        sensorRead(params["analogPin"]);
      }
    }

    request->send(200, "text/plain", "Sequenz empfangen und ausgeführt.");
});


  server.begin();
  Serial.println("Async Webserver gestartet.");
}


// ======= Setup =======
void setup() {
  Serial.begin(115200);

  Serial.println("Starte SD-Karten Test...");

  // Initialisiere den benutzerdefinierten SPI-Bus mit den definierten Pins
  mySPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, SD_CS_PIN);

  // SD-Karten-Initialisierung über den benutzerdefinierten SPI-Bus
  if (!SD.begin(SD_CS_PIN, mySPI)) {
    Serial.println("SD-Karten Initialisierung fehlgeschlagen!");
    return;
  }
  Serial.println("SD-Karte erfolgreich initialisiert.");

  // WLAN verbinden
  WiFi.begin(ssid, password);
  Serial.print("Verbinde mit WLAN");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWLAN verbunden: " + WiFi.localIP().toString());

 

  // Root: index.html aus SD
  setupWebServer();
}




// ======= Loop nicht benötigt =======
void loop() {}

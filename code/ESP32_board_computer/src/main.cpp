#include <Arduino.h>
#include <WiFi.h>
#include <SPI.h>
#include <SD.h>
#include <Update.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoOTA.h>
#include <WebSocketsServer.h>
#include <stdarg.h>

const char* ssid = "UniFi";
const char* password = "lassmichbitterein";
#define SD_CS_PIN 5

AsyncWebServer server(80);
WebSocketsServer webSocket(81);

// --- Logging mit printf-Stil ---
void serialLog(const char* format, ...) {
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  Serial.println(buffer);
  webSocket.broadcastTXT(buffer);
}

// WebSocket Callback (nicht genutzt, aber nötig)
void handleWebSocket(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {}

void listFiles(AsyncWebServerRequest *request) {
  String output = "[";
  File root = SD.open("/");
  while (File file = root.openNextFile()) {
    if (output != "[") output += ",";
    output += "\"" + String(file.name()) + "\"";
    file.close();
  }
  output += "]";
  request->send(200, "application/json", output);
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  serialLog("WLAN verbunden: %s", WiFi.localIP().toString().c_str());

  if (!SD.begin(SD_CS_PIN)) {
    serialLog("SD-Karte nicht gefunden");
    return;
  }

  ArduinoOTA.begin();
  webSocket.begin();
  webSocket.onEvent(handleWebSocket);

  // --- Datei-Upload mit automatischem Überschreiben ---
  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) {
    request->send(200);
  }, [](AsyncWebServerRequest *request, String filename, size_t index,
        uint8_t *data, size_t len, bool final) {
    static File file;
    if (index == 0) {
      String path = "/" + filename;
      if (SD.exists(path)) SD.remove(path);
      file = SD.open(path, FILE_WRITE);
    }
    if (file) file.write(data, len);
    if (final && file) file.close();
  });

  // --- Datei löschen ---
  server.on("/delete", HTTP_DELETE, [](AsyncWebServerRequest *request) {
    if (request->hasParam("name", true)) {
      String name = request->getParam("name", true)->value();
      if (SD.exists("/" + name)) SD.remove("/" + name);
    }
    request->send(200);
  });

  server.on("/list", HTTP_GET, listFiles);

  // --- SD-Karte als Webroot, index.html als Startseite ---
  server.serveStatic("/", SD, "/").setDefaultFile("index.html");

  server.begin();
}

void loop() {
  ArduinoOTA.handle();
  webSocket.loop();

  // Beispielausgabe
  static unsigned long lastLog = 0;
  static int i = 0;
  if (millis() - lastLog > 1000) {
    lastLog = millis();
    serialLog("Batterie: %d%%", i++);
  }
}

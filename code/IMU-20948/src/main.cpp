#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <SD.h>
#include "ICM_20948.h"
#include <MadgwickAHRS.h>

// WLAN-Zugangsdaten
const char* ssid     = "UniFi";
const char* password = "lassmichbitterein";

// I²C-Pins
#define SDA_PIN 21
#define SCL_PIN 22

// SD-Karten-Pins
#define SD_CS    5
#define SD_MOSI 23
#define SD_MISO 19
#define SD_CLK   18

ICM_20948_I2C imu;
Madgwick filter;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Gyroskop-Bias (in °/s)
float gx_bias = 0, gy_bias = 0, gz_bias = 0;

void onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
               AwsEventType type, void* arg, uint8_t* data, size_t len) {
  // keine speziellen WebSocket-Events benötigt
}

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);

  // WLAN verbinden
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
  }
  Serial.println();
  Serial.print("IP-Adresse: "); Serial.println(WiFi.localIP());

  // SD-Karte initialisieren
  if (!SD.begin(SD_CS)) {
    Serial.println("SD-Karte nicht gefunden!");
    while (1) delay(100);
  }
  Serial.println("SD-Karte bereit.");

  // IMU initialisieren
  if (imu.begin(Wire, 0x68) != ICM_20948_Stat_Ok) {
    Serial.println("IMU nicht gefunden!");
    while (1) delay(100);
  }
  // Filter initialisieren (Standardrate)
  filter.begin(50.0f);

  // WebSocket-Handler
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  // Kalibrierungs-Endpoint
  server.on("/calibrate", HTTP_GET, [](AsyncWebServerRequest* req) {
    Serial.println("Starte Gyro-Kalibrierung...");
    const int samples = 100;
    int count = 0;
    float sx = 0, sy = 0, sz = 0;
    while (count < samples) {
      if (imu.dataReady()) {
        imu.getAGMT();
        sx += imu.gyrX(); sy += imu.gyrY(); sz += imu.gyrZ();
        count++;
      }
      yield();
    }
    gx_bias = sx / samples;
    gy_bias = sy / samples;
    gz_bias = sz / samples;
    Serial.printf("Gyro-Bias: %.3f, %.3f, %.3f °/s\n", gx_bias, gy_bias, gz_bias);
    req->send(200, "text/plain", "Kalibrierung abgeschlossen");
  });

  // Root-Handler: index.html aus SD senden
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(SD, "/index.html", "text/html");
  });
  // Statische Assets
  server.serveStatic("/style.css", SD, "/style.css");
  server.serveStatic("/script.js", SD, "/script.js");

  // Server starten
  server.begin();
}

void loop() {
  // IMU-Daten verarbeiten
  if (imu.dataReady()) {
    imu.getAGMT();
    float ax = imu.accX(), ay = imu.accY(), az = imu.accZ();
    float gx = (imu.gyrX() - gx_bias) * DEG_TO_RAD;
    float gy = (imu.gyrY() - gy_bias) * DEG_TO_RAD;
    float gz = (imu.gyrZ() - gz_bias) * DEG_TO_RAD;
    float mx = imu.magX(), my = imu.magY(), mz = imu.magZ();

    filter.update(gx, gy, gz, ax, ay, az, mx, my, mz);
    float pitch = filter.getPitch();
    float roll  = filter.getRoll();
    float yaw   = filter.getYaw();

    // JSON-Paket senden
    char buf[64];
    int len = snprintf(buf, sizeof(buf), "{\"pitch\":%.2f,\"roll\":%.2f,\"yaw\":%.2f}",
                       pitch, roll, yaw);
    if (len > 0) ws.textAll(buf);
  }
}

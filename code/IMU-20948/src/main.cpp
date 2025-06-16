#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <SD.h>
#include "ICM_20948.h"
#include <MadgwickAHRS.h>

// WLAN
const char* ssid     = "UniFi";
const char* password = "lassmichbitterein";

// I²C-Pins
#define SDA_PIN 21
#define SCL_PIN 22

// SD-Pins
#define SD_CS   5
#define SD_MOSI 23
#define SD_MISO 19
#define SD_CLK  18

ICM_20948_I2C imu;
Madgwick filter;

AsyncWebServer server(80);
AsyncWebSocket  ws("/ws");

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client,
               AwsEventType type, void * arg, uint8_t *data, size_t len) {
  // keine Events nötig
}

void setup(){
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);

  // WLAN verbinden
  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("IP: "); Serial.println(WiFi.localIP());

  // SD-Karte
  if(!SD.begin(SD_CS)){
    Serial.println("SD-Karte nicht gefunden!");
    while(1) delay(100);
  }
  Serial.println("SD-Karte bereit.");

  // IMU initialisieren
  if(imu.begin(Wire, 0x68) != ICM_20948_Stat_Ok){
    Serial.println("IMU nicht gefunden!");
    while(1) delay(100);
  }
  filter.begin(50.0f);

  // WebSocket
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  // Static Files von SD: /www/index.html, CSS/JS/etc.
  server.serveStatic("/", SD, "/www/").setDefaultFile("index.html");

  server.begin();
}

void loop(){
  static uint32_t last = millis();
  if(imu.dataReady()){
    imu.getAGMT();
    float ax = imu.accX(), ay = imu.accY(), az = imu.accZ();
    float gx = imu.gyrX()*DEG_TO_RAD, gy = imu.gyrY()*DEG_TO_RAD, gz = imu.gyrZ()*DEG_TO_RAD;
    float mx = imu.magX(),   my = imu.magY(),   mz = imu.magZ();

    filter.update(gx, gy, gz, ax, ay, az, mx, my, mz);
    float pitch = filter.getPitch();
    float roll  = filter.getRoll();
    float yaw   = filter.getYaw();

    String msg = "{\"pitch\":" + String(pitch,2) +
                 ",\"roll\":"  + String(roll,2)  +
                 ",\"yaw\":"   + String(yaw,2)   + "}";
    ws.textAll(msg);
  }

  uint32_t now = millis();
  int32_t diff = 20 - (now - last);
  if(diff > 0) delay(diff);
  last = now;
}

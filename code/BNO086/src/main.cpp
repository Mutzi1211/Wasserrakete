#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SparkFun_BNO08x_Arduino_Library.h>
#include <SparkFunBME280.h>

// ---- I²C-Pins ----
#define SDA_PIN 21
#define SCL_PIN 22

// ---- SD-Karte (SPI) ----
#define SD_CS_PIN 5 // CS→GPIO5, MOSI→23, MISO→19, SCK→18

// ---- IMU-Pins & Adresse ----
#define INT_PIN 14
#define RST_PIN 32
#define BNO08X_ADDR 0x4B

// ---- WLAN (Station Mode) ----
const char *ssid = "UniFi";
const char *password = "lassmichbitterein";

// ---- Sensor-Objekte ----
BME280 myBME;
BNO08x myIMU;

// ---- Web-Server & WebSocket ----
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// ---- Offset für relative Orientierung ----
bool initOrientationSet = false;
float initYaw = 0, initPitch = 0, initRoll = 0;

// ── Timing für festen Send-Rhythmus ────────────────────────────
static uint32_t nextSend = 0;
static const uint32_t SEND_INTERVAL = 50; // ms, z.B. 20 Hz

// ── Caches für zuletzt gelesene Sensorwerte ────────────────────
// Euler-Winkel
static float yaw = 0.0f;
static float pitch = 0.0f;
static float roll = 0.0f;
// Roh-Beschleunigung
static float accelX = 0.0f;
static float accelY = 0.0f;
static float accelZ = 0.0f;
// Lineare Beschleunigung
static float linX = 0.0f;
static float linY = 0.0f;
static float linZ = 0.0f;



// Optional: zum Messen der Abstände
static uint32_t lastMillis = 0;

// ---- WebSocket-Events ----
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    if (type == WS_EVT_CONNECT)
    {
        Serial.printf("WS Client #%u verbunden\n", client->id());
    }
    else if (type == WS_EVT_DISCONNECT)
    {
        Serial.printf("WS Client #%u getrennt\n", client->id());
    }
}

void setup()
{
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n>> Starte ESP32 Async-Webserver mit SD-Serving");

    // ---- I2C init ----
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(400000);

    // ---- WLAN verbinden ----
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.print("Verbinde mit WLAN ");
    Serial.print(ssid);
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print('.');
        delay(500);
    }
    Serial.println();
    Serial.print("WLAN verbunden, IP: ");
    Serial.println(WiFi.localIP());

    // ---- SD-Karte mounten ----
    Serial.print("SD-Karte initialisieren...");
    if (!SD.begin(SD_CS_PIN))
    {
        Serial.println(" FEHLER!");
        while (1)
            delay(1000);
    }
    Serial.println(" OK");

    // ---- WebSocket konfigurieren ----
    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    // ---- index.html auf "/" ----
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *req)
              { req->send(SD, "/index.html", "text/html"); });

    // ---- alle anderen URLs: nur existierende Dateien ausliefern ----
    server.onNotFound([](AsyncWebServerRequest *req)
                      {
    String path = req->url();    // z.B. "/static/three.min.js" oder "/rocket-model.glb"
    if (SD.exists(path)) {
      req->send(SD, path, String());  // MIME-Type automatisch
    } else {
      req->send(404);
    } });

    // ---- Server starten ----
    server.begin();
    Serial.println("Server läuft: HTTP / und WS /ws");

    // ---- IMU Hardware-Reset & Init ----
    pinMode(RST_PIN, OUTPUT);
    digitalWrite(RST_PIN, LOW);
    delay(10);
    digitalWrite(RST_PIN, HIGH);
    delay(500);

    Serial.print("Initialisiere BNO08x...");
    if (!myIMU.begin(BNO08X_ADDR, Wire))
    {   
        while (1)
        {
           delay(1000);
        }
        
        Serial.println(" FEHLER! BNO08x ");
    }
    Serial.println(" OK");
    myIMU.enableRotationVector();
    Serial.println("RotationVector aktiviert");

    myIMU.enableAccelerometer();
    Serial.println("Accelerometer aktiviert");
    myIMU.enableLinearAccelerometer();
    Serial.println("LinearAccelerometer aktiviert");

    Serial.print("Initialisiere BME280...");
    if (!myBME.beginI2C())
    {
        Serial.println(" FEHLER! BME280 ");
        while (1)
            delay(1000);
    }
    Serial.println(" OK");
}



void loop()
{
    ws.cleanupClients();
    // 1) Immer alle verfügbaren Sensor-Events verarbeiten:
    while (myIMU.getSensorEvent())
    {
        uint8_t id = myIMU.getSensorEventID();
        switch (id)
        {
        case SENSOR_REPORTID_ACCELEROMETER:
            accelX = myIMU.getAccelX();
            accelY = myIMU.getAccelY();
            accelZ = myIMU.getAccelZ();
            break;
        case SENSOR_REPORTID_LINEAR_ACCELERATION:
            linX = myIMU.getLinAccelX();
            linY = myIMU.getLinAccelY();
            linZ = myIMU.getLinAccelZ();
            break;
        case SENSOR_REPORTID_ROTATION_VECTOR:
        case SENSOR_REPORTID_GAME_ROTATION_VECTOR:
            // ... Rotation lesen und cache
            break;
        }
    }

    // 2) Nur, wenn der nächste Sende-Zeitpunkt erreicht ist:
    uint32_t now = millis();
    if ((int32_t)(now - nextSend) >= 0)
    {
        nextSend = now + SEND_INTERVAL;

        // JSON mit allen gecachten Werten
        char buf[128];
        int len = snprintf(buf, sizeof(buf),
                           "{\"yaw\":%.1f,\"pitch\":%.1f,\"roll\":%.1f,"
                           "\"accX\":%.2f,\"accY\":%.2f,\"accZ\":%.2f,"
                           "\"linX\":%.2f,\"linY\":%.2f,\"linZ\":%.2f}",
                           yaw, pitch, roll,
                           accelX, accelY, accelZ,
                           linX, linY, linZ);
        ws.textAll(buf, len);
        Serial.println(buf);

        // Für Debug-Abstandsmessung:
        Serial.println(now - lastMillis);
        lastMillis = now;
    }

    // ein winziges delay, um Watchdog zu füttern
    delay(1);
}

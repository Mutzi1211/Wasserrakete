
#pragma once

#include <Wire.h>
#include <SparkFunBME280.h>

// #include "SparkFun_BNO08x_Arduino_Library.h"
// #include <Adafruit_BNO08x.h>

#include "Rakete_BME280.h"
#include "Rakete_Display.h"
#include "Rakete_ICM20948.h"
#include "Rakete_MAX17048.h"
#include "Rakete_Servo.h"
#include "Rakete_Webserver.h"

#include <Arduino.h>
#include <AsyncEventSource.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>

#include <WiFi.h>
#include <Wire.h>

// // WLAN
#define SSID "UniFi"
#define PASSPHRASE "lol"

// WLAN AP
#define SSID_AP "ESP32-Rakete"
#define PASSPHRASE_AP "12345678"

// Webserver

// Pinbelegung SPI
#define MISO 19 // Data In
#define MOSI 23 // Data Out
#define SCK 18  // Clock

#define SD_CS 5 // Chip Select (CS) für die SD-Karte



Rakete_MAX17048 lipo;
bool lipo_active = false;


Rakete_Display display(32, 128);
bool oled_active = false;

Rakete_ICM20948 icm;
bool icm_active = false;

Rakete_BME280 bme;
bool bme_active = false;
float max_altitude = 0.0f;
float min_altitude = 3000.0f;

Rakete_Servo servo(15);
bool servo_active = false;

Rakete_Webserver webserver;



File csvFile;

// Pinbelegung I2C
#define SDA 21 // Data Line
#define SCL 22 // Clock Line

bool parachute_deployed = false;
bool parachute_progress = false;
int last_parachute = 0;

#define PUBLISH_INTERVAL \
    100 // Zeitintervall für die Veröffentlichung von Daten in Millisekunden
uint32_t nextMicros;

struct LogEntry
{
    uint32_t timestamp;                    // Zeitstempel der Aufnahme
    float temp, hum, pres, altitude;       // BME280 Messwerte
    float yaw, pitch, roll;                // BNO08x Euler-Winkel in Grad
    float linAccelX, linAccelY, linAccelZ; // BNO08x lineare Beschleunigung
    float accelX, accelY, accelZ;          // BNO08x Beschleunigung
    float soc, voltage;                    // MAX17048 Ladezustand und Spannung
};

void mountSD();
void connectWifi();
void initRecording();

void saveBattery()
{
    String payload = "{";
    payload += "\"soc\":" + String(lipo.getSOC(), 1) + ",";
    payload += "\"voltage\":" + String(lipo.getVoltage(), 3);
    payload += "}";

    File jsonFile = SD.open("/battery.json", FILE_WRITE);
    if (jsonFile)
    {
        jsonFile.print(payload);
        jsonFile.close();
    }

    display.show("Launch -> ");
}

void initWebserver()
{
    display.show("Bat: \n" + String(lipo.getSOC()) + "%");

    delay(500);

    connectWifi();

    mountSD();

    webserver.init();
}

void initRecording()
{
    Serial.println("Aufnahme gestartet");

    uint16_t maxIndex = 0;
    File root = SD.open("/");
    while (true)
    {
        File entry = root.openNextFile();
        if (!entry)
            break;

        if (!entry.isDirectory())
        {
            const char *fullName = entry.name();
            const char *fn = fullName[0] == '/' ? fullName + 1 : fullName;
            if (strncmp(fn, "log_", 4) == 0 && strlen(fn) == 12 && fn[8] == '.' &&
                tolower(fn[9]) == 'c' && tolower(fn[10]) == 's' &&
                tolower(fn[11]) == 'v')
            {
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
    if (!csvFile)
    {
        Serial.println("Fehler beim Erstellen der CSV-Datei!");
        return;
    }

    csvFile.println("time_us,temp_C,hum_pct,press_hPa,altitude_m,accelX,"
                    "accelY,accelZ,voltage_V");
    csvFile.flush();
}

void connectWifi()
{
    // Initialize WiFi
    WiFi.begin(SSID, PASSPHRASE);
    Serial.print("\nConnecting to WiFi...");

    for (int i = 0; i < 2; i++)
    {
        delay(1000);
        Serial.print(".");
    }

    if (WiFi.status() != WL_CONNECTED)
    {

        Serial.println("FEHLER: WiFi nicht verbunden");

        WiFi.softAP(SSID_AP, PASSPHRASE_AP);
        Serial.println("Own AP started");

        return;
    }
    else
    {

        Serial.println("\nConnected to WiFi");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
    }
}

void mountSD()
{
    Serial.print("SD-Karte initialisieren...");
    if (!SD.begin(SD_CS, SPI, 4000000))
    {
        Serial.println(" FEHLER!");
        delay(1000);
    }
    else
    {
        Serial.println(" OK");
    }

    Serial.print("SD-Karte Typ: ");
    switch (SD.cardType())
    {
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

    webserver.saveJsonListing("/list.json");
}

void record()
{
    LogEntry entry;
    entry.timestamp = micros();

    // BME280
    if (bme_active)
    {
        entry.temp = bme.getTemperatur();
        entry.hum = bme.getHumidity();
        entry.pres = bme.getPressure() / 100.0; // hPa
        entry.altitude = bme.getAltitude();
    }
    else
    {
        entry.temp = entry.hum = entry.pres = 0.0f;
    }

    // ICM-20948
    if (icm_active)
    {

        entry.accelX = icm.getAccX();
        entry.accelY = icm.getAccY();
        entry.accelZ = icm.getAccZ();
    }

    // Ins CSV schreiben
    if (csvFile)
    {
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

bool check_parachute()
{

    bool zeroG = false;

    if (icm_active)
    {
        float totalAcc = sqrt(icm.getAccX() * icm.getAccX() + icm.getAccY() * icm.getAccY() + icm.getAccZ() * icm.getAccZ());
        if (totalAcc < 200)
        {
            zeroG = true;
        }
        else
        {
            zeroG = false;
        }
    }

    if (bme_active)
    {

        float altitude = bme.getAltitude();

        // Serial.println("Altitude: " + String(altitude) + "m");
        // Serial.println("Max Altitude: " + String(max_altitude) + "m");

        if (altitude < min_altitude)
        {
            min_altitude = altitude;
        }

        if (altitude > max_altitude)
        {
            max_altitude = altitude;
            return false;
        }

        if (zeroG && altitude < max_altitude - 0.1)
        {
            return true;
        }
    }
    return false;
}

void deploy_parachute()
{
    if (parachute_deployed)
    {
        return;
    }

    servo.setAngle(90);
    last_parachute = millis();
    parachute_deployed = true;
    parachute_progress = true;

    float height = max_altitude - min_altitude;
    display.show("Hoehe: \n" + String(height));
}

void stop_parachute()
{
    servo.setAngle(0);
    parachute_progress = false;
}

void reset()
{
    parachute_deployed = false;
    max_altitude = 0;
    min_altitude = 1000;
    Serial.println("Fallschirm zurückgesetzt");
    display.show("Launch ->");
}

void init()
{

    delay(100);
    Serial.println("Start");

    // ---- I2C initialisieren ----
    Wire.begin(SDA, SCL);
    Serial.println("I2C initialized");

    // ---- SPI initialisieren ----
    SPI.begin(SCK, MISO, MOSI, SD_CS);
    Serial.println("SPI initialized");
}
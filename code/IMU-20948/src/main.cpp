#include <Wire.h>
#include <SparkFun_BNO08x_Arduino_Library.h>
#include <SparkFunBME280.h>

BME280 myBME;

BNO08x myIMU;

// I²C
#define SDA_PIN 21
#define SCL_PIN 22

// INT & RST
#define INT_PIN 14
#define RST_PIN 32

// I²C Adresse
#define BNO08X_ADDR 0x4B // oder 0x4A bei Jumper geschlossen

void hardResetSensor()
{
  Serial.println("➤ Führe Hardware-Reset durch...");
  pinMode(RST_PIN, OUTPUT);
  digitalWrite(RST_PIN, LOW);
  delay(10);
  digitalWrite(RST_PIN, HIGH);
  delay(500); // Zeit für Neustart
}

void setup()
{
  Serial.begin(115200);
  delay(2000);
  Serial.println("\nStarte BNO08x Initialisierung...");

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000); // schnellere I2C-Kommunikation

  hardResetSensor();

  if (!myIMU.begin(BNO08X_ADDR, Wire, INT_PIN, RST_PIN))
  {
    Serial.println("❌ BNO08x nicht gefunden. Checke Adresse und Verkabelung!");
    while (1)
      ;
  }

  Serial.println("✅ BNO08x erkannt!");
  delay(500);
  delay(100);

  if (myIMU.enableRotationVector())
  {
    Serial.println("✅ Rotation Vector aktiviert.");
  }
  else if (myIMU.enableGameRotationVector())
  {
    Serial.println("⚠️ Rotation Vector nicht verfügbar – Game Rotation Vector aktiviert.");
  }
  else
  {
    Serial.println("❌ Weder Rotation noch Game Rotation Vector konnten aktiviert werden!");
    while (1)
      ;
  }

  Serial.println("Starte Ausgabe von Yaw, Pitch, Roll...");

  if (myBME.beginI2C() == false)
  {
    Serial.println("❌ BME280 nicht erkannt!");
    while (1)
      ;
  }

  Serial.println("✅ BME280 erkannt.");
}

void loop()
{
  delay(5);

float temp = myBME.readTempC();
float pressure = myBME.readFloatPressure() / 100.0;
float humidity = myBME.readFloatHumidity();

  

  if (myIMU.wasReset())
  {
    Serial.println("🔁 Sensor wurde zurückgesetzt. Aktiviere Vector erneut...");
    delay(100);
    myIMU.enableRotationVector();
  }

  if (myIMU.getSensorEvent())
  {
    uint8_t reportID = myIMU.getSensorEventID();
    if (reportID == SENSOR_REPORTID_ROTATION_VECTOR || reportID == SENSOR_REPORTID_GAME_ROTATION_VECTOR)
    {
      float yaw = myIMU.getYaw() * 180.0 / PI;
      float pitch = myIMU.getPitch() * 180.0 / PI;
      float roll = myIMU.getRoll() * 180.0 / PI;
      Serial.printf("Temp: %.1f°C  Press: %.1f hPa  Hum: %.1f%%  ", temp, pressure, humidity);
      Serial.printf("Yaw: %.1f°, Pitch: %.1f°, Roll: %.1f°\n", yaw, pitch, roll);

    }
  }
}
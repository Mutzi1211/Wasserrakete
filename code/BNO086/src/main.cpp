#include <Wire.h>
#include "SparkFun_BNO08x_Arduino_Library.h"

// ---- I²C-Pins ----
#define SDA_PIN     21
#define SCL_PIN     22

// ---- IMU-Pins & Adresse ----
#define INT_PIN     32
#define RST_PIN     14
#define BNO08X_ADDR 0x4B

// ---- Sensor-Objekt ----
BNO08x myIMU;

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n>> BNO08x Serial-Demo starten");

  // ---- I2C init ----
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);

  // ---- Hardware-Reset des IMU ----
  pinMode(RST_PIN, OUTPUT);
  digitalWrite(RST_PIN, LOW);
  delay(10);
  digitalWrite(RST_PIN, HIGH);
  delay(500);

  // ---- IMU initialisieren ----
  Serial.print("Initialisiere BNO08x...");
  if (!myIMU.begin(BNO08X_ADDR, Wire, INT_PIN, RST_PIN)) {
    Serial.println(" FEHLER!");
    while (1) delay(1000);
  }
  myIMU.enableRotationVector();
  Serial.println(" OK – RotationVector aktiviert");
}

void loop() {
  // Falls sich der IMU zurücksetzt, wieder RotationVector aktivieren
  if (myIMU.wasReset()) {
    myIMU.enableRotationVector();
  }

  // Neuen Sensorevent abholen
  if (myIMU.getSensorEvent()) {
    uint8_t id = myIMU.getSensorEventID();
    if (id == SENSOR_REPORTID_ROTATION_VECTOR ||
        id == SENSOR_REPORTID_GAME_ROTATION_VECTOR) {

      // Winkel in Grad
      float yaw   = myIMU.getYaw()   * 180.0 / PI;
      float pitch = myIMU.getPitch() * 180.0 / PI;
      float roll  = myIMU.getRoll()  * 180.0 / PI;

      // Ausgabe über Serial
      Serial.print("Yaw:   ");
      Serial.print(yaw,   1);
      Serial.print("\tPitch: ");
      Serial.print(pitch, 1);
      Serial.print("\tRoll:  ");
      Serial.println(roll,  1);
    }
  }

  delay(100); // 10 Hz-Ausgabe
}

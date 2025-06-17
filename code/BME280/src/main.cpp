// ESP32 Beispiel: BME280 von SparkFun auslesen
// Benötigte Bibliothek: SparkFunBME280 Arduino Library
// Installation über Bibliotheksverwalter: "SparkFun BME280"

#include <Wire.h>
#include <SparkFunBME280.h>


#define SDA 21 // Standard-SDA-Pin auf ESP32
#define SCL 22 // Standard-SCL-Pin auf ESP32


BME280 bme280;

void setup() {
  Serial.begin(115200);

  Wire.begin(SDA,SCL); // Default SDA=S21, SCL=S22 auf ESP32
  Wire.setClock(400000); // I2C-Takt auf 400 kHz setzen

  // Sensor initialisieren
  if (!bme280.beginI2C()) {
    Serial.println("BME280 nicht gefunden. Überprüfe Verkabelung und Adresse.");
  }

  Serial.println("BME280 initialisiert.");
}

void loop() {
  // Messwerte auslesen
  float temperature = bme280.readTempC();    // Temperatur in °C
  float pressure    = bme280.readFloatPressure() / 100.0F;  // Druck in hPa
  float humidity    = bme280.readFloatHumidity(); // Luftfeuchtigkeit in %

  // Ausgabe
  Serial.print("Temperatur: ");
  Serial.print(temperature, 2);
  Serial.print(" °C, Druck: ");
  Serial.print(pressure, 2);
  Serial.print(" hPa, Feuchte: ");
  Serial.print(humidity, 2);
  Serial.println(" %");

  delay(2000); // 2 Sekunden warten
}

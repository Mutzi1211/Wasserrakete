
#include "Rakete_BME280.h"

Rakete_BME280::Rakete_BME280() {
  return;

}

bool Rakete_BME280::init() {
  if (!bme280.begin()) {
    Serial.println("BME280 initialisierung fehlgeschlagen!");
    return false;
  }
  Serial.println("BME280 initialisiert");
  return true;
}

float Rakete_BME280::getTemperatur() { return bme280.readTempC(); }
float Rakete_BME280::getHumidity() { return bme280.readFloatHumidity(); }
float Rakete_BME280::getPressure() { return bme280.readFloatPressure(); }
float Rakete_BME280::getAltitude() { return bme280.readFloatAltitudeMeters(); }
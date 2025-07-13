#include "Rakete_ICM20948.h"

Rakete_ICM20948::Rakete_ICM20948() {  return;}

bool Rakete_ICM20948::init() {
  if (icm.begin()) {
    Serial.println("ICM20948 initialisierung fehlgeschlagen!");
    return false;
  }
  return true;
}

float Rakete_ICM20948::getAccX() {
  icm.getAGMT();
  return icm.accX();
}

float Rakete_ICM20948::getAccY() {
  icm.getAGMT();
  return icm.accY();
}

float Rakete_ICM20948::getAccZ() {
  icm.getAGMT();
  return icm.accZ();
}


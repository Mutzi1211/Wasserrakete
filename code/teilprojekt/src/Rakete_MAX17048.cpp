
#include "Rakete_MAX17048.h"

Rakete_MAX17048::Rakete_MAX17048() {}

bool Rakete_MAX17048::init() {
  if (!lipo.begin()) {
    Serial.println("MAX17048 initialisierung fehlgeschlagen!");
    return false;
  }
  Serial.println("MAX17048 initialisiert");

  // Optional: QuickStart-Reset (force full recalibration)
  lipo.quickStart();

  return true;
}

float Rakete_MAX17048::getVoltage() { return lipo.getVoltage(); }
float Rakete_MAX17048::getSOC() { return lipo.getSOC(); }
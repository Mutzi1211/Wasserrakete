#include <Arduino.h>

#define LED_PIN 0  // auf manchen SparkFun-Platinen ist LED an GPIO 10

void setup() {
  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  digitalWrite(LED_PIN, HIGH);
  delay(500);
  digitalWrite(LED_PIN, LOW);
  delay(500);
}


#include "Rakete_Servo.h"
#include "Arduino.h"

Rakete_Servo::Rakete_Servo( int servoPin_init) {
  servoPin = servoPin_init;
}

bool Rakete_Servo::init() {
  ledcSetup(channel, freq, resolution);
  ledcAttachPin(servoPin, channel);
  return true;
}

void Rakete_Servo::setAngle(int angle) {
  angle = constrain(angle, 0, 180);
  int pulse = map(angle, 0, 180, minPulseUs, maxPulseUs);
  uint32_t duty = pulseUsToDuty(pulse);
  ledcWrite(channel, duty);
}


uint32_t Rakete_Servo::pulseUsToDuty(int pulse_us) {
  float periodUs = 1e6 / freq;
  float dutyFraction = pulse_us / periodUs;
  return (uint32_t)(dutyFraction * ((1 << resolution) - 1));
}
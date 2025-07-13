
#pragma once

#include <Wire.h>

class Rakete_Servo {

public:
  Rakete_Servo(int servoPin_init);
  bool init();
  void setAngle(int angle);

private:

  uint32_t pulseUsToDuty(int pulse_us);
  int servoPin = 15;     // GPIO15
  const int freq = 50;         // 50 Hz for servos
  const int channel = 0;       // LEDC channel 0
  const int resolution = 16;   // 16-bit resolution
  const int minPulseUs = 500;  // Minimum pulse width in microseconds
  const int maxPulseUs = 2400; // Maximum pulse width in microseconds
};

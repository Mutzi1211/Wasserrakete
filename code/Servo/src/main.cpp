#include <Arduino.h>

// Servo parameters and LEDC configuration
const int servoPin = 15;         // GPIO15
const int freq = 50;             // 50 Hz for servos
const int channel = 0;           // LEDC channel 0
const int resolution = 16;       // 16-bit resolution
const int minPulseUs = 500;      // Minimum pulse width in microseconds
const int maxPulseUs = 2400;     // Maximum pulse width in microseconds

// Convert pulse width [us] to duty cycle counts
uint32_t pulseUsToDuty(int pulse_us) {
    // duty = (pulse_us / (1e6 / freq)) * (2^resolution)
    float periodUs = 1e6 / freq;
    float dutyFraction = pulse_us / periodUs;
    return (uint32_t)(dutyFraction * ((1 << resolution) - 1));
}

void setup() {
    Serial.begin(115200);
    delay(500);
    // Configure LEDC PWM
    ledcSetup(channel, freq, resolution);
    ledcAttachPin(servoPin, channel);
    Serial.println("LEDC Servo on GPIO15 initialized");
}

void setServoAngle(int angle) {
    // Constrain angle 0-180
    angle = constrain(angle, 0, 180);
    // Map angle to pulse width
    int pulse = map(angle, 0, 180, minPulseUs, maxPulseUs);
    uint32_t duty = pulseUsToDuty(pulse);
    ledcWrite(channel, duty);
}

void loop() {
    // Center
    setServoAngle(45);
    Serial.println("Angle: 90°"); delay(1000);
    // Left
    setServoAngle(90);
    Serial.println("Angle: 0°"); delay(1000);
    // Back to center
   
}

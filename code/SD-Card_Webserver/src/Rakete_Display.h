
#pragma once

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

class Rakete_Display {

public:
  Rakete_Display(int height, int wide);
  bool init();
  void show(String text);

private:
  Adafruit_SSD1306 display;

 
};

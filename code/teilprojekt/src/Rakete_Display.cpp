
#include "Rakete_Display.h"
#include "Arduino.h"

Rakete_Display::Rakete_Display(int height, int wide) {
  Adafruit_SSD1306 display(height, wide, &Wire);
}

bool Rakete_Display::init() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");
    return false;
  }
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Init");
  display.println("Success!");
  display.display();
  return true;
}

void Rakete_Display::show(String text) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.print(text);
  display.display();


}

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
z
// OLED display parameters
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET    -1 // Reset pin # (or -1 if sharing Arduino reset pin)

// Instantiate the display object
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
    Serial.begin(115200);
    // Initialize I2C
    Wire.begin();

    // Initialize OLED display
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("SSD1306 allocation failed");
        for (;;) ; // Don't proceed, loop forever
    }

    // Clear the buffer
    display.clearDisplay();

    // Display initial text
    display.setTextSize(2);             // Normal 1:1 pixel scale
    display.setTextColor(SSD1306_WHITE);// Draw white text
    display.setCursor(0, 0);            // Start at top-left corner
    display.println("Hallo");
    display.println("Welt!");
    display.display();                  // Send buffer to the display

    Serial.println("OLED-Display initialisiert und Text angezeigt");
}

void loop() {
    // Example: scroll the display every 2 seconds
    display.startscrollright(0x00, 0x0F);
    delay(2000);
    display.stopscroll();
    delay(1000);

    display.startscrollleft(0x00, 0x0F);
    delay(2000);
    display.stopscroll();
    delay(1000);
}

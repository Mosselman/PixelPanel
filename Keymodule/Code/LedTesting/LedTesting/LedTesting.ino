// ── SK6812 MINI-E LEDs on RP2040 via SN74LV1T34 buffer ──
// 25 LEDs connected to GP18

#include <Adafruit_NeoPixel.h>

// ── Configuration ──────────────────────────────────────
#define LED_PIN     18    // GP18 on RP2040
#define NUM_LEDS    25    // Total number of SK6812 LEDs
#define BRIGHTNESS  60    // 0–255 (start low to be safe!)

// SK6812 uses NEO_GRBW because it has a white channel
// If your LEDs look wrong, try NEO_GRB instead
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRBW + NEO_KHZ800);

// ── Setup: runs once when the board powers on ──────────
void setup() {
  strip.begin();           // Start talking to the LEDs
  strip.setBrightness(BRIGHTNESS);
  strip.show();            // Turn all LEDs off at startup
}

// ── Loop: runs forever after setup() ──────────────────
void loop() {
  rainbowChase(50);        // Fun rainbow animation!
}

// ── Rainbow chase animation ────────────────────────────
// Each LED gets a slightly different colour, and they all
// shift together to create a moving rainbow effect.
void rainbowChase(int delayMs) {
  for (long firstPixelHue = 0; firstPixelHue < 5 * 65536; firstPixelHue += 256) {
    for (int i = 0; i < strip.numPixels(); i++) {
      // Spread the rainbow evenly across all LEDs
      int pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());
      // gamma32 makes the colours look more natural to your eyes
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
    }
    strip.show();
    delay(delayMs);
  }
}

// ── Handy helper: set ALL LEDs to one colour ──────────
// Usage: solidColor(255, 0, 0, 0);  // red
//        solidColor(0, 0, 0, 255);  // pure white (uses W channel)
void solidColor(uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, strip.Color(r, g, b, w));
  }
  strip.show();
}
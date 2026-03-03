/*
Chat prompt for the code

i am using a ESP-32 Wroom, an potentiometer, a button and a oled screen.
The oled screen is 128 x 32 pixels and is connected to via I2C.
I want you to write the code for the master esp. the code has to do the following: using the WiFiManager library to connect to the wifi and display text: "setting up wifi",untill the esp32 is connected to the wifi.
When that is done the oled screen should display text: "initilizing" and output a high signal on pin27.
Then the esp should run a function to look for i2c devices, this function needs to return the amount of i2c devices. //// as well as if there are any duplicate i2c adresses <--change this
When there are no duplicate adresses it needs to set pin27 to low and display the number of i2c devices found on the oled display for 2 seconds.
Then the oled screen should have a scrolable menu with 3 options: for the scroling position the input of the potentiometer will be used. when the button is pressed the selected option will be entered.
*/

/*
  ESP32 Master (Arduino)
  - OLED 128x32 over I2C
  - WiFiManager captive portal connect
  - Shows "setting up wifi" until connected
  - Then shows "initilizing" and sets GPIO27 HIGH
  - Scans I2C bus:
      * returns device count
      * detects duplicate addresses (should never happen on I2C, but we report if found)
  - If no duplicates:
      * set GPIO27 LOW
      * show "<n> I2C devices" for 2 seconds
  - Then shows a scrollable 3-item menu:
      * potentiometer selects
      * button enters selected option

  Libraries needed (Arduino Library Manager):
  - WiFiManager by tzapu
  - Adafruit GFX Library
  - Adafruit SSD1306

  Notes:
  - Set your correct SDA/SCL pins if not defaults (ESP32 default: SDA=21, SCL=22)
  - Button is wired to GND, uses INPUT_PULLUP
  - Pot wiper to ADC pin (e.g. GPIO34), ends to 3.3V and GND
*/

/*
  ESP32 (Arduino) Master
  - OLED 128x32 (I2C)
  - Wi-Fi Provisioning (Espressif Wi-Fi Provisioning app)
  - During WiFi setup phase:
      * shows "setting up wifi"
      * shows a scannable QR code on the OLED for the provisioning app
  - When WiFi is connected:
      * shows "initilizing"
      * sets GPIO27 HIGH
      * scans I2C devices and returns amount
      * sets GPIO27 LOW and shows "<n> I2C devices" for 2 seconds
  - Then a scrollable 3-option menu:
      * potentiometer selects
      * button enters option

  Libraries (Arduino Library Manager):
  - Adafruit GFX Library
  - Adafruit SSD1306
  - QRCode by Ricmoo (search "QRCode" by Richard Moore / ricmoo)
  - (Built-in with ESP32 core) WiFiProv

  Wiring assumptions:
  - OLED SDA=21, SCL=22, address 0x3C
  - Pot wiper -> GPIO34 (ADC1), ends -> 3.3V & GND
  - Button -> GPIO26 to GND (uses INPUT_PULLUP)
  - Status pin -> GPIO27
*/

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include "WiFiProv.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <qrcode.h>   // QRCode library (ricmoo)

// ----------------- PINS -----------------
static const int PIN_POT    = 34;
static const int PIN_BUTTON = 26;
static const int PIN_STATUS = 27;

static const int I2C_SDA = 21;
static const int I2C_SCL = 22;

// ----------------- OLED -----------------
static const uint8_t OLED_ADDR = 0x3C;
static const int SCREEN_WIDTH  = 128;
static const int SCREEN_HEIGHT = 32;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ----------------- Provisioning (edit these) -----------------
static const char *POP          = "abcd1234";     // Proof-of-possession (Security 1)
static const char *SERVICE_NAME = "PROV_ESP32";   // Shown in provisioning app
// ------------------------------------------------------------

// WiFi status flag (set from event callback)
volatile bool g_wifiConnected = false;

// ----------------- UI / MENU -----------------
static const char* MENU_ITEMS[3] = {"Option 1", "Option 2", "Option 3"};

enum UiState : uint8_t { STATE_MENU = 0, STATE_OPTION = 1 };
UiState uiState = STATE_MENU;

int selectedIndex = 0;
int activeOption  = -1;
unsigned long optionEnteredMs = 0;

// Pot stability filter
int lastPotIndex = 0;
unsigned long lastPotChangeMs = 0;
static const unsigned long POT_STABLE_MS = 80;

// Button debounce
bool lastButtonReading = HIGH;
bool buttonStableState = HIGH;
unsigned long lastDebounceMs = 0;
static const unsigned long DEBOUNCE_MS = 30;

// ----------------- Helpers -----------------
void oledMessage(const String &line1, const String &line2 = "") {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println(line1);

  if (line2.length()) {
    display.setCursor(0, 12);
    display.println(line2);
  }

  display.display();
}

int potToIndex(int raw) {
  raw = constrain(raw, 0, 4095);
  return map(raw, 0, 4095, 0, 2);
}

void drawMenu(int index) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println("Menu:");

  for (int i = 0; i < 3; i++) {
    int y = 10 + i * 7;
    display.setCursor(0, y);

    if (i == index) {
      display.fillRect(0, y - 1, 128, 8, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
      display.print("> ");
      display.print(MENU_ITEMS[i]);
      display.setTextColor(SSD1306_WHITE);
    } else {
      display.print("  ");
      display.print(MENU_ITEMS[i]);
    }
  }

  display.display();
}

void drawOptionScreen(int option) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println("Selected:");
  display.setCursor(0, 12);
  display.println(MENU_ITEMS[option]);

  display.setCursor(0, 24);
  display.println("Press = back");

  display.display();
}

void handleButtonPress() {
  if (uiState == STATE_MENU) {
    activeOption = selectedIndex;
    uiState = STATE_OPTION;
    optionEnteredMs = millis();
    drawOptionScreen(activeOption);
  } else {
    uiState = STATE_MENU;
    drawMenu(selectedIndex);
  }
}

// ----------------- I2C scan (count devices only) -----------------
uint8_t scanI2CCount() {
  uint8_t count = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission(true);
    if (err == 0) count++;
    delay(2);
  }
  return count;
}

// ----------------- WiFi Provisioning: QR rendering -----------------
String buildProvQRPayload() {
  // Espressif provisioning QR payload (commonly used format):
  // {"ver":"v1","name":"<SERVICE_NAME>","pop":"<POP>","transport":"ble"}
  // Transport: "ble" or "softap"
  String s = "{\"ver\":\"v1\",\"name\":\"";
  s += SERVICE_NAME;
  s += "\",\"pop\":\"";
  s += POP;
  s += "\",\"transport\":\"ble\"}";
  return s;
}

void drawProvisioningQR(const String &payload) {
  // Version 1 QR = 21x21 modules (fits on 128x32 at 1 pixel/module)
  // We'll render at scale=1, with a small border, on the left side.
  QRCode qrcode;
  const uint8_t version = 1;
  uint8_t qrcodeData[qrcode_getBufferSize(version)];
  qrcode_initText(&qrcode, qrcodeData, version, ECC_LOW, payload.c_str());

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Text on the right
  display.setCursor(26, 0);
  display.println("setting up");
  display.setCursor(26, 10);
  display.println("wifi");
  display.setCursor(26, 22);
  display.println("scan QR");

  // QR placement
  const int scale = 1;          // 1 pixel per module
  const int qrSize = qrcode.size * scale; // 21
  const int border = 1;         // quiet zone border in pixels
  const int x0 = 0 + border;
  const int y0 = (SCREEN_HEIGHT - (qrSize + 2 * border)) / 2 + border;

  // White background behind QR to improve readability
  display.fillRect(0, y0 - border, qrSize + 2 * border, qrSize + 2 * border, SSD1306_WHITE);

  // Draw modules (black squares)
  for (int y = 0; y < qrcode.size; y++) {
    for (int x = 0; x < qrcode.size; x++) {
      bool module = qrcode_getModule(&qrcode, x, y);
      if (module) {
        // module = black
        display.drawPixel(x0 + x * scale, y0 + y * scale, SSD1306_BLACK);
        // scale==1 so drawPixel is enough; if you increase scale, use fillRect
      }
    }
  }

  display.display();
}

// ----------------- WiFi Provisioning Event Handler -----------------
void SysProvEvent(arduino_event_t *sys_event) {
  switch (sys_event->event_id) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      g_wifiConnected = true;
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      g_wifiConnected = false;
      break;
    default:
      break;
  }
}

void startProvisioning() {
  WiFi.mode(WIFI_MODE_STA);
  WiFi.onEvent(SysProvEvent);

  // Start provisioning over BLE for the Espressif Wi-Fi Provisioning app
  WiFiProv.beginProvision(
    provSchemeBLE,
    WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM,
    WIFI_PROV_SECURITY_1,
    POP,
    SERVICE_NAME,
    NULL,
    NULL
  );
}

void waitForWiFiConnectedWithQR() {
  String payload = buildProvQRPayload();

  // Update screen periodically while waiting
  unsigned long lastDraw = 0;
  while (!g_wifiConnected) {
    if (millis() - lastDraw > 500) {
      drawProvisioningQR(payload);
      lastDraw = millis();
    }
    delay(20);
  }
}

// ----------------- Setup / Loop -----------------
void setup() {
  pinMode(PIN_STATUS, OUTPUT);
  digitalWrite(PIN_STATUS, LOW);

  pinMode(PIN_BUTTON, INPUT_PULLUP);

  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    while (true) { delay(100); }
  }

  // 1) Provision + connect WiFi (show QR during setup)
  startProvisioning();
  waitForWiFiConnectedWithQR();

  // 2) Initializing + status HIGH
  oledMessage("initilizing");
  digitalWrite(PIN_STATUS, HIGH);

  // 3) Scan I2C, count devices
  uint8_t n = scanI2CCount();

  // 4) Set status LOW and show device count for 2 seconds
  digitalWrite(PIN_STATUS, LOW);
  oledMessage(String(n) + " I2C devices");
  delay(2000);

  // 5) Menu
  uiState = STATE_MENU;
  drawMenu(selectedIndex);
}

void loop() {
  // ---- Pot controls selection ----
  if (uiState == STATE_MENU) {
    int raw = analogRead(PIN_POT);
    int idx = potToIndex(raw);

    if (idx != lastPotIndex) {
      lastPotIndex = idx;
      lastPotChangeMs = millis();
    } else if ((millis() - lastPotChangeMs) > POT_STABLE_MS && idx != selectedIndex) {
      selectedIndex = idx;
      drawMenu(selectedIndex);
    }
  }

  // ---- Button debounce + press ----
  bool reading = digitalRead(PIN_BUTTON);

  if (reading != lastButtonReading) {
    lastDebounceMs = millis();
    lastButtonReading = reading;
  }

  if ((millis() - lastDebounceMs) > DEBOUNCE_MS) {
    if (reading != buttonStableState) {
      buttonStableState = reading;
      if (buttonStableState == LOW) {
        handleButtonPress();
      }
    }
  }

  // Optional: auto-return after 5 seconds (remove if undesired)
  if (uiState == STATE_OPTION) {
    if (millis() - optionEnteredMs > 5000) {
      uiState = STATE_MENU;
      drawMenu(selectedIndex);
    }
  }
}
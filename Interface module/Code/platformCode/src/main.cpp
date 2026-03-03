#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager for commands

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

// ----------------- UI / MENU -----------------
int activeMenu = 0;

static const char* MAIN_MENU[4] = {"Make new", "Send saved", "Settings", "Dev options"};
static const char* SETTINGS_MENU[3] = {"Brightness", "screen timer", "Option 3"};
static const char* DEV_MENU[3] = {"Reconnect wifi", "reset wifi", "reconnect MQTT"};

enum UiState : uint8_t { STATE_MENU = 0, STATE_OPTION = 1 };
UiState uiState = STATE_MENU;

int selectedIndex = 0;
int activeOption  = -1;
unsigned long optionEnteredMs = 0;

void wifisetup() {
    WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
    // it is a good practice to make sure your code sets wifi mode how you want it.
 
    // put your setup code here, to run once:
    Serial.begin(115200);
    
    //WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wm;
 
    // reset settings - wipe stored credentials for testing
    // these are stored by the esp library
    //wm.resetSettings();
 
    // Automatically connect using saved credentials,
    // if connection fails, it starts an access point with the specified name ( "AutoConnectAP"),
    // if empty will auto generate SSID, if password is blank it will be anonymous AP (wm.autoConnect())
    // then goes into a blocking loop awaiting configuration and will return success result
 
    bool res;
    // res = wm.autoConnect(); // auto generated AP name from chipid
    //res = wm.autoConnect("AutoConnectAP","password"); // password protected ap
    res = wm.autoConnect("Pixelpanel"); // anonymous ap
    
    if(!res) {
        Serial.println("Failed to connect");
        // ESP.restart();
    } 
    else {
        //if you get here you have connected to the WiFi    
        Serial.println("connected...yeey :)");
    }
 
} 

void drawMenu(int index,) {
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
      display.print(MAIN_MENU[i]); // needs to be different print
      display.setTextColor(SSD1306_WHITE);
    } else {
      display.print("  ");
      display.print(MAIN_MENU[i]); // needs to be different print
    }
  }

  display.display();
}


void setup() {
  

  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    while (true) { delay(100); }
  }

  wifisetup();
}
 
void loop() {

drawMenu(selectedIndex); 
}
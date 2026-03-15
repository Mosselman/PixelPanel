#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager for commands

// ----------------- PINS -----------------
#define PIN_POT    = 34;
#define PIN_BUTTON = 26;
#define PIN_STATUS = 27;

const static int I2C_SDA = 21;
const static int I2C_SCL = 22;

const int PIN_encoderCLK = 34; // PinCLK
const int PIN_encoderDT = 35; // PinDT
const int PIN_encoderButton = 15; // PinSW

int PIN_DATA = 5;

// ----------------- Encoder -----------------
volatile int encoderValue = 0;
int lastReportedValue = 1;
static int lastEncoderValue = 0;

// Variables to debounce Rotary Encoder
long TimeOfLastDebounce = 0;
const int DelayofDebounce = 2; // Reduced debounce delay in milliseconds

// Store previous Pins state
int PreviousCLK;
int PreviousDT;

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

//=======================================================

void IRAM_ATTR handleEncoderChange() {
  int currentCLK = digitalRead(PIN_encoderCLK);
  int currentDT = digitalRead(PIN_encoderDT);

  if (PreviousCLK == 0 && currentCLK == 1) {
    if (currentDT == 0) {
      encoderValue++;  // Clockwise
    } else {
      encoderValue--;  // Counter-Clockwise
    }
  } else if (PreviousCLK == 1 && currentCLK == 0) {
    if (currentDT == 1) {
      encoderValue++;  // Clockwise
    } else {
      encoderValue--;  // Counter-Clockwise
    }
  }

  PreviousCLK = currentCLK;
  PreviousDT = currentDT;
}

void IRAM_ATTR handleButtonPress() {
  unsigned long currentTime = millis();
  if (currentTime - TimeOfLastDebounce > DelayofDebounce) {
    TimeOfLastDebounce = currentTime;
    Serial.println("Button Pressed!");
  }
}

void readEncoderTask(void * pvParameters) {
  for (;;) {
    if (lastEncoderValue != encoderValue) {
      // Handle encoder value changes
      lastEncoderValue = encoderValue;
    }
    vTaskDelay(1 / portTICK_PERIOD_MS); // Delay for 1 ms
  }
}

void encodersetup() {
  pinMode(PIN_encoderCLK, INPUT);
  pinMode(PIN_encoderDT, INPUT);
  pinMode(PIN_encoderButton, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(PIN_encoderCLK), handleEncoderChange, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_encoderButton), handleButtonPress, FALLING);

  PreviousCLK = digitalRead(PIN_encoderCLK);
  PreviousDT = digitalRead(PIN_encoderDT);

  xTaskCreatePinnedToCore(
    readEncoderTask,    // Function to implement the task
    "readEncoderTask",  // Name of the task
    10000,              // Stack size in words
    NULL,               // Task input parameter
    1,                  // Priority of the task
    NULL,               // Task handle
    0                   // Core where the task should run
  );
}

void wifisetup() {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Connecting to wifi...:");   
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

void panelSetup() {
  //i2c recieve on 
  pinMode(PIN_DATA, OUTPUT);
  PIN_DATA = HIGH;
  //map i2c adresses to each panel
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
    }

    switch (int menuNav = 0)  {
      case 0
        display.print(MAIN_MENU[i]); // needs to be different print
        display.setTextColor(SSD1306_WHITE);
      break;
        
      case 1
        display.print(SETTINGS_MENU[i]); // needs to be different print
        display.setTextColor(SSD1306_WHITE);
      break;

      }

    else {
      display.print("  ");
      display.print(MAIN_MENU[i]); // needs to be different print
    }
  }
  display.display();
}

//========================================================================

void setup() {
  encodersetup();

  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    while (true) { delay(100); }
  }

  wifisetup();
  panelSetup();
}
 
void loop() {

  if (lastReportedValue != encoderValue) {
    Serial.println(encoderValue);
    lastReportedValue = encoderValue;
  }
  delay(10);
  
  drawMenu(selectedIndex); 

}




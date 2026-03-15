#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager for commands

// +++++++++++++++++++ TEMP/TEST pins & arguments +++++++++++++++++++

const int PIN_potmeter = 34;
const int PIN_button = 15;

// ----------------- PINS -----------------

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

bool buttonState;

// ----------------- OLED -----------------
static const uint8_t OLED_ADDR = 0x3C;
static const int SCREEN_WIDTH  = 128;
static const int SCREEN_HEIGHT = 32;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ----------------- UI / MENU -----------------
int activeMenu = 1;

const char* MenuItems[][5] = {
  {"Main menu", "Send art", "Settings", "Dev options","restart"},
  {"Send options", "Make new", "send saved", "animation", "return"},
  {"Settings", "Brightness", "led off timer", "sleep timer", "return"},
  {"Dev options", "Reconnect wifi", "reset wifi", "reconnect MQTT", "return"}
};

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
    buttonState = true;
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
  display.println(MenuItems[activeMenu][1]);//title of the menu

  for (int i = 1; i < 5; i++) {//start at 1 to skip the title of the menu
    int y = 10 + (i-1) * 7; //calc the height of the cursur to get even spacing
    display.setCursor(0, y);

    if (i == index) { 
      display.fillRect(0, y - 1, 128, 8, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
      display.print("> ");
      display.print(MenuItems[activeMenu][i]); 
      display.setTextColor(SSD1306_WHITE); }
    else {
      display.print("  ");
      display.print(MenuItems[activeMenu][i]); 
    }
  }
  display.display();
}

void buttonNav() {
  if (activeMenu == 1 ) { //handles main menu navigation and is sold
    if (selectedIndex == 5)
    {
      ESP.restart(); } //restart in menu is selected (restart esp)
    else {
      activeMenu = selectedIndex; }//go to the menu that is selected
  }
  else if (activeMenu != 1 && selectedIndex == 5) {
  activeMenu = 1; } //return to main menu

  else {
    switch (activeMenu) {
    case 2://send art
      
    break;
    
    case 3://settings
      
    break;

    case 4://dev options

    break;
    }
  }
  buttonState = false;
}
//========================================================================

void setup() {
  Serial.begin(116500);

  //display first
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    while (true) { delay(100); }
  }

  //display a little logo WIP
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Pixelpanel logo tm");   
  delay(2000);

  //input devices 2nd
  encodersetup();

  //wifi before panel setup due to possible instability
  wifisetup();

  panelSetup(); //still need to be implemented, will handle i2c communication and panel mapping
  
}
 
void loop() {

  if (lastReportedValue != encoderValue) {
    Serial.println(encoderValue);
    lastReportedValue = encoderValue;
  }

  delay(10);
  selectedIndex = map(analogRead(PIN_potmeter), 0, 4095, 0, 5);//temp solution for testing, will be removed when encoder is fully implemented
  drawMenu(selectedIndex); 


  if (buttonState){
    buttonNav();
  }
}




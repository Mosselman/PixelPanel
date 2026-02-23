//#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

#include <FastLED.h>

// Wi-Fi
const char* ssid = "De wifi fabriek";
const char* password = "rpfFhk7yNrPT";

// MQTT (HiveMQ Cloud)
const char* mqtt_broker   = "034ea6dce7954b1fb5e4f4ad8d535315.s1.eu.hivemq.cloud";
const int   mqtt_port     = 8883;
const char* mqtt_username = "Herman";
const char* mqtt_password = "Banaan1!";

// Topics
const char* topic_push = "esp32/HermanCasualInSocks";     // what THIS device publishes
const char* topic_pull = "esp32/HermanSend";  // what THIS device subscribes to

//wifi things | make sure to set up secure wifi
WiFiClientSecure wifiClient;
PubSubClient mqttClient(wifiClient);

// variable to confirm sending the data
bool sendArray = false;
bool recievedArray = false;

// 5x5 payloads
byte pixelArray[5][5] = { 0 };     // to send
byte receivedImage[5][5] = { 0 };  // last received

int ledNumber = 0;
unsigned int menuState = 0;


//fastled stuff
#define COLOR_ORDER GRB
#define CHIPSET     WS2812B
#define LED_PIN     4 //placeholder pin
#define NUM_LEDS    25

CRGB leds[NUM_LEDS];

uint16_t hue = 0;
uint16_t saturation = 255;
uint16_t brightness = 100;

uint8_t columnBits = 5;
uint8_t rowBits = 5;

//74hc165 shift register pins
uint8_t columnClockPin = 7; //placeholder number
uint8_t columnDataPin = 7; //placeholder number
uint8_t rowClockPin = 7; //placeholder number
uint8_t rowDataPin = 7; //placeholder number

uint8_t potPin = 7; //placeholder number

// ----- MQTT -----
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("MQTT msg on "); Serial.print(topic);
  Serial.print(" length="); Serial.println(length);

  if (length == 25) {
    // Copy flat 25 bytes into 5x5 matrix (row-major)
    memcpy(receivedImage, payload, 25);

    Serial.println("Received 5x5 array:");
    for (int r = 0; r < 5; r++) {
      for (int c = 0; c < 5; c++) {
        Serial.print(receivedImage[r][c]);
        Serial.print(c == 4 ? '\n' : ' ');
      }
    }
  recievedArray = true;  
  } else {
    Serial.println("Unexpected data length (expected 25).");
  }
}

void mqttConnect() {
  mqttClient.setServer(mqtt_broker, mqtt_port);
  mqttClient.setCallback(mqttCallback);

  Serial.println("Connecting to MQTT Broker...");
  while (!mqttClient.connected()) {
    String clientId = "ESP32Client-" + String((uint32_t)esp_random(), HEX);
    if (mqttClient.connect(clientId.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("Connected to MQTT.");
      // Subscribe to incoming topic
      mqttClient.subscribe(topic_pull);
      Serial.print("Subscribed to: "); Serial.println(topic_pull);
    } else {
      Serial.print("Failed, rc="); Serial.print(mqttClient.state());
      Serial.println(" . Retry in 5s.");
      delay(5000);
    }
  }
}

// ----- WiFi -----
void wifiConnect() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Wi-Fi OK. IP: "); Serial.println(WiFi.localIP());

  // TLS: insecure for testing. Use setCACert(...) in production.
  wifiClient.setInsecure();
}

void buttonRead() {
  
}

void writeImageArray() {
  for (int c = 0; c < columnBits; c++) {
    int cBit = digitalRead(columnDataPin);
    if (cBit == HIGH) {
      for (int r = 0; r < rowBits; r++) {
        int rBit = digitalRead(rowDataPin);
        if (rBit == HIGH) {
          hue = map(analogRead(potPin), 0, 4095, 0, 255);
          if(pixelArray[r][c] == hue){
            pixelArray[r][c] = 0;
          }
          else{
            pixelArray[r][c] = hue;
          }
        }
        else {
          digitalWrite(rowClockPin, HIGH); // Shift out the next bit for row
          digitalWrite(rowClockPin, LOW);
        }
      } 
    }
    else {
      digitalWrite(columnClockPin, HIGH); // Shift out the next bit of column
      digitalWrite(columnClockPin, LOW);
    }
  }
}

void drawPicture() {
  for (int row = 0; row < 5; row++) {
    for (int col = 0; col < 5; col++) {
      uint8_t hue = receivedImage[row][col];

      int index;
      if (row % 2 == 0) {
        index = row * 5 + col;          // even row
      } else {
        index = row * 5 + (4 - col);    // odd row reversed
      }

      if (index >= 0 && index < NUM_LEDS) {
        leds[index] = CHSV(hue, saturation, brightness);
      }
    }
  }
  FastLED.show();
}

void sendPicture() {
  if (sendArray == true) {
      sendArray = false;

    // Publish raw 25 bytes (5x5) on topic_push
    bool ok = mqttClient.publish(
      topic_push,
      reinterpret_cast<const uint8_t*>(pixelArray), // raw bytes
      sizeof(pixelArray),                            // 25
      false                                          // not retained
    );

    Serial.print("Published 5x5 bytes to ");
    Serial.print(topic_push);
    Serial.println(ok ? " [OK]" : " [FAIL]");
  }
}
/*
void menu() {
   switch(menuState) {

    //setup menu
    case 0:
    //while( condition ) {
    //things like brighness and oled display menu etc...
    //}
    menuState++;
    break;
    
    //check server for images and display them
    // should default to this case periodicly checking if another picture is send
    case 1:
    mqttClient.rea();
    drawPicture();
    //menuState++;
    
    delay(20);
    break;
    
    //make new image
    case 2:
    writeImageArray();
    sendPicture();
    break;
    
    //something else
    case 3:

    break;
}
*/
//------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(100);

  wifiConnect();
  mqttConnect();

  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);// verify correct color order
}

void loop() {
  //in case of wifi disconnect -> reconnect
  if (!mqttClient.connected()) {
    mqttConnect();
  }
  mqttClient.loop();
  if (recievedArray)  {
    recievedArray = false;
    drawPicture();
  }

  //menu();

  // Send once (or periodically) from the sender device
}
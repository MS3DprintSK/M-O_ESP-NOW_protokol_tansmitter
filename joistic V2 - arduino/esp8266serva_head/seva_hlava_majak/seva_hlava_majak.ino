#include <ESP8266WiFi.h>
#include <espnow.h>
#include <Servo.h>
#include <NeoPixelBus.h>

// --- ALLOWED MAC ADDRESS CONFIGURATION ---
uint8_t allowedMacAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// --- PIN CONFIGURATION ---
#define PIN_BEACON_SERVO D8
#define PIN_SERVO_Y      D7
const uint16_t LedCount = 8; 

// NeoPixel setup (Data wire must be on pin D4 / GPIO2)
NeoPixelBus<NeoGrbFeature, NeoEsp8266Uart1800KbpsMethod> strip(LedCount);

RgbColor red(255, 0, 0);
RgbColor black(0, 0, 0);

// --- BEACON ANIMATION VARIABLES ---
const uint8_t pairs[4][2] = { {0, 4}, {1, 5}, {2, 6}, {3, 7} };
uint8_t currentPair = 0;
uint8_t nextPair    = 1;
uint8_t fade        = 0;
bool fadingIn       = true;
uint16_t rotationSpeed = 10; 
uint8_t fadeStep       = 15;
unsigned long lastUpdate = 0;

// --- SERVO SETTINGS ---
Servo beaconServo;
Servo servoY;
const int BEACON_MIN = 0;
const int BEACON_MAX = 165;
bool isBeaconExtended = false; 
const int Y_OFFSET = 60; // Offset for head lift
const int Y_LIMIT_MIN = 120; // Minimum safe head angle

// --- DATA STRUCTURE ---
typedef struct struct_message {
    int id;
    int joy[4];
} struct_message;

struct_message incomingData;

// --- DATA RECEIVE FUNCTION WITH MAC FILTER ---
void OnDataRecv(uint8_t * mac, uint8_t *data, uint8_t len) {
  // 1. Check if data comes from the allowed MAC address
  for (int i = 0; i < 6; i++) {
    if (mac[i] != allowedMacAddress[i]) {
      return; // Ignore unknown devices
    }
  }

  // 2. Copy received data
  memcpy(&incomingData, data, sizeof(incomingData));
  
  // 3. Beacon logic (extension based on ID)
  bool shouldBeExtended = (incomingData.id == 5 || incomingData.id == 6 || incomingData.id == 7);
  if (shouldBeExtended != isBeaconExtended) {
      if (shouldBeExtended) {
          beaconServo.write(BEACON_MAX);
      } else {
          beaconServo.write(BEACON_MIN);
          // Turn off LEDs when retracted
          for(int i=0; i<LedCount; i++) strip.SetPixelColor(i, black);
          strip.Show();
      }
      isBeaconExtended = shouldBeExtended;
  }

  // 4. Servo Y logic with limit 120
  int invertedY = 180 - incomingData.joy[1];
  int finalY = constrain(invertedY + Y_OFFSET, Y_LIMIT_MIN, 180); 
  servoY.write(finalY);
}

void setup() {
  Serial.begin(115200);

  // Initialize LED strip
  strip.Begin();
  strip.Show(); 
  
  // Initialize servos
  beaconServo.attach(PIN_BEACON_SERVO, 650, 2600);
  servoY.attach(PIN_SERVO_Y);
  
  // Initial positions (with limits applied)
  beaconServo.write(BEACON_MIN);
  servoY.write(constrain(90 + Y_OFFSET, Y_LIMIT_MIN, 180));

  // Initialize WiFi and ESP-NOW
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  
  if (esp_now_init() != 0) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_recv_cb(OnDataRecv);
}

// --- LED ANIMATION (BEACON) ---
void animateBeacon() {
  unsigned long now = millis();
  if (now - lastUpdate >= rotationSpeed) {
    lastUpdate = now;

    if (fadingIn) {
      if (fade < 255 - fadeStep) fade += fadeStep;
      else fadingIn = false;
    } else {
      if (fade > fadeStep) fade -= fadeStep;
      else {
        currentPair = nextPair;
        nextPair = (nextPair + 1) % 4;
        fade = 255;
        fadingIn = false;
      }
    }
    
    // Clear and draw new LED pair
    for(int i=0; i<LedCount; i++) strip.SetPixelColor(i, black);

    if (fadingIn) {
      RgbColor c = RgbColor(fade, 0, 0);
      strip.SetPixelColor(pairs[currentPair][0], c);
      strip.SetPixelColor(pairs[currentPair][1], c);
    } else {
      RgbColor c1 = RgbColor(fade, 0, 0);
      RgbColor c2 = RgbColor(255 - fade, 0, 0);
      strip.SetPixelColor(pairs[currentPair][0], c1);
      strip.SetPixelColor(pairs[currentPair][1], c1);
      strip.SetPixelColor(pairs[nextPair][0], c2);
      strip.SetPixelColor(pairs[nextPair][1], c2);
    }
    strip.Show();
  }
}

void loop() {
  // Animation runs only if the beacon is mechanically extended
  if (isBeaconExtended) {
    animateBeacon();
  }
  yield(); // Maintain WiFi stack stability
}
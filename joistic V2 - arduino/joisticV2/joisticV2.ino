#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <TJpg_Decoder.h>
#include <SD.h>
#include <WiFi.h>
#include <esp_now.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>

// --- PIN CONFIGURATION (CYD) ---
#define XPT_CS   33
#define XPT_IRQ  36
#define XPT_MOSI 32
#define XPT_MISO 39
#define XPT_CLK  25
#define SD_CS     5

// --- I2C PINS FOR ADS1115 (Safe pins) ---
#define I2C_SDA 27  // Connector P3
#define I2C_SCL 22  // Connector CN1

// --- JOYSTICK SETTINGS ---
Adafruit_ADS1115 ads;
const int ADC_MAX = 26400; 
const int DEADZONE = 400;

// List of specific receiver MAC addresses
uint8_t receiverAddresses[][6] = {
  {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // Left eye
  {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // Right eye
  {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // Head servos
  {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}  // Body
};
const int receiverCount = 4;

// Data structure to send
typedef struct struct_message {
    int id;
    int joy[4]; // [0]=J1X, [1]=J1Y, [2]=J2X, [3]=J2Y
} struct_message;

struct_message myData;

// Objects for display and touch
SPIClass touchSPI = SPIClass(VSPI);
XPT2046_Touchscreen touch(XPT_CS, XPT_IRQ);
TFT_eSPI tft = TFT_eSPI();

// Image grid parameters
const int imgW = 120; 
const int imgH = 58;  
const int colCount = 2;
int lastID = -1;

// --- DISPLAY FUNCTIONS ---
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (y >= tft.height()) return false;
  tft.pushImage(x, y, w, h, bitmap);
  return true;
}

void drawSelectionFrame(int id, uint16_t color) {
  if (id < 1 || id > 7) return;
  int i = id - 1;
  int x = (i % colCount) * imgW;
  int y = (i / colCount) * imgH;
  for (int t = 0; t < 5; t++) {
    tft.drawRect(x + t, y + t, imgW - (2 * t), imgH - (2 * t), color);
  }
}

// --- SETUP ---
void setup() {
  Serial.begin(115200);

  // 1. ADS1115 Initialization
  Wire.begin(I2C_SDA, I2C_SCL);
  if (!ads.begin()) {
    Serial.println("ADS1115 not found on pins 27/22!");
  } else {
    ads.setGain(GAIN_ONE);
    Serial.println("ADS1115 ready.");
  }

  // 2. Display Initialization
  tft.init();
  tft.setRotation(1);
  tft.invertDisplay(false); 
  tft.setSwapBytes(true);
  tft.fillScreen(TFT_BLACK);

  // 3. Loading images from SD card
  Serial.println("Mounting SD card...");
  if (!SD.begin(SD_CS)) {
    Serial.println("SD card error!");
    tft.fillScreen(TFT_RED);
    while(1); 
  }

  TJpgDec.setJpgScale(1);
  TJpgDec.setCallback(tft_output);

  for (int i = 0; i < 7; i++) {
    int x = (i % colCount) * imgW;
    int y = (i / colCount) * imgH;
    char filename[20];
    sprintf(filename, "/%d.jpg", i + 1);
    
    if (TJpgDec.drawSdJpg(x, y, filename) != 0) {
       tft.drawRect(x, y, imgW, imgH, TFT_WHITE);
       tft.drawNumber(i+1, x + 50, y + 20, 4);
    }
  }

  // Release SD card bus
  SD.end(); 
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  // 4. Touch Initialization
  touchSPI.begin(XPT_CLK, XPT_MISO, XPT_MOSI, XPT_CS);
  touch.begin(touchSPI);
  touch.setRotation(1);

  // 5. ESP-NOW Initialization
  WiFi.mode(WIFI_STA);
  if (esp_now_init() == ESP_OK) {
    esp_now_peer_info_t peerInfo = {};
    peerInfo.channel = 0;  
    peerInfo.encrypt = false;
    
    // Register all peers from the list
    for (int i = 0; i < receiverCount; i++) {
      memcpy(peerInfo.peer_addr, receiverAddresses[i], 6);
      esp_now_add_peer(&peerInfo);
    }
    Serial.println("ESP-NOW initialized for specific MAC addresses.");
  }

  myData.id = 0;
  Serial.println("System running...");
}

// --- LOOP ---
void loop() {
  // 1. Read analog joysticks via ADS1115
  for (int i = 0; i < 4; i++) {
    int16_t raw = ads.readADC_SingleEnded(i);
    
    // ADS1115 at GAIN_ONE reads 0 to ~26400 with 3.3V supply.
    // Map this range directly to 0-180. Center (13200) maps to 90.
    int mappedVal = map(raw, 0, 26400, 0, 180);
    
    // Deadzone management around the center (90)
    if (mappedVal > (90 - 3) && mappedVal < (90 + 3)) {
      mappedVal = 90;
    }

    myData.joy[i] = constrain(mappedVal, 0, 180);
  }

  // 2. Handle display touch
  if (touch.touched()) {
    TS_Point p = touch.getPoint();
    
    // Map touch coordinates to pixels
    int pixX = map(p.x, 300, 3800, 0, 320);
    int pixY = map(p.y, 200, 3700, 0, 240);

    int col = pixX / imgW;
    int row = pixY / imgH;
    int currentID = (row * colCount) + col + 1;

    if (currentID >= 1 && currentID <= 7 && currentID != lastID) {
      if (lastID != -1) drawSelectionFrame(lastID, TFT_BLACK);
      drawSelectionFrame(currentID, TFT_GREEN);
      lastID = currentID;
      myData.id = currentID;
    }
  }

  // 3. Serial Monitor debug output (every 200ms)
  static unsigned long lastDebug = 0;
  if (millis() - lastDebug > 200) {
    Serial.printf("ID: %d | J1[X:%3d Y:%3d] | J2[X:%3d Y:%3d]\n", 
                  myData.id, myData.joy[0], myData.joy[1], myData.joy[2], myData.joy[3]);
    lastDebug = millis();
  }

  // 4. Send data via ESP-NOW (every 50ms) to specific addresses
  static unsigned long lastSend = 0;
  if (millis() - lastSend > 50) {
    for (int i = 0; i < receiverCount; i++) {
      esp_now_send(receiverAddresses[i], (uint8_t *) &myData, sizeof(myData));
    }
    lastSend = millis();
  }

  delay(5); // Stability delay
}
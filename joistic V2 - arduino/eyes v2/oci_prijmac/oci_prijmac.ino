#include <esp_now.h>
#include <WiFi.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include <SD.h>

TFT_eSPI tft = TFT_eSPI();

#define SD_CS 5

// --- ALLOWED MAC ADDRESS CONFIGURATION ---
uint8_t allowedMacAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Dimensions of your images
const int imgW = 320;
const int imgH = 240;

// Variable to track the last displayed image
int lastDisplayedID = -1;

// Data structure (MUST BE IDENTICAL TO THE TRANSMITTER)
typedef struct struct_message {
    int id;
    int joy[4]; 
} struct_message;

struct_message incomingData;

// Function called by the decoder for rendering
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (y >= tft.height()) return false;
  tft.pushImage(x, y, w, h, bitmap);
  return true;
}

// Function called when data is received via ESP-NOW
void OnDataRecv(const uint8_t * mac, const uint8_t *data, int len) {
  // --- MAC ADDRESS CHECK ---
  for (int i = 0; i < 6; i++) {
    if (mac[i] != allowedMacAddress[i]) {
      return; // If MAC doesn't match, ignore data
    }
  }

  // Copy received data into our structure
  memcpy(&incomingData, data, sizeof(incomingData));
  
  int receivedID = incomingData.id;

  // MAIN CONDITION: Redraw only if ID changed and is within range
  if (receivedID >= 1 && receivedID <= 7 && receivedID != lastDisplayedID) {
    char filename[20];
    sprintf(filename, "/%d.jpg", receivedID);
    
    Serial.print("Image change to ID: "); Serial.println(receivedID);
    
    // Rendering the image (TJpgDec handles the rest)
    // If your transmitter sends ID 1-7, the SD card must contain 1.jpg to 7.jpg
    if (TJpgDec.drawSdJpg(0, 0, filename) != 0) {
       Serial.println("Error reading JPG from SD!");
    }
    
    lastDisplayedID = receivedID; // Remember what was rendered
  }

  // You can add servo control using incomingData.joy[0-3] here
  // Serial.printf("Joystick J1X: %d\n", incomingData.joy[0]);
}

void setup() {
  Serial.begin(115200);

  // Display initialization
  tft.init();
  tft.setRotation(3); // 1 for right eye or 3 for eye orientation
  tft.fillScreen(TFT_BLACK);
  tft.invertDisplay(false); 

  // SD card initialization
  if (!SD.begin(SD_CS)) {
    Serial.println("SD card failed!");
    tft.setTextColor(TFT_RED);
    tft.drawString("SD ERROR", 10, 10, 4);
    return;
  }

  // Decoder setup
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true); 
  TJpgDec.setCallback(tft_output);

  // Wi-Fi and ESP-NOW setup
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Register data receive callback function
  esp_now_register_recv_cb(OnDataRecv);

  Serial.println("Receiver ready.");
  Serial.print("MAC ADDRESS: ");
  Serial.println(WiFi.macAddress());
  
  tft.setTextColor(TFT_WHITE);
  tft.drawCentreString("WAITING FOR DATA...", 160, 110, 2);
}

void loop() {
  // Loop remains empty, everything is handled by the callback upon data arrival
}
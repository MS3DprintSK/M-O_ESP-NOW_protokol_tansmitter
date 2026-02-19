#include "WiFi.h"

void setup() {
  Serial.begin(115200);
  delay(1000);

  // WiFi modul musí byť v určitom režime, aby vrátil MAC adresu
  WiFi.mode(WIFI_STA); 

  Serial.println();
  Serial.print("ESP32 MAC adresa: ");
  Serial.println(WiFi.macAddress());
}

void loop() {
  // Tu nemusíme nič robiť, adresu stačí vypísať raz pri štarte
}
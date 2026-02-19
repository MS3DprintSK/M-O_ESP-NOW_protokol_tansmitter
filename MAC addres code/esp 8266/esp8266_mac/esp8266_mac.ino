#include <ESP8266WiFi.h>

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Nastavíme režim stanice (Station mode)
  WiFi.mode(WIFI_STA);

  Serial.println();
  Serial.print("ESP8266 MAC adresa: ");
  Serial.println(WiFi.macAddress());
}

void loop() {
  // Loop nechávame prázdny
}
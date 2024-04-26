#include <WiFi.h>
#include <ESPping.h>
#include "wifi_passwords.h"

const char *ssid_Router = ROUTER_SSID;
const char *password_Router = ROUTER_PASSWORD;

void setup() {
  Serial.begin(115200);
  while(!Serial);
  Serial.println("Setup start");
  WiFi.begin(ssid_Router, password_Router);
  Serial.println(String("Connecting to ") + ssid_Router);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected, IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("Setup End");
}
void loop() {
  if(WiFi.status()){
    Serial.println("Pinging 8.8.8.8...");
    bool success = false;
    for(int attempt = 1; attempt <= 3 && !success; ++attempt){
      IPAddress google_dns (8, 8, 8, 8);
      success = Ping.ping(google_dns);
      Serial.print("Attempt " + String(attempt) + String(": "));
      if(success){
        Serial.println(String("Ping took: ") + String(Ping.averageTime()) + String("ms"));
      }else{
        Serial.println("FAILED");
      }
    }
  }else{
    Serial.println("Wifi not connected. Attempting to reconnect!");
    WiFi.begin(ssid_Router, password_Router);
    Serial.println(String("Connecting to ") + ssid_Router);
    while (WiFi.status() != WL_CONNECTED) {
      Serial.print(".");
    }
  }
  delay(5000);
}
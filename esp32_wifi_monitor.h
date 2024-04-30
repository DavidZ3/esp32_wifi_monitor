#ifndef ESP_32_WIFI_MONITOR_H
#define ESP_32_WIFI_MONITOR_H
#include <arduino.h>

void setupI2cDisplay();
void setupStatusLED();
void setupWifiConnection();
void setupRTC();
void setupSdCard();
void setup();
void lcdBacklightToggle();
void printLocalTime();
void printAndLog(String str);

#endif /* ESP_32_WIFI_MONITOR_H */
#include <ESPping.h>
#include <WiFi.h>
#include <time.h>
#include <led_strip.h>
#include "esp32_wifi_monitor.h"
#include "wifi_passwords.h"

#include "sd_read_write.h"
#include "SD_MMC.h"
#define SD_MMC_CMD 38 //Please do not modify it.
#define SD_MMC_CLK 39 //Please do not modify it.
#define SD_MMC_D0 40 //Please do not modify it.

#define SD_STATUS_LED 48

const char *ssidRouter = ROUTER_SSID;
const char *passwordRouter = ROUTER_PASSWORD;

const char *ntpServer = "pool.ntp.org";
const long gmtOffsetSec = 36000; // GMT +10
const int daylightOffsetSec = 0;
u16_t tick_count = 0;

void printLocalTime() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        return;
    }
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

void setupStatusLED(){
    pinMode(SD_STATUS_LED, OUTPUT);
    neopixelWrite(SD_STATUS_LED, 255, 255, 255);
}

void setupWifiConnection() {
    WiFi.begin(ssidRouter, passwordRouter);
    Serial.println(String("Connecting to ") + ssidRouter);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConnected, IP address: ");
    Serial.println(WiFi.localIP());
}

void setupRTC() {
    // Init and get the time
    Serial.println("Configuring RTC with timeserver.");
    configTime(gmtOffsetSec, daylightOffsetSec, ntpServer);
    printLocalTime();
}

void setupSdCard(){
    SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0);
    if (!SD_MMC.begin("/sdcard", true, true, SDMMC_FREQ_DEFAULT, 5)) {
        Serial.println("Card Mount Failed");
        return;
    }
    uint8_t cardType = SD_MMC.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("No SD_MMC card attached");
        return;
    }
    Serial.print("SD_MMC Card Type: ");
    if (cardType == CARD_MMC) {
        Serial.println("MMC");
    } else if (cardType == CARD_SD) {
        Serial.println("SDSC");
    } else if (cardType == CARD_SDHC) {
        Serial.println("SDHC");
    } else {
        Serial.println("UNKNOWN");
    }
    uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
    Serial.printf("SD_MMC Card Size: %lluMB\n", cardSize);
}

void setup() {
    Serial.begin(115200);
    while (!Serial)
        ;
    Serial.println("Setup start");
    setupStatusLED();
    setupWifiConnection();
    setupRTC();
    setupSdCard();
    Serial.println("Setup End");
}

void printAndLog(String str){
    Serial.print(str);
    appendFile(SD_MMC, "/log.txt", str.c_str());
}

void loop() {
    if(!canOpen(SD_MMC, "/log.txt")){
        neopixelWrite(SD_STATUS_LED, 50, 0, 0);
        SD_MMC.end();
        setupSdCard();
    }

    String timeStr;
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        timeStr = String("Unknown Time: ");
    }else{
        char timeBuffer[64];
        const char *format = "%Y-%m-%d %H:%M:%S: ";
        strftime(timeBuffer, 64, format, &timeinfo);
        timeStr = String(timeBuffer);
    }

    boolean success = false;
    if (WiFi.status()) {
        IPAddress googleDns(8, 8, 8, 8);
        success = Ping.ping(googleDns);
        if (success) {
            printAndLog(timeStr + String(Ping.averageTime()) + String("ms\n"));
            neopixelWrite(SD_STATUS_LED, 0, 50, 0);
        } else {
            printAndLog(timeStr + String("FAILED\n"));
            neopixelWrite(SD_STATUS_LED, 0, 0, 50);
        }
    } else {
        // Serial.println("Wifi not connected. Attempting to reconnect!");
        printAndLog(timeStr + String("Wifi not connected. Attempting to reconnect!"));
        WiFi.begin(ssidRouter, passwordRouter);
        Serial.println(String("Connecting to ") + ssidRouter);
        while (WiFi.status() != WL_CONNECTED) {
            Serial.print(".");
        }
    }
    if(tick_count >= 360){
        if(success){
            setupRTC();
            tick_count = 0;
        }
    }else{
        tick_count++;
    }
    delay(5000);
}
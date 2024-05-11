#include <ESPping.h>
#include <WiFi.h>
#include <Wire.h>
#include <led_strip.h>
#include <time.h>
#include "LiquidCrystal_I2C.h"
#include "SD_MMC.h"
#include "esp32_wifi_monitor.h"
#include "sd_read_write.h"
#include "wifi_passwords.h"

// #define FAKE_SUPERSPEED_TEST_TIME
#define SET_LED_RED neopixelWrite(SD_STATUS_LED, 50, 0, 0)
#define SET_LED_GREEN neopixelWrite(SD_STATUS_LED, 0, 1, 0)
#define SET_LED_BLUE neopixelWrite(SD_STATUS_LED, 0, 0, 50)
#define SET_LED_WHITE neopixelWrite(SD_STATUS_LED, 50, 50, 50)

// I2C LCD Display
#define TOGGLE_DISPLAY_BACKLIGHT_BUTTON_PIN 0
#define SDA 14
#define SCL 13
#define LCD_I2C_ADDR 0x3F
LiquidCrystal_I2C lcd(LCD_I2C_ADDR, 16, 2);

bool lcdBacklight = 0;
bool doBacklightUpdate = 0;

// SD Card
#define SD_MMC_CMD 38  // Please do not modify it.
#define SD_MMC_CLK 39  // Please do not modify it.
#define SD_MMC_D0 40   // Please do not modify it.

// Onboard RGB LED
#define SD_STATUS_LED 48

// WIFI User/Pass
const char *ssidRouter = ROUTER_SSID;
const char *passwordRouter = ROUTER_PASSWORD;

// Time Server and RTC Settings
const char *ntpServer = "pool.ntp.org";
const long gmtOffsetSec = 36000;  // GMT +10
const int daylightOffsetSec = 0;

// Used for uptime/downtime statistics
struct tm bootTime;
struct tm currentStatusTime;
bool netOkay = true;
bool prevPingPassed = true;
static long totalDrops = 0;

void setupI2cDisplay() {
    Wire.begin(SDA, SCL);
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("Setting up...");
}

void setupButtonInterrupt() {
    pinMode(TOGGLE_DISPLAY_BACKLIGHT_BUTTON_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(TOGGLE_DISPLAY_BACKLIGHT_BUTTON_PIN),
                    lcdBacklightToggle, RISING);
}

void setupStatusLED() {
    pinMode(SD_STATUS_LED, OUTPUT);
    SET_LED_WHITE;
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

void setupSdCard() {
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

static String bootTimeStr;
void setup() {
    Serial.begin(115200);
    while (!Serial)
        ;
    Serial.println("Setup start");
    setupI2cDisplay();
    setupButtonInterrupt();
    setupStatusLED();
    setupWifiConnection();
    setupRTC();
    setupSdCard();

    // Setup Current Time as Boot-Time and Current Status Time
    getLocalTime(&bootTime);
    getLocalTime(&currentStatusTime);

    // struct tm timeinfo;
    // if (!getLocalTime(&timeinfo)) {
    //     bootTimeStr = String("Boot:??:??");
    // } else {
    //     char timeBuffer[64];
    //     const char *format = "Boot:%H:%M";
    //     strftime(timeBuffer, 64, format, &timeinfo);
    //     bootTimeStr = String(timeBuffer);
    // }

    // String firstLineStr = bootTimeStr + String(" ") + String(totalDrops);
    // lcd.setCursor(0, 0);
    // lcd.clear();
    // lcd.print(firstLineStr);

    Serial.println("Setup End");

    /*
    BaseType_t xTaskCreatePinnedToCore(
        TaskFunction_t pvTaskCode, const char *const pcName,
        const uint32_t usStackDepth, void *const pvParameters,
        UBaseType_t uxPriority, TaskHandle_t *const pvCreatedTask,
        const BaseType_t xCoreID);
    */
    xTaskCreate(lcdDisplayUpdateTask, "Update LCD Display Task", 5000, NULL, 4,
                NULL);

    xTaskCreate(wifiPingAndLogTask, "Wifi Ping And Log Task", 15000, NULL, 3,
                NULL);
    xTaskCreate(sdCardRemountTask, "Remount SD card if needed", 5000, NULL, 2,
                NULL);
    xTaskCreate(updateRTCTask, "Update the RTC", 2000, NULL, 2,
                NULL);
    // xTaskCreatePinnedToCore(lcdDisplayUpdateTask, "Update LCD Display Task",
    //                         5000, NULL, 1, NULL, 0);

    // xTaskCreatePinnedToCore(wifiPingAndLogTask, "Wifi Ping And Log Task",
    // 10000,
    //                         NULL, 2, NULL, 0);
}

void lcdBacklightToggle() {
    lcdBacklight = !lcdBacklight;
    doBacklightUpdate = 1;
}

void printLocalTime() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        return;
    }
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

void printAndLog(String str) {
    Serial.print(str);
    appendFile(SD_MMC, "/log.txt", str.c_str());
}

void loop() { delay(1000); }

void lcdDisplayUpdateTask(void *parameter) {
    long currTotalDrops = totalDrops;
    static int prevBootTimeStrLen = 0;
    static int prevStatTimeStrLen = 0;
    for (;;) {
        if (doBacklightUpdate) {
            lcd.setBacklight(lcdBacklight);
            doBacklightUpdate = 0;
        }

        // Update for row 1
        // if(currTotalDrops != totalDrops){
        //     currTotalDrops = totalDrops;
        //     String firstLineStr = bootTimeStr + String(" ") + String(totalDrops);
        //     lcd.setCursor(0, 0);
        //     lcd.print(firstLineStr);
        // }

        time_t currTime;
        time(&currTime);
        double diffSeconds = difftime(currTime, mktime(&currentStatusTime));
        int hours = diffSeconds/3600;
        int minutes = (((int) diffSeconds)/60)%60;
        int seconds = ((int) diffSeconds)%60;
        char bootTimeStr[100];
        sprintf(bootTimeStr, "%03d:%02d:%02d %d", hours, minutes, seconds, totalDrops);


        // Update for row 2
        #ifdef FAKE_SUPERSPEED_TEST_TIME
        diffSeconds += 111;
        #else
        time(&currTime);
        diffSeconds = difftime(currTime, mktime(&currentStatusTime));
        #endif
        hours = diffSeconds/3600;
        minutes = (((int) diffSeconds)/60)%60;
        seconds = ((int) diffSeconds)%60;


        char statTimeStr[100];
        if(netOkay){
            sprintf(statTimeStr, "UP: %02d:%02d:%02d", hours, minutes, seconds);
        }else{
            sprintf(statTimeStr, "DOWN: %02d:%02d:%02d", hours, minutes, seconds);
        }

        // Clear LCD Only if the Display Length Has Changed
        if ((strlen(bootTimeStr) != prevBootTimeStrLen) ||
            (strlen(statTimeStr) != prevStatTimeStrLen)) {
                prevBootTimeStrLen = strlen(bootTimeStr);
                prevStatTimeStrLen = strlen(statTimeStr);
                Serial.println(prevBootTimeStrLen);
                Serial.println(prevStatTimeStrLen);
                lcd.clear();
                lcd.setBacklight(lcdBacklight);
        }

        lcd.setCursor(0, 0);
        lcd.print(bootTimeStr);
        lcd.setCursor(0, 1);
        lcd.print(statTimeStr);
        vTaskDelay(200);
    }
}

void sdCardRemountTask(void *parameter) {
    for (;;) {
        if (!canOpen(SD_MMC, "/log.txt")) {
            SET_LED_RED;
            SD_MMC.end();
            setupSdCard();
        }
        vTaskDelay(1000);
    }
}

void updateRTCTask(void *parameter){
    // Update the RTC every 30min if network avaliable.
    // Try again once a minute if unavaliable.
    for(;;){
        vTaskDelay(30*60*1000);
        for (boolean success = false; !success; vTaskDelay(60*1000)) {
            IPAddress googleDns(8, 8, 8, 8);
            success = Ping.ping(googleDns);
        }
        setupRTC();
    }
}

void wifiPingAndLogTask(void *parameter) {
    for (;;) {
        String timeStr;
        struct tm timeinfo;
        if (!getLocalTime(&timeinfo)) {
            timeStr = String("Unknown Time: ");
        } else {
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
                printAndLog(timeStr + String(Ping.averageTime()) +
                            String("ms\n"));
                SET_LED_GREEN;
            } else {
                totalDrops++;
                printAndLog(timeStr + String("FAILED\n"));
                SET_LED_BLUE;
            }
        } else {
            // Serial.println("Wifi not connected. Attempting to reconnect!");
            printAndLog(timeStr +
                        String("Wifi not connected. Attempting to reconnect!"));
            WiFi.begin(ssidRouter, passwordRouter);
            Serial.println(String("Connecting to ") + ssidRouter);
            while (WiFi.status() != WL_CONNECTED) {
                Serial.print(".");
            }
        }
        if(!success && !prevPingPassed && netOkay){
            getLocalTime(&currentStatusTime);
            netOkay = false;
        }else if(success && !netOkay){
            getLocalTime(&currentStatusTime);
            netOkay = true;
        }
        prevPingPassed = success;
        vTaskDelay(5000);
    }
}
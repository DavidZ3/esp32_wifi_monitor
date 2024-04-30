# esp32_wifi_monitor
This a 1 day project I made to monitor my network status as it drops out occationally.
I've used an ESP32S3 I had lying around with the arduino libraries as this is simplier than the espressif libraries and the saving in time is more valuable than the overheads.
It should also be noted that a lot of the libraries/code was copied as program memory was not a concern for this project as it's quite simple for an ESP32.
I initially started with a superloop approach but decided to switch to an RTOS in order to handle delays better and to prevent the ISR from exceeding the watchdog timeout.

## Features
The finished (for now) project has the following features:
* Network Status Monitoring (Pings Google DNS every 5s).
* Setup internal RTC using timeserver (refreshing once every 30min or when network is next avaliable).
* SD card logging in the format of `YYYY-MM-DD HH:MM:SS: ({RRT}ms|FAILED)`.
* LCD Display with the uC boot-time and current uptime/downtime in HH:MM:SS.
* Button with ISR to toggle LCD backlight.

## Image of Final Project
![Image of Final Project](images/wifi_monitor.jpg)

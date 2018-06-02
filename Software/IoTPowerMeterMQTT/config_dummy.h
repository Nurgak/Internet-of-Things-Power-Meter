/*
  IoTPowerMeter - Software for the IoTPowerMeter based on the ESP8266

  Copyright (c) 2016 Karl Kangur. All rights reserved.
  This file is part of IoTPowerMeter.

  IoTPowerMeter is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  IoTPowerMeter is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with IoTPowerMeter.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
 * File:    config_dummy.h
 * Author:  Karl Kangur <karl.kangur@gmail.com>
 * Licnece: GPL
 * URL:     https://github.com/Nurgak/Internet-of-Things-Power-Meter
 */

#ifndef CONFIG_H
#define CONFIG_H

/*** WARNING: ESP-12 and ESP-12E have their GPIO4/5 swapped, so define the correct pins here ***/

// Light sensor pin (do not use GPIO16, it cannot do interrupts)
#define SENSOR_PIN 5
// Button is used for entering programming mode or to toggle between functions (GPIO0)
#define BUTTON_PIN 0
// Chip select pin for the SD card reader
#define SD_CS_PIN 16
// Pin connected to the LED (serial TX)
#define LED_PIN 1
// I2C data and clock pins for the screen
#define PIN_SDA 4
#define PIN_SCL 2

// How often the internal time is synchronised with the NTP server
#define TIME_SYNC_PERIOD 12*60*60 // [s]
// NTP server from which to fetch the time
static const char * ntpServerName = "time.nist.gov";

#define ENABLE_INTERNET // Comment thus line to remove all internet related functionalities
static const char * wifi_ssid = "...";
static const char * wifi_password = "...";

// MQTT server configuration
static const char * mqtt_server = "192.168.1.3";
static const unsigned int mqtt_port = 1883;
static const char * hostName = "IoTPowerMeter";

// Global constants, no magic numbers
#define TIME_WIFI_CONNECT 5000 // Maximum waiting time in seconds for Wi-Fi connection
#define TIME_SYNC_RESPONSE 3000 // Maximum time the system will wait for a response from the NTP server (milliseconds)
#define TIME_BUTTON_PRESS_SHORT 100 // Time in milliseconds for the short button press routine to execute
#define TIME_BUTTON_PRESS_LONG 2000 // Time in milliseconds for the long button press routine to execute
#define TIME_DEBOUNCE 200 // Time in milliseconds during which LED blinks are ignored when one was just detected
#define SCREEN_TITLE_COLUMN 0 // Horizontal position from which the title should be displayed on screen
#define SCREEN_START_COLUMN 6 // Horizontal position from which the data should be displayed on screen
#define SCREEN_ROW_STAT  0
#define SCREEN_ROW_IP    1
#define SCREEN_ROW_DATE  2
#define SCREEN_ROW_TIME  3
#define SCREEN_ROW_NOW   4
#define SCREEN_ROW_TODAY 5
#define SCREEN_ROW_HEAP  6
#define NTP_PACKET_SIZE 48

// For debugging
/*#define DEBUG_SERIAL Serial
#define DEBUG_BAUDRATE 115200
#define DEBUGV(...) ets_printf(__VA_ARGS__)*/

#endif

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
 * File:    config_default.h
 * Author:  Karl Kangur <karl.kangur@gmail.com>
 * Licnece: GPL
 * URL:     https://github.com/Nurgak/Internet-of-Things-Power-Meter
 */

// This file contains default configuration values, change them and then rename the file to "config.h"

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
static const char * wifi_ssid = "SSID";
static const char * wifi_password = "password";

//#define ENABLE_STATIC_IP // Comment this line for dynamic IP
static const uint8_t wifi_ip[] = {192, 168, 0, 123};     // The IP you want to give to your device
static const uint8_t wifi_gateway[] = {192, 168, 0, 1};  // Router IP
static const uint8_t wifi_subnet[] = {255, 255, 255, 0}; // You probably do not need to change this one

#define PUSH_GOOGLE_SPREADSHEETS // Comment this line to disable logging to Google Spreadsheets
static const char * googleSpreadSheetsScript = "/macros/s/SCRIPT_ID/exec";

// Name of the host, use to connect to device via "http://power.local" instead of using the IP address locally
static const char* localHostName = "power";

// Username and password to access the server
#define ENABLE_AUTHENTIFICATION
static const char * http_username = "admin";
static const char * http_password = "admin";

// For debugging
//#define DEBUG_SERIAL Serial
//#define DEBUGV(...) ets_printf(__VA_ARGS__)

#endif


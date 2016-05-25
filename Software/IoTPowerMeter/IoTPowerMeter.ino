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
 * File:    IoTPowerMeter.ino
 * Author:  Karl Kangur <karl.kangur@gmail.com>
 * Licnece: GPL
 * URL:     https://github.com/Nurgak/Internet-of-Things-Power-Meter
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <TimeLib.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>

#include "config.h"
#include "IoTPowerMeter.h"
#include "ESP_SSD1306.h"
#include "server.h"
#include "push.h"

// Global instances
IPAddress ip;
ESP_SSD1306 display;

// Global variables
static volatile uint16_t powerCounter          = 0;  // Internal counter [Wh]
static volatile uint16_t powerCounterNow       = 0;  // Instant power usage [Wh]
static volatile uint16_t powerCounterToday     = 0;  // Total day power usage [Wh]
static volatile uint8_t screenUpdateFieldFlags = 0;

void setup(void)
{
  // See config.h file to enable/disable debugging
  #ifdef DEBUG_SERIAL
  DEBUG_SERIAL.begin(DEBUG_BAUDRATE);
  DEBUG_SERIAL.setDebugOutput(true);
  #endif
  
  // Initialise the screen
  display.begin();
  
  // Load a dummy screen
  display.blank();
  
  #ifdef ENABLE_INTERNET
  // Try to connect to the access point (AP)
  if(WiFi.status() != WL_CONNECTED)
  {
    screenStatus("Connecting...");
    while(!setupWiFi())
    {
      screenStatus("WiFi error");
      ESP.restart();
      delay(1000);
    }
    screenStatus("Connected");
  }
  
  // Initialise the SD card
  if(!SD.begin(SD_CS_PIN))
  {
    screenStatus("No SD card");
    // Require a reset, inserting an SD card while in opertation does not work well
    while(1){};
  }
  
  initServer();
  
  // Set up the internal clock synchronizing function with a Network Time Protocol (NTP) server
  setSyncProvider(syncTime);
  setSyncInterval(TIME_SYNC_PERIOD);
  #else
  // Turn off WiFi
  WiFi.disconnect(true);
  #endif
  
  // Reset the counters on the screen
  screenUpdateFieldFlags |= TODAY | NOW;
  
  // Attach the interrupt that counts the used Watts
  attachInterrupt(SENSOR_PIN, blinkWatt, FALLING);
  // Attach the interrupt for the button press, attached to GPIO0
  // GPIO0 has an external pull-up 
  attachInterrupt(BUTTON_PIN, buttonPress, FALLING);
}

void loop(void)
{
  // Define and initialise with invalid values so they will get updated right away
  static uint8_t currentMinute = 0xff;
  static uint8_t currentDay = 0xff;
  static uint8_t currentHour = 0xff;
  static uint16_t powerCounterHour = 0; // Power counter for the current hour
  static uint16_t powerCounterHourTemp = 0; // Temporary variable for pushing data to the internet
  static uint16_t powerCounterTemp = 0; // Temporary variable for logging
  static bool uploadData = false; // Temporary variable indicating if data should be uploaded
  static char buffer[32]; // Buffer for log messages

  #if defined(ENABLE_INTERNET) && defined(PUSH_GOOGLE_SPREADSHEETS)
  static GoogleSpreadsheets gs(googleSpreadSheetsScript);
  #endif
  
  // Handle client connecting to server
  #ifdef ENABLE_INTERNET
  handleClient();
  #endif

  // A new minute has happened! Log, save and reset
  if(currentMinute != minute())
  {
    currentMinute = minute();
    
    // Log the last minute data to SD card
    // Copy the counter to a temporary variable and reset it
    // TODO: these actions should be atomic, a blink can potentially happen between these instructions and be lost
    powerCounterTemp = powerCounter;
    powerCounter = 0;

    // Cumulative hour counter to log data hourly
    powerCounterHour += powerCounterTemp;

    // Get the previous minute value by removing 60 seconds from current minute
    // Second precision does not matter
    time_t timeLogEntry = now() - 60;
    logData(timeLogEntry, powerCounterTemp);

    // Reset the today counter when the day changes
    if(day() != currentDay)
    {
      sprintf(buffer, "Day power usage: %dWh", powerCounterToday);
      logEvent(buffer);
      
      currentDay = day();
      powerCounterToday = 0;
    }

    // Hourly counter
    if(currentHour != hour())
    {
      currentHour = hour();
      powerCounterHourTemp = powerCounterHour;
      powerCounterHour = 0;
      
      // This is used to upload data hourly further down below, but it might take time so just set a flag here
      uploadData = true;
    }

    #ifdef ENABLE_INTERNET
    // Check that the device is still connected to the WiFi
    if(WiFi.status() != WL_CONNECTED)
    {
      screenStatus("Connecting...");
      if(!setupWiFi())
      {
        screenStatus("WiFi error");
      }
    }

    if(WiFi.status() == WL_CONNECTED)
    {
      // Try to sync time if it is unsychronised (invalid time)
      if(timeStatus() == timeNotSet)
      {
        // Will try to sync the time and set the appropriate state for timeStatus()
        setSyncProvider(syncTime);
      }
      
      #if defined(PUSH_GOOGLE_SPREADSHEETS)
      if(uploadData)
      {
        uploadData = false;
        
        // Try to push data to Google Spreadsheets
        screenStatus("Uploading data");
        
        // Try a couple of times
        uint8_t retries = 0;
        boolean uploadComplete = false;
        while(retries++ < MAX_TRIES_UPLOAD)
        {
          DEBUGV("Uploading data, try %d\n", retries);

          // For hourly logs the minutes are not important, but to be sure we have the right hour (the last one) remove 30 minutes (minus the 1 from before too) from now
          // This is because the data uploaded is for the last hour, not the current hour, the server will only keep the hour of the day, the minutes are irrelevant
          time_t timeLogHour = timeLogEntry - 30 * 60;
          if(gs.submit(timeLogHour, powerCounterHourTemp))
          {
            screenStatus("OK");
            
            uploadComplete = true;
            
            sprintf(buffer, "Data uploading tries: %d", retries);
            logEvent(buffer);
            
            break;
          }
        }

        if(!uploadComplete)
        {
          screenStatus("Upload failed");
          logEvent("Upload failed");
        }
      }
      #endif
    }
    #endif
  }

  // Detect long button press
  static uint32_t button_hold_time = 0;
  if(digitalRead(BUTTON_PIN) == LOW && button_hold_time == 0)
  {
    // Button was pressed and the button hold timer was 0, so button push just started
    // Start timeout counter
    button_hold_time = millis();
  }
  else if(digitalRead(BUTTON_PIN) == LOW && millis() - button_hold_time > TIME_BUTTON_PRESS_LONG)
  {
    // Button was held down for TIME_BUTTON_PRESS_LONG milliseconds, execute the long button press routine
    // Reset timeout
    button_hold_time = 0;
    // Execute the long press routine
    buttonPressLong();
  }
  else if(digitalRead(BUTTON_PIN) == HIGH && button_hold_time != 0)
  {
    // Button was released before the TIME_BUTTON_PRESS_LONG milliseconds finished, just reset the counter 
    // Reset timeout when button is released
    button_hold_time = 0;
  }
  
  // Handle screen updates
  screenUpdate();
}

void ICACHE_FLASH_ATTR buttonPress()
{
  // Update all screen fields
  screenUpdateFieldFlags = 0xff;
  // Do not call screenUpdate(), it makes the ESP crash when the button is clicked too much
}

void ICACHE_FLASH_ATTR buttonPressLong()
{
  // TODO: set AP mode to configure Wi-Fi credentials
}

// Show the current action in the STAT field on the screen
void ICACHE_FLASH_ATTR screenStatus(const char * status)
{
  display.clear(SCREEN_ROW_STAT, SCREEN_START_COLUMN);
  display.sendStrXY(status, SCREEN_ROW_STAT, SCREEN_START_COLUMN);
}

// Update data on screen
void ICACHE_FLASH_ATTR screenUpdate()
{
  static char buffer[16] = {0};
  
  // Set invalid values so they are updated right away
  static unsigned char currentSecond = 0xff;
  static unsigned char currentDay = 0xff;
  static uint16_t powerCounterNowTemp = 0;
  static uint16_t powerCounterTodayTemp = 0;
  static uint32_t heapTemp = 0;
  
  if(screenUpdateFieldFlags & SSID)
  {
    display.clear(SCREEN_ROW_SSID, SCREEN_START_COLUMN);
    display.sendStrXY((char*)wifi_ssid, SCREEN_ROW_SSID, SCREEN_START_COLUMN);
  }
  
  // Row 1 is for the status, updated by the screenStatus() function
  
  if(screenUpdateFieldFlags & IP)
  {
    memset(buffer, 0, sizeof(buffer));
    sprintf(buffer, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    display.clear(SCREEN_ROW_IP, SCREEN_START_COLUMN);
    display.sendStrXY(buffer, SCREEN_ROW_IP, SCREEN_START_COLUMN);
  }
  
  // Auto-update when day has changed
  if(currentDay != day() || screenUpdateFieldFlags & DATE)
  {
    currentDay = day();
    memset(buffer, 0, sizeof(buffer));
    sprintf(buffer, "%04d-%02d-%02d", year(), month(), currentDay);
    display.clear(SCREEN_ROW_DATE, SCREEN_START_COLUMN);
    display.sendStrXY(buffer, SCREEN_ROW_DATE, SCREEN_START_COLUMN);
  }
  
  // Auto-update when second has changed
  if(currentSecond != second() || screenUpdateFieldFlags & TIME)
  {
    currentSecond = second();
    memset(buffer, 0, sizeof(buffer));
    sprintf(buffer, "%02d:%02d:%02dUTC", hour(), minute(), currentSecond);
    display.clear(SCREEN_ROW_TIME, SCREEN_START_COLUMN);
    display.sendStrXY(buffer, SCREEN_ROW_TIME, SCREEN_START_COLUMN);
  }
  
  if(powerCounterNow != powerCounterNowTemp || screenUpdateFieldFlags & NOW)
  {
    powerCounterNowTemp = powerCounterNow;
    memset(buffer, 0, sizeof(buffer));
    sprintf(buffer, "%dWh", powerCounterNowTemp);
    display.clear(SCREEN_ROW_NOW, SCREEN_START_COLUMN);
    display.sendStrXY(buffer, SCREEN_ROW_NOW, SCREEN_START_COLUMN);
  }
  
  if(powerCounterToday != powerCounterTodayTemp || screenUpdateFieldFlags & TODAY)
  {
    powerCounterTodayTemp = powerCounterToday;
    memset(buffer, 0, sizeof(buffer));
    sprintf(buffer, "%dWh", powerCounterTodayTemp);
    display.clear(SCREEN_ROW_TODAY, SCREEN_START_COLUMN);
    display.sendStrXY(buffer, SCREEN_ROW_TODAY, SCREEN_START_COLUMN);
  }

  // Update the heap size information if it has changed
  if(heapTemp != ESP.getFreeHeap() || screenUpdateFieldFlags & HEAP)
  {
    heapTemp = ESP.getFreeHeap();
    memset(buffer, 0, sizeof(buffer));
    sprintf(buffer, "%db", heapTemp);
    display.clear(SCREEN_ROW_HEAP, SCREEN_START_COLUMN);
    display.sendStrXY(buffer, SCREEN_ROW_HEAP, SCREEN_START_COLUMN);
  }
  
  // Reset flags
  screenUpdateFieldFlags = 0;
}

// Configure WiFi connection
boolean ICACHE_FLASH_ATTR setupWiFi()
{
  // Buffer to store the temporary log message
  char buffer[32];
  
  if(WiFi.status() == WL_CONNECTED)
  {
    return true;
  }
  
  // Update the access point name on screen
  screenUpdateFieldFlags |= SSID;
  screenUpdate();
  
  sprintf(buffer, "Connecting to AP: %s", wifi_ssid);
  logEvent(buffer);
  
  #ifdef ENABLE_STATIC_IP
  WiFi.config(wifi_ip, wifi_gateway, wifi_subnet);
  #endif
  WiFi.begin(wifi_ssid, wifi_password);
  
  // Wait for connection
  uint8_t retries = 0;
  while(WiFi.status() != WL_CONNECTED && retries++ < MAX_TRIES_WIFI_CONNECT)
  {
    DEBUGV("WiFi status: %d\n", WiFi.status());
    
    if(WiFi.status() == WL_CONNECT_FAILED)
    {
      return false;
    }
    
    // Wait a second between retries
    delay(1000);
  }
  
  if(WiFi.status() != WL_CONNECTED)
  {
    logEvent("Failed to connect to WiFi");
    return false;
  }

  // Save situation to log file
  sprintf(buffer, "Wifi connect tries: %d", retries);
  logEvent(buffer);
  
  ip = WiFi.localIP();
  
  sprintf(buffer, "Device IP: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
  logEvent(buffer);
  
  screenUpdateFieldFlags |= IP;
  screenUpdate();
  
  // Multicast DNS so that the device could be accessed via a host name instead of IP
  if(MDNS.begin(localHostName))
  {
    MDNS.addService("http", "tcp", 80);
  }
  
  return true;
}

// Synchronise local time with an Network Time Protocol (NTP) server
time_t ICACHE_FLASH_ATTR syncTime()
{
  WiFiUDP udp;
  IPAddress timeServerIP;
  
  byte packetBuffer[NTP_PACKET_SIZE] = {0};
  time_t currentTime = 0;
  
  screenStatus("Time sync.");

  uint8_t retries = MAX_TRIES_TIME_SYNC;
  while(retries--)
  {
    udp.begin(2390);

    DEBUGV("Time sync try %d\n", MAX_TRIES_TIME_SYNC - retries);
    
    // Get the IP from the address
    WiFi.hostByName(ntpServerName, timeServerIP);
    
    // Send an NTP packet to a time server
    // Clean the buffer
    memset(packetBuffer, 0, NTP_PACKET_SIZE);
    
    packetBuffer[0] = 0b11100011;  // LI, Version, Mode
    packetBuffer[1] = 0;           // Stratum, or type of clock
    packetBuffer[2] = 6;           // Polling Interval
    packetBuffer[3] = 0xEC;        // Peer Clock Precision
    // 8 bytes of zero for Root Delay & Root Dispersion
    packetBuffer[12] = 49;
    packetBuffer[13] = 0x4E;
    packetBuffer[14] = 49;
    packetBuffer[15] = 52;

    // Send the packet to port 123 of the NTP server
    udp.beginPacket(timeServerIP, 123);
    udp.write(packetBuffer, NTP_PACKET_SIZE);
    udp.endPacket();
    
    // Clear the buffer for the reply
    memset(packetBuffer, 0, NTP_PACKET_SIZE);
    
    uint8_t waitResponse = MAX_TRIES_TIME_SYNC_RESPONSE;
    while(waitResponse--)
    {
      // No packet receieved yet, wait a second... and try again
      if(!udp.parsePacket())
      {
        // Wait one second between next period
        delay(1000);
        continue;
      }
      
      // Read packet to buffer
      udp.read(packetBuffer, NTP_PACKET_SIZE);
      
      // Only bytes 40 to 43 are important, trash the rest
      // Concatenate 4 bytes and remove 70 years since Unix time starts on Jan 1 1970 and we want 1900
      currentTime = ((packetBuffer[40] << 24) | (packetBuffer[41] << 16) | (packetBuffer[42] << 8) | packetBuffer[43]) - 2208988800UL;
      DEBUGV("Timestamp %d\n", currentTime);
      
      // Break both loops
      retries = 0;
      break;
    }
    
    // Stop and release the resources
    udp.stop();
  }
  
  if(currentTime)
  {
    screenStatus("OK");
  }
  else
  {
    // Time synchronisation was unsuccessful
    screenStatus("Time error");

    // Wait one second to show the message on screen
    delay(1000);
  }
  
  return currentTime;
}

// Blink interrupt, called every time the LED blinks
void ICACHE_FLASH_ATTR blinkWatt()
{
  static uint32_t timeBlinkCurrent = 0;
  static uint32_t timeBlinkLast = 0;
  
  // Update the power counters
  powerCounter++;
  powerCounterToday++;

  // Evaluate the current power consumption 60 seconds * 60 minutes * 1000 milliseconds = h
  timeBlinkCurrent = millis();
  // Avoid dividing by zero (crashes the program otherwise)
  if(timeBlinkCurrent != timeBlinkLast)
  {
    powerCounterNow = 60 * 60 * 1000 / (timeBlinkCurrent - timeBlinkLast);
  }
  timeBlinkLast = timeBlinkCurrent;
  
  // Always update the today and now fields on the display
  screenUpdateFieldFlags |= TODAY | NOW;
}

uint16_t ICACHE_FLASH_ATTR livePowerUsage()
{
  return powerCounterNow;
}

uint16_t ICACHE_FLASH_ATTR todayPowerUsage()
{
  return powerCounterToday;
}

void ICACHE_FLASH_ATTR logData(time_t timestamp, uint16_t power)
{
  // Do not log anything if the time has not been properly synchronised
  if(timeStatus() == timeNotSet)
  {
    return;
  }
  
  // This makes the code crash for some reason, do not test for the existance of the /power folder
  /*String powerFolder = "/power";
  if(!SD.exists(powerFolder))
  {
    SD.mkdir(powerFolder);
  }*/
  
  // Open/make the file name
  char buffer[32];
  sprintf(buffer, "/power/%04d%02d%02d.csv", year(timestamp), month(timestamp), day(timestamp));
  
  // Open the current log file, or create it if it does not exist
  boolean addHeaders = false;
  if(!SD.exists(buffer))
  {
    // Add the headers
    addHeaders = true;
  }
  
  // Open the file for writing (appends data if the file already exists)
  File dataFile = SD.open(buffer, FILE_WRITE);
  if(addHeaders)
  {
    // Add headers if it is a newly created file
    dataFile.println("Timestamp,Power [W*min]");
  }

  // Do not clear the buffer, the following contents are always longer than the file name length
  // YYYYMMDD.csv < YYYY-MM-DDTHH:MMZ,XXX
  
  // Actually write the data to the file in the format of YYYY-MM-DDTHH:MMZ,W*min
  sprintf(
    buffer,
    "%04d-%02d-%02dT%02d:%02dZ,%d",
    year(timestamp),
    month(timestamp),
    day(timestamp),
    hour(timestamp),
    minute(timestamp),
    power
  );
  dataFile.println(buffer);
  
  // This commits the data, otherwise nothing appears in the file
  dataFile.close();
}

void ICACHE_FLASH_ATTR logEvent(char * event)
{
  #ifdef ENABLE_EVENT_LOGGING
  // Open the file for writing (appends data if the file already exists)
  File logFile = SD.open("/log.txt", FILE_WRITE);
  
  // Write the event to the file in the format of YYYY-MM-DDTHH:MMZ - <event>
  char buffer[64];
  sprintf(
    buffer,
    "%04d-%02d-%02dT%02d:%02dZ - %s",
    year(),
    month(),
    day(),
    hour(),
    minute(),
    event
  );
  
  logFile.println(buffer);
  // This commits the log, otherwise nothing appears in the file
  logFile.close();
  #endif
}


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
 * File:    IoTPowerMeterMQTT.cpp
 * Author:  Karl Kangur <karl.kangur@gmail.com>
 * Licnece: GPL
 * URL:     https://github.com/Nurgak/Internet-of-Things-Power-Meter
 */

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <TimeLib.h>
#include <Wire.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>

#include "config.h"
#include "IoTPowerMeterMQTT.h"
#include "ESP_SSD1306.h"

// Global instances
ESP_SSD1306 display;
WiFiClient espClient;
PubSubClient client(espClient);

// Global variables
static volatile uint16_t powerCounterMinute = 0;  // Counter used for logs
static volatile uint16_t powerCounterNow    = 0;  // Counter used for display
static volatile uint16_t powerCounterToday  = 0;  // Counter used for display
static volatile uint16_t powerCounterHour   = 0;  // Counter used for upload

// State transition matrix
const transition_t state_transitions[] = {
  {state_wifi_connect,   OK,   state_ota},
  {state_wifi_connect,   BUSY, state_display_update},
  {state_wifi_connect,   FAIL, state_display_update},
  {state_ota,            OK,   state_time_sync},
  {state_time_sync,      OK,   state_mqtt},
  {state_time_sync,      BUSY, state_display_update},
  {state_time_sync,      FAIL, state_display_update},
  {state_mqtt,           OK,   state_log},
  {state_mqtt,           BUSY, state_mqtt},
  {state_log,            OK,   state_display_update},
  {state_log,            BUSY, state_log},
  {state_log,            FAIL, state_display_update},
  {state_display_update, OK,   state_button_check},
  {state_button_check,   OK,   state_wifi_connect}
};

#define START_STATE state_wifi_connect

void setup(void)
{
  // See config.h file to enable/disable debugging
  #ifdef DEBUG_SERIAL
  DEBUG_SERIAL.begin(DEBUG_BAUDRATE);
  DEBUG_SERIAL.setDebugOutput(true);
  #endif

  // Initialise the screen and load a dummy screen
  display.begin();
  display.blank();

  // Define time syncing periodicity
  setSyncInterval(TIME_SYNC_PERIOD);

  // Attach the interrupt that counts the used Watts
  attachInterrupt(SENSOR_PIN, interrupt_blink, FALLING);
  
  // Show the new firmware upload progress on the display in percent
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
  {
    char buffer[16] = {0};
    sprintf(buffer, "Update: %d%%", (100 * progress / total));
    helper_set_status(buffer);
  });

  // Start the Over-The-Air update process
  ArduinoOTA.setHostname(hostName);
  ArduinoOTA.begin();
}

void loop()
{
  // Pointer to current state function, start with wifi connect
  static STATUS (*current_state)(void) = NULL;
  static STATUS code;
  static const size_t transitions = sizeof(state_transitions) / sizeof(state_transitions[0]);
  static size_t i;

  // Run the state machine
  for(i = 0; i < transitions; i++)
  {
    if(state_transitions[i].state_source == current_state && state_transitions[i].code == code)
    {
      current_state = state_transitions[i].state_destination;
      // Call the state function, get the return code back for next transition
      code = (current_state)();
      // Return immediately
      return;
    }
  }

  // Exception: no transition from state was found, start again
  current_state = START_STATE;
}

STATUS ICACHE_FLASH_ATTR state_wifi_connect()
{
  static STATUS wifi_connect_status = FAIL;
  static uint32_t time_wifi_connect = 0;
  static bool wifi_configured = false;

  // If connected and configuration is correct return immediately
  if(wifi_configured && WiFi.status() == WL_CONNECTED)
  {
    if(wifi_connect_status != OK)
    {
      helper_set_status("OK");
      time_wifi_connect = 0;
      
      client.setServer(mqtt_server, mqtt_port);
  
      wifi_connect_status = OK;
    }
  }
  else if(wifi_connect_status == BUSY)
  {
    if(millis() - time_wifi_connect > TIME_WIFI_CONNECT)
    {
      time_wifi_connect = 0;
      wifi_configured = false;
      wifi_connect_status = FAIL;
    }
    else
    {
      wifi_connect_status = BUSY;
    }
  }
  else
  {
    // Calling WiFi.begin disconnects and prevents the module from connecting
    WiFi.begin(wifi_ssid, wifi_password);

    // Without this the WiFi might connect too fast using an old configuration
    wifi_configured = true;

    // Required to get mDNS working
    MDNS.begin(hostName);

    helper_set_status("Connecting");
    time_wifi_connect = millis();
    wifi_connect_status = BUSY;
  }

  return wifi_connect_status;
}

STATUS ICACHE_FLASH_ATTR state_ota()
{
  // Handle OTA updates
  ArduinoOTA.handle();

  // This state can only return true, the particular cases are taken care by OTA
  // callbacks (start, progress, end...) which must be defined in the setup
  return OK;
}

STATUS ICACHE_FLASH_ATTR state_time_sync()
{
  // Internal time sync state
  static STATUS time_sync_status = FAIL;
  static WiFiUDP udp;
  static byte packetBuffer[NTP_PACKET_SIZE] = {0};
  // Time when the request was started
  static uint32_t time_request = 0;

  // Time can be set, unset or in need of synchronisation modes
  if(timeStatus() == timeSet)
  {
    time_sync_status = OK;
  }
  else if(time_sync_status == BUSY)
  {
    if(udp.parsePacket())
    {
      // Read packet to buffer
      udp.read(packetBuffer, NTP_PACKET_SIZE);

      // Only bytes 40 to 43 are important, trash the rest
      // Concatenate 4 bytes and remove 70 years since Unix time starts on Jan 1 1970 and we want 1900
      time_t currentTime = ((packetBuffer[40] << 24) | (packetBuffer[41] << 16) | (packetBuffer[42] << 8) | packetBuffer[43]) - 2208988800UL;
      setTime(currentTime);

      // Stop and release the resources
      udp.stop();
      // Time has been synchronised with server, return OK
      helper_set_status("OK");
      time_request = 0;
      time_sync_status = OK;
    }
    else if(millis() - time_request > TIME_SYNC_RESPONSE)
    {
      // Stop and release the resources
      udp.stop();
      // Waited for too long for time sync
      helper_set_status("Time error");
      time_request = 0;
      time_sync_status = FAIL;
    }
    else
    {
      time_sync_status = BUSY;
    }
  }
  else
  {
    helper_set_status("Time sync");

    // Synchronise local time with an Network Time Protocol (NTP) server
    IPAddress timeServerIP;

    udp.begin(2390);

    // Get the IP from the address
    WiFi.hostByName(ntpServerName, timeServerIP);

    // Send an NTP packet to a time server
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

    memset(packetBuffer, 0, NTP_PACKET_SIZE);
    time_request = millis();
    time_sync_status = BUSY;
  }

  return time_sync_status;
}

STATUS ICACHE_FLASH_ATTR state_mqtt()
{
  if(!client.connected())
  {
    if(client.connect(hostName))
    {
      // Subscribe to topics here
    }
    return BUSY;
  }
  client.loop();
  
  return OK;
}

STATUS ICACHE_FLASH_ATTR state_log()
{
  // Start logging at the beginning of the next minute/hour
  static uint8_t currentHour = hour();
  static uint8_t currentMinute = minute();
  static uint16_t powerCounterHourTemp = 0;
  static uint16_t powerCounterMinuteTemp = 0;
  static bool fileRemoved = false;

  if(minute() != currentMinute)
  {
    helper_set_status("Logging");

    // TODO These two lines should be atomic, a blink might occur between them
    powerCounterMinuteTemp = powerCounterMinute;
    powerCounterMinute = 0;

    // Remove 60 seconds as data is valid for the previous minute
    time_t timestamp = now() - 60;
    uint16_t minuteOfTheDay = hour(timestamp) * 60 + minute(timestamp);
    char data[16];
    sprintf(data, "%d,%d\n", minuteOfTheDay, powerCounterMinuteTemp);
    client.publish("powerCounterMinute", data);

    currentMinute = minute();

    // Publish the hour counter every new hour (Wh)
    if(hour() != currentHour)
    {
      powerCounterHourTemp = powerCounterHour;
      // Reset the counter for the next hour
      powerCounterHour = 0;

      // Clear the buffer
      memset(data, 0, sizeof(data));
      sprintf(data, "%d,%d\n", hour(timestamp), powerCounterHourTemp);
      client.publish("powerCounterHour", data);

      currentHour = hour();
    }

    // Logging successful
    helper_set_status("OK");
  }

  return OK;
}

STATUS ICACHE_FLASH_ATTR state_button_check()
{
  // Detect short and long button presses by polling the button pin
  static uint32_t button_hold_time = 0;
  if(digitalRead(BUTTON_PIN) == LOW && button_hold_time == 0)
  {
    // Button was pressed and the button hold timer was 0, start counter
    button_hold_time = millis();
  }
  else if(digitalRead(BUTTON_PIN) == HIGH && button_hold_time != 0)
  {
    if(millis() - button_hold_time > TIME_BUTTON_PRESS_LONG)
    {
      // Button was held down for TIME_BUTTON_PRESS_LONG milliseconds or more
      helper_button_long();
    }
    else if(millis() - button_hold_time > TIME_BUTTON_PRESS_SHORT)
    {
      // Button was released before TIME_BUTTON_PRESS_LONG milliseconds
      helper_button_short();
    }

    // Reset counter
    button_hold_time = 0;
  }

  return OK;
}

// Update data on screen
STATUS ICACHE_FLASH_ATTR state_display_update()
{
  static char buffer[16] = {0};

  // Initilise with invalid values so they are updated right away
  static uint8_t currentSecond = 0xff;
  static uint8_t currentDay = 0xff;
  static uint16_t powerCounterNowTemp = 0;
  static uint16_t powerCounterTodayTemp = 0;
  static uint32_t heapTemp = 0;
  static uint32_t sizeTemp = 0;
  static IPAddress ip;

  if(ip != WiFi.localIP())
  {
    ip = WiFi.localIP();
    memset(buffer, 0, sizeof(buffer));
    sprintf(buffer, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    display.clear(SCREEN_ROW_IP, SCREEN_START_COLUMN);
    display.sendStrXY(buffer, SCREEN_ROW_IP, SCREEN_START_COLUMN);
  }

  // Auto-update date if day has changed
  if(currentDay != day())
  {
    currentDay = day();
    memset(buffer, 0, sizeof(buffer));
    sprintf(buffer, "%04d-%02d-%02d", year(), month(), currentDay);
    display.clear(SCREEN_ROW_DATE, SCREEN_START_COLUMN);
    display.sendStrXY(buffer, SCREEN_ROW_DATE, SCREEN_START_COLUMN);
    // Reset today power counter upon day change
    powerCounterToday = 0;
  }

  // Auto-update time if second has changed
  if(currentSecond != second())
  {
    currentSecond = second();
    memset(buffer, 0, sizeof(buffer));
    sprintf(buffer, "%02d:%02d:%02dUTC", hour(), minute(), currentSecond);
    display.clear(SCREEN_ROW_TIME, SCREEN_START_COLUMN);
    display.sendStrXY(buffer, SCREEN_ROW_TIME, SCREEN_START_COLUMN);
  }

  // Auto-update current power counter if it has changed

  if(powerCounterNow != powerCounterNowTemp)
  {
    powerCounterNowTemp = powerCounterNow;
    memset(buffer, 0, sizeof(buffer));
    sprintf(buffer, "%dW", powerCounterNowTemp);
    display.clear(SCREEN_ROW_NOW, SCREEN_START_COLUMN);
    display.sendStrXY(buffer, SCREEN_ROW_NOW, SCREEN_START_COLUMN);

    // Publish the counter via MQTT
    client.publish("powerCounterNow", buffer);
  }

  // Auto-update day power counter if it has changed
  if(powerCounterToday != powerCounterTodayTemp)
  {
    powerCounterTodayTemp = powerCounterToday;
    memset(buffer, 0, sizeof(buffer));
    sprintf(buffer, "%dWh", powerCounterTodayTemp);
    display.clear(SCREEN_ROW_TODAY, SCREEN_START_COLUMN);
    display.sendStrXY(buffer, SCREEN_ROW_TODAY, SCREEN_START_COLUMN);
    
    // Publish the counter via MQTT
    client.publish("powerCounterToday", buffer);
  }

  // Auto-update the heap size information if it has changed
  if(heapTemp != ESP.getFreeHeap())
  {
    heapTemp = ESP.getFreeHeap();
    memset(buffer, 0, sizeof(buffer));
    sprintf(buffer, "%db", heapTemp);
    display.clear(SCREEN_ROW_HEAP, SCREEN_START_COLUMN);
    display.sendStrXY(buffer, SCREEN_ROW_HEAP, SCREEN_START_COLUMN);
  }

  return OK;
}

void ICACHE_FLASH_ATTR helper_button_short()
{
  helper_set_status("Short press");
  client.publish("powerCounterButton", "button press short");
}

void ICACHE_FLASH_ATTR helper_button_long()
{
  helper_set_status("Long press");
  client.publish("powerCounterButton", "button press long");
}

// Show the current action in the STAT field on the screen
void ICACHE_FLASH_ATTR helper_set_status(const char * status)
{
  display.clear(SCREEN_ROW_STAT, SCREEN_START_COLUMN);
  display.sendStrXY(status, SCREEN_ROW_STAT, SCREEN_START_COLUMN);
}

// Blink interrupt, called every time the LED blinks
void ICACHE_FLASH_ATTR interrupt_blink()
{
  static uint32_t timeBlinkLast = 0;
  static uint32_t timeBlinkCurrent = 0;

  // Debounce routine, because sometimes one LED blink gets detected multiple times
  // This is millis() rollover safe as (millis() - timeBlinkCurrent) is never negative
  if((uint32_t)(millis() - timeBlinkLast) < TIME_DEBOUNCE)
  {
    // Do not accept blinks more often than TIME_DEBOUNCE (milliseconds)
    return;
  }

  // Update the power counters
  powerCounterMinute++;
  powerCounterHour++;
  powerCounterToday++;

  // Avoid dividing by zero (crashes the program otherwise)
  timeBlinkCurrent = millis();
  if(timeBlinkCurrent != timeBlinkLast)
  {
    // Evaluate the current power consumption 60 seconds * 60 minutes * 1000 milliseconds = h
    powerCounterNow = 60 * 60 * 1000 / (timeBlinkCurrent - timeBlinkLast);
  }
  timeBlinkLast = timeBlinkCurrent;
}

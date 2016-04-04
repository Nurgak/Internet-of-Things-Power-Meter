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
 * File:    IoTPowerMeter.h
 * Author:  Karl Kangur <karl.kangur@gmail.com>
 * Licnece: GPL
 * URL:     https://github.com/Nurgak/Internet-of-Things-Power-Meter
 */

#ifndef IOT_POWER_METER_H
#define IOT_POWER_METER_H

// Bit field used to refresh parts of display
enum DisplayFields {
  SSID  = 0b00000001,
  STAT  = 0b00000010,
  IP    = 0b00000100,
  DATE  = 0b00001000,
  TIME  = 0b00010000,
  NOW   = 0b00100000,
  TODAY = 0b01000000,
  HEAP  = 0b10000000
};

void screenStatus(const char *);
boolean setupWiFi();
time_t syncTime();
void blinkWatt();
void buttonPress();
void buttonPressLong();
void logData(time_t, uint16_t);
void screenUpdate();
uint16_t livePowerUsage();
uint16_t todayPowerUsage();

#endif


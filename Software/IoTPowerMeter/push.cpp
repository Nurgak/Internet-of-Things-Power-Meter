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
 * File:    push.cpp
 * Author:  Karl Kangur <karl.kangur@gmail.com>
 * Licnece: GPL
 * URL:     https://github.com/Nurgak/Internet-of-Things-Power-Meter
 */

#include <WiFiClientSecure.h>
#include <TimeLib.h>

//#include "config.h"
#include "push.h"

GoogleSpreadsheets::GoogleSpreadsheets(const char * _script)
{
  // Define the Google Spreadsheets host
  host = "script.google.com";
  script = _script;
}

bool GoogleSpreadsheets::submit(time_t time, uint16_t power)
{
  //Serial.println("Submitting data to Google Spreadsheets");
  
  // Connect to Google Spreadsheets
  if (!client.connect(host, 443))
  {
    //Serial.println("Connection failed");
    return false;
  }

  // Check that the certificate matches
  if(!client.verify(fingerprint, host))
  {
    //Serial.println("Certificate not valid");
    return false;
  }

  // Construct the GET request
  String url = String(script) + "?time=" + time + "&power=" + power;

  client.print(String("GET ") + url + " HTTP/1.1\r\nHost: " + host + "\r\nConnection: close\r\n\r\n");
  
  // At this point we might as well assume the request was successful,
  // however for debugging one can view the server response by uncommending the following block:
  /*while(client.connected())
  {
    String line = client.readStringUntil('\n');
    Serial.print(line);
  }*/

  //Serial.println("Sumission succeeded");
  return true;
}


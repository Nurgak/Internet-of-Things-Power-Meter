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
 * File:    push.h
 * Author:  Karl Kangur <karl.kangur@gmail.com>
 * Licnece: GPL
 * URL:     https://github.com/Nurgak/Internet-of-Things-Power-Meter
 */

#ifndef PUSH_H
#define PUSH_H

// Service to push data to the internet in order to update some online database
class Push
{
  protected:
  const char * host;
  
  public:
  virtual bool submit(time_t time, uint16_t power) = 0;
};

// Class to submit the power data to Google Spreadsheets
class GoogleSpreadsheets : public Push
{
  private:
  WiFiClientSecure client;
  const char * script;
  // SHA1 fingerprint of Google Spreadsheets
  const char * fingerprint = "81 50 50 6A 2B 1C 60 02 C2 96 51 57 AC 25 FA C9 51 FD F5 A4";
  
  public:
  GoogleSpreadsheets(const char * _script);
  bool submit(time_t time, uint16_t power);
};

#endif


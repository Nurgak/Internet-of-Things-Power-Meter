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
 * File:    ESP_SSD1306.cpp
 * Author:  Karl Kangur <karl.kangur@gmail.com>
 * Licnece: GPL
 * URL:     https://github.com/Nurgak/Internet-of-Things-Power-Meter
 */

#include <Wire.h>
#include "ESP_SSD1306.h"
#include "font.h"
#include "config.h"

void ICACHE_FLASH_ATTR ESP_SSD1306::begin(void)
{
  // SDA, SCL pin definitions in that order, can be any pin that can do interrupts
  Wire.begin(PIN_SDA, PIN_SCL);
  
  turnOff();
  
  // Reverse displayed data
  send(0xA1); // Bottom to top
  send(0xC8); // Right to left

  // Enable charge pump to 
  send(SSD1306_CHARGEPUMP);
  send(0x14);

  // Clear the screen, turning screen on (inside reset()) is part of charge pump setting
  reset();
}

// Send commands to the display
void ICACHE_FLASH_ATTR ESP_SSD1306::send(unsigned char com)
{
  Wire.beginTransmission(address);
  Wire.write(0x80);
  Wire.write(com);
  Wire.endTransmission();
}

void ICACHE_FLASH_ATTR ESP_SSD1306::sendChar(unsigned char data) 
{
  Wire.beginTransmission(address); // begin transmitting
  Wire.write(0x40);//data mode
  Wire.write(data);
  Wire.endTransmission();    // stop transmitting
}

// Prints a character on the display
void ICACHE_FLASH_ATTR ESP_SSD1306::sendCharXY(unsigned char data, unsigned char x, unsigned char y)
{
  setXY(x, y);
  Wire.beginTransmission(address);
  Wire.write(0x40);
  for(uint8_t i = 0; i < 6; i++)
  {
    Wire.write(pgm_read_byte(font[data - 0x20] + i));
  }
  Wire.endTransmission();
}

void ICACHE_FLASH_ATTR ESP_SSD1306::sendStrXY(const char * string, unsigned char x, unsigned char y)
{
  setXY(x, y);
  unsigned char i = 0;
  while(*string)
  {
    for(i=0;i<6;i++)
    {
      sendChar(pgm_read_byte(font[*string - 0x20] + i));
    }
    *string++;
  }
}

// Set the cursor position
void ICACHE_FLASH_ATTR ESP_SSD1306::setXY(unsigned char row, unsigned char col)
{
  // Set page address
  send(0xb0 + row);
  send(6 * col & 0x0f);       // set low col address
  send(0x10 + ((6 * col >> 4) & 0x0f));  // set high col address
}

// Clears the display by sendind 0 to all the screen map.
void ICACHE_FLASH_ATTR ESP_SSD1306::clear(void)
{
  unsigned char i, k;
  // For every line
  for(k = 0; k < 8; k++)
  {	
    setXY(k, 0);    
    // For every column
    for(i = 0; i < 128; i++)
    {
      // A column of a line is 8 bits
      sendChar(0x0);
    }
  }
}

void ICACHE_FLASH_ATTR ESP_SSD1306::blank(void)
{
  clear();
  sendStrXY("SSID  ?", 0, 0);
  sendStrXY("STAT  ?", 1, 0);
  sendStrXY("IP    ?", 2, 0);
  sendStrXY("DATE  ?", 3, 0);
  sendStrXY("TIME  ?", 4, 0);
  sendStrXY("NOW   ?", 5, 0); // Instant used power [Wh]
  sendStrXY("TODAY ?", 6, 0); // Power used today [Wh]
  sendStrXY("HEAP  ?", 7, 0);
}

// Clear the line y starting from the position x
void ICACHE_FLASH_ATTR ESP_SSD1306::clear(unsigned char row, unsigned char col)
{
  unsigned char i;
  setXY(row, col);    
  // For every column
  for(i = 6*col; i < 128; i++)
  {
    // A column of a line is 8 bits
    sendChar(0x0);
  }
}

void ICACHE_FLASH_ATTR ESP_SSD1306::reset(void)
{
  turnOff();
  clear();
  turnOn();
}

// Turns display on.
void ICACHE_FLASH_ATTR ESP_SSD1306::turnOn(void)
{
  send(SSD1306_DISPLAYON);
}

// Turns display off.
void ICACHE_FLASH_ATTR ESP_SSD1306::turnOff(void)
{
  send(SSD1306_DISPLAYON);
}


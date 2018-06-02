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
 * File:    IoTPowerMeterMQTT.h
 * Author:  Karl Kangur <karl.kangur@gmail.com>
 * Licnece: GPL
 * URL:     https://github.com/Nurgak/Internet-of-Things-Power-Meter
 */

#ifndef IOT_POWER_METER_H
#define IOT_POWER_METER_H

#include <c_types.h>

// Transition structure
struct transition_t
{
    STATUS (*state_source)(void);
    STATUS code;
    STATUS (*state_destination)(void);
};

void interrupt_blink(void);

void helper_set_status(const char *);
void helper_button_short(void);
void helper_button_long(void);

STATUS state_wifi_connect(void);
STATUS state_ota(void);
STATUS state_time_sync(void);
STATUS state_mqtt(void);
STATUS state_log(void);
STATUS state_button_check(void);
STATUS state_display_update(void);

#endif

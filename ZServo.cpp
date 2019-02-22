/**
 * SMuFF Firmware
 * Copyright (C) 2019 Technik Gegg
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "ZServo.h"

bool ZServo::setServoPos(int degree) {
  bool stat = false;
  if(degree >= 0 && degree <= 180) {
    int pulseLen = US_PER_PULSE_0DEG + (US_PER_PULSE_DEGREE * degree);
    int steps = 90;
    for(int i=0; i< steps; i++) 
      setServoMS(pulseLen);
    stat = true;
    _degree = degree;
  }
  return stat;
}

void ZServo::setServoMS(int microseconds) {
    digitalWrite(_pin, HIGH);
    delayMicroseconds(microseconds);
    digitalWrite(_pin, LOW);
    delayMicroseconds(US_PER_PERIOD);
}

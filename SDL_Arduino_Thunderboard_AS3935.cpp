/*
  Modified by SwitchDoc Labs October 2017
*/
/*
  AS3935.cpp - AS3935 Franklin Lightning Sensor™ IC by AMS library
  Copyright (c) 2012 Raivis Rengelis (raivis [at] rrkb.lv). All rights reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 3 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "SDL_Arduino_ThunderBoard_AS3935.h"
// I2c library by Wayne Truchsess
#include "I2C.h"

SDL_Arduino_ThunderBoard_AS3935::SDL_Arduino_ThunderBoard_AS3935(uint8_t irq, uint8_t addr)
{
  //configure the uint16_terrupt
  _IRQPin = irq;
  //configure the address
  _ADDR = addr;
  pinMode(_IRQPin, INPUT);

}

uint8_t SDL_Arduino_ThunderBoard_AS3935::_ffsz(uint8_t mask)
{
  uint8_t i = 0;
  if (mask)
    for (i = 1; ~mask & 1; i++)
      mask >>= 1;
  return i;
}

void SDL_Arduino_ThunderBoard_AS3935::registerWrite(uint8_t reg, uint8_t mask, uint8_t data)
{
#ifdef DEBUG

  Serial.print(F("regW "));
  Serial.print(reg, HEX);
  Serial.print(" ");
  Serial.print(mask, HEX);
  Serial.print(" ");
  Serial.print(data, HEX);
  Serial.print(F(" read "));
#endif
  //read 1
  I2c.read((uint8_t)_ADDR, (uint8_t)reg, (uint8_t)0x01);
  //put it to regval
  uint8_t regval = I2c.receive();
#ifdef DEBUG
  Serial.print(regval, HEX);
#endif
  //do masking
  regval &= ~(mask);
  if (mask)
    regval |= (data << (_ffsz(mask) - 1));
  else
    regval |= data;

#ifdef DEBUG
  Serial.print(F(" write "));
  Serial.println(regval, HEX);

#endif

  //write the register back
  I2c.write(_ADDR, reg, regval);
}

uint8_t SDL_Arduino_ThunderBoard_AS3935::registerRead(uint8_t reg, uint8_t mask)
{
  //read 1 uint8_t
  I2c.read((uint8_t)_ADDR, (uint8_t)reg, (uint8_t)0x01);
  //put it to regval
  uint8_t regval = I2c.receive();
  //mask
  regval = regval & mask;
  if (mask)
    regval >>= (_ffsz(mask) - 1);
  return regval;
}

void SDL_Arduino_ThunderBoard_AS3935::reset()
{
  //write to 0x3c, value 0x96
  I2c.write((uint8_t)_ADDR, (uint8_t)0x3c, (uint8_t)0x96);


  delay(2);
}

bool SDL_Arduino_ThunderBoard_AS3935::calibrate()
{
  uint16_t target = 3125, currentcount = 0, bestdiff = INT_MAX, currdiff = 0;
  uint8_t bestTune = 0, currTune = 0;
  unsigned long setUpTime;
  uint16_t currIrq, prevIrq;
  // set lco_fdiv divider to 0, which translates to 16
  // so we are looking for 31250Hz on irq pin
  // and since we are counting for 100ms that translates to number 3125
  // each capacitor changes second least significant digit
  // using this timing so this is probably the best way to go
  registerWrite(AS3935_LCO_FDIV, 0);
  registerWrite(AS3935_DISP_LCO, 1);
  // tuning is not linear, can't do any shortcuts here
  // going over all built-in cap values and finding the best
  for (currTune = 0; currTune <= 0x0F; currTune++)
  {
    registerWrite(AS3935_TUN_CAP, currTune);
    // let it settle
    delay(2);
    currentcount = 0;
    prevIrq = digitalRead(_IRQPin);
    setUpTime = millis() + 100;
    while ((long)(millis() - setUpTime) < 0)
    {
      currIrq = digitalRead(_IRQPin);
      if (currIrq > prevIrq)
      {
        currentcount++;
      }
      prevIrq = currIrq;
    }
    currdiff = target - currentcount;
    // don't look at me, abs() misbehaves
    if (currdiff < 0)
      currdiff = -currdiff;
    if (bestdiff > currdiff)
    {
      bestdiff = currdiff;
      bestTune = currTune;
    }
  }
  registerWrite(AS3935_TUN_CAP, bestTune);
  delay(2);
  registerWrite(AS3935_DISP_LCO, 0);
  // and now do RCO calibration
  I2c.write((uint8_t)_ADDR, (uint8_t)0x3D, (uint8_t)0x96);
  delay(3);
  // if error is over 109, we are outside allowed tuning range of +/-3.5%
  Serial.print(F("bestTune = "));
  Serial.println(bestTune);
  Serial.print(F("Difference ="));
  Serial.println(bestdiff);
  return bestdiff > 109 ? false : true;
}

void SDL_Arduino_ThunderBoard_AS3935::powerDown()
{
  registerWrite(AS3935_PWD, 1);
}

void SDL_Arduino_ThunderBoard_AS3935::powerUp()
{
  registerWrite(AS3935_PWD, 0);
  I2c.write((uint8_t)_ADDR, (uint8_t)0x3D, (uint8_t)0x96);
  delay(3);
}

uint16_t SDL_Arduino_ThunderBoard_AS3935::interruptSource()
{
  return registerRead(AS3935_INT);
}

void SDL_Arduino_ThunderBoard_AS3935::disableDisturbers()
{
  registerWrite(AS3935_MASK_DIST, 1);

}

void SDL_Arduino_ThunderBoard_AS3935::enableDisturbers()
{
  registerWrite(AS3935_MASK_DIST, 0);

}

uint16_t SDL_Arduino_ThunderBoard_AS3935::getMinimumLightnings()
{
  return registerRead(AS3935_MIN_NUM_LIGH);
}

uint16_t SDL_Arduino_ThunderBoard_AS3935::setMinimumLightnings(uint16_t minlightning)
{
  registerWrite(AS3935_MIN_NUM_LIGH, minlightning);
  return getMinimumLightnings();
}

uint16_t SDL_Arduino_ThunderBoard_AS3935::lightningDistanceKm()
{
  return registerRead(AS3935_DISTANCE);
}

// Added EDB -- This function is shown ina number of other AS3935 libraries, so adding it here.
uint16_t SDL_Arduino_ThunderBoard_AS3935::getEnergy()
{
  return (registerRead(0x06, 0x1F) << 16 | registerRead(0x06, 0x00) << 8 | registerRead(0x04, 0x00));
}

void SDL_Arduino_ThunderBoard_AS3935::setIndoors()
{
  registerWrite(AS3935_AFE_GB, AS3935_AFE_INDOOR);
}

void SDL_Arduino_ThunderBoard_AS3935::setOutdoors()
{
  registerWrite(AS3935_AFE_GB, AS3935_AFE_OUTDOOR);
}

uint16_t SDL_Arduino_ThunderBoard_AS3935::getNoiseFloor()
{
  return registerRead(AS3935_NF_LEV);
}

uint16_t SDL_Arduino_ThunderBoard_AS3935::setNoiseFloor(uint16_t noisefloor)
{
  registerWrite(AS3935_NF_LEV, noisefloor);
  return getNoiseFloor();
}

uint16_t SDL_Arduino_ThunderBoard_AS3935::getSpikeRejection()
{
  return registerRead(AS3935_SREJ);
}

uint16_t SDL_Arduino_ThunderBoard_AS3935::setSpikeRejection(uint16_t srej)
{
  registerWrite(AS3935_SREJ, srej);
  return getSpikeRejection();
}

uint16_t SDL_Arduino_ThunderBoard_AS3935::getWatchdogThreshold()
{
  return registerRead(AS3935_WDTH);
}

uint16_t SDL_Arduino_ThunderBoard_AS3935::setWatchdogThreshold(uint16_t wdth)
{
  registerWrite(AS3935_WDTH, wdth);
  return getWatchdogThreshold();
}

void SDL_Arduino_ThunderBoard_AS3935::clearStats()
{
  registerWrite(AS3935_CL_STAT, 1);
  registerWrite(AS3935_CL_STAT, 0);
  registerWrite(AS3935_CL_STAT, 1);
}



/*
  #include <linux/i2c.h>
  static void i2c_rd(int fd, uint16_t reg, uint8_t *values, uint32_t n)
  {
   int err;
   uint8_t buf[2] = { reg >> 8, reg & 0xff };
   struct i2c_rdwr_ioctl_data msgset;
   struct i2c_msg msgs[2] = {
      {
         .addr = 0x29,
         .flags = 0,
         .len = 2,
         .buf = buf,
      },
      {
         .addr = 0x29,
         .flags = I2C_M_RD,
         .len = n,
         .buf = values,
      },
   };

   msgset.msgs = msgs;
   msgset.nmsgs = 2;

   err = ioctl(fd, I2C_RDWR, &msgset);
  }

*/

/*
  Copyright (c) 2026 moddo inc. All rights reserved.

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.
*/

#pragma once

#include <Arduino.h>

#if !defined(ARDUINO_MODDO_PINCH)
  #error "PinchLowPower is only supported by the moddo pinch board."
#endif

#include "PinchRTC.h"

#define RTC_ALARM_WAKEUP	0xFF
#define RTC_PERIODIC_WAKEUP	0xFE

/**
 * Deep-standby support for pinch.
 *
 * attachInterruptWakeup() configures an ordinary external-interrupt pin as a
 * wake source. It returns false for an invalid pin or mode, a pin without a
 * usable EIC channel, or the NMI-only D1 pin. The pin mode is deliberately
 * left to the sketch so it can use an external pull resistor or INPUT_PULLUP.
 * The overload without a callback installs a safe no-op ISR that clears the
 * EIC flag after wake.
 *
 * attachInterruptWakeup(RTC_PERIODIC_WAKEUP, callback, interval) wakes the
 * board on every RTC periodic tick, where interval is a
 * PinchRTC::Periodic_Interval (128 Hz down to 1 Hz). Each sleep() then
 * returns after the next tick, and sleep(millis) also returns early on a
 * tick. Count ticks in the sketch for periods longer than 1 s.
 * detachInterruptWakeup(RTC_PERIODIC_WAKEUP) stops the ticks.
 *
 * The RTC, EIC and EVSYS share GCLK2 at 1024 Hz, so pin pulses shorter than
 * about 1 ms may be missed in standby.
 */
class PinchLowPowerClass
{
public:
  bool attachInterruptWakeup(uint32_t pin, uint32_t mode = FALLING);
  bool attachInterruptWakeup(uint32_t pin, voidFuncPtr callback, uint32_t mode);
  void detachInterruptWakeup(uint32_t pin);

  // Enter SAMD11 standby.
  void deepSleep();
  void deepSleep(uint32_t millis);
  void deepSleep(int millis) {
    deepSleep((uint32_t)millis);
  }

  // Alias provided for sketches that prefer the ArduinoLowPower-style name.
  void sleep() { deepSleep(); }
  void sleep(uint32_t millis) { deepSleep(millis); }
  void sleep(int millis) { deepSleep(millis); }

private:
  void configureEicStandbyClock();
  void setAlarmIn(uint32_t millis);
  uint8_t wakeMask_ = 0;
  bool periodicWake_ = false;
  PinchRTC rtc;
};

extern PinchLowPowerClass LowPower;

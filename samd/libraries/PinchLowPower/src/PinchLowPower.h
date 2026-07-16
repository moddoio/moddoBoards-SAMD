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

/**
 * Deep-standby support for pinch.
 *
 * attachInterruptWakeup() configures an ordinary external-interrupt pin as a
 * wake source. It returns false for an invalid pin or mode, a pin without a
 * usable EIC channel, or the NMI-only D1 pin. The pin mode is deliberately
 * left to the sketch so it can use an external pull resistor or INPUT_PULLUP.
 * The overload without a callback installs a safe no-op ISR that clears the
 * EIC flag after wake.
 */
class PinchLowPowerClass
{
public:
  bool attachInterruptWakeup(uint32_t pin, uint32_t mode = FALLING);
  bool attachInterruptWakeup(uint32_t pin, voidFuncPtr callback, uint32_t mode);

  // Enter SAMD11 standby. Returns false until a wake source is configured.
  bool deepSleep();

  // Alias provided for sketches that prefer the ArduinoLowPower-style name.
  bool sleep() { return deepSleep(); }

private:
  void configureEicStandbyClock();
  uint8_t wakeMask_ = 0;
};

extern PinchLowPowerClass PinchLowPower;

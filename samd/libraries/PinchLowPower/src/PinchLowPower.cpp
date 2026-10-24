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

#include "PinchLowPower.h"

#include <time.h>

#include <wiring_private.h>

namespace
{
// A callback is required so the core EIC handler clears INTFLAG after wake.
// Supplying this fallback lets a sketch intentionally wake without work.
void clearWakeInterrupt()
{
}

bool isUsableEicLine(uint32_t pin, uint32_t& eicLine)
{
  if (pin >= PINS_COUNT) {
    return false;
  }

  eicLine = static_cast<uint32_t>(GetExtInt(pin));

  // SAMD11 has EXTINT0 through EXTINT7. This also rejects D1's NMI line,
  // unassigned/shared pins, and the internal USB pins.
  return eicLine <= EXTERNAL_INT_7;
}
}

PinchLowPowerClass LowPower;

bool PinchLowPowerClass::attachInterruptWakeup(uint32_t pin, uint32_t mode)
{
  return attachInterruptWakeup(pin, clearWakeInterrupt, mode);
}

bool PinchLowPowerClass::attachInterruptWakeup(uint32_t pin,
                                                voidFuncPtr callback,
                                                uint32_t mode)
{
  if (pin > PINS_COUNT) {
    // check for external wakeup sources
    // RTC library should call this API to enable the alarm subsystem
    if (pin == RTC_ALARM_WAKEUP) {
        if (!rtc.isConfigured()) {
          rtc.begin(false);
        }
        rtc.attachInterrupt(callback);
        return true;
    }
    if (pin == RTC_PERIODIC_WAKEUP) {
        // mode carries the PinchRTC::Periodic_Interval
        if (mode > PinchRTC::PER_1HZ) {
          return false;
        }
        if (!rtc.isConfigured()) {
          rtc.begin(false);
        }
        rtc.attachPeriodicInterrupt(callback ? callback : clearWakeInterrupt);
        rtc.enablePeriodicInterrupt(static_cast<PinchRTC::Periodic_Interval>(mode));
        periodicWake_ = true;
        return true;
    }
    return false;
  }

  if (mode > RISING) {
    return false;
  }

  uint32_t eicLine;
  if (!isUsableEicLine(pin, eicLine)) {
    return false;
  }

  // The core owns pin multiplexing, EIC configuration, and NVIC dispatch.
  // A non-null callback also makes the core clear the EIC flag after wake.
  attachInterrupt(pin, callback ? callback : clearWakeInterrupt, mode);

  configureEicStandbyClock();

  const uint32_t mask = 1u << eicLine;
  wakeMask_ |= mask;
  EIC->INTFLAG.reg = mask;       // Discard an edge that happened during setup.
  EIC->WAKEUP.reg |= mask;       // Permit this EIC line to wake standby mode.
  EIC->INTENSET.reg = mask;
  return true;
}

void PinchLowPowerClass::detachInterruptWakeup(uint32_t pin)
{
  if (pin == RTC_PERIODIC_WAKEUP) {
    rtc.disablePeriodicInterrupt();
    rtc.detachPeriodicInterrupt();
    periodicWake_ = false;
  }
}

void PinchLowPowerClass::configureEicStandbyClock()
{
  PM->APBAMASK.reg |= PM_APBAMASK_EIC;

  // The SAMD11 only implements GCLK0 through GCLK5 and the core uses all but
  // GCLK2. GCLK2 is shared with the RTC, which needs it at 1024 Hz, so the
  // EIC runs from that same clock while the CPU is in standby.
  PinchRTC::configureLowPowerClock();

  // Change the EIC's clock generator only while its generic clock is off.
  GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(GCM_EIC);
  while (GCLK->STATUS.bit.SYNCBUSY) {
  }

  GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(GCM_EIC) |
                      GCLK_CLKCTRL_GEN_GCLK2 |
                      GCLK_CLKCTRL_CLKEN;
  while (GCLK->STATUS.bit.SYNCBUSY) {
  }

  // SAMD11 standby-wake errata workaround: retain enough Flash power for
  // reliable code fetch after the EIC interrupt wakes the MCU.
  NVMCTRL->CTRLB.bit.SLEEPPRM = NVMCTRL_CTRLB_SLEEPPRM_DISABLED_Val;
}


void PinchLowPowerClass::setAlarmIn(uint32_t millis) {

  if (!rtc.isConfigured()) {
    attachInterruptWakeup(RTC_ALARM_WAKEUP, NULL, 0);
  }

  // The RTC counts whole seconds, and an alarm matches only when the clock
  // ticks onto it, so an alarm on the current second never fires. Round up to
  // whole seconds, at least one.
  int seconds = (int)(millis / 1000) + ((millis % 1000) != 0 ? 1 : 0);
  if (seconds == 0) {
    seconds = 1;
  }

  struct tm now;
  struct tm check;
  do {
    rtc.getTm(&now);

    struct tm alarm = now;
    alarm.tm_sec += seconds;

    // setAlarmTm will normalize (roll seconds overflow into minutes, etc.)
    rtc.setAlarmTm(&alarm);

    rtc.enableAlarm(rtc.MATCH_YYMMDDHHMMSS);

    // RTC writes wait on its 1 kHz clock, so a second can tick while the
    // alarm is set, leaving it on a second that has already started. Set it
    // again from the new time if that happened.
    rtc.getTm(&check);
  } while (check.tm_sec != now.tm_sec);
}

void PinchLowPowerClass::deepSleep()
{
  // Remove stale EIC/SysTick requests before sleeping. A level-triggered
  // wake source can legitimately reassert after this point.
  EIC->INTFLAG.reg = wakeMask_;
  NVIC_ClearPendingIRQ(EIC_IRQn);

  if (periodicWake_) {
    EVSYS->INTFLAG.reg = EVSYS_INTFLAG_EVD0 | EVSYS_INTFLAG_OVR0;
    NVIC_ClearPendingIRQ(EVSYS_IRQn);
  }

  const uint32_t savedSysTick = SysTick->CTRL;
  const uint32_t savedScr = SCB->SCR;

  // Otherwise the 1 ms SysTick interrupt wakes standby immediately.
  SysTick->CTRL = savedSysTick & ~SysTick_CTRL_TICKINT_Msk;
  SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;

  SCB->SCR = savedScr | SCB_SCR_SLEEPDEEP_Msk;
  __DSB();
  __WFI();
  __ISB();
  SCB->SCR = savedScr;

  SysTick->CTRL = savedSysTick;
}

void PinchLowPowerClass::deepSleep(uint32_t millis) {
  setAlarmIn(millis);
  deepSleep();
}

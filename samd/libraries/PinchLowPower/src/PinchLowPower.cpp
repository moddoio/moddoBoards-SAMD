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

#include <wiring_private.h>

namespace
{
constexpr uint8_t kEicLowPowerGenerator = 2;

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

PinchLowPowerClass PinchLowPower;

bool PinchLowPowerClass::attachInterruptWakeup(uint32_t pin, uint32_t mode)
{
  return attachInterruptWakeup(pin, clearWakeInterrupt, mode);
}

bool PinchLowPowerClass::attachInterruptWakeup(uint32_t pin,
                                                voidFuncPtr callback,
                                                uint32_t mode)
{
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

void PinchLowPowerClass::configureEicStandbyClock()
{
  PM->APBAMASK.reg |= PM_APBAMASK_EIC;

  // The SAMD11 only implements GCLK0 through GCLK5. GCLK2 already uses the
  // ultra-low-power 32 kHz oscillator for the watchdog, and can also clock
  // the EIC while the CPU is in standby.
  GCLK->GENDIV.reg = GCLK_GENDIV_ID(kEicLowPowerGenerator) |
                     GCLK_GENDIV_DIV(1);
  while (GCLK->STATUS.bit.SYNCBUSY) {
  }

  GCLK->GENCTRL.reg = GCLK_GENCTRL_ID(kEicLowPowerGenerator) |
                      GCLK_GENCTRL_SRC_OSCULP32K |
                      GCLK_GENCTRL_GENEN |
                      GCLK_GENCTRL_RUNSTDBY;
  while (GCLK->STATUS.bit.SYNCBUSY) {
  }

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

bool PinchLowPowerClass::deepSleep()
{
  if (wakeMask_ == 0) {
    return false;
  }

  // Remove stale EIC/SysTick requests before sleeping. A level-triggered
  // wake source can legitimately reassert after this point.
  EIC->INTFLAG.reg = wakeMask_;
  NVIC_ClearPendingIRQ(EIC_IRQn);

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
  return true;
}

/*
  Copyright (c) 2026 moddo inc. All rights reserved.

  Flash pinch's RGB LED for three seconds, then enter deep standby.
  Touch the selected wake pin to GND to wake it and repeat.

  Test result: A P1150D current-measurement supply provided 3.3 V to pinch's
  3V3 pin and measured approximately 80 uA during deep standby. Enabling or
  disabling USB does not affect deep-standby power consumption.
  Testing details: https://wiki.moddo.io/pinch-dev-board/LowPower

  Change WAKE_PIN to test one of the header-accessible EIC-owner pins:
    D4  (PA17 / EXTINT1)  D6  (PA31 / EXTINT3)
    D7  (PA07 / EXTINT7)  D8  (PA06 / EXTINT6)
    D9  (PA05 / EXTINT5)  D10 (PA04 / EXTINT4)
    D11 (PA02 / EXTINT2)

  D6 is also SWDIO, so do not use it while an SWD debugger is connected.
  This standby implementation works with either Tools > USB Config option.
*/

#include <PinchLowPower.h>

constexpr uint8_t WAKE_PIN = 4; // D4 is the default test pin.

void setRgb(bool red, bool green, bool blue)
{
  // pinch's RGB LED is active-low.
  digitalWrite(PIN_LED_RED, red ? LOW : HIGH);
  digitalWrite(PIN_LED_GREEN, green ? LOW : HIGH);
  digitalWrite(PIN_LED_BLUE, blue ? LOW : HIGH);
}

void flashRgbForThreeSeconds()
{
  const uint32_t started = millis();
  while (millis() - started < 3000) {
    setRgb(true, false, false);  delay(150);
    setRgb(false, true, false);  delay(150);
    setRgb(false, false, true);  delay(150);
  }
  setRgb(false, false, false);
}

void setup()
{
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_BLUE, OUTPUT);
  setRgb(false, false, false);

  pinMode(WAKE_PIN, INPUT_PULLUP);
  PinchLowPower.attachInterruptWakeup(WAKE_PIN, FALLING);
}

void loop()
{
  flashRgbForThreeSeconds();
  PinchLowPower.deepSleep();
}

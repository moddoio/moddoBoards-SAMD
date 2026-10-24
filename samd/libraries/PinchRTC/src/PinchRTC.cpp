/*
  RTC library for Arduino Zero.
  Copyright (c) 2015 Arduino LLC. All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include <time.h>

#include "PinchRTC.h"

#define EPOCH_TIME_OFF      946684800  // This is 1st January 2000, 00:00:00 in epoch time
#define EPOCH_TIME_YEAR_OFF 100        // years since 1900

// Default date & time after reset
#define DEFAULT_YEAR    2000    // 2000..2063
#define DEFAULT_MONTH   1       // 1..12
#define DEFAULT_DAY     1       // 1..31
#define DEFAULT_HOUR    0       // 1..23
#define DEFAULT_MINUTE  0       // 0..59
#define DEFAULT_SECOND  0       // 0..59

voidFuncPtr RTC_callBack = NULL;
voidFuncPtr RTC_periodicCallBack = NULL;

PinchRTC::PinchRTC()
{
  _configured = false;
}

void PinchRTC::begin(bool resetTime)
{
  uint16_t tmp_reg = 0;

  PM->APBAMASK.reg |= PM_APBAMASK_RTC; // turn on digital interface clock
  config32kOSC();

  // If the RTC is in clock mode and the reset was
  // not due to POR or BOD, preserve the clock time
  // POR causes a reset anyway, BOD behaviour is?
  bool validTime = false;
  RTC_MODE2_CLOCK_Type oldTime;

  if ((!resetTime) && (PM->RCAUSE.reg & (PM_RCAUSE_SYST | PM_RCAUSE_WDT | PM_RCAUSE_EXT))) {
    if (RTC->MODE2.CTRL.reg & RTC_MODE2_CTRL_MODE_CLOCK) {
      validTime = true;
      oldTime.reg = RTC->MODE2.CLOCK.reg;
    }
  }

  // Setup clock GCLK2 with OSC32K divided by 32
  configureLowPowerClock();

  RTCdisable();

  RTCreset();

  tmp_reg |= RTC_MODE2_CTRL_MODE_CLOCK; // set clock operating mode
  tmp_reg |= RTC_MODE2_CTRL_PRESCALER_DIV1024; // set prescaler to 1024 for MODE2
  tmp_reg &= ~RTC_MODE2_CTRL_MATCHCLR; // disable clear on match

  //According to the datasheet RTC_MODE2_CTRL_CLKREP = 0 for 24h
  tmp_reg &= ~RTC_MODE2_CTRL_CLKREP; // 24h time representation

  RTC->MODE2.READREQ.reg &= ~RTC_READREQ_RCONT; // disable continuously mode

  RTC->MODE2.CTRL.reg = tmp_reg;
  while (RTCisSyncing())
    ;

  NVIC_EnableIRQ(RTC_IRQn); // enable RTC interrupt
  NVIC_SetPriority(RTC_IRQn, 0x00);

  RTC->MODE2.INTENSET.reg |= RTC_MODE2_INTENSET_ALARM0; // enable alarm interrupt
  RTC->MODE2.Mode2Alarm[0].MASK.bit.SEL = MATCH_OFF; // default alarm match is off (disabled)

  while (RTCisSyncing())
    ;

  RTCenable();
  RTCresetRemove();

  // If desired and valid, restore the time value, else use first valid time value
  if ((!resetTime) && (validTime) && (oldTime.reg != 0L)) {
    RTC->MODE2.CLOCK.reg = oldTime.reg;
  }
  else {
    RTC->MODE2.CLOCK.reg = RTC_MODE2_CLOCK_YEAR(DEFAULT_YEAR - 2000) | RTC_MODE2_CLOCK_MONTH(DEFAULT_MONTH)
        | RTC_MODE2_CLOCK_DAY(DEFAULT_DAY) | RTC_MODE2_CLOCK_HOUR(DEFAULT_HOUR)
        | RTC_MODE2_CLOCK_MINUTE(DEFAULT_MINUTE) | RTC_MODE2_CLOCK_SECOND(DEFAULT_SECOND);
  }
  while (RTCisSyncing())
    ;

  _configured = true;
}

void RTC_Handler(void)
{
  if (RTC_callBack != NULL) {
    RTC_callBack();
  }

  RTC->MODE2.INTFLAG.reg = RTC_MODE2_INTFLAG_ALARM0; // must clear flag at end
}

void PinchRTC::enableAlarm(Alarm_Match match)
{
  if (_configured) {
    RTC->MODE2.Mode2Alarm[0].MASK.bit.SEL = match;
    while (RTCisSyncing())
      ;
  }
}

void PinchRTC::disableAlarm()
{
  if (_configured) {
    RTC->MODE2.Mode2Alarm[0].MASK.bit.SEL = 0x00;
    while (RTCisSyncing())
      ;
  }
}

void PinchRTC::attachInterrupt(voidFuncPtr callback)
{
  RTC_callBack = callback;
}

void PinchRTC::detachInterrupt()
{
  RTC_callBack = NULL;
}

void EVSYS_Handler(void)
{
  if (RTC_periodicCallBack != NULL) {
    RTC_periodicCallBack();
  }

  EVSYS->INTFLAG.reg = EVSYS_INTFLAG_EVD0 | EVSYS_INTFLAG_OVR0; // must clear flag at end
}

void PinchRTC::enablePeriodicInterrupt(Periodic_Interval interval)
{
  if (_configured && interval <= PER_1HZ) {
    PM->APBCMASK.reg |= PM_APBCMASK_EVSYS;

    // The resynchronized path needs a channel clock to raise the EVD interrupt
    GCLK->CLKCTRL.reg = (uint16_t)(GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK2 | GCLK_CLKCTRL_ID(EVSYS_GCLK_ID_0));
    while (GCLK->STATUS.bit.SYNCBUSY)
      ;

    // EVCTRL is enable-protected
    RTCdisable();
    RTC->MODE2.EVCTRL.reg = RTC_MODE2_EVCTRL_PEREO(1 << interval);
    RTCenable();

    EVSYS->CHANNEL.reg = EVSYS_CHANNEL_CHANNEL(0)
        | EVSYS_CHANNEL_EVGEN(EVSYS_ID_GEN_RTC_PER_0 + interval)
        | EVSYS_CHANNEL_PATH_RESYNCHRONIZED
        | EVSYS_CHANNEL_EDGSEL_RISING_EDGE;

    EVSYS->INTFLAG.reg = EVSYS_INTFLAG_EVD0 | EVSYS_INTFLAG_OVR0;
    EVSYS->INTENSET.reg = EVSYS_INTENSET_EVD0;

    NVIC_SetPriority(EVSYS_IRQn, 0x00);
    NVIC_EnableIRQ(EVSYS_IRQn);
  }
}

void PinchRTC::disablePeriodicInterrupt()
{
  if (_configured) {
    EVSYS->INTENCLR.reg = EVSYS_INTENCLR_EVD0;
    NVIC_DisableIRQ(EVSYS_IRQn);

    RTCdisable();
    RTC->MODE2.EVCTRL.reg &= ~RTC_MODE2_EVCTRL_PEREO_Msk;
    RTCenable();

    // Writing a channel with EVGEN = 0 disconnects it
    EVSYS->CHANNEL.reg = EVSYS_CHANNEL_CHANNEL(0);
    EVSYS->INTFLAG.reg = EVSYS_INTFLAG_EVD0 | EVSYS_INTFLAG_OVR0;
    NVIC_ClearPendingIRQ(EVSYS_IRQn);
  }
}

void PinchRTC::attachPeriodicInterrupt(voidFuncPtr callback)
{
  RTC_periodicCallBack = callback;
}

void PinchRTC::detachPeriodicInterrupt()
{
  RTC_periodicCallBack = NULL;
}

void PinchRTC::standbyMode()
{
  // Entering standby mode when connected
  // via the native USB port causes issues.
  SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
  __DSB();
  __WFI();
}

/*
 * Get Functions
 */

uint8_t PinchRTC::getSeconds()
{
  RTCreadRequest();
  return RTC->MODE2.CLOCK.bit.SECOND;
}

uint8_t PinchRTC::getMinutes()
{
  RTCreadRequest();
  return RTC->MODE2.CLOCK.bit.MINUTE;
}

uint8_t PinchRTC::getHours()
{
  RTCreadRequest();
  return RTC->MODE2.CLOCK.bit.HOUR;
}

uint8_t PinchRTC::getDay()
{
  RTCreadRequest();
  return RTC->MODE2.CLOCK.bit.DAY;
}

uint8_t PinchRTC::getMonth()
{
  RTCreadRequest();
  return RTC->MODE2.CLOCK.bit.MONTH;
}

uint8_t PinchRTC::getYear()
{
  RTCreadRequest();
  return RTC->MODE2.CLOCK.bit.YEAR;
}

uint8_t PinchRTC::getAlarmSeconds()
{
  return RTC->MODE2.Mode2Alarm[0].ALARM.bit.SECOND;
}

uint8_t PinchRTC::getAlarmMinutes()
{
  return RTC->MODE2.Mode2Alarm[0].ALARM.bit.MINUTE;
}

uint8_t PinchRTC::getAlarmHours()
{
  return RTC->MODE2.Mode2Alarm[0].ALARM.bit.HOUR;
}

uint8_t PinchRTC::getAlarmDay()
{
  return RTC->MODE2.Mode2Alarm[0].ALARM.bit.DAY;
}

uint8_t PinchRTC::getAlarmMonth()
{
  return RTC->MODE2.Mode2Alarm[0].ALARM.bit.MONTH;
}

uint8_t PinchRTC::getAlarmYear()
{
  return RTC->MODE2.Mode2Alarm[0].ALARM.bit.YEAR;
}

/*
 * Set Functions
 */

void PinchRTC::setSeconds(uint8_t seconds)
{
  if (_configured) {
    RTC->MODE2.CLOCK.bit.SECOND = seconds;
    while (RTCisSyncing())
      ;
  }
}

void PinchRTC::setMinutes(uint8_t minutes)
{
  if (_configured) {
    RTC->MODE2.CLOCK.bit.MINUTE = minutes;
    while (RTCisSyncing())
      ;
  }
}

void PinchRTC::setHours(uint8_t hours)
{
  if (_configured) {
    RTC->MODE2.CLOCK.bit.HOUR = hours;
    while (RTCisSyncing())
      ;
  }
}

void PinchRTC::setTime(uint8_t hours, uint8_t minutes, uint8_t seconds)
{
  if (_configured) {
    setSeconds(seconds);
    setMinutes(minutes);
    setHours(hours);
  }
}

void PinchRTC::setDay(uint8_t day)
{
  if (_configured) {
    RTC->MODE2.CLOCK.bit.DAY = day;
    while (RTCisSyncing())
      ;
  }
}

void PinchRTC::setMonth(uint8_t month)
{
  if (_configured) {
    RTC->MODE2.CLOCK.bit.MONTH = month;
    while (RTCisSyncing())
      ;
  }
}

void PinchRTC::setYear(uint8_t year)
{
  if (_configured) {
    RTC->MODE2.CLOCK.bit.YEAR = year;
    while (RTCisSyncing())
      ;
  }
}

void PinchRTC::setDate(uint8_t day, uint8_t month, uint8_t year)
{
  if (_configured) {
    setDay(day);
    setMonth(month);
    setYear(year);
  }
}

void PinchRTC::setAlarmSeconds(uint8_t seconds)
{
  if (_configured) {
    RTC->MODE2.Mode2Alarm[0].ALARM.bit.SECOND = seconds;
    while (RTCisSyncing())
      ;
  }
}

void PinchRTC::setAlarmMinutes(uint8_t minutes)
{
  if (_configured) {
    RTC->MODE2.Mode2Alarm[0].ALARM.bit.MINUTE = minutes;
    while (RTCisSyncing())
      ;
  }
}

void PinchRTC::setAlarmHours(uint8_t hours)
{
  if (_configured) {
    RTC->MODE2.Mode2Alarm[0].ALARM.bit.HOUR = hours;
    while (RTCisSyncing())
      ;
  }
}

void PinchRTC::setAlarmTime(uint8_t hours, uint8_t minutes, uint8_t seconds)
{
  if (_configured) {
    setAlarmSeconds(seconds);
    setAlarmMinutes(minutes);
    setAlarmHours(hours);
  }
}

void PinchRTC::setAlarmDay(uint8_t day)
{
  if (_configured) {
    RTC->MODE2.Mode2Alarm[0].ALARM.bit.DAY = day;
    while (RTCisSyncing())
      ;
  }
}

void PinchRTC::setAlarmMonth(uint8_t month)
{
  if (_configured) {
    RTC->MODE2.Mode2Alarm[0].ALARM.bit.MONTH = month;
    while (RTCisSyncing())
      ;
  }
}

void PinchRTC::setAlarmYear(uint8_t year)
{
  if (_configured) {
    RTC->MODE2.Mode2Alarm[0].ALARM.bit.YEAR = year;
    while (RTCisSyncing())
      ;
  }
}

void PinchRTC::setAlarmDate(uint8_t day, uint8_t month, uint8_t year)
{
  if (_configured) {
    setAlarmDay(day);
    setAlarmMonth(month);
    setAlarmYear(year);
  }
}

void PinchRTC::getTm(struct tm *tm)
{
  RTCreadRequest();
  RTC_MODE2_CLOCK_Type clockTime;
  clockTime.reg = RTC->MODE2.CLOCK.reg;

  tm->tm_isdst = -1;
  tm->tm_yday = 0;
  tm->tm_wday = 0;
  tm->tm_year = clockTime.bit.YEAR + EPOCH_TIME_YEAR_OFF;
  tm->tm_mon = clockTime.bit.MONTH - 1;
  tm->tm_mday = clockTime.bit.DAY;
  tm->tm_hour = clockTime.bit.HOUR;
  tm->tm_min = clockTime.bit.MINUTE;
  tm->tm_sec = clockTime.bit.SECOND;
}

bool PinchRTC::isLeapYear(int year) {
    // struct tm fields: tm_year is years since 1900
    int actual_year = year + 1900;
    return (actual_year % 4 == 0 && actual_year % 100 != 0) || (actual_year % 400 == 0);
}

int PinchRTC::getDaysInMonth(int month, int year) {
    static const int days_per_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if (month == 1 && isLeapYear(year)) { // February is index 1
        return 29;
    }
    return days_per_month[month];
}

// Manually normalizes struct tm after fields change, without using mktime()
void PinchRTC::normalizeTm(struct tm *t)
{
  // 1. Roll up Seconds to Minutes
  if (t->tm_sec >= 60 || t->tm_sec < 0)
  {
    t->tm_min += t->tm_sec / 60;
    t->tm_sec %= 60;
    if (t->tm_sec < 0)
    {
      t->tm_sec += 60;
      t->tm_min--;
    }
  }

  // 2. Roll up Minutes to Hours
  if (t->tm_min >= 60 || t->tm_min < 0)
  {
    t->tm_hour += t->tm_min / 60;
    t->tm_min %= 60;
    if (t->tm_min < 0)
    {
      t->tm_min += 60;
      t->tm_hour--;
    }
  }

  // 3. Roll up Hours to Days
  if (t->tm_hour >= 24 || t->tm_hour < 0)
  {
    t->tm_mday += t->tm_hour / 24;
    t->tm_hour %= 24;
    if (t->tm_hour < 0)
    {
      t->tm_hour += 24;
      t->tm_mday--;
    }
  }

  // 4. Roll up Months to Years (Needed first to establish correct days-in-month context)
  if (t->tm_mon >= 12 || t->tm_mon < 0)
  {
    t->tm_year += t->tm_mon / 12;
    t->tm_mon %= 12;
    if (t->tm_mon < 0)
    {
      t->tm_mon += 12;
      t->tm_year--;
    }
  }

  // 5. Roll up Days to Months / Years
  // Handle moving forward in days
  while (t->tm_mday > getDaysInMonth(t->tm_mon, t->tm_year))
  {
    t->tm_mday -= getDaysInMonth(t->tm_mon, t->tm_year);
    t->tm_mon++;
    if (t->tm_mon >= 12)
    {
      t->tm_mon = 0;
      t->tm_year++;
    }
  }

  // Handle moving backward in days (if negative increments were used)
  while (t->tm_mday <= 0)
  {
    t->tm_mon--;
    if (t->tm_mon < 0)
    {
      t->tm_mon = 11;
      t->tm_year--;
    }
    t->tm_mday += getDaysInMonth(t->tm_mon, t->tm_year);
  }
}

void PinchRTC::setAlarmTm(struct tm *tm)
{
  if (_configured) {
    normalizeTm(tm);

    setAlarmDate(tm->tm_mday, tm->tm_mon + 1, tm->tm_year - EPOCH_TIME_YEAR_OFF);
    setAlarmTime(tm->tm_hour, tm->tm_min, tm->tm_sec);
  }
}

void PinchRTC::setTm(struct tm *tm)
{
  if (_configured) {
    normalizeTm(tm);

    RTC_MODE2_CLOCK_Type clockTime;

    clockTime.bit.YEAR = tm->tm_year - EPOCH_TIME_YEAR_OFF;
    clockTime.bit.MONTH = tm->tm_mon + 1;
    clockTime.bit.DAY = tm->tm_mday;
    clockTime.bit.HOUR = tm->tm_hour;
    clockTime.bit.MINUTE = tm->tm_min;
    clockTime.bit.SECOND = tm->tm_sec;

    RTC->MODE2.CLOCK.reg = clockTime.reg;

    while (RTCisSyncing())
      ;
  }
}

/* Attach peripheral clock to 32k oscillator */
void PinchRTC::configureLowPowerClock() {
  GCLK->GENDIV.reg = GCLK_GENDIV_ID(2)|GCLK_GENDIV_DIV(4);
  while (GCLK->STATUS.reg & GCLK_STATUS_SYNCBUSY)
    ;
#ifdef CRYSTALLESS
  GCLK->GENCTRL.reg = (GCLK_GENCTRL_GENEN | GCLK_GENCTRL_SRC_OSCULP32K | GCLK_GENCTRL_ID(2) | GCLK_GENCTRL_DIVSEL | GCLK_GENCTRL_RUNSTDBY );
#else
  GCLK->GENCTRL.reg = (GCLK_GENCTRL_GENEN | GCLK_GENCTRL_SRC_XOSC32K | GCLK_GENCTRL_ID(2) | GCLK_GENCTRL_DIVSEL | GCLK_GENCTRL_RUNSTDBY );
#endif
  while (GCLK->STATUS.reg & GCLK_STATUS_SYNCBUSY)
    ;
  GCLK->CLKCTRL.reg = (uint32_t)((GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK2 | (RTC_GCLK_ID << GCLK_CLKCTRL_ID_Pos)));
  while (GCLK->STATUS.bit.SYNCBUSY)
    ;
}

/*
 * Private Utility Functions
 */

/* Configure the 32768Hz Oscillator */
void PinchRTC::config32kOSC()
{
#ifndef CRYSTALLESS
  SYSCTRL->XOSC32K.reg = SYSCTRL_XOSC32K_ONDEMAND |
                         SYSCTRL_XOSC32K_RUNSTDBY |
                         SYSCTRL_XOSC32K_EN32K |
                         SYSCTRL_XOSC32K_XTALEN |
                         SYSCTRL_XOSC32K_STARTUP(6) |
                         SYSCTRL_XOSC32K_ENABLE;
#endif
}

/* Synchronise the CLOCK register for reading*/
inline void PinchRTC::RTCreadRequest() {
  if (_configured) {
    RTC->MODE2.READREQ.reg = RTC_READREQ_RREQ;
    while (RTCisSyncing())
      ;
  }
}

/* Wait for sync in write operations */
inline bool PinchRTC::RTCisSyncing()
{
  return (RTC->MODE2.STATUS.bit.SYNCBUSY);
}

void PinchRTC::RTCdisable()
{
  RTC->MODE2.CTRL.reg &= ~RTC_MODE2_CTRL_ENABLE; // disable RTC
  while (RTCisSyncing())
    ;
}

void PinchRTC::RTCenable()
{
  RTC->MODE2.CTRL.reg |= RTC_MODE2_CTRL_ENABLE; // enable RTC
  while (RTCisSyncing())
    ;
}

void PinchRTC::RTCreset()
{
  RTC->MODE2.CTRL.reg |= RTC_MODE2_CTRL_SWRST; // software reset
  while (RTCisSyncing())
    ;
}

void PinchRTC::RTCresetRemove()
{
  RTC->MODE2.CTRL.reg &= ~RTC_MODE2_CTRL_SWRST; // software reset remove
  while (RTCisSyncing())
    ;
}

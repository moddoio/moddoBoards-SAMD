/*
  Copyright (c) 2014-2015 Arduino LLC.  All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

/*
  Copyright (c) 2026 moddo inc.  All right reserved.

  Modified by moddo for the Arduino board package.
*/

#ifndef _VARIANT_MODDO_PINCH_
#define _VARIANT_MODDO_PINCH_


/*----------------------------------------------------------------------------
 *        Clock Configuration
 *----------------------------------------------------------------------------*/

/** Master clock frequency (also Fcpu frequency) */
#define VARIANT_MCK             (48000000ul)

/* If CLOCKCONFIG_HS_CRYSTAL is defined, then HS_CRYSTAL_FREQUENCY_HERTZ
 * must also be defined with the external crystal frequency in Hertz.
 */
#define HS_CRYSTAL_FREQUENCY_HERTZ      16000000UL

/* If the PLL is used (CLOCKCONFIG_32768HZ_CRYSTAL, or CLOCKCONFIG_HS_CRYSTAL
 * defined), then PLL_FRACTIONAL_ENABLED can be defined, which will result in
 * a more accurate 48MHz output frequency at the expense of increased jitter.
 */
//#define PLL_FRACTIONAL_ENABLED

/* If both PLL_FAST_STARTUP and CLOCKCONFIG_HS_CRYSTAL are defined, the crystal
 * will be divided down to 1MHz - 2MHz, rather than 32KHz - 64KHz, before being
 * multiplied by the PLL. This will result in a faster lock time for the PLL,
 * however, it will also result in a less accurate PLL output frequency if the
 * crystal is not divisible (without remainder) by 1MHz. In this case, define
 * PLL_FRACTIONAL_ENABLED as well.
 */
//#define PLL_FAST_STARTUP

/* The fine calibration value for DFLL open-loop mode is defined here.
 * The coarse calibration value is loaded from NVM OTP (factory calibration values).
 */
#define NVM_SW_CALIB_DFLL48M_FINE_VAL     (512)

/*----------------------------------------------------------------------------
 *        Headers
 *----------------------------------------------------------------------------*/

#include "WVariant.h"
#include "sam.h"
#include "../../config.h"

#ifdef __cplusplus
#include "SERCOM.h"
#include "Uart.h"
#endif // __cplusplus

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

/*----------------------------------------------------------------------------
 *        Pins
 *----------------------------------------------------------------------------*/

// Number of pins defined in PinDescription array
// Compact map: entries indexed by Arduino D number (D0..D14 = index 0..14),
// followed by the two USB pins. digitalWrite(N) writes pin Dn on the J2 header.
//#define PIN_MAP_STANDARD
#define PIN_MAP_COMPACT

#if defined PIN_MAP_STANDARD
  #define NUM_PIN_DESCRIPTION_ENTRIES     (32u)
#elif defined PIN_MAP_COMPACT
  #define NUM_PIN_DESCRIPTION_ENTRIES     (17u)
#else
  #error "variant.h: You must set PIN_MAP_STANDARD or PIN_MAP_COMPACT"
#endif

#define PINS_COUNT           NUM_PIN_DESCRIPTION_ENTRIES
#define NUM_DIGITAL_PINS     PINS_COUNT
#define NUM_ANALOG_INPUTS    (5u)
#define NUM_ANALOG_OUTPUTS   (1u)
#define analogInputToDigitalPin(p)  (p)

#define digitalPinToPort(P)        ( &(PORT->Group[GetPort(P)]) )
#define digitalPinToBitMask(P)     ( 1 << GetPin(P) )

#if defined(PIN_DESCRIPTION_TABLE_SIMPLE)
  #define digitalPinHasPWM(P)        ( g_APinDescription[P].ulTCChannel != NOT_ON_TIMER )
#else
  #define digitalPinHasPWM(P)        ( (g_APinDescription[P].ulPinAttribute & PIN_ATTR_TIMER_PWM) == PIN_ATTR_TIMER_PWM )
#endif

//#define analogInPinToBit(P)        ( )
#define portOutputRegister(port)   ( &(port->OUT.reg) )
#define portInputRegister(port)    ( &(port->IN.reg) )
#define portModeRegister(port)     ( &(port->DIR.reg) )

/* LEDs
 * pinch RGB LED, all active low (GPIO LOW = LED ON)
 *
 * J2 connector pinout:
 *   PA15 = D0 / SCL                PA14 = D1 / SDA
 *   PA09 = D2                      PA08 = D3
 *   PA17 = D4                      PA30 = D5 / TX1 / SWCLK
 *   PA31 = D6 / RX1 / SWDIO        PA07 = D7 / A0 / MOSI
 *   PA06 = D8 / A1 / MISO          PA05 = D9 / A2 / SCK
 *   PA04 = D10 / A3 / SS           PA02 = D11 / A4 / DAC0
 *
 * Hardware peripheral routing (concurrent: UART1 + I2C + SPI):
 *   I2C   on SERCOM2-ALT: SDA=PA14, SCL=PA15
 *   UART1 on SERCOM1-ALT: TX1=PA30 (pad2), RX1=PA31 (pad3)
 *   SPI   on SERCOM0-ALT: SS=PA04, SCK=PA05, MISO=PA06, MOSI=PA07
 *
 * Off-connector:
 *   PA16 = LED_R (D12, active low, LED_BUILTIN)
 *   PA11 = LED_B (D13, active low)
 *   PA10 = LED_G (D14, active low)
 * USB (internal):
 *   PA24 = USB_DM
 *   PA25 = USB_DP
 */
#if defined PIN_MAP_STANDARD
#define PIN_LED_RED          (16u)   // PA16, active low, LED_BUILTIN
#define PIN_LED_BLUE         (11u)   // PA11, active low
#define PIN_LED_GREEN        (10u)   // PA10, active low
#elif defined PIN_MAP_COMPACT
#define PIN_LED_RED          (12u)   // D12 = PA16, active low, LED_BUILTIN
#define PIN_LED_BLUE         (13u)   // D13 = PA11, active low
#define PIN_LED_GREEN        (14u)   // D14 = PA10, active low
#endif

#define PIN_LED              PIN_LED_RED
#define LED_BUILTIN          PIN_LED_RED


/*
 * Analog pins
 *
 * PA02..PA07 have a usable ADC sample/hold front-end and are exposed as
 * A0..A4 on D7..D11:
 *   A0 = D7  (PA07)   A1 = D8  (PA06)   A2 = D9  (PA05)
 *   A3 = D10 (PA04)   A4 = D11 (PA02, also DAC0)
 *
 * D0/D1 (PA15/PA14) have no usable ADC on the SAMD11D14A, so they are not
 * analog-capable. Use them as digital I/O or I2C (SDA/SCL).
 */
#if defined PIN_MAP_STANDARD
#define PIN_A0               (7ul)    // PA07 (AIN5)
#define PIN_A1               (6ul)    // PA06 (AIN4)
#define PIN_A2               (5ul)    // PA05 (AIN3)
#define PIN_A3               (4ul)    // PA04 (AIN2)
#define PIN_A4               (2ul)    // PA02 (AIN0)
#define PIN_DAC0             (2ul)    // PA02
#elif defined PIN_MAP_COMPACT
#define PIN_A0               (7ul)    // D7  = PA07 (AIN5)
#define PIN_A1               (8ul)    // D8  = PA06 (AIN4)
#define PIN_A2               (9ul)    // D9  = PA05 (AIN3)
#define PIN_A3               (10ul)   // D10 = PA04 (AIN2)
#define PIN_A4               (11ul)   // D11 = PA02 (AIN0)
#define PIN_DAC0             (11ul)   // D11 = PA02
#endif

static const uint8_t A0   = PIN_A0;
static const uint8_t A1   = PIN_A1;
static const uint8_t A2   = PIN_A2;
static const uint8_t A3   = PIN_A3;
static const uint8_t A4   = PIN_A4;
static const uint8_t DAC0 = PIN_DAC0;

#define ADC_RESOLUTION		12

#define REMAP_ANALOG_PIN_ID	while(0)

/* Set default analog voltage reference */
#define VARIANT_AR_DEFAULT	AR_DEFAULT

/* Reference voltage pins (silicon)
 *   REFA = PA03, off the J2 connector on this board and not in the pin table,
 *                so AR_EXTERNAL_REFA is intentionally left undefined.
 *   REFB = PA04, shared with D10/SS on J2; AR_EXTERNAL_REFB conflicts with
 *                hardware SPI (SS) on the same pin.
 */
#if defined PIN_MAP_STANDARD
#define REFA_PIN    (3ul)    // PA03
#define REFB_PIN    (4ul)    // PA04
#elif defined PIN_MAP_COMPACT
#define REFB_PIN    (10ul)   // D10 = PA04
#endif


/*
 * Serial interfaces
 * Serial1: SERCOM1-ALT, TX=PA30(pad2), RX=PA31(pad3)
 */
#if defined PIN_MAP_STANDARD
#define PIN_SERIAL1_TX       (30ul)  // PA30, SERCOM1-ALT pad2
#define PIN_SERIAL1_RX       (31ul)  // PA31, SERCOM1-ALT pad3
#elif defined PIN_MAP_COMPACT
#define PIN_SERIAL1_TX       (5ul)   // D5 = PA30, SERCOM1-ALT pad2
#define PIN_SERIAL1_RX       (6ul)   // D6 = PA31, SERCOM1-ALT pad3
#endif

#define PAD_SERIAL1_TX       (UART_TX_PAD_2)
#define PAD_SERIAL1_RX       (SERCOM_RX_PAD_3)
#define SERCOM_INSTANCE_SERIAL1       &sercom1


/*
 * SPI Interface
 * SERCOM0-ALT mux: PA04(pad0), PA05(pad1), PA06(pad2), PA07(pad3)
 * (SERCOM0 main mux has the pad pairs swapped: PA04=pad2, PA05=pad3,
 *  PA06=pad0, PA07=pad1, so the ALT mux is used to match the J2 layout.)
 * Config:
 *   SS   = PA04 (D10/SS,  SERCOM0-ALT pad0)
 *   SCK  = PA05 (D9 /SCK, SERCOM0-ALT pad1)
 *   MISO = PA06 (D8 /MISO,SERCOM0-ALT pad2)
 *   MOSI = PA07 (D7 /MOSI,SERCOM0-ALT pad3)
 */
#if defined(ONE_SPI)
#define SPI_INTERFACES_COUNT 1
#else
#define SPI_INTERFACES_COUNT 0
#endif

#if defined PIN_MAP_STANDARD
#define PIN_SPI_MOSI         (7u)    // PA07, SERCOM0-ALT pad3
#define PIN_SPI_SCK          (5u)    // PA05, SERCOM0-ALT pad1
#define PIN_SPI_MISO         (6u)    // PA06, SERCOM0-ALT pad2
#define PIN_SPI_SS           (4u)    // PA04, SERCOM0-ALT pad0 (manual CS)
#elif defined PIN_MAP_COMPACT
#define PIN_SPI_MOSI         (7u)    // D7  = PA07, SERCOM0-ALT pad3
#define PIN_SPI_SCK          (9u)    // D9  = PA05, SERCOM0-ALT pad1
#define PIN_SPI_MISO         (8u)    // D8  = PA06, SERCOM0-ALT pad2
#define PIN_SPI_SS           (10u)   // D10 = PA04, SERCOM0-ALT pad0 (manual CS)
#endif

#define PERIPH_SPI           sercom0
#define PAD_SPI_TX           SPI_PAD_3_SCK_1
#define PAD_SPI_RX           SERCOM_RX_PAD_2

static const uint8_t SS   = PIN_SPI_SS;
static const uint8_t MOSI = PIN_SPI_MOSI;
static const uint8_t MISO = PIN_SPI_MISO;
static const uint8_t SCK  = PIN_SPI_SCK;


/*
 * Wire Interfaces
 * SERCOM2 ALT mux: PA14(pad0)=SDA, PA15(pad1)=SCL
 */
#if defined(ONE_WIRE)
#define WIRE_INTERFACES_COUNT 1
#else
#define WIRE_INTERFACES_COUNT 0
#endif

#if defined PIN_MAP_STANDARD
#define PIN_WIRE_SDA         (14u)   // PA14, SERCOM2-ALT pad0
#define PIN_WIRE_SCL         (15u)   // PA15, SERCOM2-ALT pad1
#elif defined PIN_MAP_COMPACT
#define PIN_WIRE_SDA         (1u)    // D1 = PA14, SERCOM2-ALT pad0
#define PIN_WIRE_SCL         (0u)    // D0 = PA15, SERCOM2-ALT pad1
#endif

#define PERIPH_WIRE          sercom2
#define WIRE_IT_HANDLER      SERCOM2_Handler

static const uint8_t SDA = PIN_WIRE_SDA;
static const uint8_t SCL = PIN_WIRE_SCL;


/*
 * USB
 */
#if defined PIN_MAP_STANDARD
#define PIN_USB_DM                      (24ul)  // PA24
#define PIN_USB_DP                      (25ul)  // PA25
#elif defined PIN_MAP_COMPACT
#define PIN_USB_DM                      (15ul)  // PA24
#define PIN_USB_DP                      (16ul)  // PA25
#endif

#define PIN_USB_HOST_ENABLE_VALUE	HIGH

#ifdef __cplusplus
}
#endif


/*----------------------------------------------------------------------------
 *        Arduino objects (C++ only)
 *----------------------------------------------------------------------------*/

#ifdef __cplusplus

/*	=========================
 *	===== SERCOM DEFINITION
 *	=========================
*/
extern SERCOM sercom0;
extern SERCOM sercom1;
extern SERCOM sercom2;

extern Uart Serial1;

#endif

#define SERIAL_PORT_USBVIRTUAL      SerialUSB
#define SERIAL_PORT_MONITOR         Serial1
#define SERIAL_PORT_HARDWARE        Serial1
#define SERIAL_PORT_HARDWARE_OPEN   Serial1

// When USB CDC is enabled (native stack or the bootloader-provided driver),
// Serial refers to SerialUSB, otherwise it refers to Serial1.
#if defined(CDC_ONLY) || defined(CDC_HID) || defined(WITH_CDC) || defined(CDC_BOOTLOADER)
#define Serial                      SerialUSB
#else
#define Serial                      Serial1
#endif

#endif /* _VARIANT_MODDO_PINCH_ */

// Copyright 2009 Olivier Gillet.
//
// Author: Olivier Gillet (ol.gillet@gmail.com)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// -----------------------------------------------------------------------------
//
// An alternative gpio library based on templates.
//
// Examples of use:
//
// NumberedGpio<3>::set_mode(DIGITAL_INPUT)
// NumberedGpio<4>::set_mode(DIGITAL_OUTPUT)
// NumberedGpio<3>::value()
// NumberedGpio<4>::High()
// NumberedGpio<4>::Low()
// NumberedGpio<4>::set_value(1)
// NumberedGpio<4>::set_value(0)

#ifndef AVRLIB_GPIO_H_
#define AVRLIB_GPIO_H_

#include <avr/io.h>

#include "avrlib/avrlib.h"
#include "avrlib/timer.h"

namespace avrlib {

enum PinMode {
  DIGITAL_INPUT = 0,
  DIGITAL_OUTPUT = 1,
  PWM_OUTPUT = 2
};

// All the registers used in the following definitions are wrapped here.
IORegister(DDRB);
IORegister(DDRC);
IORegister(DDRD);

IORegister(PORTB);
IORegister(PORTC);
IORegister(PORTD);

IORegister(PINB);
IORegister(PINC);
IORegister(PIND);

// Represents a i/o port, which has input, output and mode registers.
template<typename InputRegister, typename OutputRegister,
         typename ModeRegister>
struct Port {
  typedef InputRegister Input;
  typedef OutputRegister Output;
  typedef ModeRegister Mode;
};

// Definition of I/O ports.
typedef Port<PINBRegister, PORTBRegister, DDRBRegister> PortB;
typedef Port<PINCRegister, PORTCRegister, DDRCRegister> PortC;
typedef Port<PINDRegister, PORTDRegister, DDRDRegister> PortD;

#if defined(ATMEGA164P) || defined(ATMEGA324P) || defined(ATMEGA644P) || defined(ATMEGA1284P)

IORegister(DDRA);
IORegister(PORTA);
IORegister(PINA);
typedef Port<PINARegister, PORTARegister, DDRARegister> PortA;

#endif



// The actual implementation of a pin, not very convenient to use because it
// requires the actual parameters of the pin to be passed as template
// arguments.
template<typename Port, typename PwmChannel, uint8_t bit>
struct GpioImpl {
  typedef BitInRegister<typename Port::Mode, bit> ModeBit;
  typedef BitInRegister<typename Port::Output, bit> OutputBit;
  typedef BitInRegister<typename Port::Input, bit> InputBit;
  typedef PwmChannel Pwm;

  static inline void set_mode(uint8_t mode) {
    if (mode == DIGITAL_INPUT) {
      ModeBit::clear();
    } else if (mode == DIGITAL_OUTPUT || mode == PWM_OUTPUT) {
      ModeBit::set();
    }
    if (mode == PWM_OUTPUT) {
      PwmChannel::Start();
    } else {
      PwmChannel::Stop();
    }
  }

  static inline void High() {
    OutputBit::set();
  }
  static inline void Low() {
    OutputBit::clear();
  }
  static inline void Toggle() {
    OutputBit::toggle();
  }
  static inline void set_value(uint8_t value) {
    if (value == 0) {
      Low();
    } else {
      High();
    }
  }
  
  static inline void set_pwm_value(uint8_t value) {
    if (PwmChannel::has_pwm) {
      PwmChannel::Write(value);
    } else {
      set_value(value);
    }
  }

  static inline uint8_t value() {
    return InputBit::value();
  }
  static inline uint8_t is_high() {
    return InputBit::value();
  }
  static inline uint8_t is_low() {
    return InputBit::value() == 0;
  }
};


template<typename port, uint8_t bit>
struct Gpio {
  typedef GpioImpl<port, NoPwmChannel, bit> Impl;
  static void High() { Impl::High(); }
  static void Low() { Impl::Low(); }
  static void Toggle() { Impl::Toggle(); }
  static void set_mode(uint8_t mode) { Impl::set_mode(mode); }
  static void set_value(uint8_t value) { Impl::set_value(value); }
  static void set_pwm_value(uint8_t value) { Impl::set_pwm_value(value); }
  static uint8_t value() { return Impl::value(); }
  static uint8_t is_low() { return Impl::is_low(); }
  static uint8_t is_high() { return Impl::is_high(); }
};

struct DummyGpio {
  static void High() { }
  static void Low() { }
  static void set_mode(uint8_t mode) { }
  static void set_value(uint8_t value) { }
  static void set_pwm_value(uint8_t value) { }
  static uint8_t value() { return 0; }
  static uint8_t is_low() { return 0; }
  static uint8_t is_high() { return 0; }
};

template<typename Gpio>
struct Inverter {
  static void High() { Gpio::Low(); }
  static void Low() { Gpio::High(); }
  static void set_mode(uint8_t mode) { Gpio::set_mode(mode); }
  static void set_value(uint8_t value) { Gpio::set_value(!value); }
  static void set_pwm_value(uint8_t value) { Gpio::set_pwm_value(~value); }
  static uint8_t value() { return !Gpio::value(); }
  static uint8_t is_low() { return !Gpio::is_low(); }
  static uint8_t is_high() { return !Gpio::is_high(); }
};

template<typename gpio>
struct DigitalInput {
  enum {
    buffer_size = 0,
    data_size = 1,
  };
  static void Init() {
    gpio::set_mode(DIGITAL_INPUT);
  }
  static void EnablePullUpResistor() {
    gpio::High();
  }
  static void DisablePullUpResistor() {
    gpio::Low();
  }
  static uint8_t Read() {
    return gpio::value();
  }
};

// A template that will be specialized for each pin, allowing the pin number to
// be specified as a template parameter.
template<int n>
struct NumberedGpioInternal { };

// Macro to make the pin definitions (template specializations) easier to read.
#define SetupGpio(n, port, timer, bit) \
template<> struct NumberedGpioInternal<n> { \
  typedef GpioImpl<port, timer, bit> Impl; };

// Pin definitions for ATmega lineup

#if defined(ATMEGA48P) || defined(ATMEGA88P) || defined(ATMEGA168P) || defined(ATMEGA328P)

SetupGpio(0, PortD, NoPwmChannel, 0);
SetupGpio(1, PortD, NoPwmChannel, 1);
SetupGpio(2, PortD, NoPwmChannel, 2);
SetupGpio(3, PortD, PwmChannel2B, 3);
SetupGpio(4, PortD, NoPwmChannel, 4);
SetupGpio(5, PortD, PwmChannel0B, 5);
SetupGpio(6, PortD, PwmChannel0A, 6);
SetupGpio(7, PortD, NoPwmChannel, 7);
SetupGpio(8, PortB, NoPwmChannel, 0);
SetupGpio(9, PortB, PwmChannel1A, 1);
SetupGpio(10, PortB, PwmChannel1B, 2);
SetupGpio(11, PortB, PwmChannel2A, 3);
SetupGpio(12, PortB, NoPwmChannel, 4);
SetupGpio(13, PortB, NoPwmChannel, 5);
SetupGpio(14, PortC, NoPwmChannel, 0);
SetupGpio(15, PortC, NoPwmChannel, 1);
SetupGpio(16, PortC, NoPwmChannel, 2);
SetupGpio(17, PortC, NoPwmChannel, 3);
SetupGpio(18, PortC, NoPwmChannel, 4);
SetupGpio(19, PortC, NoPwmChannel, 5);

SetupGpio(255, PortB, NoPwmChannel, 0);

typedef Gpio<PortB, 5> SpiSCK;
typedef Gpio<PortB, 4> SpiMISO;
typedef Gpio<PortB, 3> SpiMOSI;
typedef Gpio<PortB, 2> SpiSS;

typedef Gpio<PortD, 4> UartSpi0XCK;
typedef Gpio<PortD, 1> UartSpi0TX;
typedef Gpio<PortD, 0> UartSpi0RX;

#define HAS_USART0

#elif defined(ATMEGA164P) || defined(ATMEGA324P) || defined(ATMEGA644P) || defined(ATMEGA1284P)

SetupGpio(0,  PortB, NoPwmChannel, 0);
SetupGpio(1,  PortB, NoPwmChannel, 1);
SetupGpio(2,  PortB, NoPwmChannel, 2);
SetupGpio(3,  PortB, PwmChannel0A, 3);
SetupGpio(4,  PortB, PwmChannel0B, 4);
SetupGpio(5,  PortB, NoPwmChannel, 5);
SetupGpio(6,  PortB, NoPwmChannel, 6);
SetupGpio(7,  PortB, NoPwmChannel, 7);

SetupGpio(8,  PortD, NoPwmChannel, 0);
SetupGpio(9,  PortD, NoPwmChannel, 1);
SetupGpio(10, PortD, NoPwmChannel, 2);
SetupGpio(11, PortD, NoPwmChannel, 3);
SetupGpio(12, PortD, PwmChannel1B, 4);
SetupGpio(13, PortD, PwmChannel1A, 5);
SetupGpio(14, PortD, PwmChannel2B, 6);
SetupGpio(15, PortD, PwmChannel2A, 7);

SetupGpio(16, PortC, NoPwmChannel, 0);
SetupGpio(17, PortC, NoPwmChannel, 1);
SetupGpio(18, PortC, NoPwmChannel, 2);
SetupGpio(19, PortC, NoPwmChannel, 3);
SetupGpio(20, PortC, NoPwmChannel, 4);
SetupGpio(21, PortC, NoPwmChannel, 5);
SetupGpio(22, PortC, NoPwmChannel, 6);
SetupGpio(23, PortC, NoPwmChannel, 7);

SetupGpio(255, PortB, NoPwmChannel, 0);

typedef Gpio<PortB, 7> SpiSCK;
typedef Gpio<PortB, 6> SpiMISO;
typedef Gpio<PortB, 5> SpiMOSI;
typedef Gpio<PortB, 4> SpiSS;

typedef Gpio<PortB, 0> UartSpi0XCK;
typedef Gpio<PortD, 1> UartSpi0TX;
typedef Gpio<PortD, 0> UartSpi0RX;

typedef Gpio<PortD, 4> UartSpi1XCK;
typedef Gpio<PortD, 3> UartSpi1TX;
typedef Gpio<PortD, 2> UartSpi1RX;

#define HAS_USART0
#define HAS_USART1

#ifdef ATMEGA1284P
#define HAS_TIMER3
#endif

#else

#error Unsupported MCU type

#endif

// Two specializations of the numbered pin template, one which clears the timer
// for each access to the PWM pins, as does the original Arduino wire lib,
// the other that does not (use with care!).
template<int n>
struct NumberedGpio {
  typedef typename NumberedGpioInternal<n>::Impl Impl;
  static void High() { Impl::High(); }
  static void Low() { Impl::Low(); }
  static void set_mode(uint8_t mode) { Impl::set_mode(mode); }
  static void set_value(uint8_t value) { Impl::set_value(value); }
  static void set_pwm_value(uint8_t value) { Impl::set_pwm_value(value); }
  static uint8_t value() { return Impl::value(); }
};

template<int n>
struct PwmOutput {
  enum {
    buffer_size = 0,
    data_size = 8,
  };
  static void Init() {
    NumberedGpio<n>::set_mode(PWM_OUTPUT);
  }
  static void Write(uint8_t value) {
    return NumberedGpio<n>::set_pwm_value(value);
  }
  static void Stop() {
    NumberedGpio<n>::Impl::Pwm::Stop();
  }
  static void Start() {
    NumberedGpio<n>::Impl::Pwm::Start();
  }
};

}  // namespace avrlib

#endif   // AVRLIB_GPIO_H_

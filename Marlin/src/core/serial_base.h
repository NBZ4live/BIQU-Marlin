/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2020 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (c) 2011 Camiel Gubbels / Erik van der Zalm
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */
#pragma once

#include "../inc/MarlinConfigPre.h"
#include "macros.h"

#if ENABLED(EMERGENCY_PARSER)
  #include "../feature/e_parser.h"
#endif

// flushTX is not implemented in all HAL, so use SFINAE to call the method where it is.
CALL_IF_EXISTS_IMPL(void, flushTX);
CALL_IF_EXISTS_IMPL(bool, connected, true);

// In order to catch usage errors in code, we make the base to encode number explicit
// If given a number (and not this enum), the compiler will reject the overload, falling back to the (double, digit) version
// We don't want hidden conversion of the first parameter to double, so it has to be as hard to do for the compiler as creating this enum
enum class PrintBase {
  Dec = 10,
  Hex = 16,
  Oct = 8,
  Bin = 2
};

// A simple forward struct that prevent the compiler to select print(double, int) as a default overload for any type different than
// double or float. For double or float, a conversion exists so the call will be transparent
struct EnsureDouble {
  double a;
  FORCE_INLINE operator double() { return a; }
  // If the compiler breaks on ambiguity here, it's likely because you're calling print(X, base) with X not a double or a float, and a
  // base that's not one of PrintBase's value. This exact code is made to detect such error, you NEED to set a base explicitely like this:
  // SERIAL_PRINT(v, PrintBase::Hex)
  FORCE_INLINE EnsureDouble(double a) : a(a) {}
  FORCE_INLINE EnsureDouble(float a) : a(a) {}
};

// Using Curiously Recurring Template Pattern here to avoid virtual table cost when compiling.
// Since the real serial class is known at compile time, this results in the compiler writing
// a completely efficient code.
template <class Child>
struct SerialBase {
  #if ENABLED(EMERGENCY_PARSER)
    const bool ep_enabled;
    EmergencyParser::State emergency_state;
    inline bool emergency_parser_enabled() { return ep_enabled; }
    SerialBase(bool ep_capable) : ep_enabled(ep_capable), emergency_state(EmergencyParser::State::EP_RESET) {}
  #else
    SerialBase(const bool) {}
  #endif

  // Static dispatch methods below:
  // The most important method here is where it all ends to:
  size_t write(uint8_t c)           { return static_cast<Child*>(this)->write(c); }
  // Called when the parser finished processing an instruction, usually build to nothing
  void msgDone()                    { static_cast<Child*>(this)->msgDone(); }
  // Called upon initialization
  void begin(const long baudRate)   { static_cast<Child*>(this)->begin(baudRate); }
  // Called upon destruction
  void end()                        { static_cast<Child*>(this)->end(); }
  /** Check for available data from the port
      @param index  The port index, usually 0 */
  bool available(uint8_t index = 0) { return static_cast<Child*>(this)->available(index); }
  /** Read a value from the port
      @param index  The port index, usually 0 */
  int  read(uint8_t index = 0)      { return static_cast<Child*>(this)->read(index); }
  // Check if the serial port is connected (usually bypassed)
  bool connected()                  { return static_cast<Child*>(this)->connected(); }
  // Redirect flush
  void flush()                      { static_cast<Child*>(this)->flush(); }
  // Not all implementation have a flushTX, so let's call them only if the child has the implementation
  void flushTX()                    { CALL_IF_EXISTS(void, static_cast<Child*>(this), flushTX); }

  // Glue code here
  FORCE_INLINE void write(const char* str)                    { while (*str) write(*str++); }
  FORCE_INLINE void write(const uint8_t* buffer, size_t size) { while (size--) write(*buffer++); }
  FORCE_INLINE void print(const char* str)                    { write(str); }
  // No default argument to avoid ambiguity
  NO_INLINE void print(char c, PrintBase base)                { printNumber((signed long)c, (uint8_t)base); }
  NO_INLINE void print(unsigned char c, PrintBase base)       { printNumber((unsigned long)c, (uint8_t)base); }
  NO_INLINE void print(int c, PrintBase base)                 { printNumber((signed long)c, (uint8_t)base); }
  NO_INLINE void print(unsigned int c, PrintBase base)        { printNumber((unsigned long)c, (uint8_t)base); }
  void print(unsigned long c, PrintBase base)                 { printNumber((unsigned long)c, (uint8_t)base); }
  void print(long c, PrintBase base)                          { printNumber((signed long)c, (uint8_t)base); }
  void print(EnsureDouble c, int digits)                      { printFloat(c, digits); }

  // Forward the call to the former's method
  FORCE_INLINE void print(char c)                { print(c, PrintBase::Dec); }
  FORCE_INLINE void print(unsigned char c)       { print(c, PrintBase::Dec); }
  FORCE_INLINE void print(int c)                 { print(c, PrintBase::Dec); }
  FORCE_INLINE void print(unsigned int c)        { print(c, PrintBase::Dec); }
  FORCE_INLINE void print(unsigned long c)       { print(c, PrintBase::Dec); }
  FORCE_INLINE void print(long c)                { print(c, PrintBase::Dec); }
  FORCE_INLINE void print(double c)              { print(c, 2); }

  FORCE_INLINE void println(const char s[])                  { print(s); println(); }
  FORCE_INLINE void println(char c, PrintBase base)          { print(c, base); println(); }
  FORCE_INLINE void println(unsigned char c, PrintBase base) { print(c, base); println(); }
  FORCE_INLINE void println(int c, PrintBase base)           { print(c, base); println(); }
  FORCE_INLINE void println(unsigned int c, PrintBase base)  { print(c, base); println(); }
  FORCE_INLINE void println(long c, PrintBase base)          { print(c, base); println(); }
  FORCE_INLINE void println(unsigned long c, PrintBase base) { print(c, base); println(); }
  FORCE_INLINE void println(double c, int digits)            { print(c, digits); println(); }
  FORCE_INLINE void println()                                { write('\r'); write('\n'); }

  // Forward the call to the former's method
  FORCE_INLINE void println(char c)                { println(c, PrintBase::Dec); }
  FORCE_INLINE void println(unsigned char c)       { println(c, PrintBase::Dec); }
  FORCE_INLINE void println(int c)                 { println(c, PrintBase::Dec); }
  FORCE_INLINE void println(unsigned int c)        { println(c, PrintBase::Dec); }
  FORCE_INLINE void println(unsigned long c)       { println(c, PrintBase::Dec); }
  FORCE_INLINE void println(long c)                { println(c, PrintBase::Dec); }
  FORCE_INLINE void println(double c)              { println(c, 2); }

  // Print a number with the given base
  NO_INLINE void printNumber(unsigned long n, const uint8_t base) {
    if (!base) return; // Hopefully, this should raise visible bug immediately

    if (n) {
      unsigned char buf[8 * sizeof(long)]; // Enough space for base 2
      int8_t i = 0;
      while (n) {
        buf[i++] = n % base;
        n /= base;
      }
      while (i--) write((char)(buf[i] + (buf[i] < 10 ? '0' : 'A' - 10)));
    }
    else write('0');
  }
  void printNumber(signed long n, const uint8_t base) {
    if (base == 10 && n < 0) {
      n = -n; // This works because all platforms Marlin's builds on are using 2-complement encoding for negative number
              // On such CPU, changing the sign of a number is done by inverting the bits and adding one, so if n = 0x80000000 = -2147483648 then
              // -n = 0x7FFFFFFF + 1 => 0x80000000 = 2147483648 (if interpreted as unsigned) or -2147483648 if interpreted as signed.
              // On non 2-complement CPU, there would be no possible representation for 2147483648.
      write('-');
    }
    printNumber((unsigned long)n , base);
  }

  // Print a decimal number
  NO_INLINE void printFloat(double number, uint8_t digits) {
    // Handle negative numbers
    if (number < 0.0) {
      write('-');
      number = -number;
    }

    // Round correctly so that print(1.999, 2) prints as "2.00"
    double rounding = 0.5;
    LOOP_L_N(i, digits) rounding *= 0.1;
    number += rounding;

    // Extract the integer part of the number and print it
    unsigned long int_part = (unsigned long)number;
    double remainder = number - (double)int_part;
    printNumber(int_part, 10);

    // Print the decimal point, but only if there are digits beyond
    if (digits) {
      write('.');
      // Extract digits from the remainder one at a time
      while (digits--) {
        remainder *= 10.0;
        unsigned long toPrint = (unsigned long)remainder;
        printNumber(toPrint, 10);
        remainder -= toPrint;
      }
    }
  }
};

// All serial instances will be built by chaining the features required
// for the function in the form of a template type definition.

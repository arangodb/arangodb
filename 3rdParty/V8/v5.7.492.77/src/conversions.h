// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CONVERSIONS_H_
#define V8_CONVERSIONS_H_

#include <limits>

#include "src/base/logging.h"
#include "src/handles.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

class UnicodeCache;

// Maximum number of significant digits in decimal representation.
// The longest possible double in decimal representation is
// (2^53 - 1) * 2 ^ -1074 that is (2 ^ 53 - 1) * 5 ^ 1074 / 10 ^ 1074
// (768 digits). If we parse a number whose first digits are equal to a
// mean of 2 adjacent doubles (that could have up to 769 digits) the result
// must be rounded to the bigger one unless the tail consists of zeros, so
// we don't need to preserve all the digits.
const int kMaxSignificantDigits = 772;


inline bool isDigit(int x, int radix) {
  return (x >= '0' && x <= '9' && x < '0' + radix)
      || (radix > 10 && x >= 'a' && x < 'a' + radix - 10)
      || (radix > 10 && x >= 'A' && x < 'A' + radix - 10);
}


inline bool isBinaryDigit(int x) {
  return x == '0' || x == '1';
}


// The fast double-to-(unsigned-)int conversion routine does not guarantee
// rounding towards zero.
// If x is NaN, the result is INT_MIN.  Otherwise the result is the argument x,
// clamped to [INT_MIN, INT_MAX] and then rounded to an integer.
inline int FastD2IChecked(double x) {
  if (!(x >= INT_MIN)) return INT_MIN;  // Negation to catch NaNs.
  if (x > INT_MAX) return INT_MAX;
  return static_cast<int>(x);
}


// The fast double-to-(unsigned-)int conversion routine does not guarantee
// rounding towards zero.
// The result is unspecified if x is infinite or NaN, or if the rounded
// integer value is outside the range of type int.
inline int FastD2I(double x) {
  return static_cast<int32_t>(x);
}

inline unsigned int FastD2UI(double x);


inline double FastI2D(int x) {
  // There is no rounding involved in converting an integer to a
  // double, so this code should compile to a few instructions without
  // any FPU pipeline stalls.
  return static_cast<double>(x);
}


inline double FastUI2D(unsigned x) {
  // There is no rounding involved in converting an unsigned integer to a
  // double, so this code should compile to a few instructions without
  // any FPU pipeline stalls.
  return static_cast<double>(x);
}


// This function should match the exact semantics of ECMA-262 20.2.2.17.
inline float DoubleToFloat32(double x);


// This function should match the exact semantics of ECMA-262 9.4.
inline double DoubleToInteger(double x);


// This function should match the exact semantics of ECMA-262 9.5.
inline int32_t DoubleToInt32(double x);


// This function should match the exact semantics of ECMA-262 9.6.
inline uint32_t DoubleToUint32(double x);


// Enumeration for allowing octals and ignoring junk when converting
// strings to numbers.
enum ConversionFlags {
  NO_FLAGS = 0,
  ALLOW_HEX = 1,
  ALLOW_OCTAL = 2,
  ALLOW_IMPLICIT_OCTAL = 4,
  ALLOW_BINARY = 8,
  ALLOW_TRAILING_JUNK = 16
};


// Converts a string into a double value according to ECMA-262 9.3.1
double StringToDouble(UnicodeCache* unicode_cache,
                      Vector<const uint8_t> str,
                      int flags,
                      double empty_string_val = 0);
double StringToDouble(UnicodeCache* unicode_cache,
                      Vector<const uc16> str,
                      int flags,
                      double empty_string_val = 0);
// This version expects a zero-terminated character array.
double StringToDouble(UnicodeCache* unicode_cache,
                      const char* str,
                      int flags,
                      double empty_string_val = 0);

// Converts a string into an integer.
double StringToInt(UnicodeCache* unicode_cache,
                   Vector<const uint8_t> vector,
                   int radix);


double StringToInt(UnicodeCache* unicode_cache,
                   Vector<const uc16> vector,
                   int radix);

const int kDoubleToCStringMinBufferSize = 100;

// Converts a double to a string value according to ECMA-262 9.8.1.
// The buffer should be large enough for any floating point number.
// 100 characters is enough.
const char* DoubleToCString(double value, Vector<char> buffer);

// Convert an int to a null-terminated string. The returned string is
// located inside the buffer, but not necessarily at the start.
const char* IntToCString(int n, Vector<char> buffer);

// Additional number to string conversions for the number type.
// The caller is responsible for calling free on the returned pointer.
char* DoubleToFixedCString(double value, int f);
char* DoubleToExponentialCString(double value, int f);
char* DoubleToPrecisionCString(double value, int f);
char* DoubleToRadixCString(double value, int radix);

static inline bool IsMinusZero(double value) {
  return bit_cast<int64_t>(value) == bit_cast<int64_t>(-0.0);
}

// Returns true if value can be converted to a SMI, and returns the resulting
// integer value of the SMI in |smi_int_value|.
inline bool DoubleToSmiInteger(double value, int* smi_int_value);

inline bool IsSmiDouble(double value);

// Integer32 is an integer that can be represented as a signed 32-bit
// integer. It has to be in the range [-2^31, 2^31 - 1].
// We also have to check for negative 0 as it is not an Integer32.
inline bool IsInt32Double(double value);

// UInteger32 is an integer that can be represented as an unsigned 32-bit
// integer. It has to be in the range [0, 2^32 - 1].
// We also have to check for negative 0 as it is not a UInteger32.
inline bool IsUint32Double(double value);

// Tries to convert |value| to a uint32, setting the result in |uint32_value|.
// If the output does not compare equal to the input, returns false and the
// value in |uint32_value| is left unspecified.
// Used for conversions such as in ECMA-262 15.4.2.2, which check "ToUint32(len)
// is equal to len".
inline bool DoubleToUint32IfEqualToSelf(double value, uint32_t* uint32_value);

// Convert from Number object to C integer.
inline uint32_t PositiveNumberToUint32(Object* number);
inline int32_t NumberToInt32(Object* number);
inline uint32_t NumberToUint32(Object* number);
inline int64_t NumberToInt64(Object* number);

double StringToDouble(UnicodeCache* unicode_cache, Handle<String> string,
                      int flags, double empty_string_val = 0.0);

inline bool TryNumberToSize(Object* number, size_t* result);

// Converts a number into size_t.
inline size_t NumberToSize(Object* number);

// returns DoubleToString(StringToDouble(string)) == string
bool IsSpecialIndex(UnicodeCache* unicode_cache, String* string);

}  // namespace internal
}  // namespace v8

#endif  // V8_CONVERSIONS_H_

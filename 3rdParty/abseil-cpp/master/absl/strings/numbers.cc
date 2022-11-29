// Copyright 2017 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// This file contains string processing functions related to
// numeric values.

#include "absl/strings/numbers.h"

#include <algorithm>
#include <cassert>
#include <cfloat>  // for DBL_DIG and FLT_DIG
#include <cmath>   // for HUGE_VAL
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <limits>
#include <memory>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/internal/raw_logging.h"
#include "absl/numeric/bits.h"
#include "absl/strings/ascii.h"
#include "absl/strings/charconv.h"
#include "absl/strings/escaping.h"
#include "absl/strings/internal/memutil.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"

namespace absl {
ABSL_NAMESPACE_BEGIN

bool SimpleAtof(absl::string_view str, float* out) {
  *out = 0.0;
  str = StripAsciiWhitespace(str);
  // std::from_chars doesn't accept an initial +, but SimpleAtof does, so if one
  // is present, skip it, while avoiding accepting "+-0" as valid.
  if (!str.empty() && str[0] == '+') {
    str.remove_prefix(1);
    if (!str.empty() && str[0] == '-') {
      return false;
    }
  }
  auto result = absl::from_chars(str.data(), str.data() + str.size(), *out);
  if (result.ec == std::errc::invalid_argument) {
    return false;
  }
  if (result.ptr != str.data() + str.size()) {
    // not all non-whitespace characters consumed
    return false;
  }
  // from_chars() with DR 3081's current wording will return max() on
  // overflow.  SimpleAtof returns infinity instead.
  if (result.ec == std::errc::result_out_of_range) {
    if (*out > 1.0) {
      *out = std::numeric_limits<float>::infinity();
    } else if (*out < -1.0) {
      *out = -std::numeric_limits<float>::infinity();
    }
  }
  return true;
}

bool SimpleAtod(absl::string_view str, double* out) {
  *out = 0.0;
  str = StripAsciiWhitespace(str);
  // std::from_chars doesn't accept an initial +, but SimpleAtod does, so if one
  // is present, skip it, while avoiding accepting "+-0" as valid.
  if (!str.empty() && str[0] == '+') {
    str.remove_prefix(1);
    if (!str.empty() && str[0] == '-') {
      return false;
    }
  }
  auto result = absl::from_chars(str.data(), str.data() + str.size(), *out);
  if (result.ec == std::errc::invalid_argument) {
    return false;
  }
  if (result.ptr != str.data() + str.size()) {
    // not all non-whitespace characters consumed
    return false;
  }
  // from_chars() with DR 3081's current wording will return max() on
  // overflow.  SimpleAtod returns infinity instead.
  if (result.ec == std::errc::result_out_of_range) {
    if (*out > 1.0) {
      *out = std::numeric_limits<double>::infinity();
    } else if (*out < -1.0) {
      *out = -std::numeric_limits<double>::infinity();
    }
  }
  return true;
}

bool SimpleAtob(absl::string_view str, bool* out) {
  ABSL_RAW_CHECK(out != nullptr, "Output pointer must not be nullptr.");
  if (EqualsIgnoreCase(str, "true") || EqualsIgnoreCase(str, "t") ||
      EqualsIgnoreCase(str, "yes") || EqualsIgnoreCase(str, "y") ||
      EqualsIgnoreCase(str, "1")) {
    *out = true;
    return true;
  }
  if (EqualsIgnoreCase(str, "false") || EqualsIgnoreCase(str, "f") ||
      EqualsIgnoreCase(str, "no") || EqualsIgnoreCase(str, "n") ||
      EqualsIgnoreCase(str, "0")) {
    *out = false;
    return true;
  }
  return false;
}

// ----------------------------------------------------------------------
// FastIntToBuffer() overloads
//
// Like the Fast*ToBuffer() functions above, these are intended for speed.
// Unlike the Fast*ToBuffer() functions, however, these functions write
// their output to the beginning of the buffer.  The caller is responsible
// for ensuring that the buffer has enough space to hold the output.
//
// Returns a pointer to the end of the string (i.e. the null character
// terminating the string).
// ----------------------------------------------------------------------

namespace {

// Used to optimize printing a decimal number's final digit.
const char one_ASCII_final_digits[10][2] {
  {'0', 0}, {'1', 0}, {'2', 0}, {'3', 0}, {'4', 0},
  {'5', 0}, {'6', 0}, {'7', 0}, {'8', 0}, {'9', 0},
};

}  // namespace

char* numbers_internal::FastIntToBuffer(uint32_t i, char* buffer) {
  uint32_t digits;
  // The idea of this implementation is to trim the number of divides to as few
  // as possible, and also reducing memory stores and branches, by going in
  // steps of two digits at a time rather than one whenever possible.
  // The huge-number case is first, in the hopes that the compiler will output
  // that case in one branch-free block of code, and only output conditional
  // branches into it from below.
  if (i >= 1000000000) {     // >= 1,000,000,000
    digits = i / 100000000;  //      100,000,000
    i -= digits * 100000000;
    PutTwoDigits(digits, buffer);
    buffer += 2;
  lt100_000_000:
    digits = i / 1000000;  // 1,000,000
    i -= digits * 1000000;
    PutTwoDigits(digits, buffer);
    buffer += 2;
  lt1_000_000:
    digits = i / 10000;  // 10,000
    i -= digits * 10000;
    PutTwoDigits(digits, buffer);
    buffer += 2;
  lt10_000:
    digits = i / 100;
    i -= digits * 100;
    PutTwoDigits(digits, buffer);
    buffer += 2;
 lt100:
    digits = i;
    PutTwoDigits(digits, buffer);
    buffer += 2;
    *buffer = 0;
    return buffer;
  }

  if (i < 100) {
    digits = i;
    if (i >= 10) goto lt100;
    memcpy(buffer, one_ASCII_final_digits[i], 2);
    return buffer + 1;
  }
  if (i < 10000) {  //    10,000
    if (i >= 1000) goto lt10_000;
    digits = i / 100;
    i -= digits * 100;
    *buffer++ = '0' + static_cast<char>(digits);
    goto lt100;
  }
  if (i < 1000000) {  //    1,000,000
    if (i >= 100000) goto lt1_000_000;
    digits = i / 10000;  //    10,000
    i -= digits * 10000;
    *buffer++ = '0' + static_cast<char>(digits);
    goto lt10_000;
  }
  if (i < 100000000) {  //    100,000,000
    if (i >= 10000000) goto lt100_000_000;
    digits = i / 1000000;  //   1,000,000
    i -= digits * 1000000;
    *buffer++ = '0' + static_cast<char>(digits);
    goto lt1_000_000;
  }
  // we already know that i < 1,000,000,000
  digits = i / 100000000;  //   100,000,000
  i -= digits * 100000000;
  *buffer++ = '0' + static_cast<char>(digits);
  goto lt100_000_000;
}

char* numbers_internal::FastIntToBuffer(int32_t i, char* buffer) {
  uint32_t u = static_cast<uint32_t>(i);
  if (i < 0) {
    *buffer++ = '-';
    // We need to do the negation in modular (i.e., "unsigned")
    // arithmetic; MSVC++ apprently warns for plain "-u", so
    // we write the equivalent expression "0 - u" instead.
    u = 0 - u;
  }
  return numbers_internal::FastIntToBuffer(u, buffer);
}

char* numbers_internal::FastIntToBuffer(uint64_t i, char* buffer) {
  uint32_t u32 = static_cast<uint32_t>(i);
  if (u32 == i) return numbers_internal::FastIntToBuffer(u32, buffer);

  // Here we know i has at least 10 decimal digits.
  uint64_t top_1to11 = i / 1000000000;
  u32 = static_cast<uint32_t>(i - top_1to11 * 1000000000);
  uint32_t top_1to11_32 = static_cast<uint32_t>(top_1to11);

  if (top_1to11_32 == top_1to11) {
    buffer = numbers_internal::FastIntToBuffer(top_1to11_32, buffer);
  } else {
    // top_1to11 has more than 32 bits too; print it in two steps.
    uint32_t top_8to9 = static_cast<uint32_t>(top_1to11 / 100);
    uint32_t mid_2 = static_cast<uint32_t>(top_1to11 - top_8to9 * 100);
    buffer = numbers_internal::FastIntToBuffer(top_8to9, buffer);
    PutTwoDigits(mid_2, buffer);
    buffer += 2;
  }

  // We have only 9 digits now, again the maximum uint32_t can handle fully.
  uint32_t digits = u32 / 10000000;  // 10,000,000
  u32 -= digits * 10000000;
  PutTwoDigits(digits, buffer);
  buffer += 2;
  digits = u32 / 100000;  // 100,000
  u32 -= digits * 100000;
  PutTwoDigits(digits, buffer);
  buffer += 2;
  digits = u32 / 1000;  // 1,000
  u32 -= digits * 1000;
  PutTwoDigits(digits, buffer);
  buffer += 2;
  digits = u32 / 10;
  u32 -= digits * 10;
  PutTwoDigits(digits, buffer);
  buffer += 2;
  memcpy(buffer, one_ASCII_final_digits[u32], 2);
  return buffer + 1;
}

char* numbers_internal::FastIntToBuffer(int64_t i, char* buffer) {
  uint64_t u = static_cast<uint64_t>(i);
  if (i < 0) {
    *buffer++ = '-';
    u = 0 - u;
  }
  return numbers_internal::FastIntToBuffer(u, buffer);
}

// Given a 128-bit number expressed as a pair of uint64_t, high half first,
// return that number multiplied by the given 32-bit value.  If the result is
// too large to fit in a 128-bit number, divide it by 2 until it fits.
static std::pair<uint64_t, uint64_t> Mul32(std::pair<uint64_t, uint64_t> num,
                                           uint32_t mul) {
  uint64_t bits0_31 = num.second & 0xFFFFFFFF;
  uint64_t bits32_63 = num.second >> 32;
  uint64_t bits64_95 = num.first & 0xFFFFFFFF;
  uint64_t bits96_127 = num.first >> 32;

  // The picture so far: each of these 64-bit values has only the lower 32 bits
  // filled in.
  // bits96_127:          [ 00000000 xxxxxxxx ]
  // bits64_95:                    [ 00000000 xxxxxxxx ]
  // bits32_63:                             [ 00000000 xxxxxxxx ]
  // bits0_31:                                       [ 00000000 xxxxxxxx ]

  bits0_31 *= mul;
  bits32_63 *= mul;
  bits64_95 *= mul;
  bits96_127 *= mul;

  // Now the top halves may also have value, though all 64 of their bits will
  // never be set at the same time, since they are a result of a 32x32 bit
  // multiply.  This makes the carry calculation slightly easier.
  // bits96_127:          [ mmmmmmmm | mmmmmmmm ]
  // bits64_95:                    [ | mmmmmmmm mmmmmmmm | ]
  // bits32_63:                      |        [ mmmmmmmm | mmmmmmmm ]
  // bits0_31:                       |                 [ | mmmmmmmm mmmmmmmm ]
  // eventually:        [ bits128_up | ...bits64_127.... | ..bits0_63... ]

  uint64_t bits0_63 = bits0_31 + (bits32_63 << 32);
  uint64_t bits64_127 = bits64_95 + (bits96_127 << 32) + (bits32_63 >> 32) +
                        (bits0_63 < bits0_31);
  uint64_t bits128_up = (bits96_127 >> 32) + (bits64_127 < bits64_95);
  if (bits128_up == 0) return {bits64_127, bits0_63};

  auto shift = static_cast<unsigned>(bit_width(bits128_up));
  uint64_t lo = (bits0_63 >> shift) + (bits64_127 << (64 - shift));
  uint64_t hi = (bits64_127 >> shift) + (bits128_up << (64 - shift));
  return {hi, lo};
}

// Compute num * 5 ^ expfive, and return the first 128 bits of the result,
// where the first bit is always a one.  So PowFive(1, 0) starts 0b100000,
// PowFive(1, 1) starts 0b101000, PowFive(1, 2) starts 0b110010, etc.
static std::pair<uint64_t, uint64_t> PowFive(uint64_t num, int expfive) {
  std::pair<uint64_t, uint64_t> result = {num, 0};
  while (expfive >= 13) {
    // 5^13 is the highest power of five that will fit in a 32-bit integer.
    result = Mul32(result, 5 * 5 * 5 * 5 * 5 * 5 * 5 * 5 * 5 * 5 * 5 * 5 * 5);
    expfive -= 13;
  }
  constexpr uint32_t powers_of_five[13] = {
      1,
      5,
      5 * 5,
      5 * 5 * 5,
      5 * 5 * 5 * 5,
      5 * 5 * 5 * 5 * 5,
      5 * 5 * 5 * 5 * 5 * 5,
      5 * 5 * 5 * 5 * 5 * 5 * 5,
      5 * 5 * 5 * 5 * 5 * 5 * 5 * 5,
      5 * 5 * 5 * 5 * 5 * 5 * 5 * 5 * 5,
      5 * 5 * 5 * 5 * 5 * 5 * 5 * 5 * 5 * 5,
      5 * 5 * 5 * 5 * 5 * 5 * 5 * 5 * 5 * 5 * 5,
      5 * 5 * 5 * 5 * 5 * 5 * 5 * 5 * 5 * 5 * 5 * 5};
  result = Mul32(result, powers_of_five[expfive & 15]);
  int shift = countl_zero(result.first);
  if (shift != 0) {
    result.first = (result.first << shift) + (result.second >> (64 - shift));
    result.second = (result.second << shift);
  }
  return result;
}

struct ExpDigits {
  int32_t exponent;
  char digits[6];
};

// SplitToSix converts value, a positive double-precision floating-point number,
// into a base-10 exponent and 6 ASCII digits, where the first digit is never
// zero.  For example, SplitToSix(1) returns an exponent of zero and a digits
// array of {'1', '0', '0', '0', '0', '0'}.  If value is exactly halfway between
// two possible representations, e.g. value = 100000.5, then "round to even" is
// performed.
static ExpDigits SplitToSix(const double value) {
  ExpDigits exp_dig;
  int exp = 5;
  double d = value;
  // First step: calculate a close approximation of the output, where the
  // value d will be between 100,000 and 999,999, representing the digits
  // in the output ASCII array, and exp is the base-10 exponent.  It would be
  // faster to use a table here, and to look up the base-2 exponent of value,
  // however value is an IEEE-754 64-bit number, so the table would have 2,000
  // entries, which is not cache-friendly.
  if (d >= 999999.5) {
    if (d >= 1e+261) exp += 256, d *= 1e-256;
    if (d >= 1e+133) exp += 128, d *= 1e-128;
    if (d >= 1e+69) exp += 64, d *= 1e-64;
    if (d >= 1e+37) exp += 32, d *= 1e-32;
    if (d >= 1e+21) exp += 16, d *= 1e-16;
    if (d >= 1e+13) exp += 8, d *= 1e-8;
    if (d >= 1e+9) exp += 4, d *= 1e-4;
    if (d >= 1e+7) exp += 2, d *= 1e-2;
    if (d >= 1e+6) exp += 1, d *= 1e-1;
  } else {
    if (d < 1e-250) exp -= 256, d *= 1e256;
    if (d < 1e-122) exp -= 128, d *= 1e128;
    if (d < 1e-58) exp -= 64, d *= 1e64;
    if (d < 1e-26) exp -= 32, d *= 1e32;
    if (d < 1e-10) exp -= 16, d *= 1e16;
    if (d < 1e-2) exp -= 8, d *= 1e8;
    if (d < 1e+2) exp -= 4, d *= 1e4;
    if (d < 1e+4) exp -= 2, d *= 1e2;
    if (d < 1e+5) exp -= 1, d *= 1e1;
  }
  // At this point, d is in the range [99999.5..999999.5) and exp is in the
  // range [-324..308]. Since we need to round d up, we want to add a half
  // and truncate.
  // However, the technique above may have lost some precision, due to its
  // repeated multiplication by constants that each may be off by half a bit
  // of precision.  This only matters if we're close to the edge though.
  // Since we'd like to know if the fractional part of d is close to a half,
  // we multiply it by 65536 and see if the fractional part is close to 32768.
  // (The number doesn't have to be a power of two,but powers of two are faster)
  uint64_t d64k = d * 65536;
  uint32_t dddddd;  // A 6-digit decimal integer.
  if ((d64k % 65536) == 32767 || (d64k % 65536) == 32768) {
    // OK, it's fairly likely that precision was lost above, which is
    // not a surprise given only 52 mantissa bits are available.  Therefore
    // redo the calculation using 128-bit numbers.  (64 bits are not enough).

    // Start out with digits rounded down; maybe add one below.
    dddddd = static_cast<uint32_t>(d64k / 65536);

    // mantissa is a 64-bit integer representing M.mmm... * 2^63.  The actual
    // value we're representing, of course, is M.mmm... * 2^exp2.
    int exp2;
    double m = std::frexp(value, &exp2);
    uint64_t mantissa = m * (32768.0 * 65536.0 * 65536.0 * 65536.0);
    // std::frexp returns an m value in the range [0.5, 1.0), however we
    // can't multiply it by 2^64 and convert to an integer because some FPUs
    // throw an exception when converting an number higher than 2^63 into an
    // integer - even an unsigned 64-bit integer!  Fortunately it doesn't matter
    // since m only has 52 significant bits anyway.
    mantissa <<= 1;
    exp2 -= 64;  // not needed, but nice for debugging

    // OK, we are here to compare:
    //     (dddddd + 0.5) * 10^(exp-5)  vs.  mantissa * 2^exp2
    // so we can round up dddddd if appropriate.  Those values span the full
    // range of 600 orders of magnitude of IEE 64-bit floating-point.
    // Fortunately, we already know they are very close, so we don't need to
    // track the base-2 exponent of both sides.  This greatly simplifies the
    // the math since the 2^exp2 calculation is unnecessary and the power-of-10
    // calculation can become a power-of-5 instead.

    std::pair<uint64_t, uint64_t> edge, val;
    if (exp >= 6) {
      // Compare (dddddd + 0.5) * 5 ^ (exp - 5) to mantissa
      // Since we're tossing powers of two, 2 * dddddd + 1 is the
      // same as dddddd + 0.5
      edge = PowFive(2 * dddddd + 1, exp - 5);

      val.first = mantissa;
      val.second = 0;
    } else {
      // We can't compare (dddddd + 0.5) * 5 ^ (exp - 5) to mantissa as we did
      // above because (exp - 5) is negative.  So we compare (dddddd + 0.5) to
      // mantissa * 5 ^ (5 - exp)
      edge = PowFive(2 * dddddd + 1, 0);

      val = PowFive(mantissa, 5 - exp);
    }
    // printf("exp=%d %016lx %016lx vs %016lx %016lx\n", exp, val.first,
    //        val.second, edge.first, edge.second);
    if (val > edge) {
      dddddd++;
    } else if (val == edge) {
      dddddd += (dddddd & 1);
    }
  } else {
    // Here, we are not close to the edge.
    dddddd = static_cast<uint32_t>((d64k + 32768) / 65536);
  }
  if (dddddd == 1000000) {
    dddddd = 100000;
    exp += 1;
  }
  exp_dig.exponent = exp;

  uint32_t two_digits = dddddd / 10000;
  dddddd -= two_digits * 10000;
  numbers_internal::PutTwoDigits(two_digits, &exp_dig.digits[0]);

  two_digits = dddddd / 100;
  dddddd -= two_digits * 100;
  numbers_internal::PutTwoDigits(two_digits, &exp_dig.digits[2]);

  numbers_internal::PutTwoDigits(dddddd, &exp_dig.digits[4]);
  return exp_dig;
}

// Helper function for fast formatting of floating-point.
// The result is the same as "%g", a.k.a. "%.6g".
size_t numbers_internal::SixDigitsToBuffer(double d, char* const buffer) {
  static_assert(std::numeric_limits<float>::is_iec559,
                "IEEE-754/IEC-559 support only");

  char* out = buffer;  // we write data to out, incrementing as we go, but
                       // FloatToBuffer always returns the address of the buffer
                       // passed in.

  if (std::isnan(d)) {
    strcpy(out, "nan");  // NOLINT(runtime/printf)
    return 3;
  }
  if (d == 0) {  // +0 and -0 are handled here
    if (std::signbit(d)) *out++ = '-';
    *out++ = '0';
    *out = 0;
    return static_cast<size_t>(out - buffer);
  }
  if (d < 0) {
    *out++ = '-';
    d = -d;
  }
  if (d > std::numeric_limits<double>::max()) {
    strcpy(out, "inf");  // NOLINT(runtime/printf)
    return static_cast<size_t>(out + 3 - buffer);
  }

  auto exp_dig = SplitToSix(d);
  int exp = exp_dig.exponent;
  const char* digits = exp_dig.digits;
  out[0] = '0';
  out[1] = '.';
  switch (exp) {
    case 5:
      memcpy(out, &digits[0], 6), out += 6;
      *out = 0;
      return static_cast<size_t>(out - buffer);
    case 4:
      memcpy(out, &digits[0], 5), out += 5;
      if (digits[5] != '0') {
        *out++ = '.';
        *out++ = digits[5];
      }
      *out = 0;
      return static_cast<size_t>(out - buffer);
    case 3:
      memcpy(out, &digits[0], 4), out += 4;
      if ((digits[5] | digits[4]) != '0') {
        *out++ = '.';
        *out++ = digits[4];
        if (digits[5] != '0') *out++ = digits[5];
      }
      *out = 0;
      return static_cast<size_t>(out - buffer);
    case 2:
      memcpy(out, &digits[0], 3), out += 3;
      *out++ = '.';
      memcpy(out, &digits[3], 3);
      out += 3;
      while (out[-1] == '0') --out;
      if (out[-1] == '.') --out;
      *out = 0;
      return static_cast<size_t>(out - buffer);
    case 1:
      memcpy(out, &digits[0], 2), out += 2;
      *out++ = '.';
      memcpy(out, &digits[2], 4);
      out += 4;
      while (out[-1] == '0') --out;
      if (out[-1] == '.') --out;
      *out = 0;
      return static_cast<size_t>(out - buffer);
    case 0:
      memcpy(out, &digits[0], 1), out += 1;
      *out++ = '.';
      memcpy(out, &digits[1], 5);
      out += 5;
      while (out[-1] == '0') --out;
      if (out[-1] == '.') --out;
      *out = 0;
      return static_cast<size_t>(out - buffer);
    case -4:
      out[2] = '0';
      ++out;
      ABSL_FALLTHROUGH_INTENDED;
    case -3:
      out[2] = '0';
      ++out;
      ABSL_FALLTHROUGH_INTENDED;
    case -2:
      out[2] = '0';
      ++out;
      ABSL_FALLTHROUGH_INTENDED;
    case -1:
      out += 2;
      memcpy(out, &digits[0], 6);
      out += 6;
      while (out[-1] == '0') --out;
      *out = 0;
      return static_cast<size_t>(out - buffer);
  }
  assert(exp < -4 || exp >= 6);
  out[0] = digits[0];
  assert(out[1] == '.');
  out += 2;
  memcpy(out, &digits[1], 5), out += 5;
  while (out[-1] == '0') --out;
  if (out[-1] == '.') --out;
  *out++ = 'e';
  if (exp > 0) {
    *out++ = '+';
  } else {
    *out++ = '-';
    exp = -exp;
  }
  if (exp > 99) {
    int dig1 = exp / 100;
    exp -= dig1 * 100;
    *out++ = '0' + static_cast<char>(dig1);
  }
  PutTwoDigits(static_cast<uint32_t>(exp), out);
  out += 2;
  *out = 0;
  return static_cast<size_t>(out - buffer);
}

namespace {
// Represents integer values of digits.
// Uses 36 to indicate an invalid character since we support
// bases up to 36.
static const int8_t kAsciiToInt[256] = {
    36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36,  // 16 36s.
    36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
    36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 0,  1,  2,  3,  4,  5,
    6,  7,  8,  9,  36, 36, 36, 36, 36, 36, 36, 10, 11, 12, 13, 14, 15, 16, 17,
    18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36,
    36, 36, 36, 36, 36, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
    24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 36, 36, 36, 36, 36, 36,
    36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
    36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
    36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
    36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
    36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
    36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
    36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36};

// Parse the sign and optional hex or oct prefix in text.
inline bool safe_parse_sign_and_base(absl::string_view* text /*inout*/,
                                     int* base_ptr /*inout*/,
                                     bool* negative_ptr /*output*/) {
  if (text->data() == nullptr) {
    return false;
  }

  const char* start = text->data();
  const char* end = start + text->size();
  int base = *base_ptr;

  // Consume whitespace.
  while (start < end &&
         absl::ascii_isspace(static_cast<unsigned char>(start[0]))) {
    ++start;
  }
  while (start < end &&
         absl::ascii_isspace(static_cast<unsigned char>(end[-1]))) {
    --end;
  }
  if (start >= end) {
    return false;
  }

  // Consume sign.
  *negative_ptr = (start[0] == '-');
  if (*negative_ptr || start[0] == '+') {
    ++start;
    if (start >= end) {
      return false;
    }
  }

  // Consume base-dependent prefix.
  //  base 0: "0x" -> base 16, "0" -> base 8, default -> base 10
  //  base 16: "0x" -> base 16
  // Also validate the base.
  if (base == 0) {
    if (end - start >= 2 && start[0] == '0' &&
        (start[1] == 'x' || start[1] == 'X')) {
      base = 16;
      start += 2;
      if (start >= end) {
        // "0x" with no digits after is invalid.
        return false;
      }
    } else if (end - start >= 1 && start[0] == '0') {
      base = 8;
      start += 1;
    } else {
      base = 10;
    }
  } else if (base == 16) {
    if (end - start >= 2 && start[0] == '0' &&
        (start[1] == 'x' || start[1] == 'X')) {
      start += 2;
      if (start >= end) {
        // "0x" with no digits after is invalid.
        return false;
      }
    }
  } else if (base >= 2 && base <= 36) {
    // okay
  } else {
    return false;
  }
  *text = absl::string_view(start, static_cast<size_t>(end - start));
  *base_ptr = base;
  return true;
}

// Consume digits.
//
// The classic loop:
//
//   for each digit
//     value = value * base + digit
//   value *= sign
//
// The classic loop needs overflow checking.  It also fails on the most
// negative integer, -2147483648 in 32-bit two's complement representation.
//
// My improved loop:
//
//  if (!negative)
//    for each digit
//      value = value * base
//      value = value + digit
//  else
//    for each digit
//      value = value * base
//      value = value - digit
//
// Overflow checking becomes simple.

// Lookup tables per IntType:
// vmax/base and vmin/base are precomputed because division costs at least 8ns.
// TODO(junyer): Doing this per base instead (i.e. an array of structs, not a
// struct of arrays) would probably be better in terms of d-cache for the most
// commonly used bases.
template <typename IntType>
struct LookupTables {
  ABSL_CONST_INIT static const IntType kVmaxOverBase[];
  ABSL_CONST_INIT static const IntType kVminOverBase[];
};

// An array initializer macro for X/base where base in [0, 36].
// However, note that lookups for base in [0, 1] should never happen because
// base has been validated to be in [2, 36] by safe_parse_sign_and_base().
#define X_OVER_BASE_INITIALIZER(X)                                        \
  {                                                                       \
    0, 0, X / 2, X / 3, X / 4, X / 5, X / 6, X / 7, X / 8, X / 9, X / 10, \
        X / 11, X / 12, X / 13, X / 14, X / 15, X / 16, X / 17, X / 18,   \
        X / 19, X / 20, X / 21, X / 22, X / 23, X / 24, X / 25, X / 26,   \
        X / 27, X / 28, X / 29, X / 30, X / 31, X / 32, X / 33, X / 34,   \
        X / 35, X / 36,                                                   \
  }

// This kVmaxOverBase is generated with
//  for (int base = 2; base < 37; ++base) {
//    absl::uint128 max = std::numeric_limits<absl::uint128>::max();
//    auto result = max / base;
//    std::cout << "    MakeUint128(" << absl::Uint128High64(result) << "u, "
//              << absl::Uint128Low64(result) << "u),\n";
//  }
// See https://godbolt.org/z/aneYsb
//
// uint128& operator/=(uint128) is not constexpr, so hardcode the resulting
// array to avoid a static initializer.
template <>
ABSL_CONST_INIT const uint128 LookupTables<uint128>::kVmaxOverBase[] = {
    0,
    0,
    MakeUint128(9223372036854775807u, 18446744073709551615u),
    MakeUint128(6148914691236517205u, 6148914691236517205u),
    MakeUint128(4611686018427387903u, 18446744073709551615u),
    MakeUint128(3689348814741910323u, 3689348814741910323u),
    MakeUint128(3074457345618258602u, 12297829382473034410u),
    MakeUint128(2635249153387078802u, 5270498306774157604u),
    MakeUint128(2305843009213693951u, 18446744073709551615u),
    MakeUint128(2049638230412172401u, 14347467612885206812u),
    MakeUint128(1844674407370955161u, 11068046444225730969u),
    MakeUint128(1676976733973595601u, 8384883669867978007u),
    MakeUint128(1537228672809129301u, 6148914691236517205u),
    MakeUint128(1418980313362273201u, 4256940940086819603u),
    MakeUint128(1317624576693539401u, 2635249153387078802u),
    MakeUint128(1229782938247303441u, 1229782938247303441u),
    MakeUint128(1152921504606846975u, 18446744073709551615u),
    MakeUint128(1085102592571150095u, 1085102592571150095u),
    MakeUint128(1024819115206086200u, 16397105843297379214u),
    MakeUint128(970881267037344821u, 16504981539634861972u),
    MakeUint128(922337203685477580u, 14757395258967641292u),
    MakeUint128(878416384462359600u, 14054662151397753612u),
    MakeUint128(838488366986797800u, 13415813871788764811u),
    MakeUint128(802032351030850070u, 4812194106185100421u),
    MakeUint128(768614336404564650u, 12297829382473034410u),
    MakeUint128(737869762948382064u, 11805916207174113034u),
    MakeUint128(709490156681136600u, 11351842506898185609u),
    MakeUint128(683212743470724133u, 17080318586768103348u),
    MakeUint128(658812288346769700u, 10540996613548315209u),
    MakeUint128(636094623231363848u, 15266270957552732371u),
    MakeUint128(614891469123651720u, 9838263505978427528u),
    MakeUint128(595056260442243600u, 9520900167075897608u),
    MakeUint128(576460752303423487u, 18446744073709551615u),
    MakeUint128(558992244657865200u, 8943875914525843207u),
    MakeUint128(542551296285575047u, 9765923333140350855u),
    MakeUint128(527049830677415760u, 8432797290838652167u),
    MakeUint128(512409557603043100u, 8198552921648689607u),
};

// This kVmaxOverBase generated with
//   for (int base = 2; base < 37; ++base) {
//    absl::int128 max = std::numeric_limits<absl::int128>::max();
//    auto result = max / base;
//    std::cout << "\tMakeInt128(" << absl::Int128High64(result) << ", "
//              << absl::Int128Low64(result) << "u),\n";
//  }
// See https://godbolt.org/z/7djYWz
//
// int128& operator/=(int128) is not constexpr, so hardcode the resulting array
// to avoid a static initializer.
template <>
ABSL_CONST_INIT const int128 LookupTables<int128>::kVmaxOverBase[] = {
    0,
    0,
    MakeInt128(4611686018427387903, 18446744073709551615u),
    MakeInt128(3074457345618258602, 12297829382473034410u),
    MakeInt128(2305843009213693951, 18446744073709551615u),
    MakeInt128(1844674407370955161, 11068046444225730969u),
    MakeInt128(1537228672809129301, 6148914691236517205u),
    MakeInt128(1317624576693539401, 2635249153387078802u),
    MakeInt128(1152921504606846975, 18446744073709551615u),
    MakeInt128(1024819115206086200, 16397105843297379214u),
    MakeInt128(922337203685477580, 14757395258967641292u),
    MakeInt128(838488366986797800, 13415813871788764811u),
    MakeInt128(768614336404564650, 12297829382473034410u),
    MakeInt128(709490156681136600, 11351842506898185609u),
    MakeInt128(658812288346769700, 10540996613548315209u),
    MakeInt128(614891469123651720, 9838263505978427528u),
    MakeInt128(576460752303423487, 18446744073709551615u),
    MakeInt128(542551296285575047, 9765923333140350855u),
    MakeInt128(512409557603043100, 8198552921648689607u),
    MakeInt128(485440633518672410, 17475862806672206794u),
    MakeInt128(461168601842738790, 7378697629483820646u),
    MakeInt128(439208192231179800, 7027331075698876806u),
    MakeInt128(419244183493398900, 6707906935894382405u),
    MakeInt128(401016175515425035, 2406097053092550210u),
    MakeInt128(384307168202282325, 6148914691236517205u),
    MakeInt128(368934881474191032, 5902958103587056517u),
    MakeInt128(354745078340568300, 5675921253449092804u),
    MakeInt128(341606371735362066, 17763531330238827482u),
    MakeInt128(329406144173384850, 5270498306774157604u),
    MakeInt128(318047311615681924, 7633135478776366185u),
    MakeInt128(307445734561825860, 4919131752989213764u),
    MakeInt128(297528130221121800, 4760450083537948804u),
    MakeInt128(288230376151711743, 18446744073709551615u),
    MakeInt128(279496122328932600, 4471937957262921603u),
    MakeInt128(271275648142787523, 14106333703424951235u),
    MakeInt128(263524915338707880, 4216398645419326083u),
    MakeInt128(256204778801521550, 4099276460824344803u),
};

// This kVminOverBase generated with
//  for (int base = 2; base < 37; ++base) {
//    absl::int128 min = std::numeric_limits<absl::int128>::min();
//    auto result = min / base;
//    std::cout << "\tMakeInt128(" << absl::Int128High64(result) << ", "
//              << absl::Int128Low64(result) << "u),\n";
//  }
//
// See https://godbolt.org/z/7djYWz
//
// int128& operator/=(int128) is not constexpr, so hardcode the resulting array
// to avoid a static initializer.
template <>
ABSL_CONST_INIT const int128 LookupTables<int128>::kVminOverBase[] = {
    0,
    0,
    MakeInt128(-4611686018427387904, 0u),
    MakeInt128(-3074457345618258603, 6148914691236517206u),
    MakeInt128(-2305843009213693952, 0u),
    MakeInt128(-1844674407370955162, 7378697629483820647u),
    MakeInt128(-1537228672809129302, 12297829382473034411u),
    MakeInt128(-1317624576693539402, 15811494920322472814u),
    MakeInt128(-1152921504606846976, 0u),
    MakeInt128(-1024819115206086201, 2049638230412172402u),
    MakeInt128(-922337203685477581, 3689348814741910324u),
    MakeInt128(-838488366986797801, 5030930201920786805u),
    MakeInt128(-768614336404564651, 6148914691236517206u),
    MakeInt128(-709490156681136601, 7094901566811366007u),
    MakeInt128(-658812288346769701, 7905747460161236407u),
    MakeInt128(-614891469123651721, 8608480567731124088u),
    MakeInt128(-576460752303423488, 0u),
    MakeInt128(-542551296285575048, 8680820740569200761u),
    MakeInt128(-512409557603043101, 10248191152060862009u),
    MakeInt128(-485440633518672411, 970881267037344822u),
    MakeInt128(-461168601842738791, 11068046444225730970u),
    MakeInt128(-439208192231179801, 11419412998010674810u),
    MakeInt128(-419244183493398901, 11738837137815169211u),
    MakeInt128(-401016175515425036, 16040647020617001406u),
    MakeInt128(-384307168202282326, 12297829382473034411u),
    MakeInt128(-368934881474191033, 12543785970122495099u),
    MakeInt128(-354745078340568301, 12770822820260458812u),
    MakeInt128(-341606371735362067, 683212743470724134u),
    MakeInt128(-329406144173384851, 13176245766935394012u),
    MakeInt128(-318047311615681925, 10813608594933185431u),
    MakeInt128(-307445734561825861, 13527612320720337852u),
    MakeInt128(-297528130221121801, 13686293990171602812u),
    MakeInt128(-288230376151711744, 0u),
    MakeInt128(-279496122328932601, 13974806116446630013u),
    MakeInt128(-271275648142787524, 4340410370284600381u),
    MakeInt128(-263524915338707881, 14230345428290225533u),
    MakeInt128(-256204778801521551, 14347467612885206813u),
};

template <typename IntType>
ABSL_CONST_INIT const IntType LookupTables<IntType>::kVmaxOverBase[] =
    X_OVER_BASE_INITIALIZER(std::numeric_limits<IntType>::max());

template <typename IntType>
ABSL_CONST_INIT const IntType LookupTables<IntType>::kVminOverBase[] =
    X_OVER_BASE_INITIALIZER(std::numeric_limits<IntType>::min());

#undef X_OVER_BASE_INITIALIZER

template <typename IntType>
inline bool safe_parse_positive_int(absl::string_view text, int base,
                                    IntType* value_p) {
  IntType value = 0;
  const IntType vmax = std::numeric_limits<IntType>::max();
  assert(vmax > 0);
  assert(base >= 0);
  const IntType base_inttype = static_cast<IntType>(base);
  assert(vmax >= base_inttype);
  const IntType vmax_over_base = LookupTables<IntType>::kVmaxOverBase[base];
  assert(base < 2 ||
         std::numeric_limits<IntType>::max() / base_inttype == vmax_over_base);
  const char* start = text.data();
  const char* end = start + text.size();
  // loop over digits
  for (; start < end; ++start) {
    unsigned char c = static_cast<unsigned char>(start[0]);
    IntType digit = static_cast<IntType>(kAsciiToInt[c]);
    if (digit >= base_inttype) {
      *value_p = value;
      return false;
    }
    if (value > vmax_over_base) {
      *value_p = vmax;
      return false;
    }
    value *= base_inttype;
    if (value > vmax - digit) {
      *value_p = vmax;
      return false;
    }
    value += digit;
  }
  *value_p = value;
  return true;
}

template <typename IntType>
inline bool safe_parse_negative_int(absl::string_view text, int base,
                                    IntType* value_p) {
  IntType value = 0;
  const IntType vmin = std::numeric_limits<IntType>::min();
  assert(vmin < 0);
  assert(vmin <= 0 - base);
  IntType vmin_over_base = LookupTables<IntType>::kVminOverBase[base];
  assert(base < 2 ||
         std::numeric_limits<IntType>::min() / base == vmin_over_base);
  // 2003 c++ standard [expr.mul]
  // "... the sign of the remainder is implementation-defined."
  // Although (vmin/base)*base + vmin%base is always vmin.
  // 2011 c++ standard tightens the spec but we cannot rely on it.
  // TODO(junyer): Handle this in the lookup table generation.
  if (vmin % base > 0) {
    vmin_over_base += 1;
  }
  const char* start = text.data();
  const char* end = start + text.size();
  // loop over digits
  for (; start < end; ++start) {
    unsigned char c = static_cast<unsigned char>(start[0]);
    int digit = kAsciiToInt[c];
    if (digit >= base) {
      *value_p = value;
      return false;
    }
    if (value < vmin_over_base) {
      *value_p = vmin;
      return false;
    }
    value *= base;
    if (value < vmin + digit) {
      *value_p = vmin;
      return false;
    }
    value -= digit;
  }
  *value_p = value;
  return true;
}

// Input format based on POSIX.1-2008 strtol
// http://pubs.opengroup.org/onlinepubs/9699919799/functions/strtol.html
template <typename IntType>
inline bool safe_int_internal(absl::string_view text, IntType* value_p,
                              int base) {
  *value_p = 0;
  bool negative;
  if (!safe_parse_sign_and_base(&text, &base, &negative)) {
    return false;
  }
  if (!negative) {
    return safe_parse_positive_int(text, base, value_p);
  } else {
    return safe_parse_negative_int(text, base, value_p);
  }
}

template <typename IntType>
inline bool safe_uint_internal(absl::string_view text, IntType* value_p,
                               int base) {
  *value_p = 0;
  bool negative;
  if (!safe_parse_sign_and_base(&text, &base, &negative) || negative) {
    return false;
  }
  return safe_parse_positive_int(text, base, value_p);
}
}  // anonymous namespace

namespace numbers_internal {

// Digit conversion.
ABSL_CONST_INIT ABSL_DLL const char kHexChar[] =
    "0123456789abcdef";

ABSL_CONST_INIT ABSL_DLL const char kHexTable[513] =
    "000102030405060708090a0b0c0d0e0f"
    "101112131415161718191a1b1c1d1e1f"
    "202122232425262728292a2b2c2d2e2f"
    "303132333435363738393a3b3c3d3e3f"
    "404142434445464748494a4b4c4d4e4f"
    "505152535455565758595a5b5c5d5e5f"
    "606162636465666768696a6b6c6d6e6f"
    "707172737475767778797a7b7c7d7e7f"
    "808182838485868788898a8b8c8d8e8f"
    "909192939495969798999a9b9c9d9e9f"
    "a0a1a2a3a4a5a6a7a8a9aaabacadaeaf"
    "b0b1b2b3b4b5b6b7b8b9babbbcbdbebf"
    "c0c1c2c3c4c5c6c7c8c9cacbcccdcecf"
    "d0d1d2d3d4d5d6d7d8d9dadbdcdddedf"
    "e0e1e2e3e4e5e6e7e8e9eaebecedeeef"
    "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff";

ABSL_CONST_INIT ABSL_DLL const char two_ASCII_digits[100][2] = {
    {'0', '0'}, {'0', '1'}, {'0', '2'}, {'0', '3'}, {'0', '4'}, {'0', '5'},
    {'0', '6'}, {'0', '7'}, {'0', '8'}, {'0', '9'}, {'1', '0'}, {'1', '1'},
    {'1', '2'}, {'1', '3'}, {'1', '4'}, {'1', '5'}, {'1', '6'}, {'1', '7'},
    {'1', '8'}, {'1', '9'}, {'2', '0'}, {'2', '1'}, {'2', '2'}, {'2', '3'},
    {'2', '4'}, {'2', '5'}, {'2', '6'}, {'2', '7'}, {'2', '8'}, {'2', '9'},
    {'3', '0'}, {'3', '1'}, {'3', '2'}, {'3', '3'}, {'3', '4'}, {'3', '5'},
    {'3', '6'}, {'3', '7'}, {'3', '8'}, {'3', '9'}, {'4', '0'}, {'4', '1'},
    {'4', '2'}, {'4', '3'}, {'4', '4'}, {'4', '5'}, {'4', '6'}, {'4', '7'},
    {'4', '8'}, {'4', '9'}, {'5', '0'}, {'5', '1'}, {'5', '2'}, {'5', '3'},
    {'5', '4'}, {'5', '5'}, {'5', '6'}, {'5', '7'}, {'5', '8'}, {'5', '9'},
    {'6', '0'}, {'6', '1'}, {'6', '2'}, {'6', '3'}, {'6', '4'}, {'6', '5'},
    {'6', '6'}, {'6', '7'}, {'6', '8'}, {'6', '9'}, {'7', '0'}, {'7', '1'},
    {'7', '2'}, {'7', '3'}, {'7', '4'}, {'7', '5'}, {'7', '6'}, {'7', '7'},
    {'7', '8'}, {'7', '9'}, {'8', '0'}, {'8', '1'}, {'8', '2'}, {'8', '3'},
    {'8', '4'}, {'8', '5'}, {'8', '6'}, {'8', '7'}, {'8', '8'}, {'8', '9'},
    {'9', '0'}, {'9', '1'}, {'9', '2'}, {'9', '3'}, {'9', '4'}, {'9', '5'},
    {'9', '6'}, {'9', '7'}, {'9', '8'}, {'9', '9'}};

bool safe_strto32_base(absl::string_view text, int32_t* value, int base) {
  return safe_int_internal<int32_t>(text, value, base);
}

bool safe_strto64_base(absl::string_view text, int64_t* value, int base) {
  return safe_int_internal<int64_t>(text, value, base);
}

bool safe_strto128_base(absl::string_view text, int128* value, int base) {
  return safe_int_internal<absl::int128>(text, value, base);
}

bool safe_strtou32_base(absl::string_view text, uint32_t* value, int base) {
  return safe_uint_internal<uint32_t>(text, value, base);
}

bool safe_strtou64_base(absl::string_view text, uint64_t* value, int base) {
  return safe_uint_internal<uint64_t>(text, value, base);
}

bool safe_strtou128_base(absl::string_view text, uint128* value, int base) {
  return safe_uint_internal<absl::uint128>(text, value, base);
}

}  // namespace numbers_internal
ABSL_NAMESPACE_END
}  // namespace absl

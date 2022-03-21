// Copyright 2001 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

//
// This class is intended to contain a collection of useful (static)
// mathematical functions, properly coded (by consulting numerical
// recipes or another authoritative source first).

#ifndef S2_UTIL_MATH_MATHUTIL_H_
#define S2_UTIL_MATH_MATHUTIL_H_

#include <type_traits>

#include "s2/base/integral_types.h"
#include "s2/base/logging.h"
#include "s2/util/bits/bits.h"

// Returns the sign of x:
//   -1 if x < 0,
//   +1 if x > 0,
//    0 if x = 0,
//    unspecified if x is NaN.
template <class T>
inline T sgn(const T x) {
  return (x == 0 ? 0 : (x < 0 ? -1 : 1));
}

class MathUtil {
 public:
  // Solves for the real roots of x^3+ax^2+bx+c=0, returns true iff
  // all three are real, in which case the roots are stored (in any
  // order) in r1, r2, r3; otherwise, exactly one real root exists and
  // it is stored in r1.
  static bool RealRootsForCubic(long double a,
                                long double b,
                                long double c,
                                long double *r1,
                                long double *r2,
                                long double *r3);

  // --------------------------------------------------------------------
  // Round
  //   This function rounds a floating-point number to an integer. It
  //   works for positive or negative numbers.
  //
  //   Values that are halfway between two integers may be rounded up or
  //   down, for example Round<int>(0.5) == 0 and Round<int>(1.5) == 2.
  //   This allows the function to be implemented efficiently on Intel
  //   processors (see the template specializations at the bottom of this
  //   file).  You should not use this function if you care about which
  //   way such half-integers are rounded.
  //
  //   Example usage:
  //     double y, z;
  //     int x = Round<int>(y + 3.7);
  //     int64 b = Round<int64>(0.3 * z);
  //
  //   Note that the floating-point template parameter is typically inferred
  //   from the argument type, i.e. there is no need to specify it explicitly.
  // --------------------------------------------------------------------
  template <class IntOut, class FloatIn>
  static IntOut Round(FloatIn x) {
    static_assert(!std::is_integral<FloatIn>::value, "FloatIn is integer");
    static_assert(std::is_integral<IntOut>::value, "IntOut is not integer");

    // We don't use sgn(x) below because there is no need to distinguish the
    // (x == 0) case.  Also note that there are specialized faster versions
    // of this function for Intel processors at the bottom of this file.
    return static_cast<IntOut>(x < 0 ? (x - 0.5) : (x + 0.5));
  }

  // --------------------------------------------------------------------
  // FastIntRound, FastInt64Round
  //   Fast routines for converting floating-point numbers to integers.
  //
  //   These routines are approximately 6 times faster than the default
  //   implementation of Round<int> on Intel processors (12 times faster on
  //   the Pentium 3).  They are also more than 5 times faster than simply
  //   casting a "double" to an "int" using static_cast<int>.  This is
  //   because casts are defined to truncate towards zero, which on Intel
  //   processors requires changing the rounding mode and flushing the
  //   floating-point pipeline (unless programs are compiled specifically
  //   for the Pentium 4, which has a new instruction to avoid this).
  //
  //   Numbers that are halfway between two integers may be rounded up or
  //   down.  This is because the conversion is done using the default
  //   rounding mode, which rounds towards the closest even number in case
  //   of ties.  So for example, FastIntRound(0.5) == 0, but
  //   FastIntRound(1.5) == 2.  These functions should only be used with
  //   applications that don't care about which way such half-integers are
  //   rounded.
  //
  //   There are template specializations of Round() which call these
  //   functions (for "int" and "int64" only), but it's safer to call them
  //   directly.
  //
  //   This functions are equivalent to lrint() and llrint() as defined in
  //   the ISO C99 standard.  Unfortunately this standard does not seem to
  //   widely adopted yet and these functions are not available by default.
  //   --------------------------------------------------------------------

  static int32 FastIntRound(double x) {
    // This function is not templatized because gcc doesn't seem to be able
    // to deal with inline assembly code in templatized functions, and there
    // is no advantage to passing an argument type of "float" on Intel
    // architectures anyway.

#if defined __GNUC__ && (defined __i386__ || defined __SSE2__)
#if defined __SSE2__
    // SSE2.
    int32 result;
    __asm__ __volatile__
        ("cvtsd2si %1, %0"
         : "=r" (result)    // Output operand is a register
         : "x" (x));        // Input operand is an xmm register
    return result;
#elif defined __i386__
    // FPU stack.  Adapted from /usr/include/bits/mathinline.h.
    int32 result;
    __asm__ __volatile__
        ("fistpl %0"
         : "=m" (result)    // Output operand is a memory location
         : "t" (x)          // Input operand is top of FP stack
         : "st");           // Clobbers (pops) top of FP stack
    return result;
#endif  // if defined __x86_64__ || ...
#else
    return Round<int32, double>(x);
#endif  // if defined __GNUC__ && ...
  }

  static int64 FastInt64Round(double x) {
#if defined __GNUC__ && (defined __i386__ || defined __x86_64__)
#if defined __x86_64__
    // SSE2.
    int64 result;
    __asm__ __volatile__
        ("cvtsd2si %1, %0"
         : "=r" (result)    // Output operand is a register
         : "x" (x));        // Input operand is an xmm register
    return result;
#elif defined __i386__
    // There is no CVTSD2SI in i386 to produce a 64 bit int, even with SSE2.
    // FPU stack.  Adapted from /usr/include/bits/mathinline.h.
    int64 result;
    __asm__ __volatile__
        ("fistpll %0"
         : "=m" (result)    // Output operand is a memory location
         : "t" (x)          // Input operand is top of FP stack
         : "st");           // Clobbers (pops) top of FP stack
    return result;
#endif  // if defined __i386__
#else
    return Round<int64, double>(x);
#endif  // if defined __GNUC__ && ...
  }

  // Computes v^i, where i is a non-negative integer.
  // When T is a floating point type, this has the same semantics as pow(), but
  // is much faster.
  // T can also be any integral type, in which case computations will be
  // performed in the value domain of this integral type, and overflow semantics
  // will be those of T.
  // You can also use any type for which operator*= is defined.
  template <typename T>
  static T IPow(T base, int exp) {
    S2_DCHECK_GE(exp, 0);
    uint32 uexp = static_cast<uint32>(exp);

    if (uexp < 16) {
      T result = (uexp & 1) ? base : static_cast<T>(1);
      if (uexp >= 2) {
        base *= base;
        if (uexp & 2) {
          result *= base;
        }
        if (uexp >= 4) {
          base *= base;
          if (uexp & 4) {
            result *= base;
          }
          if (uexp >= 8) {
            base *= base;
            result *= base;
          }
        }
      }
      return result;
    }

    T result = base;
    int count = 31 ^ Bits::Log2FloorNonZero(uexp);

    uexp <<= count;
    count ^= 31;

    while (count--) {
      uexp <<= 1;
      result *= result;
      if (uexp >= 0x80000000) {
        result *= base;
      }
    }

    return result;
  }
};

// ========================================================================= //

#if (defined __i386__ || defined __x86_64__) && defined __GNUC__

// We define template specializations of Round() to get the more efficient
// Intel versions when possible.  Note that gcc does not currently support
// partial specialization of templatized functions.

template<>
inline int32 MathUtil::Round<int32, double>(double x) {
  return FastIntRound(x);
}

template<>
inline int32 MathUtil::Round<int32, float>(float x) {
  return FastIntRound(x);
}

template<>
inline int64 MathUtil::Round<int64, double>(double x) {
  return FastInt64Round(x);
}

template<>
inline int64 MathUtil::Round<int64, float>(float x) {
  return FastInt64Round(x);
}

#endif

#endif  // S2_UTIL_MATH_MATHUTIL_H_

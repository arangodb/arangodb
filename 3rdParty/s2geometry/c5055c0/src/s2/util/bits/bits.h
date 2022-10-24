// Copyright 2002 Google Inc. All Rights Reserved.
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

#ifndef S2_UTIL_BITS_BITS_H_
#define S2_UTIL_BITS_BITS_H_

//
// Various bit-twiddling functions, all of which are static members of the Bits
// class (making it effectively a namespace). Operands are unsigned integers.
// Munging bits in _signed_ integers is fraught with peril! For example,
// -5 << n has undefined behavior (for some values of n).
//
// Bits provide the following:
//
//   * Count(Ones.*|LeadingZeros.*)? . In a similar vein, there's also the
//     Find[LM]SBSetNonZero.* family of functions. You can think of them as
//     (trailing|leading) zero bit count + 1. Also in a similar vein,
//     (Capped)?Difference, which count the number of one bits in foo ^ bar.
//
//   * ReverseBits${power_of_two}
//
//   * Log2(Floor|Ceiling)(NonZero)?.* - The NonZero variants have undefined
//     behavior if argument is 0.
//
//   * Bytes(ContainByte(LessThan)?|AllInRange) - These scan a sequence of bytes
//     looking for one with(out)? some property.
//
//   * (Get|Set|Copy)Bits
//
//   * GetLowBits - Extract N lowest bits from value.
//
// The only other thing is BitPattern, which is a trait class template (not in
// Bits) containing a few bit patterns (which vary based on value of template
// parameter).

#include <cstdint>

#include "absl/base/casts.h"
#include "absl/base/macros.h"
#include "absl/meta/type_traits.h"
#include "absl/numeric/bits.h"
#include "absl/numeric/int128.h"
#if defined(__i386__) || defined(__x86_64__)
#include <x86intrin.h>
#endif
#if defined(_MSC_VER)
#include <intrin.h>
#endif

// TODO: Remove this and just use ABSL_ASSUME when it is in an LTS release.
#if defined(ABSL_ASSUME)
#define S2_ASSUME(cond) ABSL_ASSUME(cond)
#elif defined(ABSL_INTERNAL_ASSUME)
#define S2_ASSUME(cond) ABSL_INTERNAL_ASSUME(cond)
#else
#error "abseil-cpp must provide ABSL_ASSUME or ABSL_INTERNAL_ASSUME, what version are you using?"
#endif

#include <type_traits>

#include "s2/base/integral_types.h"
#include "s2/base/logging.h"
#include "s2/base/port.h"

class Bits {
 public:
  // A traits class template for unsigned integer type sizes. Primary
  // information contained herein is corresponding (unsigned) integer type.
  // E.g. UnsignedTypeBySize<4>::Type is uint32. Used by UnsignedType.
  template <int size /* in bytes */>
  struct UnsignedTypeBySize;

  // Auxiliary struct for figuring out an unsigned type for a given type.
  template <typename T>
  struct UnsignedType {
    typedef typename UnsignedTypeBySize<sizeof(T)>::Type Type;
  };

  template <typename T,
            absl::enable_if_t<std::is_unsigned<absl::remove_cv_t<
                                  absl::remove_reference_t<T>>>::value,
                              int> = 0>
  ABSL_DEPRECATED("Inline the implementation")
  static int CountOnes(T &&n) {
    return absl::popcount(n);
  }

  template <typename T,
            absl::enable_if_t<!std::is_unsigned<absl::remove_cv_t<
                                  absl::remove_reference_t<T>>>::value,
                              int> = 0>
  ABSL_DEPRECATED("Inline the implementation")
  static int CountOnes(T &&n) {
    return absl::popcount(static_cast<uint32>(n));
  }

  template <typename T,
            absl::enable_if_t<std::is_unsigned<absl::remove_cv_t<
                                  absl::remove_reference_t<T>>>::value,
                              int> = 0>
  ABSL_DEPRECATED("Inline the implementation")
  static inline int CountOnes64(T &&n) {
    return absl::popcount(n);
  }

  template <typename T,
            absl::enable_if_t<!std::is_unsigned<absl::remove_cv_t<
                                  absl::remove_reference_t<T>>>::value,
                              int> = 0>
  ABSL_DEPRECATED("Inline the implementation")
  static inline int CountOnes64(T &&n) {
    return absl::popcount(static_cast<uint64>(n));
  }

  // Count bits in uint128
  static inline int CountOnes128(absl::uint128 n) {
    return absl::popcount(absl::Uint128High64(n)) +
           absl::popcount(absl::Uint128Low64(n));
  }

  // Count leading zeroes.  This is similar to wordsize - 1 - floor(log2(n)).
  // Returns number of bits if n is 0.
  static inline int CountLeadingZeros128(absl::uint128 n) {
    if (uint64 hi = absl::Uint128High64(n)) return absl::countl_zero(hi);
    return absl::countl_zero(absl::Uint128Low64(n)) + 64;
  }

  // Reverse the bits in the given integer.
  static uint8 ReverseBits8(uint8 n);
  static uint16 ReverseBits16(uint16 n);
  static uint32 ReverseBits32(uint32 n);
  static uint64 ReverseBits64(uint64 n);
  static absl::uint128 ReverseBits128(absl::uint128 n);

  // Return the number of one bits in the byte sequence.
  static int Count(const void *m, int num_bytes);

  // Return the number of different bits in the given byte sequences.
  // (i.e., the Hamming distance)
  static int Difference(const void *m1, const void *m2, int num_bytes);

  // Return the number of different bits in the given byte sequences,
  // up to a maximum.  Values larger than the maximum may be returned
  // (because multiple bits are checked at a time), but the function
  // may exit early if the cap is exceeded.
  static int CappedDifference(const void *m1, const void *m2, int num_bytes,
                              int cap);

  // Return floor(log2(n)) for positive integer n.  Returns -1 iff n == 0.

  template <typename T,
            absl::enable_if_t<std::is_unsigned<absl::remove_cv_t<
                                  absl::remove_reference_t<T>>>::value,
                              int> = 0>
  ABSL_DEPRECATED("Inline the implementation")
  static int Log2Floor(T &&n);

  template <typename T,
            absl::enable_if_t<!std::is_unsigned<absl::remove_cv_t<
                                  absl::remove_reference_t<T>>>::value,
                              int> = 0>
  ABSL_DEPRECATED("Inline the implementation")
  static int Log2Floor(T &&n);

  template <typename T,
            absl::enable_if_t<std::is_unsigned<absl::remove_cv_t<
                                  absl::remove_reference_t<T>>>::value,
                              int> = 0>
  ABSL_DEPRECATED("Inline the implementation")
  static int Log2Floor64(T &&n);

  template <typename T,
            absl::enable_if_t<!std::is_unsigned<absl::remove_cv_t<
                                  absl::remove_reference_t<T>>>::value,
                              int> = 0>
  ABSL_DEPRECATED("Inline the implementation")
  static int Log2Floor64(T &&n);

  static int Log2Floor128(absl::uint128 n);

  // Potentially faster version of Log2Floor() that returns an
  // undefined value if n == 0
  static int Log2FloorNonZero(uint32 n);
  static int Log2FloorNonZero64(uint64 n);
  static int Log2FloorNonZero128(absl::uint128 n);

  // Return ceiling(log2(n)) for positive integer n.  Returns -1 iff n == 0.
  static int Log2Ceiling(uint32 n);
  static int Log2Ceiling64(uint64 n);
  static int Log2Ceiling128(absl::uint128 n);

  // Returns true if and only if n is a power of two.
  template <typename IntType,
            absl::enable_if_t<std::is_unsigned<IntType>::value, int> = 0>
  ABSL_DEPRECATED("Inline the implementation")
  static constexpr bool IsPowerOfTwo(IntType n) {
    return absl::has_single_bit(n);
  }

  template <typename IntType,
            absl::enable_if_t<!std::is_unsigned<IntType>::value, int> = 0>
  static constexpr bool IsPowerOfTwo(IntType n) {
    static_assert(std::is_integral<IntType>::value, "");
    return n > 0 && (n & (n - 1)) == 0;
  }

  // Return the first set least / most significant bit, 0-indexed.  Returns an
  // undefined value if n == 0.  FindLSBSetNonZero() is similar to ffs() except
  // that it's 0-indexed, while FindMSBSetNonZero() is the same as
  // Log2FloorNonZero().
  static int FindLSBSetNonZero(uint32 n);
  static int FindLSBSetNonZero64(uint64 n);
  static int FindLSBSetNonZero128(absl::uint128 n);
  static int FindMSBSetNonZero(uint32 n) { return Log2FloorNonZero(n); }
  static int FindMSBSetNonZero64(uint64 n) { return Log2FloorNonZero64(n); }
  static int FindMSBSetNonZero128(absl::uint128 n) {
    return Log2FloorNonZero128(n);
  }

  // Viewing bytes as a stream of unsigned bytes, does that stream
  // contain any byte equal to c?
  template <class T>
  static bool BytesContainByte(T bytes, uint8 c);

  // Viewing bytes as a stream of unsigned bytes, does that stream
  // contain any byte b < c?
  template <class T>
  static bool BytesContainByteLessThan(T bytes, uint8 c);

  // Viewing bytes as a stream of unsigned bytes, are all elements of that
  // stream in [lo, hi]?
  template <class T>
  static bool BytesAllInRange(T bytes, uint8 lo, uint8 hi);

  // Extract 'nbits' consecutive bits from 'src'.  Position of bits are
  // specified by 'offset' from the LSB.  'T' is a scalar type (integral,
  // float or pointer) whose size is the same as one of the unsigned types.
  // The return type is an unsigned type having the same size as T.
  template <typename T>
  static typename UnsignedType<T>::Type GetBits(const T src, const int offset,
                                                const int nbits) {
    typedef typename UnsignedType<T>::Type UnsignedT;
    const UnsignedT unsigned_src = absl::bit_cast<UnsignedT>(src);
    S2_DCHECK_GT(sizeof(UnsignedT) * 8, offset);
    S2_DCHECK_GE(sizeof(UnsignedT) * 8, offset + nbits);
    return GetBitsImpl(unsigned_src, offset, nbits);
  }

  // Overwrite 'nbits' consecutive bits of 'dest.'.  Position of bits are
  // specified by an offset from the LSB.  'T' is a scalar type (integral,
  // float or pointer) whose size is the same as one of the unsigned types.
  template <typename T>
  static void SetBits(const typename UnsignedType<T>::Type value,
                      const int offset, const int nbits, T *const dest) {
    typedef typename UnsignedType<T>::Type UnsignedT;
    const UnsignedT unsigned_dest = absl::bit_cast<UnsignedT>(*dest);
    S2_DCHECK_GT(sizeof(UnsignedT) * 8, offset);
    S2_DCHECK_GE(sizeof(UnsignedT) * 8, offset + nbits);
    const UnsignedT mask = NBitsFromLSB<UnsignedT>(nbits);
    const UnsignedT unsigned_result =
        (unsigned_dest & ~(mask << offset)) | ((value & mask) << offset);
    *dest = absl::bit_cast<T>(unsigned_result);
  }

  // Combine SetBits and GetBits for convenience.  This is meant to be a
  // replacement for BitCopy() for some use cases.  Unlike BitCopy(),
  // Bits::CopyBits() operating on multibyte types has the same behavior on
  // big-endian and little-endian machines. Sample usage:
  //
  // uint32 a, b;
  // Bits::CopyBits(&a, 0, b, 12, 3);
  template <typename DestType, typename SrcType>
  static void CopyBits(DestType *const dest, const int dest_offset,
                       const SrcType src, const int src_offset,
                       const int nbits) {
    const typename UnsignedType<SrcType>::Type value =
        GetBits(src, src_offset, nbits);
    SetBits(value, dest_offset, nbits, dest);
  }

  // Extract the lowest 'nbits' consecutive bits from 'src'.
  // Bits::GetLowBits(13, 3); /* = 5 (0b1101 => 0b101) */
  template <typename T>
  static typename UnsignedType<T>::Type GetLowBits(const T src,
                                                   const int nbits) {
    typedef typename UnsignedType<T>::Type UnsignedT;
    const UnsignedT unsigned_src = absl::bit_cast<UnsignedT>(src);
    S2_DCHECK_GE(sizeof(UnsignedT) * 8, nbits);
    return GetLowBitsImpl(unsigned_src, nbits);
  }

 private:
  // We only use this for unsigned types and for 0 <= n <= sizeof(UnsignedT).
  template <typename UnsignedT>
  static UnsignedT NBitsFromLSB(const int nbits) {
    const UnsignedT all_ones = ~static_cast<UnsignedT>(0);
    return nbits == 0 ? static_cast<UnsignedT>(0)
                      : all_ones >> (sizeof(UnsignedT) * 8 - nbits);
  }

  template <typename UnsignedT>
  static inline UnsignedT GetBitsImpl(const UnsignedT src, const int offset,
                                      const int nbits);
  template <typename UnsignedT>
  static inline UnsignedT GetLowBitsImpl(const UnsignedT src, const int nbits);

#ifdef __GNUC__
#if defined(__BMI__) && (defined(__i386__) || defined(__x86_64__))
  static inline uint32 GetBitsImpl(const uint32 src, const int offset,
                                   const int nbits);
#endif
#if defined(__BMI__) && defined(__x86_64__)
  static inline uint64 GetBitsImpl(const uint64 src, const int offset,
                                   const int nbits);
#endif
#if defined(__BMI2__) && (defined(__i386__) || defined(__x86_64__))
  static inline uint32 GetLowBitsImpl(const uint32 src, const int nbits);
#endif
#if defined(__BMI2__) && defined(__x86_64__)
  static inline uint64 GetLowBitsImpl(const uint64 src, const int nbits);
#endif
#endif  // __GNUC__

  Bits(Bits const &) = delete;
  void operator=(Bits const &) = delete;
};

// A utility class for some handy bit patterns.  The names l and h
// were chosen to match Knuth Volume 4: l is 0x010101... and h is 0x808080...;
// half_ones is ones in the lower half only.  We assume sizeof(T) is 1 or even.
template <class T>
struct BitPattern {
  typedef typename std::make_unsigned<T>::type U;
  static constexpr U half_ones = (static_cast<U>(1) << (sizeof(U) * 4)) - 1;
  static constexpr U l =
      (sizeof(U) == 1) ? 1 : (half_ones / 0xff * (half_ones + 2));
  static constexpr U h = ~(l * 0x7f);
};

// ------------------------------------------------------------------------
// Implementation details follow
// ------------------------------------------------------------------------

template <
    typename T,
    absl::enable_if_t<
        std::is_unsigned<absl::remove_cv_t<absl::remove_reference_t<T>>>::value,
        int>>
inline int Bits::Log2Floor(T &&n) {
  return absl::bit_width(n) - 1;
}

template <typename T,
          absl::enable_if_t<!std::is_unsigned<absl::remove_cv_t<
                                absl::remove_reference_t<T>>>::value,
                            int>>
inline int Bits::Log2Floor(T &&n) {
  return absl::bit_width(static_cast<uint32>(n)) - 1;
}

inline int Bits::Log2FloorNonZero(uint32 n) {
  S2_ASSUME(n != 0);
  return absl::bit_width(n) - 1;
}

template <
    typename T,
    absl::enable_if_t<
        std::is_unsigned<absl::remove_cv_t<absl::remove_reference_t<T>>>::value,
        int>>
inline int Bits::Log2Floor64(T &&n) {
  return absl::bit_width(n) - 1;
}

template <typename T,
          absl::enable_if_t<!std::is_unsigned<absl::remove_cv_t<
                                absl::remove_reference_t<T>>>::value,
                            int>>
inline int Bits::Log2Floor64(T &&n) {
  return absl::bit_width(static_cast<uint64>(n)) - 1;
}

inline int Bits::Log2FloorNonZero64(uint64 n) {
  S2_ASSUME(n != 0);
  return absl::bit_width(n) - 1;
}

inline int Bits::FindLSBSetNonZero(uint32 n) {
  S2_ASSUME(n != 0);
  return absl::countr_zero(n);
}

inline int Bits::FindLSBSetNonZero64(uint64 n) {
  S2_ASSUME(n != 0);
  return absl::countr_zero(n);
}

inline int Bits::Log2Floor128(absl::uint128 n) {
  if (uint64 hi = absl::Uint128High64(n)) return 64 + Log2FloorNonZero64(hi);
  return (absl::bit_width(absl::Uint128Low64(n)) - 1);
}

inline int Bits::Log2FloorNonZero128(absl::uint128 n) {
  if (uint64 hi = absl::Uint128High64(n)) return 64 + Log2FloorNonZero64(hi);
  return Log2FloorNonZero64(absl::Uint128Low64(n));
}

inline int Bits::FindLSBSetNonZero128(absl::uint128 n) {
  if (uint64 lo = absl::Uint128Low64(n)) return Bits::FindLSBSetNonZero64(lo);
  return 64 + Bits::FindLSBSetNonZero64(absl::Uint128High64(n));
}

inline uint8 Bits::ReverseBits8(unsigned char n) {
#if defined(__aarch64__) && defined(__GNUC__)
  // aarch64 has a reverse bits instruction but there is no gcc builtin.
  uint32 result;
  const uint32 n_shifted = static_cast<uint32>(n) << 24;
  asm("rbit %w0, %w1" : "=r"(result) : "r"(n_shifted));
  return static_cast<uint8>(result);
#elif defined(__powerpc64__)
  uint64 temp = n;
  // bpermd selects a byte's worth of bits from its second input. Grab one byte
  // at a time, in reversed order. 0x3f is the lowest order bit of a 64-bit int.
  // Bits 0x0 through 0x37 will all be zero, and bits 0x38 through 0x3f will
  // hold the 8 bits from `n`.
  uint64 result = __builtin_bpermd(0x3f3e3d3c3b3a3938, temp);
  return static_cast<unsigned char>(result);
#else
  n = static_cast<unsigned char>(((n >> 1) & 0x55) | ((n & 0x55) << 1));
  n = static_cast<unsigned char>(((n >> 2) & 0x33) | ((n & 0x33) << 2));
  return static_cast<unsigned char>(((n >> 4) & 0x0f) | ((n & 0x0f) << 4));
#endif
}

inline uint16 Bits::ReverseBits16(uint16 n) {
#if defined(__aarch64__) && defined(__GNUC__)
  uint32 result;
  const uint32 n_shifted = static_cast<uint32>(n) << 16;
  result = ReverseBits32(n_shifted);
  return static_cast<uint16>(result);
#elif defined(__powerpc64__)
  uint64 temp = n;
  uint64 result_0 = __builtin_bpermd(0x3f3e3d3c3b3a3938, temp) << 8;
  uint64 result_1 = __builtin_bpermd(0x3736353433323130, temp);
  return static_cast<uint16>(result_0 | result_1);
#else
  n = static_cast<uint16>(((n >> 1) & 0x5555) | ((n & 0x5555) << 1));
  n = static_cast<uint16>(((n >> 2) & 0x3333) | ((n & 0x3333) << 2));
  n = static_cast<uint16>(((n >> 4) & 0x0f0f) | ((n & 0x0f0f) << 4));
  return bswap_16(n);
#endif
}

inline uint32 Bits::ReverseBits32(uint32 n) {
#if defined(__aarch64__) && defined(__GNUC__)
  uint32 result;
  asm("rbit %w0, %w1" : "=r"(result) : "r"(n));
  return result;
#elif defined(__powerpc64__)
  uint64 temp = n;
  uint64 result_0 = __builtin_bpermd(0x3f3e3d3c3b3a3938, temp) << 24;
  uint64 result_1 = __builtin_bpermd(0x3736353433323130, temp) << 16;
  uint64 result_2 = __builtin_bpermd(0x2f2e2d2c2b2a2928, temp) << 8;
  uint64 result_3 = __builtin_bpermd(0x2726252423222120, temp);
  return static_cast<uint32>(result_0 | result_1 | result_2 | result_3);
#else
  n = ((n >> 1) & 0x55555555) | ((n & 0x55555555) << 1);
  n = ((n >> 2) & 0x33333333) | ((n & 0x33333333) << 2);
  n = ((n >> 4) & 0x0F0F0F0F) | ((n & 0x0F0F0F0F) << 4);
  return bswap_32(n);
#endif
}

inline uint64 Bits::ReverseBits64(uint64 n) {
#if defined(__aarch64__) && defined(__GNUC__)
  uint64 result;
  asm("rbit %0, %1" : "=r"(result) : "r"(n));
  return result;
#elif defined(__powerpc64__)
  uint64 result_lo0 = __builtin_bpermd(0x3f3e3d3c3b3a3938, n) << 56;
  uint64 result_lo1 = __builtin_bpermd(0x3736353433323130, n) << 48;
  uint64 result_lo2 = __builtin_bpermd(0x2f2e2d2c2b2a2928, n) << 40;
  uint64 result_lo3 = __builtin_bpermd(0x2726252423222120, n) << 32;
  uint64 result_hi0 = __builtin_bpermd(0x1f1e1d1c1b1a1918, n) << 24;
  uint64 result_hi1 = __builtin_bpermd(0x1716151413121110, n) << 16;
  uint64 result_hi2 = __builtin_bpermd(0x0f0e0d0c0b0a0908, n) << 8;
  uint64 result_hi3 = __builtin_bpermd(0x0706050403020100, n);
  return (result_lo0 | result_lo1 | result_lo2 | result_lo3 | result_hi0 |
          result_hi1 | result_hi2 | result_hi3);
#elif defined(_LP64)
  n = ((n >> 1) & 0x5555555555555555ULL) | ((n & 0x5555555555555555ULL) << 1);
  n = ((n >> 2) & 0x3333333333333333ULL) | ((n & 0x3333333333333333ULL) << 2);
  n = ((n >> 4) & 0x0F0F0F0F0F0F0F0FULL) | ((n & 0x0F0F0F0F0F0F0F0FULL) << 4);
  return bswap_64(n);
#else
  return ReverseBits32(n >> 32) |
         (static_cast<uint64>(ReverseBits32(n & 0xffffffff)) << 32);
#endif
}

inline absl::uint128 Bits::ReverseBits128(absl::uint128 n) {
  return absl::MakeUint128(ReverseBits64(absl::Uint128Low64(n)),
                           ReverseBits64(absl::Uint128High64(n)));
}

template <class T>
inline bool Bits::BytesContainByteLessThan(T bytes, uint8 c) {
  auto l = BitPattern<T>::l;
  auto h = BitPattern<T>::h;
  // The c <= 0x80 code is straight out of Knuth Volume 4.
  // Usually c will be manifestly constant.
  return c <= 0x80 ? ((h & (bytes - l * c) & ~bytes) != 0)
                   : ((((bytes - l * c) | (bytes ^ h)) & h) != 0);
}

template <class T>
inline bool Bits::BytesContainByte(T bytes, uint8 c) {
  // Usually c will be manifestly constant.
  return Bits::BytesContainByteLessThan<T>(bytes ^ (c * BitPattern<T>::l), 1);
}

template <class T>
inline bool Bits::BytesAllInRange(T bytes, uint8 lo, uint8 hi) {
  auto l = BitPattern<T>::l;
  auto h = BitPattern<T>::h;
  // In the common case, lo and hi are manifest constants.
  if (lo > hi) {
    return false;
  }
  if (hi - lo < 128) {
    auto x = bytes - l * lo;
    auto y = bytes + l * (127 - hi);
    return ((x | y) & h) == 0;
  }
  return !Bits::BytesContainByteLessThan(bytes + (255 - hi) * l,
                                         lo + (255 - hi));
}

// Specializations for Bits::UnsignedTypeBySize.  For unsupported type
// sizes, a compile-time error will be generated.
template <>
struct Bits::UnsignedTypeBySize<1> {
  typedef uint8 Type;
};

template <>
struct Bits::UnsignedTypeBySize<2> {
  typedef uint16 Type;
};

template <>
struct Bits::UnsignedTypeBySize<4> {
  typedef uint32 Type;
};

template <>
struct Bits::UnsignedTypeBySize<8> {
  typedef uint64 Type;
};

template <>
struct Bits::UnsignedTypeBySize<16> {
  typedef absl::uint128 Type;
};

#ifdef __GNUC__
#if defined(__BMI__) && (defined(__i386__) || defined(__x86_64__))
inline uint32 Bits::GetBitsImpl(const uint32 src, const int offset,
                                const int nbits) {
  return _bextr_u32(src, offset, nbits);
}
#endif

#if defined(__BMI__) && defined(__x86_64__)
inline uint64 Bits::GetBitsImpl(const uint64 src, const int offset,
                                const int nbits) {
  return _bextr_u64(src, offset, nbits);
}
#endif

#if defined(__BMI2__) && (defined(__i386__) || defined(__x86_64__))
inline uint32 Bits::GetLowBitsImpl(const uint32 src, const int nbits) {
  return _bzhi_u32(src, nbits);
}
#endif

#if defined(__BMI2__) && defined(__x86_64__)
inline uint64 Bits::GetLowBitsImpl(const uint64 src, const int nbits) {
  return _bzhi_u64(src, nbits);
}
#endif

#endif  // __GNUC__

template <typename UnsignedT>
inline UnsignedT Bits::GetBitsImpl(const UnsignedT src, const int offset,
                                   const int nbits) {
  const UnsignedT result = (src >> offset) & NBitsFromLSB<UnsignedT>(nbits);
  return result;
}

template <typename UnsignedT>
inline UnsignedT Bits::GetLowBitsImpl(const UnsignedT src, const int nbits) {
  return GetBitsImpl(src, 0, nbits);
}

#endif  // S2_UTIL_BITS_BITS_H_

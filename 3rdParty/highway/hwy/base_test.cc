// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <limits>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "base_test.cc"
#include "hwy/foreach_target.h"  // IWYU pragma: keep
#include "hwy/highway.h"
#include "hwy/tests/test_util-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

HWY_NOINLINE void TestAllLimits() {
  HWY_ASSERT_EQ(uint8_t{0}, LimitsMin<uint8_t>());
  HWY_ASSERT_EQ(uint16_t{0}, LimitsMin<uint16_t>());
  HWY_ASSERT_EQ(uint32_t{0}, LimitsMin<uint32_t>());
  HWY_ASSERT_EQ(uint64_t{0}, LimitsMin<uint64_t>());

  HWY_ASSERT_EQ(int8_t{-128}, LimitsMin<int8_t>());
  HWY_ASSERT_EQ(int16_t{-32768}, LimitsMin<int16_t>());
  HWY_ASSERT_EQ(static_cast<int32_t>(0x80000000u), LimitsMin<int32_t>());
  HWY_ASSERT_EQ(static_cast<int64_t>(0x8000000000000000ull),
                LimitsMin<int64_t>());

  HWY_ASSERT_EQ(uint8_t{0xFF}, LimitsMax<uint8_t>());
  HWY_ASSERT_EQ(uint16_t{0xFFFF}, LimitsMax<uint16_t>());
  HWY_ASSERT_EQ(uint32_t{0xFFFFFFFFu}, LimitsMax<uint32_t>());
  HWY_ASSERT_EQ(uint64_t{0xFFFFFFFFFFFFFFFFull}, LimitsMax<uint64_t>());

  HWY_ASSERT_EQ(int8_t{0x7F}, LimitsMax<int8_t>());
  HWY_ASSERT_EQ(int16_t{0x7FFF}, LimitsMax<int16_t>());
  HWY_ASSERT_EQ(int32_t{0x7FFFFFFFu}, LimitsMax<int32_t>());
  HWY_ASSERT_EQ(int64_t{0x7FFFFFFFFFFFFFFFull}, LimitsMax<int64_t>());
}

struct TestLowestHighest {
  template <class T>
  HWY_NOINLINE void operator()(T /*unused*/) const {
    HWY_ASSERT_EQ(std::numeric_limits<T>::lowest(), LowestValue<T>());
    HWY_ASSERT_EQ(std::numeric_limits<T>::max(), HighestValue<T>());
  }
};

HWY_NOINLINE void TestAllLowestHighest() { ForAllTypes(TestLowestHighest()); }
struct TestIsUnsigned {
  template <class T>
  HWY_NOINLINE void operator()(T /*unused*/) const {
    static_assert(!IsFloat<T>(), "Expected !IsFloat");
    static_assert(!IsSigned<T>(), "Expected !IsSigned");
  }
};

struct TestIsSigned {
  template <class T>
  HWY_NOINLINE void operator()(T /*unused*/) const {
    static_assert(!IsFloat<T>(), "Expected !IsFloat");
    static_assert(IsSigned<T>(), "Expected IsSigned");
  }
};

struct TestIsFloat {
  template <class T>
  HWY_NOINLINE void operator()(T /*unused*/) const {
    static_assert(IsFloat<T>(), "Expected IsFloat");
    static_assert(IsSigned<T>(), "Floats are also considered signed");
  }
};

HWY_NOINLINE void TestAllType() {
  ForUnsignedTypes(TestIsUnsigned());
  ForSignedTypes(TestIsSigned());
  ForFloatTypes(TestIsFloat());

  static_assert(sizeof(MakeUnsigned<hwy::uint128_t>) == 16, "");
  static_assert(sizeof(MakeWide<uint64_t>) == 16, "Expected uint128_t");
  static_assert(sizeof(MakeNarrow<hwy::uint128_t>) == 8, "Expected uint64_t");
}

struct TestIsSame {
  template <class T>
  HWY_NOINLINE void operator()(T /*unused*/) const {
    static_assert(IsSame<T, T>(), "T == T");
    static_assert(!IsSame<MakeSigned<T>, MakeUnsigned<T>>(), "S != U");
    static_assert(!IsSame<MakeUnsigned<T>, MakeSigned<T>>(), "U != S");
  }
};

HWY_NOINLINE void TestAllIsSame() { ForAllTypes(TestIsSame()); }

HWY_NOINLINE void TestAllBitScan() {
  HWY_ASSERT_EQ(size_t{0}, Num0BitsAboveMS1Bit_Nonzero32(0x80000000u));
  HWY_ASSERT_EQ(size_t{0}, Num0BitsAboveMS1Bit_Nonzero32(0xFFFFFFFFu));
  HWY_ASSERT_EQ(size_t{1}, Num0BitsAboveMS1Bit_Nonzero32(0x40000000u));
  HWY_ASSERT_EQ(size_t{1}, Num0BitsAboveMS1Bit_Nonzero32(0x40108210u));
  HWY_ASSERT_EQ(size_t{30}, Num0BitsAboveMS1Bit_Nonzero32(2u));
  HWY_ASSERT_EQ(size_t{30}, Num0BitsAboveMS1Bit_Nonzero32(3u));
  HWY_ASSERT_EQ(size_t{31}, Num0BitsAboveMS1Bit_Nonzero32(1u));

  HWY_ASSERT_EQ(size_t{0},
                Num0BitsAboveMS1Bit_Nonzero64(0x8000000000000000ull));
  HWY_ASSERT_EQ(size_t{0},
                Num0BitsAboveMS1Bit_Nonzero64(0xFFFFFFFFFFFFFFFFull));
  HWY_ASSERT_EQ(size_t{1},
                Num0BitsAboveMS1Bit_Nonzero64(0x4000000000000000ull));
  HWY_ASSERT_EQ(size_t{1},
                Num0BitsAboveMS1Bit_Nonzero64(0x4010821004200011ull));
  HWY_ASSERT_EQ(size_t{62}, Num0BitsAboveMS1Bit_Nonzero64(2ull));
  HWY_ASSERT_EQ(size_t{62}, Num0BitsAboveMS1Bit_Nonzero64(3ull));
  HWY_ASSERT_EQ(size_t{63}, Num0BitsAboveMS1Bit_Nonzero64(1ull));

  HWY_ASSERT_EQ(size_t{0}, Num0BitsBelowLS1Bit_Nonzero32(1u));
  HWY_ASSERT_EQ(size_t{1}, Num0BitsBelowLS1Bit_Nonzero32(2u));
  HWY_ASSERT_EQ(size_t{30}, Num0BitsBelowLS1Bit_Nonzero32(0xC0000000u));
  HWY_ASSERT_EQ(size_t{31}, Num0BitsBelowLS1Bit_Nonzero32(0x80000000u));

  HWY_ASSERT_EQ(size_t{0}, Num0BitsBelowLS1Bit_Nonzero64(1ull));
  HWY_ASSERT_EQ(size_t{1}, Num0BitsBelowLS1Bit_Nonzero64(2ull));
  HWY_ASSERT_EQ(size_t{62},
                Num0BitsBelowLS1Bit_Nonzero64(0xC000000000000000ull));
  HWY_ASSERT_EQ(size_t{63},
                Num0BitsBelowLS1Bit_Nonzero64(0x8000000000000000ull));
}

HWY_NOINLINE void TestAllPopCount() {
  HWY_ASSERT_EQ(size_t{0}, PopCount(0u));
  HWY_ASSERT_EQ(size_t{1}, PopCount(1u));
  HWY_ASSERT_EQ(size_t{1}, PopCount(2u));
  HWY_ASSERT_EQ(size_t{2}, PopCount(3u));
  HWY_ASSERT_EQ(size_t{1}, PopCount(0x80000000u));
  HWY_ASSERT_EQ(size_t{31}, PopCount(0x7FFFFFFFu));
  HWY_ASSERT_EQ(size_t{32}, PopCount(0xFFFFFFFFu));

  HWY_ASSERT_EQ(size_t{1}, PopCount(0x80000000ull));
  HWY_ASSERT_EQ(size_t{31}, PopCount(0x7FFFFFFFull));
  HWY_ASSERT_EQ(size_t{32}, PopCount(0xFFFFFFFFull));
  HWY_ASSERT_EQ(size_t{33}, PopCount(0x10FFFFFFFFull));
  HWY_ASSERT_EQ(size_t{63}, PopCount(0xFFFEFFFFFFFFFFFFull));
  HWY_ASSERT_EQ(size_t{64}, PopCount(0xFFFFFFFFFFFFFFFFull));
}

template <class T>
static HWY_INLINE T TestEndianGetIntegerVal(T val) {
  static_assert(!IsFloat<T>() && !IsSpecialFloat<T>(),
                "T must not be a floating-point type");
  using TU = MakeUnsigned<T>;
  static_assert(sizeof(T) == sizeof(TU),
                "sizeof(T) == sizeof(TU) must be true");

  uint8_t result_bytes[sizeof(T)];
  const TU val_u = static_cast<TU>(val);

  for (size_t i = 0; i < sizeof(T); i++) {
#if HWY_IS_BIG_ENDIAN
    const size_t shift_amt = (sizeof(T) - 1 - i) * 8;
#else
    const size_t shift_amt = i * 8;
#endif
    result_bytes[i] = static_cast<uint8_t>((val_u >> shift_amt) & 0xFF);
  }

  T result;
  CopyBytes<sizeof(T)>(result_bytes, &result);
  return result;
}

template <class T, class... Bytes>
static HWY_INLINE T TestEndianCreateValueFromBytes(Bytes&&... bytes) {
  static_assert(sizeof(T) > 0, "sizeof(T) > 0 must be true");
  static_assert(sizeof...(Bytes) == sizeof(T),
                "sizeof...(Bytes) == sizeof(T) must be true");

  const uint8_t src_bytes[sizeof(T)]{static_cast<uint8_t>(bytes)...};

  T result;
  CopyBytes<sizeof(T)>(src_bytes, &result);
  return result;
}

#define HWY_TEST_ENDIAN_CHECK_INTEGER_VAL(val) \
  HWY_ASSERT_EQ(val, TestEndianGetIntegerVal(val))

HWY_NOINLINE void TestAllEndian() {
  HWY_TEST_ENDIAN_CHECK_INTEGER_VAL(int8_t{0x01});
  HWY_TEST_ENDIAN_CHECK_INTEGER_VAL(uint8_t{0x01});
  HWY_TEST_ENDIAN_CHECK_INTEGER_VAL(int16_t{0x0102});
  HWY_TEST_ENDIAN_CHECK_INTEGER_VAL(uint16_t{0x0102});
  HWY_TEST_ENDIAN_CHECK_INTEGER_VAL(int32_t{0x01020304});
  HWY_TEST_ENDIAN_CHECK_INTEGER_VAL(uint32_t{0x01020304});
  HWY_TEST_ENDIAN_CHECK_INTEGER_VAL(int64_t{0x0102030405060708});
  HWY_TEST_ENDIAN_CHECK_INTEGER_VAL(uint64_t{0x0102030405060708});

  HWY_TEST_ENDIAN_CHECK_INTEGER_VAL(int16_t{0x0201});
  HWY_TEST_ENDIAN_CHECK_INTEGER_VAL(uint16_t{0x0201});
  HWY_TEST_ENDIAN_CHECK_INTEGER_VAL(int32_t{0x04030201});
  HWY_TEST_ENDIAN_CHECK_INTEGER_VAL(uint32_t{0x04030201});
  HWY_TEST_ENDIAN_CHECK_INTEGER_VAL(int64_t{0x0807060504030201});
  HWY_TEST_ENDIAN_CHECK_INTEGER_VAL(uint64_t{0x0807060504030201});

  HWY_ASSERT_EQ(HWY_IS_BIG_ENDIAN ? int16_t{0x0102} : int16_t{0x0201},
                TestEndianCreateValueFromBytes<int16_t>(0x01, 0x02));
  HWY_ASSERT_EQ(HWY_IS_BIG_ENDIAN ? uint16_t{0x0102} : uint16_t{0x0201},
                TestEndianCreateValueFromBytes<uint16_t>(0x01, 0x02));
  HWY_ASSERT_EQ(
      HWY_IS_BIG_ENDIAN ? int32_t{0x01020304} : int32_t{0x04030201},
      TestEndianCreateValueFromBytes<int32_t>(0x01, 0x02, 0x03, 0x04));
  HWY_ASSERT_EQ(
      HWY_IS_BIG_ENDIAN ? uint32_t{0x01020304} : uint32_t{0x04030201},
      TestEndianCreateValueFromBytes<uint32_t>(0x01, 0x02, 0x03, 0x04));
  HWY_ASSERT_EQ(HWY_IS_BIG_ENDIAN ? int64_t{0x0102030405060708}
                                  : int64_t{0x0807060504030201},
                TestEndianCreateValueFromBytes<int64_t>(
                    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08));
  HWY_ASSERT_EQ(HWY_IS_BIG_ENDIAN ? uint64_t{0x0102030405060708}
                                  : uint64_t{0x0807060504030201},
                TestEndianCreateValueFromBytes<uint64_t>(
                    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08));

  HWY_ASSERT_EQ(HWY_IS_BIG_ENDIAN ? int16_t{-0x5EFE} : int16_t{0x02A1},
                TestEndianCreateValueFromBytes<int16_t>(0xA1, 0x02));
  HWY_ASSERT_EQ(
      HWY_IS_BIG_ENDIAN ? int32_t{-0x5E4D3CFC} : int32_t{0x04C3B2A1},
      TestEndianCreateValueFromBytes<int32_t>(0xA1, 0xB2, 0xC3, 0x04));
  HWY_ASSERT_EQ(HWY_IS_BIG_ENDIAN ? int64_t{-0x6E5D4C3B2A1908F8}
                                  : int64_t{0x08F7E6D5C4B3A291},
                TestEndianCreateValueFromBytes<int64_t>(
                    0x91, 0xA2, 0xB3, 0xC4, 0xD5, 0xE6, 0xF7, 0x08));

  HWY_ASSERT_EQ(HWY_IS_LITTLE_ENDIAN ? int16_t{-0x5DFF} : int16_t{0x01A2},
                TestEndianCreateValueFromBytes<int16_t>(0x01, 0xA2));
  HWY_ASSERT_EQ(
      HWY_IS_LITTLE_ENDIAN ? int32_t{-0x3B4C5DFF} : int32_t{0x01A2B3C4},
      TestEndianCreateValueFromBytes<int32_t>(0x01, 0xA2, 0xB3, 0xC4));
  HWY_ASSERT_EQ(HWY_IS_LITTLE_ENDIAN ? int64_t{-0x0718293A4B5C6DFF}
                                     : int64_t{0x0192A3B4C5D6E7F8},
                TestEndianCreateValueFromBytes<int64_t>(
                    0x01, 0x92, 0xA3, 0xB4, 0xC5, 0xD6, 0xE7, 0xF8));

#if HWY_IS_BIG_ENDIAN
  HWY_ASSERT_EQ(1.0f,
                TestEndianCreateValueFromBytes<float>(0x3F, 0x80, 0x00, 0x00));
  HWY_ASSERT_EQ(15922433.0f,
                TestEndianCreateValueFromBytes<float>(0x4B, 0x72, 0xF5, 0x01));
  HWY_ASSERT_EQ(-12357485.0f,
                TestEndianCreateValueFromBytes<float>(0xCB, 0x3C, 0x8F, 0x6D));
#else
  HWY_ASSERT_EQ(1.0f,
                TestEndianCreateValueFromBytes<float>(0x00, 0x00, 0x80, 0x3F));
  HWY_ASSERT_EQ(15922433.0f,
                TestEndianCreateValueFromBytes<float>(0x01, 0xF5, 0x72, 0x4B));
  HWY_ASSERT_EQ(-12357485.0f,
                TestEndianCreateValueFromBytes<float>(0x6D, 0x8F, 0x3C, 0xCB));
#endif

#if HWY_HAVE_FLOAT64
#if HWY_IS_BIG_ENDIAN
  HWY_ASSERT_EQ(1.0, TestEndianCreateValueFromBytes<double>(
                         0x3F, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00));
  HWY_ASSERT_EQ(8707235690688195.0,
                TestEndianCreateValueFromBytes<double>(0x43, 0x3E, 0xEF, 0x2F,
                                                       0x4A, 0x51, 0xAE, 0xC3));
  HWY_ASSERT_EQ(-6815854340348452.0,
                TestEndianCreateValueFromBytes<double>(0xC3, 0x38, 0x36, 0xFB,
                                                       0xC0, 0xCC, 0x1A, 0x24));
#else
  HWY_ASSERT_EQ(1.0, TestEndianCreateValueFromBytes<double>(
                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x3F));
  HWY_ASSERT_EQ(8707235690688195.0,
                TestEndianCreateValueFromBytes<double>(0xC3, 0xAE, 0x51, 0x4A,
                                                       0x2F, 0xEF, 0x3E, 0x43));
  HWY_ASSERT_EQ(-6815854340348452.0,
                TestEndianCreateValueFromBytes<double>(0x24, 0x1A, 0xCC, 0xC0,
                                                       0xFB, 0x36, 0x38, 0xC3));
#endif  // HWY_IS_BIG_ENDIAN
#endif  // HWY_HAVE_FLOAT64
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace hwy {
HWY_BEFORE_TEST(BaseTest);
HWY_EXPORT_AND_TEST_P(BaseTest, TestAllLimits);
HWY_EXPORT_AND_TEST_P(BaseTest, TestAllLowestHighest);
HWY_EXPORT_AND_TEST_P(BaseTest, TestAllType);
HWY_EXPORT_AND_TEST_P(BaseTest, TestAllIsSame);
HWY_EXPORT_AND_TEST_P(BaseTest, TestAllBitScan);
HWY_EXPORT_AND_TEST_P(BaseTest, TestAllPopCount);
HWY_EXPORT_AND_TEST_P(BaseTest, TestAllEndian);
}  // namespace hwy

#endif

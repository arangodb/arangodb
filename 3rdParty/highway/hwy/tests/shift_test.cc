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

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "tests/shift_test.cc"
#include "hwy/foreach_target.h"  // IWYU pragma: keep
#include "hwy/highway.h"
#include "hwy/tests/test_util-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

template <bool kSigned>
struct TestLeftShifts {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T t, D d) {
    if (kSigned) {
      // Also test positive values
      TestLeftShifts</*kSigned=*/false>()(t, d);
    }

    using TI = MakeSigned<T>;
    using TU = MakeUnsigned<T>;
    const size_t N = Lanes(d);
    auto expected = AllocateAligned<T>(N);
    HWY_ASSERT(expected);

    // Values to shift
    const auto values = Iota(d, static_cast<T>(kSigned ? -TI(N) : TI(0)));
    constexpr size_t kMaxShift = (sizeof(T) * 8) - 1;

    // 0
    HWY_ASSERT_VEC_EQ(d, values, ShiftLeft<0>(values));
    HWY_ASSERT_VEC_EQ(d, values, ShiftLeftSame(values, 0));

    // 1
    for (size_t i = 0; i < N; ++i) {
      const T value = kSigned ? T(T(i) - T(N)) : T(i);
      expected[i] = T(TU(value) << 1);
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), ShiftLeft<1>(values));
    HWY_ASSERT_VEC_EQ(d, expected.get(), ShiftLeftSame(values, 1));

    // max
    for (size_t i = 0; i < N; ++i) {
      const T value = kSigned ? T(T(i) - T(N)) : T(i);
      expected[i] = T(TU(value) << kMaxShift);
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), ShiftLeft<kMaxShift>(values));
    HWY_ASSERT_VEC_EQ(d, expected.get(), ShiftLeftSame(values, kMaxShift));
  }
};

template <bool kSigned>
struct TestVariableLeftShifts {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T t, D d) {
    if (kSigned) {
      // Also test positive values
      TestVariableLeftShifts</*kSigned=*/false>()(t, d);
    }

    using TI = MakeSigned<T>;
    using TU = MakeUnsigned<T>;
    const size_t N = Lanes(d);
    auto expected = AllocateAligned<T>(N);
    HWY_ASSERT(expected);

    const auto v0 = Zero(d);
    const auto v1 = Set(d, 1);
    const auto values = Iota(d, kSigned ? -TI(N) : TI(0));  // value to shift

    constexpr size_t kMaxShift = (sizeof(T) * 8) - 1;
    const auto max_shift = Set(d, kMaxShift);
    const auto small_shifts = And(Iota(d, 0), max_shift);
    const auto large_shifts = max_shift - small_shifts;

    // Same: 0
    HWY_ASSERT_VEC_EQ(d, values, Shl(values, v0));

    // Same: 1
    for (size_t i = 0; i < N; ++i) {
      const T value = kSigned ? T(i) - T(N) : T(i);
      expected[i] = T(TU(value) << 1);
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), Shl(values, v1));

    // Same: max
    for (size_t i = 0; i < N; ++i) {
      const T value = kSigned ? T(i) - T(N) : T(i);
      expected[i] = T(TU(value) << kMaxShift);
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), Shl(values, max_shift));

    // Variable: small
    for (size_t i = 0; i < N; ++i) {
      const T value = kSigned ? T(i) - T(N) : T(i);
      expected[i] = T(TU(value) << (i & kMaxShift));
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), Shl(values, small_shifts));

    // Variable: large
    for (size_t i = 0; i < N; ++i) {
      expected[i] = T(TU(1) << (kMaxShift - (i & kMaxShift)));
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), Shl(v1, large_shifts));
  }
};

struct TestUnsignedRightShifts {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);
    auto expected = AllocateAligned<T>(N);
    HWY_ASSERT(expected);

    const auto values = Iota(d, 0);

    const T kMax = LimitsMax<T>();
    constexpr size_t kMaxShift = (sizeof(T) * 8) - 1;

    // Shift by 0
    HWY_ASSERT_VEC_EQ(d, values, ShiftRight<0>(values));
    HWY_ASSERT_VEC_EQ(d, values, ShiftRightSame(values, 0));

    // Shift by 1
    for (size_t i = 0; i < N; ++i) {
      expected[i] = T(T(i & kMax) >> 1);
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), ShiftRight<1>(values));
    HWY_ASSERT_VEC_EQ(d, expected.get(), ShiftRightSame(values, 1));

    // max
    for (size_t i = 0; i < N; ++i) {
      expected[i] = T(T(i & kMax) >> kMaxShift);
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), ShiftRight<kMaxShift>(values));
    HWY_ASSERT_VEC_EQ(d, expected.get(), ShiftRightSame(values, kMaxShift));
  }
};

struct TestRotateRight {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);
    auto expected = AllocateAligned<T>(N);
    HWY_ASSERT(expected);

    constexpr size_t kBits = sizeof(T) * 8;
    const Vec<D> mask_shift = Set(d, T{kBits});
    // Cover as many bit positions as possible to test shifting out
    const Vec<D> values = Shl(Set(d, T{1}), And(Iota(d, 0), mask_shift));

    // Rotate by 0
    HWY_ASSERT_VEC_EQ(d, values, RotateRight<0>(values));

    // Rotate by 1
    Store(values, d, expected.get());
    for (size_t i = 0; i < N; ++i) {
      expected[i] =
          static_cast<T>((expected[i] >> 1) | (expected[i] << (kBits - 1)));
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), RotateRight<1>(values));

    // Rotate by half
    Store(values, d, expected.get());
    for (size_t i = 0; i < N; ++i) {
      expected[i] = static_cast<T>((expected[i] >> (kBits / 2)) |
                                   (expected[i] << (kBits / 2)));
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), RotateRight<kBits / 2>(values));

    // Rotate by max
    Store(values, d, expected.get());
    for (size_t i = 0; i < N; ++i) {
      expected[i] =
          static_cast<T>((expected[i] >> (kBits - 1)) | (expected[i] << 1));
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), RotateRight<kBits - 1>(values));
  }
};

struct TestVariableUnsignedRightShifts {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);
    auto expected = AllocateAligned<T>(N);
    HWY_ASSERT(expected);

    const auto v0 = Zero(d);
    const auto v1 = Set(d, 1);
    const auto values = Iota(d, 0);

    const T kMax = LimitsMax<T>();
    const auto max = Set(d, kMax);

    constexpr size_t kMaxShift = (sizeof(T) * 8) - 1;
    const auto max_shift = Set(d, kMaxShift);
    const auto small_shifts = And(Iota(d, 0), max_shift);
    const auto large_shifts = max_shift - small_shifts;

    // Same: 0
    HWY_ASSERT_VEC_EQ(d, values, Shr(values, v0));

    // Same: 1
    for (size_t i = 0; i < N; ++i) {
      expected[i] = T(T(i & kMax) >> 1);
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), Shr(values, v1));

    // Same: max
    HWY_ASSERT_VEC_EQ(d, v0, Shr(values, max_shift));

    // Variable: small
    for (size_t i = 0; i < N; ++i) {
      expected[i] = T(i) >> (i & kMaxShift);
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), Shr(values, small_shifts));

    // Variable: Large
    for (size_t i = 0; i < N; ++i) {
      expected[i] = kMax >> (kMaxShift - (i & kMaxShift));
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), Shr(max, large_shifts));
  }
};

template <int kAmount, typename T>
T RightShiftNegative(T val) {
  // C++ shifts are implementation-defined for negative numbers, and we have
  // seen divisions replaced with shifts, so resort to bit operations.
  using TU = hwy::MakeUnsigned<T>;
  TU bits;
  CopySameSize(&val, &bits);

  const TU shifted = TU(bits >> kAmount);

  const TU all = TU(~TU(0));
  const size_t num_zero = sizeof(TU) * 8 - 1 - kAmount;
  const TU sign_extended = static_cast<TU>((all << num_zero) & LimitsMax<TU>());

  bits = shifted | sign_extended;
  CopySameSize(&bits, &val);
  return val;
}

class TestSignedRightShifts {
 public:
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);
    auto expected = AllocateAligned<T>(N);
    HWY_ASSERT(expected);
    constexpr T kMin = LimitsMin<T>();
    constexpr T kMax = LimitsMax<T>();
    constexpr size_t kMaxShift = (sizeof(T) * 8) - 1;

    // First test positive values, negative are checked below.
    const auto v0 = Zero(d);
    const auto values = And(Iota(d, 0), Set(d, kMax));

    // Shift by 0
    HWY_ASSERT_VEC_EQ(d, values, ShiftRight<0>(values));
    HWY_ASSERT_VEC_EQ(d, values, ShiftRightSame(values, 0));

    // Shift by 1
    for (size_t i = 0; i < N; ++i) {
      expected[i] = T(T(i & kMax) >> 1);
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), ShiftRight<1>(values));
    HWY_ASSERT_VEC_EQ(d, expected.get(), ShiftRightSame(values, 1));

    // max
    HWY_ASSERT_VEC_EQ(d, v0, ShiftRight<kMaxShift>(values));
    HWY_ASSERT_VEC_EQ(d, v0, ShiftRightSame(values, kMaxShift));

    // Even negative value
    Test<0>(kMin, d, __LINE__);
    Test<1>(kMin, d, __LINE__);
    Test<2>(kMin, d, __LINE__);
    Test<kMaxShift>(kMin, d, __LINE__);

    const T odd = static_cast<T>(kMin + 1);
    Test<0>(odd, d, __LINE__);
    Test<1>(odd, d, __LINE__);
    Test<2>(odd, d, __LINE__);
    Test<kMaxShift>(odd, d, __LINE__);
  }

 private:
  template <int kAmount, typename T, class D>
  void Test(T val, D d, int line) {
    const auto expected = Set(d, RightShiftNegative<kAmount>(val));
    const auto in = Set(d, val);
    const char* file = __FILE__;
    AssertVecEqual(d, expected, ShiftRight<kAmount>(in), file, line);
    AssertVecEqual(d, expected, ShiftRightSame(in, kAmount), file, line);
  }
};

struct TestVariableSignedRightShifts {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    using TU = MakeUnsigned<T>;
    const size_t N = Lanes(d);
    auto expected = AllocateAligned<T>(N);
    HWY_ASSERT(expected);

    constexpr T kMin = LimitsMin<T>();
    constexpr T kMax = LimitsMax<T>();

    constexpr size_t kMaxShift = (sizeof(T) * 8) - 1;

    // First test positive values, negative are checked below.
    const auto v0 = Zero(d);
    const auto positive = Iota(d, 0) & Set(d, kMax);

    // Shift by 0
    HWY_ASSERT_VEC_EQ(d, positive, ShiftRight<0>(positive));
    HWY_ASSERT_VEC_EQ(d, positive, ShiftRightSame(positive, 0));

    // Shift by 1
    for (size_t i = 0; i < N; ++i) {
      expected[i] = T(T(i & kMax) >> 1);
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), ShiftRight<1>(positive));
    HWY_ASSERT_VEC_EQ(d, expected.get(), ShiftRightSame(positive, 1));

    // max
    HWY_ASSERT_VEC_EQ(d, v0, ShiftRight<kMaxShift>(positive));
    HWY_ASSERT_VEC_EQ(d, v0, ShiftRightSame(positive, kMaxShift));

    const auto max_shift = Set(d, kMaxShift);
    const auto small_shifts = And(Iota(d, 0), max_shift);
    const auto large_shifts = max_shift - small_shifts;

    const auto negative = Iota(d, kMin);

    // Test varying negative to shift
    for (size_t i = 0; i < N; ++i) {
      expected[i] = RightShiftNegative<1>(static_cast<T>(kMin + i));
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), Shr(negative, Set(d, 1)));

    // Shift MSB right by small amounts
    for (size_t i = 0; i < N; ++i) {
      const size_t amount = i & kMaxShift;
      const TU shifted = ~((1ull << (kMaxShift - amount)) - 1);
      CopySameSize(&shifted, &expected[i]);
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), Shr(Set(d, kMin), small_shifts));

    // Shift MSB right by large amounts
    for (size_t i = 0; i < N; ++i) {
      const size_t amount = kMaxShift - (i & kMaxShift);
      const TU shifted = ~((1ull << (kMaxShift - amount)) - 1);
      CopySameSize(&shifted, &expected[i]);
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), Shr(Set(d, kMin), large_shifts));
  }
};

HWY_NOINLINE void TestAllShifts() {
  ForUnsignedTypes(ForPartialVectors<TestLeftShifts</*kSigned=*/false>>());
  ForSignedTypes(ForPartialVectors<TestLeftShifts</*kSigned=*/true>>());
  ForUnsignedTypes(ForPartialVectors<TestUnsignedRightShifts>());
  ForSignedTypes(ForPartialVectors<TestSignedRightShifts>());
}

HWY_NOINLINE void TestAllVariableShifts() {
  ForUnsignedTypes(ForPartialVectors<TestLeftShifts</*kSigned=*/false>>());
  ForUnsignedTypes(ForPartialVectors<TestUnsignedRightShifts>());

  const ForPartialVectors<TestLeftShifts</*kSigned=*/true>> shl_s;
  const ForPartialVectors<TestSignedRightShifts> shr_s;

  shl_s(int16_t());
  shr_s(int16_t());
  shl_s(int32_t());
  shr_s(int32_t());

#if HWY_HAVE_INTEGER64
  shl_s(int64_t());
  shr_s(int64_t());
#endif
}

HWY_NOINLINE void TestAllRotateRight() {
  ForUnsignedTypes(ForPartialVectors<TestRotateRight>());
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace hwy {
HWY_BEFORE_TEST(HwyShiftTest);
HWY_EXPORT_AND_TEST_P(HwyShiftTest, TestAllShifts);
HWY_EXPORT_AND_TEST_P(HwyShiftTest, TestAllVariableShifts);
HWY_EXPORT_AND_TEST_P(HwyShiftTest, TestAllRotateRight);
}  // namespace hwy

#endif

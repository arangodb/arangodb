// Copyright 2019 Google LLC
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

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "tests/arithmetic_test.cc"
#include "hwy/foreach_target.h"
#include "hwy/highway.h"
#include "hwy/tests/test_util-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

struct TestPlusMinus {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v2 = Iota(d, T(2));
    const auto v3 = Iota(d, T(3));
    const auto v4 = Iota(d, T(4));

    const size_t N = Lanes(d);
    auto lanes = AllocateAligned<T>(N);
    for (size_t i = 0; i < N; ++i) {
      lanes[i] = static_cast<T>((2 + i) + (3 + i));
    }
    HWY_ASSERT_VEC_EQ(d, lanes.get(), Add(v2, v3));
    HWY_ASSERT_VEC_EQ(d, Set(d, 2), Sub(v4, v2));

    for (size_t i = 0; i < N; ++i) {
      lanes[i] = static_cast<T>((2 + i) + (4 + i));
    }
    auto sum = v2;
    sum = Add(sum, v4);  // sum == 6,8..
    HWY_ASSERT_VEC_EQ(d, Load(d, lanes.get()), sum);

    sum = Sub(sum, v4);
    HWY_ASSERT_VEC_EQ(d, v2, sum);
  }
};

HWY_NOINLINE void TestAllPlusMinus() {
  ForAllTypes(ForPartialVectors<TestPlusMinus>());
}

struct TestUnsignedSaturatingArithmetic {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v0 = Zero(d);
    const auto vi = Iota(d, 1);
    const auto vm = Set(d, LimitsMax<T>());

    HWY_ASSERT_VEC_EQ(d, Add(v0, v0), SaturatedAdd(v0, v0));
    HWY_ASSERT_VEC_EQ(d, Add(v0, vi), SaturatedAdd(v0, vi));
    HWY_ASSERT_VEC_EQ(d, Add(v0, vm), SaturatedAdd(v0, vm));
    HWY_ASSERT_VEC_EQ(d, vm, SaturatedAdd(vi, vm));
    HWY_ASSERT_VEC_EQ(d, vm, SaturatedAdd(vm, vm));

    HWY_ASSERT_VEC_EQ(d, v0, SaturatedSub(v0, v0));
    HWY_ASSERT_VEC_EQ(d, v0, SaturatedSub(v0, vi));
    HWY_ASSERT_VEC_EQ(d, v0, SaturatedSub(vi, vi));
    HWY_ASSERT_VEC_EQ(d, v0, SaturatedSub(vi, vm));
    HWY_ASSERT_VEC_EQ(d, Sub(vm, vi), SaturatedSub(vm, vi));
  }
};

struct TestSignedSaturatingArithmetic {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v0 = Zero(d);
    const auto vpm = Set(d, LimitsMax<T>());
    // Ensure all lanes are positive, even if Iota wraps around
    const auto vi = Or(And(Iota(d, 0), vpm), Set(d, 1));
    const auto vn = Sub(v0, vi);
    const auto vnm = Set(d, LimitsMin<T>());
    HWY_ASSERT_MASK_EQ(d, MaskTrue(d), Gt(vi, v0));
    HWY_ASSERT_MASK_EQ(d, MaskTrue(d), Lt(vn, v0));

    HWY_ASSERT_VEC_EQ(d, v0, SaturatedAdd(v0, v0));
    HWY_ASSERT_VEC_EQ(d, vi, SaturatedAdd(v0, vi));
    HWY_ASSERT_VEC_EQ(d, vpm, SaturatedAdd(v0, vpm));
    HWY_ASSERT_VEC_EQ(d, vpm, SaturatedAdd(vi, vpm));
    HWY_ASSERT_VEC_EQ(d, vpm, SaturatedAdd(vpm, vpm));

    HWY_ASSERT_VEC_EQ(d, v0, SaturatedSub(v0, v0));
    HWY_ASSERT_VEC_EQ(d, Sub(v0, vi), SaturatedSub(v0, vi));
    HWY_ASSERT_VEC_EQ(d, vn, SaturatedSub(vn, v0));
    HWY_ASSERT_VEC_EQ(d, vnm, SaturatedSub(vnm, vi));
    HWY_ASSERT_VEC_EQ(d, vnm, SaturatedSub(vnm, vpm));
  }
};

HWY_NOINLINE void TestAllSaturatingArithmetic() {
  const ForPartialVectors<TestUnsignedSaturatingArithmetic> test_unsigned;
  test_unsigned(uint8_t());
  test_unsigned(uint16_t());

  const ForPartialVectors<TestSignedSaturatingArithmetic> test_signed;
  test_signed(int8_t());
  test_signed(int16_t());
}

struct TestAverage {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v0 = Zero(d);
    const auto v1 = Set(d, T(1));
    const auto v2 = Set(d, T(2));

    HWY_ASSERT_VEC_EQ(d, v0, AverageRound(v0, v0));
    HWY_ASSERT_VEC_EQ(d, v1, AverageRound(v0, v1));
    HWY_ASSERT_VEC_EQ(d, v1, AverageRound(v1, v1));
    HWY_ASSERT_VEC_EQ(d, v2, AverageRound(v1, v2));
    HWY_ASSERT_VEC_EQ(d, v2, AverageRound(v2, v2));
  }
};

HWY_NOINLINE void TestAllAverage() {
  const ForPartialVectors<TestAverage> test;
  test(uint8_t());
  test(uint16_t());
}

struct TestAbs {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v0 = Zero(d);
    const auto vp1 = Set(d, T(1));
    const auto vn1 = Set(d, T(-1));
    const auto vpm = Set(d, LimitsMax<T>());
    const auto vnm = Set(d, LimitsMin<T>());

    HWY_ASSERT_VEC_EQ(d, v0, Abs(v0));
    HWY_ASSERT_VEC_EQ(d, vp1, Abs(vp1));
    HWY_ASSERT_VEC_EQ(d, vp1, Abs(vn1));
    HWY_ASSERT_VEC_EQ(d, vpm, Abs(vpm));
    HWY_ASSERT_VEC_EQ(d, vnm, Abs(vnm));
  }
};

struct TestFloatAbs {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v0 = Zero(d);
    const auto vp1 = Set(d, T(1));
    const auto vn1 = Set(d, T(-1));
    const auto vp2 = Set(d, T(0.01));
    const auto vn2 = Set(d, T(-0.01));

    HWY_ASSERT_VEC_EQ(d, v0, Abs(v0));
    HWY_ASSERT_VEC_EQ(d, vp1, Abs(vp1));
    HWY_ASSERT_VEC_EQ(d, vp1, Abs(vn1));
    HWY_ASSERT_VEC_EQ(d, vp2, Abs(vp2));
    HWY_ASSERT_VEC_EQ(d, vp2, Abs(vn2));
  }
};

HWY_NOINLINE void TestAllAbs() {
  ForSignedTypes(ForPartialVectors<TestAbs>());
  ForFloatTypes(ForPartialVectors<TestFloatAbs>());
}

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

    const auto values = Iota(d, kSigned ? -TI(N) : TI(0));  // value to shift
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

struct TestVariableUnsignedRightShifts {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);
    auto expected = AllocateAligned<T>(N);

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
  CopyBytes<sizeof(T)>(&val, &bits);

  const TU shifted = TU(bits >> kAmount);

  const TU all = TU(~TU(0));
  const size_t num_zero = sizeof(TU) * 8 - 1 - kAmount;
  const TU sign_extended = static_cast<TU>((all << num_zero) & LimitsMax<TU>());

  bits = shifted | sign_extended;
  CopyBytes<sizeof(T)>(&bits, &val);
  return val;
}

class TestSignedRightShifts {
 public:
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);
    auto expected = AllocateAligned<T>(N);
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
      CopyBytes<sizeof(T)>(&shifted, &expected[i]);
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), Shr(Set(d, kMin), small_shifts));

    // Shift MSB right by large amounts
    for (size_t i = 0; i < N; ++i) {
      const size_t amount = kMaxShift - (i & kMaxShift);
      const TU shifted = ~((1ull << (kMaxShift - amount)) - 1);
      CopyBytes<sizeof(T)>(&shifted, &expected[i]);
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
  const ForPartialVectors<TestLeftShifts</*kSigned=*/false>> shl_u;
  const ForPartialVectors<TestLeftShifts</*kSigned=*/true>> shl_s;
  const ForPartialVectors<TestUnsignedRightShifts> shr_u;
  const ForPartialVectors<TestSignedRightShifts> shr_s;

  shl_u(uint16_t());
  shr_u(uint16_t());

  shl_u(uint32_t());
  shr_u(uint32_t());

  shl_s(int16_t());
  shr_s(int16_t());

  shl_s(int32_t());
  shr_s(int32_t());

#if HWY_CAP_INTEGER64
  shl_u(uint64_t());
  shr_u(uint64_t());

  shl_s(int64_t());
  shr_s(int64_t());
#endif
}

struct TestUnsignedMinMax {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v0 = Zero(d);
    // Leave headroom such that v1 < v2 even after wraparound.
    const auto mod = And(Iota(d, 0), Set(d, LimitsMax<T>() >> 1));
    const auto v1 = Add(mod, Set(d, 1));
    const auto v2 = Add(mod, Set(d, 2));
    HWY_ASSERT_VEC_EQ(d, v1, Min(v1, v2));
    HWY_ASSERT_VEC_EQ(d, v2, Max(v1, v2));
    HWY_ASSERT_VEC_EQ(d, v0, Min(v1, v0));
    HWY_ASSERT_VEC_EQ(d, v1, Max(v1, v0));

    const auto vmin = Set(d, LimitsMin<T>());
    const auto vmax = Set(d, LimitsMax<T>());

    HWY_ASSERT_VEC_EQ(d, vmin, Min(vmin, vmax));
    HWY_ASSERT_VEC_EQ(d, vmin, Min(vmax, vmin));

    HWY_ASSERT_VEC_EQ(d, vmax, Max(vmin, vmax));
    HWY_ASSERT_VEC_EQ(d, vmax, Max(vmax, vmin));
  }
};

struct TestSignedMinMax {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    // Leave headroom such that v1 < v2 even after wraparound.
    const auto mod = And(Iota(d, 0), Set(d, LimitsMax<T>() >> 1));
    const auto v1 = Add(mod, Set(d, 1));
    const auto v2 = Add(mod, Set(d, 2));
    const auto v_neg = Sub(Zero(d), v1);
    HWY_ASSERT_VEC_EQ(d, v1, Min(v1, v2));
    HWY_ASSERT_VEC_EQ(d, v2, Max(v1, v2));
    HWY_ASSERT_VEC_EQ(d, v_neg, Min(v1, v_neg));
    HWY_ASSERT_VEC_EQ(d, v1, Max(v1, v_neg));

    const auto v0 = Zero(d);
    const auto vmin = Set(d, LimitsMin<T>());
    const auto vmax = Set(d, LimitsMax<T>());
    HWY_ASSERT_VEC_EQ(d, vmin, Min(v0, vmin));
    HWY_ASSERT_VEC_EQ(d, vmin, Min(vmin, v0));
    HWY_ASSERT_VEC_EQ(d, v0, Max(v0, vmin));
    HWY_ASSERT_VEC_EQ(d, v0, Max(vmin, v0));

    HWY_ASSERT_VEC_EQ(d, vmin, Min(vmin, vmax));
    HWY_ASSERT_VEC_EQ(d, vmin, Min(vmax, vmin));

    HWY_ASSERT_VEC_EQ(d, vmax, Max(vmin, vmax));
    HWY_ASSERT_VEC_EQ(d, vmax, Max(vmax, vmin));
  }
};

struct TestFloatMinMax {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v1 = Iota(d, 1);
    const auto v2 = Iota(d, 2);
    const auto v_neg = Iota(d, -T(Lanes(d)));
    HWY_ASSERT_VEC_EQ(d, v1, Min(v1, v2));
    HWY_ASSERT_VEC_EQ(d, v2, Max(v1, v2));
    HWY_ASSERT_VEC_EQ(d, v_neg, Min(v1, v_neg));
    HWY_ASSERT_VEC_EQ(d, v1, Max(v1, v_neg));

    const auto v0 = Zero(d);
    const auto vmin = Set(d, T(-1E30));
    const auto vmax = Set(d, T(1E30));
    HWY_ASSERT_VEC_EQ(d, vmin, Min(v0, vmin));
    HWY_ASSERT_VEC_EQ(d, vmin, Min(vmin, v0));
    HWY_ASSERT_VEC_EQ(d, v0, Max(v0, vmin));
    HWY_ASSERT_VEC_EQ(d, v0, Max(vmin, v0));

    HWY_ASSERT_VEC_EQ(d, vmin, Min(vmin, vmax));
    HWY_ASSERT_VEC_EQ(d, vmin, Min(vmax, vmin));

    HWY_ASSERT_VEC_EQ(d, vmax, Max(vmin, vmax));
    HWY_ASSERT_VEC_EQ(d, vmax, Max(vmax, vmin));
  }
};

HWY_NOINLINE void TestAllMinMax() {
  ForUnsignedTypes(ForPartialVectors<TestUnsignedMinMax>());
  ForSignedTypes(ForPartialVectors<TestSignedMinMax>());
  ForFloatTypes(ForPartialVectors<TestFloatMinMax>());
}

struct TestUnsignedMul {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v0 = Zero(d);
    const auto v1 = Set(d, T(1));
    const auto vi = Iota(d, 1);
    const auto vj = Iota(d, 3);
    const size_t N = Lanes(d);
    auto expected = AllocateAligned<T>(N);

    HWY_ASSERT_VEC_EQ(d, v0, Mul(v0, v0));
    HWY_ASSERT_VEC_EQ(d, v1, Mul(v1, v1));
    HWY_ASSERT_VEC_EQ(d, vi, Mul(v1, vi));
    HWY_ASSERT_VEC_EQ(d, vi, Mul(vi, v1));

    for (size_t i = 0; i < N; ++i) {
      expected[i] = static_cast<T>((1 + i) * (1 + i));
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), Mul(vi, vi));

    for (size_t i = 0; i < N; ++i) {
      expected[i] = static_cast<T>((1 + i) * (3 + i));
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), Mul(vi, vj));

    const T max = LimitsMax<T>();
    const auto vmax = Set(d, max);
    HWY_ASSERT_VEC_EQ(d, vmax, Mul(vmax, v1));
    HWY_ASSERT_VEC_EQ(d, vmax, Mul(v1, vmax));

    const size_t bits = sizeof(T) * 8;
    const uint64_t mask = (1ull << bits) - 1;
    const T max2 = (uint64_t(max) * max) & mask;
    HWY_ASSERT_VEC_EQ(d, Set(d, max2), Mul(vmax, vmax));
  }
};

struct TestSignedMul {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);
    auto expected = AllocateAligned<T>(N);

    const auto v0 = Zero(d);
    const auto v1 = Set(d, T(1));
    const auto vi = Iota(d, 1);
    const auto vn = Iota(d, -T(N));  // no i8 supported, so no wraparound
    HWY_ASSERT_VEC_EQ(d, v0, Mul(v0, v0));
    HWY_ASSERT_VEC_EQ(d, v1, Mul(v1, v1));
    HWY_ASSERT_VEC_EQ(d, vi, Mul(v1, vi));
    HWY_ASSERT_VEC_EQ(d, vi, Mul(vi, v1));

    for (size_t i = 0; i < N; ++i) {
      expected[i] = static_cast<T>((1 + i) * (1 + i));
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), Mul(vi, vi));

    for (size_t i = 0; i < N; ++i) {
      expected[i] = static_cast<T>((-T(N) + T(i)) * T(1u + i));
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), Mul(vn, vi));
    HWY_ASSERT_VEC_EQ(d, expected.get(), Mul(vi, vn));
  }
};

HWY_NOINLINE void TestAllMul() {
  const ForPartialVectors<TestUnsignedMul> test_unsigned;
  // No u8.
  test_unsigned(uint16_t());
  test_unsigned(uint32_t());
  // No u64.

  const ForPartialVectors<TestSignedMul> test_signed;
  // No i8.
  test_signed(int16_t());
  test_signed(int32_t());
  // No i64.
}

struct TestMulHigh {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    using Wide = MakeWide<T>;
    const size_t N = Lanes(d);
    auto in_lanes = AllocateAligned<T>(N);
    auto expected_lanes = AllocateAligned<T>(N);

    const auto vi = Iota(d, 1);
    const auto vni = Iota(d, -T(N));  // no i8 supported, so no wraparound

    const auto v0 = Zero(d);
    HWY_ASSERT_VEC_EQ(d, v0, MulHigh(v0, v0));
    HWY_ASSERT_VEC_EQ(d, v0, MulHigh(v0, vi));
    HWY_ASSERT_VEC_EQ(d, v0, MulHigh(vi, v0));

    // Large positive squared
    for (size_t i = 0; i < N; ++i) {
      in_lanes[i] = T(LimitsMax<T>() >> i);
      expected_lanes[i] = T((Wide(in_lanes[i]) * in_lanes[i]) >> 16);
    }
    auto v = Load(d, in_lanes.get());
    HWY_ASSERT_VEC_EQ(d, expected_lanes.get(), MulHigh(v, v));

    // Large positive * small positive
    for (size_t i = 0; i < N; ++i) {
      expected_lanes[i] = T((Wide(in_lanes[i]) * T(1u + i)) >> 16);
    }
    HWY_ASSERT_VEC_EQ(d, expected_lanes.get(), MulHigh(v, vi));
    HWY_ASSERT_VEC_EQ(d, expected_lanes.get(), MulHigh(vi, v));

    // Large positive * small negative
    for (size_t i = 0; i < N; ++i) {
      expected_lanes[i] = T((Wide(in_lanes[i]) * T(i - N)) >> 16);
    }
    HWY_ASSERT_VEC_EQ(d, expected_lanes.get(), MulHigh(v, vni));
    HWY_ASSERT_VEC_EQ(d, expected_lanes.get(), MulHigh(vni, v));
  }
};

HWY_NOINLINE void TestAllMulHigh() {
  ForPartialVectors<TestMulHigh> test;
  test(int16_t());
  test(uint16_t());
}

struct TestMulEven {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    using Wide = MakeWide<T>;
    const Repartition<Wide, D> d2;
    const auto v0 = Zero(d);
    HWY_ASSERT_VEC_EQ(d2, Zero(d2), MulEven(v0, v0));

    const size_t N = Lanes(d);
    auto in_lanes = AllocateAligned<T>(N);
    auto expected = AllocateAligned<Wide>(Lanes(d2));
    for (size_t i = 0; i < N; i += 2) {
      in_lanes[i + 0] = LimitsMax<T>() >> i;
      if (N != 1) {
        in_lanes[i + 1] = 1;  // unused
      }
      expected[i / 2] = Wide(in_lanes[i + 0]) * in_lanes[i + 0];
    }

    const auto v = Load(d, in_lanes.get());
    HWY_ASSERT_VEC_EQ(d2, expected.get(), MulEven(v, v));
  }
};

struct TestMulEvenOdd64 {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
#if HWY_TARGET != HWY_SCALAR
    const auto v0 = Zero(d);
    HWY_ASSERT_VEC_EQ(d, Zero(d), MulEven(v0, v0));
    HWY_ASSERT_VEC_EQ(d, Zero(d), MulOdd(v0, v0));

    const size_t N = Lanes(d);
    if (N == 1) return;

    auto in1 = AllocateAligned<T>(N);
    auto in2 = AllocateAligned<T>(N);
    auto expected_even = AllocateAligned<T>(N);
    auto expected_odd = AllocateAligned<T>(N);

    // Random inputs in each lane
    RandomState rng;
    for (size_t rep = 0; rep < 1000; ++rep) {
      for (size_t i = 0; i < N; ++i) {
        in1[i] = Random64(&rng);
        in2[i] = Random64(&rng);
      }

      for (size_t i = 0; i < N; i += 2) {
        expected_even[i] = Mul128(in1[i], in2[i], &expected_even[i + 1]);
        expected_odd[i] = Mul128(in1[i + 1], in2[i + 1], &expected_odd[i + 1]);
      }

      const auto a = Load(d, in1.get());
      const auto b = Load(d, in2.get());
      HWY_ASSERT_VEC_EQ(d, expected_even.get(), MulEven(a, b));
      HWY_ASSERT_VEC_EQ(d, expected_odd.get(), MulOdd(a, b));
    }
#else
    (void)d;
#endif  // HWY_TARGET != HWY_SCALAR
  }
};

HWY_NOINLINE void TestAllMulEven() {
  ForExtendableVectors<TestMulEven> test;
  test(int32_t());
  test(uint32_t());

  ForGE128Vectors<TestMulEvenOdd64>()(uint64_t());
}

struct TestMulAdd {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto k0 = Zero(d);
    const auto kNeg0 = Set(d, T(-0.0));
    const auto v1 = Iota(d, 1);
    const auto v2 = Iota(d, 2);
    const size_t N = Lanes(d);
    auto expected = AllocateAligned<T>(N);
    HWY_ASSERT_VEC_EQ(d, k0, MulAdd(k0, k0, k0));
    HWY_ASSERT_VEC_EQ(d, v2, MulAdd(k0, v1, v2));
    HWY_ASSERT_VEC_EQ(d, v2, MulAdd(v1, k0, v2));
    HWY_ASSERT_VEC_EQ(d, k0, NegMulAdd(k0, k0, k0));
    HWY_ASSERT_VEC_EQ(d, v2, NegMulAdd(k0, v1, v2));
    HWY_ASSERT_VEC_EQ(d, v2, NegMulAdd(v1, k0, v2));

    for (size_t i = 0; i < N; ++i) {
      expected[i] = static_cast<T>((i + 1) * (i + 2));
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), MulAdd(v2, v1, k0));
    HWY_ASSERT_VEC_EQ(d, expected.get(), MulAdd(v1, v2, k0));
    HWY_ASSERT_VEC_EQ(d, expected.get(), NegMulAdd(Neg(v2), v1, k0));
    HWY_ASSERT_VEC_EQ(d, expected.get(), NegMulAdd(v1, Neg(v2), k0));

    for (size_t i = 0; i < N; ++i) {
      expected[i] = static_cast<T>((i + 2) * (i + 2) + (i + 1));
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), MulAdd(v2, v2, v1));
    HWY_ASSERT_VEC_EQ(d, expected.get(), NegMulAdd(Neg(v2), v2, v1));

    for (size_t i = 0; i < N; ++i) {
      expected[i] =
          T(-T(i + 2u) * static_cast<T>(i + 2) + static_cast<T>(1 + i));
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), NegMulAdd(v2, v2, v1));

    HWY_ASSERT_VEC_EQ(d, k0, MulSub(k0, k0, k0));
    HWY_ASSERT_VEC_EQ(d, kNeg0, NegMulSub(k0, k0, k0));

    for (size_t i = 0; i < N; ++i) {
      expected[i] = -T(i + 2);
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), MulSub(k0, v1, v2));
    HWY_ASSERT_VEC_EQ(d, expected.get(), MulSub(v1, k0, v2));
    HWY_ASSERT_VEC_EQ(d, expected.get(), NegMulSub(Neg(k0), v1, v2));
    HWY_ASSERT_VEC_EQ(d, expected.get(), NegMulSub(v1, Neg(k0), v2));

    for (size_t i = 0; i < N; ++i) {
      expected[i] = static_cast<T>((i + 1) * (i + 2));
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), MulSub(v1, v2, k0));
    HWY_ASSERT_VEC_EQ(d, expected.get(), MulSub(v2, v1, k0));
    HWY_ASSERT_VEC_EQ(d, expected.get(), NegMulSub(Neg(v1), v2, k0));
    HWY_ASSERT_VEC_EQ(d, expected.get(), NegMulSub(v2, Neg(v1), k0));

    for (size_t i = 0; i < N; ++i) {
      expected[i] = static_cast<T>((i + 2) * (i + 2) - (1 + i));
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), MulSub(v2, v2, v1));
    HWY_ASSERT_VEC_EQ(d, expected.get(), NegMulSub(Neg(v2), v2, v1));
  }
};

HWY_NOINLINE void TestAllMulAdd() {
  ForFloatTypes(ForPartialVectors<TestMulAdd>());
}

struct TestDiv {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v = Iota(d, T(-2));
    const auto v1 = Set(d, T(1));

    // Unchanged after division by 1.
    HWY_ASSERT_VEC_EQ(d, v, Div(v, v1));

    const size_t N = Lanes(d);
    auto expected = AllocateAligned<T>(N);
    for (size_t i = 0; i < N; ++i) {
      expected[i] = (T(i) - 2) / T(2);
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), Div(v, Set(d, T(2))));
  }
};

HWY_NOINLINE void TestAllDiv() { ForFloatTypes(ForPartialVectors<TestDiv>()); }

struct TestApproximateReciprocal {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v = Iota(d, T(-2));
    const auto nonzero = IfThenElse(Eq(v, Zero(d)), Set(d, T(1)), v);
    const size_t N = Lanes(d);
    auto input = AllocateAligned<T>(N);
    Store(nonzero, d, input.get());

    auto actual = AllocateAligned<T>(N);
    Store(ApproximateReciprocal(nonzero), d, actual.get());

    double max_l1 = 0.0;
    for (size_t i = 0; i < N; ++i) {
      max_l1 = HWY_MAX(max_l1, std::abs((1.0 / input[i]) - actual[i]));
    }
    const double max_rel = max_l1 / std::abs(1.0 / input[N - 1]);
    printf("max err %f\n", max_rel);

    HWY_ASSERT(max_rel < 0.002);
  }
};

HWY_NOINLINE void TestAllApproximateReciprocal() {
  ForPartialVectors<TestApproximateReciprocal>()(float());
}

struct TestSquareRoot {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto vi = Iota(d, 0);
    HWY_ASSERT_VEC_EQ(d, vi, Sqrt(Mul(vi, vi)));
  }
};

HWY_NOINLINE void TestAllSquareRoot() {
  ForFloatTypes(ForPartialVectors<TestSquareRoot>());
}

struct TestReciprocalSquareRoot {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v = Set(d, 123.0f);
    const size_t N = Lanes(d);
    auto lanes = AllocateAligned<T>(N);
    Store(ApproximateReciprocalSqrt(v), d, lanes.get());
    for (size_t i = 0; i < N; ++i) {
      float err = lanes[i] - 0.090166f;
      if (err < 0.0f) err = -err;
      HWY_ASSERT(err < 1E-4f);
    }
  }
};

HWY_NOINLINE void TestAllReciprocalSquareRoot() {
  ForPartialVectors<TestReciprocalSquareRoot>()(float());
}

template <typename T, class D>
AlignedFreeUniquePtr<T[]> RoundTestCases(T /*unused*/, D d, size_t& padded) {
  const T eps = std::numeric_limits<T>::epsilon();
  const T test_cases[] = {
    // +/- 1
    T(1),
    T(-1),
    // +/- 0
    T(0),
    T(-0),
    // near 0
    T(0.4),
    T(-0.4),
    // +/- integer
    T(4),
    T(-32),
    // positive near limit
    MantissaEnd<T>() - T(1.5),
    MantissaEnd<T>() + T(1.5),
    // negative near limit
    -MantissaEnd<T>() - T(1.5),
    -MantissaEnd<T>() + T(1.5),
    // positive tiebreak
    T(1.5),
    T(2.5),
    // negative tiebreak
    T(-1.5),
    T(-2.5),
    // positive +/- delta
    T(2.0001),
    T(3.9999),
    // negative +/- delta
    T(-999.9999),
    T(-998.0001),
    // positive +/- epsilon
    T(1) + eps,
    T(1) - eps,
    // negative +/- epsilon
    T(-1) + eps,
    T(-1) - eps,
#if !defined(HWY_EMULATE_SVE)  // these are not safe to just cast to int
    // +/- huge (but still fits in float)
    T(1E34),
    T(-1E35),
    // +/- infinity
    std::numeric_limits<T>::infinity(),
    -std::numeric_limits<T>::infinity(),
    // qNaN
    GetLane(NaN(d))
#endif
  };
  const size_t kNumTestCases = sizeof(test_cases) / sizeof(test_cases[0]);
  const size_t N = Lanes(d);
  padded = RoundUpTo(kNumTestCases, N);  // allow loading whole vectors
  auto in = AllocateAligned<T>(padded);
  auto expected = AllocateAligned<T>(padded);
  std::copy(test_cases, test_cases + kNumTestCases, in.get());
  std::fill(in.get() + kNumTestCases, in.get() + padded, T(0));
  return in;
}

struct TestRound {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T t, D d) {
    size_t padded;
    auto in = RoundTestCases(t, d, padded);
    auto expected = AllocateAligned<T>(padded);

    for (size_t i = 0; i < padded; ++i) {
      // Avoid [std::]round, which does not round to nearest *even*.
      // NOTE: std:: version from C++11 cmath is not defined in RVV GCC, see
      // https://lists.freebsd.org/pipermail/freebsd-current/2014-January/048130.html
      expected[i] = static_cast<T>(nearbyint(in[i]));
    }
    for (size_t i = 0; i < padded; i += Lanes(d)) {
      HWY_ASSERT_VEC_EQ(d, &expected[i], Round(Load(d, &in[i])));
    }
  }
};

HWY_NOINLINE void TestAllRound() {
  ForFloatTypes(ForPartialVectors<TestRound>());
}

struct TestNearestInt {
  template <typename TF, class DF>
  HWY_NOINLINE void operator()(TF tf, const DF df) {
    using TI = MakeSigned<TF>;
    const RebindToSigned<DF> di;

    size_t padded;
    auto in = RoundTestCases(tf, df, padded);
    auto expected = AllocateAligned<TI>(padded);

    constexpr double max = static_cast<double>(LimitsMax<TI>());
    for (size_t i = 0; i < padded; ++i) {
      if (std::isnan(in[i])) {
        // We replace NaN with 0 below (no_nan)
        expected[i] = 0;
      } else if (std::isinf(in[i]) || double(std::abs(in[i])) >= max) {
        // Avoid undefined result for lrintf
        expected[i] = std::signbit(in[i]) ? LimitsMin<TI>() : LimitsMax<TI>();
      } else {
        expected[i] = static_cast<TI>(lrintf(in[i]));
      }
    }
    for (size_t i = 0; i < padded; i += Lanes(df)) {
      const auto v = Load(df, &in[i]);
      const auto no_nan = IfThenElse(Eq(v, v), v, Zero(df));
      HWY_ASSERT_VEC_EQ(di, &expected[i], NearestInt(no_nan));
    }
  }
};

HWY_NOINLINE void TestAllNearestInt() {
  ForPartialVectors<TestNearestInt>()(float());
}

struct TestTrunc {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T t, D d) {
    size_t padded;
    auto in = RoundTestCases(t, d, padded);
    auto expected = AllocateAligned<T>(padded);

    for (size_t i = 0; i < padded; ++i) {
      // NOTE: std:: version from C++11 cmath is not defined in RVV GCC, see
      // https://lists.freebsd.org/pipermail/freebsd-current/2014-January/048130.html
      expected[i] = static_cast<T>(trunc(in[i]));
    }
    for (size_t i = 0; i < padded; i += Lanes(d)) {
      HWY_ASSERT_VEC_EQ(d, &expected[i], Trunc(Load(d, &in[i])));
    }
  }
};

HWY_NOINLINE void TestAllTrunc() {
  ForFloatTypes(ForPartialVectors<TestTrunc>());
}

struct TestCeil {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T t, D d) {
    size_t padded;
    auto in = RoundTestCases(t, d, padded);
    auto expected = AllocateAligned<T>(padded);

    for (size_t i = 0; i < padded; ++i) {
      expected[i] = std::ceil(in[i]);
    }
    for (size_t i = 0; i < padded; i += Lanes(d)) {
      HWY_ASSERT_VEC_EQ(d, &expected[i], Ceil(Load(d, &in[i])));
    }
  }
};

HWY_NOINLINE void TestAllCeil() {
  ForFloatTypes(ForPartialVectors<TestCeil>());
}

struct TestFloor {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T t, D d) {
    size_t padded;
    auto in = RoundTestCases(t, d, padded);
    auto expected = AllocateAligned<T>(padded);

    for (size_t i = 0; i < padded; ++i) {
      expected[i] = std::floor(in[i]);
    }
    for (size_t i = 0; i < padded; i += Lanes(d)) {
      HWY_ASSERT_VEC_EQ(d, &expected[i], Floor(Load(d, &in[i])));
    }
  }
};

HWY_NOINLINE void TestAllFloor() {
  ForFloatTypes(ForPartialVectors<TestFloor>());
}

struct TestSumOfLanes {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);
    auto in_lanes = AllocateAligned<T>(N);

    // Lane i = bit i, higher lanes 0
    double sum = 0.0;
    // Avoid setting sign bit and cap at double precision
    constexpr size_t kBits = HWY_MIN(sizeof(T) * 8 - 1, 51);
    for (size_t i = 0; i < N; ++i) {
      in_lanes[i] = i < kBits ? static_cast<T>(1ull << i) : 0;
      sum += static_cast<double>(in_lanes[i]);
    }
    HWY_ASSERT_VEC_EQ(d, Set(d, T(sum)),
                      SumOfLanes(d, Load(d, in_lanes.get())));

    // Lane i = i (iota) to include upper lanes
    sum = 0.0;
    for (size_t i = 0; i < N; ++i) {
      sum += static_cast<double>(i);
    }
    HWY_ASSERT_VEC_EQ(d, Set(d, T(sum)), SumOfLanes(d, Iota(d, 0)));
  }
};

HWY_NOINLINE void TestAllSumOfLanes() {
  const ForPartialVectors<TestSumOfLanes> test;

  // No u8/u16/i8/i16.
  test(uint32_t());
  test(int32_t());

#if HWY_CAP_INTEGER64
  test(uint64_t());
  test(int64_t());
#endif

  ForFloatTypes(test);
}

struct TestMinOfLanes {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);
    auto in_lanes = AllocateAligned<T>(N);

    // Lane i = bit i, higher lanes = 2 (not the minimum)
    T min = HighestValue<T>();
    // Avoid setting sign bit and cap at double precision
    constexpr size_t kBits = HWY_MIN(sizeof(T) * 8 - 1, 51);
    for (size_t i = 0; i < N; ++i) {
      in_lanes[i] = i < kBits ? static_cast<T>(1ull << i) : 2;
      min = HWY_MIN(min, in_lanes[i]);
    }
    HWY_ASSERT_VEC_EQ(d, Set(d, min), MinOfLanes(d, Load(d, in_lanes.get())));

    // Lane i = N - i to include upper lanes
    min = HighestValue<T>();
    for (size_t i = 0; i < N; ++i) {
      in_lanes[i] = static_cast<T>(N - i);  // no 8-bit T so no wraparound
      min = HWY_MIN(min, in_lanes[i]);
    }
    HWY_ASSERT_VEC_EQ(d, Set(d, min), MinOfLanes(d, Load(d, in_lanes.get())));
  }
};

struct TestMaxOfLanes {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);
    auto in_lanes = AllocateAligned<T>(N);

    T max = LowestValue<T>();
    // Avoid setting sign bit and cap at double precision
    constexpr size_t kBits = HWY_MIN(sizeof(T) * 8 - 1, 51);
    for (size_t i = 0; i < N; ++i) {
      in_lanes[i] = i < kBits ? static_cast<T>(1ull << i) : 0;
      max = HWY_MAX(max, in_lanes[i]);
    }
    HWY_ASSERT_VEC_EQ(d, Set(d, max), MaxOfLanes(d, Load(d, in_lanes.get())));

    // Lane i = i to include upper lanes
    max = LowestValue<T>();
    for (size_t i = 0; i < N; ++i) {
      in_lanes[i] = static_cast<T>(i);  // no 8-bit T so no wraparound
      max = HWY_MAX(max, in_lanes[i]);
    }
    HWY_ASSERT_VEC_EQ(d, Set(d, max), MaxOfLanes(d, Load(d, in_lanes.get())));
  }
};

HWY_NOINLINE void TestAllMinMaxOfLanes() {
  const ForPartialVectors<TestMinOfLanes> min;
  const ForPartialVectors<TestMaxOfLanes> max;

  // No u8/u16/i8/i16.
  min(uint32_t());
  max(uint32_t());
  min(int32_t());
  max(int32_t());

#if HWY_CAP_INTEGER64
  min(uint64_t());
  max(uint64_t());
  min(int64_t());
  max(int64_t());
#endif

  ForFloatTypes(min);
  ForFloatTypes(max);
}

struct TestAbsDiff {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);
    auto in_lanes_a = AllocateAligned<T>(N);
    auto in_lanes_b = AllocateAligned<T>(N);
    auto out_lanes = AllocateAligned<T>(N);
    for (size_t i = 0; i < N; ++i) {
      in_lanes_a[i] = static_cast<T>((i ^ 1u) << i);
      in_lanes_b[i] = static_cast<T>(i << i);
      out_lanes[i] = std::abs(in_lanes_a[i] - in_lanes_b[i]);
    }
    const auto a = Load(d, in_lanes_a.get());
    const auto b = Load(d, in_lanes_b.get());
    const auto expected = Load(d, out_lanes.get());
    HWY_ASSERT_VEC_EQ(d, expected, AbsDiff(a, b));
    HWY_ASSERT_VEC_EQ(d, expected, AbsDiff(b, a));
  }
};

HWY_NOINLINE void TestAllAbsDiff() {
  ForPartialVectors<TestAbsDiff>()(float());
}

struct TestNeg {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v0 = Zero(d);
    const auto vn = Set(d, T(-3));
    const auto vp = Set(d, T(3));
    HWY_ASSERT_VEC_EQ(d, v0, Neg(v0));
    HWY_ASSERT_VEC_EQ(d, vp, Neg(vn));
    HWY_ASSERT_VEC_EQ(d, vn, Neg(vp));
  }
};

HWY_NOINLINE void TestAllNeg() {
  ForSignedTypes(ForPartialVectors<TestNeg>());
  ForFloatTypes(ForPartialVectors<TestNeg>());
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace hwy {
HWY_BEFORE_TEST(HwyArithmeticTest);
HWY_EXPORT_AND_TEST_P(HwyArithmeticTest, TestAllPlusMinus);
HWY_EXPORT_AND_TEST_P(HwyArithmeticTest, TestAllSaturatingArithmetic);
HWY_EXPORT_AND_TEST_P(HwyArithmeticTest, TestAllShifts);
HWY_EXPORT_AND_TEST_P(HwyArithmeticTest, TestAllVariableShifts);
HWY_EXPORT_AND_TEST_P(HwyArithmeticTest, TestAllMinMax);
HWY_EXPORT_AND_TEST_P(HwyArithmeticTest, TestAllAverage);
HWY_EXPORT_AND_TEST_P(HwyArithmeticTest, TestAllAbs);
HWY_EXPORT_AND_TEST_P(HwyArithmeticTest, TestAllMul);
HWY_EXPORT_AND_TEST_P(HwyArithmeticTest, TestAllMulHigh);
HWY_EXPORT_AND_TEST_P(HwyArithmeticTest, TestAllMulEven);
HWY_EXPORT_AND_TEST_P(HwyArithmeticTest, TestAllMulAdd);
HWY_EXPORT_AND_TEST_P(HwyArithmeticTest, TestAllDiv);
HWY_EXPORT_AND_TEST_P(HwyArithmeticTest, TestAllApproximateReciprocal);
HWY_EXPORT_AND_TEST_P(HwyArithmeticTest, TestAllSquareRoot);
HWY_EXPORT_AND_TEST_P(HwyArithmeticTest, TestAllReciprocalSquareRoot);
HWY_EXPORT_AND_TEST_P(HwyArithmeticTest, TestAllSumOfLanes);
HWY_EXPORT_AND_TEST_P(HwyArithmeticTest, TestAllMinMaxOfLanes);
HWY_EXPORT_AND_TEST_P(HwyArithmeticTest, TestAllRound);
HWY_EXPORT_AND_TEST_P(HwyArithmeticTest, TestAllNearestInt);
HWY_EXPORT_AND_TEST_P(HwyArithmeticTest, TestAllTrunc);
HWY_EXPORT_AND_TEST_P(HwyArithmeticTest, TestAllCeil);
HWY_EXPORT_AND_TEST_P(HwyArithmeticTest, TestAllFloor);
HWY_EXPORT_AND_TEST_P(HwyArithmeticTest, TestAllAbsDiff);
HWY_EXPORT_AND_TEST_P(HwyArithmeticTest, TestAllNeg);
}  // namespace hwy

// Ought not to be necessary, but without this, no tests run on RVV.
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

#endif

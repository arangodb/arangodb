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
#define HWY_TARGET_INCLUDE "tests/arithmetic_test.cc"
#include "hwy/foreach_target.h"  // IWYU pragma: keep
#include "hwy/highway.h"
#include "hwy/tests/test_util-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

struct TestPlusMinus {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v2 = Iota(d, T{2});
    const auto v3 = Iota(d, T{3});
    const auto v4 = Iota(d, T{4});

    const size_t N = Lanes(d);
    auto lanes = AllocateAligned<T>(N);
    HWY_ASSERT(lanes);
    for (size_t i = 0; i < N; ++i) {
      lanes[i] = static_cast<T>((2 + i) + (3 + i));
    }
    HWY_ASSERT_VEC_EQ(d, lanes.get(), Add(v2, v3));
    HWY_ASSERT_VEC_EQ(d, Set(d, T{2}), Sub(v4, v2));

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

struct TestPlusMinusOverflow {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v1 = Iota(d, T(1));
    const auto vMax = Iota(d, LimitsMax<T>());
    const auto vMin = Iota(d, LimitsMin<T>());

    // Check that no UB triggered.
    // "assert" here is formal - to avoid compiler dropping calculations
    HWY_ASSERT_VEC_EQ(d, Add(v1, vMax), Add(vMax, v1));
    HWY_ASSERT_VEC_EQ(d, Add(vMax, vMax), Add(vMax, vMax));
    HWY_ASSERT_VEC_EQ(d, Sub(vMin, v1), Sub(vMin, v1));
    HWY_ASSERT_VEC_EQ(d, Sub(vMin, vMax), Sub(vMin, vMax));
  }
};

HWY_NOINLINE void TestAllPlusMinus() {
  ForAllTypes(ForPartialVectors<TestPlusMinus>());
  ForIntegerTypes(ForPartialVectors<TestPlusMinusOverflow>());
}

struct TestUnsignedSaturatingArithmetic {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v0 = Zero(d);
    const auto vi = Iota(d, T{1});
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
    const auto vi = Or(And(Iota(d, 0), vpm), Set(d, T{1}));
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

struct TestSaturatingArithmeticOverflow {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v1 = Iota(d, T(1));
    const auto vMax = Iota(d, LimitsMax<T>());
    const auto vMin = Iota(d, LimitsMin<T>());

    // Check that no UB triggered.
    // "assert" here is formal - to avoid compiler dropping calculations
    HWY_ASSERT_VEC_EQ(d, SaturatedAdd(v1, vMax), SaturatedAdd(vMax, v1));
    HWY_ASSERT_VEC_EQ(d, SaturatedAdd(vMax, vMax), SaturatedAdd(vMax, vMax));
    HWY_ASSERT_VEC_EQ(d, SaturatedAdd(vMin, vMax), SaturatedAdd(vMin, vMax));
    HWY_ASSERT_VEC_EQ(d, SaturatedAdd(vMin, vMin), SaturatedAdd(vMin, vMin));
    HWY_ASSERT_VEC_EQ(d, SaturatedSub(vMin, v1), SaturatedSub(vMin, v1));
    HWY_ASSERT_VEC_EQ(d, SaturatedSub(vMin, vMax), SaturatedSub(vMin, vMax));
    HWY_ASSERT_VEC_EQ(d, SaturatedSub(vMax, vMin), SaturatedSub(vMax, vMin));
    HWY_ASSERT_VEC_EQ(d, SaturatedSub(vMin, vMin), SaturatedSub(vMin, vMin));
  }
};

HWY_NOINLINE void TestAllSaturatingArithmetic() {
  ForUnsignedTypes(ForPartialVectors<TestUnsignedSaturatingArithmetic>());
  ForSignedTypes(ForPartialVectors<TestSignedSaturatingArithmetic>());
  ForIntegerTypes(ForPartialVectors<TestSaturatingArithmeticOverflow>());
}

struct TestAverage {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v0 = Zero(d);
    const auto v1 = Set(d, T{1});
    const auto v2 = Set(d, T{2});

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
    const auto vp1 = Set(d, T{1});
    const auto vn1 = Set(d, T{-1});
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
    const auto vp1 = Set(d, T{1});
    const auto vn1 = Set(d, T{-1});
    const auto vp2 = Set(d, static_cast<T>(0.01));
    const auto vn2 = Set(d, static_cast<T>(-0.01));

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

struct TestNeg {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v0 = Zero(d);
    const auto vn = Set(d, T{-3});
    const auto vp = Set(d, T{3});
    HWY_ASSERT_VEC_EQ(d, v0, Neg(v0));
    HWY_ASSERT_VEC_EQ(d, vp, Neg(vn));
    HWY_ASSERT_VEC_EQ(d, vn, Neg(vp));
  }
};

struct TestNegOverflow {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto vn = Set(d, LimitsMin<T>());
    const auto vp = Set(d, LimitsMax<T>());
    HWY_ASSERT_VEC_EQ(d, Neg(vn), Neg(vn));
    HWY_ASSERT_VEC_EQ(d, Neg(vp), Neg(vp));
  }
};

HWY_NOINLINE void TestAllNeg() {
  ForSignedTypes(ForPartialVectors<TestNeg>());
  ForFloatTypes(ForPartialVectors<TestNeg>());
  ForSignedTypes(ForPartialVectors<TestNegOverflow>());
}

struct TestUnsignedMinMax {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v0 = Zero(d);
    // Leave headroom such that v1 < v2 even after wraparound.
    const auto mod = And(Iota(d, 0), Set(d, LimitsMax<T>() >> 1));
    const auto v1 = Add(mod, Set(d, T{1}));
    const auto v2 = Add(mod, Set(d, T{2}));
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
    const auto mod =
        And(Iota(d, 0), Set(d, static_cast<T>(LimitsMax<T>() >> 1)));
    const auto v1 = Add(mod, Set(d, T{1}));
    const auto v2 = Add(mod, Set(d, T{2}));
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
    const auto vmin = Set(d, static_cast<T>(-1E30));
    const auto vmax = Set(d, static_cast<T>(1E30));
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

template <class D>
static HWY_NOINLINE Vec<D> Make128(D d, uint64_t hi, uint64_t lo) {
  alignas(16) uint64_t in[2];
  in[0] = lo;
  in[1] = hi;
  return LoadDup128(d, in);
}

struct TestMinMax128 {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    using V = Vec<D>;
    const size_t N = Lanes(d);
    auto a_lanes = AllocateAligned<T>(N);
    auto b_lanes = AllocateAligned<T>(N);
    auto min_lanes = AllocateAligned<T>(N);
    auto max_lanes = AllocateAligned<T>(N);
    RandomState rng;

    const V v00 = Zero(d);
    const V v01 = Make128(d, 0, 1);
    const V v10 = Make128(d, 1, 0);
    const V v11 = Add(v01, v10);

    // Same arg
    HWY_ASSERT_VEC_EQ(d, v00, Min128(d, v00, v00));
    HWY_ASSERT_VEC_EQ(d, v01, Min128(d, v01, v01));
    HWY_ASSERT_VEC_EQ(d, v10, Min128(d, v10, v10));
    HWY_ASSERT_VEC_EQ(d, v11, Min128(d, v11, v11));
    HWY_ASSERT_VEC_EQ(d, v00, Max128(d, v00, v00));
    HWY_ASSERT_VEC_EQ(d, v01, Max128(d, v01, v01));
    HWY_ASSERT_VEC_EQ(d, v10, Max128(d, v10, v10));
    HWY_ASSERT_VEC_EQ(d, v11, Max128(d, v11, v11));

    // First arg less
    HWY_ASSERT_VEC_EQ(d, v00, Min128(d, v00, v01));
    HWY_ASSERT_VEC_EQ(d, v01, Min128(d, v01, v10));
    HWY_ASSERT_VEC_EQ(d, v10, Min128(d, v10, v11));
    HWY_ASSERT_VEC_EQ(d, v01, Max128(d, v00, v01));
    HWY_ASSERT_VEC_EQ(d, v10, Max128(d, v01, v10));
    HWY_ASSERT_VEC_EQ(d, v11, Max128(d, v10, v11));

    // Second arg less
    HWY_ASSERT_VEC_EQ(d, v00, Min128(d, v01, v00));
    HWY_ASSERT_VEC_EQ(d, v01, Min128(d, v10, v01));
    HWY_ASSERT_VEC_EQ(d, v10, Min128(d, v11, v10));
    HWY_ASSERT_VEC_EQ(d, v01, Max128(d, v01, v00));
    HWY_ASSERT_VEC_EQ(d, v10, Max128(d, v10, v01));
    HWY_ASSERT_VEC_EQ(d, v11, Max128(d, v11, v10));

    // Also check 128-bit blocks are independent
    for (size_t rep = 0; rep < AdjustedReps(1000); ++rep) {
      for (size_t i = 0; i < N; ++i) {
        a_lanes[i] = Random64(&rng);
        b_lanes[i] = Random64(&rng);
      }
      const V a = Load(d, a_lanes.get());
      const V b = Load(d, b_lanes.get());
      for (size_t i = 0; i < N; i += 2) {
        const bool lt = a_lanes[i + 1] == b_lanes[i + 1]
                            ? (a_lanes[i] < b_lanes[i])
                            : (a_lanes[i + 1] < b_lanes[i + 1]);
        min_lanes[i + 0] = lt ? a_lanes[i + 0] : b_lanes[i + 0];
        min_lanes[i + 1] = lt ? a_lanes[i + 1] : b_lanes[i + 1];
        max_lanes[i + 0] = lt ? b_lanes[i + 0] : a_lanes[i + 0];
        max_lanes[i + 1] = lt ? b_lanes[i + 1] : a_lanes[i + 1];
      }
      HWY_ASSERT_VEC_EQ(d, min_lanes.get(), Min128(d, a, b));
      HWY_ASSERT_VEC_EQ(d, max_lanes.get(), Max128(d, a, b));
    }
  }
};

HWY_NOINLINE void TestAllMinMax128() {
  ForGEVectors<128, TestMinMax128>()(uint64_t());
}

struct TestMinMax128Upper {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    using V = Vec<D>;
    const size_t N = Lanes(d);
    auto a_lanes = AllocateAligned<T>(N);
    auto b_lanes = AllocateAligned<T>(N);
    auto min_lanes = AllocateAligned<T>(N);
    auto max_lanes = AllocateAligned<T>(N);
    RandomState rng;

    const V v00 = Zero(d);
    const V v01 = Make128(d, 0, 1);
    const V v10 = Make128(d, 1, 0);
    const V v11 = Add(v01, v10);

    // Same arg
    HWY_ASSERT_VEC_EQ(d, v00, Min128Upper(d, v00, v00));
    HWY_ASSERT_VEC_EQ(d, v01, Min128Upper(d, v01, v01));
    HWY_ASSERT_VEC_EQ(d, v10, Min128Upper(d, v10, v10));
    HWY_ASSERT_VEC_EQ(d, v11, Min128Upper(d, v11, v11));
    HWY_ASSERT_VEC_EQ(d, v00, Max128Upper(d, v00, v00));
    HWY_ASSERT_VEC_EQ(d, v01, Max128Upper(d, v01, v01));
    HWY_ASSERT_VEC_EQ(d, v10, Max128Upper(d, v10, v10));
    HWY_ASSERT_VEC_EQ(d, v11, Max128Upper(d, v11, v11));

    // Equivalent but not equal (chooses second arg)
    HWY_ASSERT_VEC_EQ(d, v01, Min128Upper(d, v00, v01));
    HWY_ASSERT_VEC_EQ(d, v11, Min128Upper(d, v10, v11));
    HWY_ASSERT_VEC_EQ(d, v00, Min128Upper(d, v01, v00));
    HWY_ASSERT_VEC_EQ(d, v10, Min128Upper(d, v11, v10));
    HWY_ASSERT_VEC_EQ(d, v00, Max128Upper(d, v01, v00));
    HWY_ASSERT_VEC_EQ(d, v10, Max128Upper(d, v11, v10));
    HWY_ASSERT_VEC_EQ(d, v01, Max128Upper(d, v00, v01));
    HWY_ASSERT_VEC_EQ(d, v11, Max128Upper(d, v10, v11));

    // First arg less
    HWY_ASSERT_VEC_EQ(d, v01, Min128Upper(d, v01, v10));
    HWY_ASSERT_VEC_EQ(d, v10, Max128Upper(d, v01, v10));

    // Second arg less
    HWY_ASSERT_VEC_EQ(d, v01, Min128Upper(d, v10, v01));
    HWY_ASSERT_VEC_EQ(d, v10, Max128Upper(d, v10, v01));

    // Also check 128-bit blocks are independent
    for (size_t rep = 0; rep < AdjustedReps(1000); ++rep) {
      for (size_t i = 0; i < N; ++i) {
        a_lanes[i] = Random64(&rng);
        b_lanes[i] = Random64(&rng);
      }
      const V a = Load(d, a_lanes.get());
      const V b = Load(d, b_lanes.get());
      for (size_t i = 0; i < N; i += 2) {
        const bool lt = a_lanes[i + 1] < b_lanes[i + 1];
        min_lanes[i + 0] = lt ? a_lanes[i + 0] : b_lanes[i + 0];
        min_lanes[i + 1] = lt ? a_lanes[i + 1] : b_lanes[i + 1];
        max_lanes[i + 0] = lt ? b_lanes[i + 0] : a_lanes[i + 0];
        max_lanes[i + 1] = lt ? b_lanes[i + 1] : a_lanes[i + 1];
      }
      HWY_ASSERT_VEC_EQ(d, min_lanes.get(), Min128Upper(d, a, b));
      HWY_ASSERT_VEC_EQ(d, max_lanes.get(), Max128Upper(d, a, b));
    }
  }
};

HWY_NOINLINE void TestAllMinMax128Upper() {
  ForGEVectors<128, TestMinMax128Upper>()(uint64_t());
}

struct TestIntegerAbsDiff {
  template<typename T, HWY_IF_T_SIZE_ONE_OF(T, (1 << 1) | (1 << 2) | (1 << 4))>
  static inline T ScalarAbsDiff(T a, T b) {
    using TW = MakeSigned<MakeWide<T>>;
    const TW diff = static_cast<TW>(a) - static_cast<TW>(b);
    return static_cast<T>((diff >= 0) ? diff : -diff);
  }
  template<typename T, HWY_IF_T_SIZE(T, 8)>
  static inline T ScalarAbsDiff(T a, T b) {
    if (a >= b) {
      return a - b;
    } else {
      return b - a;
    }
  }

  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);
    auto in_lanes_a = AllocateAligned<T>(N);
    auto in_lanes_b = AllocateAligned<T>(N);
    auto out_lanes = AllocateAligned<T>(N);
    constexpr size_t shift_amt_mask = sizeof(T) * 8 - 1;
    for (size_t i = 0; i < N; ++i) {
      // Need to mask out shift_amt as i can be greater than or equal to
      // the number of bits in T if T is int8_t, uint8_t, int16_t, or uint16_t.
      const auto shift_amt = i & shift_amt_mask;
      in_lanes_a[i] =
        static_cast<T>((static_cast<uint64_t>(i) ^ 1u) << shift_amt);
      in_lanes_b[i] =
        static_cast<T>(static_cast<uint64_t>(i) << shift_amt);
      out_lanes[i] = ScalarAbsDiff(in_lanes_a[i], in_lanes_b[i]);
    }
    const auto a = Load(d, in_lanes_a.get());
    const auto b = Load(d, in_lanes_b.get());
    const auto expected = Load(d, out_lanes.get());
    HWY_ASSERT_VEC_EQ(d, expected, AbsDiff(a, b));
    HWY_ASSERT_VEC_EQ(d, expected, AbsDiff(b, a));
  }
};

HWY_NOINLINE void TestAllIntegerAbsDiff() {
  ForPartialVectors<TestIntegerAbsDiff>()(int8_t());
  ForPartialVectors<TestIntegerAbsDiff>()(uint8_t());
  ForPartialVectors<TestIntegerAbsDiff>()(int16_t());
  ForPartialVectors<TestIntegerAbsDiff>()(uint16_t());
  ForPartialVectors<TestIntegerAbsDiff>()(int32_t());
  ForPartialVectors<TestIntegerAbsDiff>()(uint32_t());
#if HWY_HAVE_INTEGER64
  ForPartialVectors<TestIntegerAbsDiff>()(int64_t());
  ForPartialVectors<TestIntegerAbsDiff>()(uint64_t());
#endif
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
HWY_EXPORT_AND_TEST_P(HwyArithmeticTest, TestAllAverage);
HWY_EXPORT_AND_TEST_P(HwyArithmeticTest, TestAllAbs);
HWY_EXPORT_AND_TEST_P(HwyArithmeticTest, TestAllNeg);
HWY_EXPORT_AND_TEST_P(HwyArithmeticTest, TestAllMinMax);
HWY_EXPORT_AND_TEST_P(HwyArithmeticTest, TestAllMinMax128);
HWY_EXPORT_AND_TEST_P(HwyArithmeticTest, TestAllMinMax128Upper);
HWY_EXPORT_AND_TEST_P(HwyArithmeticTest, TestAllIntegerAbsDiff);
}  // namespace hwy

#endif

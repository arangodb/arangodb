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

#include <string.h>  // memcpy

#include <algorithm>  // std::fill

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "tests/combine_test.cc"
#include "hwy/foreach_target.h"  // IWYU pragma: keep
#include "hwy/highway.h"
#include "hwy/tests/test_util-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

struct TestLowerHalf {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const Half<D> d2;

    const size_t N = Lanes(d);
    auto lanes = AllocateAligned<T>(N);
    auto lanes2 = AllocateAligned<T>(N);
    HWY_ASSERT(lanes && lanes2);
    std::fill(lanes.get(), lanes.get() + N, T(0));
    std::fill(lanes2.get(), lanes2.get() + N, T(0));
    const auto v = Iota(d, 1);
    Store(LowerHalf(d2, v), d2, lanes.get());
    Store(LowerHalf(v), d2, lanes2.get());  // optionally without D
    size_t i = 0;
    for (; i < Lanes(d2); ++i) {
      HWY_ASSERT_EQ(T(1 + i), lanes[i]);
      HWY_ASSERT_EQ(T(1 + i), lanes2[i]);
    }
    // Other half remains unchanged
    for (; i < N; ++i) {
      HWY_ASSERT_EQ(T(0), lanes[i]);
      HWY_ASSERT_EQ(T(0), lanes2[i]);
    }
  }
};

struct TestLowerQuarter {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const Half<D> d2;
    const Half<decltype(d2)> d4;

    const size_t N = Lanes(d);
    auto lanes = AllocateAligned<T>(N);
    auto lanes2 = AllocateAligned<T>(N);
    HWY_ASSERT(lanes && lanes2);
    std::fill(lanes.get(), lanes.get() + N, T(0));
    std::fill(lanes2.get(), lanes2.get() + N, T(0));
    const auto v = Iota(d, 1);
    const auto lo = LowerHalf(d4, LowerHalf(d2, v));
    const auto lo2 = LowerHalf(LowerHalf(v));  // optionally without D
    Store(lo, d4, lanes.get());
    Store(lo2, d4, lanes2.get());
    size_t i = 0;
    for (; i < Lanes(d4); ++i) {
      HWY_ASSERT_EQ(T(i + 1), lanes[i]);
      HWY_ASSERT_EQ(T(i + 1), lanes2[i]);
    }
    // Upper 3/4 remain unchanged
    for (; i < N; ++i) {
      HWY_ASSERT_EQ(T(0), lanes[i]);
      HWY_ASSERT_EQ(T(0), lanes2[i]);
    }
  }
};

HWY_NOINLINE void TestAllLowerHalf() {
  ForAllTypes(ForHalfVectors<TestLowerHalf>());

  // The minimum vector size is 128 bits, so there's no guarantee we can have
  // quarters of 64-bit lanes, hence test 'all' other types.
  ForHalfVectors<TestLowerQuarter, 2> test_quarter;
  ForUI8(test_quarter);
  ForUI16(test_quarter);  // exclude float16_t - cannot compare
  ForUIF32(test_quarter);
}

struct TestUpperHalf {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    // Scalar does not define UpperHalf.
#if HWY_TARGET != HWY_SCALAR
    const Half<D> d2;
    const size_t N2 = Lanes(d2);
    if (N2 < 2) return;
    HWY_ASSERT_EQ(N2 * 2, Lanes(d));
    auto expected = AllocateAligned<T>(N2);
    HWY_ASSERT(expected);
    size_t i = 0;
    for (; i < N2; ++i) {
      expected[i] = static_cast<T>(N2 + 1 + i);
    }
    HWY_ASSERT_VEC_EQ(d2, expected.get(), UpperHalf(d2, Iota(d, 1)));
#else
    (void)d;
#endif
  }
};

HWY_NOINLINE void TestAllUpperHalf() {
  ForAllTypes(ForHalfVectors<TestUpperHalf>());
}

struct TestZeroExtendVector {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const Twice<D> d2;

    const auto v = Iota(d, 1);
    const size_t N = Lanes(d);
    const size_t N2 = Lanes(d2);
    // If equal, then N was already MaxLanes(d) and it's not clear what
    // Combine or ZeroExtendVector should return.
    if (N2 == N) return;
    HWY_ASSERT(N2 == 2 * N);
    auto lanes = AllocateAligned<T>(N2);
    HWY_ASSERT(lanes);
    Store(v, d, &lanes[0]);
    Store(v, d, &lanes[N]);

    const VFromD<decltype(d2)> ext = ZeroExtendVector(d2, v);
    Store(ext, d2, lanes.get());

    // Lower half is unchanged
    HWY_ASSERT_VEC_EQ(d, v, Load(d, &lanes[0]));
    // Upper half is zero
    HWY_ASSERT_VEC_EQ(d, Zero(d), Load(d, &lanes[N]));
  }
};

HWY_NOINLINE void TestAllZeroExtendVector() {
  ForAllTypes(ForExtendableVectors<TestZeroExtendVector>());
}

struct TestCombine {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const Twice<D> d2;
    const size_t N2 = Lanes(d2);
    if (N2 < 2) return;
    auto lanes = AllocateAligned<T>(N2);
    HWY_ASSERT(lanes);

    const Vec<D> lo = Iota(d, 1);
    const Vec<D> hi = Iota(d, static_cast<T>(N2 / 2 + 1));
    const Vec<decltype(d2)> combined = Combine(d2, hi, lo);
    Store(combined, d2, lanes.get());

    const Vec<decltype(d2)> expected = Iota(d2, 1);
    HWY_ASSERT_VEC_EQ(d2, expected, combined);
  }
};

HWY_NOINLINE void TestAllCombine() {
  ForAllTypes(ForExtendableVectors<TestCombine>());
}

struct TestConcat {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);
    if (N == 1) return;
    const size_t half_bytes = N * sizeof(T) / 2;

    auto hi = AllocateAligned<T>(N);
    auto lo = AllocateAligned<T>(N);
    auto expected = AllocateAligned<T>(N);
    HWY_ASSERT(hi && lo && expected);
    RandomState rng;
    for (size_t rep = 0; rep < 10; ++rep) {
      for (size_t i = 0; i < N; ++i) {
        hi[i] = static_cast<T>(Random64(&rng) & 0xFF);
        lo[i] = static_cast<T>(Random64(&rng) & 0xFF);
      }

      {
        memcpy(&expected[N / 2], &hi[N / 2], half_bytes);
        memcpy(&expected[0], &lo[0], half_bytes);
        const auto vhi = Load(d, hi.get());
        const auto vlo = Load(d, lo.get());
        HWY_ASSERT_VEC_EQ(d, expected.get(), ConcatUpperLower(d, vhi, vlo));
      }

      {
        memcpy(&expected[N / 2], &hi[N / 2], half_bytes);
        memcpy(&expected[0], &lo[N / 2], half_bytes);
        const auto vhi = Load(d, hi.get());
        const auto vlo = Load(d, lo.get());
        HWY_ASSERT_VEC_EQ(d, expected.get(), ConcatUpperUpper(d, vhi, vlo));
      }

      {
        memcpy(&expected[N / 2], &hi[0], half_bytes);
        memcpy(&expected[0], &lo[N / 2], half_bytes);
        const auto vhi = Load(d, hi.get());
        const auto vlo = Load(d, lo.get());
        HWY_ASSERT_VEC_EQ(d, expected.get(), ConcatLowerUpper(d, vhi, vlo));
      }

      {
        memcpy(&expected[N / 2], &hi[0], half_bytes);
        memcpy(&expected[0], &lo[0], half_bytes);
        const auto vhi = Load(d, hi.get());
        const auto vlo = Load(d, lo.get());
        HWY_ASSERT_VEC_EQ(d, expected.get(), ConcatLowerLower(d, vhi, vlo));
      }
    }
  }
};

HWY_NOINLINE void TestAllConcat() {
  ForAllTypes(ForShrinkableVectors<TestConcat>());
}

struct TestConcatOddEven {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
#if HWY_TARGET != HWY_SCALAR
    const size_t N = Lanes(d);
    const auto hi = Iota(d, static_cast<T>(N));
    const auto lo = Iota(d, 0);
    const auto even = Add(Iota(d, 0), Iota(d, 0));
    const auto odd = Add(even, Set(d, 1));
    HWY_ASSERT_VEC_EQ(d, odd, ConcatOdd(d, hi, lo));
    HWY_ASSERT_VEC_EQ(d, even, ConcatEven(d, hi, lo));

    const auto v_1 = Set(d, T{1});
    const auto v_2 = Set(d, T{2});
    const auto v_3 = Set(d, T{3});
    const auto v_4 = Set(d, T{4});

    const Half<decltype(d)> dh;
    const auto v_12 = InterleaveLower(v_1, v_2); /* {1, 2, 1, 2, ...} */
    const auto v_34 = InterleaveLower(v_3, v_4); /* {3, 4, 3, 4, ...} */
    const auto v_13 =
        ConcatLowerLower(d, v_3, v_1); /* {1, 1, ..., 3, 3, ...} */
    const auto v_24 =
        ConcatLowerLower(d, v_4, v_2); /* {2, 2, ..., 4, 4, ...} */

    const auto concat_even_1234_result = ConcatEven(d, v_34, v_12);
    const auto concat_odd_1234_result = ConcatOdd(d, v_34, v_12);

    HWY_ASSERT_VEC_EQ(d, v_13, concat_even_1234_result);
    HWY_ASSERT_VEC_EQ(d, v_24, concat_odd_1234_result);
    HWY_ASSERT_VEC_EQ(dh, LowerHalf(dh, v_3),
                      UpperHalf(dh, concat_even_1234_result));
    HWY_ASSERT_VEC_EQ(dh, LowerHalf(dh, v_4),
                      UpperHalf(dh, concat_odd_1234_result));

    // This test catches inadvertent saturation.
    const auto min = Set(d, LowestValue<T>());
    const auto max = Set(d, HighestValue<T>());
    HWY_ASSERT_VEC_EQ(d, max, ConcatOdd(d, max, max));
    HWY_ASSERT_VEC_EQ(d, max, ConcatEven(d, max, max));
    HWY_ASSERT_VEC_EQ(d, min, ConcatOdd(d, min, min));
    HWY_ASSERT_VEC_EQ(d, min, ConcatEven(d, min, min));
#else
    (void)d;
#endif  // HWY_TARGET != HWY_SCALAR
  }
};

HWY_NOINLINE void TestAllConcatOddEven() {
  ForAllTypes(ForShrinkableVectors<TestConcatOddEven>());
}

class TestTruncatingResizeBitCast {
#if HWY_TARGET != HWY_SCALAR
 private:
  template <class DTo, class DFrom>
  static HWY_INLINE void DoTruncResizeBitCastTest(DTo d_to, DFrom d_from) {
    const VFromD<DFrom> v = Iota(d_from, 1);
    const VFromD<DTo> expected = Iota(d_to, 1);

    const VFromD<DTo> actual_1 = ResizeBitCast(d_to, v);
    HWY_ASSERT_VEC_EQ(d_to, expected, actual_1);

    const VFromD<DTo> actual_2 = ZeroExtendResizeBitCast(d_to, d_from, v);
    HWY_ASSERT_VEC_EQ(d_to, expected, actual_2);
  }
#endif  // HWY_TARGET != HWY_SCALAR
 public:
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
#if HWY_TARGET != HWY_SCALAR
    const Half<D> dh;
    DoTruncResizeBitCastTest(dh, d);

    const auto v_full = Iota(d, 1);
    const VFromD<decltype(dh)> expected_full_to_half = LowerHalf(dh, v_full);
    HWY_ASSERT_VEC_EQ(dh, expected_full_to_half, ResizeBitCast(dh, v_full));
    HWY_ASSERT_VEC_EQ(dh, expected_full_to_half,
                      ZeroExtendResizeBitCast(dh, d, v_full));

    constexpr size_t kMaxLanes = MaxLanes(d);
#if HWY_TARGET == HWY_RVV
    constexpr int kFromVectPow2 = DFromV<VFromD<D>>().Pow2();
    static_assert(kFromVectPow2 >= -3 && kFromVectPow2 <= 3,
                  "kFromVectPow2 must be between -3 and 3");

    constexpr size_t kScaledMaxLanes =
        HWY_MAX((kMaxLanes << 3) >> (kFromVectPow2 + 3), 1);
    constexpr int kMinPow2 = -3 + static_cast<int>(FloorLog2(sizeof(T)));
    static_assert(kMinPow2 >= -3 && kMinPow2 <= 0,
                  "kMinPow2 must be between -3 and 0");

    constexpr size_t kQuarterScaledMaxLanes = kScaledMaxLanes;
    constexpr size_t kEighthScaledMaxLanes = kScaledMaxLanes;
    constexpr int kQuarterPow2 = HWY_MAX(kFromVectPow2 - 2, kMinPow2);
    constexpr int kEighthPow2 = HWY_MAX(kFromVectPow2 - 3, kMinPow2);
#else
    constexpr size_t kQuarterScaledMaxLanes = HWY_MAX(kMaxLanes / 4, 1);
    constexpr size_t kEighthScaledMaxLanes = HWY_MAX(kMaxLanes / 8, 1);
    constexpr int kQuarterPow2 = 0;
    constexpr int kEighthPow2 = 0;
#endif

    const CappedTag<T, kQuarterScaledMaxLanes, kQuarterPow2> d_quarter;
    const CappedTag<T, kEighthScaledMaxLanes, kEighthPow2> d_eighth;
    if (MaxLanes(d_quarter) == kMaxLanes / 4) {
      DoTruncResizeBitCastTest(d_quarter, d);
      if (MaxLanes(d_eighth) == kMaxLanes / 8) {
        DoTruncResizeBitCastTest(d_eighth, d);
      }
    }
#else
    (void)d;
#endif  // HWY_TARGET != HWY_SCALAR
  }
};

HWY_NOINLINE void TestAllTruncatingResizeBitCast() {
  ForAllTypes(ForShrinkableVectors<TestTruncatingResizeBitCast>());
}

class TestExtendingResizeBitCast {
#if HWY_TARGET != HWY_SCALAR
 private:
  template <class DTo, class DFrom>
  static HWY_INLINE void DoExtResizeBitCastTest(DTo d_to, DFrom d_from) {
    const size_t N = Lanes(d_from);
    const auto active_elements_mask = FirstN(d_to, N);

    const VFromD<DTo> expected =
        IfThenElseZero(active_elements_mask, Iota(d_to, 1));
    const VFromD<DFrom> v = Iota(d_from, 1);

    const VFromD<DTo> actual_1 = ResizeBitCast(d_to, v);
    const VFromD<DTo> actual_2 = ZeroExtendResizeBitCast(d_to, d_from, v);

    HWY_ASSERT_VEC_EQ(d_to, expected,
                      IfThenElseZero(active_elements_mask, actual_1));
    HWY_ASSERT_VEC_EQ(d_to, expected, actual_2);
  }
  template <class DFrom>
  static HWY_INLINE void DoExtResizeBitCastToTwiceDTest(DFrom d_from) {
    using DTo = Twice<DFrom>;
    const DTo d_to;
    DoExtResizeBitCastTest(d_to, d_from);

    const VFromD<DFrom> v = Iota(d_from, 1);
    const VFromD<DTo> expected = ZeroExtendVector(d_to, v);

    const VFromD<DTo> actual_1 = ResizeBitCast(d_to, v);
    const VFromD<DTo> actual_2 = ZeroExtendResizeBitCast(d_to, d_from, v);

    HWY_ASSERT_VEC_EQ(d_from, v, LowerHalf(d_from, actual_1));
    HWY_ASSERT_VEC_EQ(d_from, v, LowerHalf(d_from, actual_2));
    HWY_ASSERT_VEC_EQ(d_to, expected, actual_2);
  }
#endif  // HWY_TARGET != HWY_SCALAR
 public:
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
#if HWY_TARGET != HWY_SCALAR
    DoExtResizeBitCastToTwiceDTest(d);

    constexpr size_t kMaxLanes = MaxLanes(d);
#if HWY_TARGET == HWY_RVV
    constexpr int kFromVectPow2 = DFromV<VFromD<D>>().Pow2();
    static_assert(kFromVectPow2 >= -3 && kFromVectPow2 <= 3,
                  "kFromVectPow2 must be between -3 and 3");

    constexpr size_t kScaledMaxLanes =
        HWY_MAX((kMaxLanes << 3) >> (kFromVectPow2 + 3), 1);
    constexpr size_t kQuadrupleScaledLimit = kScaledMaxLanes;
    constexpr size_t kOctupleScaledLimit = kScaledMaxLanes;
    constexpr int kQuadruplePow2 = HWY_MIN(kFromVectPow2 + 2, 3);
    constexpr int kOctuplePow2 = HWY_MIN(kFromVectPow2 + 3, 3);
#else
    constexpr size_t kQuadrupleScaledLimit = kMaxLanes * 4;
    constexpr size_t kOctupleScaledLimit = kMaxLanes * 8;
    constexpr int kQuadruplePow2 = 0;
    constexpr int kOctuplePow2 = 0;
#endif

    const CappedTag<T, kQuadrupleScaledLimit, kQuadruplePow2> d_quadruple;
    const CappedTag<T, kOctupleScaledLimit, kOctuplePow2> d_octuple;

    if (MaxLanes(d_quadruple) == kMaxLanes * 4) {
      DoExtResizeBitCastTest(d_quadruple, d);
      if (MaxLanes(d_octuple) == kMaxLanes * 8) {
        DoExtResizeBitCastTest(d_octuple, d);
      }
    }
#else
    (void)d;
#endif  // HWY_TARGET != HWY_SCALAR
  }
};

HWY_NOINLINE void TestAllExtendingResizeBitCast() {
  ForAllTypes(ForExtendableVectors<TestExtendingResizeBitCast>());
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace hwy {
HWY_BEFORE_TEST(HwyCombineTest);
HWY_EXPORT_AND_TEST_P(HwyCombineTest, TestAllLowerHalf);
HWY_EXPORT_AND_TEST_P(HwyCombineTest, TestAllUpperHalf);
HWY_EXPORT_AND_TEST_P(HwyCombineTest, TestAllZeroExtendVector);
HWY_EXPORT_AND_TEST_P(HwyCombineTest, TestAllCombine);
HWY_EXPORT_AND_TEST_P(HwyCombineTest, TestAllConcat);
HWY_EXPORT_AND_TEST_P(HwyCombineTest, TestAllConcatOddEven);
HWY_EXPORT_AND_TEST_P(HwyCombineTest, TestAllTruncatingResizeBitCast);
HWY_EXPORT_AND_TEST_P(HwyCombineTest, TestAllExtendingResizeBitCast);
}  // namespace hwy

#endif  // HWY_ONCE

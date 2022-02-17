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

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "tests/combine_test.cc"
#include "hwy/foreach_target.h"

#include "hwy/highway.h"
#include "hwy/tests/test_util-inl.h"

// Not yet implemented
#if HWY_TARGET != HWY_RVV

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
  ForAllTypes(ForDemoteVectors<TestLowerHalf>());
  ForAllTypes(ForDemoteVectors<TestLowerQuarter, 4>());
}

struct TestUpperHalf {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    // Scalar does not define UpperHalf.
#if HWY_TARGET != HWY_SCALAR
    const Half<D> d2;

    const auto v = Iota(d, 1);
    const size_t N = Lanes(d);
    auto lanes = AllocateAligned<T>(N);
    std::fill(lanes.get(), lanes.get() + N, T(0));

    Store(UpperHalf(d2, v), d2, lanes.get());
    size_t i = 0;
    for (; i < Lanes(d2); ++i) {
      HWY_ASSERT_EQ(T(Lanes(d2) + 1 + i), lanes[i]);
    }
    // Other half remains unchanged
    for (; i < N; ++i) {
      HWY_ASSERT_EQ(T(0), lanes[i]);
    }
#else
    (void)d;
#endif
  }
};

HWY_NOINLINE void TestAllUpperHalf() {
  ForAllTypes(ForShrinkableVectors<TestUpperHalf>());
}

struct TestZeroExtendVector {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const Twice<D> d2;

    const auto v = Iota(d, 1);
    const size_t N2 = Lanes(d2);
    auto lanes = AllocateAligned<T>(N2);
    Store(v, d, &lanes[0]);
    Store(v, d, &lanes[N2 / 2]);

    const auto ext = ZeroExtendVector(d2, v);
    Store(ext, d2, lanes.get());

    size_t i = 0;
    // Lower half is unchanged
    for (; i < N2 / 2; ++i) {
      HWY_ASSERT_EQ(T(1 + i), lanes[i]);
    }
    // Upper half is zero
    for (; i < N2; ++i) {
      HWY_ASSERT_EQ(T(0), lanes[i]);
    }
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
    auto lanes = AllocateAligned<T>(N2);

    const auto lo = Iota(d, 1);
    const auto hi = Iota(d, N2 / 2 + 1);
    const auto combined = Combine(d2, hi, lo);
    Store(combined, d2, lanes.get());

    const auto expected = Iota(d2, 1);
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
}  // namespace hwy

// Ought not to be necessary, but without this, no tests run on RVV.
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

#endif  // HWY_ONCE

#else
int main(int, char**) { return 0; }
#endif  // HWY_TARGET != HWY_RVV

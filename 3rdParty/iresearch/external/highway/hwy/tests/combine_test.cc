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
    std::fill(lanes.get(), lanes.get() + N, T(0));
    const auto v = Iota(d, 1);
    Store(LowerHalf(v), d2, lanes.get());
    size_t i = 0;
    for (; i < Lanes(d2); ++i) {
      HWY_ASSERT_EQ(T(1 + i), lanes[i]);
    }
    // Other half remains unchanged
    for (; i < N; ++i) {
      HWY_ASSERT_EQ(T(0), lanes[i]);
    }
  }
};

struct TestLowerQuarter {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const Half<Half<D>> d4;

    const size_t N = Lanes(d);
    auto lanes = AllocateAligned<T>(N);
    std::fill(lanes.get(), lanes.get() + N, T(0));
    const auto v = Iota(d, 1);
    const auto lo = LowerHalf(LowerHalf(v));
    Store(lo, d4, lanes.get());
    size_t i = 0;
    for (; i < Lanes(d4); ++i) {
      HWY_ASSERT_EQ(T(i + 1), lanes[i]);
    }
    // Upper 3/4 remain unchanged
    for (; i < N; ++i) {
      HWY_ASSERT_EQ(T(0), lanes[i]);
    }
  }
};

HWY_NOINLINE void TestAllLowerHalf() {
  constexpr size_t kDiv = 1;
  ForAllTypes(ForPartialVectors<TestLowerHalf, kDiv, /*kMinLanes=*/2>());
  ForAllTypes(ForPartialVectors<TestLowerQuarter, kDiv, /*kMinLanes=*/4>());
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

    Store(UpperHalf(v), d2, lanes.get());
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
  ForAllTypes(ForGE128Vectors<TestUpperHalf>());
}

struct TestZeroExtendVector {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
#if HWY_CAP_GE256
    const Twice<D> d2;

    const auto v = Iota(d, 1);
    const size_t N2 = Lanes(d2);
    auto lanes = AllocateAligned<T>(N2);
    Store(v, d, &lanes[0]);
    Store(v, d, &lanes[N2 / 2]);

    const auto ext = ZeroExtendVector(v);
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
#else
    (void)d;
#endif
  }
};

HWY_NOINLINE void TestAllZeroExtendVector() {
  ForAllTypes(ForExtendableVectors<TestZeroExtendVector>());
}

struct TestCombine {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
#if HWY_CAP_GE256
    const Twice<D> d2;
    const size_t N2 = Lanes(d2);
    auto lanes = AllocateAligned<T>(N2);

    const auto lo = Iota(d, 1);
    const auto hi = Iota(d, N2 / 2 + 1);
    const auto combined = Combine(hi, lo);
    Store(combined, d2, lanes.get());

    const auto expected = Iota(d2, 1);
    HWY_ASSERT_VEC_EQ(d2, expected, combined);
#else
    (void)d;
#endif
  }
};

HWY_NOINLINE void TestAllCombine() {
  ForAllTypes(ForExtendableVectors<TestCombine>());
}


template <int kBytes>
struct TestCombineShiftRightBytesR {
  template <class T, class D>
  HWY_NOINLINE void operator()(T t, D d) {
// Scalar does not define CombineShiftRightBytes.
#if HWY_TARGET != HWY_SCALAR || HWY_IDE
    const Repartition<uint8_t, D> d8;
    const size_t N8 = Lanes(d8);
    const auto lo = BitCast(d, Iota(d8, 1));
    const auto hi = BitCast(d, Iota(d8, 1 + N8));

    auto expected = AllocateAligned<T>(Lanes(d));
    uint8_t* expected_bytes = reinterpret_cast<uint8_t*>(expected.get());

    const size_t kBlockSize = 16;
    for (size_t i = 0; i < N8; ++i) {
      const size_t block = i / kBlockSize;
      const size_t lane = i % kBlockSize;
      const size_t first_lo = block * kBlockSize;
      const size_t idx = lane + kBytes;
      const size_t offset = (idx < kBlockSize) ? 0 : N8 - kBlockSize;
      const bool at_end = idx >= 2 * kBlockSize;
      expected_bytes[i] =
          at_end ? 0 : static_cast<uint8_t>(first_lo + idx + 1 + offset);
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(),
                      CombineShiftRightBytes<kBytes>(hi, lo));

    TestCombineShiftRightBytesR<kBytes - 1>()(t, d);
#else
    (void)t;
    (void)d;
#endif  // #if HWY_TARGET != HWY_SCALAR
  }
};

template <int kLanes>
struct TestCombineShiftRightLanesR {
  template <class T, class D>
  HWY_NOINLINE void operator()(T t, D d) {
// Scalar does not define CombineShiftRightBytes (needed for *Lanes).
#if HWY_TARGET != HWY_SCALAR || HWY_IDE
    const Repartition<uint8_t, D> d8;
    const size_t N8 = Lanes(d8);
    const auto lo = BitCast(d, Iota(d8, 1));
    const auto hi = BitCast(d, Iota(d8, 1 + N8));

    auto expected = AllocateAligned<T>(Lanes(d));

    uint8_t* expected_bytes = reinterpret_cast<uint8_t*>(expected.get());

    const size_t kBlockSize = 16;
    for (size_t i = 0; i < N8; ++i) {
      const size_t block = i / kBlockSize;
      const size_t lane = i % kBlockSize;
      const size_t first_lo = block * kBlockSize;
      const size_t idx = lane + kLanes * sizeof(T);
      const size_t offset = (idx < kBlockSize) ? 0 : N8 - kBlockSize;
      const bool at_end = idx >= 2 * kBlockSize;
      expected_bytes[i] =
          at_end ? 0 : static_cast<uint8_t>(first_lo + idx + 1 + offset);
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(),
                      CombineShiftRightLanes<kLanes>(hi, lo));

    TestCombineShiftRightBytesR<kLanes - 1>()(t, d);
#else
    (void)t;
    (void)d;
#endif  // #if HWY_TARGET != HWY_SCALAR
  }
};

template <>
struct TestCombineShiftRightBytesR<0> {
  template <class T, class D>
  void operator()(T /*unused*/, D /*unused*/) {}
};

template <>
struct TestCombineShiftRightLanesR<0> {
  template <class T, class D>
  void operator()(T /*unused*/, D /*unused*/) {}
};

struct TestCombineShiftRight {
  template <class T, class D>
  HWY_NOINLINE void operator()(T t, D d) {
    TestCombineShiftRightBytesR<15>()(t, d);
    TestCombineShiftRightLanesR<16 / sizeof(T) - 1>()(t, d);
  }
};

HWY_NOINLINE void TestAllCombineShiftRight() {
  ForAllTypes(ForGE128Vectors<TestCombineShiftRight>());
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
HWY_BEFORE_TEST(HwyCombineTest);
HWY_EXPORT_AND_TEST_P(HwyCombineTest, TestAllLowerHalf);
HWY_EXPORT_AND_TEST_P(HwyCombineTest, TestAllUpperHalf);
HWY_EXPORT_AND_TEST_P(HwyCombineTest, TestAllZeroExtendVector);
HWY_EXPORT_AND_TEST_P(HwyCombineTest, TestAllCombine);
HWY_EXPORT_AND_TEST_P(HwyCombineTest, TestAllCombineShiftRight);
HWY_AFTER_TEST();
#endif

#else
int main(int, char**) { return 0; }
#endif  // HWY_TARGET != HWY_RVV

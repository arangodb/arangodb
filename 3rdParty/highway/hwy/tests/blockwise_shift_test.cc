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
#define HWY_TARGET_INCLUDE "tests/blockwise_shift_test.cc"
#include "hwy/foreach_target.h"  // IWYU pragma: keep
#include "hwy/highway.h"
#include "hwy/tests/test_util-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

struct TestShiftBytes {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    // Scalar does not define Shift*Bytes.
#if HWY_TARGET != HWY_SCALAR || HWY_IDE
    const Repartition<uint8_t, D> du8;
    const size_t N8 = Lanes(du8);
    const size_t N = Lanes(d);

    // Zero remains zero
    const auto v0 = Zero(d);
    HWY_ASSERT_VEC_EQ(d, v0, ShiftLeftBytes<1>(v0));
    HWY_ASSERT_VEC_EQ(d, v0, ShiftLeftBytes<1>(d, v0));
    HWY_ASSERT_VEC_EQ(d, v0, ShiftRightBytes<1>(d, v0));

    auto bytes = AllocateAligned<uint8_t>(N8);
    auto in = AllocateAligned<T>(N);
    auto expected = AllocateAligned<T>(N);
    HWY_ASSERT(bytes && in && expected);

    // Zero after shifting out the high/low byte
    std::fill(bytes.get(), bytes.get() + N8, 0);
    bytes[N8 - 1] = 0x7F;
    const auto vhi = BitCast(d, Load(du8, bytes.get()));
    bytes[N8 - 1] = 0;
    bytes[0] = 0x7F;
    const auto vlo = BitCast(d, Load(du8, bytes.get()));
    HWY_ASSERT_VEC_EQ(d, v0, ShiftLeftBytes<1>(vhi));
    HWY_ASSERT_VEC_EQ(d, v0, ShiftLeftBytes<1>(d, vhi));
    HWY_ASSERT_VEC_EQ(d, v0, ShiftRightBytes<1>(d, vlo));

    // Check expected result with Iota
    const uint8_t* in_bytes = reinterpret_cast<const uint8_t*>(in.get());
    const auto v = BitCast(d, Iota(du8, 1));
    Store(v, d, in.get());

    uint8_t* expected_bytes = reinterpret_cast<uint8_t*>(expected.get());

    const size_t block_size = HWY_MIN(N8, 16);
    for (size_t block = 0; block < N8; block += block_size) {
      expected_bytes[block] = 0;
      memcpy(expected_bytes + block + 1, in_bytes + block, block_size - 1);
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), ShiftLeftBytes<1>(v));
    HWY_ASSERT_VEC_EQ(d, expected.get(), ShiftLeftBytes<1>(d, v));

    for (size_t block = 0; block < N8; block += block_size) {
      memcpy(expected_bytes + block, in_bytes + block + 1, block_size - 1);
      expected_bytes[block + block_size - 1] = 0;
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), ShiftRightBytes<1>(d, v));
#else
    (void)d;
#endif  // #if HWY_TARGET != HWY_SCALAR
  }
};

HWY_NOINLINE void TestAllShiftBytes() {
  ForIntegerTypes(ForPartialVectors<TestShiftBytes>());
}

struct TestShiftLeftLanes {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    // Scalar does not define Shift*Lanes.
#if HWY_TARGET != HWY_SCALAR || HWY_IDE
    const auto v = Iota(d, T(1));
    const size_t N = Lanes(d);
    if (N == 1) return;
    auto expected = AllocateAligned<T>(N);
    HWY_ASSERT(expected);

    HWY_ASSERT_VEC_EQ(d, v, ShiftLeftLanes<0>(v));
    HWY_ASSERT_VEC_EQ(d, v, ShiftLeftLanes<0>(d, v));

    constexpr size_t kLanesPerBlock = 16 / sizeof(T);

    for (size_t i = 0; i < N; ++i) {
      expected[i] = (i % kLanesPerBlock) == 0 ? T(0) : T(i);
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), ShiftLeftLanes<1>(v));
    HWY_ASSERT_VEC_EQ(d, expected.get(), ShiftLeftLanes<1>(d, v));
#else
    (void)d;
#endif  // #if HWY_TARGET != HWY_SCALAR
  }
};

struct TestShiftRightLanes {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    // Scalar does not define Shift*Lanes.
#if HWY_TARGET != HWY_SCALAR || HWY_IDE
    const auto v = Iota(d, T(1));
    const size_t N = Lanes(d);
    if (N == 1) return;
    auto expected = AllocateAligned<T>(N);
    HWY_ASSERT(expected);

    HWY_ASSERT_VEC_EQ(d, v, ShiftRightLanes<0>(d, v));

    constexpr size_t kLanesPerBlock = 16 / sizeof(T);

    for (size_t i = 0; i < N; ++i) {
      const size_t mod = i % kLanesPerBlock;
      expected[i] = mod == (kLanesPerBlock - 1) || i >= N - 1 ? T(0) : T(2 + i);
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), ShiftRightLanes<1>(d, v));
#else
    (void)d;
#endif  // #if HWY_TARGET != HWY_SCALAR
  }
};

HWY_NOINLINE void TestAllShiftLeftLanes() {
  ForAllTypes(ForPartialVectors<TestShiftLeftLanes>());
}

HWY_NOINLINE void TestAllShiftRightLanes() {
  ForAllTypes(ForPartialVectors<TestShiftRightLanes>());
}

// Scalar does not define CombineShiftRightBytes.
#if HWY_TARGET != HWY_SCALAR || HWY_IDE

template <int kBytes>
struct TestCombineShiftRightBytes {
  template <class T, class D>
  HWY_NOINLINE void operator()(T, D d) {
    constexpr size_t kBlockSize = 16;
    static_assert(kBytes < kBlockSize, "Shift count is per block");
    const Repartition<uint8_t, D> d8;
    const size_t N8 = Lanes(d8);
    if (N8 < 16) return;
    auto hi_bytes = AllocateAligned<uint8_t>(N8);
    auto lo_bytes = AllocateAligned<uint8_t>(N8);
    auto expected_bytes = AllocateAligned<uint8_t>(N8);
    HWY_ASSERT(hi_bytes && lo_bytes && expected_bytes);
    uint8_t combined[2 * kBlockSize];

    // Random inputs in each lane
    RandomState rng;
    for (size_t rep = 0; rep < AdjustedReps(100); ++rep) {
      for (size_t i = 0; i < N8; ++i) {
        hi_bytes[i] = static_cast<uint8_t>(Random64(&rng) & 0xFF);
        lo_bytes[i] = static_cast<uint8_t>(Random64(&rng) & 0xFF);
      }
      for (size_t i = 0; i < N8; i += kBlockSize) {
        // Arguments are not the same size.
        CopyBytes<kBlockSize>(&lo_bytes[i], combined);
        CopyBytes<kBlockSize>(&hi_bytes[i], combined + kBlockSize);
        CopyBytes<kBlockSize>(combined + kBytes, &expected_bytes[i]);
      }

      const auto hi = BitCast(d, Load(d8, hi_bytes.get()));
      const auto lo = BitCast(d, Load(d8, lo_bytes.get()));
      const auto expected = BitCast(d, Load(d8, expected_bytes.get()));
      HWY_ASSERT_VEC_EQ(d, expected, CombineShiftRightBytes<kBytes>(d, hi, lo));
    }
  }
};

template <int kLanes>
struct TestCombineShiftRightLanes {
  template <class T, class D>
  HWY_NOINLINE void operator()(T, D d) {
    const Repartition<uint8_t, D> d8;
    const size_t N8 = Lanes(d8);
    if (N8 < 16) return;

    auto hi_bytes = AllocateAligned<uint8_t>(N8);
    auto lo_bytes = AllocateAligned<uint8_t>(N8);
    auto expected_bytes = AllocateAligned<uint8_t>(N8);
    HWY_ASSERT(hi_bytes && lo_bytes && expected_bytes);
    constexpr size_t kBlockSize = 16;
    uint8_t combined[2 * kBlockSize];

    // Random inputs in each lane
    RandomState rng;
    for (size_t rep = 0; rep < AdjustedReps(100); ++rep) {
      for (size_t i = 0; i < N8; ++i) {
        hi_bytes[i] = static_cast<uint8_t>(Random64(&rng) & 0xFF);
        lo_bytes[i] = static_cast<uint8_t>(Random64(&rng) & 0xFF);
      }
      for (size_t i = 0; i < N8; i += kBlockSize) {
        // Arguments are not the same size.
        CopyBytes<kBlockSize>(&lo_bytes[i], combined);
        CopyBytes<kBlockSize>(&hi_bytes[i], combined + kBlockSize);
        CopyBytes<kBlockSize>(combined + kLanes * sizeof(T),
                              &expected_bytes[i]);
      }

      const auto hi = BitCast(d, Load(d8, hi_bytes.get()));
      const auto lo = BitCast(d, Load(d8, lo_bytes.get()));
      const auto expected = BitCast(d, Load(d8, expected_bytes.get()));
      HWY_ASSERT_VEC_EQ(d, expected, CombineShiftRightLanes<kLanes>(d, hi, lo));
    }
  }
};

#endif  // #if HWY_TARGET != HWY_SCALAR

struct TestCombineShiftRight {
  template <class T, class D>
  HWY_NOINLINE void operator()(T t, D d) {
// Scalar does not define CombineShiftRightBytes.
#if HWY_TARGET != HWY_SCALAR || HWY_IDE
    constexpr int kMaxBytes =
        HWY_MIN(16, static_cast<int>(MaxLanes(d) * sizeof(T)));
    constexpr int kMaxLanes = kMaxBytes / static_cast<int>(sizeof(T));
    TestCombineShiftRightBytes<kMaxBytes - 1>()(t, d);
    TestCombineShiftRightBytes<HWY_MAX(kMaxBytes / 2, 1)>()(t, d);
    TestCombineShiftRightBytes<1>()(t, d);

    TestCombineShiftRightLanes<kMaxLanes - 1>()(t, d);
    TestCombineShiftRightLanes<HWY_MAX(kMaxLanes / 2, -1)>()(t, d);
    TestCombineShiftRightLanes<1>()(t, d);
#else
    (void)t;
    (void)d;
#endif
  }
};

HWY_NOINLINE void TestAllCombineShiftRight() {
  // Need at least 2 lanes.
  ForAllTypes(ForShrinkableVectors<TestCombineShiftRight>());
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace hwy {
HWY_BEFORE_TEST(HwyBlockwiseShiftTest);
HWY_EXPORT_AND_TEST_P(HwyBlockwiseShiftTest, TestAllShiftBytes);
HWY_EXPORT_AND_TEST_P(HwyBlockwiseShiftTest, TestAllShiftLeftLanes);
HWY_EXPORT_AND_TEST_P(HwyBlockwiseShiftTest, TestAllShiftRightLanes);
HWY_EXPORT_AND_TEST_P(HwyBlockwiseShiftTest, TestAllCombineShiftRight);
}  // namespace hwy

#endif

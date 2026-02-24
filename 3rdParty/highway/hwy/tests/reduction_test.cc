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
#define HWY_TARGET_INCLUDE "tests/reduction_test.cc"
#include "hwy/foreach_target.h"  // IWYU pragma: keep
#include "hwy/highway.h"
#include "hwy/tests/test_util-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

struct TestSumOfLanes {
  template <typename T, size_t N, int P,
            hwy::EnableIf<!IsSigned<T>() || ((N & 1) != 0)>* = nullptr>
  HWY_NOINLINE void SignedEvenLengthVectorTests(Simd<T, N, P>) {
    // do nothing
  }
  template <typename T, size_t N, int P,
            hwy::EnableIf<IsSigned<T>() && ((N & 1) == 0)>* = nullptr>
  HWY_NOINLINE void SignedEvenLengthVectorTests(Simd<T, N, P> d) {
    const T pairs = static_cast<T>(Lanes(d) / 2);

    // Lanes are the repeated sequence -2, 1, [...]; each pair sums to -1,
    // so the eventual total is just -(N/2).
    Vec<decltype(d)> v =
        InterleaveLower(Set(d, static_cast<T>(-2)), Set(d, T{1}));
    HWY_ASSERT_VEC_EQ(d, Set(d, static_cast<T>(-pairs)), SumOfLanes(d, v));
    HWY_ASSERT_EQ(static_cast<T>(-pairs), ReduceSum(d, v));

    // Similar test with a positive result.
    v = InterleaveLower(Set(d, static_cast<T>(-2)), Set(d, T{4}));
    HWY_ASSERT_VEC_EQ(d, Set(d, static_cast<T>(pairs * 2)), SumOfLanes(d, v));
    HWY_ASSERT_EQ(static_cast<T>(pairs * 2), ReduceSum(d, v));
  }

  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);
    auto in_lanes = AllocateAligned<T>(N);
    HWY_ASSERT(in_lanes);

    // Lane i = bit i, higher lanes 0
    double sum = 0.0;
    // Avoid setting sign bit and cap at double precision
    constexpr size_t kBits = HWY_MIN(sizeof(T) * 8 - 1, 51);
    for (size_t i = 0; i < N; ++i) {
      in_lanes[i] = i < kBits ? static_cast<T>(1ull << i) : static_cast<T>(0);
      sum += static_cast<double>(in_lanes[i]);
    }
    HWY_ASSERT_VEC_EQ(d, Set(d, T(sum)),
                      SumOfLanes(d, Load(d, in_lanes.get())));
    HWY_ASSERT_EQ(T(sum), ReduceSum(d, Load(d, in_lanes.get())));
    // Lane i = i (iota) to include upper lanes
    sum = 0.0;
    for (size_t i = 0; i < N; ++i) {
      sum += static_cast<double>(i);
    }
    HWY_ASSERT_VEC_EQ(d, Set(d, T(sum)), SumOfLanes(d, Iota(d, 0)));
    HWY_ASSERT_EQ(T(sum), ReduceSum(d, Iota(d, 0)));

    // Run more tests only for signed types with even vector lengths. Some of
    // this code may not otherwise compile, so put it in a templated function.
    SignedEvenLengthVectorTests(d);
  }
};

HWY_NOINLINE void TestAllSumOfLanes() {
  ForUIF3264(ForPartialVectors<TestSumOfLanes>());
  ForUI16(ForPartialVectors<TestSumOfLanes>());

// UI8 is only implemented for some targets.
#if HWY_MAX_BYTES == 16 && (HWY_ARCH_ARM || HWY_ARCH_X86)
  ForUI8(ForGEVectors<64, TestSumOfLanes>());
#endif
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
      in_lanes[i] = i < kBits ? static_cast<T>(1ull << i) : static_cast<T>(2);
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

    // Bug #910: also check negative values
    min = HighestValue<T>();
    const T input_copy[] = {static_cast<T>(-1),
                            static_cast<T>(-2),
                            1,
                            2,
                            3,
                            4,
                            5,
                            6,
                            7,
                            8,
                            9,
                            10,
                            11,
                            12,
                            13,
                            14};
    size_t i = 0;
    for (; i < HWY_MIN(N, sizeof(input_copy) / sizeof(T)); ++i) {
      in_lanes[i] = input_copy[i];
      min = HWY_MIN(min, input_copy[i]);
    }
    // Pad with neutral element to full vector (so we can load)
    for (; i < N; ++i) {
      in_lanes[i] = min;
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
      in_lanes[i] = i < kBits ? static_cast<T>(1ull << i) : static_cast<T>(0);
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

    // Bug #910: also check negative values
    max = LowestValue<T>();
    const T input_copy[] = {static_cast<T>(-1),
                            static_cast<T>(-2),
                            1,
                            2,
                            3,
                            4,
                            5,
                            6,
                            7,
                            8,
                            9,
                            10,
                            11,
                            12,
                            13,
                            14};
    size_t i = 0;
    for (; i < HWY_MIN(N, sizeof(input_copy) / sizeof(T)); ++i) {
      in_lanes[i] = input_copy[i];
      max = HWY_MAX(max, in_lanes[i]);
    }
    // Pad with neutral element to full vector (so we can load)
    for (; i < N; ++i) {
      in_lanes[i] = max;
    }
    HWY_ASSERT_VEC_EQ(d, Set(d, max), MaxOfLanes(d, Load(d, in_lanes.get())));
  }
};

HWY_NOINLINE void TestAllMinMaxOfLanes() {
  const ForPartialVectors<TestMinOfLanes> test_min;
  const ForPartialVectors<TestMaxOfLanes> test_max;
  ForUIF3264(test_min);
  ForUIF3264(test_max);
  ForUI16(test_min);
  ForUI16(test_max);

// UI8 is only implemented for some targets.
#if HWY_MAX_BYTES == 16 && (HWY_ARCH_ARM || HWY_ARCH_X86)
  ForUI8(ForGEVectors<64, TestMinOfLanes>());
  ForUI8(ForGEVectors<64, TestMaxOfLanes>());
#endif
}

struct TestSumsOf8 {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    RandomState rng;

    const size_t N = Lanes(d);
    if (N < 8) return;
    const Repartition<uint64_t, D> du64;

    auto in_lanes = AllocateAligned<T>(N);
    auto sum_lanes = AllocateAligned<uint64_t>(N / 8);

    for (size_t rep = 0; rep < 100; ++rep) {
      for (size_t i = 0; i < N; ++i) {
        in_lanes[i] = Random64(&rng) & 0xFF;
      }

      for (size_t idx_sum = 0; idx_sum < N / 8; ++idx_sum) {
        uint64_t sum = 0;
        for (size_t i = 0; i < 8; ++i) {
          sum += in_lanes[idx_sum * 8 + i];
        }
        sum_lanes[idx_sum] = sum;
      }

      const Vec<D> in = Load(d, in_lanes.get());
      HWY_ASSERT_VEC_EQ(du64, sum_lanes.get(), SumsOf8(in));
    }
  }
};

HWY_NOINLINE void TestAllSumsOf8() {
  ForGEVectors<64, TestSumsOf8>()(uint8_t());
}

struct TestSumsOf8AbsDiff {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    RandomState rng;

    const size_t N = Lanes(d);
    if (N < 8) return;
    const Repartition<uint64_t, D> du64;

    auto in_lanes_a = AllocateAligned<T>(N);
    auto in_lanes_b = AllocateAligned<T>(N);
    auto sum_lanes = AllocateAligned<uint64_t>(N / 8);

    for (size_t rep = 0; rep < 100; ++rep) {
      for (size_t i = 0; i < N; ++i) {
        uint64_t rand64_val = Random64(&rng);
        in_lanes_a[i] = rand64_val & 0xFF;
        in_lanes_b[i] = (rand64_val >> 8) & 0xFF;
      }

      for (size_t idx_sum = 0; idx_sum < N / 8; ++idx_sum) {
        uint64_t sum = 0;
        for (size_t i = 0; i < 8; ++i) {
          const auto lane_diff =
            static_cast<int16_t>(in_lanes_a[idx_sum * 8 + i]) -
            static_cast<int16_t>(in_lanes_b[idx_sum * 8 + i]);
          sum +=
            static_cast<uint64_t>((lane_diff >= 0) ? lane_diff : -lane_diff);
        }
        sum_lanes[idx_sum] = sum;
      }

      const Vec<D> a = Load(d, in_lanes_a.get());
      const Vec<D> b = Load(d, in_lanes_b.get());
      HWY_ASSERT_VEC_EQ(du64, sum_lanes.get(), SumsOf8AbsDiff(a, b));
    }
  }
};

HWY_NOINLINE void TestAllSumsOf8AbsDiff() {
  ForGEVectors<64, TestSumsOf8AbsDiff>()(uint8_t());
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace hwy {
HWY_BEFORE_TEST(HwyReductionTest);
HWY_EXPORT_AND_TEST_P(HwyReductionTest, TestAllSumOfLanes);
HWY_EXPORT_AND_TEST_P(HwyReductionTest, TestAllMinMaxOfLanes);
HWY_EXPORT_AND_TEST_P(HwyReductionTest, TestAllSumsOf8);
HWY_EXPORT_AND_TEST_P(HwyReductionTest, TestAllSumsOf8AbsDiff);
}  // namespace hwy

#endif

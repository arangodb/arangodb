// Copyright 2021 Google LLC
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "hwy/aligned_allocator.h"

// clang-format off
#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "hwy/contrib/dot/dot_test.cc"
#include "hwy/foreach_target.h"  // IWYU pragma: keep
#include "hwy/highway.h"
#include "hwy/contrib/dot/dot-inl.h"
#include "hwy/tests/test_util-inl.h"
// clang-format on

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

template <typename T>
HWY_NOINLINE T SimpleDot(const T* pa, const T* pb, size_t num) {
  double sum = 0.0;
  for (size_t i = 0; i < num; ++i) {
    sum += pa[i] * pb[i];
  }
  return static_cast<T>(sum);
}

HWY_NOINLINE float SimpleDot(const bfloat16_t* pa, const bfloat16_t* pb,
                             size_t num) {
  float sum = 0.0f;
  for (size_t i = 0; i < num; ++i) {
    sum += F32FromBF16(pa[i]) * F32FromBF16(pb[i]);
  }
  return sum;
}

template <typename T>
void SetValue(const float value, T* HWY_RESTRICT ptr) {
  *ptr = static_cast<T>(value);
}
void SetValue(const float value, bfloat16_t* HWY_RESTRICT ptr) {
  *ptr = BF16FromF32(value);
}

class TestDot {
  // Computes/verifies one dot product.
  template <int kAssumptions, class D>
  void Test(D d, size_t num, size_t misalign_a, size_t misalign_b,
            RandomState& rng) {
    using T = TFromD<D>;
    const size_t N = Lanes(d);
    const auto random_t = [&rng]() {
      const int32_t bits = static_cast<int32_t>(Random32(&rng)) & 1023;
      return static_cast<float>(bits - 512) * (1.0f / 64);
    };

    const size_t padded =
        (kAssumptions & Dot::kPaddedToVector) ? RoundUpTo(num, N) : num;
    AlignedFreeUniquePtr<T[]> pa = AllocateAligned<T>(misalign_a + padded);
    AlignedFreeUniquePtr<T[]> pb = AllocateAligned<T>(misalign_b + padded);
    HWY_ASSERT(pa && pb);
    T* a = pa.get() + misalign_a;
    T* b = pb.get() + misalign_b;
    size_t i = 0;
    for (; i < num; ++i) {
      SetValue(random_t(), a + i);
      SetValue(random_t(), b + i);
    }
    // Fill padding with NaN - the values are not used, but avoids MSAN errors.
    for (; i < padded; ++i) {
      ScalableTag<float> df1;
      SetValue(GetLane(NaN(df1)), a + i);
      SetValue(GetLane(NaN(df1)), b + i);
    }

    const auto expected = SimpleDot(a, b, num);
    const auto actual = Dot::Compute<kAssumptions>(d, a, b, num);
    const auto max = static_cast<decltype(actual)>(8 * 8 * num);
    HWY_ASSERT(-max <= actual && actual <= max);
    HWY_ASSERT(expected - 1E-4 <= actual && actual <= expected + 1E-4);
  }

  // Runs tests with various alignments.
  template <int kAssumptions, class D>
  void ForeachMisalign(D d, size_t num, RandomState& rng) {
    const size_t N = Lanes(d);
    const size_t misalignments[3] = {0, N / 4, 3 * N / 5};
    for (size_t ma : misalignments) {
      for (size_t mb : misalignments) {
        Test<kAssumptions>(d, num, ma, mb, rng);
      }
    }
  }

  // Runs tests with various lengths compatible with the given assumptions.
  template <int kAssumptions, class D>
  void ForeachCount(D d, RandomState& rng) {
    const size_t N = Lanes(d);
    const size_t counts[] = {1,
                             3,
                             7,
                             16,
                             HWY_MAX(N / 2, 1),
                             HWY_MAX(2 * N / 3, 1),
                             N,
                             N + 1,
                             4 * N / 3,
                             3 * N,
                             8 * N,
                             8 * N + 2};
    for (size_t num : counts) {
      if ((kAssumptions & Dot::kAtLeastOneVector) && num < N) continue;
      if ((kAssumptions & Dot::kMultipleOfVector) && (num % N) != 0) continue;
      ForeachMisalign<kAssumptions>(d, num, rng);
    }
  }

 public:
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    RandomState rng;

    // All 8 combinations of the three length-related flags:
    ForeachCount<0>(d, rng);
    ForeachCount<Dot::kAtLeastOneVector>(d, rng);
    ForeachCount<Dot::kMultipleOfVector>(d, rng);
    ForeachCount<Dot::kMultipleOfVector | Dot::kAtLeastOneVector>(d, rng);
    ForeachCount<Dot::kPaddedToVector>(d, rng);
    ForeachCount<Dot::kPaddedToVector | Dot::kAtLeastOneVector>(d, rng);
    ForeachCount<Dot::kPaddedToVector | Dot::kMultipleOfVector>(d, rng);
    ForeachCount<Dot::kPaddedToVector | Dot::kMultipleOfVector |
                 Dot::kAtLeastOneVector>(d, rng);
  }
};

void TestAllDot() { ForFloatTypes(ForPartialVectors<TestDot>()); }
void TestAllDotBF16() { ForShrinkableVectors<TestDot>()(bfloat16_t()); }

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace hwy {
HWY_BEFORE_TEST(DotTest);
HWY_EXPORT_AND_TEST_P(DotTest, TestAllDot);
HWY_EXPORT_AND_TEST_P(DotTest, TestAllDotBF16);
}  // namespace hwy

#endif

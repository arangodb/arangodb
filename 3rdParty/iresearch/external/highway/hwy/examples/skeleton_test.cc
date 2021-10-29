// Copyright 2020 Google LLC
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

// Example of unit test for the "skeleton" library.

#include "hwy/examples/skeleton.h"

#include <stdio.h>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "examples/skeleton_test.cc"
#include "hwy/foreach_target.h"
#include "hwy/highway.h"
#include "hwy/tests/test_util-inl.h"

// Optional: factor out parts of the implementation into *-inl.h
#include "hwy/examples/skeleton-inl.h"

HWY_BEFORE_NAMESPACE();
namespace skeleton {
namespace HWY_NAMESPACE {

using namespace hwy::HWY_NAMESPACE;

// Calls function defined in skeleton.cc.
struct TestFloorLog2 {
  template <class T, class DF>
  HWY_NOINLINE void operator()(T /*unused*/, DF df) {
    const size_t count = 5 * Lanes(df);
    auto in = hwy::AllocateAligned<uint8_t>(count);
    auto expected = hwy::AllocateAligned<uint8_t>(count);

    hwy::RandomState rng;
    for (size_t i = 0; i < count; ++i) {
      expected[i] = Random32(&rng) & 7;
      in[i] = static_cast<uint8_t>(1u << expected[i]);
    }
    auto out = hwy::AllocateAligned<uint8_t>(count);
    CallFloorLog2(in.get(), count, out.get());
    int sum = 0;
    for (size_t i = 0; i < count; ++i) {
      // TODO(janwas): implement
#if HWY_TARGET != HWY_RVV
      HWY_ASSERT_EQ(expected[i], out[i]);
#endif
      sum += out[i];
    }
    hwy::PreventElision(sum);
  }
};

HWY_NOINLINE void TestAllFloorLog2() {
  ForPartialVectors<TestFloorLog2>()(float());
}

// Calls function defined in skeleton-inl.h.
struct TestSumMulAdd {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    hwy::RandomState rng;
    const size_t count = 4096;
    EXPECT_TRUE(count % Lanes(d) == 0);
    auto mul = hwy::AllocateAligned<T>(count);
    auto x = hwy::AllocateAligned<T>(count);
    auto add = hwy::AllocateAligned<T>(count);
    for (size_t i = 0; i < count; ++i) {
      mul[i] = static_cast<T>(Random32(&rng) & 0xF);
      x[i] = static_cast<T>(Random32(&rng) & 0xFF);
      add[i] = static_cast<T>(Random32(&rng) & 0xFF);
    }
    double expected_sum = 0.0;
    for (size_t i = 0; i < count; ++i) {
      expected_sum += mul[i] * x[i] + add[i];
    }

    MulAddLoop(d, mul.get(), add.get(), count, x.get());
    HWY_ASSERT_EQ(4344240.0, expected_sum);
  }
};

HWY_NOINLINE void TestAllSumMulAdd() {
  ForFloatTypes(ForPartialVectors<TestSumMulAdd>());
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace skeleton
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace skeleton {
HWY_BEFORE_TEST(SkeletonTest);
HWY_EXPORT_AND_TEST_P(SkeletonTest, TestAllFloorLog2);
HWY_EXPORT_AND_TEST_P(SkeletonTest, TestAllSumMulAdd);
}  // namespace skeleton

// Ought not to be necessary, but without this, no tests run on RVV.
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

#endif

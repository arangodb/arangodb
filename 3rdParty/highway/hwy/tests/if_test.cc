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
#define HWY_TARGET_INCLUDE "tests/if_test.cc"
#include "hwy/foreach_target.h"  // IWYU pragma: keep
#include "hwy/highway.h"
#include "hwy/tests/test_util-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

struct TestIfThenElse {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    RandomState rng;

    using TI = MakeSigned<T>;  // For mask > 0 comparison
    const Rebind<TI, D> di;
    const size_t N = Lanes(d);
    auto in1 = AllocateAligned<T>(N);
    auto in2 = AllocateAligned<T>(N);
    auto bool_lanes = AllocateAligned<TI>(N);
    auto expected = AllocateAligned<T>(N);
    HWY_ASSERT(in1 && in2 && bool_lanes && expected);

    // Each lane should have a chance of having mask=true.
    for (size_t rep = 0; rep < AdjustedReps(200); ++rep) {
      for (size_t i = 0; i < N; ++i) {
        in1[i] = static_cast<T>(Random32(&rng));
        in2[i] = static_cast<T>(Random32(&rng));
        bool_lanes[i] = (Random32(&rng) & 16) ? TI(1) : TI(0);
      }

      const auto v1 = Load(d, in1.get());
      const auto v2 = Load(d, in2.get());
      const auto mask = RebindMask(d, Gt(Load(di, bool_lanes.get()), Zero(di)));

      for (size_t i = 0; i < N; ++i) {
        expected[i] = bool_lanes[i] ? in1[i] : in2[i];
      }
      HWY_ASSERT_VEC_EQ(d, expected.get(), IfThenElse(mask, v1, v2));

      for (size_t i = 0; i < N; ++i) {
        expected[i] = bool_lanes[i] ? in1[i] : T(0);
      }
      HWY_ASSERT_VEC_EQ(d, expected.get(), IfThenElseZero(mask, v1));

      for (size_t i = 0; i < N; ++i) {
        expected[i] = bool_lanes[i] ? T(0) : in2[i];
      }
      HWY_ASSERT_VEC_EQ(d, expected.get(), IfThenZeroElse(mask, v2));
    }
  }
};

HWY_NOINLINE void TestAllIfThenElse() {
  ForAllTypes(ForPartialVectors<TestIfThenElse>());
}

struct TestIfVecThenElse {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    RandomState rng;

    using TU = MakeUnsigned<T>;  // For all-one mask
    const Rebind<TU, D> du;
    const size_t N = Lanes(d);
    auto in1 = AllocateAligned<T>(N);
    auto in2 = AllocateAligned<T>(N);
    auto vec_lanes = AllocateAligned<TU>(N);
    auto expected = AllocateAligned<T>(N);
    HWY_ASSERT(in1 && in2 && vec_lanes && expected);

    // Each lane should have a chance of having mask=true.
    for (size_t rep = 0; rep < AdjustedReps(200); ++rep) {
      for (size_t i = 0; i < N; ++i) {
        in1[i] = static_cast<T>(Random32(&rng));
        in2[i] = static_cast<T>(Random32(&rng));
        vec_lanes[i] = (Random32(&rng) & 16) ? static_cast<TU>(~TU(0)) : TU(0);
      }

      const auto v1 = Load(d, in1.get());
      const auto v2 = Load(d, in2.get());
      const auto vec = BitCast(d, Load(du, vec_lanes.get()));

      for (size_t i = 0; i < N; ++i) {
        expected[i] = vec_lanes[i] ? in1[i] : in2[i];
      }
      HWY_ASSERT_VEC_EQ(d, expected.get(), IfVecThenElse(vec, v1, v2));
    }
  }
};

HWY_NOINLINE void TestAllIfVecThenElse() {
  ForAllTypes(ForPartialVectors<TestIfVecThenElse>());
}

struct TestZeroIfNegative {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v0 = Zero(d);
    const auto vp = Iota(d, 1);
    const auto vn = Iota(d, T(-1E5));  // assumes N < 10^5

    // Zero and positive remain unchanged
    HWY_ASSERT_VEC_EQ(d, v0, ZeroIfNegative(v0));
    HWY_ASSERT_VEC_EQ(d, vp, ZeroIfNegative(vp));

    // Negative are all replaced with zero
    HWY_ASSERT_VEC_EQ(d, v0, ZeroIfNegative(vn));
  }
};

HWY_NOINLINE void TestAllZeroIfNegative() {
  ForFloatTypes(ForPartialVectors<TestZeroIfNegative>());
}

struct TestIfNegative {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v0 = Zero(d);
    const auto vp = Iota(d, 1);
    const auto vn = Or(vp, SignBit(d));

    // Zero and positive remain unchanged
    HWY_ASSERT_VEC_EQ(d, v0, IfNegativeThenElse(v0, vn, v0));
    HWY_ASSERT_VEC_EQ(d, vn, IfNegativeThenElse(v0, v0, vn));
    HWY_ASSERT_VEC_EQ(d, vp, IfNegativeThenElse(vp, vn, vp));
    HWY_ASSERT_VEC_EQ(d, vn, IfNegativeThenElse(vp, vp, vn));

    // Negative are replaced with 2nd arg
    HWY_ASSERT_VEC_EQ(d, v0, IfNegativeThenElse(vn, v0, vp));
    HWY_ASSERT_VEC_EQ(d, vn, IfNegativeThenElse(vn, vn, v0));
    HWY_ASSERT_VEC_EQ(d, vp, IfNegativeThenElse(vn, vp, vn));
  }
};

HWY_NOINLINE void TestAllIfNegative() {
  ForFloatTypes(ForPartialVectors<TestIfNegative>());
  ForSignedTypes(ForPartialVectors<TestIfNegative>());
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace hwy {
HWY_BEFORE_TEST(HwyIfTest);
HWY_EXPORT_AND_TEST_P(HwyIfTest, TestAllIfThenElse);
HWY_EXPORT_AND_TEST_P(HwyIfTest, TestAllIfVecThenElse);
HWY_EXPORT_AND_TEST_P(HwyIfTest, TestAllZeroIfNegative);
HWY_EXPORT_AND_TEST_P(HwyIfTest, TestAllIfNegative);
}  // namespace hwy

#endif

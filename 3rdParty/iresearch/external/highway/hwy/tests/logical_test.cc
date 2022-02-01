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
#include <string.h>  // memcmp

#include "hwy/aligned_allocator.h"
#include "hwy/base.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "tests/logical_test.cc"
#include "hwy/foreach_target.h"
#include "hwy/highway.h"
#include "hwy/tests/test_util-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

struct TestLogicalInteger {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v0 = Zero(d);
    const auto vi = Iota(d, 0);
    const auto ones = VecFromMask(d, Eq(v0, v0));
    const auto v1 = Set(d, 1);
    const auto vnot1 = Set(d, T(~T(1)));

    HWY_ASSERT_VEC_EQ(d, v0, Not(ones));
    HWY_ASSERT_VEC_EQ(d, ones, Not(v0));
    HWY_ASSERT_VEC_EQ(d, v1, Not(vnot1));
    HWY_ASSERT_VEC_EQ(d, vnot1, Not(v1));

    HWY_ASSERT_VEC_EQ(d, v0, And(v0, vi));
    HWY_ASSERT_VEC_EQ(d, v0, And(vi, v0));
    HWY_ASSERT_VEC_EQ(d, vi, And(vi, vi));

    HWY_ASSERT_VEC_EQ(d, vi, Or(v0, vi));
    HWY_ASSERT_VEC_EQ(d, vi, Or(vi, v0));
    HWY_ASSERT_VEC_EQ(d, vi, Or(vi, vi));

    HWY_ASSERT_VEC_EQ(d, vi, Xor(v0, vi));
    HWY_ASSERT_VEC_EQ(d, vi, Xor(vi, v0));
    HWY_ASSERT_VEC_EQ(d, v0, Xor(vi, vi));

    HWY_ASSERT_VEC_EQ(d, vi, AndNot(v0, vi));
    HWY_ASSERT_VEC_EQ(d, v0, AndNot(vi, v0));
    HWY_ASSERT_VEC_EQ(d, v0, AndNot(vi, vi));

    auto v = vi;
    v = And(v, vi);
    HWY_ASSERT_VEC_EQ(d, vi, v);
    v = And(v, v0);
    HWY_ASSERT_VEC_EQ(d, v0, v);

    v = Or(v, vi);
    HWY_ASSERT_VEC_EQ(d, vi, v);
    v = Or(v, v0);
    HWY_ASSERT_VEC_EQ(d, vi, v);

    v = Xor(v, vi);
    HWY_ASSERT_VEC_EQ(d, v0, v);
    v = Xor(v, v0);
    HWY_ASSERT_VEC_EQ(d, v0, v);
  }
};

HWY_NOINLINE void TestAllLogicalInteger() {
  ForIntegerTypes(ForPartialVectors<TestLogicalInteger>());
}

struct TestLogicalFloat {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v0 = Zero(d);
    const auto vi = Iota(d, 0);

    HWY_ASSERT_VEC_EQ(d, v0, And(v0, vi));
    HWY_ASSERT_VEC_EQ(d, v0, And(vi, v0));
    HWY_ASSERT_VEC_EQ(d, vi, And(vi, vi));

    HWY_ASSERT_VEC_EQ(d, vi, Or(v0, vi));
    HWY_ASSERT_VEC_EQ(d, vi, Or(vi, v0));
    HWY_ASSERT_VEC_EQ(d, vi, Or(vi, vi));

    HWY_ASSERT_VEC_EQ(d, vi, Xor(v0, vi));
    HWY_ASSERT_VEC_EQ(d, vi, Xor(vi, v0));
    HWY_ASSERT_VEC_EQ(d, v0, Xor(vi, vi));

    HWY_ASSERT_VEC_EQ(d, vi, AndNot(v0, vi));
    HWY_ASSERT_VEC_EQ(d, v0, AndNot(vi, v0));
    HWY_ASSERT_VEC_EQ(d, v0, AndNot(vi, vi));

    auto v = vi;
    v = And(v, vi);
    HWY_ASSERT_VEC_EQ(d, vi, v);
    v = And(v, v0);
    HWY_ASSERT_VEC_EQ(d, v0, v);

    v = Or(v, vi);
    HWY_ASSERT_VEC_EQ(d, vi, v);
    v = Or(v, v0);
    HWY_ASSERT_VEC_EQ(d, vi, v);

    v = Xor(v, vi);
    HWY_ASSERT_VEC_EQ(d, v0, v);
    v = Xor(v, v0);
    HWY_ASSERT_VEC_EQ(d, v0, v);
  }
};

HWY_NOINLINE void TestAllLogicalFloat() {
  ForFloatTypes(ForPartialVectors<TestLogicalFloat>());
}

struct TestCopySign {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v0 = Zero(d);
    const auto vp = Iota(d, 1);
    const auto vn = Iota(d, T(-1E5));  // assumes N < 10^5

    // Zero remains zero regardless of sign
    HWY_ASSERT_VEC_EQ(d, v0, CopySign(v0, v0));
    HWY_ASSERT_VEC_EQ(d, v0, CopySign(v0, vp));
    HWY_ASSERT_VEC_EQ(d, v0, CopySign(v0, vn));
    HWY_ASSERT_VEC_EQ(d, v0, CopySignToAbs(v0, v0));
    HWY_ASSERT_VEC_EQ(d, v0, CopySignToAbs(v0, vp));
    HWY_ASSERT_VEC_EQ(d, v0, CopySignToAbs(v0, vn));

    // Positive input, positive sign => unchanged
    HWY_ASSERT_VEC_EQ(d, vp, CopySign(vp, vp));
    HWY_ASSERT_VEC_EQ(d, vp, CopySignToAbs(vp, vp));

    // Positive input, negative sign => negated
    HWY_ASSERT_VEC_EQ(d, Neg(vp), CopySign(vp, vn));
    HWY_ASSERT_VEC_EQ(d, Neg(vp), CopySignToAbs(vp, vn));

    // Negative input, negative sign => unchanged
    HWY_ASSERT_VEC_EQ(d, vn, CopySign(vn, vn));

    // Negative input, positive sign => negated
    HWY_ASSERT_VEC_EQ(d, Neg(vn), CopySign(vn, vp));
  }
};

HWY_NOINLINE void TestAllCopySign() {
  ForFloatTypes(ForPartialVectors<TestCopySign>());
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

struct TestBroadcastSignBit {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto s0 = Zero(d);
    const auto s1 = Set(d, -1);  // all bit set
    const auto vpos = And(Iota(d, 0), Set(d, LimitsMax<T>()));
    const auto vneg = Sub(s1, vpos);

    HWY_ASSERT_VEC_EQ(d, s0, BroadcastSignBit(vpos));
    HWY_ASSERT_VEC_EQ(d, s0, BroadcastSignBit(Set(d, LimitsMax<T>())));

    HWY_ASSERT_VEC_EQ(d, s1, BroadcastSignBit(vneg));
    HWY_ASSERT_VEC_EQ(d, s1, BroadcastSignBit(Set(d, LimitsMin<T>())));
    HWY_ASSERT_VEC_EQ(d, s1, BroadcastSignBit(Set(d, LimitsMin<T>() / 2)));
  }
};

HWY_NOINLINE void TestAllBroadcastSignBit() {
  ForSignedTypes(ForPartialVectors<TestBroadcastSignBit>());
}

struct TestTestBit {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t kNumBits = sizeof(T) * 8;
    for (size_t i = 0; i < kNumBits; ++i) {
      const auto bit1 = Set(d, T(1ull << i));
      const auto bit2 = Set(d, T(1ull << ((i + 1) % kNumBits)));
      const auto bit3 = Set(d, T(1ull << ((i + 2) % kNumBits)));
      const auto bits12 = Or(bit1, bit2);
      const auto bits23 = Or(bit2, bit3);
      HWY_ASSERT(AllTrue(d, TestBit(bit1, bit1)));
      HWY_ASSERT(AllTrue(d, TestBit(bits12, bit1)));
      HWY_ASSERT(AllTrue(d, TestBit(bits12, bit2)));

      HWY_ASSERT(AllFalse(d, TestBit(bits12, bit3)));
      HWY_ASSERT(AllFalse(d, TestBit(bits23, bit1)));
      HWY_ASSERT(AllFalse(d, TestBit(bit1, bit2)));
      HWY_ASSERT(AllFalse(d, TestBit(bit2, bit1)));
      HWY_ASSERT(AllFalse(d, TestBit(bit1, bit3)));
      HWY_ASSERT(AllFalse(d, TestBit(bit3, bit1)));
      HWY_ASSERT(AllFalse(d, TestBit(bit2, bit3)));
      HWY_ASSERT(AllFalse(d, TestBit(bit3, bit2)));
    }
  }
};

HWY_NOINLINE void TestAllTestBit() {
  ForIntegerTypes(ForPartialVectors<TestTestBit>());
}

struct TestPopulationCount {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
#if HWY_TARGET == HWY_RVV || HWY_IS_DEBUG_BUILD
    constexpr size_t kNumTests = 1 << 14;
#else
    constexpr size_t kNumTests = 1 << 20;
#endif
    RandomState rng;
    size_t N = Lanes(d);
    auto data = AllocateAligned<T>(N);
    auto popcnt = AllocateAligned<T>(N);
    for (size_t i = 0; i < kNumTests / N; i++) {
      for (size_t i = 0; i < N; i++) {
        data[i] = static_cast<T>(rng());
        popcnt[i] = static_cast<T>(PopCount(data[i]));
      }
      HWY_ASSERT_VEC_EQ(d, popcnt.get(), PopulationCount(Load(d, data.get())));
    }
  }
};

HWY_NOINLINE void TestAllPopulationCount() {
  ForUnsignedTypes(ForPartialVectors<TestPopulationCount>());
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace hwy {
HWY_BEFORE_TEST(HwyLogicalTest);
HWY_EXPORT_AND_TEST_P(HwyLogicalTest, TestAllLogicalInteger);
HWY_EXPORT_AND_TEST_P(HwyLogicalTest, TestAllLogicalFloat);
HWY_EXPORT_AND_TEST_P(HwyLogicalTest, TestAllCopySign);
HWY_EXPORT_AND_TEST_P(HwyLogicalTest, TestAllZeroIfNegative);
HWY_EXPORT_AND_TEST_P(HwyLogicalTest, TestAllBroadcastSignBit);
HWY_EXPORT_AND_TEST_P(HwyLogicalTest, TestAllTestBit);
HWY_EXPORT_AND_TEST_P(HwyLogicalTest, TestAllPopulationCount);
}  // namespace hwy

// Ought not to be necessary, but without this, no tests run on RVV.
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

#endif

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
#define HWY_TARGET_INCLUDE "tests/mul_test.cc"
#include "hwy/foreach_target.h"  // IWYU pragma: keep
#include "hwy/highway.h"
#include "hwy/tests/test_util-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

template <size_t kBits>
constexpr uint64_t FirstBits() {
  return (1ull << kBits) - 1;
}
template <>
constexpr uint64_t FirstBits<64>() {
  return ~uint64_t{0};
}

struct TestUnsignedMul {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v0 = Zero(d);
    const auto v1 = Set(d, T(1));
    const auto vi = Iota(d, 1);
    const auto vj = Iota(d, 3);
    const size_t N = Lanes(d);
    auto expected = AllocateAligned<T>(N);
    HWY_ASSERT(expected);

    HWY_ASSERT_VEC_EQ(d, v0, Mul(v0, v0));
    HWY_ASSERT_VEC_EQ(d, v1, Mul(v1, v1));
    HWY_ASSERT_VEC_EQ(d, vi, Mul(v1, vi));
    HWY_ASSERT_VEC_EQ(d, vi, Mul(vi, v1));

    for (size_t i = 0; i < N; ++i) {
      expected[i] = static_cast<T>((1 + i) * (1 + i));
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), Mul(vi, vi));

    for (size_t i = 0; i < N; ++i) {
      expected[i] = static_cast<T>((1 + i) * (3 + i));
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), Mul(vi, vj));

    const T max = LimitsMax<T>();
    const auto vmax = Set(d, max);
    HWY_ASSERT_VEC_EQ(d, vmax, Mul(vmax, v1));
    HWY_ASSERT_VEC_EQ(d, vmax, Mul(v1, vmax));

    constexpr uint64_t kMask = FirstBits<sizeof(T) * 8>();
    const T max2 = (static_cast<uint64_t>(max) * max) & kMask;
    HWY_ASSERT_VEC_EQ(d, Set(d, max2), Mul(vmax, vmax));
  }
};

struct TestSignedMul {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);
    auto expected = AllocateAligned<T>(N);

    const auto v0 = Zero(d);
    const auto v1 = Set(d, T(1));
    const auto vi = Iota(d, 1);
    const auto vn = Iota(d, -T(N));  // no i8 supported, so no wraparound
    HWY_ASSERT_VEC_EQ(d, v0, Mul(v0, v0));
    HWY_ASSERT_VEC_EQ(d, v1, Mul(v1, v1));
    HWY_ASSERT_VEC_EQ(d, vi, Mul(v1, vi));
    HWY_ASSERT_VEC_EQ(d, vi, Mul(vi, v1));

    for (size_t i = 0; i < N; ++i) {
      expected[i] = static_cast<T>((1 + i) * (1 + i));
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), Mul(vi, vi));

    for (size_t i = 0; i < N; ++i) {
      expected[i] = static_cast<T>((-T(N) + T(i)) * T(1u + i));
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), Mul(vn, vi));
    HWY_ASSERT_VEC_EQ(d, expected.get(), Mul(vi, vn));
  }
};

struct TestMulOverflow {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto vMax = Set(d, LimitsMax<T>());
    HWY_ASSERT_VEC_EQ(d, Mul(vMax, vMax), Mul(vMax, vMax));
  }
};

struct TestDivOverflow {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto vZero = Set(d, T(0));
    const auto v1 = Set(d, T(1));
    HWY_ASSERT_VEC_EQ(d, Div(v1, vZero), Div(v1, vZero));
  }
};

HWY_NOINLINE void TestAllMul() {
  ForUnsignedTypes(ForPartialVectors<TestUnsignedMul>());
  ForSignedTypes(ForPartialVectors<TestSignedMul>());

  ForSignedTypes(ForPartialVectors<TestMulOverflow>());

  ForFloatTypes(ForPartialVectors<TestDivOverflow>());
}

struct TestMulHigh {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    using Wide = MakeWide<T>;
    const size_t N = Lanes(d);
    auto in_lanes = AllocateAligned<T>(N);
    auto expected_lanes = AllocateAligned<T>(N);

    const auto vi = Iota(d, 1);
    // no i8 supported, so no wraparound
    const auto vni = Iota(d, T(static_cast<T>(~N + 1)));

    const auto v0 = Zero(d);
    HWY_ASSERT_VEC_EQ(d, v0, MulHigh(v0, v0));
    HWY_ASSERT_VEC_EQ(d, v0, MulHigh(v0, vi));
    HWY_ASSERT_VEC_EQ(d, v0, MulHigh(vi, v0));

    // Large positive squared
    for (size_t i = 0; i < N; ++i) {
      in_lanes[i] = T(LimitsMax<T>() >> i);
      expected_lanes[i] = T((Wide(in_lanes[i]) * in_lanes[i]) >> 16);
    }
    auto v = Load(d, in_lanes.get());
    HWY_ASSERT_VEC_EQ(d, expected_lanes.get(), MulHigh(v, v));

    // Large positive * small positive
    for (size_t i = 0; i < N; ++i) {
      expected_lanes[i] = T((Wide(in_lanes[i]) * T(1u + i)) >> 16);
    }
    HWY_ASSERT_VEC_EQ(d, expected_lanes.get(), MulHigh(v, vi));
    HWY_ASSERT_VEC_EQ(d, expected_lanes.get(), MulHigh(vi, v));

    // Large positive * small negative
    for (size_t i = 0; i < N; ++i) {
      expected_lanes[i] = T((Wide(in_lanes[i]) * T(i - N)) >> 16);
    }
    HWY_ASSERT_VEC_EQ(d, expected_lanes.get(), MulHigh(v, vni));
    HWY_ASSERT_VEC_EQ(d, expected_lanes.get(), MulHigh(vni, v));
  }
};

HWY_NOINLINE void TestAllMulHigh() {
  ForPartialVectors<TestMulHigh> test;
  test(int16_t());
  test(uint16_t());
}

struct TestMulFixedPoint15 {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v0 = Zero(d);
    HWY_ASSERT_VEC_EQ(d, v0, MulFixedPoint15(v0, v0));
    HWY_ASSERT_VEC_EQ(d, v0, MulFixedPoint15(v0, v0));

    const size_t N = Lanes(d);
    auto in1 = AllocateAligned<T>(N);
    auto in2 = AllocateAligned<T>(N);
    auto expected = AllocateAligned<T>(N);

    // Random inputs in each lane
    RandomState rng;
    for (size_t rep = 0; rep < AdjustedReps(10000); ++rep) {
      for (size_t i = 0; i < N; ++i) {
        in1[i] = static_cast<T>(Random64(&rng) & 0xFFFF);
        in2[i] = static_cast<T>(Random64(&rng) & 0xFFFF);
      }

      for (size_t i = 0; i < N; ++i) {
        // There are three ways to compute the results. x86 and Arm are defined
        // using 32-bit multiplication results:
        const int arm = (2 * in1[i] * in2[i] + 0x8000) >> 16;
        const int x86 = (((in1[i] * in2[i]) >> 14) + 1) >> 1;
        // On other platforms, split the result into upper and lower 16 bits.
        const auto v1 = Set(d, in1[i]);
        const auto v2 = Set(d, in2[i]);
        const int hi = GetLane(MulHigh(v1, v2));
        const int lo = GetLane(Mul(v1, v2)) & 0xFFFF;
        const int split = 2 * hi + ((lo + 0x4000) >> 15);
        expected[i] = static_cast<T>(arm);
        if (in1[i] != -32768 || in2[i] != -32768) {
          HWY_ASSERT_EQ(arm, x86);
          HWY_ASSERT_EQ(arm, split);
        }
      }

      const auto a = Load(d, in1.get());
      const auto b = Load(d, in2.get());
      HWY_ASSERT_VEC_EQ(d, expected.get(), MulFixedPoint15(a, b));
    }
  }
};

HWY_NOINLINE void TestAllMulFixedPoint15() {
  ForPartialVectors<TestMulFixedPoint15>()(int16_t());
}

struct TestMulEven {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    using Wide = MakeWide<T>;
    const Repartition<Wide, D> d2;
    const auto v0 = Zero(d);
    HWY_ASSERT_VEC_EQ(d2, Zero(d2), MulEven(v0, v0));

    const size_t N = Lanes(d);
    auto in_lanes = AllocateAligned<T>(N);
    auto expected = AllocateAligned<Wide>(Lanes(d2));
    for (size_t i = 0; i < N; i += 2) {
      in_lanes[i + 0] = LimitsMax<T>() >> i;
      if (N != 1) {
        in_lanes[i + 1] = 1;  // unused
      }
      expected[i / 2] = Wide(in_lanes[i + 0]) * in_lanes[i + 0];
    }

    const auto v = Load(d, in_lanes.get());
    HWY_ASSERT_VEC_EQ(d2, expected.get(), MulEven(v, v));
  }
};

struct TestMulEvenOdd64 {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
#if HWY_TARGET != HWY_SCALAR
    const auto v0 = Zero(d);
    HWY_ASSERT_VEC_EQ(d, Zero(d), MulEven(v0, v0));
    HWY_ASSERT_VEC_EQ(d, Zero(d), MulOdd(v0, v0));

    const size_t N = Lanes(d);
    if (N == 1) return;

    auto in1 = AllocateAligned<T>(N);
    auto in2 = AllocateAligned<T>(N);
    auto expected_even = AllocateAligned<T>(N);
    auto expected_odd = AllocateAligned<T>(N);

    // Random inputs in each lane
    RandomState rng;
    for (size_t rep = 0; rep < AdjustedReps(1000); ++rep) {
      for (size_t i = 0; i < N; ++i) {
        in1[i] = Random64(&rng);
        in2[i] = Random64(&rng);
      }

      for (size_t i = 0; i < N; i += 2) {
        expected_even[i] = Mul128(in1[i], in2[i], &expected_even[i + 1]);
        expected_odd[i] = Mul128(in1[i + 1], in2[i + 1], &expected_odd[i + 1]);
      }

      const auto a = Load(d, in1.get());
      const auto b = Load(d, in2.get());
      HWY_ASSERT_VEC_EQ(d, expected_even.get(), MulEven(a, b));
      HWY_ASSERT_VEC_EQ(d, expected_odd.get(), MulOdd(a, b));
    }
#else
    (void)d;
#endif  // HWY_TARGET != HWY_SCALAR
  }
};

HWY_NOINLINE void TestAllMulEven() {
  ForGEVectors<64, TestMulEven> test;
  test(int32_t());
  test(uint32_t());

  ForGEVectors<128, TestMulEvenOdd64>()(uint64_t());
}

#ifndef HWY_NATIVE_FMA
#error "Bug in set_macros-inl.h, did not set HWY_NATIVE_FMA"
#endif

struct TestMulAdd {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const Vec<D> k0 = Zero(d);
    const Vec<D> v1 = Iota(d, 1);
    const Vec<D> v2 = Iota(d, 2);

    // Unlike RebindToSigned, we want to leave floating-point unchanged.
    // This allows Neg for unsigned types.
    const Rebind<If<IsFloat<T>(), T, MakeSigned<T>>, D> di;
    const Vec<D> neg_v2 = BitCast(d, Neg(BitCast(di, v2)));

    const size_t N = Lanes(d);
    auto expected = AllocateAligned<T>(N);
    HWY_ASSERT_VEC_EQ(d, k0, MulAdd(k0, k0, k0));
    HWY_ASSERT_VEC_EQ(d, v2, MulAdd(k0, v1, v2));
    HWY_ASSERT_VEC_EQ(d, v2, MulAdd(v1, k0, v2));
    HWY_ASSERT_VEC_EQ(d, k0, NegMulAdd(k0, k0, k0));
    HWY_ASSERT_VEC_EQ(d, v2, NegMulAdd(k0, v1, v2));
    HWY_ASSERT_VEC_EQ(d, v2, NegMulAdd(v1, k0, v2));

    for (size_t i = 0; i < N; ++i) {
      expected[i] = static_cast<T>((i + 1) * (i + 2));
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), MulAdd(v2, v1, k0));
    HWY_ASSERT_VEC_EQ(d, expected.get(), MulAdd(v1, v2, k0));
    HWY_ASSERT_VEC_EQ(d, expected.get(), NegMulAdd(neg_v2, v1, k0));
    HWY_ASSERT_VEC_EQ(d, expected.get(), NegMulAdd(v1, neg_v2, k0));

    for (size_t i = 0; i < N; ++i) {
      expected[i] = static_cast<T>((i + 2) * (i + 2) + (i + 1));
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), MulAdd(v2, v2, v1));
    HWY_ASSERT_VEC_EQ(d, expected.get(), NegMulAdd(neg_v2, v2, v1));

    for (size_t i = 0; i < N; ++i) {
      expected[i] =
          T(-T(i + 2u) * static_cast<T>(i + 2) + static_cast<T>(1 + i));
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), NegMulAdd(v2, v2, v1));
  }
};

struct TestMulSub {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const Vec<D> k0 = Zero(d);
    const Vec<D> kNeg0 = Set(d, T(-0.0));
    const Vec<D> v1 = Iota(d, 1);
    const Vec<D> v2 = Iota(d, 2);
    const size_t N = Lanes(d);
    auto expected = AllocateAligned<T>(N);

    HWY_ASSERT_VEC_EQ(d, k0, MulSub(k0, k0, k0));
    HWY_ASSERT_VEC_EQ(d, kNeg0, NegMulSub(k0, k0, k0));

    for (size_t i = 0; i < N; ++i) {
      expected[i] = -T(i + 2);
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), MulSub(k0, v1, v2));
    HWY_ASSERT_VEC_EQ(d, expected.get(), MulSub(v1, k0, v2));
    HWY_ASSERT_VEC_EQ(d, expected.get(), NegMulSub(Neg(k0), v1, v2));
    HWY_ASSERT_VEC_EQ(d, expected.get(), NegMulSub(v1, Neg(k0), v2));

    for (size_t i = 0; i < N; ++i) {
      expected[i] = static_cast<T>((i + 1) * (i + 2));
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), MulSub(v1, v2, k0));
    HWY_ASSERT_VEC_EQ(d, expected.get(), MulSub(v2, v1, k0));
    HWY_ASSERT_VEC_EQ(d, expected.get(), NegMulSub(Neg(v1), v2, k0));
    HWY_ASSERT_VEC_EQ(d, expected.get(), NegMulSub(v2, Neg(v1), k0));

    for (size_t i = 0; i < N; ++i) {
      expected[i] = static_cast<T>((i + 2) * (i + 2) - (1 + i));
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), MulSub(v2, v2, v1));
    HWY_ASSERT_VEC_EQ(d, expected.get(), NegMulSub(Neg(v2), v2, v1));
  }
};

HWY_NOINLINE void TestAllMulAdd() {
  ForAllTypes(ForPartialVectors<TestMulAdd>());
  ForFloatTypes(ForPartialVectors<TestMulSub>());
}

struct TestReorderWidenMulAccumulate {
  template <typename TN, class DN>
  HWY_NOINLINE void operator()(TN /*unused*/, DN dn) {
    using TW = MakeWide<TN>;
    const RepartitionToWide<DN> dw;
    const Half<DN> dnh;
    using VW = Vec<decltype(dw)>;
    using VN = Vec<decltype(dn)>;
    const size_t NN = Lanes(dn);

    const VW f0 = Zero(dw);
    const VW f1 = Set(dw, TW{1});
    const VN bf0 = Zero(dn);
    // Cannot Set() bfloat16_t directly.
    const VN bf1 = ReorderDemote2To(dn, f1, f1);

    // Any input zero => both outputs zero
    VW sum1 = f0;
    HWY_ASSERT_VEC_EQ(dw, f0,
                      ReorderWidenMulAccumulate(dw, bf0, bf0, f0, sum1));
    HWY_ASSERT_VEC_EQ(dw, f0, sum1);
    HWY_ASSERT_VEC_EQ(dw, f0,
                      ReorderWidenMulAccumulate(dw, bf0, bf1, f0, sum1));
    HWY_ASSERT_VEC_EQ(dw, f0, sum1);
    HWY_ASSERT_VEC_EQ(dw, f0,
                      ReorderWidenMulAccumulate(dw, bf1, bf0, f0, sum1));
    HWY_ASSERT_VEC_EQ(dw, f0, sum1);

    // delta[p] := 1, all others zero. For each p: Dot(delta, all-ones) == 1.
    auto delta_w = AllocateAligned<TW>(NN);
    for (size_t p = 0; p < NN; ++p) {
      // Workaround for incorrect Clang wasm codegen: re-initialize the entire
      // array rather than zero-initialize once and then toggle lane p.
      for (size_t i = 0; i < NN; ++i) {
        delta_w[i] = static_cast<TW>(i == p);
      }
      const VW delta0 = Load(dw, delta_w.get());
      const VW delta1 = Load(dw, delta_w.get() + NN / 2);
      const VN delta = ReorderDemote2To(dn, delta0, delta1);

      {
        sum1 = f0;
        const VW sum0 = ReorderWidenMulAccumulate(dw, delta, bf1, f0, sum1);
        HWY_ASSERT_EQ(TW{1}, ReduceSum(dw, Add(sum0, sum1)));
      }
      // Swapped arg order
      {
        sum1 = f0;
        const VW sum0 = ReorderWidenMulAccumulate(dw, bf1, delta, f0, sum1);
        HWY_ASSERT_EQ(TW{1}, ReduceSum(dw, Add(sum0, sum1)));
      }
      // Start with nonzero sum0 or sum1
      {
        VW sum0 = PromoteTo(dw, LowerHalf(dnh, delta));
        sum1 = PromoteTo(dw, UpperHalf(dnh, delta));
        sum0 = ReorderWidenMulAccumulate(dw, delta, bf1, sum0, sum1);
        HWY_ASSERT_EQ(TW{2}, ReduceSum(dw, Add(sum0, sum1)));
      }
      // Start with nonzero sum0 or sum1, and swap arg order
      {
        VW sum0 = PromoteTo(dw, LowerHalf(dnh, delta));
        sum1 = PromoteTo(dw, UpperHalf(dnh, delta));
        sum0 = ReorderWidenMulAccumulate(dw, bf1, delta, sum0, sum1);
        HWY_ASSERT_EQ(TW{2}, ReduceSum(dw, Add(sum0, sum1)));
      }
    }
  }
};

HWY_NOINLINE void TestAllReorderWidenMulAccumulate() {
  ForShrinkableVectors<TestReorderWidenMulAccumulate>()(bfloat16_t());
  ForShrinkableVectors<TestReorderWidenMulAccumulate>()(int16_t());
}

struct TestRearrangeToOddPlusEven {
  template <typename TN, class DN>
  HWY_NOINLINE void operator()(TN /*unused*/, DN dn) {
    using TW = MakeWide<TN>;
    const RebindToUnsigned<DN> du;
    const RepartitionToWide<DN> dw;
    const Half<DN> dnh;
    const RebindToUnsigned<decltype(dnh)> duh;
    using VW = Vec<decltype(dw)>;
    using VN = Vec<decltype(dn)>;
    const size_t NW = Lanes(dw);

    const VW up0 = Iota(dw, TW{1});
    const VW up1 = Iota(dw, static_cast<TW>(1 + NW));
    // We will compute i * (N-i) to avoid per-lane overflow.
    const VW down0 = Reverse(dw, up1);
    const VW down1 = Reverse(dw, up0);

    // Combine is not available for bf16, so cast to u16.
    const auto a0 = BitCast(duh, DemoteTo(dnh, up0));
    const auto a1 = BitCast(duh, DemoteTo(dnh, up1));
    const VN a = BitCast(dn, Combine(du, a1, a0));
    const auto b0 = BitCast(duh, DemoteTo(dnh, down0));
    const auto b1 = BitCast(duh, DemoteTo(dnh, down1));
    const VN b = BitCast(dn, Combine(du, b1, b0));

    const auto expected = AllocateAligned<TW>(NW);
    for (size_t iw = 0; iw < NW; ++iw) {
      const size_t in = iw * 2;  // even, odd is +1
      const size_t a0 = 1 + in;
      const size_t b0 = 1 + 2 * NW - a0;
      const size_t a1 = a0 + 1;
      const size_t b1 = b0 - 1;
      expected[iw] = static_cast<TW>(a0 * b0 + a1 * b1);
    }

    VW sum1 = Zero(dw);
    const VW sum0 = ReorderWidenMulAccumulate(dw, a, b, Zero(dw), sum1);
    const VW sum_odd_even = RearrangeToOddPlusEven(sum0, sum1);
    HWY_ASSERT_VEC_EQ(dw, expected.get(), sum_odd_even);
  }
};

HWY_NOINLINE void TestAllRearrangeToOddPlusEven() {
  ForShrinkableVectors<TestRearrangeToOddPlusEven>()(bfloat16_t());
  ForShrinkableVectors<TestRearrangeToOddPlusEven>()(int16_t());
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace hwy {
HWY_BEFORE_TEST(HwyMulTest);
HWY_EXPORT_AND_TEST_P(HwyMulTest, TestAllMul);
HWY_EXPORT_AND_TEST_P(HwyMulTest, TestAllMulHigh);
HWY_EXPORT_AND_TEST_P(HwyMulTest, TestAllMulFixedPoint15);
HWY_EXPORT_AND_TEST_P(HwyMulTest, TestAllMulEven);
HWY_EXPORT_AND_TEST_P(HwyMulTest, TestAllMulAdd);
HWY_EXPORT_AND_TEST_P(HwyMulTest, TestAllReorderWidenMulAccumulate);
HWY_EXPORT_AND_TEST_P(HwyMulTest, TestAllRearrangeToOddPlusEven);

}  // namespace hwy

#endif

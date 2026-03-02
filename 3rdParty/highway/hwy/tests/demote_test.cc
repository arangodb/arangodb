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

#include <algorithm>  // std::fill
#include <cmath>      // std::isfinite

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "tests/demote_test.cc"
#include "hwy/foreach_target.h"  // IWYU pragma: keep
#include "hwy/highway.h"
#include "hwy/tests/test_util-inl.h"

// Causes build timeout.
#if !HWY_IS_MSAN

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

template <typename T, HWY_IF_FLOAT(T)>
bool IsFiniteT(T t) {
  return std::isfinite(t);
}
// Wrapper avoids calling std::isfinite for integer types (ambiguous).
template <typename T, HWY_IF_NOT_FLOAT(T)>
bool IsFiniteT(T /*unused*/) {
  return true;
}

template <typename ToT>
struct TestDemoteTo {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D from_d) {
    static_assert(!IsFloat<ToT>(), "Use TestDemoteToFloat for float output");
    static_assert(sizeof(T) > sizeof(ToT), "Input type must be wider");
    const Rebind<ToT, D> to_d;

    const size_t N = Lanes(from_d);
    auto from = AllocateAligned<T>(N);
    auto expected = AllocateAligned<ToT>(N);
    HWY_ASSERT(from && expected);

    // Narrower range in the wider type, for clamping before we cast
    const T min = static_cast<T>(IsSigned<T>() ? LimitsMin<ToT>() : ToT{0});
    const T max = LimitsMax<ToT>();

    RandomState rng;
    for (size_t rep = 0; rep < AdjustedReps(1000); ++rep) {
      for (size_t i = 0; i < N; ++i) {
        const uint64_t bits = rng();
        CopyBytes<sizeof(T)>(&bits, &from[i]);  // not same size
        expected[i] = static_cast<ToT>(HWY_MIN(HWY_MAX(min, from[i]), max));
      }
      const auto in = Load(from_d, from.get());
      HWY_ASSERT_VEC_EQ(to_d, expected.get(), DemoteTo(to_d, in));
    }

    for (size_t rep = 0; rep < AdjustedReps(1000); ++rep) {
      for (size_t i = 0; i < N; ++i) {
        const uint64_t bits = rng();
        CopyBytes<sizeof(ToT)>(&bits, &expected[i]);  // not same size

        if (!IsSigned<T>() && IsSigned<ToT>()) {
          expected[i] &= static_cast<ToT>(max);
        }

        from[i] = static_cast<T>(expected[i]);
      }

      const auto in = Load(from_d, from.get());
      HWY_ASSERT_VEC_EQ(to_d, expected.get(), DemoteTo(to_d, in));
    }
  }
};

HWY_NOINLINE void TestAllDemoteToInt() {
  const ForDemoteVectors<TestDemoteTo<uint8_t>> from_i16_to_u8;
  from_i16_to_u8(int16_t());
  from_i16_to_u8(uint16_t());

  const ForDemoteVectors<TestDemoteTo<int8_t>> from_i16_to_i8;
  from_i16_to_i8(int16_t());
  from_i16_to_i8(uint16_t());

  const ForDemoteVectors<TestDemoteTo<uint8_t>, 2> from_i32_to_u8;
  from_i32_to_u8(int32_t());
  from_i32_to_u8(uint32_t());

  const ForDemoteVectors<TestDemoteTo<int8_t>, 2> from_i32_to_i8;
  from_i32_to_i8(int32_t());
  from_i32_to_i8(uint32_t());

#if HWY_HAVE_INTEGER64
  const ForDemoteVectors<TestDemoteTo<uint8_t>, 3> from_i64_to_u8;
  from_i64_to_u8(int64_t());
  from_i64_to_u8(uint64_t());

  const ForDemoteVectors<TestDemoteTo<int8_t>, 3> from_i64_to_i8;
  from_i64_to_i8(int64_t());
  from_i64_to_i8(uint64_t());
#endif

  const ForDemoteVectors<TestDemoteTo<uint16_t>> from_i32_to_u16;
  from_i32_to_u16(int32_t());
  from_i32_to_u16(uint32_t());

  const ForDemoteVectors<TestDemoteTo<int16_t>> from_i32_to_i16;
  from_i32_to_i16(int32_t());
  from_i32_to_i16(uint32_t());

#if HWY_HAVE_INTEGER64
  const ForDemoteVectors<TestDemoteTo<uint16_t>, 2> from_i64_to_u16;
  from_i64_to_u16(int64_t());
  from_i64_to_u16(uint64_t());

  const ForDemoteVectors<TestDemoteTo<int16_t>, 2> from_i64_to_i16;
  from_i64_to_i16(int64_t());
  from_i64_to_i16(uint64_t());

  const ForDemoteVectors<TestDemoteTo<uint32_t>> from_i64_to_u32;
  from_i64_to_u32(int64_t());
  from_i64_to_u32(uint64_t());

  const ForDemoteVectors<TestDemoteTo<int32_t>> from_i64_to_i32;
  from_i64_to_i32(int64_t());
  from_i64_to_i32(uint64_t());
#endif
}

HWY_NOINLINE void TestAllDemoteToMixed() {
#if HWY_HAVE_FLOAT64
  const ForDemoteVectors<TestDemoteTo<int32_t>> to_i32;
  to_i32(double());
#endif
}

template <typename ToT>
struct TestDemoteToFloat {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D from_d) {
    // For floats, we clamp differently and cannot call LimitsMin.
    static_assert(IsFloat<ToT>(), "Use TestDemoteTo for integer output");
    static_assert(sizeof(T) > sizeof(ToT), "Input type must be wider");
    const Rebind<ToT, D> to_d;

    const size_t N = Lanes(from_d);
    auto from = AllocateAligned<T>(N);
    auto expected = AllocateAligned<ToT>(N);
    HWY_ASSERT(from && expected);

    RandomState rng;
    for (size_t rep = 0; rep < AdjustedReps(1000); ++rep) {
      for (size_t i = 0; i < N; ++i) {
        do {
          const uint64_t bits = rng();
          CopyBytes<sizeof(T)>(&bits, &from[i]);  // not same size
        } while (!IsFiniteT(from[i]));
        const T magn = std::abs(from[i]);
        const T max_abs = HighestValue<ToT>();
        // NOTE: std:: version from C++11 cmath is not defined in RVV GCC, see
        // https://lists.freebsd.org/pipermail/freebsd-current/2014-January/048130.html
        const T clipped = copysign(HWY_MIN(magn, max_abs), from[i]);
        expected[i] = static_cast<ToT>(clipped);
      }

      HWY_ASSERT_VEC_EQ(to_d, expected.get(),
                        DemoteTo(to_d, Load(from_d, from.get())));
    }
  }
};

HWY_NOINLINE void TestAllDemoteToFloat() {
  // Must test f16 separately because we can only load/store/convert them.

#if HWY_HAVE_FLOAT64
  const ForDemoteVectors<TestDemoteToFloat<float>, 1> to_float;
  to_float(double());
#endif
}

struct TestDemoteToBF16 {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D from_d) {
    // For floats, we clamp differently and cannot call LimitsMin.
    static_assert(IsSame<T, float>(),
                  "TestDemoteToBF16 can only be called if T is float");
    const Rebind<bfloat16_t, D> to_d;
    const Rebind<uint32_t, D> du32;
    const Rebind<uint16_t, D> du16;

    const size_t N = Lanes(from_d);
    auto from = AllocateAligned<T>(N);
    auto expected = AllocateAligned<bfloat16_t>(N);
    HWY_ASSERT(from && expected);

    const auto u16_zero_vect = Zero(du16);
    const auto u16_one_vect = Set(du16, 1);

    RandomState rng;
    for (size_t rep = 0; rep < AdjustedReps(1000); ++rep) {
      for (size_t i = 0; i < N; ++i) {
        do {
          const uint64_t bits = rng();
          CopyBytes<sizeof(T)>(&bits, &from[i]);  // not same size
        } while (!IsFiniteT(from[i]));

        uint32_t fromBits;
        CopyBytes<sizeof(uint32_t)>(&from[i], &fromBits);

        uint16_t bf16Bits = static_cast<uint16_t>(fromBits >> 16);
        CopyBytes<sizeof(uint16_t)>(&bf16Bits, &expected[i]);
      }

      const auto in = Load(from_d, from.get());
      const auto actual = DemoteTo(to_d, in);

      // Adjust expected to account for any possible rounding that was
      // carried out by the DemoteTo operation
      auto expected_vect = BitCast(du16, Load(to_d, expected.get()));

      const auto low_f32_bits = TruncateTo(du16, BitCast(du32, in));

      // max_diff_from_expected is equal to (low_f32_bits == 0 ? 0 : 1)
      const auto max_diff_from_expected =
          Add(VecFromMask(du16, Eq(low_f32_bits, u16_zero_vect)), u16_one_vect);

      // expected_adj is equal to (actual_bits - expected_bits == 1 &&
      // max_diff_from_expected != 0) ? 1 : 0, where actual_bits is the bits of
      // actual and expected_bits is the bits of expected.
      auto expected_adj =
          And(max_diff_from_expected,
              VecFromMask(du16, Eq(Sub(BitCast(du16, actual), expected_vect),
                                   u16_one_vect)));

      // Increment expected_vect by expected_adj
      expected_vect = Add(expected_vect, expected_adj);

      // Store the adjusted expected_vect back into expected
      Store(BitCast(to_d, expected_vect), to_d, expected.get());

      HWY_ASSERT_VEC_EQ(to_d, expected.get(), actual);
    }
  }
};

HWY_NOINLINE void TestAllDemoteToBF16() {
  const ForDemoteVectors<TestDemoteToBF16, 1> to_bf16;
  to_bf16(float());
}

template <class D>
AlignedFreeUniquePtr<float[]> ReorderBF16TestCases(D d, size_t& padded) {
  const float test_cases[] = {
      // Same as BF16TestCases:
      // +/- 1
      1.0f,
      -1.0f,
      // +/- 0
      0.0f,
      -0.0f,
      // near 0
      0.25f,
      -0.25f,
      // +/- integer
      4.0f,
      -32.0f,
      // positive +/- delta
      2.015625f,
      3.984375f,
      // negative +/- delta
      -2.015625f,
      -3.984375f,

      // No huge values - would interfere with sum. But add more to fill 2 * N:
      -2.0f,
      -10.0f,
      0.03125f,
      1.03125f,
      1.5f,
      2.0f,
      4.0f,
      5.0f,
      6.0f,
      8.0f,
      10.0f,
      256.0f,
      448.0f,
      2080.0f,
  };
  const size_t kNumTestCases = sizeof(test_cases) / sizeof(test_cases[0]);
  const size_t N = Lanes(d);
  padded = RoundUpTo(kNumTestCases, 2 * N);  // allow loading pairs of vectors
  auto in = AllocateAligned<float>(padded);
  auto expected = AllocateAligned<float>(padded);
  HWY_ASSERT(in && expected);
  std::copy(test_cases, test_cases + kNumTestCases, in.get());
  std::fill(in.get() + kNumTestCases, in.get() + padded, 0.0f);
  return in;
}

class TestReorderDemote2To {
  // In-place N^2 selection sort to avoid dependencies
  void Sort(float* p, size_t count) {
    for (size_t i = 0; i < count - 1; ++i) {
      // Find min_element
      size_t idx_min = i;
      for (size_t j = i + 1; j < count; j++) {
        if (p[j] < p[idx_min]) {
          idx_min = j;
        }
      }

      // Swap with current
      const float tmp = p[i];
      p[i] = p[idx_min];
      p[idx_min] = tmp;
    }
  }

 public:
  template <typename TF32, class DF32>
  HWY_NOINLINE void operator()(TF32 /*t*/, DF32 d32) {
#if HWY_TARGET != HWY_SCALAR
    size_t padded;
    auto in = ReorderBF16TestCases(d32, padded);

    using TBF16 = bfloat16_t;
    const Repartition<TBF16, DF32> dbf16;
    const Half<decltype(dbf16)> dbf16_half;
    const size_t N = Lanes(d32);
    auto temp16 = AllocateAligned<TBF16>(2 * N);
    auto expected = AllocateAligned<float>(2 * N);
    auto actual = AllocateAligned<float>(2 * N);
    HWY_ASSERT(temp16 && expected && actual);

    for (size_t i = 0; i < padded; i += 2 * N) {
      const auto f0 = Load(d32, &in[i + 0]);
      const auto f1 = Load(d32, &in[i + N]);
      const auto v16 = ReorderDemote2To(dbf16, f0, f1);
      Store(v16, dbf16, temp16.get());
      const auto promoted0 = PromoteTo(d32, Load(dbf16_half, temp16.get() + 0));
      const auto promoted1 = PromoteTo(d32, Load(dbf16_half, temp16.get() + N));

      // Smoke test: sum should be same (with tolerance for non-associativity)
      const auto sum_expected = ReduceSum(d32, Add(f0, f1));
      const auto sum_actual = ReduceSum(d32, Add(promoted0, promoted1));

      HWY_ASSERT(sum_expected - 1E-4 <= sum_actual &&
                 sum_actual <= sum_expected + 1E-4);

      // Ensure values are the same after sorting to undo the Reorder
      Store(f0, d32, expected.get() + 0);
      Store(f1, d32, expected.get() + N);
      Store(promoted0, d32, actual.get() + 0);
      Store(promoted1, d32, actual.get() + N);
      Sort(expected.get(), 2 * N);
      Sort(actual.get(), 2 * N);
      HWY_ASSERT_VEC_EQ(d32, expected.get() + 0, Load(d32, actual.get() + 0));
      HWY_ASSERT_VEC_EQ(d32, expected.get() + N, Load(d32, actual.get() + N));
    }
#else  // HWY_SCALAR
    (void)d32;
#endif
  }
};

class TestIntegerReorderDemote2To {
#if HWY_TARGET != HWY_SCALAR

 private:
  // In-place N^2 selection sort to avoid dependencies
  template <class T>
  static void Sort(T* p, size_t count) {
    for (size_t i = 0; i < count - 1; ++i) {
      // Find min_element
      size_t idx_min = i;
      for (size_t j = i + 1; j < count; j++) {
        if (p[j] < p[idx_min]) {
          idx_min = j;
        }
      }

      // Swap with current
      const T tmp = p[i];
      p[i] = p[idx_min];
      p[idx_min] = tmp;
    }
  }

  template <class T, class D, class DN>
  static void DoIntegerReorderDemote2ToTest(DN dn, T /* t */, D d) {
    using TN = TFromD<DN>;

    const size_t N = Lanes(d);
    const size_t twiceN = N * 2;
    auto from = AllocateAligned<T>(twiceN);
    auto expected = AllocateAligned<TN>(twiceN);
    auto actual = AllocateAligned<TN>(twiceN);
    HWY_ASSERT(from && expected && actual);

    // Narrower range in the wider type, for clamping before we cast
    const T min = static_cast<T>(IsSigned<T>() ? LimitsMin<TN>() : TN{0});
    const T max = LimitsMax<TN>();

    RandomState rng;
    for (size_t rep = 0; rep < AdjustedReps(1000); ++rep) {
      for (size_t i = 0; i < twiceN; ++i) {
        const uint64_t bits = rng();
        CopyBytes<sizeof(T)>(&bits, &from[i]);  // not same size
        expected[i] = static_cast<TN>(HWY_MIN(HWY_MAX(min, from[i]), max));
      }

      const auto in_1 = Load(d, from.get());
      const auto in_2 = Load(d, from.get() + N);
      const auto demoted_vect = ReorderDemote2To(dn, in_1, in_2);
      Store(demoted_vect, dn, actual.get());
      Sort(actual.get(), twiceN);
      Sort(expected.get(), twiceN);
      HWY_ASSERT_VEC_EQ(dn, expected.get(), Load(dn, actual.get()));
    }

    for (size_t rep = 0; rep < AdjustedReps(1000); ++rep) {
      for (size_t i = 0; i < twiceN; ++i) {
        const uint64_t bits = rng();
        CopyBytes<sizeof(TN)>(&bits, &expected[i]);  // not same size
        if (!IsSigned<T>() && IsSigned<TN>()) {
          expected[i] &= static_cast<TN>(max);
        }

        from[i] = static_cast<T>(expected[i]);
      }

      const auto in_1 = Load(d, from.get());
      const auto in_2 = Load(d, from.get() + N);
      const auto demoted_vect = ReorderDemote2To(dn, in_1, in_2);
      Store(demoted_vect, dn, actual.get());
      Sort(actual.get(), twiceN);
      Sort(expected.get(), twiceN);
      HWY_ASSERT_VEC_EQ(dn, expected.get(), Load(dn, actual.get()));
    }
  }
#endif

 public:
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*t*/, D d) {
#if HWY_TARGET != HWY_SCALAR
    const RepartitionToNarrow<D> dn;
    const RebindToSigned<decltype(dn)> dn_i;
    const RebindToUnsigned<decltype(dn)> dn_u;

    DoIntegerReorderDemote2ToTest(dn_i, T(), d);
    DoIntegerReorderDemote2ToTest(dn_u, T(), d);
#else
    (void)d;
#endif
  }
};

HWY_NOINLINE void TestAllReorderDemote2To() {
  ForUI163264(ForShrinkableVectors<TestIntegerReorderDemote2To>());
  ForShrinkableVectors<TestReorderDemote2To>()(float());
}

struct TestFloatOrderedDemote2To {
  template <typename TF, class DF>
  HWY_NOINLINE void operator()(TF /*t*/, DF df) {
#if HWY_TARGET != HWY_SCALAR
    const Repartition<bfloat16_t, decltype(df)> dbf16;
    const RebindToUnsigned<decltype(dbf16)> du16;
    const RebindToUnsigned<decltype(df)> du32;
    const Half<decltype(du16)> du16_half;
    const size_t N = Lanes(df);
    const size_t twiceN = N * 2;
    auto from = AllocateAligned<TF>(twiceN);
    auto expected = AllocateAligned<bfloat16_t>(twiceN);
    HWY_ASSERT(from && expected);

    const auto u16_zero_vect = Zero(du16);
    const auto u16_one_vect = Set(du16, 1);

    RandomState rng;
    for (size_t rep = 0; rep < AdjustedReps(1000); ++rep) {
      for (size_t i = 0; i < twiceN; ++i) {
        do {
          const uint64_t bits = rng();
          CopyBytes<sizeof(TF)>(&bits, &from[i]);  // not same size
        } while (!IsFiniteT(from[i]));

        uint32_t u32Bits;
        CopyBytes<sizeof(uint32_t)>(&from[i], &u32Bits);

        const uint16_t expected_bf16_bits =
            static_cast<uint16_t>(u32Bits >> 16);

        CopyBytes<sizeof(bfloat16_t)>(&expected_bf16_bits, &expected[i]);
      }

      const auto in_1 = Load(df, from.get());
      const auto in_2 = Load(df, from.get() + N);
      const auto actual = OrderedDemote2To(dbf16, in_1, in_2);

      // Adjust expected to account for any possible rounding that was
      // carried out by the OrderedDemote2To operation
      auto expected_vect = BitCast(du16, Load(dbf16, expected.get()));

      const auto low_f32_bits =
          Combine(du16, TruncateTo(du16_half, BitCast(du32, in_2)),
                  TruncateTo(du16_half, BitCast(du32, in_1)));
      // max_diff_from_expected is equal to (low_f32_bits == 0 ? 0 : 1)
      const auto max_diff_from_expected =
          Add(VecFromMask(du16, Eq(low_f32_bits, u16_zero_vect)), u16_one_vect);

      // expected_adj is equal to (actual_bits - expected_bits == 1 &&
      // max_diff_from_expected != 0) ? 1 : 0, where actual_bits is the bits of
      // actual and expected_bits is the bits of expected.
      auto expected_adj =
          And(max_diff_from_expected,
              VecFromMask(du16, Eq(Sub(BitCast(du16, actual), expected_vect),
                                   u16_one_vect)));

      // Increment expected_vect by expected_adj
      expected_vect = Add(expected_vect, expected_adj);

      // Store the adjusted expected_vect back into expected
      Store(BitCast(dbf16, expected_vect), dbf16, expected.get());

      HWY_ASSERT_VEC_EQ(dbf16, expected.get(), actual);
    }
#else
    (void)df;
#endif
  }
};

class TestIntegerOrderedDemote2To {
#if HWY_TARGET != HWY_SCALAR

 private:
  template <class T, class D, class DN>
  static void DoIntegerOrderedDemote2ToTest(DN dn, T /*t*/, D d) {
    using TN = TFromD<DN>;

    const size_t N = Lanes(d);
    const size_t twiceN = N * 2;
    auto from = AllocateAligned<T>(twiceN);
    auto expected = AllocateAligned<TN>(twiceN);
    HWY_ASSERT(from && expected);

    // Narrower range in the wider type, for clamping before we cast
    const T min = static_cast<T>(IsSigned<T>() ? LimitsMin<TN>() : TN{0});
    const T max = LimitsMax<TN>();

    RandomState rng;
    for (size_t rep = 0; rep < AdjustedReps(1000); ++rep) {
      for (size_t i = 0; i < twiceN; ++i) {
        const uint64_t bits = rng();
        CopyBytes<sizeof(T)>(&bits, &from[i]);  // not same size
        expected[i] = static_cast<TN>(HWY_MIN(HWY_MAX(min, from[i]), max));
      }

      const auto in_1 = Load(d, from.get());
      const auto in_2 = Load(d, from.get() + N);
      const auto actual = OrderedDemote2To(dn, in_1, in_2);
      HWY_ASSERT_VEC_EQ(dn, expected.get(), actual);
    }

    for (size_t rep = 0; rep < AdjustedReps(1000); ++rep) {
      for (size_t i = 0; i < twiceN; ++i) {
        const uint64_t bits = rng();
        CopyBytes<sizeof(TN)>(&bits, &expected[i]);  // not same size
        if (!IsSigned<T>() && IsSigned<TN>()) {
          expected[i] &= static_cast<TN>(max);
        }

        from[i] = static_cast<T>(expected[i]);
      }

      const auto in_1 = Load(d, from.get());
      const auto in_2 = Load(d, from.get() + N);
      const auto actual = OrderedDemote2To(dn, in_1, in_2);
      HWY_ASSERT_VEC_EQ(dn, expected.get(), actual);
    }
  }
#endif

 public:
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*t*/, D d) {
#if HWY_TARGET != HWY_SCALAR
    const RepartitionToNarrow<D> dn;
    const RebindToSigned<decltype(dn)> dn_i;
    const RebindToUnsigned<decltype(dn)> dn_u;

    DoIntegerOrderedDemote2ToTest(dn_i, T(), d);
    DoIntegerOrderedDemote2ToTest(dn_u, T(), d);
#else
    (void)d;
#endif
  }
};

HWY_NOINLINE void TestAllOrderedDemote2To() {
  ForUI163264(ForShrinkableVectors<TestIntegerOrderedDemote2To>());
  ForShrinkableVectors<TestFloatOrderedDemote2To>()(float());
}

struct TestI32F64 {
  template <typename TF, class DF>
  HWY_NOINLINE void operator()(TF /*unused*/, const DF df) {
    using TI = int32_t;
    const Rebind<TI, DF> di;
    const size_t N = Lanes(df);

    // Integer positive
    HWY_ASSERT_VEC_EQ(di, Iota(di, TI(4)), DemoteTo(di, Iota(df, TF(4.0))));

    // Integer negative
    HWY_ASSERT_VEC_EQ(di, Iota(di, -TI(N)), DemoteTo(di, Iota(df, -TF(N))));

    // Above positive
    HWY_ASSERT_VEC_EQ(di, Iota(di, TI(2)), DemoteTo(di, Iota(df, TF(2.001))));

    // Below positive
    HWY_ASSERT_VEC_EQ(di, Iota(di, TI(3)), DemoteTo(di, Iota(df, TF(3.9999))));

    const TF eps = static_cast<TF>(0.0001);
    // Above negative
    HWY_ASSERT_VEC_EQ(di, Iota(di, -TI(N)),
                      DemoteTo(di, Iota(df, -TF(N + 1) + eps)));

    // Below negative
    HWY_ASSERT_VEC_EQ(di, Iota(di, -TI(N + 1)),
                      DemoteTo(di, Iota(df, -TF(N + 1) - eps)));

    // Huge positive float
    HWY_ASSERT_VEC_EQ(di, Set(di, LimitsMax<TI>()),
                      DemoteTo(di, Set(df, TF(1E12))));

    // Huge negative float
    HWY_ASSERT_VEC_EQ(di, Set(di, LimitsMin<TI>()),
                      DemoteTo(di, Set(df, TF(-1E12))));
  }
};

HWY_NOINLINE void TestAllI32F64() {
#if HWY_HAVE_FLOAT64
  ForDemoteVectors<TestI32F64>()(double());
#endif
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#endif  //  !HWY_IS_MSAN

#if HWY_ONCE

namespace hwy {
#if !HWY_IS_MSAN
HWY_BEFORE_TEST(HwyDemoteTest);
HWY_EXPORT_AND_TEST_P(HwyDemoteTest, TestAllDemoteToInt);
HWY_EXPORT_AND_TEST_P(HwyDemoteTest, TestAllDemoteToMixed);
HWY_EXPORT_AND_TEST_P(HwyDemoteTest, TestAllDemoteToFloat);
HWY_EXPORT_AND_TEST_P(HwyDemoteTest, TestAllDemoteToBF16);
HWY_EXPORT_AND_TEST_P(HwyDemoteTest, TestAllReorderDemote2To);
HWY_EXPORT_AND_TEST_P(HwyDemoteTest, TestAllOrderedDemote2To);
HWY_EXPORT_AND_TEST_P(HwyDemoteTest, TestAllI32F64);
#endif  //  !HWY_IS_MSAN
}  // namespace hwy

#endif

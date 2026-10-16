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

#include <string.h>

#include <cmath>  // std::isfinite

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "tests/convert_test.cc"
#include "hwy/foreach_target.h"  // IWYU pragma: keep
#include "hwy/highway.h"
#include "hwy/tests/test_util-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

template <typename T, size_t N, int kPow2>
size_t DeduceN(Simd<T, N, kPow2>) {
  return N;
}

template <typename ToT>
struct TestRebind {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const Rebind<ToT, D> dto;
    const size_t N = Lanes(d);
    HWY_ASSERT(N <= MaxLanes(d));
    const size_t NTo = Lanes(dto);
    if (NTo != N) {
      HWY_ABORT("u%zu -> u%zu: lanes %zu %zu pow2 %d %d cap %zu %zu\n",
                8 * sizeof(T), 8 * sizeof(ToT), N, NTo, d.Pow2(), dto.Pow2(),
                DeduceN(d), DeduceN(dto));
    }
  }
};

// Lane count remains the same when we rebind to smaller/equal/larger types.
HWY_NOINLINE void TestAllRebind() {
#if HWY_HAVE_INTEGER64
  ForShrinkableVectors<TestRebind<uint8_t>, 3>()(uint64_t());
#endif  // HWY_HAVE_INTEGER64
  ForShrinkableVectors<TestRebind<uint8_t>, 2>()(uint32_t());
  ForShrinkableVectors<TestRebind<uint8_t>, 1>()(uint16_t());
  ForPartialVectors<TestRebind<uint8_t>>()(uint8_t());
  ForExtendableVectors<TestRebind<uint16_t>, 1>()(uint8_t());
  ForExtendableVectors<TestRebind<uint32_t>, 2>()(uint8_t());
#if HWY_HAVE_INTEGER64
  ForExtendableVectors<TestRebind<uint64_t>, 3>()(uint8_t());
#endif  // HWY_HAVE_INTEGER64
}

// Cast and ensure bytes are the same. Called directly from TestAllBitCast or
// via TestBitCastFrom.
template <typename ToT>
struct TestBitCast {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const Repartition<ToT, D> dto;
    const size_t N = Lanes(d);
    const size_t Nto = Lanes(dto);
    if (N == 0 || Nto == 0) return;
    HWY_ASSERT_EQ(N * sizeof(T), Nto * sizeof(ToT));
    const auto vf = Iota(d, 1);
    const auto vt = BitCast(dto, vf);
    // Must return the same bits
    auto from_lanes = AllocateAligned<T>(Lanes(d));
    auto to_lanes = AllocateAligned<ToT>(Lanes(dto));
    HWY_ASSERT(from_lanes && to_lanes);
    Store(vf, d, from_lanes.get());
    Store(vt, dto, to_lanes.get());
    HWY_ASSERT(
        BytesEqual(from_lanes.get(), to_lanes.get(), Lanes(d) * sizeof(T)));
  }
};

// From D to all types.
struct TestBitCastFrom {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T t, D d) {
    TestBitCast<uint8_t>()(t, d);
    TestBitCast<uint16_t>()(t, d);
    TestBitCast<uint32_t>()(t, d);
#if HWY_HAVE_INTEGER64
    TestBitCast<uint64_t>()(t, d);
#endif
    TestBitCast<int8_t>()(t, d);
    TestBitCast<int16_t>()(t, d);
    TestBitCast<int32_t>()(t, d);
#if HWY_HAVE_INTEGER64
    TestBitCast<int64_t>()(t, d);
#endif
    TestBitCast<float>()(t, d);
#if HWY_HAVE_FLOAT64
    TestBitCast<double>()(t, d);
#endif
  }
};

HWY_NOINLINE void TestAllBitCast() {
  // For HWY_SCALAR and partial vectors, we can only cast to same-sized types:
  // the former can't partition its single lane, and the latter can be smaller
  // than a destination type.
  const ForPartialVectors<TestBitCast<uint8_t>> to_u8;
  to_u8(uint8_t());
  to_u8(int8_t());

  const ForPartialVectors<TestBitCast<int8_t>> to_i8;
  to_i8(uint8_t());
  to_i8(int8_t());

  const ForPartialVectors<TestBitCast<uint16_t>> to_u16;
  to_u16(uint16_t());
  to_u16(int16_t());

  const ForPartialVectors<TestBitCast<int16_t>> to_i16;
  to_i16(uint16_t());
  to_i16(int16_t());

  const ForPartialVectors<TestBitCast<uint32_t>> to_u32;
  to_u32(uint32_t());
  to_u32(int32_t());
  to_u32(float());

  const ForPartialVectors<TestBitCast<int32_t>> to_i32;
  to_i32(uint32_t());
  to_i32(int32_t());
  to_i32(float());

#if HWY_HAVE_INTEGER64
  const ForPartialVectors<TestBitCast<uint64_t>> to_u64;
  to_u64(uint64_t());
  to_u64(int64_t());
#if HWY_HAVE_FLOAT64
  to_u64(double());
#endif

  const ForPartialVectors<TestBitCast<int64_t>> to_i64;
  to_i64(uint64_t());
  to_i64(int64_t());
#if HWY_HAVE_FLOAT64
  to_i64(double());
#endif
#endif  // HWY_HAVE_INTEGER64

  const ForPartialVectors<TestBitCast<float>> to_float;
  to_float(uint32_t());
  to_float(int32_t());
  to_float(float());

#if HWY_HAVE_FLOAT64
  const ForPartialVectors<TestBitCast<double>> to_double;
  to_double(double());
#if HWY_HAVE_INTEGER64
  to_double(uint64_t());
  to_double(int64_t());
#endif  // HWY_HAVE_INTEGER64
#endif  // HWY_HAVE_FLOAT64

#if HWY_TARGET != HWY_SCALAR
  // For non-scalar vectors, we can cast all types to all.
  ForAllTypes(ForGEVectors<64, TestBitCastFrom>());
#endif
}

template <class TTo>
struct TestResizeBitCastToOneLaneVect {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);
    if (N == 0) {
      return;
    }

    auto from_lanes = AllocateAligned<T>(N);
    HWY_ASSERT(from_lanes);
    auto v = Iota(d, 1);
    Store(v, d, from_lanes.get());

    const size_t num_of_bytes_to_copy = HWY_MIN(N * sizeof(T), sizeof(TTo));

    int8_t active_bits_mask_i8_arr[sizeof(TTo)] = {};
    for (size_t i = 0; i < num_of_bytes_to_copy; i++) {
      active_bits_mask_i8_arr[i] = int8_t{-1};
    }

    TTo active_bits_int_mask;
    CopyBytes<sizeof(TTo)>(active_bits_mask_i8_arr, &active_bits_int_mask);

    const FixedTag<TTo, 1> d_to;
    TTo expected_bits = 0;
    memcpy(&expected_bits, from_lanes.get(), num_of_bytes_to_copy);

    const auto expected = Set(d_to, expected_bits);
    const auto v_active_bits_mask = Set(d_to, active_bits_int_mask);

    const auto actual_1 = And(v_active_bits_mask, ResizeBitCast(d_to, v));
    const auto actual_2 = ZeroExtendResizeBitCast(d_to, d, v);

    HWY_ASSERT_VEC_EQ(d_to, expected, actual_1);
    HWY_ASSERT_VEC_EQ(d_to, expected, actual_2);
  }
};

HWY_NOINLINE void TestAllResizeBitCastToOneLaneVect() {
  ForAllTypes(ForPartialVectors<TestResizeBitCastToOneLaneVect<uint32_t>>());
#if HWY_HAVE_INTEGER64
  ForAllTypes(ForPartialVectors<TestResizeBitCastToOneLaneVect<uint64_t>>());
#endif
}

// Cast and ensure bytes are the same. Called directly from
// TestAllSameSizeResizeBitCast or via TestSameSizeResizeBitCastFrom.
template <typename ToT>
struct TestSameSizeResizeBitCast {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const Repartition<ToT, D> dto;
    const auto v = Iota(d, 1);
    const auto expected = BitCast(dto, v);

    const VFromD<decltype(dto)> actual_1 = ResizeBitCast(dto, v);
    const VFromD<decltype(dto)> actual_2 = ZeroExtendResizeBitCast(dto, d, v);

    HWY_ASSERT_VEC_EQ(dto, expected, actual_1);
    HWY_ASSERT_VEC_EQ(dto, expected, actual_2);
  }
};

// From D to all types.
struct TestSameSizeResizeBitCastFrom {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T t, D d) {
    TestSameSizeResizeBitCast<uint8_t>()(t, d);
    TestSameSizeResizeBitCast<uint16_t>()(t, d);
    TestSameSizeResizeBitCast<uint32_t>()(t, d);
#if HWY_HAVE_INTEGER64
    TestSameSizeResizeBitCast<uint64_t>()(t, d);
#endif
    TestSameSizeResizeBitCast<int8_t>()(t, d);
    TestSameSizeResizeBitCast<int16_t>()(t, d);
    TestSameSizeResizeBitCast<int32_t>()(t, d);
#if HWY_HAVE_INTEGER64
    TestSameSizeResizeBitCast<int64_t>()(t, d);
#endif
    TestSameSizeResizeBitCast<float>()(t, d);
#if HWY_HAVE_FLOAT64
    TestSameSizeResizeBitCast<double>()(t, d);
#endif
  }
};

HWY_NOINLINE void TestAllSameSizeResizeBitCast() {
  // For HWY_SCALAR and partial vectors, we can only cast to same-sized types:
  // the former can't partition its single lane, and the latter can be smaller
  // than a destination type.
  const ForPartialVectors<TestSameSizeResizeBitCast<uint8_t>> to_u8;
  to_u8(uint8_t());
  to_u8(int8_t());

  const ForPartialVectors<TestSameSizeResizeBitCast<int8_t>> to_i8;
  to_i8(uint8_t());
  to_i8(int8_t());

  const ForPartialVectors<TestSameSizeResizeBitCast<uint16_t>> to_u16;
  to_u16(uint16_t());
  to_u16(int16_t());

  const ForPartialVectors<TestSameSizeResizeBitCast<int16_t>> to_i16;
  to_i16(uint16_t());
  to_i16(int16_t());

  const ForPartialVectors<TestSameSizeResizeBitCast<uint32_t>> to_u32;
  to_u32(uint32_t());
  to_u32(int32_t());
  to_u32(float());

  const ForPartialVectors<TestSameSizeResizeBitCast<int32_t>> to_i32;
  to_i32(uint32_t());
  to_i32(int32_t());
  to_i32(float());

#if HWY_HAVE_INTEGER64
  const ForPartialVectors<TestSameSizeResizeBitCast<uint64_t>> to_u64;
  to_u64(uint64_t());
  to_u64(int64_t());
#if HWY_HAVE_FLOAT64
  to_u64(double());
#endif

  const ForPartialVectors<TestSameSizeResizeBitCast<int64_t>> to_i64;
  to_i64(uint64_t());
  to_i64(int64_t());
#if HWY_HAVE_FLOAT64
  to_i64(double());
#endif
#endif  // HWY_HAVE_INTEGER64

  const ForPartialVectors<TestSameSizeResizeBitCast<float>> to_float;
  to_float(uint32_t());
  to_float(int32_t());
  to_float(float());

#if HWY_HAVE_FLOAT64
  const ForPartialVectors<TestSameSizeResizeBitCast<double>> to_double;
  to_double(double());
#if HWY_HAVE_INTEGER64
  to_double(uint64_t());
  to_double(int64_t());
#endif  // HWY_HAVE_INTEGER64
#endif  // HWY_HAVE_FLOAT64

#if HWY_TARGET != HWY_SCALAR
  // For non-scalar vectors, we can cast all types to all.
  ForAllTypes(ForGEVectors<64, TestSameSizeResizeBitCastFrom>());
#endif
}

template <typename ToT>
struct TestPromoteTo {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D from_d) {
    static_assert(sizeof(T) < sizeof(ToT), "Input type must be narrower");
    const Rebind<ToT, D> to_d;

    const size_t N = Lanes(from_d);
    auto from = AllocateAligned<T>(N);
    auto expected = AllocateAligned<ToT>(N);

    RandomState rng;
    for (size_t rep = 0; rep < AdjustedReps(200); ++rep) {
      for (size_t i = 0; i < N; ++i) {
        const uint64_t bits = rng();
        CopyBytes<sizeof(T)>(&bits, &from[i]);  // not same size
        expected[i] = from[i];
      }

      HWY_ASSERT_VEC_EQ(to_d, expected.get(),
                        PromoteTo(to_d, Load(from_d, from.get())));
    }
  }
};

HWY_NOINLINE void TestAllPromoteTo() {
  const ForPromoteVectors<TestPromoteTo<uint16_t>, 1> to_u16div2;
  to_u16div2(uint8_t());

  const ForPromoteVectors<TestPromoteTo<uint32_t>, 2> to_u32div4;
  to_u32div4(uint8_t());

  const ForPromoteVectors<TestPromoteTo<uint32_t>, 1> to_u32div2;
  to_u32div2(uint16_t());

  const ForPromoteVectors<TestPromoteTo<int16_t>, 1> to_i16div2;
  to_i16div2(uint8_t());
  to_i16div2(int8_t());

  const ForPromoteVectors<TestPromoteTo<int32_t>, 1> to_i32div2;
  to_i32div2(uint16_t());
  to_i32div2(int16_t());

  const ForPromoteVectors<TestPromoteTo<int32_t>, 2> to_i32div4;
  to_i32div4(uint8_t());
  to_i32div4(int8_t());

  // Must test f16/bf16 separately because we can only load/store/convert them.

#if HWY_HAVE_INTEGER64
  const ForPromoteVectors<TestPromoteTo<uint64_t>, 1> to_u64div2;
  to_u64div2(uint32_t());

  const ForPromoteVectors<TestPromoteTo<int64_t>, 1> to_i64div2;
  to_i64div2(int32_t());
  to_i64div2(uint32_t());

  const ForPromoteVectors<TestPromoteTo<uint64_t>, 2> to_u64div4;
  to_u64div4(uint16_t());

  const ForPromoteVectors<TestPromoteTo<int64_t>, 2> to_i64div4;
  to_i64div4(int16_t());
  to_i64div4(uint16_t());

  const ForPromoteVectors<TestPromoteTo<uint64_t>, 3> to_u64div8;
  to_u64div8(uint8_t());

  const ForPromoteVectors<TestPromoteTo<int64_t>, 3> to_i64div8;
  to_i64div8(int8_t());
  to_i64div8(uint8_t());
#endif

#if HWY_HAVE_FLOAT64
  const ForPromoteVectors<TestPromoteTo<double>, 1> to_f64div2;
  to_f64div2(int32_t());
  to_f64div2(float());
#endif
}

template <typename T, HWY_IF_FLOAT(T)>
bool IsFinite(T t) {
  return std::isfinite(t);
}
// Wrapper avoids calling std::isfinite for integer types (ambiguous).
template <typename T, HWY_IF_NOT_FLOAT(T)>
bool IsFinite(T /*unused*/) {
  return true;
}

template <class D>
AlignedFreeUniquePtr<float[]> F16TestCases(D d, size_t& padded) {
  const float test_cases[] = {
      // +/- 1
      1.0f, -1.0f,
      // +/- 0
      0.0f, -0.0f,
      // near 0
      0.25f, -0.25f,
      // +/- integer
      4.0f, -32.0f,
      // positive near limit
      65472.0f, 65504.0f,
      // negative near limit
      -65472.0f, -65504.0f,
      // positive +/- delta
      2.00390625f, 3.99609375f,
      // negative +/- delta
      -2.00390625f, -3.99609375f,
      // No infinity/NaN - implementation-defined due to Arm.
  };
  constexpr size_t kNumTestCases = sizeof(test_cases) / sizeof(test_cases[0]);
  const size_t N = Lanes(d);
  HWY_ASSERT(N != 0);
  padded = RoundUpTo(kNumTestCases, N);  // allow loading whole vectors
  auto in = AllocateAligned<float>(padded);
  auto expected = AllocateAligned<float>(padded);
  size_t i = 0;
  for (; i < kNumTestCases; ++i) {
    in[i] = test_cases[i];
  }
  for (; i < padded; ++i) {
    in[i] = 0.0f;
  }
  return in;
}

struct TestF16 {
  template <typename TF32, class DF32>
  HWY_NOINLINE void operator()(TF32 /*t*/, DF32 d32) {
#if HWY_HAVE_FLOAT16
    size_t padded;
    const size_t N = Lanes(d32);  // same count for f16
    HWY_ASSERT(N != 0);
    auto in = F16TestCases(d32, padded);
    using TF16 = float16_t;
    const Rebind<TF16, DF32> d16;
    auto temp16 = AllocateAligned<TF16>(N);

    for (size_t i = 0; i < padded; i += N) {
      const auto loaded = Load(d32, &in[i]);
      Store(DemoteTo(d16, loaded), d16, temp16.get());
      HWY_ASSERT_VEC_EQ(d32, loaded, PromoteTo(d32, Load(d16, temp16.get())));
    }
#else
    (void)d32;
#endif
  }
};

HWY_NOINLINE void TestAllF16() { ForDemoteVectors<TestF16>()(float()); }

template <class D>
AlignedFreeUniquePtr<float[]> BF16TestCases(D d, size_t& padded) {
  const float test_cases[] = {
      // +/- 1
      1.0f, -1.0f,
      // +/- 0
      0.0f, -0.0f,
      // near 0
      0.25f, -0.25f,
      // +/- integer
      4.0f, -32.0f,
      // positive near limit
      3.389531389251535E38f, 1.99384199368e+38f,
      // negative near limit
      -3.389531389251535E38f, -1.99384199368e+38f,
      // positive +/- delta
      2.015625f, 3.984375f,
      // negative +/- delta
      -2.015625f, -3.984375f,
  };
  constexpr size_t kNumTestCases = sizeof(test_cases) / sizeof(test_cases[0]);
  const size_t N = Lanes(d);
  HWY_ASSERT(N != 0);
  padded = RoundUpTo(kNumTestCases, N);  // allow loading whole vectors
  auto in = AllocateAligned<float>(padded);
  auto expected = AllocateAligned<float>(padded);
  size_t i = 0;
  for (; i < kNumTestCases; ++i) {
    in[i] = test_cases[i];
  }
  for (; i < padded; ++i) {
    in[i] = 0.0f;
  }
  return in;
}

struct TestBF16 {
  template <typename TF32, class DF32>
  HWY_NOINLINE void operator()(TF32 /*t*/, DF32 d32) {
#if !defined(HWY_EMULATE_SVE)
    size_t padded;
    auto in = BF16TestCases(d32, padded);
    using TBF16 = bfloat16_t;
#if HWY_TARGET == HWY_SCALAR
    const Rebind<TBF16, DF32> dbf16;  // avoid 4/2 = 2 lanes
#else
    const Repartition<TBF16, DF32> dbf16;
#endif
    const Half<decltype(dbf16)> dbf16_half;
    const size_t N = Lanes(d32);

    HWY_ASSERT(Lanes(dbf16_half) == N);
    auto temp16 = AllocateAligned<TBF16>(N);

    for (size_t i = 0; i < padded; i += N) {
      const auto loaded = Load(d32, &in[i]);
      const auto v16 = DemoteTo(dbf16_half, loaded);
      Store(v16, dbf16_half, temp16.get());
      const auto v16_loaded = Load(dbf16_half, temp16.get());
      HWY_ASSERT_VEC_EQ(d32, loaded, PromoteTo(d32, v16_loaded));
    }
#else
    (void)d32;
#endif
  }
};

HWY_NOINLINE void TestAllBF16() { ForShrinkableVectors<TestBF16>()(float()); }

struct TestConvertU8 {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, const D du32) {
    const Rebind<uint8_t, D> du8;
    const auto wrap = Set(du32, 0xFF);
    HWY_ASSERT_VEC_EQ(du8, Iota(du8, 0), U8FromU32(And(Iota(du32, 0), wrap)));
    HWY_ASSERT_VEC_EQ(du8, Iota(du8, 0x7F),
                      U8FromU32(And(Iota(du32, 0x7F), wrap)));
  }
};

HWY_NOINLINE void TestAllConvertU8() {
  ForDemoteVectors<TestConvertU8, 2>()(uint32_t());
}

template <typename From, typename To, class D>
constexpr bool IsSupportedTruncation() {
  return (sizeof(To) < sizeof(From)) &&
         (Rebind<To, D>().Pow2() + 3 >= static_cast<int>(CeilLog2(sizeof(To))));
}

struct TestTruncateTo {
  template <typename From, typename To, class D,
            hwy::EnableIf<!IsSupportedTruncation<From, To, D>()>* = nullptr>
  HWY_NOINLINE void testTo(From, To, const D) {
    // do nothing
  }

  template <typename From, typename To, class D,
            hwy::EnableIf<IsSupportedTruncation<From, To, D>()>* = nullptr>
  HWY_NOINLINE void testTo(From, To, const D d) {
    constexpr uint32_t base = 0xFA578D00;
    const Rebind<To, D> dTo;
    const auto src = Iota(d, static_cast<From>(base));
    const auto expected = Iota(dTo, static_cast<To>(base));
    const VFromD<decltype(dTo)> actual = TruncateTo(dTo, src);
    HWY_ASSERT_VEC_EQ(dTo, expected, actual);
  }

  template <typename T, class D>
  HWY_NOINLINE void operator()(T from, const D d) {
    testTo<T, uint8_t, D>(from, uint8_t(), d);
    testTo<T, uint16_t, D>(from, uint16_t(), d);
    testTo<T, uint32_t, D>(from, uint32_t(), d);
  }
};

HWY_NOINLINE void TestAllTruncate() {
  ForUnsignedTypes(ForPartialVectors<TestTruncateTo>());
}

struct TestOrderedTruncate2To {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*t*/, D d) {
#if HWY_TARGET != HWY_SCALAR
    const Repartition<MakeNarrow<T>, decltype(d)> dn;
    using TN = TFromD<decltype(dn)>;

    const size_t N = Lanes(d);
    const size_t twiceN = N * 2;
    auto from = AllocateAligned<T>(twiceN);
    auto expected = AllocateAligned<TN>(twiceN);

    const T max = LimitsMax<TN>();

    constexpr uint32_t iota_base = 0xFA578D00;
    const auto src_iota_a = Iota(d, static_cast<T>(iota_base));
    const auto src_iota_b = Iota(d, static_cast<T>(iota_base + N));
    const auto expected_iota_trunc_result =
        Iota(dn, static_cast<TN>(iota_base));
    const auto actual_iota_trunc_result =
        OrderedTruncate2To(dn, src_iota_a, src_iota_b);
    HWY_ASSERT_VEC_EQ(dn, expected_iota_trunc_result, actual_iota_trunc_result);

    RandomState rng;
    for (size_t rep = 0; rep < AdjustedReps(1000); ++rep) {
      for (size_t i = 0; i < twiceN; ++i) {
        const uint64_t bits = rng();
        CopyBytes<sizeof(T)>(&bits, &from[i]);  // not same size
        expected[i] = static_cast<TN>(from[i] & max);
      }

      const auto in_1 = Load(d, from.get());
      const auto in_2 = Load(d, from.get() + N);
      const auto actual = OrderedTruncate2To(dn, in_1, in_2);
      HWY_ASSERT_VEC_EQ(dn, expected.get(), actual);
    }
#else
    (void)d;
#endif
  }
};

HWY_NOINLINE void TestAllOrderedTruncate2To() {
  ForU163264(ForShrinkableVectors<TestOrderedTruncate2To>());
}

// Separate function to attempt to work around a compiler bug on Arm: when this
// is merged with TestIntFromFloat, outputs match a previous Iota(-(N+1)) input.
struct TestIntFromFloatHuge {
  template <typename TF, class DF>
  HWY_NOINLINE void operator()(TF /*unused*/, const DF df) {
    // The Armv7 manual says that float->int saturates, i.e. chooses the
    // nearest representable value. This works correctly on armhf with GCC, but
    // not with clang. For reasons unknown, MSVC also runs into an out-of-memory
    // error here.
#if HWY_COMPILER_CLANG || HWY_COMPILER_MSVC
    (void)df;
#else
    using TI = MakeSigned<TF>;
    const Rebind<TI, DF> di;

    // Workaround for incorrect 32-bit GCC codegen for SSSE3 - Print-ing
    // the expected lvalue also seems to prevent the issue.
    const size_t N = Lanes(df);
    auto expected = AllocateAligned<TI>(N);

    // Huge positive
    Store(Set(di, LimitsMax<TI>()), di, expected.get());
    HWY_ASSERT_VEC_EQ(di, expected.get(), ConvertTo(di, Set(df, TF(1E20))));

    // Huge negative
    Store(Set(di, LimitsMin<TI>()), di, expected.get());
    HWY_ASSERT_VEC_EQ(di, expected.get(), ConvertTo(di, Set(df, TF(-1E20))));
#endif
  }
};

class TestIntFromFloat {
  template <typename TF, class DF>
  static HWY_NOINLINE void TestPowers(TF /*unused*/, const DF df) {
    using TI = MakeSigned<TF>;
    const Rebind<TI, DF> di;
    constexpr size_t kBits = sizeof(TF) * 8;

    // Powers of two, plus offsets to set some mantissa bits.
    const int64_t ofs_table[3] = {0LL, 3LL << (kBits / 2), 1LL << (kBits - 15)};
    for (int sign = 0; sign < 2; ++sign) {
      for (size_t shift = 0; shift < kBits - 1; ++shift) {
        for (int64_t ofs : ofs_table) {
          const int64_t mag = (int64_t{1} << shift) + ofs;
          const int64_t val = sign ? mag : -mag;
          HWY_ASSERT_VEC_EQ(di, Set(di, static_cast<TI>(val)),
                            ConvertTo(di, Set(df, static_cast<TF>(val))));
        }
      }
    }
  }

  template <typename TF, class DF>
  static HWY_NOINLINE void TestRandom(TF /*unused*/, const DF df) {
    using TI = MakeSigned<TF>;
    const Rebind<TI, DF> di;
    const size_t N = Lanes(df);

    // TF does not have enough precision to represent TI.
    const double min = static_cast<double>(LimitsMin<TI>());
    const double max = static_cast<double>(LimitsMax<TI>());

    // Also check random values.
    auto from = AllocateAligned<TF>(N);
    auto expected = AllocateAligned<TI>(N);
    RandomState rng;
    for (size_t rep = 0; rep < AdjustedReps(1000); ++rep) {
      for (size_t i = 0; i < N; ++i) {
        do {
          const uint64_t bits = rng();
          CopyBytes<sizeof(TF)>(&bits, &from[i]);  // not same size
        } while (!std::isfinite(from[i]));
        if (from[i] >= max) {
          expected[i] = LimitsMax<TI>();
        } else if (from[i] <= min) {
          expected[i] = LimitsMin<TI>();
        } else {
          expected[i] = static_cast<TI>(from[i]);
        }
      }

      HWY_ASSERT_VEC_EQ(di, expected.get(),
                        ConvertTo(di, Load(df, from.get())));
    }
  }

 public:
  template <typename TF, class DF>
  HWY_NOINLINE void operator()(TF tf, const DF df) {
    using TI = MakeSigned<TF>;
    const Rebind<TI, DF> di;
    const size_t N = Lanes(df);

    // Integer positive
    HWY_ASSERT_VEC_EQ(di, Iota(di, TI(4)), ConvertTo(di, Iota(df, TF(4.0))));

    // Integer negative
    HWY_ASSERT_VEC_EQ(di, Iota(di, -TI(N)), ConvertTo(di, Iota(df, -TF(N))));

    // Above positive
    HWY_ASSERT_VEC_EQ(di, Iota(di, TI(2)), ConvertTo(di, Iota(df, TF(2.001))));

    // Below positive
    HWY_ASSERT_VEC_EQ(di, Iota(di, TI(3)), ConvertTo(di, Iota(df, TF(3.9999))));

    const TF eps = static_cast<TF>(0.0001);
    // Above negative
    HWY_ASSERT_VEC_EQ(di, Iota(di, -TI(N)),
                      ConvertTo(di, Iota(df, -TF(N + 1) + eps)));

    // Below negative
    HWY_ASSERT_VEC_EQ(di, Iota(di, -TI(N + 1)),
                      ConvertTo(di, Iota(df, -TF(N + 1) - eps)));

    TestPowers(tf, df);
    TestRandom(tf, df);
  }
};

HWY_NOINLINE void TestAllIntFromFloat() {
  ForFloatTypes(ForPartialVectors<TestIntFromFloatHuge>());
  ForFloatTypes(ForPartialVectors<TestIntFromFloat>());
}

struct TestFloatFromInt {
  template <typename TF, class DF>
  HWY_NOINLINE void operator()(TF /*unused*/, const DF df) {
    using TI = MakeSigned<TF>;
    const RebindToSigned<DF> di;
    const size_t N = Lanes(df);

    // Integer positive
    HWY_ASSERT_VEC_EQ(df, Iota(df, TF(4.0)), ConvertTo(df, Iota(di, TI(4))));

    // Integer negative
    HWY_ASSERT_VEC_EQ(df, Iota(df, -TF(N)), ConvertTo(df, Iota(di, -TI(N))));

    // Max positive
    HWY_ASSERT_VEC_EQ(df, Set(df, TF(LimitsMax<TI>())),
                      ConvertTo(df, Set(di, LimitsMax<TI>())));

    // Min negative
    HWY_ASSERT_VEC_EQ(df, Set(df, TF(LimitsMin<TI>())),
                      ConvertTo(df, Set(di, LimitsMin<TI>())));
  }
};

HWY_NOINLINE void TestAllFloatFromInt() {
  ForFloatTypes(ForPartialVectors<TestFloatFromInt>());
}

struct TestFloatFromUint {
  template <typename TF, class DF>
  HWY_NOINLINE void operator()(TF /*unused*/, const DF df) {
    using TU = MakeUnsigned<TF>;
    const RebindToUnsigned<DF> du;

    // Integer positive
    HWY_ASSERT_VEC_EQ(df, Iota(df, TF(4.0)), ConvertTo(df, Iota(du, TU(4))));
    HWY_ASSERT_VEC_EQ(df, Iota(df, TF(65535.0)),
                      ConvertTo(df, Iota(du, 65535)));  // 2^16-1
    if (sizeof(TF) > 4) {
      HWY_ASSERT_VEC_EQ(df, Iota(df, TF(4294967295.0)),
                        ConvertTo(df, Iota(du, 4294967295ULL)));  // 2^32-1
    }

    // Max positive
    HWY_ASSERT_VEC_EQ(df, Set(df, TF(LimitsMax<TU>())),
                      ConvertTo(df, Set(du, LimitsMax<TU>())));

    // Zero
    HWY_ASSERT_VEC_EQ(df, Zero(df), ConvertTo(df, Zero(du)));
  }
};

HWY_NOINLINE void TestAllFloatFromUint() {
  ForFloatTypes(ForPartialVectors<TestFloatFromUint>());
}

struct TestI32F64 {
  template <typename TF, class DF>
  HWY_NOINLINE void operator()(TF /*unused*/, const DF df) {
    using TI = int32_t;
    const Rebind<TI, DF> di;
    const size_t N = Lanes(df);

    // Integer positive
    HWY_ASSERT_VEC_EQ(df, Iota(df, TF(4.0)), PromoteTo(df, Iota(di, TI(4))));

    // Integer negative
    HWY_ASSERT_VEC_EQ(df, Iota(df, -TF(N)), PromoteTo(df, Iota(di, -TI(N))));

    // Above positive
    HWY_ASSERT_VEC_EQ(df, Iota(df, TF(2.0)), PromoteTo(df, Iota(di, TI(2))));

    // Below positive
    HWY_ASSERT_VEC_EQ(df, Iota(df, TF(4.0)), PromoteTo(df, Iota(di, TI(4))));

    // Above negative
    HWY_ASSERT_VEC_EQ(df, Iota(df, TF(-4.0)), PromoteTo(df, Iota(di, TI(-4))));

    // Below negative
    HWY_ASSERT_VEC_EQ(df, Iota(df, TF(-2.0)), PromoteTo(df, Iota(di, TI(-2))));

    // Max positive int
    HWY_ASSERT_VEC_EQ(df, Set(df, TF(LimitsMax<TI>())),
                      PromoteTo(df, Set(di, LimitsMax<TI>())));

    // Min negative int
    HWY_ASSERT_VEC_EQ(df, Set(df, TF(LimitsMin<TI>())),
                      PromoteTo(df, Set(di, LimitsMin<TI>())));
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

#if HWY_ONCE

namespace hwy {
HWY_BEFORE_TEST(HwyConvertTest);
HWY_EXPORT_AND_TEST_P(HwyConvertTest, TestAllRebind);
HWY_EXPORT_AND_TEST_P(HwyConvertTest, TestAllBitCast);
HWY_EXPORT_AND_TEST_P(HwyConvertTest, TestAllResizeBitCastToOneLaneVect);
HWY_EXPORT_AND_TEST_P(HwyConvertTest, TestAllSameSizeResizeBitCast);
HWY_EXPORT_AND_TEST_P(HwyConvertTest, TestAllPromoteTo);
HWY_EXPORT_AND_TEST_P(HwyConvertTest, TestAllF16);
HWY_EXPORT_AND_TEST_P(HwyConvertTest, TestAllBF16);
HWY_EXPORT_AND_TEST_P(HwyConvertTest, TestAllConvertU8);
HWY_EXPORT_AND_TEST_P(HwyConvertTest, TestAllTruncate);
HWY_EXPORT_AND_TEST_P(HwyConvertTest, TestAllOrderedTruncate2To);
HWY_EXPORT_AND_TEST_P(HwyConvertTest, TestAllIntFromFloat);
HWY_EXPORT_AND_TEST_P(HwyConvertTest, TestAllFloatFromInt);
HWY_EXPORT_AND_TEST_P(HwyConvertTest, TestAllFloatFromUint);
HWY_EXPORT_AND_TEST_P(HwyConvertTest, TestAllI32F64);
}  // namespace hwy

#endif

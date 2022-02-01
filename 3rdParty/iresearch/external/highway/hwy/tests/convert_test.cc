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
#include <string.h>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "tests/convert_test.cc"
#include "hwy/foreach_target.h"

#include "hwy/highway.h"
#include "hwy/tests/test_util-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

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
#if HWY_CAP_INTEGER64
    TestBitCast<uint64_t>()(t, d);
#endif
    TestBitCast<int8_t>()(t, d);
    TestBitCast<int16_t>()(t, d);
    TestBitCast<int32_t>()(t, d);
#if HWY_CAP_INTEGER64
    TestBitCast<int64_t>()(t, d);
#endif
    TestBitCast<float>()(t, d);
#if HWY_CAP_FLOAT64
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

#if HWY_CAP_INTEGER64
  const ForPartialVectors<TestBitCast<uint64_t>> to_u64;
  to_u64(uint64_t());
  to_u64(int64_t());
#if HWY_CAP_FLOAT64
  to_u64(double());
#endif

  const ForPartialVectors<TestBitCast<int64_t>> to_i64;
  to_i64(uint64_t());
  to_i64(int64_t());
#if HWY_CAP_FLOAT64
  to_i64(double());
#endif
#endif  // HWY_CAP_INTEGER64

  const ForPartialVectors<TestBitCast<float>> to_float;
  to_float(uint32_t());
  to_float(int32_t());
  to_float(float());

#if HWY_CAP_FLOAT64
  const ForPartialVectors<TestBitCast<double>> to_double;
  to_double(double());
#if HWY_CAP_INTEGER64
  to_double(uint64_t());
  to_double(int64_t());
#endif  // HWY_CAP_INTEGER64
#endif  // HWY_CAP_FLOAT64

#if HWY_TARGET != HWY_SCALAR
  // For non-scalar vectors, we can cast all types to all.
  ForAllTypes(ForGE64Vectors<TestBitCastFrom>());
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
    for (size_t rep = 0; rep < 200; ++rep) {
      for (size_t i = 0; i < N; ++i) {
        const uint64_t bits = rng();
        memcpy(&from[i], &bits, sizeof(T));
        expected[i] = from[i];
      }

      HWY_ASSERT_VEC_EQ(to_d, expected.get(),
                        PromoteTo(to_d, Load(from_d, from.get())));
    }
  }
};

HWY_NOINLINE void TestAllPromoteTo() {
  const ForPromoteVectors<TestPromoteTo<uint16_t>, 2> to_u16div2;
  to_u16div2(uint8_t());

  const ForPromoteVectors<TestPromoteTo<uint32_t>, 4> to_u32div4;
  to_u32div4(uint8_t());

  const ForPromoteVectors<TestPromoteTo<uint32_t>, 2> to_u32div2;
  to_u32div2(uint16_t());

  const ForPromoteVectors<TestPromoteTo<int16_t>, 2> to_i16div2;
  to_i16div2(uint8_t());
  to_i16div2(int8_t());

  const ForPromoteVectors<TestPromoteTo<int32_t>, 2> to_i32div2;
  to_i32div2(uint16_t());
  to_i32div2(int16_t());

  const ForPromoteVectors<TestPromoteTo<int32_t>, 4> to_i32div4;
  to_i32div4(uint8_t());
  to_i32div4(int8_t());

  // Must test f16 separately because we can only load/store/convert them.

#if HWY_CAP_INTEGER64
  const ForPromoteVectors<TestPromoteTo<uint64_t>, 2> to_u64div2;
  to_u64div2(uint32_t());

  const ForPromoteVectors<TestPromoteTo<int64_t>, 2> to_i64div2;
  to_i64div2(int32_t());
#endif

#if HWY_CAP_FLOAT64
  const ForPromoteVectors<TestPromoteTo<double>, 2> to_f64div2;
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

    // Narrower range in the wider type, for clamping before we cast
    const T min = LimitsMin<ToT>();
    const T max = LimitsMax<ToT>();

    const auto value_ok = [&](T& value) {
      if (!IsFinite(value)) return false;
#if HWY_EMULATE_SVE
      // farm_sve just casts, which is undefined if the value is out of range.
      value = HWY_MIN(HWY_MAX(min, value), max);
#endif
      return true;
    };

    RandomState rng;
    for (size_t rep = 0; rep < 1000; ++rep) {
      for (size_t i = 0; i < N; ++i) {
        do {
          const uint64_t bits = rng();
          memcpy(&from[i], &bits, sizeof(T));
        } while (!value_ok(from[i]));
        expected[i] = static_cast<ToT>(HWY_MIN(HWY_MAX(min, from[i]), max));
      }

      HWY_ASSERT_VEC_EQ(to_d, expected.get(),
                        DemoteTo(to_d, Load(from_d, from.get())));
    }
  }
};

HWY_NOINLINE void TestAllDemoteToInt() {
  ForDemoteVectors<TestDemoteTo<uint8_t>>()(int16_t());
  ForDemoteVectors<TestDemoteTo<uint8_t>, 4>()(int32_t());

  ForDemoteVectors<TestDemoteTo<int8_t>>()(int16_t());
  ForDemoteVectors<TestDemoteTo<int8_t>, 4>()(int32_t());

  const ForDemoteVectors<TestDemoteTo<uint16_t>> to_u16;
  to_u16(int32_t());

  const ForDemoteVectors<TestDemoteTo<int16_t>> to_i16;
  to_i16(int32_t());
}

HWY_NOINLINE void TestAllDemoteToMixed() {
#if HWY_CAP_FLOAT64
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

    RandomState rng;
    for (size_t rep = 0; rep < 1000; ++rep) {
      for (size_t i = 0; i < N; ++i) {
        do {
          const uint64_t bits = rng();
          memcpy(&from[i], &bits, sizeof(T));
        } while (!IsFinite(from[i]));
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

#if HWY_CAP_FLOAT64
  const ForDemoteVectors<TestDemoteToFloat<float>, 2> to_float;
  to_float(double());
#endif
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
      // No infinity/NaN - implementation-defined due to ARM.
  };
  const size_t kNumTestCases = sizeof(test_cases) / sizeof(test_cases[0]);
  const size_t N = Lanes(d);
  padded = RoundUpTo(kNumTestCases, N);  // allow loading whole vectors
  auto in = AllocateAligned<float>(padded);
  auto expected = AllocateAligned<float>(padded);
  std::copy(test_cases, test_cases + kNumTestCases, in.get());
  std::fill(in.get() + kNumTestCases, in.get() + padded, 0.0f);
  return in;
}

struct TestF16 {
  template <typename TF32, class DF32>
  HWY_NOINLINE void operator()(TF32 /*t*/, DF32 d32) {
#if HWY_CAP_FLOAT16
    size_t padded;
    auto in = F16TestCases(d32, padded);
    using TF16 = float16_t;
    const Rebind<TF16, DF32> d16;
    const size_t N = Lanes(d32);  // same count for f16
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

struct TestConvertU8 {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, const D du32) {
    const Rebind<uint8_t, D> du8;
    auto lanes8 = AllocateAligned<uint8_t>(Lanes(du8));
    Store(Iota(du8, 0), du8, lanes8.get());
    HWY_ASSERT_VEC_EQ(du8, Iota(du8, 0), U8FromU32(Iota(du32, 0)));
    HWY_ASSERT_VEC_EQ(du8, Iota(du8, 0x7F), U8FromU32(Iota(du32, 0x7F)));
  }
};

HWY_NOINLINE void TestAllConvertU8() {
  ForDemoteVectors<TestConvertU8, 4>()(uint32_t());
}

// Separate function to attempt to work around a compiler bug on ARM: when this
// is merged with TestIntFromFloat, outputs match a previous Iota(-(N+1)) input.
struct TestIntFromFloatHuge {
  template <typename TF, class DF>
  HWY_NOINLINE void operator()(TF /*unused*/, const DF df) {
    // Still does not work, although ARMv7 manual says that float->int
    // saturates, i.e. chooses the nearest representable value. Also causes
    // out-of-memory for MSVC, and unsafe cast in farm_sve.
#if HWY_TARGET != HWY_NEON && !HWY_COMPILER_MSVC && !defined(HWY_EMULATE_SVE)
    using TI = MakeSigned<TF>;
    const Rebind<TI, DF> di;

    // Huge positive (lvalue works around GCC bug, tested with 10.2.1, where
    // the expected i32 value is otherwise 0x80..00).
    const auto expected_max = Set(di, LimitsMax<TI>());
    HWY_ASSERT_VEC_EQ(di, expected_max, ConvertTo(di, Set(df, TF(1E20))));

    // Huge negative (also lvalue for safety, but GCC bug was not triggered)
    const auto expected_min = Set(di, LimitsMin<TI>());
    HWY_ASSERT_VEC_EQ(di, expected_min, ConvertTo(di, Set(df, TF(-1E20))));
#else
    (void)df;
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
          const int64_t mag = (int64_t(1) << shift) + ofs;
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
    for (size_t rep = 0; rep < 1000; ++rep) {
      for (size_t i = 0; i < N; ++i) {
        do {
          const uint64_t bits = rng();
          memcpy(&from[i], &bits, sizeof(TF));
        } while (!std::isfinite(from[i]));
#if defined(HWY_EMULATE_SVE)
        // farm_sve just casts, which is undefined if the value is out of range.
        from[i] = HWY_MIN(HWY_MAX(min / 2, from[i]), max / 2);
#endif
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

struct TestI32F64 {
  template <typename TF, class DF>
  HWY_NOINLINE void operator()(TF /*unused*/, const DF df) {
    using TI = int32_t;
    const Rebind<TI, DF> di;
    const size_t N = Lanes(df);

    // Integer positive
    HWY_ASSERT_VEC_EQ(di, Iota(di, TI(4)), DemoteTo(di, Iota(df, TF(4.0))));
    HWY_ASSERT_VEC_EQ(df, Iota(df, TF(4.0)), PromoteTo(df, Iota(di, TI(4))));

    // Integer negative
    HWY_ASSERT_VEC_EQ(di, Iota(di, -TI(N)), DemoteTo(di, Iota(df, -TF(N))));
    HWY_ASSERT_VEC_EQ(df, Iota(df, -TF(N)), PromoteTo(df, Iota(di, -TI(N))));

    // Above positive
    HWY_ASSERT_VEC_EQ(di, Iota(di, TI(2)), DemoteTo(di, Iota(df, TF(2.001))));
    HWY_ASSERT_VEC_EQ(df, Iota(df, TF(2.0)), PromoteTo(df, Iota(di, TI(2))));

    // Below positive
    HWY_ASSERT_VEC_EQ(di, Iota(di, TI(3)), DemoteTo(di, Iota(df, TF(3.9999))));
    HWY_ASSERT_VEC_EQ(df, Iota(df, TF(4.0)), PromoteTo(df, Iota(di, TI(4))));

    const TF eps = static_cast<TF>(0.0001);
    // Above negative
    HWY_ASSERT_VEC_EQ(di, Iota(di, -TI(N)),
                      DemoteTo(di, Iota(df, -TF(N + 1) + eps)));
    HWY_ASSERT_VEC_EQ(df, Iota(df, TF(-4.0)), PromoteTo(df, Iota(di, TI(-4))));

    // Below negative
    HWY_ASSERT_VEC_EQ(di, Iota(di, -TI(N + 1)),
                      DemoteTo(di, Iota(df, -TF(N + 1) - eps)));
    HWY_ASSERT_VEC_EQ(df, Iota(df, TF(-2.0)), PromoteTo(df, Iota(di, TI(-2))));

    // Max positive int
    HWY_ASSERT_VEC_EQ(df, Set(df, TF(LimitsMax<TI>())),
                      PromoteTo(df, Set(di, LimitsMax<TI>())));

    // Min negative int
    HWY_ASSERT_VEC_EQ(df, Set(df, TF(LimitsMin<TI>())),
                      PromoteTo(df, Set(di, LimitsMin<TI>())));

    // farm_sve just casts, which is undefined if the value is out of range.
#if !defined(HWY_EMULATE_SVE)
    // Huge positive float
    HWY_ASSERT_VEC_EQ(di, Set(di, LimitsMax<TI>()),
                      DemoteTo(di, Set(df, TF(1E12))));

    // Huge negative float
    HWY_ASSERT_VEC_EQ(di, Set(di, LimitsMin<TI>()),
                      DemoteTo(di, Set(df, TF(-1E12))));
#endif
  }
};

HWY_NOINLINE void TestAllI32F64() {
#if HWY_CAP_FLOAT64
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
HWY_EXPORT_AND_TEST_P(HwyConvertTest, TestAllBitCast);
HWY_EXPORT_AND_TEST_P(HwyConvertTest, TestAllPromoteTo);
HWY_EXPORT_AND_TEST_P(HwyConvertTest, TestAllDemoteToInt);
HWY_EXPORT_AND_TEST_P(HwyConvertTest, TestAllDemoteToMixed);
HWY_EXPORT_AND_TEST_P(HwyConvertTest, TestAllDemoteToFloat);
HWY_EXPORT_AND_TEST_P(HwyConvertTest, TestAllF16);
HWY_EXPORT_AND_TEST_P(HwyConvertTest, TestAllConvertU8);
HWY_EXPORT_AND_TEST_P(HwyConvertTest, TestAllIntFromFloat);
HWY_EXPORT_AND_TEST_P(HwyConvertTest, TestAllFloatFromInt);
HWY_EXPORT_AND_TEST_P(HwyConvertTest, TestAllI32F64);
}  // namespace hwy

// Ought not to be necessary, but without this, no tests run on RVV.
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

#endif

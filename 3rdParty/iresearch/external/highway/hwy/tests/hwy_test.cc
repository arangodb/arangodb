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

#include <limits>

#include "hwy/base.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "tests/hwy_test.cc"
#include "hwy/foreach_target.h"
#include "hwy/highway.h"
#include "hwy/nanobenchmark.h"  // Unpredictable1
#include "hwy/tests/test_util-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

template <class DF>
HWY_NOINLINE void FloorLog2(const DF df, const uint8_t* HWY_RESTRICT values,
                            uint8_t* HWY_RESTRICT log2) {
  // Descriptors for all required data types:
  const Rebind<int32_t, DF> d32;
  const Rebind<uint8_t, DF> d8;

  const auto u8 = Load(d8, values);
  const auto bits = BitCast(d32, ConvertTo(df, PromoteTo(d32, u8)));
  const auto exponent = ShiftRight<23>(bits) - Set(d32, 127);
  Store(DemoteTo(d8, exponent), d8, log2);
}

struct TestFloorLog2 {
  template <class T, class DF>
  HWY_NOINLINE void operator()(T /*unused*/, DF df) {
    const size_t N = Lanes(df);
    auto in = AllocateAligned<uint8_t>(N);
    auto expected = AllocateAligned<uint8_t>(N);

    RandomState rng;
    for (size_t i = 0; i < N; ++i) {
      expected[i] = Random32(&rng) & 7;
      in[i] = static_cast<uint8_t>(1u << expected[i]);
    }
    auto out = AllocateAligned<uint8_t>(N);
    FloorLog2(df, in.get(), out.get());
    int sum = 0;
    for (size_t i = 0; i < N; ++i) {
      HWY_ASSERT_EQ(expected[i], out[i]);
      sum += out[i];
    }
    PreventElision(sum);
  }
};

HWY_NOINLINE void TestAllFloorLog2() {
  ForDemoteVectors<TestFloorLog2, 4>()(float());
}

template <class D, typename T>
HWY_NOINLINE void MulAddLoop(const D d, const T* HWY_RESTRICT mul_array,
                             const T* HWY_RESTRICT add_array, const size_t size,
                             T* HWY_RESTRICT x_array) {
  // Type-agnostic (caller-specified lane type) and width-agnostic (uses
  // best available instruction set).
  for (size_t i = 0; i < size; i += Lanes(d)) {
    const auto mul = Load(d, mul_array + i);
    const auto add = Load(d, add_array + i);
    auto x = Load(d, x_array + i);
    x = MulAdd(mul, x, add);
    Store(x, d, x_array + i);
  }
}

struct TestSumMulAdd {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    RandomState rng;
    const size_t kSize = 64;
    HWY_ALIGN T mul[kSize];
    HWY_ALIGN T x[kSize];
    HWY_ALIGN T add[kSize];
    for (size_t i = 0; i < kSize; ++i) {
      mul[i] = static_cast<T>(Random32(&rng) & 0xF);
      x[i] = static_cast<T>(Random32(&rng) & 0xFF);
      add[i] = static_cast<T>(Random32(&rng) & 0xFF);
    }
    double expected_sum = 0.0;
    for (size_t i = 0; i < kSize; ++i) {
      expected_sum += mul[i] * x[i] + add[i];
    }

    MulAddLoop(d, mul, add, kSize, x);
    HWY_ASSERT_EQ(67008.0, expected_sum);
  }
};

HWY_NOINLINE void TestAllSumMulAdd() {
  ForFloatTypes(ForPartialVectors<TestSumMulAdd>());
}

//------------------------------------------------------------------------------
// base.h

HWY_NOINLINE void TestAllLimits() {
  HWY_ASSERT_EQ(uint8_t(0), LimitsMin<uint8_t>());
  HWY_ASSERT_EQ(uint16_t(0), LimitsMin<uint16_t>());
  HWY_ASSERT_EQ(uint32_t(0), LimitsMin<uint32_t>());
  HWY_ASSERT_EQ(uint64_t(0), LimitsMin<uint64_t>());

  HWY_ASSERT_EQ(int8_t(-128), LimitsMin<int8_t>());
  HWY_ASSERT_EQ(int16_t(-32768), LimitsMin<int16_t>());
  HWY_ASSERT_EQ(int32_t(0x80000000u), LimitsMin<int32_t>());
  HWY_ASSERT_EQ(int64_t(0x8000000000000000ull), LimitsMin<int64_t>());

  HWY_ASSERT_EQ(uint8_t(0xFF), LimitsMax<uint8_t>());
  HWY_ASSERT_EQ(uint16_t(0xFFFF), LimitsMax<uint16_t>());
  HWY_ASSERT_EQ(uint32_t(0xFFFFFFFFu), LimitsMax<uint32_t>());
  HWY_ASSERT_EQ(uint64_t(0xFFFFFFFFFFFFFFFFull), LimitsMax<uint64_t>());

  HWY_ASSERT_EQ(int8_t(0x7F), LimitsMax<int8_t>());
  HWY_ASSERT_EQ(int16_t(0x7FFF), LimitsMax<int16_t>());
  HWY_ASSERT_EQ(int32_t(0x7FFFFFFFu), LimitsMax<int32_t>());
  HWY_ASSERT_EQ(int64_t(0x7FFFFFFFFFFFFFFFull), LimitsMax<int64_t>());
}

struct TestLowestHighest {
  template <class T>
  HWY_NOINLINE void operator()(T /*unused*/) const {
    HWY_ASSERT_EQ(std::numeric_limits<T>::lowest(), LowestValue<T>());
    HWY_ASSERT_EQ(std::numeric_limits<T>::max(), HighestValue<T>());
  }
};

HWY_NOINLINE void TestAllLowestHighest() { ForAllTypes(TestLowestHighest()); }
struct TestIsUnsigned {
  template <class T>
  HWY_NOINLINE void operator()(T /*unused*/) const {
    static_assert(!IsFloat<T>(), "Expected !IsFloat");
    static_assert(!IsSigned<T>(), "Expected !IsSigned");
  }
};

struct TestIsSigned {
  template <class T>
  HWY_NOINLINE void operator()(T /*unused*/) const {
    static_assert(!IsFloat<T>(), "Expected !IsFloat");
    static_assert(IsSigned<T>(), "Expected IsSigned");
  }
};

struct TestIsFloat {
  template <class T>
  HWY_NOINLINE void operator()(T /*unused*/) const {
    static_assert(IsFloat<T>(), "Expected IsFloat");
    static_assert(IsSigned<T>(), "Floats are also considered signed");
  }
};

HWY_NOINLINE void TestAllType() {
  ForUnsignedTypes(TestIsUnsigned());
  ForSignedTypes(TestIsSigned());
  ForFloatTypes(TestIsFloat());
}

HWY_NOINLINE void TestAllPopCount() {
  HWY_ASSERT_EQ(size_t(0), PopCount(0u));
  HWY_ASSERT_EQ(size_t(1), PopCount(1u));
  HWY_ASSERT_EQ(size_t(1), PopCount(2u));
  HWY_ASSERT_EQ(size_t(2), PopCount(3u));
  HWY_ASSERT_EQ(size_t(1), PopCount(0x80000000u));
  HWY_ASSERT_EQ(size_t(31), PopCount(0x7FFFFFFFu));
  HWY_ASSERT_EQ(size_t(32), PopCount(0xFFFFFFFFu));

  HWY_ASSERT_EQ(size_t(1), PopCount(0x80000000ull));
  HWY_ASSERT_EQ(size_t(31), PopCount(0x7FFFFFFFull));
  HWY_ASSERT_EQ(size_t(32), PopCount(0xFFFFFFFFull));
  HWY_ASSERT_EQ(size_t(33), PopCount(0x10FFFFFFFFull));
  HWY_ASSERT_EQ(size_t(63), PopCount(0xFFFEFFFFFFFFFFFFull));
  HWY_ASSERT_EQ(size_t(64), PopCount(0xFFFFFFFFFFFFFFFFull));
}

//------------------------------------------------------------------------------
// test_util-inl.h

struct TestName {
  template <class T, class D>
  HWY_NOINLINE void operator()(T t, D d) {
    std::string expected = IsFloat<T>() ? "f" : (IsSigned<T>() ? "i" : "u");
    expected += std::to_string(sizeof(T) * 8);

    const size_t N = Lanes(d);
    if (N != 1) {
      expected += 'x';
      expected += std::to_string(N);
    }
    const std::string actual = TypeName(t, N);
    if (expected != actual) {
      NotifyFailure(__FILE__, __LINE__, expected.c_str(), 0, expected.c_str(),
                    actual.c_str());
    }
  }
};

HWY_NOINLINE void TestAllName() { ForAllTypes(ForPartialVectors<TestName>()); }

struct TestEqualInteger {
  template <class T>
  HWY_NOINLINE void operator()(T /*t*/) const {
    HWY_ASSERT(IsEqual(T(0), T(0)));
    HWY_ASSERT(IsEqual(T(1), T(1)));
    HWY_ASSERT(IsEqual(T(-1), T(-1)));
    HWY_ASSERT(IsEqual(LimitsMin<T>(), LimitsMin<T>()));

    HWY_ASSERT(!IsEqual(T(0), T(1)));
    HWY_ASSERT(!IsEqual(T(1), T(0)));
    HWY_ASSERT(!IsEqual(T(1), T(-1)));
    HWY_ASSERT(!IsEqual(T(-1), T(1)));
    HWY_ASSERT(!IsEqual(LimitsMin<T>(), LimitsMax<T>()));
    HWY_ASSERT(!IsEqual(LimitsMax<T>(), LimitsMin<T>()));
  }
};

struct TestEqualFloat {
  template <class T>
  HWY_NOINLINE void operator()(T /*t*/) const {
    HWY_ASSERT(IsEqual(T(0), T(0)));
    HWY_ASSERT(IsEqual(T(1), T(1)));
    HWY_ASSERT(IsEqual(T(-1), T(-1)));
    HWY_ASSERT(IsEqual(MantissaEnd<T>(), MantissaEnd<T>()));

    HWY_ASSERT(!IsEqual(T(0), T(1)));
    HWY_ASSERT(!IsEqual(T(1), T(0)));
    HWY_ASSERT(!IsEqual(T(1), T(-1)));
    HWY_ASSERT(!IsEqual(T(-1), T(1)));
    HWY_ASSERT(!IsEqual(LowestValue<T>(), HighestValue<T>()));
    HWY_ASSERT(!IsEqual(HighestValue<T>(), LowestValue<T>()));
  }
};

HWY_NOINLINE void TestAllEqual() {
  ForIntegerTypes(TestEqualInteger());
  ForFloatTypes(TestEqualFloat());
}

//------------------------------------------------------------------------------
// highway.h

struct TestSet {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    // Zero
    const auto v0 = Zero(d);
    const size_t N = Lanes(d);
    auto expected = AllocateAligned<T>(N);
    std::fill(expected.get(), expected.get() + N, T(0));
    HWY_ASSERT_VEC_EQ(d, expected.get(), v0);

    // Set
    const auto v2 = Set(d, T(2));
    for (size_t i = 0; i < N; ++i) {
      expected[i] = 2;
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), v2);

    // Iota
    const auto vi = Iota(d, T(5));
    for (size_t i = 0; i < N; ++i) {
      expected[i] = T(5 + i);
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), vi);

    // Undefined
    const auto vu = Undefined(d);
    Store(vu, d, expected.get());
  }
};

HWY_NOINLINE void TestAllSet() { ForAllTypes(ForPartialVectors<TestSet>()); }

// Ensures wraparound (mod 2^bits)
struct TestOverflow {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v1 = Set(d, T(1));
    const auto vmax = Set(d, LimitsMax<T>());
    const auto vmin = Set(d, LimitsMin<T>());
    // Unsigned underflow / negative -> positive
    HWY_ASSERT_VEC_EQ(d, vmax, vmin - v1);
    // Unsigned overflow / positive -> negative
    HWY_ASSERT_VEC_EQ(d, vmin, vmax + v1);
  }
};

HWY_NOINLINE void TestAllOverflow() {
  ForIntegerTypes(ForPartialVectors<TestOverflow>());
}

struct TestSignBitInteger {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v0 = Zero(d);
    const auto all = VecFromMask(d, Eq(v0, v0));
    const auto vs = SignBit(d);
    const auto other = Sub(vs, Set(d, 1));

    // Shifting left by one => overflow, equal zero
    HWY_ASSERT_VEC_EQ(d, v0, Add(vs, vs));
    // Verify the lower bits are zero (only +/- and logical ops are available
    // for all types)
    HWY_ASSERT_VEC_EQ(d, all, Add(vs, other));
  }
};

struct TestSignBitFloat {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v0 = Zero(d);
    const auto vs = SignBit(d);
    const auto vp = Set(d, 2.25);
    const auto vn = Set(d, -2.25);
    HWY_ASSERT_VEC_EQ(d, Or(vp, vs), vn);
    HWY_ASSERT_VEC_EQ(d, AndNot(vs, vn), vp);
    HWY_ASSERT_VEC_EQ(d, v0, vs);
  }
};

HWY_NOINLINE void TestAllSignBit() {
  ForIntegerTypes(ForPartialVectors<TestSignBitInteger>());
  ForFloatTypes(ForPartialVectors<TestSignBitFloat>());
}

// std::isnan returns false for 0x7F..FF in clang AVX3 builds, so DIY.
template <typename TF>
bool IsNaN(TF f) {
  MakeUnsigned<TF> bits;
  memcpy(&bits, &f, sizeof(TF));
  bits += bits;
  bits >>= 1;  // clear sign bit
  // NaN if all exponent bits are set and the mantissa is not zero.
  return bits > ExponentMask<decltype(bits)>();
}

template <class D, class V>
void AssertNaN(const D d, const V v, const char* file, int line) {
  using T = TFromD<D>;
  const T lane = GetLane(v);
  if (!IsNaN(lane)) {
    const std::string type_name = TypeName(T(), Lanes(d));
    MakeUnsigned<T> bits;
    memcpy(&bits, &lane, sizeof(T));
    // RVV lacks PRIu64, so use size_t; double will be truncated on 32-bit.
    Abort(file, line, "Expected %s NaN, got %E (%zu)", type_name.c_str(), lane,
          size_t(bits));
  }
}

#define HWY_ASSERT_NAN(d, v) AssertNaN(d, v, __FILE__, __LINE__)

struct TestNaN {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v1 = Set(d, T(Unpredictable1()));
    const auto nan = IfThenElse(Eq(v1, Set(d, T(1))), NaN(d), v1);
    HWY_ASSERT_NAN(d, nan);

    // Arithmetic
    HWY_ASSERT_NAN(d, Add(nan, v1));
    HWY_ASSERT_NAN(d, Add(v1, nan));
    HWY_ASSERT_NAN(d, Sub(nan, v1));
    HWY_ASSERT_NAN(d, Sub(v1, nan));
    HWY_ASSERT_NAN(d, Mul(nan, v1));
    HWY_ASSERT_NAN(d, Mul(v1, nan));
    HWY_ASSERT_NAN(d, Div(nan, v1));
    HWY_ASSERT_NAN(d, Div(v1, nan));

    // FMA
    HWY_ASSERT_NAN(d, MulAdd(nan, v1, v1));
    HWY_ASSERT_NAN(d, MulAdd(v1, nan, v1));
    HWY_ASSERT_NAN(d, MulAdd(v1, v1, nan));
    HWY_ASSERT_NAN(d, MulSub(nan, v1, v1));
    HWY_ASSERT_NAN(d, MulSub(v1, nan, v1));
    HWY_ASSERT_NAN(d, MulSub(v1, v1, nan));
    HWY_ASSERT_NAN(d, NegMulAdd(nan, v1, v1));
    HWY_ASSERT_NAN(d, NegMulAdd(v1, nan, v1));
    HWY_ASSERT_NAN(d, NegMulAdd(v1, v1, nan));
    HWY_ASSERT_NAN(d, NegMulSub(nan, v1, v1));
    HWY_ASSERT_NAN(d, NegMulSub(v1, nan, v1));
    HWY_ASSERT_NAN(d, NegMulSub(v1, v1, nan));

    // Rcp/Sqrt
    HWY_ASSERT_NAN(d, Sqrt(nan));

    // Sign manipulation
    HWY_ASSERT_NAN(d, Abs(nan));
    HWY_ASSERT_NAN(d, Neg(nan));
    HWY_ASSERT_NAN(d, CopySign(nan, v1));
    HWY_ASSERT_NAN(d, CopySignToAbs(nan, v1));

    // Rounding
    HWY_ASSERT_NAN(d, Ceil(nan));
    HWY_ASSERT_NAN(d, Floor(nan));
    HWY_ASSERT_NAN(d, Round(nan));
    HWY_ASSERT_NAN(d, Trunc(nan));

    // Logical (And/AndNot/Xor will clear NaN!)
    HWY_ASSERT_NAN(d, Or(nan, v1));

    // Comparison
    HWY_ASSERT(AllFalse(Eq(nan, v1)));
    HWY_ASSERT(AllFalse(Gt(nan, v1)));
    HWY_ASSERT(AllFalse(Lt(nan, v1)));
    HWY_ASSERT(AllFalse(Ge(nan, v1)));
    HWY_ASSERT(AllFalse(Le(nan, v1)));
  }
};

// For functions only available for float32
struct TestF32NaN {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v1 = Set(d, T(Unpredictable1()));
    const auto nan = IfThenElse(Eq(v1, Set(d, T(1))), NaN(d), v1);
    HWY_ASSERT_NAN(d, ApproximateReciprocal(nan));
    HWY_ASSERT_NAN(d, ApproximateReciprocalSqrt(nan));
    HWY_ASSERT_NAN(d, AbsDiff(nan, v1));
    HWY_ASSERT_NAN(d, AbsDiff(v1, nan));
  }
};

// TODO(janwas): move to TestNaN once supported for partial vectors
struct TestFullNaN {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v1 = Set(d, T(Unpredictable1()));
    const auto nan = IfThenElse(Eq(v1, Set(d, T(1))), NaN(d), v1);

    HWY_ASSERT_NAN(d, SumOfLanes(nan));
// Reduction (pending clarification on RVV)
#if HWY_TARGET != HWY_RVV
    HWY_ASSERT_NAN(d, MinOfLanes(nan));
    HWY_ASSERT_NAN(d, MaxOfLanes(nan));
#endif

#if HWY_ARCH_X86 && HWY_TARGET != HWY_SCALAR
    // x86 SIMD returns the second operand if any input is NaN.
    HWY_ASSERT_VEC_EQ(d, v1, Min(nan, v1));
    HWY_ASSERT_VEC_EQ(d, v1, Max(nan, v1));
    HWY_ASSERT_NAN(d, Min(v1, nan));
    HWY_ASSERT_NAN(d, Max(v1, nan));
#elif HWY_ARCH_WASM
    // Should return NaN if any input is NaN, but does not for scalar.
    // TODO(janwas): remove once this is fixed.
#elif HWY_TARGET == HWY_NEON && !defined(__aarch64__)
    // ARMv7 NEON returns NaN if any input is NaN.
    HWY_ASSERT_NAN(d, Min(v1, nan));
    HWY_ASSERT_NAN(d, Max(v1, nan));
    HWY_ASSERT_NAN(d, Min(nan, v1));
    HWY_ASSERT_NAN(d, Max(nan, v1));
#else
    // IEEE 754-2019 minimumNumber is defined as the other argument if exactly
    // one is NaN, and qNaN if both are.
    HWY_ASSERT_VEC_EQ(d, v1, Min(nan, v1));
    HWY_ASSERT_VEC_EQ(d, v1, Max(nan, v1));
    HWY_ASSERT_VEC_EQ(d, v1, Min(v1, nan));
    HWY_ASSERT_VEC_EQ(d, v1, Max(v1, nan));
#endif
    HWY_ASSERT_NAN(d, Min(nan, nan));
    HWY_ASSERT_NAN(d, Max(nan, nan));

    // Comparison
    HWY_ASSERT(AllFalse(Eq(nan, v1)));
    HWY_ASSERT(AllFalse(Gt(nan, v1)));
    HWY_ASSERT(AllFalse(Lt(nan, v1)));
    HWY_ASSERT(AllFalse(Ge(nan, v1)));
    HWY_ASSERT(AllFalse(Le(nan, v1)));
  }
};

HWY_NOINLINE void TestAllNaN() {
  ForFloatTypes(ForPartialVectors<TestNaN>());
  ForPartialVectors<TestF32NaN>()(float());
  ForFloatTypes(ForFullVectors<TestFullNaN>());
}

struct TestCopyAndAssign {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    // copy V
    const auto v3 = Iota(d, 3);
    auto v3b(v3);
    HWY_ASSERT_VEC_EQ(d, v3, v3b);

    // assign V
    auto v3c = Undefined(d);
    v3c = v3;
    HWY_ASSERT_VEC_EQ(d, v3, v3c);
  }
};

HWY_NOINLINE void TestAllCopyAndAssign() {
  ForAllTypes(ForPartialVectors<TestCopyAndAssign>());
}

struct TestGetLane {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    HWY_ASSERT_EQ(T(0), GetLane(Zero(d)));
    HWY_ASSERT_EQ(T(1), GetLane(Set(d, 1)));
  }
};

HWY_NOINLINE void TestAllGetLane() {
  ForAllTypes(ForPartialVectors<TestGetLane>());
}


// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
HWY_BEFORE_TEST(HwyHwyTest);
HWY_EXPORT_AND_TEST_P(HwyHwyTest, TestAllFloorLog2);
HWY_EXPORT_AND_TEST_P(HwyHwyTest, TestAllSumMulAdd);
HWY_EXPORT_AND_TEST_P(HwyHwyTest, TestAllLimits);
HWY_EXPORT_AND_TEST_P(HwyHwyTest, TestAllLowestHighest);
HWY_EXPORT_AND_TEST_P(HwyHwyTest, TestAllType);
HWY_EXPORT_AND_TEST_P(HwyHwyTest, TestAllPopCount);
HWY_EXPORT_AND_TEST_P(HwyHwyTest, TestAllEqual);
HWY_EXPORT_AND_TEST_P(HwyHwyTest, TestAllSet);
HWY_EXPORT_AND_TEST_P(HwyHwyTest, TestAllOverflow);
HWY_EXPORT_AND_TEST_P(HwyHwyTest, TestAllSignBit);
HWY_EXPORT_AND_TEST_P(HwyHwyTest, TestAllName);
HWY_EXPORT_AND_TEST_P(HwyHwyTest, TestAllNaN);
HWY_EXPORT_AND_TEST_P(HwyHwyTest, TestAllCopyAndAssign);
HWY_EXPORT_AND_TEST_P(HwyHwyTest, TestAllGetLane);
HWY_AFTER_TEST();
#endif

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

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "highway_test.cc"
#include "hwy/foreach_target.h"
#include "hwy/highway.h"
#include "hwy/nanobenchmark.h"  // Unpredictable1
#include "hwy/tests/test_util-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

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
    HWY_ASSERT_VEC_EQ(d, vmax, Sub(vmin, v1));
    // Unsigned overflow / positive -> negative
    HWY_ASSERT_VEC_EQ(d, vmin, Add(vmax, v1));
  }
};

HWY_NOINLINE void TestAllOverflow() {
  ForIntegerTypes(ForPartialVectors<TestOverflow>());
}

struct TestClamp {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v0 = Zero(d);
    const auto v1 = Set(d, 1);
    const auto v2 = Set(d, 2);

    HWY_ASSERT_VEC_EQ(d, v1, Clamp(v2, v0, v1));
    HWY_ASSERT_VEC_EQ(d, v1, Clamp(v0, v1, v2));
  }
};

HWY_NOINLINE void TestAllClamp() {
  ForAllTypes(ForPartialVectors<TestClamp>());
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
HWY_NOINLINE void AssertNaN(D d, VecArg<V> v, const char* file, int line) {
  using T = TFromD<D>;
  const T lane = GetLane(v);
  if (!IsNaN(lane)) {
    const std::string type_name = TypeName(T(), Lanes(d));
    // RVV lacks PRIu64 and MSYS still has problems with %zu, so print bytes to
    // avoid truncating doubles.
    uint8_t bytes[HWY_MAX(sizeof(T), 8)] = {0};
    memcpy(bytes, &lane, sizeof(T));
    Abort(file, line,
          "Expected %s NaN, got %E (bytes %02x %02x %02x %02x %02x %02x %02x "
          "%02x)",
          type_name.c_str(), lane, bytes[0], bytes[1], bytes[2], bytes[3],
          bytes[4], bytes[5], bytes[6], bytes[7]);
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
    HWY_ASSERT(AllFalse(d, Eq(nan, v1)));
    HWY_ASSERT(AllFalse(d, Gt(nan, v1)));
    HWY_ASSERT(AllFalse(d, Lt(nan, v1)));
    HWY_ASSERT(AllFalse(d, Ge(nan, v1)));
    HWY_ASSERT(AllFalse(d, Le(nan, v1)));

    // Reduction
    HWY_ASSERT_NAN(d, SumOfLanes(d, nan));
// TODO(janwas): re-enable after QEMU is fixed
#if HWY_TARGET != HWY_RVV
    HWY_ASSERT_NAN(d, MinOfLanes(d, nan));
    HWY_ASSERT_NAN(d, MaxOfLanes(d, nan));
#endif

    // Min
#if HWY_ARCH_X86 && HWY_TARGET != HWY_SCALAR
    // x86 SIMD returns the second operand if any input is NaN.
    HWY_ASSERT_VEC_EQ(d, v1, Min(nan, v1));
    HWY_ASSERT_VEC_EQ(d, v1, Max(nan, v1));
    HWY_ASSERT_NAN(d, Min(v1, nan));
    HWY_ASSERT_NAN(d, Max(v1, nan));
#elif HWY_ARCH_WASM
    // Should return NaN if any input is NaN, but does not for scalar.
    // TODO(janwas): remove once this is fixed.
#elif HWY_TARGET == HWY_NEON && HWY_ARCH_ARM_V7
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

HWY_NOINLINE void TestAllNaN() {
  ForFloatTypes(ForPartialVectors<TestNaN>());
  ForPartialVectors<TestF32NaN>()(float());
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

struct TestDFromV {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v0 = Zero(d);
    using D0 = DFromV<decltype(v0)>;         // not necessarily same as D
    const auto v0b = And(v0, Set(D0(), 1));  // but vectors can interoperate
    HWY_ASSERT_VEC_EQ(d, v0, v0b);
  }
};

HWY_NOINLINE void TestAllDFromV() {
  ForAllTypes(ForPartialVectors<TestDFromV>());
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace hwy {
HWY_BEFORE_TEST(HighwayTest);
HWY_EXPORT_AND_TEST_P(HighwayTest, TestAllSet);
HWY_EXPORT_AND_TEST_P(HighwayTest, TestAllOverflow);
HWY_EXPORT_AND_TEST_P(HighwayTest, TestAllClamp);
HWY_EXPORT_AND_TEST_P(HighwayTest, TestAllSignBit);
HWY_EXPORT_AND_TEST_P(HighwayTest, TestAllNaN);
HWY_EXPORT_AND_TEST_P(HighwayTest, TestAllCopyAndAssign);
HWY_EXPORT_AND_TEST_P(HighwayTest, TestAllGetLane);
HWY_EXPORT_AND_TEST_P(HighwayTest, TestAllDFromV);
}  // namespace hwy

// Ought not to be necessary, but without this, no tests run on RVV.
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

#endif

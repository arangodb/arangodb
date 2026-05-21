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

#include <stdio.h>

#include <algorithm>  // std::fill
#include <bitset>
#include <string>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "highway_test.cc"
#include "hwy/foreach_target.h"    // IWYU pragma: keep
#include "hwy/highway.h"
#include "hwy/nanobenchmark.h"  // Unpredictable1
#include "hwy/tests/test_util-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

template <size_t kLimit, typename T>
HWY_NOINLINE void TestCappedLimit(T /* tag */) {
  CappedTag<T, kLimit> d;
  // Ensure two ops compile
  HWY_ASSERT_VEC_EQ(d, Zero(d), Set(d, T{0}));

  // Ensure we do not write more than kLimit lanes
  const size_t N = Lanes(d);
  if (kLimit < N) {
    auto lanes = AllocateAligned<T>(N);
    HWY_ASSERT(lanes);
    std::fill(lanes.get(), lanes.get() + N, T{0});
    Store(Set(d, T{1}), d, lanes.get());
    for (size_t i = kLimit; i < N; ++i) {
      HWY_ASSERT_EQ(lanes[i], T{0});
    }
  }
}

// Adapter for ForAllTypes - we are constructing our own Simd<> and thus do not
// use ForPartialVectors etc.
struct TestCapped {
  template <typename T>
  void operator()(T t) const {
    TestCappedLimit<1>(t);
    TestCappedLimit<3>(t);
    TestCappedLimit<5>(t);
    TestCappedLimit<1ull << 15>(t);
  }
};

HWY_NOINLINE void TestAllCapped() { ForAllTypes(TestCapped()); }

// For testing that ForPartialVectors reaches every possible size:
using NumLanesSet = std::bitset<HWY_MAX_BYTES + 1>;

// Monostate pattern because ForPartialVectors takes a template argument, not a
// functor by reference.
static NumLanesSet* NumLanesForSize(size_t sizeof_t) {
  HWY_ASSERT(sizeof_t <= sizeof(uint64_t));
  static NumLanesSet num_lanes[sizeof(uint64_t) + 1];
  return num_lanes + sizeof_t;
}
static size_t* MaxLanesForSize(size_t sizeof_t) {
  HWY_ASSERT(sizeof_t <= sizeof(uint64_t));
  static size_t num_lanes[sizeof(uint64_t) + 1] = {0};
  return num_lanes + sizeof_t;
}

struct TestMaxLanes {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);
    const size_t kMax = MaxLanes(d);  // for RVV, includes LMUL
    HWY_ASSERT(N <= kMax);
    HWY_ASSERT(kMax <= (HWY_MAX_BYTES / sizeof(T)));

    NumLanesForSize(sizeof(T))->set(N);
    *MaxLanesForSize(sizeof(T)) = HWY_MAX(*MaxLanesForSize(sizeof(T)), N);
  }
};

HWY_NOINLINE void TestAllMaxLanes() {
  ForAllTypes(ForPartialVectors<TestMaxLanes>());

  // Ensure ForPartialVectors visited all powers of two [1, N].
  for (size_t sizeof_t : {sizeof(uint8_t), sizeof(uint16_t), sizeof(uint32_t),
                          sizeof(uint64_t)}) {
    const size_t N = *MaxLanesForSize(sizeof_t);
    for (size_t i = 1; i <= N; i += i) {
      if (!NumLanesForSize(sizeof_t)->test(i)) {
        fprintf(stderr, "T=%d: did not visit for N=%d, max=%d\n",
                static_cast<int>(sizeof_t), static_cast<int>(i),
                static_cast<int>(N));
        HWY_ASSERT(false);
      }
    }
  }
}

struct TestSet {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    // Zero
    const Vec<D> v0 = Zero(d);
    const size_t N = Lanes(d);
    auto expected = AllocateAligned<T>(N);
    HWY_ASSERT(expected);
    std::fill(expected.get(), expected.get() + N, T{0});
    HWY_ASSERT_VEC_EQ(d, expected.get(), v0);

    // Set
    const Vec<D> v2 = Set(d, T{2});
    for (size_t i = 0; i < N; ++i) {
      expected[i] = 2;
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), v2);

    // Iota
    const Vec<D> vi = Iota(d, T(5));
    for (size_t i = 0; i < N; ++i) {
      expected[i] = T(5 + i);
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), vi);

    // Undefined. This may result in a 'using uninitialized memory' warning
    // here, even though we already suppress warnings in Undefined.
    HWY_DIAGNOSTICS(push)
    HWY_DIAGNOSTICS_OFF(disable : 4700, ignored "-Wuninitialized")
#if HWY_COMPILER_GCC_ACTUAL
    HWY_DIAGNOSTICS_OFF(disable : 4701, ignored "-Wmaybe-uninitialized")
#endif
    const Vec<D> vu = Undefined(d);
    Store(vu, d, expected.get());
    HWY_DIAGNOSTICS(pop)
  }
};

HWY_NOINLINE void TestAllSet() { ForAllTypes(ForPartialVectors<TestSet>()); }

// Ensures wraparound (mod 2^bits)
struct TestOverflow {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const Vec<D> v1 = Set(d, T{1});
    const Vec<D> vmax = Set(d, LimitsMax<T>());
    const Vec<D> vmin = Set(d, LimitsMin<T>());
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
    const Vec<D> v0 = Zero(d);
    const Vec<D> v1 = Set(d, T{1});
    const Vec<D> v2 = Set(d, T{2});

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
    const Vec<D> v0 = Zero(d);
    const Vec<D> all = VecFromMask(d, Eq(v0, v0));
    const Vec<D> vs = SignBit(d);
    const Vec<D> other = Sub(vs, Set(d, T{1}));

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
    const Vec<D> v0 = Zero(d);
    const Vec<D> vs = SignBit(d);
    const Vec<D> vp = Set(d, static_cast<T>(2.25));
    const Vec<D> vn = Set(d, static_cast<T>(-2.25));
    HWY_ASSERT_VEC_EQ(d, Or(vp, vs), vn);
    HWY_ASSERT_VEC_EQ(d, AndNot(vs, vn), vp);
    HWY_ASSERT_VEC_EQ(d, v0, vs);
  }
};

HWY_NOINLINE void TestAllSignBit() {
  ForIntegerTypes(ForPartialVectors<TestSignBitInteger>());
  ForFloatTypes(ForPartialVectors<TestSignBitFloat>());
}

// inline to work around incorrect SVE codegen (only first 128 bits used).
template <class D, class V>
HWY_INLINE void AssertNaN(D d, VecArg<V> v, const char* file, int line) {
  using T = TFromD<D>;
  const size_t N = Lanes(d);
  if (!AllTrue(d, IsNaN(v))) {
    Print(d, "not all NaN", v, 0, N);
    Print(d, "mask", VecFromMask(d, IsNaN(v)), 0, N);
    const std::string type_name = TypeName(T(), N);
    // RVV lacks PRIu64 and MSYS still has problems with %zu, so print bytes to
    // avoid truncating doubles.
    uint8_t bytes[HWY_MAX(sizeof(T), 8)] = {0};
    const T lane = GetLane(v);
    CopyBytes<sizeof(T)>(&lane, bytes);
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
    const Vec<D> v1 = Set(d, static_cast<T>(Unpredictable1()));
    const Vec<D> nan = IfThenElse(Eq(v1, Set(d, T{1})), NaN(d), v1);
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
    HWY_ASSERT_NAN(d, Set(d, ReduceSum(d, nan)));
// TODO(janwas): re-enable after QEMU/Spike are fixed
#if HWY_TARGET != HWY_RVV
    HWY_ASSERT_NAN(d, MinOfLanes(d, nan));
    HWY_ASSERT_NAN(d, MaxOfLanes(d, nan));
#endif

    // Min/Max
#if (HWY_ARCH_X86 || HWY_ARCH_WASM) && (HWY_TARGET < HWY_EMU128)
    // Native WASM or x86 SIMD return the second operand if any input is NaN.
    HWY_ASSERT_VEC_EQ(d, v1, Min(nan, v1));
    HWY_ASSERT_VEC_EQ(d, v1, Max(nan, v1));
    HWY_ASSERT_NAN(d, Min(v1, nan));
    HWY_ASSERT_NAN(d, Max(v1, nan));
#elif HWY_TARGET <= HWY_NEON_WITHOUT_AES && HWY_ARCH_ARM_V7
    // Armv7 NEON returns NaN if any input is NaN.
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
    const Vec<D> v1 = Set(d, static_cast<T>(Unpredictable1()));
    const Vec<D> nan = IfThenElse(Eq(v1, Set(d, T{1})), NaN(d), v1);
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

struct TestIsNaN {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const Vec<D> v1 = Set(d, static_cast<T>(Unpredictable1()));
    const Vec<D> inf = IfThenElse(Eq(v1, Set(d, T{1})), Inf(d), v1);
    const Vec<D> nan = IfThenElse(Eq(v1, Set(d, T{1})), NaN(d), v1);
    const Vec<D> neg = Set(d, T{-1});
    HWY_ASSERT_NAN(d, nan);
    HWY_ASSERT_MASK_EQ(d, MaskFalse(d), IsNaN(inf));
    HWY_ASSERT_MASK_EQ(d, MaskFalse(d), IsNaN(CopySign(inf, neg)));
    HWY_ASSERT_MASK_EQ(d, MaskTrue(d), IsNaN(nan));
    HWY_ASSERT_MASK_EQ(d, MaskTrue(d), IsNaN(CopySign(nan, neg)));
    HWY_ASSERT_MASK_EQ(d, MaskFalse(d), IsNaN(v1));
    HWY_ASSERT_MASK_EQ(d, MaskFalse(d), IsNaN(Zero(d)));
    HWY_ASSERT_MASK_EQ(d, MaskFalse(d), IsNaN(Set(d, hwy::LowestValue<T>())));
    HWY_ASSERT_MASK_EQ(d, MaskFalse(d), IsNaN(Set(d, hwy::HighestValue<T>())));
  }
};

HWY_NOINLINE void TestAllIsNaN() {
  ForFloatTypes(ForPartialVectors<TestIsNaN>());
}

struct TestIsInf {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const Vec<D> v1 = Set(d, static_cast<T>(Unpredictable1()));
    const Vec<D> inf = IfThenElse(Eq(v1, Set(d, T{1})), Inf(d), v1);
    const Vec<D> nan = IfThenElse(Eq(v1, Set(d, T{1})), NaN(d), v1);
    const Vec<D> neg = Set(d, T{-1});
    HWY_ASSERT_MASK_EQ(d, MaskTrue(d), IsInf(inf));
    HWY_ASSERT_MASK_EQ(d, MaskTrue(d), IsInf(CopySign(inf, neg)));
    HWY_ASSERT_MASK_EQ(d, MaskFalse(d), IsInf(nan));
    HWY_ASSERT_MASK_EQ(d, MaskFalse(d), IsInf(CopySign(nan, neg)));
    HWY_ASSERT_MASK_EQ(d, MaskFalse(d), IsInf(v1));
    HWY_ASSERT_MASK_EQ(d, MaskFalse(d), IsInf(Zero(d)));
    HWY_ASSERT_MASK_EQ(d, MaskFalse(d), IsInf(Set(d, hwy::LowestValue<T>())));
    HWY_ASSERT_MASK_EQ(d, MaskFalse(d), IsInf(Set(d, hwy::HighestValue<T>())));
  }
};

HWY_NOINLINE void TestAllIsInf() {
  ForFloatTypes(ForPartialVectors<TestIsInf>());
}

struct TestIsFinite {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const Vec<D> v1 = Set(d, static_cast<T>(Unpredictable1()));
    const Vec<D> inf = IfThenElse(Eq(v1, Set(d, T{1})), Inf(d), v1);
    const Vec<D> nan = IfThenElse(Eq(v1, Set(d, T{1})), NaN(d), v1);
    const Vec<D> neg = Set(d, T{-1});
    HWY_ASSERT_MASK_EQ(d, MaskFalse(d), IsFinite(inf));
    HWY_ASSERT_MASK_EQ(d, MaskFalse(d), IsFinite(CopySign(inf, neg)));
    HWY_ASSERT_MASK_EQ(d, MaskFalse(d), IsFinite(nan));
    HWY_ASSERT_MASK_EQ(d, MaskFalse(d), IsFinite(CopySign(nan, neg)));
    HWY_ASSERT_MASK_EQ(d, MaskTrue(d), IsFinite(v1));
    HWY_ASSERT_MASK_EQ(d, MaskTrue(d), IsFinite(Zero(d)));
    HWY_ASSERT_MASK_EQ(d, MaskTrue(d), IsFinite(Set(d, hwy::LowestValue<T>())));
    HWY_ASSERT_MASK_EQ(d, MaskTrue(d),
                       IsFinite(Set(d, hwy::HighestValue<T>())));
  }
};

HWY_NOINLINE void TestAllIsFinite() {
  ForFloatTypes(ForPartialVectors<TestIsFinite>());
}

struct TestCopyAndAssign {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    // copy V
    const Vec<D> v3 = Iota(d, 3);
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
    HWY_ASSERT_EQ(T{0}, GetLane(Zero(d)));
    HWY_ASSERT_EQ(T{1}, GetLane(Set(d, T{1})));
  }
};

HWY_NOINLINE void TestAllGetLane() {
  ForAllTypes(ForPartialVectors<TestGetLane>());
}

struct TestDFromV {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const Vec<D> v0 = Zero(d);
    using D0 = DFromV<decltype(v0)>;         // not necessarily same as D
    const Vec<D> v0b = And(v0, Set(D0(), T{1}));  // vectors can interoperate
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
HWY_EXPORT_AND_TEST_P(HighwayTest, TestAllCapped);
HWY_EXPORT_AND_TEST_P(HighwayTest, TestAllMaxLanes);
HWY_EXPORT_AND_TEST_P(HighwayTest, TestAllSet);
HWY_EXPORT_AND_TEST_P(HighwayTest, TestAllOverflow);
HWY_EXPORT_AND_TEST_P(HighwayTest, TestAllClamp);
HWY_EXPORT_AND_TEST_P(HighwayTest, TestAllSignBit);
HWY_EXPORT_AND_TEST_P(HighwayTest, TestAllNaN);
HWY_EXPORT_AND_TEST_P(HighwayTest, TestAllIsNaN);
HWY_EXPORT_AND_TEST_P(HighwayTest, TestAllIsInf);
HWY_EXPORT_AND_TEST_P(HighwayTest, TestAllIsFinite);
HWY_EXPORT_AND_TEST_P(HighwayTest, TestAllCopyAndAssign);
HWY_EXPORT_AND_TEST_P(HighwayTest, TestAllGetLane);
HWY_EXPORT_AND_TEST_P(HighwayTest, TestAllDFromV);
}  // namespace hwy

#endif

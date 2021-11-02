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

// Normal include guard for non-SIMD portion of this header.
#ifndef HWY_TESTS_TEST_UTIL_H_
#define HWY_TESTS_TEST_UTIL_H_

// Helper functions for use by *_test.cc.

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <cstddef>
#include <string>
#include <utility>  // std::tuple

#include "gtest/gtest.h"
#include "hwy/aligned_allocator.h"
#include "hwy/base.h"
#include "hwy/highway.h"

namespace hwy {

// The maximum vector size used in tests when defining test data. DEPRECATED.
constexpr size_t kTestMaxVectorSize = 64;

// googletest before 1.10 didn't define INSTANTIATE_TEST_SUITE_P() but instead
// used INSTANTIATE_TEST_CASE_P which is now deprecated.
#ifdef INSTANTIATE_TEST_SUITE_P
#define HWY_GTEST_INSTANTIATE_TEST_SUITE_P INSTANTIATE_TEST_SUITE_P
#else
#define HWY_GTEST_INSTANTIATE_TEST_SUITE_P INSTANTIATE_TEST_CASE_P
#endif

// Helper class to run parametric tests using the hwy target as parameter. To
// use this define the following in your test:
//   class MyTestSuite : public TestWithParamTarget {
//    ...
//   };
//   HWY_TARGET_INSTANTIATE_TEST_SUITE_P(MyTestSuite);
//   TEST_P(MyTestSuite, MyTest) { ... }
class TestWithParamTarget : public testing::TestWithParam<uint32_t> {
 protected:
  void SetUp() override { SetSupportedTargetsForTest(GetParam()); }

  void TearDown() override {
    // Check that the parametric test calls SupportedTargets() when the source
    // was compiled with more than one target. In the single-target case only
    // static dispatch will be used anyway.
#if (HWY_TARGETS & (HWY_TARGETS - 1)) != 0
    EXPECT_TRUE(SupportedTargetsCalledForTest())
        << "This hwy target parametric test doesn't use dynamic-dispatch and "
           "doesn't need to be parametric.";
#endif
    SetSupportedTargetsForTest(0);
  }
};

// Function to convert the test parameter of a TestWithParamTarget for
// displaying it in the gtest test name.
static inline std::string TestParamTargetName(
    const testing::TestParamInfo<uint32_t>& info) {
  return TargetName(info.param);
}

#define HWY_TARGET_INSTANTIATE_TEST_SUITE_P(suite)              \
  HWY_GTEST_INSTANTIATE_TEST_SUITE_P(                           \
      suite##Group, suite,                                      \
      testing::ValuesIn(::hwy::SupportedAndGeneratedTargets()), \
      ::hwy::TestParamTargetName)

// Helper class similar to TestWithParamTarget to run parametric tests that
// depend on the target and another parametric test. If you need to use multiple
// extra parameters use a std::tuple<> of them and ::testing::Generate(...) as
// the generator. To use this class define the following in your test:
//   class MyTestSuite : public TestWithParamTargetT<int> {
//    ...
//   };
//   HWY_TARGET_INSTANTIATE_TEST_SUITE_P_T(MyTestSuite, ::testing::Range(0, 9));
//   TEST_P(MyTestSuite, MyTest) { ... GetParam() .... }
template <typename T>
class TestWithParamTargetAndT
    : public ::testing::TestWithParam<std::tuple<uint32_t, T>> {
 public:
  // Expose the parametric type here so it can be used by the
  // HWY_TARGET_INSTANTIATE_TEST_SUITE_P_T macro.
  using HwyParamType = T;

 protected:
  void SetUp() override {
    SetSupportedTargetsForTest(std::get<0>(
        ::testing::TestWithParam<std::tuple<uint32_t, T>>::GetParam()));
  }

  void TearDown() override {
    // Check that the parametric test calls SupportedTargets() when the source
    // was compiled with more than one target. In the single-target case only
    // static dispatch will be used anyway.
#if (HWY_TARGETS & (HWY_TARGETS - 1)) != 0
    EXPECT_TRUE(SupportedTargetsCalledForTest())
        << "This hwy target parametric test doesn't use dynamic-dispatch and "
           "doesn't need to be parametric.";
#endif
    SetSupportedTargetsForTest(0);
  }

  T GetParam() {
    return std::get<1>(
        ::testing::TestWithParam<std::tuple<uint32_t, T>>::GetParam());
  }
};

template <typename T>
std::string TestParamTargetNameAndT(
    const testing::TestParamInfo<std::tuple<uint32_t, T>>& info) {
  return std::string(TargetName(std::get<0>(info.param))) + "_" +
         ::testing::PrintToString(std::get<1>(info.param));
}

#define HWY_TARGET_INSTANTIATE_TEST_SUITE_P_T(suite, generator)     \
  HWY_GTEST_INSTANTIATE_TEST_SUITE_P(                               \
      suite##Group, suite,                                          \
      ::testing::Combine(                                           \
          testing::ValuesIn(::hwy::SupportedAndGeneratedTargets()), \
          generator),                                               \
      ::hwy::TestParamTargetNameAndT<suite::HwyParamType>)

// Helper macro to export a function and define a test that tests it. This is
// equivalent to do a HWY_EXPORT of a void(void) function and run it in a test:
//   class MyTestSuite : public TestWithParamTarget {
//    ...
//   };
//   HWY_TARGET_INSTANTIATE_TEST_SUITE_P(MyTestSuite);
//   HWY_EXPORT_AND_TEST_P(MyTestSuite, MyTest);
#define HWY_EXPORT_AND_TEST_P(suite, func_name)                   \
  HWY_EXPORT(func_name);                                          \
  TEST_P(suite, func_name) { HWY_DYNAMIC_DISPATCH(func_name)(); } \
  static_assert(true, "For requiring trailing semicolon")

#define HWY_EXPORT_AND_TEST_P_T(suite, func_name)                           \
  HWY_EXPORT(func_name);                                                    \
  TEST_P(suite, func_name) { HWY_DYNAMIC_DISPATCH(func_name)(GetParam()); } \
  static_assert(true, "For requiring trailing semicolon")

#define HWY_BEFORE_TEST(suite)                      \
  class suite : public hwy::TestWithParamTarget {}; \
  HWY_TARGET_INSTANTIATE_TEST_SUITE_P(suite);       \
  static_assert(true, "For requiring trailing semicolon")

// 64-bit random generator (Xorshift128+). Much smaller state than std::mt19937,
// which triggers a compiler bug.
class RandomState {
 public:
  explicit RandomState(const uint64_t seed = 0x123456789ull) {
    s0_ = SplitMix64(seed + 0x9E3779B97F4A7C15ull);
    s1_ = SplitMix64(s0_);
  }

  HWY_INLINE uint64_t operator()() {
    uint64_t s1 = s0_;
    const uint64_t s0 = s1_;
    const uint64_t bits = s1 + s0;
    s0_ = s0;
    s1 ^= s1 << 23;
    s1 ^= s0 ^ (s1 >> 18) ^ (s0 >> 5);
    s1_ = s1;
    return bits;
  }

 private:
  static uint64_t SplitMix64(uint64_t z) {
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
  }

  uint64_t s0_;
  uint64_t s1_;
};

static HWY_INLINE uint32_t Random32(RandomState* rng) {
  return static_cast<uint32_t>((*rng)());
}

static HWY_INLINE uint64_t Random64(RandomState* rng) {
  return (*rng)();
}

// Prevents the compiler from eliding the computations that led to "output".
// Works by indicating to the compiler that "output" is being read and modified.
// The +r constraint avoids unnecessary writes to memory, but only works for
// built-in types.
template <class T>
inline void PreventElision(T&& output) {
#if HWY_COMPILER_MSVC
  (void)output;
#else   // HWY_COMPILER_MSVC
  asm volatile("" : "+r"(output) : : "memory");
#endif  // HWY_COMPILER_MSVC
}

// Returns a name for the vector/part/scalar. The type prefix is u/i/f for
// unsigned/signed/floating point, followed by the number of bits per lane;
// then 'x' followed by the number of lanes. Example: u8x16. This is useful for
// understanding which instantiation of a generic test failed.
template <typename T>
static inline std::string TypeName(T /*unused*/, size_t N) {
  const char prefix = IsFloat<T>() ? 'f' : (IsSigned<T>() ? 'i' : 'u');
  char name[64];
  // Omit the xN suffix for scalars.
  if (N == 1) {
    snprintf(name, sizeof(name), "%c%zu", prefix, sizeof(T) * 8);
  } else {
    snprintf(name, sizeof(name), "%c%zux%zu", prefix, sizeof(T) * 8, N);
  }
  return name;
}

// String comparison

template <typename T1, typename T2>
inline bool BytesEqual(const T1* p1, const T2* p2, const size_t size,
                       size_t* pos = nullptr) {
  const uint8_t* bytes1 = reinterpret_cast<const uint8_t*>(p1);
  const uint8_t* bytes2 = reinterpret_cast<const uint8_t*>(p2);
  for (size_t i = 0; i < size; ++i) {
    if (bytes1[i] != bytes2[i]) {
      fprintf(stderr, "Mismatch at byte %zu of %zu: %d != %d (%s, %s)\n", i,
              size, bytes1[i], bytes2[i], TypeName(T1(), 1).c_str(),
              TypeName(T2(), 1).c_str());
      if (pos != nullptr) {
        *pos = i;
      }
      return false;
    }
  }
  return true;
}

inline bool StringsEqual(const char* s1, const char* s2) {
  while (*s1 == *s2++) {
    if (*s1++ == '\0') return true;
  }
  return false;
}

}  // namespace hwy

#endif  // HWY_TESTS_TEST_UTIL_H_

// Per-target include guard
#if defined(HIGHWAY_HWY_TESTS_TEST_UTIL_INL_H_) == defined(HWY_TARGET_TOGGLE)
#ifdef HIGHWAY_HWY_TESTS_TEST_UTIL_INL_H_
#undef HIGHWAY_HWY_TESTS_TEST_UTIL_INL_H_
#else
#define HIGHWAY_HWY_TESTS_TEST_UTIL_INL_H_
#endif

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_NOINLINE void PrintValue(T value) {
  uint8_t byte;
  CopyBytes<1>(&value, &byte);  // endian-safe: we ensured sizeof(T)=1.
  fprintf(stderr, "0x%02X,", byte);
}

#if HWY_CAP_FLOAT16
HWY_NOINLINE void PrintValue(float16_t value) {
  uint16_t bits;
  CopyBytes<2>(&value, &bits);
  fprintf(stderr, "0x%02X,", bits);
}
#endif

template <typename T, HWY_IF_NOT_LANE_SIZE(T, 1)>
HWY_NOINLINE void PrintValue(T value) {
  fprintf(stderr, "%g,", double(value));
}

// Prints lanes around `lane`, in memory order.
template <class D, class V = Vec<D>>
HWY_NOINLINE void Print(const D d, const char* caption, VecArg<V> v,
                        size_t lane_u = 0, size_t max_lanes = 7) {
  using T = TFromD<D>;
  const size_t N = Lanes(d);
  auto lanes = AllocateAligned<T>(N);
  Store(v, d, lanes.get());
  const intptr_t lane = intptr_t(lane_u);
  const size_t begin = static_cast<size_t>(HWY_MAX(0, lane - 2));
  const size_t end = HWY_MIN(begin + max_lanes, N);
  fprintf(stderr, "%s %s [%zu+ ->]:\n  ", TypeName(T(), N).c_str(), caption,
          begin);
  for (size_t i = begin; i < end; ++i) {
    PrintValue(lanes[i]);
  }
  if (begin >= end) fprintf(stderr, "(out of bounds)");
  fprintf(stderr, "\n");
}

static HWY_NORETURN HWY_NOINLINE void NotifyFailure(
    const char* filename, const int line, const char* type_name,
    const size_t lane, const char* expected, const char* actual) {
  hwy::Abort(filename, line,
             "%s, %s lane %zu mismatch: expected '%s', got '%s'.\n",
             hwy::TargetName(HWY_TARGET), type_name, lane, expected, actual);
}

template <class Out, class In>
inline Out BitCast(const In& in) {
  static_assert(sizeof(Out) == sizeof(In), "");
  Out out;
  CopyBytes<sizeof(out)>(&in, &out);
  return out;
}

// Computes the difference in units of last place between x and y.
template <typename TF>
MakeUnsigned<TF> ComputeUlpDelta(TF x, TF y) {
  static_assert(IsFloat<TF>(), "Only makes sense for floating-point");
  using TU = MakeUnsigned<TF>;

  // Handle -0 == 0 and infinities.
  if (x == y) return 0;

  // Consider "equal" if both are NaN, so we can verify an expected NaN.
  // Needs a special case because there are many possible NaN representations.
  if (std::isnan(x) && std::isnan(y)) return 0;

  // NOTE: no need to check for differing signs; they will result in large
  // differences, which is fine, and we avoid overflow.

  const TU ux = BitCast<TU>(x);
  const TU uy = BitCast<TU>(y);
  // Avoid unsigned->signed cast: 2's complement is only guaranteed by C++20.
  return HWY_MAX(ux, uy) - HWY_MIN(ux, uy);
}

template <typename T, HWY_IF_NOT_FLOAT(T)>
HWY_NOINLINE bool IsEqual(const T expected, const T actual) {
  return memcmp(&expected, &actual, sizeof(T)) == 0;
}

template <typename T, HWY_IF_FLOAT(T)>
HWY_NOINLINE bool IsEqual(const T expected, const T actual) {
  return ComputeUlpDelta(expected, actual) <= 1;
}

// Compare non-vector, non-string T.
template <typename T>
HWY_NOINLINE void AssertEqual(const T expected, const T actual,
                              const std::string& type_name,
                              const char* filename = "", const int line = -1,
                              const size_t lane = 0) {
  if (!IsEqual(expected, actual)) {
    char expected_str[100];
    snprintf(expected_str, sizeof(expected_str), "%g", double(expected));
    char actual_str[100];
    snprintf(actual_str, sizeof(actual_str), "%g", double(actual));
    NotifyFailure(filename, line, type_name.c_str(), lane, expected_str,
                  actual_str);
  }
}

static HWY_NOINLINE HWY_MAYBE_UNUSED void AssertStringEqual(
    const char* expected, const char* actual, const char* filename = "",
    const int line = -1, const size_t lane = 0) {
  if (!hwy::StringsEqual(expected, actual)) {
    NotifyFailure(filename, line, "string", lane, expected, actual);
  }
}

// Compare expected vector to vector.
template <class D, class V>
HWY_NOINLINE void AssertVecEqual(D d, VecArg<V> expected, VecArg<V> actual,
                                 const char* filename, const int line) {
  using T = TFromD<D>;
  const size_t N = Lanes(d);
  auto expected_lanes = AllocateAligned<T>(N);
  auto actual_lanes = AllocateAligned<T>(N);
  Store(expected, d, expected_lanes.get());
  Store(actual, d, actual_lanes.get());
  for (size_t i = 0; i < N; ++i) {
    if (!IsEqual(expected_lanes[i], actual_lanes[i])) {
      fprintf(stderr, "\n\n");
      Print(d, "expect", expected, i);
      Print(d, "actual", actual, i);

      char expected_str[100];
      snprintf(expected_str, sizeof(expected_str), "%g",
               double(expected_lanes[i]));
      char actual_str[100];
      snprintf(actual_str, sizeof(actual_str), "%g", double(actual_lanes[i]));

      NotifyFailure(filename, line, hwy::TypeName(T(), N).c_str(), i,
                    expected_str, actual_str);
    }
  }
}

// Compare expected lanes to vector.
template <class D>
HWY_NOINLINE void AssertVecEqual(D d, const TFromD<D>* expected,
                                 VecArg<Vec<D>> actual, const char* filename,
                                 int line) {
  AssertVecEqual(d, LoadU(d, expected), actual, filename, line);
}

// Only checks the valid mask elements (those whose index < Lanes(d)).
template <class D>
HWY_NOINLINE void AssertMaskEqual(D d, VecArg<Mask<D>> a, VecArg<Mask<D>> b,
                                  const char* filename, int line) {
  AssertVecEqual(d, VecFromMask(d, a), VecFromMask(d, b), filename, line);

  const std::string type_name = TypeName(TFromD<D>(), Lanes(d));
  AssertEqual(CountTrue(d, a), CountTrue(d, b), type_name, filename, line, 0);
  AssertEqual(AllTrue(d, a), AllTrue(d, b), type_name, filename, line, 0);
  AssertEqual(AllFalse(d, a), AllFalse(d, b), type_name, filename, line, 0);

  // TODO(janwas): remove RVV once implemented (cast or vse1)
#if HWY_TARGET != HWY_RVV && HWY_TARGET != HWY_SCALAR
  const size_t N = Lanes(d);
  const Repartition<uint8_t, D> d8;
  const size_t N8 = Lanes(d8);
  auto bits_a = AllocateAligned<uint8_t>(HWY_MAX(8, N8));
  auto bits_b = AllocateAligned<uint8_t>(HWY_MAX(8, N8));
  memset(bits_a.get(), 0, N8);
  memset(bits_b.get(), 0, N8);
  const size_t num_bytes_a = StoreMaskBits(d, a, bits_a.get());
  const size_t num_bytes_b = StoreMaskBits(d, b, bits_b.get());
  AssertEqual(num_bytes_a, num_bytes_b, type_name, filename, line, 0);
  size_t i = 0;
  // First check whole bytes (if that many elements are still valid)
  for (; i < N / 8; ++i) {
    if (bits_a[i] != bits_b[i]) {
      fprintf(stderr, "Mismatch in byte %zu: %d != %d\n", i, bits_a[i],
              bits_b[i]);
      Print(d8, "expect", Load(d8, bits_a.get()), 0, N8);
      Print(d8, "actual", Load(d8, bits_b.get()), 0, N8);
      hwy::Abort(filename, line, "Masks not equal");
    }
  }
  // Then the valid bit(s) in the last byte.
  const size_t remainder = N % 8;
  if (remainder != 0) {
    const int mask = (1 << remainder) - 1;
    const int valid_a = bits_a[i] & mask;
    const int valid_b = bits_b[i] & mask;
    if (valid_a != valid_b) {
      fprintf(stderr, "Mismatch in last byte %zu: %d != %d\n", i, valid_a,
              valid_b);
      Print(d8, "expect", Load(d8, bits_a.get()), 0, N8);
      Print(d8, "actual", Load(d8, bits_b.get()), 0, N8);
      hwy::Abort(filename, line, "Masks not equal");
    }
  }
#endif
}

// Only sets valid elements (those whose index < Lanes(d)). This helps catch
// tests that are not masking off the (undefined) upper mask elements.
//
// TODO(janwas): with HWY_NOINLINE GCC zeros the upper half of AVX2 masks.
template <class D>
HWY_INLINE Mask<D> MaskTrue(const D d) {
  return FirstN(d, Lanes(d));
}

template <class D>
HWY_INLINE Mask<D> MaskFalse(const D d) {
  const auto zero = Zero(RebindToSigned<D>());
  return RebindMask(d, Lt(zero, zero));
}

#ifndef HWY_ASSERT_EQ

#define HWY_ASSERT_EQ(expected, actual) \
  AssertEqual(expected, actual, hwy::TypeName(expected, 1), __FILE__, __LINE__)

#define HWY_ASSERT_STRING_EQ(expected, actual) \
  AssertStringEqual(expected, actual, __FILE__, __LINE__)

#define HWY_ASSERT_VEC_EQ(d, expected, actual) \
  AssertVecEqual(d, expected, actual, __FILE__, __LINE__)

#define HWY_ASSERT_MASK_EQ(d, expected, actual) \
  AssertMaskEqual(d, expected, actual, __FILE__, __LINE__)

#endif  // HWY_ASSERT_EQ

// Helpers for instantiating tests with combinations of lane types / counts.

// For ensuring we do not call tests with D such that widening D results in 0
// lanes. Example: assume T=u32, VLEN=256, and fraction=1/8: there is no 1/8th
// of a u64 vector in this case.
template <class D, HWY_IF_NOT_LANE_SIZE_D(D, 8)>
HWY_INLINE size_t PromotedLanes(const D d) {
  return Lanes(RepartitionToWide<decltype(d)>());
}
// Already the widest possible T, cannot widen.
template <class D, HWY_IF_LANE_SIZE_D(D, 8)>
HWY_INLINE size_t PromotedLanes(const D d) {
  return Lanes(d);
}

// For all power of two N in [kMinLanes, kMul * kMinLanes] (so that recursion
// stops at kMul == 0). Note that N may be capped or a fraction.
template <typename T, size_t kMul, size_t kMinLanes, class Test,
          bool kPromote = false>
struct ForeachSizeR {
  static void Do() {
    const Simd<T, kMul * kMinLanes> d;

    // Skip invalid fractions (e.g. 1/8th of u32x4).
    const size_t lanes = kPromote ? PromotedLanes(d) : Lanes(d);
    if (lanes == 0) return;

    Test()(T(), d);

    static_assert(kMul != 0, "Recursion should have ended already");
    ForeachSizeR<T, kMul / 2, kMinLanes, Test, kPromote>::Do();
  }
};

// Base case to stop the recursion.
template <typename T, size_t kMinLanes, class Test, bool kPromote>
struct ForeachSizeR<T, 0, kMinLanes, Test, kPromote> {
  static void Do() {}
};

// These adapters may be called directly, or via For*Types:

// Calls Test for all power of two N in [1, Lanes(d) / kFactor]. This is for
// ops that widen their input, e.g. Combine (not supported by HWY_SCALAR).
template <class Test, size_t kFactor = 2>
struct ForExtendableVectors {
  template <typename T>
  void operator()(T /*unused*/) const {
#if HWY_TARGET == HWY_SCALAR
    // not supported
#else
    constexpr bool kPromote = true;
#if HWY_TARGET == HWY_RVV
    ForeachSizeR<T, 8 / kFactor, HWY_LANES(T), Test, kPromote>::Do();
    // TODO(janwas): also capped
    // ForeachSizeR<T, (16 / sizeof(T)) / kFactor, 1, Test, kPromote>::Do();
#elif HWY_TARGET == HWY_SVE || HWY_TARGET == HWY_SVE2
    // Capped
    ForeachSizeR<T, (16 / sizeof(T)) / kFactor, 1, Test, kPromote>::Do();
    // Fractions
    ForeachSizeR<T, 8 / kFactor, HWY_LANES(T) / 8, Test, kPromote>::Do();
#else
    ForeachSizeR<T, HWY_LANES(T) / kFactor, 1, Test, kPromote>::Do();
#endif
#endif  // HWY_SCALAR
  }
};

// Calls Test for all power of two N in [kFactor, Lanes(d)]. This is for ops
// that narrow their input, e.g. UpperHalf.
template <class Test, size_t kFactor = 2>
struct ForShrinkableVectors {
  template <typename T>
  void operator()(T /*unused*/) const {
#if HWY_TARGET == HWY_SCALAR
    // not supported
#elif HWY_TARGET == HWY_RVV
    ForeachSizeR<T, 8 / kFactor, kFactor * HWY_LANES(T), Test>::Do();
    // TODO(janwas): also capped
#elif HWY_TARGET == HWY_SVE || HWY_TARGET == HWY_SVE2
    // Capped
    ForeachSizeR<T, (16 / sizeof(T)) / kFactor, kFactor, Test>::Do();
    // Fractions
    ForeachSizeR<T, 8 / kFactor, kFactor * HWY_LANES(T) / 8, Test>::Do();
#elif HWY_TARGET == HWY_SCALAR
    // not supported
#else
    ForeachSizeR<T, HWY_LANES(T) / kFactor, kFactor, Test>::Do();
#endif
  }
};

// Calls Test for all power of two N in [16 / sizeof(T), Lanes(d)]. This is for
// ops that require at least 128 bits, e.g. AES or 64x64 = 128 mul.
template <class Test>
struct ForGE128Vectors {
  template <typename T>
  void operator()(T /*unused*/) const {
#if HWY_TARGET == HWY_SCALAR
    // not supported
#elif HWY_TARGET == HWY_RVV
    ForeachSizeR<T, 8, HWY_LANES(T), Test>::Do();
    // TODO(janwas): also capped
    // ForeachSizeR<T, 1, (16 / sizeof(T)), Test>::Do();
#elif HWY_TARGET == HWY_SVE || HWY_TARGET == HWY_SVE2
    // Capped
    ForeachSizeR<T, 1, 16 / sizeof(T), Test>::Do();
    // Fractions
    ForeachSizeR<T, 8, HWY_LANES(T) / 8, Test>::Do();
#else
    ForeachSizeR<T, HWY_LANES(T) / (16 / sizeof(T)), (16 / sizeof(T)),
                 Test>::Do();
#endif
  }
};

// Calls Test for all power of two N in [8 / sizeof(T), Lanes(d)]. This is for
// ops that require at least 64 bits, e.g. casts.
template <class Test>
struct ForGE64Vectors {
  template <typename T>
  void operator()(T /*unused*/) const {
#if HWY_TARGET == HWY_SCALAR
    // not supported
#elif HWY_TARGET == HWY_RVV
    ForeachSizeR<T, 8, HWY_LANES(T), Test>::Do();
    // TODO(janwas): also capped
    // ForeachSizeR<T, 1, (8 / sizeof(T)), Test>::Do();
#elif HWY_TARGET == HWY_SVE || HWY_TARGET == HWY_SVE2
    // Capped
    ForeachSizeR<T, 1, 8 / sizeof(T), Test>::Do();
    // Fractions
    ForeachSizeR<T, 8, HWY_LANES(T) / 8, Test>::Do();
#else
    ForeachSizeR<T, HWY_LANES(T) / (8 / sizeof(T)), (8 / sizeof(T)),
                 Test>::Do();
#endif
  }
};

// Calls Test for all N that can be promoted (not the same as Extendable because
// HWY_SCALAR has one lane). Also used for ZipLower, but not ZipUpper.
template <class Test, size_t kFactor = 2>
struct ForPromoteVectors {
  template <typename T>
  void operator()(T /*unused*/) const {
#if HWY_TARGET == HWY_SCALAR
    ForeachSizeR<T, 1, 1, Test, /*kPromote=*/true>::Do();
#else
    return ForExtendableVectors<Test, kFactor>()(T());
#endif
  }
};

// Calls Test for all N than can be demoted (not the same as Shrinkable because
// HWY_SCALAR has one lane). Also used for LowerHalf, but not UpperHalf.
template <class Test, size_t kFactor = 2>
struct ForDemoteVectors {
  template <typename T>
  void operator()(T /*unused*/) const {
#if HWY_TARGET == HWY_SCALAR
    ForeachSizeR<T, 1, 1, Test>::Do();
#else
    return ForShrinkableVectors<Test, kFactor>()(T());
#endif
  }
};

// Calls Test for all power of two N in [1, Lanes(d)]. This is the default
// for ops that do not narrow nor widen their input, nor require 128 bits.
template <class Test>
struct ForPartialVectors {
  template <typename T>
  void operator()(T t) const {
    ForExtendableVectors<Test, 1>()(t);
  }
};

// Type lists to shorten call sites:

template <class Func>
void ForSignedTypes(const Func& func) {
  func(int8_t());
  func(int16_t());
  func(int32_t());
#if HWY_CAP_INTEGER64
  func(int64_t());
#endif
}

template <class Func>
void ForUnsignedTypes(const Func& func) {
  func(uint8_t());
  func(uint16_t());
  func(uint32_t());
#if HWY_CAP_INTEGER64
  func(uint64_t());
#endif
}

template <class Func>
void ForIntegerTypes(const Func& func) {
  ForSignedTypes(func);
  ForUnsignedTypes(func);
}

template <class Func>
void ForFloatTypes(const Func& func) {
  func(float());
#if HWY_CAP_FLOAT64
  func(double());
#endif
}

template <class Func>
void ForAllTypes(const Func& func) {
  ForIntegerTypes(func);
  ForFloatTypes(func);
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#endif  // per-target include guard

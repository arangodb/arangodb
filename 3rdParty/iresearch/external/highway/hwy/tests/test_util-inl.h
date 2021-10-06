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

#include <cmath>  // isfinite
#include <cstddef>
#include <string>
#include <utility>  // std::forward

#include "hwy/aligned_allocator.h"
#include "hwy/base.h"
#include "hwy/highway.h"

#ifndef HWY_TEST_STANDALONE
#if HWY_ARCH_RVV
// TODO(janwas): remove once gTest is supported.
#define HWY_TEST_STANDALONE 1
#else
#define HWY_TEST_STANDALONE 0
#endif
#endif

#if !HWY_TEST_STANDALONE
#include "gtest/gtest.h"
#endif

namespace hwy {

// The maximum vector size used in tests when defining test data. DEPRECATED.
constexpr size_t kTestMaxVectorSize = 64;

#if !HWY_TEST_STANDALONE

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
std::string TestParamTargetName(const testing::TestParamInfo<uint32_t>& info) {
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
  namespace hwy {                                   \
  class suite : public hwy::TestWithParamTarget {}; \
  HWY_TARGET_INSTANTIATE_TEST_SUITE_P(suite);       \
  static_assert(true, "For requiring trailing semicolon")

#define HWY_AFTER_TEST() \
  } /* namespace hwy */       \
  static_assert(true, "For requiring trailing semicolon")

#else

// Cannot be a function, otherwise the HWY_EXPORT table defined here will not
// be visible to HWY_DYNAMIC_DISPATCH.
#define HWY_EXPORT_AND_TEST_P(suite, func_name)            \
  HWY_EXPORT(func_name);                                   \
  SetSupportedTargetsForTest(0);                           \
  for (uint32_t target : SupportedAndGeneratedTargets()) { \
    SetSupportedTargetsForTest(target);                    \
    fprintf(stderr, "=== %s for %s:\n", #func_name,        \
            TargetName(static_cast<int>(target)));         \
    HWY_DYNAMIC_DISPATCH(func_name)();                     \
  }                                                        \
  /* Disable the mask after the test. */                   \
  SetSupportedTargetsForTest(0);                           \
  static_assert(true, "For requiring trailing semicolon")

#define HWY_BEFORE_TEST(suite) \
  namespace hwy {              \
  void RunAll() {              \
    static_assert(true, "For requiring trailing semicolon")

#define HWY_AFTER_TEST()               \
  } /* RunAll*/                             \
  } /* namespace hwy */                     \
  int main(int /*argc*/, char** /*argv*/) { \
    hwy::RunAll();                          \
    fprintf(stderr, "Success.\n");          \
    return 0;                               \
  }                                         \
  static_assert(true, "For requiring trailing semicolon")

// TODO(janwas): this only works for tests with a single TEST.
#define TEST(suite, name) void main()
#endif

// Calls test for each enabled and available target.
template <class Func, typename... Args>
HWY_NOINLINE void RunTest(const Func& func, Args&&... args) {
  SetSupportedTargetsForTest(0);
  auto targets = SupportedAndGeneratedTargets();

  for (uint32_t target : targets) {
    SetSupportedTargetsForTest(target);
    fprintf(stderr, "Testing for target %s.\n",
            TargetName(static_cast<int>(target)));
    func(std::forward<Args>(args)...);
  }
  // Disable the mask after the test.
  SetSupportedTargetsForTest(0);
}

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

// Prevents the compiler from eliding the computations that led to "output".
// Works by indicating to the compiler that "output" is being read and modified.
// The +r constraint avoids unnecessary writes to memory, but only works for
// built-in types.
template <class T>
inline void PreventElision(T&& output) {
#ifndef _MSC_VER
  asm volatile("" : "+r"(output) : : "memory");
#endif
}

// Returns a name for the vector/part/scalar. The type prefix is u/i/f for
// unsigned/signed/floating point, followed by the number of bits per lane;
// then 'x' followed by the number of lanes. Example: u8x16. This is useful for
// understanding which instantiation of a generic test failed.
template <typename T>
static inline std::string TypeName(T /*unused*/, size_t N) {
  std::string prefix(IsFloat<T>() ? "f" : (IsSigned<T>() ? "i" : "u"));
  prefix += std::to_string(sizeof(T) * 8);

  // Scalars: omit the xN suffix.
  if (N == 1) return prefix;

  return prefix + 'x' + std::to_string(N);
}

// String comparison

template <typename T1, typename T2>
inline bool BytesEqual(const T1* p1, const T2* p2, const size_t size) {
  const uint8_t* bytes1 = reinterpret_cast<const uint8_t*>(p1);
  const uint8_t* bytes2 = reinterpret_cast<const uint8_t*>(p2);
  for (size_t i = 0; i < size; ++i) {
    if (bytes1[i] != bytes2[i]) return false;
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

// Prints lanes around `lane`, in memory order.
template <class D>
HWY_NOINLINE void Print(const D d, const char* caption, const Vec<D> v,
                        intptr_t lane = 0) {
  using T = TFromD<D>;
  const size_t N = Lanes(d);
  auto lanes = AllocateAligned<T>(N);
  Store(v, d, lanes.get());
  const size_t begin = static_cast<size_t>(std::max<intptr_t>(0, lane - 2));
  const size_t end = std::min(begin + 5, N);
  fprintf(stderr, "%s %s [%zu+ ->]:\n  ", TypeName(T(), N).c_str(), caption,
          begin);
  for (size_t i = begin; i < end; ++i) {
    fprintf(stderr, "%s,", std::to_string(lanes[i]).c_str());
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
  return std::max(ux, uy) - std::min(ux, uy);
}

template <typename T, HWY_IF_NOT_FLOAT(T)>
HWY_NOINLINE bool IsEqual(const T expected, const T actual) {
  return expected == actual;
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
    const std::string expected_str = std::to_string(expected);
    const std::string actual_str = std::to_string(actual);
    NotifyFailure(filename, line, type_name.c_str(), lane, expected_str.c_str(),
                  actual_str.c_str());
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
HWY_NOINLINE void AssertVecEqual(D d, const V expected, const V actual,
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
      NotifyFailure(filename, line, hwy::TypeName(T(), N).c_str(), i,
                    std::to_string(expected_lanes[i]).c_str(),
                    std::to_string(actual_lanes[i]).c_str());
    }
  }
}

// Compare expected lanes to vector.
template <class D>
HWY_NOINLINE void AssertVecEqual(D d, const TFromD<D>* expected, Vec<D> actual,
                                 const char* filename, int line) {
  AssertVecEqual(d, LoadU(d, expected), actual, filename, line);
}

template <class D>
void AssertMaskEqual(D d, Mask<D> a, Mask<D> b, const char* filename,
                     int line) {
  AssertVecEqual(d, VecFromMask(d, a), VecFromMask(d, b), filename, line);

  const std::string type_name = TypeName(TFromD<D>(), Lanes(d));
  AssertEqual(CountTrue(a), CountTrue(b), type_name, filename, line, 0);
  AssertEqual(AllTrue(a), AllTrue(b), type_name, filename, line, 0);
  AssertEqual(AllFalse(a), AllFalse(b), type_name, filename, line, 0);

  // TODO(janwas): StoreMaskBits
}

template <class D>
HWY_NOINLINE Mask<D> MaskTrue(const D d) {
  const auto v0 = Zero(d);
  return Eq(v0, v0);
}

template <class D>
HWY_NOINLINE Mask<D> MaskFalse(const D d) {
  // Lt is only for signed types and we cannot yet cast mask types.
  return Eq(Zero(d), Set(d, 1));
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

// For all powers of two in [kMinLanes, N * kMinLanes] (so that recursion stops
// at N == 0)
template <typename T, size_t N, size_t kMinLanes, class Test>
struct ForeachSizeR {
  static void Do() {
    static_assert(N != 0, "End of recursion");
    Test()(T(), Simd<T, N * kMinLanes>());
    ForeachSizeR<T, N / 2, kMinLanes, Test>::Do();
  }
};

// Base case to stop the recursion.
template <typename T, size_t kMinLanes, class Test>
struct ForeachSizeR<T, 0, kMinLanes, Test> {
  static void Do() {}
};

// These adapters may be called directly, or via For*Types:

// Calls Test for all powers of two in [kMinLanes, kMaxLanes / kDivLanes].
// kMaxLanes is used for HWY_GATHER_LANES etc; use a large default because we
// don't have access to T in the template argument list.
template <class Test, size_t kDivLanes = 1, size_t kMinLanes = 1,
          size_t kMaxLanes = 1ul << 30>
struct ForPartialVectors {
  template <typename T>
  void operator()(T /*unused*/) const {
#if HWY_TARGET == HWY_RVV
    // Only m1..8 for now, can ignore kMaxLanes because HWY_*_LANES are full.
    ForeachSizeR<T, 8 / kDivLanes, HWY_LANES(T), Test>::Do();
#else
    ForeachSizeR<T, HWY_MIN(kMaxLanes, HWY_LANES(T)) / kDivLanes / kMinLanes,
                 kMinLanes, Test>::Do();
#endif
  }
};

// Calls Test for all vectors that can be demoted log2(kFactor) times.
template <class Test, size_t kFactor>
struct ForDemoteVectors {
  template <typename T>
  void operator()(T /*unused*/) const {
#if HWY_TARGET == HWY_RVV
    // Only m1..8 for now.
    ForeachSizeR<T, 8 / kFactor, kFactor * HWY_LANES(T), Test>::Do();
#else
    ForeachSizeR<T, HWY_LANES(T), 1, Test>::Do();
#endif
  }
};

// Calls Test for all powers of two in [128 bits, max bits].
template <class Test>
struct ForGE128Vectors {
  template <typename T>
  void operator()(T /*unused*/) const {
#if HWY_TARGET == HWY_RVV
    ForeachSizeR<T, 8, HWY_LANES(T), Test>::Do();
#else
    ForeachSizeR<T, HWY_LANES(T) / (16 / sizeof(T)), (16 / sizeof(T)),
                 Test>::Do();

#endif
  }
};

// Calls Test for all powers of two in [128 bits, max bits/2].
template <class Test>
struct ForExtendableVectors {
  template <typename T>
  void operator()(T /*unused*/) const {
#if HWY_TARGET == HWY_RVV
    ForeachSizeR<T, 4, HWY_LANES(T), Test>::Do();
#else
    ForeachSizeR<T, HWY_LANES(T) / 2 / (16 / sizeof(T)), (16 / sizeof(T)),
                 Test>::Do();
#endif
  }
};

// Calls Test for full vectors only.
template <class Test>
struct ForFullVectors {
  template <typename T>
  void operator()(T t) const {
#if HWY_TARGET == HWY_RVV
    ForeachSizeR<T, 8, HWY_LANES(T), Test>::Do();
    (void)t;
#else
    Test()(t, HWY_FULL(T)());
#endif
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

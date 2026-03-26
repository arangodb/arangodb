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

// Tests some ops specific to floating-point types (Div, Round etc.)

#include <stdio.h>

#include <algorithm>  // std::copy, std::fill
#include <cmath>      // std::abs, std::isnan, std::isinf, std::ceil, std::floor
#include <limits>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "tests/float_test.cc"
#include "hwy/foreach_target.h"  // IWYU pragma: keep
#include "hwy/highway.h"
#include "hwy/tests/test_util-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

struct TestDiv {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v = Iota(d, T(-2));
    const auto v1 = Set(d, T(1));

    // Unchanged after division by 1.
    HWY_ASSERT_VEC_EQ(d, v, Div(v, v1));

    const size_t N = Lanes(d);
    auto expected = AllocateAligned<T>(N);
    HWY_ASSERT(expected);
    for (size_t i = 0; i < N; ++i) {
      expected[i] = (T(i) - 2) / T(2);
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), Div(v, Set(d, T(2))));
  }
};

HWY_NOINLINE void TestAllDiv() { ForFloatTypes(ForPartialVectors<TestDiv>()); }

struct TestApproximateReciprocal {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v = Iota(d, T(-2));
    const auto nonzero = IfThenElse(Eq(v, Zero(d)), Set(d, T(1)), v);
    const size_t N = Lanes(d);
    auto input = AllocateAligned<T>(N);
    auto actual = AllocateAligned<T>(N);
    HWY_ASSERT(input && actual);

    Store(nonzero, d, input.get());
    Store(ApproximateReciprocal(nonzero), d, actual.get());

    double max_l1 = 0.0;
    double worst_expected = 0.0;
    double worst_actual = 0.0;
    for (size_t i = 0; i < N; ++i) {
      const double expected = 1.0 / input[i];
      const double l1 = std::abs(expected - actual[i]);
      if (l1 > max_l1) {
        max_l1 = l1;
        worst_expected = expected;
        worst_actual = actual[i];
      }
    }
    const double abs_worst_expected = std::abs(worst_expected);
    if (abs_worst_expected > 1E-5) {
      const double max_rel = max_l1 / abs_worst_expected;
      fprintf(stderr, "max l1 %f rel %f (%f vs %f)\n", max_l1, max_rel,
              worst_expected, worst_actual);
      HWY_ASSERT(max_rel < 0.004);
    }
  }
};

HWY_NOINLINE void TestAllApproximateReciprocal() {
  ForPartialVectors<TestApproximateReciprocal>()(float());
}

struct TestSquareRoot {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto vi = Iota(d, 0);
    HWY_ASSERT_VEC_EQ(d, vi, Sqrt(Mul(vi, vi)));
  }
};

HWY_NOINLINE void TestAllSquareRoot() {
  ForFloatTypes(ForPartialVectors<TestSquareRoot>());
}

struct TestReciprocalSquareRoot {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v = Set(d, 123.0f);
    const size_t N = Lanes(d);
    auto lanes = AllocateAligned<T>(N);
    HWY_ASSERT(lanes);
    Store(ApproximateReciprocalSqrt(v), d, lanes.get());
    for (size_t i = 0; i < N; ++i) {
      float err = lanes[i] - 0.090166f;
      if (err < 0.0f) err = -err;
      if (err >= 4E-4f) {
        HWY_ABORT("Lane %d (%d): actual %f err %f\n", static_cast<int>(i),
                  static_cast<int>(N), lanes[i], err);
      }
    }
  }
};

HWY_NOINLINE void TestAllReciprocalSquareRoot() {
  ForPartialVectors<TestReciprocalSquareRoot>()(float());
}

template <typename T, class D>
AlignedFreeUniquePtr<T[]> RoundTestCases(T /*unused*/, D d, size_t& padded) {
  const T eps = std::numeric_limits<T>::epsilon();
  const T test_cases[] = {
    // +/- 1
    T(1),
    T(-1),
    // +/- 0
    T(0),
    T(-0),
    // near 0
    T(0.4),
    T(-0.4),
    // +/- integer
    T(4),
    T(-32),
    // positive near limit
    MantissaEnd<T>() - T(1.5),
    MantissaEnd<T>() + T(1.5),
    // negative near limit
    -MantissaEnd<T>() - T(1.5),
    -MantissaEnd<T>() + T(1.5),
    // positive tiebreak
    T(1.5),
    T(2.5),
    // negative tiebreak
    T(-1.5),
    T(-2.5),
    // positive +/- delta
    T(2.0001),
    T(3.9999),
    // negative +/- delta
    T(-999.9999),
    T(-998.0001),
    // positive +/- epsilon
    T(1) + eps,
    T(1) - eps,
    // negative +/- epsilon
    T(-1) + eps,
    T(-1) - eps,
    // +/- huge (but still fits in float)
    T(1E34),
    T(-1E35),
    // +/- infinity
    std::numeric_limits<T>::infinity(),
    -std::numeric_limits<T>::infinity(),
    // qNaN
    GetLane(NaN(d))
  };
  const size_t kNumTestCases = sizeof(test_cases) / sizeof(test_cases[0]);
  const size_t N = Lanes(d);
  padded = RoundUpTo(kNumTestCases, N);  // allow loading whole vectors
  auto in = AllocateAligned<T>(padded);
  auto expected = AllocateAligned<T>(padded);
  HWY_ASSERT(in && expected);
  std::copy(test_cases, test_cases + kNumTestCases, in.get());
  std::fill(in.get() + kNumTestCases, in.get() + padded, T(0));
  return in;
}

struct TestRound {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T t, D d) {
    size_t padded;
    auto in = RoundTestCases(t, d, padded);
    auto expected = AllocateAligned<T>(padded);
    HWY_ASSERT(expected);

    for (size_t i = 0; i < padded; ++i) {
      // Avoid [std::]round, which does not round to nearest *even*.
      // NOTE: std:: version from C++11 cmath is not defined in RVV GCC, see
      // https://lists.freebsd.org/pipermail/freebsd-current/2014-January/048130.html
      expected[i] = static_cast<T>(nearbyint(in[i]));
    }
    for (size_t i = 0; i < padded; i += Lanes(d)) {
      HWY_ASSERT_VEC_EQ(d, &expected[i], Round(Load(d, &in[i])));
    }
  }
};

HWY_NOINLINE void TestAllRound() {
  ForFloatTypes(ForPartialVectors<TestRound>());
}

struct TestNearestInt {
  template <typename TF, class DF>
  HWY_NOINLINE void operator()(TF tf, const DF df) {
    using TI = MakeSigned<TF>;
    const RebindToSigned<DF> di;

    size_t padded;
    auto in = RoundTestCases(tf, df, padded);
    auto expected = AllocateAligned<TI>(padded);
    HWY_ASSERT(expected);

    constexpr double max = static_cast<double>(LimitsMax<TI>());
    for (size_t i = 0; i < padded; ++i) {
      if (std::isnan(in[i])) {
        // We replace NaN with 0 below (no_nan)
        expected[i] = 0;
      } else if (std::isinf(in[i]) || double{std::abs(in[i])} >= max) {
        // Avoid undefined result for lrintf
        expected[i] = std::signbit(in[i]) ? LimitsMin<TI>() : LimitsMax<TI>();
      } else {
        expected[i] = static_cast<TI>(lrintf(in[i]));
      }
    }
    for (size_t i = 0; i < padded; i += Lanes(df)) {
      const auto v = Load(df, &in[i]);
      const auto no_nan = IfThenElse(Eq(v, v), v, Zero(df));
      HWY_ASSERT_VEC_EQ(di, &expected[i], NearestInt(no_nan));
    }
  }
};

HWY_NOINLINE void TestAllNearestInt() {
  ForPartialVectors<TestNearestInt>()(float());
}

struct TestTrunc {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T t, D d) {
    size_t padded;
    auto in = RoundTestCases(t, d, padded);
    auto expected = AllocateAligned<T>(padded);
    HWY_ASSERT(expected);

    for (size_t i = 0; i < padded; ++i) {
      // NOTE: std:: version from C++11 cmath is not defined in RVV GCC, see
      // https://lists.freebsd.org/pipermail/freebsd-current/2014-January/048130.html
      expected[i] = static_cast<T>(trunc(in[i]));
    }
    for (size_t i = 0; i < padded; i += Lanes(d)) {
      HWY_ASSERT_VEC_EQ(d, &expected[i], Trunc(Load(d, &in[i])));
    }
  }
};

HWY_NOINLINE void TestAllTrunc() {
  ForFloatTypes(ForPartialVectors<TestTrunc>());
}

struct TestCeil {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T t, D d) {
    size_t padded;
    auto in = RoundTestCases(t, d, padded);
    auto expected = AllocateAligned<T>(padded);
    HWY_ASSERT(expected);

    for (size_t i = 0; i < padded; ++i) {
      expected[i] = std::ceil(in[i]);
    }
    for (size_t i = 0; i < padded; i += Lanes(d)) {
      HWY_ASSERT_VEC_EQ(d, &expected[i], Ceil(Load(d, &in[i])));
    }
  }
};

HWY_NOINLINE void TestAllCeil() {
  ForFloatTypes(ForPartialVectors<TestCeil>());
}

struct TestFloor {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T t, D d) {
    size_t padded;
    auto in = RoundTestCases(t, d, padded);
    auto expected = AllocateAligned<T>(padded);
    HWY_ASSERT(expected);

    for (size_t i = 0; i < padded; ++i) {
      expected[i] = std::floor(in[i]);
    }
    for (size_t i = 0; i < padded; i += Lanes(d)) {
      HWY_ASSERT_VEC_EQ(d, &expected[i], Floor(Load(d, &in[i])));
    }
  }
};

HWY_NOINLINE void TestAllFloor() {
  ForFloatTypes(ForPartialVectors<TestFloor>());
}

struct TestAbsDiff {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);
    auto in_lanes_a = AllocateAligned<T>(N);
    auto in_lanes_b = AllocateAligned<T>(N);
    auto out_lanes = AllocateAligned<T>(N);
    HWY_ASSERT(in_lanes_a && in_lanes_b && out_lanes);
    for (size_t i = 0; i < N; ++i) {
      in_lanes_a[i] = static_cast<T>((i ^ 1u) << i);
      in_lanes_b[i] = static_cast<T>(i << i);
      out_lanes[i] = std::abs(in_lanes_a[i] - in_lanes_b[i]);
    }
    const auto a = Load(d, in_lanes_a.get());
    const auto b = Load(d, in_lanes_b.get());
    const auto expected = Load(d, out_lanes.get());
    HWY_ASSERT_VEC_EQ(d, expected, AbsDiff(a, b));
    HWY_ASSERT_VEC_EQ(d, expected, AbsDiff(b, a));
  }
};

HWY_NOINLINE void TestAllAbsDiff() {
  ForPartialVectors<TestAbsDiff>()(float());
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace hwy {
HWY_BEFORE_TEST(HwyFloatTest);
HWY_EXPORT_AND_TEST_P(HwyFloatTest, TestAllDiv);
HWY_EXPORT_AND_TEST_P(HwyFloatTest, TestAllApproximateReciprocal);
HWY_EXPORT_AND_TEST_P(HwyFloatTest, TestAllSquareRoot);
HWY_EXPORT_AND_TEST_P(HwyFloatTest, TestAllReciprocalSquareRoot);
HWY_EXPORT_AND_TEST_P(HwyFloatTest, TestAllRound);
HWY_EXPORT_AND_TEST_P(HwyFloatTest, TestAllNearestInt);
HWY_EXPORT_AND_TEST_P(HwyFloatTest, TestAllTrunc);
HWY_EXPORT_AND_TEST_P(HwyFloatTest, TestAllCeil);
HWY_EXPORT_AND_TEST_P(HwyFloatTest, TestAllFloor);
HWY_EXPORT_AND_TEST_P(HwyFloatTest, TestAllAbsDiff);
}  // namespace hwy

#endif

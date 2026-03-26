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

#include <stddef.h>
#include <stdint.h>

#include <string>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "tests/test_util_test.cc"
#include "hwy/foreach_target.h"  // IWYU pragma: keep
#include "hwy/highway.h"
#include "hwy/tests/test_util-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

struct TestName {
  template <class T, class D>
  HWY_NOINLINE void operator()(T t, D d) {
    char num[10];
    std::string expected = IsFloat<T>() ? "f" : (IsSigned<T>() ? "i" : "u");
    snprintf(num, sizeof(num), "%u" , static_cast<unsigned>(sizeof(T) * 8));
    expected += num;

    const size_t N = Lanes(d);
    if (N != 1) {
      expected += 'x';
      snprintf(num, sizeof(num), "%u", static_cast<unsigned>(N));
      expected += num;
    }
    const std::string actual = TypeName(t, N);
    if (expected != actual) {
      HWY_ABORT("%s mismatch: expected '%s', got '%s'.\n",
                hwy::TargetName(HWY_TARGET), expected.c_str(), actual.c_str());
    }
  }
};

HWY_NOINLINE void TestAllName() { ForAllTypes(ForPartialVectors<TestName>()); }

struct TestEqualInteger {
  template <class T>
  HWY_NOINLINE void operator()(T /*t*/) const {
    HWY_ASSERT_EQ(T(0), T(0));
    HWY_ASSERT_EQ(T(1), T(1));
    HWY_ASSERT_EQ(T(-1), T(-1));
    HWY_ASSERT_EQ(LimitsMin<T>(), LimitsMin<T>());

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

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace hwy {
HWY_BEFORE_TEST(TestUtilTest);
HWY_EXPORT_AND_TEST_P(TestUtilTest, TestAllName);
HWY_EXPORT_AND_TEST_P(TestUtilTest, TestAllEqual);
}  // namespace hwy

#endif

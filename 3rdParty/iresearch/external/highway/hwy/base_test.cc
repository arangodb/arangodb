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
#define HWY_TARGET_INCLUDE "base_test.cc"
#include "hwy/foreach_target.h"
#include "hwy/highway.h"
#include "hwy/tests/test_util-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

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

struct TestIsSame {
  template <class T>
  HWY_NOINLINE void operator()(T /*unused*/) const {
    static_assert(IsSame<T, T>(), "T == T");
    static_assert(!IsSame<MakeSigned<T>, MakeUnsigned<T>>(), "S != U");
    static_assert(!IsSame<MakeUnsigned<T>, MakeSigned<T>>(), "U != S");
  }
};

HWY_NOINLINE void TestAllIsSame() { ForAllTypes(TestIsSame()); }

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

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace hwy {
HWY_BEFORE_TEST(BaseTest);
HWY_EXPORT_AND_TEST_P(BaseTest, TestAllLimits);
HWY_EXPORT_AND_TEST_P(BaseTest, TestAllLowestHighest);
HWY_EXPORT_AND_TEST_P(BaseTest, TestAllType);
HWY_EXPORT_AND_TEST_P(BaseTest, TestAllIsSame);
HWY_EXPORT_AND_TEST_P(BaseTest, TestAllPopCount);
}  // namespace hwy

// Ought not to be necessary, but without this, no tests run on RVV.
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

#endif

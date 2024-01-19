////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Basics/SizeLimitedString.h"

#include <cstddef>
#include <string>
#include <string_view>

using namespace arangodb;

TEST(SizeLimitedStringTest, test_empty) {
  SizeLimitedString<100> testee;

  EXPECT_TRUE(testee.empty());

  testee.push_back('a');
  EXPECT_FALSE(testee.empty());

  testee.clear();
  EXPECT_TRUE(testee.empty());

  for (size_t i = 0; i < 100; ++i) {
    testee.push_back('x');
    EXPECT_FALSE(testee.empty());
  }

  testee.push_back('y');
  EXPECT_FALSE(testee.empty());

  testee.clear();
  EXPECT_TRUE(testee.empty());
}

TEST(SizeLimitedStringTest, test_size) {
  SizeLimitedString<100> testee;

  EXPECT_EQ(0, testee.size());

  testee.push_back('a');
  EXPECT_EQ(1, testee.size());

  testee.clear();
  EXPECT_EQ(0, testee.size());

  for (size_t i = 0; i < 100; ++i) {
    EXPECT_EQ(i, testee.size());
    testee.push_back('x');
    EXPECT_EQ(i + 1, testee.size());
  }

  testee.push_back('y');
  EXPECT_EQ(100, testee.size());

  testee.clear();
  EXPECT_EQ(0, testee.size());
}

TEST(SizeLimitedStringTest, test_capacity) {
  {
    SizeLimitedString<100> testee;
    EXPECT_EQ(0, testee.size());
    EXPECT_EQ(100, testee.capacity());
  }

  {
    SizeLimitedString<1000> testee;
    EXPECT_EQ(0, testee.size());
    EXPECT_EQ(1000, testee.capacity());
  }
}

TEST(SizeLimitedStringTest, test_view) {
  SizeLimitedString<100> testee;

  EXPECT_EQ(std::string_view(), testee.view());

  testee.push_back('a');
  EXPECT_EQ(std::string_view("a"), testee.view());

  testee.clear();
  EXPECT_EQ(std::string_view(), testee.view());

  std::string cmp;
  for (size_t i = 0; i < 100; ++i) {
    testee.push_back('x');
    cmp.push_back('x');

    EXPECT_EQ(cmp, testee.view());
    EXPECT_EQ(cmp.size(), testee.size());
  }

  testee.push_back('y');
  EXPECT_EQ(cmp, testee.view());

  testee.clear();
  EXPECT_EQ(std::string_view(), testee.view());
}

TEST(SizeLimitedStringTest, test_append) {
  SizeLimitedString<100> testee;

  constexpr std::string_view value("the fox");
  testee.append(value);
  EXPECT_EQ(value, testee.view());
  EXPECT_EQ(value.size(), testee.size());

  testee.clear();
  EXPECT_EQ(std::string_view(), testee.view());
  EXPECT_EQ(0, testee.size());
  EXPECT_TRUE(testee.empty());

  std::string cmp;
  for (size_t i = 0; i < 100; ++i) {
    testee.append(value);
    cmp.append(value);

    if (cmp.size() > 100) {
      cmp = cmp.substr(0, 100);
    }
    EXPECT_EQ(cmp, testee.view());
  }

  testee.clear();
  EXPECT_EQ(std::string_view(), testee.view());
}

TEST(SizeLimitedStringTest, test_too_long_string) {
  SizeLimitedString<10> testee;

  constexpr std::string_view value("the quick brown fox jumped");
  testee.append(value);
  EXPECT_EQ(value.substr(0, 10), testee.view());
  EXPECT_EQ(10, testee.size());
}

TEST(SizeLimitedStringTest, test_append_uint64) {
  {
    uint64_t value = 0;
    // buffer to short for a large uint64_t value
    SizeLimitedString<10> testee;
    testee.appendUInt64(value);
    EXPECT_EQ(std::string_view(""), testee.view());
  }

  {
    uint64_t value = 0;
    // buffer still to short for a large uint64_t value
    SizeLimitedString<20> testee;
    testee.appendUInt64(value);
    EXPECT_EQ(std::string_view(""), testee.view());
  }

  {
    uint64_t value = 0;
    SizeLimitedString<21> testee;
    testee.appendUInt64(value);
    EXPECT_EQ(std::string_view("0"), testee.view());
  }

  {
    uint64_t value = 42;
    SizeLimitedString<21> testee;
    testee.appendUInt64(value);
    EXPECT_EQ(std::string_view("42"), testee.view());
  }

  {
    uint64_t value = 12345;
    SizeLimitedString<21> testee;
    testee.appendUInt64(value);
    EXPECT_EQ(std::string_view("12345"), testee.view());
  }

  {
    uint64_t value = 123456789;
    SizeLimitedString<21> testee;
    testee.appendUInt64(value);
    EXPECT_EQ(std::string_view("123456789"), testee.view());
  }

  {
    uint64_t value = 12345678901;
    SizeLimitedString<21> testee;
    testee.appendUInt64(value);
    EXPECT_EQ(std::string_view("12345678901"), testee.view());
  }

  {
    uint64_t value = 18446744073709551615ULL;
    SizeLimitedString<21> testee;
    testee.appendUInt64(value);
    EXPECT_EQ(std::string_view("18446744073709551615"), testee.view());
  }
}

TEST(SizeLimitedStringTest, test_append_hex_value_le) {
  {
    uint32_t value = 0;
    SizeLimitedString<10> testee;
    testee.appendHexValue(value, false);
    EXPECT_EQ(std::string_view("00000000"), testee.view());
  }

  {
    uint32_t value = 0xdeadbeef;
    SizeLimitedString<10> testee;
    testee.appendHexValue(value, false);
    EXPECT_EQ(std::string_view("deadbeef"), testee.view());
  }

  {
    char const* value = nullptr;
    SizeLimitedString<16> testee;
    testee.appendHexValue(value, false);
    EXPECT_EQ(std::string_view("0000000000000000"), testee.view());
  }

  {
    char const* value = nullptr;
    SizeLimitedString<16> testee;
    testee.appendHexValue(value, true);
    EXPECT_EQ(std::string_view("0"), testee.view());
  }

  {
    uintptr_t value = 0xabcdef01234;
    SizeLimitedString<16> testee;
    testee.appendHexValue(value, false);
    EXPECT_EQ(std::string_view("00000abcdef01234"), testee.view());
  }

  {
    uintptr_t value = 0xabcdef01234;
    SizeLimitedString<16> testee;
    testee.appendHexValue(value, true);
    EXPECT_EQ(std::string_view("abcdef01234"), testee.view());
  }

  {
    uint64_t value = 0x0fffffffffffffffULL;
    SizeLimitedString<16> testee;
    testee.appendHexValue(value, false);
    EXPECT_EQ(std::string_view("0fffffffffffffff"), testee.view());
  }

  {
    uint64_t value = 0x0fffffffffffffffULL;
    SizeLimitedString<16> testee;
    testee.appendHexValue(value, true);
    EXPECT_EQ(std::string_view("fffffffffffffff"), testee.view());
  }

  {
    uint64_t value = 0xffffffffffffffffULL;
    SizeLimitedString<16> testee;
    testee.appendHexValue(value, false);
    EXPECT_EQ(std::string_view("ffffffffffffffff"), testee.view());
  }

  {
    uint64_t value = 0xffffffffffffffffULL;
    SizeLimitedString<16> testee;
    testee.appendHexValue(value, true);
    EXPECT_EQ(std::string_view("ffffffffffffffff"), testee.view());
  }
}

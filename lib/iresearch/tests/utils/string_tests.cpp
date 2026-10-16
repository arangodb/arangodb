////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include <climits>

#include "tests_shared.hpp"
#include "utils/string.hpp"

void expect_sign_eq(long double lhs, long double rhs) {
  EXPECT_TRUE((lhs == 0 && rhs == 0) || std::signbit(lhs) == std::signbit(rhs));
}

TEST(string_tests, common_prefix) {
  using namespace irs;

  {
    const std::string_view lhs = "20-MAR-2012 19:56:11.00";
    const std::string_view rhs = "20-MAR-2012 19:56:11.00\0\0";
    EXPECT_EQ(23, CommonPrefixLength(lhs, rhs));
    EXPECT_EQ(23, CommonPrefixLength(rhs, lhs));
  }

  {
    const std::string_view lhs = "quick brown fox";
    const std::string_view rhs = "quick brown fax";
    EXPECT_EQ(13, CommonPrefixLength(lhs, rhs));
    EXPECT_EQ(13, CommonPrefixLength(rhs, lhs));
  }

  {
    const std::string_view lhs = "quick brown foxies";
    const std::string_view rhs = "quick brown fax";
    EXPECT_EQ(13, CommonPrefixLength(lhs, rhs));
    EXPECT_EQ(13, CommonPrefixLength(rhs, lhs));
  }

  {
    const std::string_view lhs = "quick brown foxies";
    const std::string_view rhs = "fuick brown fax";
    EXPECT_EQ(0, CommonPrefixLength(lhs, rhs));
    EXPECT_EQ(0, CommonPrefixLength(rhs, lhs));
  }

  {
    const std::string_view lhs = "quick brown foxies";
    const std::string_view rhs = "q1ick brown fax";
    EXPECT_EQ(1, CommonPrefixLength(lhs, rhs));
    EXPECT_EQ(1, CommonPrefixLength(rhs, lhs));
  }

  {
    const std::string_view lhs = "qui";
    const std::string_view rhs = "q1";
    EXPECT_EQ(1, CommonPrefixLength(lhs, rhs));
    EXPECT_EQ(1, CommonPrefixLength(rhs, lhs));
  }

  {
    const std::string_view lhs = "qui";
    const std::string_view rhs = "f1";
    EXPECT_EQ(0, CommonPrefixLength(lhs, rhs));
    EXPECT_EQ(0, CommonPrefixLength(rhs, lhs));
  }

  {
    const std::string_view lhs = "quick brown foxies";
    const std::string_view rhs = "qui";
    EXPECT_EQ(3, CommonPrefixLength(lhs, rhs));
    EXPECT_EQ(3, CommonPrefixLength(rhs, lhs));
  }

  {
    const std::string_view str = "quick brown foxies";
    EXPECT_EQ(0, CommonPrefixLength(std::string_view{}, str));
    EXPECT_EQ(0, CommonPrefixLength(str, std::string_view{}));
  }

  {
    const std::string_view str = "quick brown foxies";
    EXPECT_EQ(0, CommonPrefixLength(std::string_view{""}, str));
    EXPECT_EQ(0, CommonPrefixLength(str, std::string_view{""}));
  }
}

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

#include "tests_shared.hpp"

#include "utils/string.hpp"

#include <climits>

void expect_sign_eq(long double lhs, long double rhs) {
  EXPECT_TRUE((lhs == 0 && rhs == 0) || std::signbit(lhs) == std::signbit(rhs));
}

TEST(string_ref_tests, static_members) {
  EXPECT_EQ(nullptr, irs::string_ref::NIL.c_str());
  EXPECT_EQ(0, irs::string_ref::NIL.size());
  EXPECT_TRUE(irs::string_ref::NIL.null());
  EXPECT_TRUE(irs::string_ref::NIL.empty());
}

TEST(string_ref_tests, create) {
  using namespace iresearch;

  // create default reference
  {
    const string_ref ref;
    EXPECT_EQ(nullptr, ref.c_str());
    EXPECT_EQ(0, ref.size());
    EXPECT_TRUE(ref.null());
    EXPECT_TRUE(ref.empty());
    EXPECT_EQ(irs::string_ref::NIL, ref);
    EXPECT_EQ(ref.begin(), ref.end());
    EXPECT_EQ(ref.rbegin(), ref.rend());
  }

  // create empty reference
  {
    const char* empty = "";
    const string_ref ref(empty);
    EXPECT_EQ(empty, ref.c_str());
    EXPECT_EQ(0, ref.size());
    EXPECT_FALSE(ref.null());
    EXPECT_TRUE(ref.empty());
    EXPECT_EQ(ref.begin(), ref.end());
    EXPECT_EQ(ref.rbegin(), ref.rend());
  }

  // create reference to string
  {
    const std::string s = "quck brown fox";
    {
      const string_ref ref(s.c_str());
      EXPECT_EQ(s.c_str(), ref.c_str());
      EXPECT_EQ(s.size(), ref.size());
      EXPECT_FALSE(ref.null());
      EXPECT_FALSE(ref.empty());
      EXPECT_NE(ref.begin(), ref.end());
      EXPECT_NE(ref.rbegin(), ref.rend());

      auto rbegin = ref.rbegin();
      for (size_t i = 0; i < ref.size(); ++i) {
        EXPECT_EQ(&ref[ref.size() - i - 1], &*rbegin);
        ++rbegin;
      }
      EXPECT_EQ(rbegin, ref.rend());
    }

    {
      const string_ref ref(s.c_str(), s.size());
      EXPECT_EQ(s.c_str(), ref.c_str());
      EXPECT_EQ(s.size(), ref.size());
      EXPECT_FALSE(ref.null());
      EXPECT_FALSE(ref.empty());
    }
  }

  // create reference from existing reference
  {
    const std::string s = "quck brown fox";
    const string_ref ref0(s.c_str(), s.size());
    const string_ref ref1(ref0);

    EXPECT_EQ(s.c_str(), ref0.c_str());
    EXPECT_EQ(s.size(), ref0.size());
    EXPECT_EQ(s.c_str(), ref1.c_str());
    EXPECT_EQ(s.size(), ref1.size());
    EXPECT_EQ(ref0, ref1);

    EXPECT_TRUE(ref0 == ref1);
    EXPECT_FALSE(ref0 != ref1);
    EXPECT_FALSE(ref0 > ref1);
    EXPECT_FALSE(ref0 < ref1);
    EXPECT_TRUE(ref0 >= ref1);
    EXPECT_TRUE(ref0 <= ref1);
    EXPECT_TRUE(0 == compare(ref0, ref1));
  }
}

TEST(string_ref_tests, assign) {
  using namespace iresearch;

  {
    string_ref ref1;

    const std::string s = "quck brown fox";
    const string_ref ref0(s.c_str(), s.size());
    ref1 = ref0;

    EXPECT_TRUE(ref0 == ref1);
    EXPECT_FALSE(ref0 != ref1);
    EXPECT_FALSE(ref0 > ref1);
    EXPECT_FALSE(ref0 < ref1);
    EXPECT_TRUE(ref0 >= ref1);
    EXPECT_TRUE(ref0 <= ref1);
    EXPECT_TRUE(0 == compare(ref0, ref1));
  }
}

TEST(string_ref_tests, compare) {
  using namespace iresearch;

  // empty    
  {
    const std::string s0 = "";
    const string_ref ref0 = "";

    const std::string s1 = "";
    const string_ref ref1 = "";

    EXPECT_EQ(ref0 == ref1, s0 == s1);
    EXPECT_EQ(ref0 != ref1, s0 != s1);
    EXPECT_EQ(ref0 > ref1, s0 > s1);
    EXPECT_EQ(ref0 < ref1, s0 < s1);
    EXPECT_EQ(ref0 >= ref1, s0 >= s1);
    EXPECT_EQ(ref0 <= ref1, s0 <= s1);
    EXPECT_EQ(compare(ref0, ref1), s0.compare(s1));
    EXPECT_TRUE(0 == compare(ref0, s0.c_str()));
    EXPECT_TRUE(0 == compare(ref1, s1.c_str()));
  }

  // same length 
  {
    const std::string s0 = "quck brown fox";
    const string_ref ref0(s0.c_str(), s0.size());

    const std::string s1 = "over the lazy ";
    const string_ref ref1(s1.c_str(), s1.size());

    EXPECT_EQ(ref0 == ref1, s0 == s1);
    EXPECT_EQ(ref0 != ref1, s0 != s1);
    EXPECT_EQ(ref0 > ref1, s0 > s1);
    EXPECT_EQ(ref0 < ref1, s0 < s1);
    EXPECT_EQ(ref0 >= ref1, s0 >= s1);
    EXPECT_EQ(ref0 <= ref1, s0 <= s1);
    expect_sign_eq(compare(ref0, ref1), s0.compare(s1));
    EXPECT_TRUE(0 == compare(ref0, s0.c_str()));
    EXPECT_TRUE(0 == compare(ref1, s1.c_str()));
  }

  // different length
  {
    const std::string s0 = "quck brown fox";
    const string_ref ref0(s0.c_str(), s0.size());

    const std::string s1 = "over the lazy dog";
    const string_ref ref1(s1.c_str(), s1.size());

    EXPECT_EQ(ref0 == ref1, s0 == s1);
    EXPECT_EQ(ref0 != ref1, s0 != s1);
    EXPECT_EQ(ref0 > ref1, s0 > s1);
    EXPECT_EQ(ref0 < ref1, s0 < s1);
    EXPECT_EQ(ref0 >= ref1, s0 >= s1);
    EXPECT_EQ(ref0 <= ref1, s0 <= s1);
    expect_sign_eq(compare(ref0, ref1), s0.compare(s1));
    EXPECT_TRUE(0 == compare(ref0, s0.c_str()));
    EXPECT_TRUE(0 == compare(ref1, s1.c_str()));
  }
}

TEST(string_ref_tests, common_prefix) {
  using namespace iresearch;

  {
    const string_ref lhs = "20-MAR-2012 19:56:11.00";
    const string_ref rhs = "20-MAR-2012 19:56:11.00\0\0";
    EXPECT_EQ(23, common_prefix_length(lhs, rhs));
    EXPECT_EQ(23, common_prefix_length(rhs, lhs));
  }

  {
    const string_ref lhs = "quick brown fox";
    const string_ref rhs = "quick brown fax";
    EXPECT_EQ(13, common_prefix_length(lhs, rhs));
    EXPECT_EQ(13, common_prefix_length(rhs, lhs));
  }

  {
    const string_ref lhs = "quick brown foxies";
    const string_ref rhs = "quick brown fax";
    EXPECT_EQ(13, common_prefix_length(lhs, rhs));
    EXPECT_EQ(13, common_prefix_length(rhs, lhs));
  }

  {
    const string_ref lhs = "quick brown foxies";
    const string_ref rhs = "fuick brown fax";
    EXPECT_EQ(0, common_prefix_length(lhs, rhs));
    EXPECT_EQ(0, common_prefix_length(rhs, lhs));
  }

  {
    const string_ref lhs = "quick brown foxies";
    const string_ref rhs = "q1ick brown fax";
    EXPECT_EQ(1, common_prefix_length(lhs, rhs));
    EXPECT_EQ(1, common_prefix_length(rhs, lhs));
  }

  {
    const string_ref lhs = "qui";
    const string_ref rhs = "q1";
    EXPECT_EQ(1, common_prefix_length(lhs, rhs));
    EXPECT_EQ(1, common_prefix_length(rhs, lhs));
  }

  {
    const string_ref lhs = "qui";
    const string_ref rhs = "f1";
    EXPECT_EQ(0, common_prefix_length(lhs, rhs));
    EXPECT_EQ(0, common_prefix_length(rhs, lhs));
  }

  {
    const string_ref lhs = "quick brown foxies";
    const string_ref rhs = "qui";
    EXPECT_EQ(3, common_prefix_length(lhs, rhs));
    EXPECT_EQ(3, common_prefix_length(rhs, lhs));
  }

  {
    const string_ref str = "quick brown foxies";
    EXPECT_EQ(0, common_prefix_length(string_ref::NIL, str));
    EXPECT_EQ(0, common_prefix_length(str, string_ref::NIL));
  }

  {
    const string_ref str = "quick brown foxies";
    EXPECT_EQ(0, common_prefix_length(string_ref::EMPTY, str));
    EXPECT_EQ(0, common_prefix_length(str, string_ref::EMPTY));
  }
}

TEST(string_ref_tests, starts_with) {
  using namespace iresearch;

  {
    std::string s = "quick brown fox";
    string_ref ref(s.c_str(), s.size());
    const string_ref prefix("_____prefix_____");

    EXPECT_FALSE(starts_with(ref, prefix));
    s.insert(0, prefix.c_str(), prefix.size());
    ref = s.c_str(); // refresh ref
    EXPECT_TRUE(starts_with(ref, prefix));
  }
}

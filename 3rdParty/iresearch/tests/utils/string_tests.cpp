//
// IResearch search engine 
// 
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

#include "tests_shared.hpp"

#include "utils/string.hpp"

#include <climits>

void expect_sign_eq(long double lhs, long double rhs) {
  EXPECT_TRUE((lhs == 0 && rhs == 0) || std::signbit(lhs) == std::signbit(rhs));
}

TEST(string_ref_tests, create) {
  using namespace iresearch;

  // create null reference
  {
    const string_ref ref;
    EXPECT_EQ(nullptr, ref.c_str());
    EXPECT_EQ(0, ref.size());
    EXPECT_TRUE(ref.null());
    EXPECT_TRUE(ref.empty());
  }

  // create empty reference
  {
    const char* empty = "";
    const string_ref ref(empty);
    EXPECT_EQ(empty, ref.c_str());
    EXPECT_EQ(0, ref.size());
    EXPECT_FALSE(ref.null());
    EXPECT_TRUE(ref.empty());
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
    const  string_ref ref1(s1.c_str(), s1.size());

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

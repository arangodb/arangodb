////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "tests_shared.hpp"
#include "utils/fstext/fst_string_weight.h"

// -----------------------------------------------------------------------------
// --SECTION--                                           fst_string_weight_tests
// -----------------------------------------------------------------------------

TEST(fst_string_weight_test, static_const) {
  ASSERT_EQ("left_string", fst::StringLeftWeight<char>::Type());
  ASSERT_EQ(fst::StringLeftWeight<char>(fst::kStringInfinity), fst::StringLeftWeight<char>::Zero());
  ASSERT_TRUE(fst::StringLeftWeight<char>::Zero().Member());
  ASSERT_EQ(fst::StringLeftWeight<char>(), fst::StringLeftWeight<char>::One());
  ASSERT_TRUE(fst::StringLeftWeight<char>::One().Member());
  ASSERT_EQ(fst::StringLeftWeight<char>(fst::kStringBad), fst::StringLeftWeight<char>::NoWeight());
  ASSERT_FALSE(fst::StringLeftWeight<char>::NoWeight().Member());
}

TEST(fst_string_weight_test, construct) {
  fst::StringLeftWeight<char> w;
  ASSERT_TRUE(w.Empty());
  ASSERT_EQ(0, w.Size());
}

TEST(fst_string_weight_test, read_write) {
  std::stringstream ss;

  fst::StringLeftWeight<char> w0;
  w0.PushBack(1);
  w0.PushBack(char(255));
  w0.PushBack(3);
  w0.PushBack(3);
  w0.PushBack(char(129));
  w0.Write(ss);
  ASSERT_EQ(5, w0.Size());
  ASSERT_TRUE(w0.Member());

  ss.seekg(0);
  fst::StringLeftWeight<char> w1;
  w1.Read(ss);
  ASSERT_EQ(w0, w1);
  ASSERT_EQ(w0.Hash(), w1.Hash());
}

TEST(fst_string_weight_test, plus) {
  // weight + weight
  {
    const std::string lhs = "123456";
    const std::string rhs = "124";
    const std::string prefix = "12";

    const fst::StringLeftWeight<char> lhs_weight(lhs.begin(), lhs.end());
    const fst::StringLeftWeight<char> rhs_weight(rhs.begin(), rhs.end());
    const fst::StringLeftWeight<char> prefix_weight(prefix.begin(), prefix.end());
    ASSERT_EQ(prefix_weight, fst::Plus(lhs_weight,rhs_weight));
  }

  // weight + weight
  {
    const std::string lhs = "124";
    const std::string rhs = "123456";
    const std::string prefix = "12";

    const fst::StringLeftWeight<char> lhs_weight(lhs.begin(), lhs.end());
    const fst::StringLeftWeight<char> rhs_weight(rhs.begin(), rhs.end());
    const fst::StringLeftWeight<char> prefix_weight(prefix.begin(), prefix.end());
    ASSERT_EQ(prefix_weight, fst::Plus(lhs_weight,rhs_weight));
  }

  // weight + Zero()
  {
    const std::string lhs = "123456";
    const std::string prefix = "";

    const fst::StringLeftWeight<char> lhs_weight(lhs.begin(), lhs.end());
    const fst::StringLeftWeight<char> prefix_weight(prefix.begin(), prefix.end());
    ASSERT_EQ(lhs_weight, fst::Plus(lhs_weight, fst::StringLeftWeight<char>::Zero()));
  }

  // Zero() + weight
  {
    const std::string rhs = "123456";
    const std::string prefix = "";

    const fst::StringLeftWeight<char> rhs_weight(rhs.begin(), rhs.end());
    const fst::StringLeftWeight<char> prefix_weight(prefix.begin(), prefix.end());
    ASSERT_EQ(rhs_weight, fst::Plus(fst::StringLeftWeight<char>::Zero(), rhs_weight));
  }

  // weight + NoWeight()
  {
    const std::string lhs = "123456";

    const fst::StringLeftWeight<char> lhs_weight(lhs.begin(), lhs.end());
    ASSERT_EQ(fst::StringLeftWeight<char>::NoWeight(), fst::Plus(lhs_weight, fst::StringLeftWeight<char>::NoWeight()));
  }

  // NoWeight() + weight
  {
    const std::string rhs = "123456";

    const fst::StringLeftWeight<char> rhs_weight(rhs.begin(), rhs.end());
    ASSERT_EQ(fst::StringLeftWeight<char>::NoWeight(), fst::Plus(fst::StringLeftWeight<char>::NoWeight(), rhs_weight));
  }

  // weight + One()
  {
    const std::string lhs = "123456";
    const std::string prefix = "";

    const fst::StringLeftWeight<char> lhs_weight(lhs.begin(), lhs.end());
    const fst::StringLeftWeight<char> prefix_weight(prefix.begin(), prefix.end());
    ASSERT_EQ(prefix_weight, fst::Plus(lhs_weight, fst::StringLeftWeight<char>::One()));
  }

  // One() + weight
  {
    const std::string rhs = "123456";
    const std::string prefix = "";

    const fst::StringLeftWeight<char> rhs_weight(rhs.begin(), rhs.end());
    const fst::StringLeftWeight<char> prefix_weight(prefix.begin(), prefix.end());
    ASSERT_EQ(prefix_weight, fst::Plus(fst::StringLeftWeight<char>::One(), rhs_weight));
  }
}

TEST(fst_string_weight_test, times) {
  // weight * weight
  {
    const std::string lhs = "123456";
    const std::string rhs = "124";
    const std::string result = "123456124";

    const fst::StringLeftWeight<char> lhs_weight(lhs.begin(), lhs.end());
    const fst::StringLeftWeight<char> rhs_weight(rhs.begin(), rhs.end());
    const fst::StringLeftWeight<char> result_weight(result.begin(), result.end());
    ASSERT_EQ(result_weight, fst::Times(lhs_weight,rhs_weight));
  }

  // weight * weight
  {
    const std::string lhs = "124";
    const std::string rhs = "123456";
    const std::string result = "124123456";

    const fst::StringLeftWeight<char> lhs_weight(lhs.begin(), lhs.end());
    const fst::StringLeftWeight<char> rhs_weight(rhs.begin(), rhs.end());
    const fst::StringLeftWeight<char> result_weight(result.begin(), result.end());
    ASSERT_EQ(result_weight, fst::Times(lhs_weight,rhs_weight));
  }

  // weight * Zero()
  {
    const std::string lhs = "123456";
    const std::string result = "";

    const fst::StringLeftWeight<char> lhs_weight(lhs.begin(), lhs.end());
    const fst::StringLeftWeight<char> result_weight(result.begin(), result.end());
    ASSERT_EQ(fst::StringLeftWeight<char>::Zero(), fst::Times(lhs_weight, fst::StringLeftWeight<char>::Zero()));
  }

  // Zero() * weight
  {
    const std::string rhs = "123456";
    const std::string result = "";

    const fst::StringLeftWeight<char> rhs_weight(rhs.begin(), rhs.end());
    const fst::StringLeftWeight<char> result_weight(result.begin(), result.end());
    ASSERT_EQ(fst::StringLeftWeight<char>::Zero(), fst::Times(fst::StringLeftWeight<char>::Zero(), rhs_weight));
  }

  // weight * NoWeight()
  {
    const std::string lhs = "123456";

    const fst::StringLeftWeight<char> lhs_weight(lhs.begin(), lhs.end());
    ASSERT_EQ(fst::StringLeftWeight<char>::NoWeight(), fst::Times(lhs_weight, fst::StringLeftWeight<char>::NoWeight()));
  }

  // NoWeight() * weight
  {
    const std::string rhs = "123456";

    const fst::StringLeftWeight<char> rhs_weight(rhs.begin(), rhs.end());
    ASSERT_EQ(fst::StringLeftWeight<char>::NoWeight(), fst::Times(fst::StringLeftWeight<char>::NoWeight(), rhs_weight));
  }

  // weight * One()
  {
    const std::string lhs = "123456";
    const std::string result = "123456";

    const fst::StringLeftWeight<char> lhs_weight(lhs.begin(), lhs.end());
    const fst::StringLeftWeight<char> result_weight(result.begin(), result.end());
    ASSERT_EQ(result_weight, fst::Times(lhs_weight, fst::StringLeftWeight<char>::One()));
  }

  // One() + weight
  {
    const std::string rhs = "123456";
    const std::string result = "123456";

    const fst::StringLeftWeight<char> rhs_weight(rhs.begin(), rhs.end());
    const fst::StringLeftWeight<char> result_weight(result.begin(), result.end());
    ASSERT_EQ(result_weight, fst::Times(fst::StringLeftWeight<char>::One(), rhs_weight));
  }
}

TEST(fst_string_weight_test, divide) {
  // weight / weight
  {
    const std::string lhs = "123456";
    const std::string rhs = "12";
    const std::string result = "3456";

    const fst::StringLeftWeight<char> lhs_weight(lhs.begin(), lhs.end());
    const fst::StringLeftWeight<char> rhs_weight(rhs.begin(), rhs.end());
    const fst::StringLeftWeight<char> result_weight(result.begin(), result.end());
    ASSERT_EQ(result_weight, fst::Divide(lhs_weight,rhs_weight, fst::DIVIDE_LEFT));
  }

  // weight / weight
  {
    const std::string lhs = "124";
    const std::string rhs = "123456";
    const std::string result = "";

    const fst::StringLeftWeight<char> lhs_weight(lhs.begin(), lhs.end());
    const fst::StringLeftWeight<char> rhs_weight(rhs.begin(), rhs.end());
    const fst::StringLeftWeight<char> result_weight(result.begin(), result.end());
    ASSERT_EQ(result_weight, fst::Divide(lhs_weight,rhs_weight, fst::DIVIDE_LEFT));
  }

  // weight / Zero()
  {
    const std::string lhs = "123456";

    const fst::StringLeftWeight<char> lhs_weight(lhs.begin(), lhs.end());
    ASSERT_EQ(fst::StringLeftWeight<char>::NoWeight(), fst::Divide(lhs_weight, fst::StringLeftWeight<char>::Zero(), fst::DIVIDE_LEFT));
  }

  // Zero() / weight
  {
    const std::string rhs = "123456";

    const fst::StringLeftWeight<char> rhs_weight(rhs.begin(), rhs.end());
    ASSERT_EQ(fst::StringLeftWeight<char>::Zero(), fst::Divide(fst::StringLeftWeight<char>::Zero(), rhs_weight, fst::DIVIDE_LEFT));
  }

  // weight / NoWeight()
  {
    const std::string lhs = "123456";

    const fst::StringLeftWeight<char> lhs_weight(lhs.begin(), lhs.end());
    ASSERT_EQ(fst::StringLeftWeight<char>::NoWeight(), fst::Divide(lhs_weight, fst::StringLeftWeight<char>::NoWeight(), fst::DIVIDE_LEFT));
  }

  // NoWeight() / weight
  {
    const std::string rhs = "123456";

    const fst::StringLeftWeight<char> rhs_weight(rhs.begin(), rhs.end());
    ASSERT_EQ(fst::StringLeftWeight<char>::NoWeight(), fst::Divide(fst::StringLeftWeight<char>::NoWeight(), rhs_weight, fst::DIVIDE_LEFT));
  }

  // weight / One()
  {
    const std::string lhs = "123456";
    const std::string result = "123456";

    const fst::StringLeftWeight<char> lhs_weight(lhs.begin(), lhs.end());
    const fst::StringLeftWeight<char> result_weight(result.begin(), result.end());
    ASSERT_EQ(result_weight, fst::Divide(lhs_weight, fst::StringLeftWeight<char>::One(), fst::DIVIDE_LEFT));
  }

  // One() / weight
  {
    const std::string rhs = "123456";
    const std::string result = "";

    const fst::StringLeftWeight<char> rhs_weight(rhs.begin(), rhs.end());
    const fst::StringLeftWeight<char> result_weight(result.begin(), result.end());
    ASSERT_EQ(result_weight, fst::Divide(fst::StringLeftWeight<char>::One(), rhs_weight, fst::DIVIDE_LEFT));
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                           fst_string_weight_tests
// -----------------------------------------------------------------------------

TEST(fst_byte_weight_test, static_const) {
  ASSERT_EQ("left_string", fst::StringLeftWeight<irs::byte_type>::Type());
  ASSERT_TRUE(fst::StringLeftWeight<irs::byte_type>::Zero().Empty());
  ASSERT_TRUE(fst::StringLeftWeight<irs::byte_type>::Zero().Member());
  ASSERT_EQ(fst::StringLeftWeight<irs::byte_type>::One(), fst::StringLeftWeight<irs::byte_type>::Zero());
  ASSERT_EQ(fst::StringLeftWeight<irs::byte_type>::NoWeight(), fst::StringLeftWeight<irs::byte_type>::Zero());
}

TEST(fst_byte_weight_test, plus) {
  // weight + weight
  {
    const std::vector<irs::byte_type> lhs { 1,2,3,4,5,6 };
    const std::vector<irs::byte_type> rhs { 1,2,4 };
    const std::vector<irs::byte_type> prefix{ 1,2 };

    const fst::StringLeftWeight<irs::byte_type> lhs_weight(lhs.begin(), lhs.end());
    const fst::StringLeftWeight<irs::byte_type> rhs_weight(rhs.begin(), rhs.end());
    const fst::StringLeftWeight<irs::byte_type> prefix_weight(prefix.begin(), prefix.end());
    ASSERT_EQ(prefix_weight, fst::Plus(lhs_weight,rhs_weight));
  }

  // weight + weight
  {
    const std::vector<irs::byte_type> lhs { 1,2,4 };
    const std::vector<irs::byte_type> rhs { 1,2,3,4,5,6 };
    const std::vector<irs::byte_type> prefix{ 1,2 };

    const fst::StringLeftWeight<irs::byte_type> lhs_weight(lhs.begin(), lhs.end());
    const fst::StringLeftWeight<irs::byte_type> rhs_weight(rhs.begin(), rhs.end());
    const fst::StringLeftWeight<irs::byte_type> prefix_weight(prefix.begin(), prefix.end());
    ASSERT_EQ(prefix_weight, fst::Plus(lhs_weight,rhs_weight));
  }

  // weight + Zero()
  {
    const std::vector<irs::byte_type> lhs{ 1,2,3,4,5,6 };
    const std::vector<irs::byte_type> prefix{ };

    const fst::StringLeftWeight<irs::byte_type> lhs_weight(lhs.begin(), lhs.end());
    const fst::StringLeftWeight<irs::byte_type> prefix_weight(prefix.begin(), prefix.end());
    ASSERT_EQ(prefix_weight, fst::Plus(lhs_weight, fst::StringLeftWeight<irs::byte_type>::Zero()));
  }

  // Zero() + weight
  {
    const std::vector<irs::byte_type> rhs{ 1,2,3,4,5,6 };
    const std::vector<irs::byte_type> prefix{ };

    const fst::StringLeftWeight<irs::byte_type> rhs_weight(rhs.begin(), rhs.end());
    const fst::StringLeftWeight<irs::byte_type> prefix_weight(prefix.begin(), prefix.end());
    ASSERT_EQ(prefix_weight, fst::Plus(fst::StringLeftWeight<irs::byte_type>::Zero(), rhs_weight));
  }

  // weight + NoWeight()
  {
    const std::vector<irs::byte_type> lhs{ 1,2,3,4,5,6 };
    const std::vector<irs::byte_type> prefix{ };

    const fst::StringLeftWeight<irs::byte_type> lhs_weight(lhs.begin(), lhs.end());
    const fst::StringLeftWeight<irs::byte_type> prefix_weight(prefix.begin(), prefix.end());
    ASSERT_EQ(prefix_weight, fst::Plus(lhs_weight, fst::StringLeftWeight<irs::byte_type>::NoWeight()));
  }

  // NoWeight() + weight
  {
    const std::vector<irs::byte_type> rhs{ 1,2,3,4,5,6 };
    const std::vector<irs::byte_type> prefix{ };

    const fst::StringLeftWeight<irs::byte_type> rhs_weight(rhs.begin(), rhs.end());
    const fst::StringLeftWeight<irs::byte_type> prefix_weight(prefix.begin(), prefix.end());
    ASSERT_EQ(prefix_weight, fst::Plus(fst::StringLeftWeight<irs::byte_type>::NoWeight(), rhs_weight));
  }

  // weight + One()
  {
    const std::vector<irs::byte_type> lhs{ 1,2,3,4,5,6 };
    const std::vector<irs::byte_type> prefix{ };

    const fst::StringLeftWeight<irs::byte_type> lhs_weight(lhs.begin(), lhs.end());
    const fst::StringLeftWeight<irs::byte_type> prefix_weight(prefix.begin(), prefix.end());
    ASSERT_EQ(prefix_weight, fst::Plus(lhs_weight, fst::StringLeftWeight<irs::byte_type>::One()));
  }

  // One() + weight
  {
    const std::vector<irs::byte_type> rhs{ 1,2,3,4,5,6 };
    const std::vector<irs::byte_type> prefix{ };

    const fst::StringLeftWeight<irs::byte_type> rhs_weight(rhs.begin(), rhs.end());
    const fst::StringLeftWeight<irs::byte_type> prefix_weight(prefix.begin(), prefix.end());
    ASSERT_EQ(prefix_weight, fst::Plus(fst::StringLeftWeight<irs::byte_type>::One(), rhs_weight));
  }
}

TEST(fst_byte_weight_test, times) {
  // weight * weight
  {
    const std::vector<irs::byte_type> lhs { 1,2,3,4,5,6 };
    const std::vector<irs::byte_type> rhs { 1,2,4 };
    const std::vector<irs::byte_type> result{ 1,2,3,4,5,6,1,2,4 };

    const fst::StringLeftWeight<irs::byte_type> lhs_weight(lhs.begin(), lhs.end());
    const fst::StringLeftWeight<irs::byte_type> rhs_weight(rhs.begin(), rhs.end());
    const fst::StringLeftWeight<irs::byte_type> result_weight(result.begin(), result.end());
    ASSERT_EQ(result_weight, fst::Times(lhs_weight,rhs_weight));
  }

  // weight * weight
  {
    const std::vector<irs::byte_type> lhs { 1,2,4 };
    const std::vector<irs::byte_type> rhs { 1,2,3,4,5,6 };
    const std::vector<irs::byte_type> result{ 1,2,4,1,2,3,4,5,6 };

    const fst::StringLeftWeight<irs::byte_type> lhs_weight(lhs.begin(), lhs.end());
    const fst::StringLeftWeight<irs::byte_type> rhs_weight(rhs.begin(), rhs.end());
    const fst::StringLeftWeight<irs::byte_type> result_weight(result.begin(), result.end());
    ASSERT_EQ(result_weight, fst::Times(lhs_weight,rhs_weight));
  }

  // weight * Zero()
  {
    const std::vector<irs::byte_type> lhs{ 1,2,3,4,5,6 };

    const fst::StringLeftWeight<irs::byte_type> lhs_weight(lhs.begin(), lhs.end());
    ASSERT_EQ(lhs_weight, fst::Times(lhs_weight, fst::StringLeftWeight<irs::byte_type>::Zero()));
  }

  // Zero() * weight
  {
    const std::vector<irs::byte_type> rhs{ 1,2,3,4,5,6 };

    const fst::StringLeftWeight<irs::byte_type> rhs_weight(rhs.begin(), rhs.end());
    ASSERT_EQ(rhs_weight, fst::Times(fst::StringLeftWeight<irs::byte_type>::Zero(), rhs_weight));
  }

  // weight * NoWeight()
  {
    const std::vector<irs::byte_type> lhs{ 1,2,3,4,5,6 };

    const fst::StringLeftWeight<irs::byte_type> lhs_weight(lhs.begin(), lhs.end());
    ASSERT_EQ(lhs_weight, fst::Times(lhs_weight, fst::StringLeftWeight<irs::byte_type>::NoWeight()));
  }

  // NoWeight() * weight
  {
    const std::vector<irs::byte_type> rhs{ 1,2,3,4,5,6 };

    const fst::StringLeftWeight<irs::byte_type> rhs_weight(rhs.begin(), rhs.end());
    ASSERT_EQ(rhs_weight, fst::Times(fst::StringLeftWeight<irs::byte_type>::NoWeight(), rhs_weight));
  }

  // weight * One()
  {
    const std::vector<irs::byte_type> lhs{ 1,2,3,4,5,6 };

    const fst::StringLeftWeight<irs::byte_type> lhs_weight(lhs.begin(), lhs.end());
    ASSERT_EQ(lhs_weight, fst::Times(lhs_weight, fst::StringLeftWeight<irs::byte_type>::One()));
  }

  // One() * weight
  {
    const std::vector<irs::byte_type> rhs{ 1,2,3,4,5,6 };

    const fst::StringLeftWeight<irs::byte_type> rhs_weight(rhs.begin(), rhs.end());
    ASSERT_EQ(rhs_weight, fst::Times(fst::StringLeftWeight<irs::byte_type>::One(), rhs_weight));
  }
}

TEST(fst_byte_weight_test, divide) {
  // weight / weight
  {
    const std::vector<irs::byte_type> lhs { 1,2,3,4,5,6 };
    const std::vector<irs::byte_type> rhs { 1,2 };
    const std::vector<irs::byte_type> result{ 3,4,5,6 };

    const fst::StringLeftWeight<irs::byte_type> lhs_weight(lhs.begin(), lhs.end());
    const fst::StringLeftWeight<irs::byte_type> rhs_weight(rhs.begin(), rhs.end());
    const fst::StringLeftWeight<irs::byte_type> result_weight(result.begin(), result.end());
    ASSERT_EQ(result_weight, fst::Divide(lhs_weight,rhs_weight, fst::DIVIDE_LEFT));
  }

  // weight / weight
  {
    const std::vector<irs::byte_type> lhs { 1,2 };
    const std::vector<irs::byte_type> rhs { 1,2,3,4,5,6 };
    const std::vector<irs::byte_type> result{ };

    const fst::StringLeftWeight<irs::byte_type> lhs_weight(lhs.begin(), lhs.end());
    const fst::StringLeftWeight<irs::byte_type> rhs_weight(rhs.begin(), rhs.end());
    const fst::StringLeftWeight<irs::byte_type> result_weight(result.begin(), result.end());
    ASSERT_EQ(result_weight, fst::Divide(lhs_weight,rhs_weight, fst::DIVIDE_LEFT));
  }

  // weight / Zero()
  {
    const std::vector<irs::byte_type> lhs { 1,2,3,4,5,6 };

    const fst::StringLeftWeight<irs::byte_type> lhs_weight(lhs.begin(), lhs.end());
    ASSERT_EQ(lhs_weight, fst::Divide(lhs_weight, fst::StringLeftWeight<irs::byte_type>::Zero(), fst::DIVIDE_LEFT));
  }

  // Zero() / weight
  {
    const std::vector<irs::byte_type> rhs { 1,2,3,4,5,6 };
    const std::vector<irs::byte_type> result{ };

    const fst::StringLeftWeight<irs::byte_type> rhs_weight(rhs.begin(), rhs.end());
    const fst::StringLeftWeight<irs::byte_type> result_weight(result.begin(), result.end());
    ASSERT_EQ(result_weight, fst::Divide(fst::StringLeftWeight<irs::byte_type>::Zero(), rhs_weight, fst::DIVIDE_LEFT));
  }

  // weight / NoWeight()
  {
    const std::vector<irs::byte_type> lhs { 1,2,3,4,5,6 };

    const fst::StringLeftWeight<irs::byte_type> lhs_weight(lhs.begin(), lhs.end());
    ASSERT_EQ(lhs_weight, fst::Divide(lhs_weight, fst::StringLeftWeight<irs::byte_type>::NoWeight(), fst::DIVIDE_LEFT));
  }

  // NoWeight() / weight
  {
    const std::vector<irs::byte_type> rhs { 1,2,3,4,5,6 };
    const std::vector<irs::byte_type> result{ };

    const fst::StringLeftWeight<irs::byte_type> rhs_weight(rhs.begin(), rhs.end());
    const fst::StringLeftWeight<irs::byte_type> result_weight(result.begin(), result.end());
    ASSERT_EQ(result_weight, fst::Divide(fst::StringLeftWeight<irs::byte_type>::NoWeight(), rhs_weight, fst::DIVIDE_LEFT));
  }

  // weight / One()
  {
    const std::vector<irs::byte_type> lhs { 1,2,3,4,5,6 };
    const std::vector<irs::byte_type> result { 1,2,3,4,5,6 };

    const fst::StringLeftWeight<irs::byte_type> lhs_weight(lhs.begin(), lhs.end());
    const fst::StringLeftWeight<irs::byte_type> result_weight(result.begin(), result.end());
    ASSERT_EQ(result_weight, fst::Divide(lhs_weight, fst::StringLeftWeight<irs::byte_type>::One(), fst::DIVIDE_LEFT));
  }

  // One() / weight
  {
    const std::string rhs = "123456";
    const std::vector<irs::byte_type> result{ };

    const fst::StringLeftWeight<irs::byte_type> rhs_weight(rhs.begin(), rhs.end());
    const fst::StringLeftWeight<irs::byte_type> result_weight(result.begin(), result.end());
    ASSERT_EQ(result_weight, fst::Divide(fst::StringLeftWeight<irs::byte_type>::One(), rhs_weight, fst::DIVIDE_LEFT));
  }
}

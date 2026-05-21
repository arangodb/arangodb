////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "utils/wildcard_utils.hpp"

#include "tests_shared.hpp"
#include "utils/automaton_utils.hpp"
#include "utils/fstext/fst_sorted_range_matcher.hpp"

class wildcard_utils_test : public test_base {
 protected:
  static void assert_properties(const irs::automaton& a) {
    constexpr auto EXPECTED_PROPERTIES =
      fst::kILabelSorted | fst::kOLabelSorted | fst::kIDeterministic |
      fst::kAcceptor | fst::kUnweighted;

    EXPECT_EQ(EXPECTED_PROPERTIES, a.Properties(EXPECTED_PROPERTIES, true));
  }
};

TEST_F(wildcard_utils_test, same_start) {
  {
    auto a = irs::FromWildcard("%р%");
    assert_properties(a);

    bool r = irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("р")));
    EXPECT_TRUE(r);
    r = irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("с")));
    EXPECT_FALSE(r);
    r = irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("ё")));
    EXPECT_FALSE(r);
  }
  {
    auto a = irs::FromWildcard("%ара%");
    assert_properties(a);

    bool r = irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("ара")));
    EXPECT_TRUE(r);
    r = irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("аса")));
    EXPECT_FALSE(r);
    r = irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("аёа")));
    EXPECT_FALSE(r);
  }
}

TEST_F(wildcard_utils_test, same_end) {
  {
    auto a = irs::FromWildcard("%ѿ%");
    assert_properties(a);

    bool r = irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("ѿ")));
    EXPECT_TRUE(r);
    r = irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("с")));
    EXPECT_FALSE(r);
    r = irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("ё")));
    EXPECT_FALSE(r);
  }
  {
    auto a = irs::FromWildcard("%аѿа%");
    assert_properties(a);

    bool r = irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("аѿа")));
    EXPECT_TRUE(r);
    r = irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("аса")));
    EXPECT_FALSE(r);
    r = irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("аёа")));
    EXPECT_FALSE(r);
  }
}

TEST_F(wildcard_utils_test, match_wildcard) {
  {
    auto a = irs::FromWildcard("%rc%");
    assert_properties(a);

    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("corrction"))));
  }

  {
    auto a = irs::FromWildcard("%rc%");
    assert_properties(a);

    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("corrction"))));
  }

  {
    auto a = irs::FromWildcard("%bcebce%");
    assert_properties(a);

    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("bcebcebce"))));
  }

  {
    auto a = irs::FromWildcard("%bcebcd%");
    assert_properties(a);

    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("bcebcebcd"))));
  }

  {
    auto a = irs::FromWildcard("%bcebced%");
    assert_properties(a);

    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("bcebcebced"))));
    EXPECT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("bcebcebbced"))));
  }

  {
    auto a = irs::FromWildcard("%bcebce");
    assert_properties(a);

    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("bcebcebce"))));
    EXPECT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("bcebcebbce"))));
  }

  {
    auto a = irs::FromWildcard("%rrc%");
    assert_properties(a);

    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("corrction"))));
  }

  {
    auto a = irs::FromWildcard("%arc%");
    assert_properties(a);

    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("arrrc"))));
  }

  {
    auto a = irs::FromWildcard("%aca%");
    assert_properties(a);

    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("arrrc"))));
  }

  {
    auto a = irs::FromWildcard("%r_c%");
    assert_properties(a);

    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("correc"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("corerc"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("correrction"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("corrrc"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("correction"))));
  }

  {
    auto a = irs::FromWildcard("%_r_c%");
    assert_properties(a);

    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("correction"))));
  }

  // mixed from wikipedia
  {
    auto a = irs::FromWildcard("%a%_r_c%");
    assert_properties(a);

    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("Error detection and correction"))));
    //^      ^ ^
  }

  {
    auto a = irs::FromWildcard("%a%bce_bc");
    assert_properties(a);

    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abceabc"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abcebbcecbc"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abceabcbcebbc"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abcebcebc"))));
  }

  {
    auto a = irs::FromWildcard("%a%bc__bc");
    assert_properties(a);

    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abcbbc"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abcbcbcc"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abcbcbcb"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abcbbbc"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abcbcbc"))));
  }

  {
    auto a = irs::FromWildcard("%a%bc_bc");
    assert_properties(a);

    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abcbbc"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abcbbc"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abcabc"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abccbc"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abcbcbcbccbc"))));
  }

  {
    auto a = irs::FromWildcard("%a%b_b");
    assert_properties(a);

    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abab"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abbb"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abbbb"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abbabbbbbbb"))));
  }

  {
    auto a = irs::FromWildcard("%a%b__b");
    assert_properties(a);

    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abcab"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abbbb"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abbbbb"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abbbbbb"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abbccbbbcbbbbbb"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abbabbbbbbb"))));
  }

  {
    auto a = irs::FromWildcard("%a%bce___bce");
    assert_properties(a);

    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abcabcebcebce"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abbccbcebbbbce"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abbccbcebcebce"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abbccbcebcebbce"))));
  }

  {
    auto a = irs::FromWildcard("%a%bce____bce");
    assert_properties(a);

    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abceabcdbcebcebce"))));
  }

  {
    auto a = irs::FromWildcard("%a%b___b");
    assert_properties(a);

    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abcabbbcab"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abbbb"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abbbbb"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abbbbbb"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abbccbbbcbbbbbb"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abbabbbbbbb"))));
  }

  {
    auto a = irs::FromWildcard("%a%bce_____b");
    assert_properties(a);

    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a,
      irs::ViewCast<irs::byte_type>(std::string_view("abcebcebcebcebcebcb"))));
  }

  {
    auto a = irs::FromWildcard("%a%__b_b");
    assert_properties(a);

    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("a__bab"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("afasfdwerfwefbbb"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("abbbbbbbbbbbbbbbbbbbb"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abbabbbbbbb"))));
  }

  {
    auto a = irs::FromWildcard("%a%__b_b");
    assert_properties(a);

    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("a__bab"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("afasfdwerfwefbbb"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("abbbbbbbbbbbbbbbbbbbb"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abbabbbbbbb"))));
  }

  {
    auto a = irs::FromWildcard("%a%_bce____def___b%");
    assert_properties(a);

    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("a__bcedefadefbabb"))));
  }

  // mixed
  {
    auto a = irs::FromWildcard("a%bce_b");
    assert_properties(a);

    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(""))));
    ASSERT_FALSE(irs::Accept<char>(a, std::string_view{}));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("aabce1dbce1b"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("aabce1dbce11b"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abce1bb"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abceabce1b"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abcebce1b"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce1b"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce1db"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce11b"))));
  }

  // mixed
  {
    auto a = irs::FromWildcard("a%bce_d");
    assert_properties(a);
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(""))));
    ASSERT_FALSE(irs::Accept<char>(a, std::string_view{}));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("aabce1dbce1d"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("aabce1dbce11d"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abceabce1d"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abcebce1d"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce1d"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce1d1"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce11d"))));
  }

  // check automaton structure
  {
    auto lhs = irs::FromWildcard("%b%");
    auto rhs = irs::FromWildcard("%b%%%");
    ASSERT_EQ(lhs.NumStates(), rhs.NumStates());
    assert_properties(lhs);
    assert_properties(rhs);

    for (decltype(lhs)::StateId state = 0; state < lhs.NumStates(); ++state) {
      ASSERT_EQ(lhs.NumArcs(state), rhs.NumArcs(state));
    }
  }

  // check automaton structure
  {
    auto lhs = irs::FromWildcard("b%%%%%s");
    auto rhs = irs::FromWildcard("b%%%s");
    ASSERT_EQ(lhs.NumStates(), rhs.NumStates());
    assert_properties(lhs);
    assert_properties(rhs);

    for (decltype(lhs)::StateId state = 0; state < lhs.NumStates(); ++state) {
      ASSERT_EQ(lhs.NumArcs(state), rhs.NumArcs(state));
    }
  }

  // check automaton structure
  {
    auto lhs = irs::FromWildcard("b%%__%%%s%");
    auto rhs = irs::FromWildcard("b%%%%%%%__%%%%%%%%s%");
    ASSERT_EQ(lhs.NumStates(), rhs.NumStates());
    assert_properties(lhs);
    assert_properties(rhs);

    for (decltype(lhs)::StateId state = 0; state < lhs.NumStates(); ++state) {
      ASSERT_EQ(lhs.NumArcs(state), rhs.NumArcs(state));
    }
  }

  // nil string
  {
    auto a = irs::FromWildcard(std::string_view{});
    assert_properties(a);
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(""))));
    ASSERT_TRUE(irs::Accept<char>(a, std::string_view{}));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("a"))));
  }

  // empty string
  {
    auto a = irs::FromWildcard(std::string_view{""});
    assert_properties(a);
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(""))));
    ASSERT_TRUE(irs::Accept<char>(a, std::string_view{}));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("a"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("\xE2\x9E\x96"))));
  }

  // any or empty string
  {
    auto a = irs::FromWildcard("%");

    assert_properties(a);
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(""))));
    ASSERT_TRUE(irs::Accept<char>(a, std::string_view{}));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("a"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abc"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("\xD0\xBF"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("\xE2\x9E\x96"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("\xF0\x9F\x98\x81"))));
  }

  // any or empty string
  {
    auto a = irs::FromWildcard("%%");
    assert_properties(a);
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(""))));
    ASSERT_TRUE(irs::Accept<char>(a, std::string_view{}));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("a"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("aa"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce1d"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce1d1"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce11d"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("\xE2\x9E\x96"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("\xF0\x9F\x98\x81"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("a\xF0\x9F\x98\x81"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("\xF0\x9F\x98\x81\xF0\x9F\x98\x81"))));
  }

  // any char
  {
    auto a = irs::FromWildcard("_");
    assert_properties(a);
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(""))));
    ASSERT_FALSE(irs::Accept<char>(a, std::string_view{}));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("a"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abc"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("\xD0\xBF"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("\xE2\x9E\x96"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("\xF0\x9F\x98\x81"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("a\xF0\x9F\x98\x81"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("\xF0\x9F\x98\x81\xF0\x9F\x98\x81"))));
  }

  // two any chars
  {
    auto a = irs::FromWildcard("__");
    assert_properties(a);
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(""))));
    ASSERT_FALSE(irs::Accept<char>(a, std::string_view{}));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("a"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("\xE2\x9E\x96"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("a\xE2\x9E\x96\xD0\xBF"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a,
      irs::ViewCast<irs::byte_type>(std::string_view("\xE2\x9E\x96\xD0\xBF"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("\xE2\x9E\x96\xE2\x9E\x96"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("\xF0\x9F\x98\x81\xF0\x9F\x98\x81"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("a\xF0\x9F\x98\x81\xF0\x9F\x98\x81"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("ba"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce1d"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce1d1"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce11d"))));
  }

  // any char (suffix)
  {
    auto a = irs::FromWildcard("a_");
    assert_properties(a);
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(""))));
    ASSERT_FALSE(irs::Accept<char>(a, std::string_view{}));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("a_"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("a"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("ab"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("a\xF0\x9F\x98\x81\xF0\x9F\x98\x81"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("a\xF0\x9F\x98\x81"))));
  }

  // any char (prefix)
  {
    auto a = irs::FromWildcard("_a");
    assert_properties(a);
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(""))));
    ASSERT_FALSE(irs::Accept<char>(a, std::string_view{}));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("_a"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("a"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("aa"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("ba"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a,
      irs::ViewCast<irs::byte_type>(std::string_view("\xF0\x9F\x98\x81\x61"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("\xE2\x9E\x96\x61"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(
           "\xE2\xFF\xFF\x61"))));  // don't accept invalid utf8 sequence
  }

  // escaped '_'
  {
    auto a = irs::FromWildcard("\\_a");
    assert_properties(a);
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(""))));
    ASSERT_FALSE(irs::Accept<char>(a, std::string_view{}));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("_a"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("a"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("ba"))));
  }

  // escaped '\'
  {
    auto a = irs::FromWildcard("\\\\\\_a");
    assert_properties(a);
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(""))));
    ASSERT_FALSE(irs::Accept<char>(a, std::string_view{}));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("\\_a"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("a"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("\\_\xE2\x9E\x96"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("ba"))));
  }

  // '\0'
  {
    static constexpr auto kNull = std::string_view{"%\0%", 3};
    auto a = irs::FromWildcard(kNull);
    assert_properties(a);
    EXPECT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(""))));
    EXPECT_FALSE(irs::Accept<char>(a, std::string_view{}));
    EXPECT_TRUE(
      irs::Accept<irs::byte_type>(a, irs::ViewCast<irs::byte_type>(kNull)));
    EXPECT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("a\0", 2))));
    EXPECT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("\0a", 2))));
    EXPECT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("a\0a", 3))));
    EXPECT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("aa", 2))));
  }

  // escaped 'a'
  {
    auto a = irs::FromWildcard("\\a");
    assert_properties(a);
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(""))));
    ASSERT_FALSE(irs::Accept<char>(a, std::string_view{}));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("\\a"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("a"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("\\\\a"))));
  }

  // nonterminated '\'
  {
    auto a = irs::FromWildcard("a\\");
    assert_properties(a);
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(""))));
    ASSERT_FALSE(irs::Accept<char>(a, std::string_view{}));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("a\\"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("a"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("ba"))));
  }

  // escaped '%'
  {
    auto a = irs::FromWildcard("\\\\\\%a");
    assert_properties(a);
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(""))));
    ASSERT_FALSE(irs::Accept<char>(a, std::string_view{}));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("\\%a"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("a"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("ba"))));
  }

  // prefix
  {
    auto a = irs::FromWildcard("foo%");
    assert_properties(a);
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(""))));
    ASSERT_FALSE(irs::Accept<char>(a, std::string_view{}));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("foo"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("foobar"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("foa"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("foabar"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("foo\xE2\x9E\x96\xE2\x9E\x96"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("foo\xF0\x9F\x98\x81\xE2\x9E\x96\xE2\x9E\x96"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(
           "foo\xD0\xBF\xF0\x9F\x98\x81\xE2\x9E\x96\xE2\x9E\x96"))));
  }

  // prefix
  {
    auto a = irs::FromWildcard("foo\\%");
    assert_properties(a);
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(""))));
    ASSERT_FALSE(irs::Accept<char>(a, std::string_view{}));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("foo"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("foo%"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("foobar"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("foa"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("foabar"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("foo\xE2\x9E\x96\xE2\x9E\x96"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("foo\xF0\x9F\x98\x81\xE2\x9E\x96\xE2\x9E\x96"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(
           "foo\xD0\xBF\xF0\x9F\x98\x81\xE2\x9E\x96\xE2\x9E\x96"))));
  }

  // mixed
  {
    auto a = irs::FromWildcard("a%foo");
    assert_properties(a);
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(""))));
    ASSERT_FALSE(irs::Accept<char>(a, std::string_view{}));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("affoo"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("aaafofoo"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("aaafafoo"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("aaafaffoo"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("aaafoofoo"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("aaafooffffoo"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("aaafooofoo"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abcdfo"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abcdfo"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("aaaaaaaaaaaaaaaaaafoo"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a,
      irs::ViewCast<irs::byte_type>(std::string_view("aaaaaaaaaaaaaaabfoo"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("aaaaaaaaaaaaa\x66\x6F\x6F"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("aaaaaaaaaaaaa\x66\x6F\x6F"))));
  }

  // mixed
  {
    auto a = irs::FromWildcard("a%foo%boo");
    assert_properties(a);
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(""))));
    ASSERT_FALSE(irs::Accept<char>(a, std::string_view{}));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("afooboo"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("afoofoobooboo"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("afoofooboofooboo"))));
  }

  // suffix
  {
    auto a = irs::FromWildcard("%foo");
    assert_properties(a);
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(""))));
    ASSERT_FALSE(irs::Accept<char>(a, std::string_view{}));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("foo"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("fofoo"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("foofoo"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("fooofoo"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("ffoo"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("fffoo"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("bfoo"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("foa"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("bfoa"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("\xE2\x9E\x96\xE2\x9E\x96\x66\x6F\x6F"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("\xE2\x9E\x96\xE2\x9E\x96\x66\x66\x6F\x6F"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(
           "\xF0\x9F\x98\x81\xE2\x9E\x96\xE2\x9E\x96\x66\x6F\x6F"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(
           "\xD0\xBF\xF0\x9F\x98\x81\xE2\x9E\x96\xE2\x9E\x96\x66\x6F\x6F"))));
  }

  // prefix
  {
    auto a = irs::FromWildcard("v%%");
    assert_properties(a);
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(""))));
    ASSERT_FALSE(irs::Accept<char>(a, std::string_view{}));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("vcc"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("vccc"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("vczc"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("vczczvccccc"))));
  }

  // suffix
  {
    auto a = irs::FromWildcard("%ffoo");
    assert_properties(a);
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(""))));
    ASSERT_FALSE(irs::Accept<char>(a, std::string_view{}));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("ffoo"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("ffooffoo"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("fffoo"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("bffoo"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("ffob"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("bfoa"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("\xE2\x9E\x96\xE2\x9E\x96\x66\x66\x6F\x6F"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(
           "\xF0\x9F\x98\x81\xE2\x9E\x96\xE2\x9E\x96\x66\x66\x6F\x6F"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a,
      irs::ViewCast<irs::byte_type>(std::string_view(
        "\xD0\xBF\xF0\x9F\x98\x81\xE2\x9E\x96\xE2\x9E\x96\x66\x66\x6F\x6F"))));
  }

  // mixed
  {
    auto a = irs::FromWildcard("a%a");
    assert_properties(a);
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(""))));
    ASSERT_FALSE(irs::Accept<char>(a, std::string_view{}));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("aa"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("aaa"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abcdfsa"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abcdfsa"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a,
      irs::ViewCast<irs::byte_type>(std::string_view("aaaaaaaaaaaaaaaaaa"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("aaaaaaaaaaaaaaab"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("aaaaaaaaaaaaa\xE2\x9E\x96\x61"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("aaaaaaaaaaaaa\xE2\x9E\x61"))));
  }

  // mixed
  {
    auto a = irs::FromWildcard("_%a_%_a_%");
    assert_properties(a);
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(""))));
    ASSERT_FALSE(irs::Accept<char>(a, std::string_view{}));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("baaaab"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a,
      irs::ViewCast<irs::byte_type>(std::string_view("aaaaaaaaaaaaaaaaaa"))));
  }

  // mixed, invalid UTF8-sequence
  {
    auto a =
      irs::FromWildcard("\x5F\x25\xE2\x9E\x61\x5F\x25\x5F\xE2\x9E\x61\x5F\x25");
    assert_properties(a);
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(""))));
    ASSERT_FALSE(irs::Accept<char>(a, std::string_view{}));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("\x98\xE2\x9E\x61\x97\x97\xE2\x9E\x61\x98"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(
           "\xE2\x9E\x61\xE2\x9E\x61\xE2\x9E\x61\xE2\x9E\x61\xE2\x9E\x61\xE2"
           "\x9E\x61\xE2\x9E\x61\xE2\x9E\x61"))));
  }

  // mixed
  {
    auto a =
      irs::FromWildcard("\x5F\x25\xE2\x9E\x9E\x5F\x25\x5F\xE2\x9E\x9E\x5F\x25");
    assert_properties(a);
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(""))));
    ASSERT_FALSE(irs::Accept<char>(a, std::string_view{}));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(
           "\xE2\x9E\x9E\xE2\x9E\x9E\xE2\x9E\x9E\xE2\x9E\x9E\xE2\x9E\x9E\xE2"
           "\x9E\x9E\xE2\x9E\x9E\xE2\x9E\x9E"))));

    // invalid UTF8-sequence
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("\x98\xE2\x9E\x9E\x97\x97\xE2\x9E\x9E\x98"))));
  }

  // mixed, invalid UTF8-sequence
  {
    auto a = irs::FromWildcard("\xE2\x9E\x61\x25\xE2\x9E\x61");
    assert_properties(a);
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(""))));
    ASSERT_FALSE(irs::Accept<char>(a, std::string_view{}));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("\xE2\x9E\x61\xE2\x9E\x61"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("\xE2\x9E\x61\x61\xE2\x9E\x61"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("\xE2\x9E\x61\x9E\x61\xE2\x9E\x61"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("\xE2\x9E\x61\x9E\x61\xE2\x9E\xE2\x9E\x61"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("\xE2\x9E\x61\xE2\x9E\x61\xE2\x9E\x61"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(
           "\xE2\x9E\x61\xE2\x9E\x61\xE2\x9E\x61\xE2\x9E\x61"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("\xE2\x9E\x61\xE2\x9E\x61\xE2\x9E\x61\xE2\x9E\x61"
                            "\xE2\x9E\x61\xE2\x9E\x61\xE2\x9E\x61"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("\xE2\x9E\x61\xE2\x9E\x61\xE2\x9E\x61\xE2\x9E\x61"
                            "\xE2\x9E\x61\xE2\x9E\x61\xE2\x9E\x61\x61"))));
  }

  // mixed
  {
    auto a = irs::FromWildcard("\xE2\x9E\x9E\x25\xE2\x9E\x9E");
    assert_properties(a);
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(""))));
    ASSERT_FALSE(irs::Accept<char>(a, std::string_view{}));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("\xE2\x9E\x9E\xE2\x9E\x9E"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("\xE2\x9E\x9E\x9E\x9E\xE2\x9E\xE2\x9E\x9E"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("\xE2\x9E\x9E\xE2\x9E\x9E\xE2\x9E\x9E"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(
           "\xE2\x9E\x9E\xE2\x9E\x9E\xE2\x9E\x9E\xE2\x9E\x9E"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("\xE2\x9E\x9E\xE2\x9E\x9E\xE2\x9E\x9E\xE2\x9E\x9E"
                            "\xE2\x9E\x9E\xE2\x9E\x9E\xE2\x9E\x9E"))));

    // invalid UTF8 sequence
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("\xE2\x9E\x9E\x9E\xE2\x9E\x9E"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("\xE2\x9E\x9E\x9E\x9E\xE2\x9E\x9E"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("\xE2\x9E\x9E\xE2\x9E\x9E\xE2\x9E\x9E\xE2\x9E\x9E"
                            "\xE2\x9E\x9E\xE2\x9E\x9E\xE2\x9E\x9E\x9E"))));
  }

  // mixed
  {
    auto a = irs::FromWildcard("a%bce_d");
    assert_properties(a);
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(""))));
    ASSERT_FALSE(irs::Accept<char>(a, std::string_view{}));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("aabce1dbce1d"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("aabce1dbce11d"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abceabce1d"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abcebce1d"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce1d"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce1d1"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce11d"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce\xD0\xBF\x64"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("azbce\xE2\x9E\x96\x64"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("azbce\xF0\x9F\x98\x81\x64"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("azbce\xE2\x9E\x96\xF0\x9F\x98\x81\x64"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("azbce\xD0\xBF\xD0\xBF\x64"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("az\xD0\xBF\xD0\xBF\x62\x63\x65\xD0\xBF\x64"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(
           "az\xD0\xBF\xD0\xBF\x62\x63\x65\xD0\xBF\x64\x64"))));
  }

  // mixed
  {
    auto a = irs::FromWildcard("b%d%a");
    assert_properties(a);
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(""))));
    ASSERT_FALSE(irs::Accept<char>(a, std::string_view{}));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce1d"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce1d1"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce11d"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(
           "\x62\x61\x7A\xD0\xBF\xD0\xBF\x62\x63\x64\xD0\xBF\x64\x64\x61"))));
  }

  // mixed
  {
    auto a = irs::FromWildcard("a%b%d");
    assert_properties(a);
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(""))));
    ASSERT_FALSE(irs::Accept<char>(a, std::string_view{}));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce1d"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce1d1"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce11d"))));
  }

  // mixed
  {
    auto a = irs::FromWildcard("a%b%db");
    assert_properties(a);
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(""))));
    ASSERT_FALSE(irs::Accept<char>(a, std::string_view{}));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce1d"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce1db"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce1d1"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce11db"))));
  }

  // mixed
  {
    auto a = irs::FromWildcard("%_");
    assert_properties(a);
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(""))));
    ASSERT_FALSE(irs::Accept<char>(a, std::string_view{}));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("a"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("aa"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce1d"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce1d1"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce11d"))));
  }

  // mixed, terminal "\\"
  {
    auto a = irs::FromWildcard("%\\\\");
    assert_properties(a);
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(""))));
    ASSERT_FALSE(irs::Accept<char>(a, std::string_view{}));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("\\"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("a\\"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("aa\\"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce1\\"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce1\\1"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("1azbce11\\"))));
  }

  // mixed, terminal "\\"
  {
    auto a = irs::FromWildcard("%_\\\\");
    assert_properties(a);
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(""))));
    ASSERT_FALSE(irs::Accept<char>(a, std::string_view{}));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("\\"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("a\\"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("aa\\"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce1\\"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce1\\1"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("1azbce11\\"))));
  }

  // mixed, non-terminated "\\"
  {
    auto a = irs::FromWildcard("%\\");
    assert_properties(a);
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(""))));
    ASSERT_TRUE(irs::Accept<char>(a, std::string_view{}));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("\\"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("a\\"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("aa\\"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce1\\"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce1\\1"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("1azbce11\\"))));
  }

  // mixed, non-terminated "\\"
  {
    auto a = irs::FromWildcard("%_\\");
    assert_properties(a);
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(""))));
    ASSERT_FALSE(irs::Accept<char>(a, std::string_view{}));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("\\"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("a\\"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("aa\\"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce1\\"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce1\\1"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("1azbce11\\"))));
  }

  // mixed
  {
    auto a = irs::FromWildcard("%_d");
    assert_properties(a);
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(""))));
    ASSERT_FALSE(irs::Accept<char>(a, std::string_view{}));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("d"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("ad"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("aad"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce1d"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce1d1"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("1azbce11d"))));
  }

  // mixed
  {
    auto a = irs::FromWildcard("%_%_%d");
    assert_properties(a);
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(""))));
    ASSERT_FALSE(irs::Accept<char>(a, std::string_view{}));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("ad"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("add"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("add1"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abd"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("ddd"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("aad"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce1d"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce1d1"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("1azbce11d"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("\xE2\x9E\x96\x64"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("\xE2\x9E\x96\x64\x64\x64"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("a\xE2\x9E\x96\x64"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("e\xF0\x9F\x98\x81\x64"))));

    // invalid UTF8 sequence
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("xE2\x9E\x96\xF0\x9F\x98\x81\x64"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a,
      irs::ViewCast<irs::byte_type>(std::string_view("xD0\xBF\xD0\xBF\x64"))));
  }

  // mixed
  {
    auto a = irs::FromWildcard("%_%_%d%");
    assert_properties(a);
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(""))));
    ASSERT_FALSE(irs::Accept<char>(a, std::string_view{}));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("ad"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("add"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("add1"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abd"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("ddd"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("aad"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce1d"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce1d1"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("1azbce11d"))));
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("\xE2\x9E\x96\x64"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a,
      irs::ViewCast<irs::byte_type>(std::string_view("\xE2\x9E\x96\x64\x64"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("azbce\xE2\x9E\x96\x64"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("azbce\xF0\x9F\x98\x81\x64"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("azbce\xE2\x9E\x96\xF0\x9F\x98\x81\x64\xD0\xBF"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(
           std::string_view("azbce\xD0\xBF\xD0\xBF\x64\xD0\xBF"))));
  }

  // mixed
  {
    auto a = irs::FromWildcard("%%_");
    assert_properties(a);
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(""))));
    ASSERT_FALSE(irs::Accept<char>(a, std::string_view{}));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("a"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("aa"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce1d"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce1d1"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce11d"))));
  }

  // mixed
  {
    auto a = irs::FromWildcard("_%");
    assert_properties(a);
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(""))));
    ASSERT_FALSE(irs::Accept<char>(a, std::string_view{}));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("a"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("aa"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce1d"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce1d1"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("azbce11d"))));
  }

  // mixed
  {
    auto a = irs::FromWildcard("a%_b");
    assert_properties(a);
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("ababab"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abababbbb"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("ababbbbb"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abbbbbb"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abb"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("aab"))));
  }

  // mixed
  {
    auto a = irs::FromWildcard("a%_b%");
    assert_properties(a);
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abababc"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abababcababab"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abababbbbc"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("ababbbbbc"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abbbbbbc"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("abbc"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("aabc"))));
  }

  // mixed
  {
    auto a = irs::FromWildcard("v%%c");
    assert_properties(a);
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(""))));
    ASSERT_FALSE(irs::Accept<char>(a, std::string_view{}));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("vcc"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("vccc"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("vczc"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("vczczvccccc"))));
  }

  // mixed
  {
    auto a = irs::FromWildcard("v%c");
    assert_properties(a);
    ASSERT_FALSE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view(""))));
    ASSERT_FALSE(irs::Accept<char>(a, std::string_view{}));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("vcc"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("vccc"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("vczc"))));
    ASSERT_TRUE(irs::Accept<irs::byte_type>(
      a, irs::ViewCast<irs::byte_type>(std::string_view("vczczvccccc"))));
  }

  // invalid UTF-8 sequence
  ASSERT_EQ(2, irs::FromWildcard("\xD0").NumStates());
  ASSERT_EQ(3, irs::FromWildcard("\xE2\x9E").NumStates());
  ASSERT_EQ(4, irs::FromWildcard("\xF0\x9F\x98").NumStates());
}

TEST_F(wildcard_utils_test, wildcard_type) {
  ASSERT_EQ(irs::WildcardType::kTerm,
            irs::ComputeWildcardType(irs::ViewCast<irs::byte_type>(
              std::string_view("\xD0"))));  // invalid UTF-8 sequence
  ASSERT_EQ(irs::WildcardType::kTerm,
            irs::ComputeWildcardType(
              irs::ViewCast<irs::byte_type>(std::string_view("foo"))));
  ASSERT_EQ(irs::WildcardType::kTerm,
            irs::ComputeWildcardType(
              irs::ViewCast<irs::byte_type>(std::string_view("\xD0\xE2"))));
  ASSERT_EQ(irs::WildcardType::kTermEscaped,
            irs::ComputeWildcardType(
              irs::ViewCast<irs::byte_type>(std::string_view("\\foo"))));
  ASSERT_EQ(irs::WildcardType::kTermEscaped,
            irs::ComputeWildcardType(
              irs::ViewCast<irs::byte_type>(std::string_view("\\%foo"))));
  ASSERT_EQ(irs::WildcardType::kTerm,
            irs::ComputeWildcardType(
              irs::ViewCast<irs::byte_type>(std::string_view("\foo"))));
  ASSERT_EQ(irs::WildcardType::kPrefix,
            irs::ComputeWildcardType(
              irs::ViewCast<irs::byte_type>(std::string_view("foo%"))));
  ASSERT_EQ(irs::WildcardType::kPrefixEscaped,
            irs::ComputeWildcardType(irs::ViewCast<irs::byte_type>(
              std::string_view("\\\\\\\\\\\\%"))));
  ASSERT_EQ(irs::WildcardType::kTermEscaped,
            irs::ComputeWildcardType(
              irs::ViewCast<irs::byte_type>(std::string_view("\\\\\\\\\\%"))));
  ASSERT_EQ(irs::WildcardType::kPrefix,
            irs::ComputeWildcardType(
              irs::ViewCast<irs::byte_type>(std::string_view("foo%%"))));
  ASSERT_EQ(irs::WildcardType::kPrefix,
            irs::ComputeWildcardType(
              irs::ViewCast<irs::byte_type>(std::string_view("\xD0\xE2\x25"))));
  ASSERT_EQ(irs::WildcardType::kTerm,
            irs::ComputeWildcardType(
              irs::ViewCast<irs::byte_type>(std::string_view("\xD0\x25"))));
  ASSERT_EQ(irs::WildcardType::kPrefix,
            irs::ComputeWildcardType(irs::ViewCast<irs::byte_type>(
              std::string_view("\xD0\xE2\x25\x25"))));
  ASSERT_EQ(irs::WildcardType::kWildcard,
            irs::ComputeWildcardType(irs::ViewCast<irs::byte_type>(
              std::string_view("\x25\xD0\xE2\x25\x25"))));
  ASSERT_EQ(irs::WildcardType::kWildcard,
            irs::ComputeWildcardType(
              irs::ViewCast<irs::byte_type>(std::string_view("foo%_"))));
  ASSERT_EQ(irs::WildcardType::kWildcard,
            irs::ComputeWildcardType(
              irs::ViewCast<irs::byte_type>(std::string_view("foo%\\"))));
  ASSERT_EQ(irs::WildcardType::kWildcard,
            irs::ComputeWildcardType(
              irs::ViewCast<irs::byte_type>(std::string_view("fo%o\\%"))));
  ASSERT_EQ(irs::WildcardType::kWildcard,
            irs::ComputeWildcardType(
              irs::ViewCast<irs::byte_type>(std::string_view("foo_%"))));
  ASSERT_EQ(irs::WildcardType::kPrefixEscaped,
            irs::ComputeWildcardType(
              irs::ViewCast<irs::byte_type>(std::string_view("foo\\_%"))));
  ASSERT_EQ(irs::WildcardType::kWildcard,
            irs::ComputeWildcardType(
              irs::ViewCast<irs::byte_type>(std::string_view("foo__"))));
  ASSERT_EQ(irs::WildcardType::kPrefixEscaped,
            irs::ComputeWildcardType(
              irs::ViewCast<irs::byte_type>(std::string_view("foo\\%%"))));
  ASSERT_EQ(irs::WildcardType::kPrefixEscaped,
            irs::ComputeWildcardType(
              irs::ViewCast<irs::byte_type>(std::string_view("foo\\%%%"))));
  ASSERT_EQ(irs::WildcardType::kTermEscaped,
            irs::ComputeWildcardType(
              irs::ViewCast<irs::byte_type>(std::string_view("foo\\%\\%"))));
  ASSERT_EQ(irs::WildcardType::kPrefix,
            irs::ComputeWildcardType(
              irs::ViewCast<irs::byte_type>(std::string_view("%"))));
  ASSERT_EQ(irs::WildcardType::kPrefix,
            irs::ComputeWildcardType(
              irs::ViewCast<irs::byte_type>(std::string_view("%%"))));
  ASSERT_EQ(irs::WildcardType::kWildcard,
            irs::ComputeWildcardType(
              irs::ViewCast<irs::byte_type>(std::string_view("%c%"))));
  ASSERT_EQ(irs::WildcardType::kWildcard,
            irs::ComputeWildcardType(
              irs::ViewCast<irs::byte_type>(std::string_view("%%c%"))));
  ASSERT_EQ(irs::WildcardType::kWildcard,
            irs::ComputeWildcardType(
              irs::ViewCast<irs::byte_type>(std::string_view("%c%%"))));
}

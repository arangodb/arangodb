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

#include "tests_shared.hpp"

#include "utils/automaton_utils.hpp"
#include "utils/wildcard_utils.hpp"

// -----------------------------------------------------------------------------
// --SECTION--                                           wildcard_automaton_test
// -----------------------------------------------------------------------------

class wildcard_utils_test : public test_base {
 protected:
  static void assert_properties(const irs::automaton& a) {
    constexpr auto EXPECTED_PROPERTIES =
      fst::kILabelSorted | fst::kOLabelSorted |
      fst::kIDeterministic | fst::kODeterministic |
      fst::kAcceptor | fst::kUnweighted;

    ASSERT_EQ(EXPECTED_PROPERTIES, a.Properties(EXPECTED_PROPERTIES, true));
  }
};

TEST_F(wildcard_utils_test, match_wildcard) {
  // check automaton structure
  {
    auto lhs = irs::from_wildcard("%b%");
    auto rhs = irs::from_wildcard("%b%%%");
    ASSERT_EQ(lhs.NumStates(), rhs.NumStates());
    assert_properties(lhs);
    assert_properties(rhs);

    for (decltype(lhs)::StateId state = 0; state < lhs.NumStates(); ++state) {
      ASSERT_EQ(lhs.NumArcs(state), rhs.NumArcs(state));
    }
  }

  // check automaton structure
  {
    auto lhs = irs::from_wildcard("b%%%%%s");
    auto rhs = irs::from_wildcard("b%%%s");
    ASSERT_EQ(lhs.NumStates(), rhs.NumStates());
    assert_properties(lhs);
    assert_properties(rhs);

    for (decltype(lhs)::StateId state = 0; state < lhs.NumStates(); ++state) {
      ASSERT_EQ(lhs.NumArcs(state), rhs.NumArcs(state));
    }
  }

  // check automaton structure
  {
    auto lhs = irs::from_wildcard("b%%__%%%s%");
    auto rhs = irs::from_wildcard("b%%%%%%%__%%%%%%%%s%");
    ASSERT_EQ(lhs.NumStates(), rhs.NumStates());
    assert_properties(lhs);
    assert_properties(rhs);

    for (decltype(lhs)::StateId state = 0; state < lhs.NumStates(); ++state) {
      ASSERT_EQ(lhs.NumArcs(state), rhs.NumArcs(state));
    }
  }

  // nil string
  {
    auto a = irs::from_wildcard(irs::string_ref::NIL);
    assert_properties(a);
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref(""))));
    ASSERT_TRUE(irs::accept<char>(a, irs::string_ref::NIL));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("a"))));
  }

  // empty string
  {
    auto a = irs::from_wildcard(irs::string_ref::EMPTY);
    assert_properties(a);
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref(""))));
    ASSERT_TRUE(irs::accept<char>(a, irs::string_ref::NIL));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("a"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xE2\x9E\x96"))));
  }

  // any or empty string
  {
    auto a = irs::from_wildcard("%");
    assert_properties(a);
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref(""))));
    ASSERT_TRUE(irs::accept<char>(a, irs::string_ref::NIL));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("a"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("abc"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xD0\xBF"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xE2\x9E\x96"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xF0\x9F\x98\x81"))));
  }

  // any or empty string
  {
    auto a = irs::from_wildcard("%%");
    assert_properties(a);
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref(""))));
    ASSERT_TRUE(irs::accept<char>(a, irs::string_ref::NIL));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("a"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("aa"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce1d"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce1d1"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce11d"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xE2\x9E\x96"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xF0\x9F\x98\x81"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("a\xF0\x9F\x98\x81"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xF0\x9F\x98\x81\xF0\x9F\x98\x81"))));
  }

  // any char
  {
    auto a = irs::from_wildcard("_");
    assert_properties(a);
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref(""))));
    ASSERT_FALSE(irs::accept<char>(a, irs::string_ref::NIL));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("a"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("abc"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xD0\xBF"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xE2\x9E\x96"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xF0\x9F\x98\x81"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("a\xF0\x9F\x98\x81"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xF0\x9F\x98\x81\xF0\x9F\x98\x81"))));
  }

  // two any chars
  {
    auto a = irs::from_wildcard("__");
    assert_properties(a);
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref(""))));
    ASSERT_FALSE(irs::accept<char>(a, irs::string_ref::NIL));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("a"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xE2\x9E\x96"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("a\xE2\x9E\x96\xD0\xBF"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xE2\x9E\x96\xD0\xBF"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xE2\x9E\x96\xE2\x9E\x96"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xF0\x9F\x98\x81\xF0\x9F\x98\x81"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("a\xF0\x9F\x98\x81\xF0\x9F\x98\x81"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("ba"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce1d"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce1d1"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce11d"))));
  }

  // any char (suffix)
  {
    auto a = irs::from_wildcard("a_");
    assert_properties(a);
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref(""))));
    ASSERT_FALSE(irs::accept<char>(a, irs::string_ref::NIL));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("a_"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("a"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("ab"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("a\xF0\x9F\x98\x81\xF0\x9F\x98\x81"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("a\xF0\x9F\x98\x81"))));
  }

  // any char (prefix)
  {
    auto a = irs::from_wildcard("_a");
    assert_properties(a);
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref(""))));
    ASSERT_FALSE(irs::accept<char>(a, irs::string_ref::NIL));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("_a"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("a"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("aa"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("ba"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xF0\x9F\x98\x81\x61"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xE2\x9E\x96\x61"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xE2\xFF\xFF\x61")))); // accept invalid utf8 sequence
  }

  // escaped '_'
  {
    auto a = irs::from_wildcard("\\_a");
    assert_properties(a);
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref(""))));
    ASSERT_FALSE(irs::accept<char>(a, irs::string_ref::NIL));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("_a"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("a"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("ba"))));
  }

  // escaped '\'
  {
    auto a = irs::from_wildcard("\\\\\\_a");
    assert_properties(a);
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref(""))));
    ASSERT_FALSE(irs::accept<char>(a, irs::string_ref::NIL));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\\_a"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("a"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\\_\xE2\x9E\x96"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("ba"))));
  }

  // nonterminated '\'
  {
    auto a = irs::from_wildcard("a\\");
    assert_properties(a);
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref(""))));
    ASSERT_FALSE(irs::accept<char>(a, irs::string_ref::NIL));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("a\\"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("a"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("ba"))));
  }

  // escaped '%'
  {
    auto a = irs::from_wildcard("\\\\\\%a");
    assert_properties(a);
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref(""))));
    ASSERT_FALSE(irs::accept<char>(a, irs::string_ref::NIL));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\\%a"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("a"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("ba"))));
  }

  // prefix
  {
    auto a = irs::from_wildcard("foo%");
    assert_properties(a);
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref(""))));
    ASSERT_FALSE(irs::accept<char>(a, irs::string_ref::NIL));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("foo"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("foobar"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("foa"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("foabar"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("foo\xE2\x9E\x96\xE2\x9E\x96"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("foo\xF0\x9F\x98\x81\xE2\x9E\x96\xE2\x9E\x96"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("foo\xD0\xBF\xF0\x9F\x98\x81\xE2\x9E\x96\xE2\x9E\x96"))));
  }

  // mixed
  {
    auto a = irs::from_wildcard("a%foo");
    assert_properties(a);
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref(""))));
    ASSERT_FALSE(irs::accept<char>(a, irs::string_ref::NIL));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("affoo"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("aaafofoo"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("aaafafoo"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("aaafaffoo"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("aaafoofoo"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("aaafooffffoo"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("aaafooofoo"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("abcdfo"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("abcdfo"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("aaaaaaaaaaaaaaaaaafoo"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("aaaaaaaaaaaaaaabfoo"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("aaaaaaaaaaaaa\x66\x6F\x6F"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("aaaaaaaaaaaaa\x66\x6F\x6F"))));
  }

  // mixed
  {
    auto a = irs::from_wildcard("a%foo%boo");
    assert_properties(a);
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref(""))));
    ASSERT_FALSE(irs::accept<char>(a, irs::string_ref::NIL));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("afooboo"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("afoofoobooboo"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("afoofooboofooboo"))));
  }

  // suffix
  {
    auto a = irs::from_wildcard("%foo");
    assert_properties(a);
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref(""))));
    ASSERT_FALSE(irs::accept<char>(a, irs::string_ref::NIL));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("foo"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("fofoo"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("foofoo"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("fooofoo"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("ffoo"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("fffoo"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("bfoo"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("foa"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("bfoa"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xE2\x9E\x96\xE2\x9E\x96\x66\x6F\x6F"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xE2\x9E\x96\xE2\x9E\x96\x66\x66\x6F\x6F"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xF0\x9F\x98\x81\xE2\x9E\x96\xE2\x9E\x96\x66\x6F\x6F"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xD0\xBF\xF0\x9F\x98\x81\xE2\x9E\x96\xE2\x9E\x96\x66\x6F\x6F"))));
  }

  // prefix
  {
    auto a = irs::from_wildcard("v%%");
    assert_properties(a);
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref(""))));
    ASSERT_FALSE(irs::accept<char>(a, irs::string_ref::NIL));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("vcc"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("vccc"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("vczc"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("vczczvccccc"))));
  }

  // suffix
  {
    auto a = irs::from_wildcard("%ffoo");
    assert_properties(a);
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref(""))));
    ASSERT_FALSE(irs::accept<char>(a, irs::string_ref::NIL));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("ffoo"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("fffoo"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("bffoo"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("ffob"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("bfoa"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xE2\x9E\x96\xE2\x9E\x96\x66\x66\x6F\x6F"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xF0\x9F\x98\x81\xE2\x9E\x96\xE2\x9E\x96\x66\x66\x6F\x6F"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xD0\xBF\xF0\x9F\x98\x81\xE2\x9E\x96\xE2\x9E\x96\x66\x66\x6F\x6F"))));
  }

  // mixed
  {
    auto a = irs::from_wildcard("a%a");
    assert_properties(a);
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref(""))));
    ASSERT_FALSE(irs::accept<char>(a, irs::string_ref::NIL));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("aa"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("aaa"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("abcdfsa"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("abcdfsa"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("aaaaaaaaaaaaaaaaaa"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("aaaaaaaaaaaaaaab"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("aaaaaaaaaaaaa\xE2\x9E\x96\x61"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("aaaaaaaaaaaaa\xE2\x9E\x61"))));
  }

  // mixed
  {
    auto a = irs::from_wildcard("_%a_%_a_%");
    assert_properties(a);
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref(""))));
    ASSERT_FALSE(irs::accept<char>(a, irs::string_ref::NIL));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("baaaab"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("aaaaaaaaaaaaaaaaaa"))));
  }

  // mixed
  {
    auto a = irs::from_wildcard("\x5F\x25\xE2\x9E\x61\x5F\x25\x5F\xE2\x9E\x61\x5F\x25");
    assert_properties(a);
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref(""))));
    ASSERT_FALSE(irs::accept<char>(a, irs::string_ref::NIL));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\x98\xE2\x9E\x61\x97\x97\xE2\x9E\x61\x98"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xE2\x9E\x61\xE2\x9E\x61\xE2\x9E\x61\xE2\x9E\x61\xE2\x9E\x61\xE2\x9E\x61\xE2\x9E\x61\xE2\x9E\x61"))));
  }

  // mixed
  {
    auto a = irs::from_wildcard("\xE2\x9E\x61\x25\xE2\x9E\x61");
    assert_properties(a);
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref(""))));
    ASSERT_FALSE(irs::accept<char>(a, irs::string_ref::NIL));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xE2\x9E\x61\xE2\x9E\x61"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xE2\x9E\x61\x61\xE2\x9E\x61"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xE2\x9E\x61\x9E\x61\xE2\x9E\x61"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xE2\x9E\x61\x9E\x61\xE2\x9E\xE2\x9E\x61"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xE2\x9E\x61\xE2\x9E\x61\xE2\x9E\x61\xE2\x9E\x61\xE2\x9E\x61\xE2\x9E\x61\xE2\x9E\x61"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xE2\x9E\x61\xE2\x9E\x61\xE2\x9E\x61\xE2\x9E\x61\xE2\x9E\x61\xE2\x9E\x61\xE2\x9E\x61\x61"))));
  }

  // mixed
  {
    auto a = irs::from_wildcard("a%bce_d");
    assert_properties(a);
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref(""))));
    ASSERT_FALSE(irs::accept<char>(a, irs::string_ref::NIL));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce1d"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce1d1"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce11d"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce\xD0\xBF\x64"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce\xE2\x9E\x96\x64"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce\xF0\x9F\x98\x81\x64"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce\xE2\x9E\x96\xF0\x9F\x98\x81\x64"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce\xD0\xBF\xD0\xBF\x64"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("az\xD0\xBF\xD0\xBF\x62\x63\x65\xD0\xBF\x64"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("az\xD0\xBF\xD0\xBF\x62\x63\x65\xD0\xBF\x64\x64"))));
  }

  // mixed
  {
    auto a = irs::from_wildcard("b%d%a");
    assert_properties(a);
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref(""))));
    ASSERT_FALSE(irs::accept<char>(a, irs::string_ref::NIL));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce1d"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce1d1"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce11d"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\x62\x61\x7A\xD0\xBF\xD0\xBF\x62\x63\x64\xD0\xBF\x64\x64\x61"))));
  }

  // mixed
  {
    auto a = irs::from_wildcard("a%b%d");
    assert_properties(a);
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref(""))));
    ASSERT_FALSE(irs::accept<char>(a, irs::string_ref::NIL));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce1d"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce1d1"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce11d"))));
  }

  // mixed
  {
    auto a = irs::from_wildcard("a%b%db");
    assert_properties(a);
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref(""))));
    ASSERT_FALSE(irs::accept<char>(a, irs::string_ref::NIL));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce1d"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce1db"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce1d1"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce11db"))));
  }

  // mixed
  {
    auto a = irs::from_wildcard("%_");
    assert_properties(a);
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref(""))));
    ASSERT_FALSE(irs::accept<char>(a, irs::string_ref::NIL));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("a"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("aa"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce1d"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce1d1"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce11d"))));
  }

  // mixed, terminal "\\"
  {
    auto a = irs::from_wildcard("%\\\\");
    assert_properties(a);
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref(""))));
    ASSERT_FALSE(irs::accept<char>(a, irs::string_ref::NIL));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\\"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("a\\"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("aa\\"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce1\\"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce1\\1"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("1azbce11\\"))));
  }

  // mixed, terminal "\\"
  {
    auto a = irs::from_wildcard("%_\\\\");
    assert_properties(a);
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref(""))));
    ASSERT_FALSE(irs::accept<char>(a, irs::string_ref::NIL));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\\"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("a\\"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("aa\\"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce1\\"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce1\\1"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("1azbce11\\"))));
  }

  // mixed, non-terminated "\\"
  {
    auto a = irs::from_wildcard("%\\");
    assert_properties(a);
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref(""))));
    ASSERT_FALSE(irs::accept<char>(a, irs::string_ref::NIL));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\\"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("a\\"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("aa\\"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce1\\"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce1\\1"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("1azbce11\\"))));
  }

  // mixed, non-terminated "\\"
  {
    auto a = irs::from_wildcard("%_\\");
    assert_properties(a);
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref(""))));
    ASSERT_FALSE(irs::accept<char>(a, irs::string_ref::NIL));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\\"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("a\\"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("aa\\"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce1\\"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce1\\1"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("1azbce11\\"))));
  }

  // mixed
  {
    auto a = irs::from_wildcard("%_d");
    assert_properties(a);
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref(""))));
    ASSERT_FALSE(irs::accept<char>(a, irs::string_ref::NIL));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("d"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("ad"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("aad"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce1d"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce1d1"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("1azbce11d"))));
  }

  // mixed
  {
    auto a = irs::from_wildcard("%_%_%d");
    assert_properties(a);
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref(""))));
    ASSERT_FALSE(irs::accept<char>(a, irs::string_ref::NIL));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("ad"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("add"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("add1"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("abd"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("ddd"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("aad"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce1d"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce1d1"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("1azbce11d"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xE2\x9E\x96\x64"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xE2\x9E\x96\x64\x64\x64"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("a\xE2\x9E\x96\x64"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("e\xF0\x9F\x98\x81\x64"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("xE2\x9E\x96\xF0\x9F\x98\x81\x64"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("xD0\xBF\xD0\xBF\x64"))));
  }

  // mixed
  {
    auto a = irs::from_wildcard("%_%_%d%");
    assert_properties(a);
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref(""))));
    ASSERT_FALSE(irs::accept<char>(a, irs::string_ref::NIL));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("ad"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("add"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("add1"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("abd"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("ddd"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("aad"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce1d"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce1d1"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("1azbce11d"))));
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xE2\x9E\x96\x64"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xE2\x9E\x96\x64\x64"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce\xE2\x9E\x96\x64"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce\xF0\x9F\x98\x81\x64"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce\xE2\x9E\x96\xF0\x9F\x98\x81\x64\xD0\xBF"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce\xD0\xBF\xD0\xBF\x64\xD0\xBF"))));
  }

  // mixed
  {
    auto a = irs::from_wildcard("%%_");
    assert_properties(a);
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref(""))));
    ASSERT_FALSE(irs::accept<char>(a, irs::string_ref::NIL));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("a"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("aa"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce1d"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce1d1"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce11d"))));
  }

  // mixed
  {
    auto a = irs::from_wildcard("_%");
    assert_properties(a);
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref(""))));
    ASSERT_FALSE(irs::accept<char>(a, irs::string_ref::NIL));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("a"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("aa"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce1d"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce1d1"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("azbce11d"))));
  }

  // mixed
  {
    auto a = irs::from_wildcard("v%%c");
    assert_properties(a);
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref(""))));
    ASSERT_FALSE(irs::accept<char>(a, irs::string_ref::NIL));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("vcc"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("vccc"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("vczc"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("vczczvccccc"))));
  }

  // mixed
  {
    auto a = irs::from_wildcard("v%c");
    assert_properties(a);
    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref(""))));
    ASSERT_FALSE(irs::accept<char>(a, irs::string_ref::NIL));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("vcc"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("vccc"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("vczc"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("vczczvccccc"))));
  }

  // invalid UTF-8 sequence
  ASSERT_EQ(0, irs::from_wildcard("\xD0").NumStates());
  ASSERT_EQ(0, irs::from_wildcard("\xE2\x9E").NumStates());
  ASSERT_EQ(0, irs::from_wildcard("\xF0\x9F\x98").NumStates());
}

TEST_F(wildcard_utils_test, wildcard_type) {
  ASSERT_EQ(irs::WildcardType::INVALID, irs::wildcard_type(irs::ref_cast<irs::byte_type>(irs::string_ref("\xD0")))); // invalid UTF-8 sequence
  ASSERT_EQ(irs::WildcardType::TERM, irs::wildcard_type(irs::ref_cast<irs::byte_type>(irs::string_ref("foo"))));
  ASSERT_EQ(irs::WildcardType::TERM, irs::wildcard_type(irs::ref_cast<irs::byte_type>(irs::string_ref("\xD0\xE2"))));
  ASSERT_EQ(irs::WildcardType::TERM, irs::wildcard_type(irs::ref_cast<irs::byte_type>(irs::string_ref("\\foo"))));
  ASSERT_EQ(irs::WildcardType::TERM, irs::wildcard_type(irs::ref_cast<irs::byte_type>(irs::string_ref("\\%foo"))));
  ASSERT_EQ(irs::WildcardType::TERM, irs::wildcard_type(irs::ref_cast<irs::byte_type>(irs::string_ref("\foo"))));
  ASSERT_EQ(irs::WildcardType::PREFIX, irs::wildcard_type(irs::ref_cast<irs::byte_type>(irs::string_ref("foo%"))));
  ASSERT_EQ(irs::WildcardType::PREFIX, irs::wildcard_type(irs::ref_cast<irs::byte_type>(irs::string_ref("foo%%"))));
  ASSERT_EQ(irs::WildcardType::PREFIX, irs::wildcard_type(irs::ref_cast<irs::byte_type>(irs::string_ref("\xD0\xE2\x25"))));
  ASSERT_EQ(irs::WildcardType::TERM, irs::wildcard_type(irs::ref_cast<irs::byte_type>(irs::string_ref("\xD0\x25"))));
  ASSERT_EQ(irs::WildcardType::PREFIX, irs::wildcard_type(irs::ref_cast<irs::byte_type>(irs::string_ref("\xD0\xE2\x25\x25"))));
  ASSERT_EQ(irs::WildcardType::WILDCARD, irs::wildcard_type(irs::ref_cast<irs::byte_type>(irs::string_ref("\x25\xD0\xE2\x25\x25"))));
  ASSERT_EQ(irs::WildcardType::WILDCARD, irs::wildcard_type(irs::ref_cast<irs::byte_type>(irs::string_ref("foo%_"))));
  ASSERT_EQ(irs::WildcardType::WILDCARD, irs::wildcard_type(irs::ref_cast<irs::byte_type>(irs::string_ref("foo%\\"))));
  ASSERT_EQ(irs::WildcardType::WILDCARD, irs::wildcard_type(irs::ref_cast<irs::byte_type>(irs::string_ref("fo%o\\%"))));
  ASSERT_EQ(irs::WildcardType::WILDCARD, irs::wildcard_type(irs::ref_cast<irs::byte_type>(irs::string_ref("foo_%"))));
  ASSERT_EQ(irs::WildcardType::PREFIX, irs::wildcard_type(irs::ref_cast<irs::byte_type>(irs::string_ref("foo\\_%"))));
  ASSERT_EQ(irs::WildcardType::WILDCARD, irs::wildcard_type(irs::ref_cast<irs::byte_type>(irs::string_ref("foo__"))));
  ASSERT_EQ(irs::WildcardType::PREFIX, irs::wildcard_type(irs::ref_cast<irs::byte_type>(irs::string_ref("foo\\%%"))));
  ASSERT_EQ(irs::WildcardType::PREFIX, irs::wildcard_type(irs::ref_cast<irs::byte_type>(irs::string_ref("foo\\%%%"))));
  ASSERT_EQ(irs::WildcardType::TERM, irs::wildcard_type(irs::ref_cast<irs::byte_type>(irs::string_ref("foo\\%\\%"))));
  ASSERT_EQ(irs::WildcardType::MATCH_ALL, irs::wildcard_type(irs::ref_cast<irs::byte_type>(irs::string_ref("%"))));
  ASSERT_EQ(irs::WildcardType::MATCH_ALL, irs::wildcard_type(irs::ref_cast<irs::byte_type>(irs::string_ref("%%"))));
  ASSERT_EQ(irs::WildcardType::WILDCARD, irs::wildcard_type(irs::ref_cast<irs::byte_type>(irs::string_ref("%c%"))));
  ASSERT_EQ(irs::WildcardType::WILDCARD, irs::wildcard_type(irs::ref_cast<irs::byte_type>(irs::string_ref("%%c%"))));
  ASSERT_EQ(irs::WildcardType::WILDCARD, irs::wildcard_type(irs::ref_cast<irs::byte_type>(irs::string_ref("%c%%"))));
}

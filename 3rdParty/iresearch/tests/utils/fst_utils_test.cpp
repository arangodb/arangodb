////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
#include "utils/automaton.hpp"
#include "utils/fstext/fst_table_matcher.hpp"

TEST(fst_table_matcher_test, static_const) {
  static_assert(fst::MATCH_INPUT == fst::TableMatcher<fst::fsa::Automaton<>>::MATCH_TYPE, "assertion failed");
  static_assert(fst::kNoIEpsilons | fst::kIDeterministic | fst::kAcceptor == fst::TableMatcher<fst::fsa::Automaton<>>::MATCH_TYPE, "assertion failed");
  static_assert(fst::MATCH_OUTPUT == fst::TableMatcher<fst::fsa::Automaton<>, 256, false>::MATCH_TYPE, "assertion failed");
  static_assert(fst::kNoOEpsilons | fst::kODeterministic | fst::kAcceptor == fst::TableMatcher<fst::fsa::Automaton<>, 256, false>::MATCH_TYPE, "assertion failed");
  static_assert(std::is_same<fst::fsa::Automaton<>, fst::TableMatcher<fst::fsa::Automaton<>>::FST>::value, "assertion failed");
  static_assert(std::is_same<fst::fsa::Transition<>,fst::TableMatcher<fst::fsa::Automaton<>>::Arc>::value, "assertion failed");
  static_assert(std::is_same<int32_t,fst::TableMatcher<fst::fsa::Automaton<>>::Label>::value, "assertion failed");
  static_assert(std::is_same<int32_t,fst::TableMatcher<fst::fsa::Automaton<>>::StateId>::value, "assertion failed");
  static_assert(std::is_same<fst::fsa::BooleanWeight,fst::TableMatcher<fst::fsa::Automaton<>>::Weight>::value, "assertion failed");
}

TEST(fst_table_matcher_test, test_properties) {
  // non-deterministic
  {
    fst::fsa::Automaton<> a;
    a.AddState(); // 0
    a.AddState(); // 1
    a.EmplaceArc(1, 1, 0);
    a.EmplaceArc(1, 1, 0);
    fst::TableMatcher<fst::fsa::Automaton<>> matcher(a, fst::kNoLabel);
    ASSERT_EQ(fst::kError, matcher.Properties(0));
  }

  // acceptor, regardless of specified arc weights
  {
    fst::fsa::Automaton<> a;
    a.AddState(); // 0
    a.AddState(); // 1
    a.EmplaceArc(1, 1, 0);
    a.EmplaceArc(1, 2, 0);
    fst::ArcIteratorData<fst::fsa::Transition<>> data;
    a.InitArcIterator(1, &data);
    const_cast<fst::fsa::Transition<>&>(data.arcs[0]).olabel = fst::kNoLabel;

    fst::TableMatcher<fst::fsa::Automaton<>> matcher(a, fst::kNoLabel);
    ASSERT_NE(fst::kError, matcher.Properties(0));
  }
}

TEST(fst_table_matcher_test, test_matcher) {
  // create matcher with an empty automaton
  {
    fst::fsa::Automaton<> a;
    fst::TableMatcher<fst::fsa::Automaton<>> matcher(a, fst::kNoLabel);
    ASSERT_NE(fst::kError, matcher.Properties(0));
    ASSERT_TRUE(matcher.Done());
    matcher.Next();
    ASSERT_TRUE(matcher.Done());
    ASSERT_EQ(&a, &matcher.GetFst());
    ASSERT_EQ(fst::MATCH_INPUT, matcher.Type(false));
    ASSERT_EQ(fst::MATCH_INPUT, matcher.Type(true));

    std::unique_ptr<fst::TableMatcher<fst::fsa::Automaton<>>> copy(matcher.Copy(false));
    ASSERT_NE(nullptr, copy);
    ASSERT_NE(fst::kError, copy->Properties(0));
    ASSERT_TRUE(copy->Done());
    copy->Next();
    ASSERT_TRUE(copy->Done());
    ASSERT_EQ(&a, &copy->GetFst());
    ASSERT_EQ(fst::MATCH_INPUT, copy->Type(false));
    ASSERT_EQ(fst::MATCH_INPUT, copy->Type(true));
  }

  // create matcher with non-empty automaton
  {
    fst::fsa::Automaton<> a;
    a.SetFinal(a.AddState());
    a.AddState();
    a.EmplaceArc(1, 42, 0);

    fst::TableMatcher<fst::fsa::Automaton<>> matcher(a, fst::kNoLabel);
    ASSERT_NE(fst::kError, matcher.Properties(0));
    ASSERT_TRUE(matcher.Done());
    matcher.Next();
    ASSERT_TRUE(matcher.Done());
    ASSERT_EQ(fst::fsa::BooleanWeight(true), matcher.Final(0));
    ASSERT_EQ(fst::fsa::BooleanWeight(false), matcher.Final(1));
    ASSERT_EQ(0, matcher.Priority(0));
    ASSERT_EQ(1, matcher.Priority(1));
    ASSERT_EQ(&a, &matcher.GetFst());
    ASSERT_EQ(fst::MATCH_INPUT, matcher.Type(false));
    ASSERT_EQ(fst::MATCH_INPUT, matcher.Type(true));
    matcher.SetState(0);
    ASSERT_FALSE(matcher.Find(42));
    ASSERT_FALSE(matcher.Done());
    matcher.Next();
    ASSERT_TRUE(matcher.Done());
    matcher.SetState(1);
    ASSERT_TRUE(matcher.Find(42));
    ASSERT_EQ(0, matcher.Value().nextstate);
    matcher.Next();
    ASSERT_TRUE(matcher.Done());
    matcher.Next();
    ASSERT_TRUE(matcher.Done());

    std::unique_ptr<fst::TableMatcher<fst::fsa::Automaton<>>> copy(matcher.Copy(false));
    ASSERT_NE(nullptr, copy);
    ASSERT_NE(fst::kError, copy->Properties(0));
    ASSERT_TRUE(copy->Done());
    copy->Next();
    ASSERT_TRUE(copy->Done());
    ASSERT_EQ(fst::fsa::BooleanWeight(true), copy->Final(0));
    ASSERT_EQ(fst::fsa::BooleanWeight(false), copy->Final(1));
    ASSERT_EQ(0, copy->Priority(0));
    ASSERT_EQ(1, copy->Priority(1));
    ASSERT_EQ(&a, &copy->GetFst());
    ASSERT_EQ(fst::MATCH_INPUT, copy->Type(false));
    ASSERT_EQ(fst::MATCH_INPUT, copy->Type(true));

    copy->SetState(0);
    ASSERT_FALSE(copy->Find(42));
    ASSERT_FALSE(copy->Done());
    copy->Next();
    ASSERT_TRUE(copy->Done());
    copy->Next();
    ASSERT_TRUE(copy->Done());
    copy->SetState(1);
    ASSERT_TRUE(copy->Find(42));
    ASSERT_EQ(0, copy->Value().nextstate);
    copy->Next();
    ASSERT_TRUE(copy->Done());
    copy->Next();
    ASSERT_TRUE(copy->Done());
  }

  // create matcher with non-empty automaton with rho transitions
  {
    fst::fsa::Automaton<> a;
    a.SetFinal(a.AddState()); // 0
    a.AddState(); // 1
    a.EmplaceArc(1, 42, 0);
    a.EmplaceArc(1, fst::fsa::kRho, 0);

    fst::TableMatcher<fst::fsa::Automaton<>> matcher(a, fst::fsa::kRho);
    ASSERT_NE(fst::kError, matcher.Properties(0));
    ASSERT_TRUE(matcher.Done());
    matcher.Next();
    ASSERT_TRUE(matcher.Done());
    ASSERT_EQ(fst::fsa::BooleanWeight(true), matcher.Final(0));
    ASSERT_EQ(fst::fsa::BooleanWeight(false), matcher.Final(1));
    ASSERT_EQ(0, matcher.Priority(0));
    ASSERT_EQ(2, matcher.Priority(1));
    ASSERT_EQ(&a, &matcher.GetFst());
    ASSERT_EQ(fst::MATCH_INPUT, matcher.Type(false));
    ASSERT_EQ(fst::MATCH_INPUT, matcher.Type(true));

    matcher.SetState(0);
    ASSERT_FALSE(matcher.Find(42));
    ASSERT_FALSE(matcher.Done());
    matcher.Next();
    ASSERT_TRUE(matcher.Done());
    matcher.SetState(1);
    ASSERT_TRUE(matcher.Find(42));
    ASSERT_EQ(0, matcher.Value().nextstate);
    matcher.Next();
    ASSERT_EQ(0, matcher.Value().nextstate); // rho transition
    matcher.Next();
    ASSERT_TRUE(matcher.Done());
    matcher.Next();
    ASSERT_TRUE(matcher.Done());
    matcher.SetState(1);
    ASSERT_TRUE(matcher.Find(43));
    ASSERT_EQ(0, matcher.Value().nextstate); // rho transition
    matcher.Next();
    ASSERT_TRUE(matcher.Done());
    matcher.Next();
    ASSERT_TRUE(matcher.Done());


    std::unique_ptr<fst::TableMatcher<fst::fsa::Automaton<>>> copy(matcher.Copy(false));
    ASSERT_NE(nullptr, copy);
    ASSERT_NE(fst::kError, copy->Properties(0));
    ASSERT_TRUE(copy->Done());
    copy->Next();
    ASSERT_TRUE(copy->Done());
    ASSERT_EQ(fst::fsa::BooleanWeight(true), copy->Final(0));
    ASSERT_EQ(fst::fsa::BooleanWeight(false), copy->Final(1));
    ASSERT_EQ(0, copy->Priority(0));
    ASSERT_EQ(2, copy->Priority(1));
    ASSERT_EQ(&a, &copy->GetFst());
    ASSERT_EQ(fst::MATCH_INPUT, copy->Type(false));
    ASSERT_EQ(fst::MATCH_INPUT, copy->Type(true));

    copy->SetState(0);
    ASSERT_FALSE(copy->Find(42));
    ASSERT_FALSE(copy->Done());
    copy->Next();
    ASSERT_TRUE(copy->Done());
    copy->SetState(1);
    ASSERT_TRUE(copy->Find(42));
    ASSERT_EQ(0, copy->Value().nextstate);
    copy->Next();
    ASSERT_EQ(0, copy->Value().nextstate); // rho transition
    copy->Next();
    ASSERT_TRUE(copy->Done());
    copy->Next();
    ASSERT_TRUE(copy->Done());
    copy->SetState(1);
    ASSERT_TRUE(copy->Find(43));
    ASSERT_EQ(0, copy->Value().nextstate); // rho transition
    copy->Next();
    ASSERT_TRUE(copy->Done());
    copy->Next();
    ASSERT_TRUE(copy->Done());
  }

  // complex automaton
  {
    fst::fsa::Automaton<> a;

    // build automaton
    {
      auto from = a.AddState();
      auto add_state = [&a, &from](fst::fsa::Automaton<>::Arc::Label min,
                                  fst::fsa::Automaton<>::Arc::Label max,
                                  int step) mutable {
        auto to = a.AddState();

        for (; min < max; min += step) {
          a.EmplaceArc(from, min, to);
        }

        from = to;
      };

      a.SetStart(from);        // state: 0
      add_state(1, 1024, 3);   // state: 1
      add_state(512, 2048, 7); // state: 2
      add_state(152, 512, 11); // state: 3
      add_state(1, 2, 1);      // state: 4
      a.SetFinal(from);        // state: 5
    }

    // check automaton
    {
      using matcher_t = fst::TableMatcher<fst::fsa::Automaton<>>;
      using expected_matcher_t = fst::SortedMatcher<fst::fsa::Automaton<>>;

      expected_matcher_t expected_matcher(a, fst::MATCH_INPUT);
      matcher_t matcher(a, fst::fsa::kRho);
      for (fst::fsa::Automaton<>::StateId state = 0; state < a.NumStates(); ++state) {
        expected_matcher.SetState(state);
        matcher.SetState(state);

        for (matcher_t::Label min = 1, max = 2049; min < max; ++min) {
          const auto found = expected_matcher.Find(min);
          ASSERT_EQ(found, matcher.Find(min));
          if (found) {
            ASSERT_EQ(expected_matcher.Value().nextstate, matcher.Value().nextstate);
          }
        }
      }
    }
  }
}

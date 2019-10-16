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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "tests_shared.hpp"
#include "fst/matcher.h"
#include "utils/automaton.hpp"
#include "utils/fst_table_matcher.hpp"

TEST(fst_table_matcher_test, test_matcher) {
  fst::fsa::Automaton a;

  // build automaton
  {
    auto from = a.AddState();
    auto add_state = [&a, &from](fst::fsa::Automaton::Arc::Label min,
                                fst::fsa::Automaton::Arc::Label max,
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
    using matcher_t = fst::TableMatcher<fst::fsa::Automaton, fst::fsa::kRho>;
    using expected_matcher_t = fst::SortedMatcher<fst::fsa::Automaton>;

    expected_matcher_t expected_matcher(a, fst::MATCH_INPUT);
    matcher_t matcher(a, fst::fsa::kRho);
    for (fst::fsa::Automaton::StateId state = 0; state < a.NumStates(); ++state) {
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


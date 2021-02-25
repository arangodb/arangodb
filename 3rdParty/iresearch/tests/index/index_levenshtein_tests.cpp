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

#include <utf8/core.h>

#include "index/index_tests.hpp"
#include "utils/levenshtein_utils.hpp"
#include "utils/automaton_utils.hpp"
#include "utils/fstext/fst_table_matcher.hpp"

class levenshtein_automaton_index_test_case : public tests::index_test_base {
 protected:
  void assert_index(const irs::index_reader& reader,
                    const irs::parametric_description& description,
                    const irs::bytes_ref& target) {
    auto acceptor = irs::make_levenshtein_automaton(description, target);
    irs::automaton_table_matcher matcher(acceptor, fst::fsa::kRho);

    for (auto& segment : reader) {
      auto fields = segment.fields();
      ASSERT_NE(nullptr, fields);

      while (fields->next()) {
        auto expected_terms = fields->value().iterator();
        ASSERT_NE(nullptr, expected_terms);
        auto actual_terms = fields->value().iterator(matcher);
        ASSERT_NE(nullptr, actual_terms);

        auto* payload = irs::get<irs::payload>(*actual_terms);
        ASSERT_NE(nullptr, payload);
        ASSERT_EQ(1, payload->value.size());

        while (expected_terms->next()) {
          auto& expected_term = expected_terms->value();

          auto edit_distance = irs::edit_distance(expected_term, target);
          if (edit_distance > description.max_distance()) {
            continue;
          }

          const auto pos = utf8::find_invalid(expected_term.begin(), expected_term.end());
          if (pos != expected_term.end()) {
            // invalid utf8 sequence
            continue;
          }

          SCOPED_TRACE(testing::Message("Expected term: '")
            << irs::ref_cast<char>(expected_term));

          ASSERT_TRUE(actual_terms->next());
          auto& actual_term = actual_terms->value();
          ASSERT_EQ(expected_term, actual_term);
          ASSERT_EQ(1, payload->value.size());
          ASSERT_EQ(edit_distance, payload->value[0]);
        }
      }
    }
  }
};

TEST_P(levenshtein_automaton_index_test_case, test_lev_automaton) {
  const irs::parametric_description DESCRIPTIONS[] {
    irs::make_parametric_description(1, false),
    irs::make_parametric_description(2, false),
    irs::make_parametric_description(3, false),
  };

  const irs::string_ref TARGETS[] {
    "atlas", "bloom", "burden", "del",
    "survenius", "surbenus", ""
  };

  // add data
  {
    tests::templates::europarl_doc_template doc;
    tests::delim_doc_generator gen(resource("europarl.subset.txt"), doc);
    add_segment(gen);
  }

  auto reader = open_reader();
  ASSERT_NE(nullptr, reader);

  for (auto& description : DESCRIPTIONS) {
    for (auto& target : TARGETS) {
      SCOPED_TRACE(testing::Message("Target: '") << target <<
                   testing::Message("', Edit distance: ") << size_t(description.max_distance()));
      assert_index(reader, description, irs::ref_cast<irs::byte_type>(target));
    }
  }
}

INSTANTIATE_TEST_CASE_P(
  levenshtein_automaton_index_test,
  levenshtein_automaton_index_test_case,
  ::testing::Combine(
    ::testing::Values(
      &tests::memory_directory
    ),
    ::testing::Values(tests::format_info{"1_2", "1_0"})
  ),
  tests::to_string
);

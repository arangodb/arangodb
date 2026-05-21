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

#include "search/scorers.hpp"
#include "tests_shared.hpp"

TEST(scorers_tests, duplicate_register) {
  struct dummy_scorer : public irs::ScorerBase<dummy_scorer, void> {
    irs::IndexFeatures index_features() const final {
      return irs::IndexFeatures::NONE;
    }

    irs::ScoreFunction prepare_scorer(
      const irs::ColumnProvider& /*segment*/,
      const irs::feature_map_t& /*features*/, const irs::byte_type* /*stats*/,
      const irs::attribute_provider& /*doc_attrs*/,
      irs::score_t /*boost*/) const final {
      return {};
    }

    static irs::Scorer::ptr make(std::string_view) {
      return std::make_unique<dummy_scorer>();
    }
  };

  static bool initial_expected = true;

  // check required for tests with repeat (static maps are not cleared between
  // runs)
  if (initial_expected) {
    ASSERT_FALSE(
      irs::scorers::exists(irs::type<dummy_scorer>::name(),
                           irs::type<irs::text_format::vpack>::get()));
    ASSERT_FALSE(
      irs::scorers::exists(irs::type<dummy_scorer>::name(),
                           irs::type<irs::text_format::json>::get()));
    ASSERT_FALSE(
      irs::scorers::exists(irs::type<dummy_scorer>::name(),
                           irs::type<irs::text_format::text>::get()));
    ASSERT_EQ(nullptr,
              irs::scorers::get(irs::type<dummy_scorer>::name(),
                                irs::type<irs::text_format::vpack>::get(),
                                std::string_view{}));
    ASSERT_EQ(nullptr,
              irs::scorers::get(irs::type<dummy_scorer>::name(),
                                irs::type<irs::text_format::json>::get(),
                                std::string_view{}));
    ASSERT_EQ(nullptr,
              irs::scorers::get(irs::type<dummy_scorer>::name(),
                                irs::type<irs::text_format::text>::get(),
                                std::string_view{}));

    irs::scorer_registrar initial0(irs::type<dummy_scorer>::get(),
                                   irs::type<irs::text_format::vpack>::get(),
                                   &dummy_scorer::make);
    irs::scorer_registrar initial1(irs::type<dummy_scorer>::get(),
                                   irs::type<irs::text_format::json>::get(),
                                   &dummy_scorer::make);
    irs::scorer_registrar initial2(irs::type<dummy_scorer>::get(),
                                   irs::type<irs::text_format::text>::get(),
                                   &dummy_scorer::make);

    ASSERT_EQ(!initial_expected, !initial0);
    ASSERT_EQ(!initial_expected, !initial1);
    ASSERT_EQ(!initial_expected, !initial2);
  }

  initial_expected =
    false;  // next test iteration will not be able to register the same scorer
  irs::scorer_registrar duplicate0(irs::type<dummy_scorer>::get(),
                                   irs::type<irs::text_format::vpack>::get(),
                                   &dummy_scorer::make);
  irs::scorer_registrar duplicate1(irs::type<dummy_scorer>::get(),
                                   irs::type<irs::text_format::json>::get(),
                                   &dummy_scorer::make);
  irs::scorer_registrar duplicate2(irs::type<dummy_scorer>::get(),
                                   irs::type<irs::text_format::text>::get(),
                                   &dummy_scorer::make);
  ASSERT_TRUE(!duplicate0);
  ASSERT_TRUE(!duplicate1);
  ASSERT_TRUE(!duplicate2);

  ASSERT_TRUE(irs::scorers::exists(irs::type<dummy_scorer>::name(),
                                   irs::type<irs::text_format::vpack>::get()));
  ASSERT_TRUE(irs::scorers::exists(irs::type<dummy_scorer>::name(),
                                   irs::type<irs::text_format::json>::get()));
  ASSERT_TRUE(irs::scorers::exists(irs::type<dummy_scorer>::name(),
                                   irs::type<irs::text_format::text>::get()));
  ASSERT_NE(nullptr,
            irs::scorers::get(irs::type<dummy_scorer>::name(),
                              irs::type<irs::text_format::vpack>::get(),
                              std::string_view{}));
  ASSERT_NE(nullptr, irs::scorers::get(irs::type<dummy_scorer>::name(),
                                       irs::type<irs::text_format::json>::get(),
                                       std::string_view{}));
  ASSERT_NE(nullptr, irs::scorers::get(irs::type<dummy_scorer>::name(),
                                       irs::type<irs::text_format::text>::get(),
                                       std::string_view{}));
}

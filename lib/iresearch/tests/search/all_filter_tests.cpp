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

#include "filter_test_case_base.hpp"
#include "search/all_filter.hpp"
#include "search/cost.hpp"
#include "search/score.hpp"
#include "tests_shared.hpp"

namespace {

class all_filter_test_case : public tests::FilterTestCaseBase {};

TEST_P(all_filter_test_case, all_sequential) {
  // add segment
  {
    tests::json_doc_generator gen(resource("simple_sequential.json"),
                                  &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();
  ASSERT_NE(nullptr, rdr);
  ASSERT_EQ(1, rdr->size());
  auto& segment = rdr[0];

  Docs docs{1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
            17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32};
  std::vector<irs::cost::cost_t> cost{docs.size()};

  CheckQuery(irs::all(), docs, cost, rdr);

  // check iterator attributes, no order
  MaxMemoryCounter counter;
  auto it = irs::all()
              .prepare({.index = *rdr, .memory = counter})
              ->execute({.segment = segment});
  ASSERT_TRUE(irs::get<irs::document>(*it));
  auto* it_cost = irs::get<irs::cost>(*it);
  ASSERT_TRUE(it_cost);
  ASSERT_EQ(docs.size(), it_cost->estimate());
  auto& score = irs::score::get(*it);
  ASSERT_TRUE(score.Func() == &irs::ScoreFunction::DefaultScore);
  ASSERT_EQ(&score, irs::get_mutable<irs::score>(it.get()));
  it.reset();
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
}

TEST_P(all_filter_test_case, all_order) {
  // add segment
  {
    tests::json_doc_generator gen(resource("simple_sequential.json"),
                                  &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  // empty query
  // CheckQuery(irs::all(), Docs{}, Costs{0}, rdr);

  // no order (same as empty order since no score is calculated)
  {
    Docs docs{1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
              17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32};
    CheckQuery(irs::all(), docs, Costs{docs.size()}, rdr);
  }

  // custom order
  {
    Docs docs{1, 4,  5,  16, 17, 20, 21, 2,  3,  6,  7,  18, 19, 22, 23, 8,
              9, 12, 13, 24, 25, 28, 29, 10, 11, 14, 15, 26, 27, 30, 31, 32};
    size_t collector_collect_field_count = 0;
    size_t collector_collect_term_count = 0;
    size_t collector_finish_count = 0;
    size_t scorer_score_count = 0;

    irs::Scorer::ptr bucket{std::make_unique<tests::sort::custom_sort>()};
    auto* sort = static_cast<tests::sort::custom_sort*>(bucket.get());

    sort->collector_collect_field = [&collector_collect_field_count](
                                      const irs::SubReader&,
                                      const irs::term_reader&) -> void {
      ++collector_collect_field_count;
    };
    sort->collector_collect_term = [&collector_collect_term_count](
                                     const irs::SubReader&,
                                     const irs::term_reader&,
                                     const irs::attribute_provider&) -> void {
      ++collector_collect_term_count;
    };
    sort->collectors_collect_ =
      [&collector_finish_count](
        const irs::byte_type*, const irs::FieldCollector*,
        const irs::TermCollector*) -> void { ++collector_finish_count; };
    sort->scorer_score = [&scorer_score_count](irs::doc_id_t doc,
                                               irs::score_t* score) -> void {
      ++scorer_score_count;
      *score = irs::score_t(doc & 0xAAAAAAAA);
    };

    CheckQuery(irs::all(), std::span{&bucket, 1}, docs, rdr);
    ASSERT_EQ(0, collector_collect_field_count);  // should not be executed
    ASSERT_EQ(0, collector_collect_term_count);   // should not be executed
    ASSERT_EQ(1, collector_finish_count);
    ASSERT_EQ(32, scorer_score_count);
  }

  // custom order (no scorer)
  {
    Docs docs{1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
              17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32};

    irs::Scorer::ptr bucket{std::make_unique<tests::sort::custom_sort>()};
    auto& sort = static_cast<tests::sort::custom_sort&>(*bucket);

    sort.prepare_field_collector_ = []() -> irs::FieldCollector::ptr {
      return nullptr;
    };
    sort.prepare_scorer_ = [](const irs::ColumnProvider&,
                              const irs::feature_map_t&, const irs::byte_type*,
                              const irs::attribute_provider&,
                              irs::score_t) -> irs::ScoreFunction {
      return irs::ScoreFunction::Default(1);
    };
    sort.prepare_term_collector_ = []() -> irs::TermCollector::ptr {
      return nullptr;
    };
    CheckQuery(irs::all(), std::span{&bucket, 1}, docs, rdr, false);
  }

  // frequency order
  {
    Docs docs{1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
              17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32};

    irs::Scorer::ptr sort{std::make_unique<tests::sort::frequency_sort>()};

    CheckQuery(irs::all(), std::span{&sort, 1}, docs, rdr);
  }

  // bm25 order
  {
    Docs docs{1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
              17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32};

    auto sort = irs::scorers::get(
      "bm25", irs::type<irs::text_format::json>::get(), std::string_view{});

    CheckQuery(irs::all(), std::span{&sort, 1}, docs, rdr);
  }

  // tfidf order
  {
    Docs docs{1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
              17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32};

    auto sort = irs::scorers::get(
      "tfidf", irs::type<irs::text_format::json>::get(), std::string_view{});
    CheckQuery(irs::all(), std::span{&sort, 1}, docs, rdr);
  }
}

static constexpr auto kTestDirs = tests::getDirectories<tests::kTypesDefault>();

INSTANTIATE_TEST_SUITE_P(all_filter_test, all_filter_test_case,
                         ::testing::Combine(::testing::ValuesIn(kTestDirs),
                                            ::testing::Values("1_0")),
                         all_filter_test_case::to_string);

}  // namespace

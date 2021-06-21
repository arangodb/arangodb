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
#include "filter_test_case_base.hpp"
#include "search/all_filter.hpp"
#include "search/score.hpp"
#include "search/cost.hpp"

namespace {

class all_filter_test_case : public tests::filter_test_case_base { };

TEST_P(all_filter_test_case, all_sequential) {
  // add segment
  {
    tests::json_doc_generator gen(
       resource("simple_sequential.json"),
       &tests::generic_json_field_factory);
    add_segment( gen );
  }

  auto rdr = open_reader();
  ASSERT_NE(nullptr, rdr);
  ASSERT_EQ(1, rdr->size());
  auto& segment = rdr[0];

  docs_t docs{
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
    17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32
  };
  std::vector<irs::cost::cost_t> cost{ docs.size() };

  check_query(irs::all(), docs, cost, rdr);

  // check iterator attributes, no order
  auto it = irs::all().prepare(*rdr)->execute(segment);
  ASSERT_TRUE(irs::get<irs::document>(*it));
  auto* it_cost = irs::get<irs::cost>(*it);
  ASSERT_TRUE(it_cost);
  ASSERT_EQ(docs.size(), it_cost->estimate());
  auto& score = irs::score::get(*it);
  ASSERT_TRUE(score.is_default());
  ASSERT_EQ(&score, irs::get_mutable<irs::score>(it.get()));
}

TEST_P(all_filter_test_case, all_order) {
  // add segment
  {
    tests::json_doc_generator gen(
      resource("simple_sequential.json"),
      &tests::generic_json_field_factory
    );
    add_segment(gen);
  }

  auto rdr = open_reader();

  // empty query
  //check_query(irs::all(), docs_t{}, costs_t{0}, rdr);

  // no order (same as empty order since no score is calculated)
  {
    docs_t docs{
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
      17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32
    };
    check_query(irs::all(), docs, costs_t{docs.size()}, rdr);
  }

  // custom order
  {
    docs_t docs{
      1, 4, 5, 16, 17, 20, 21, 2, 3, 6, 7, 18, 19, 22, 23, 8, 9,
      12, 13, 24, 25, 28, 29, 10, 11, 14, 15, 26, 27, 30, 31, 32
    };
    irs::order order;
    size_t collector_collect_field_count = 0;
    size_t collector_collect_term_count = 0;
    size_t collector_finish_count = 0;
    size_t scorer_score_count = 0;
    auto& sort = order.add<tests::sort::custom_sort>(false);

    sort.collector_collect_field = [&collector_collect_field_count](
        const irs::sub_reader&, const irs::term_reader&)->void {
      ++collector_collect_field_count;
    };
    sort.collector_collect_term = [&collector_collect_term_count](
        const irs::sub_reader&,
        const irs::term_reader&, const irs::attribute_provider&)->void {
      ++collector_collect_term_count;
    };
    sort.collectors_collect_ = [&collector_finish_count](
        const irs::byte_type*,
        const irs::index_reader&,
        const irs::sort::field_collector*,
        const irs::sort::term_collector*)->void {
      ++collector_finish_count;
    };
    sort.scorer_add = [](irs::doc_id_t& dst, const irs::doc_id_t& src)->void {
      dst = src;
    };
    sort.scorer_less = [](const irs::doc_id_t& lhs, const irs::doc_id_t& rhs)->bool {
      return (lhs & 0xAAAAAAAAAAAAAAAA) < (rhs & 0xAAAAAAAAAAAAAAAA);
    };
    sort.scorer_score = [&scorer_score_count](irs::doc_id_t)->void {
      ++scorer_score_count;
    };

    check_query(irs::all(), order, docs, rdr);
    ASSERT_EQ(0, collector_collect_field_count); // should not be executed
    ASSERT_EQ(0, collector_collect_term_count); // should not be executed
    ASSERT_EQ(1, collector_finish_count);
    ASSERT_EQ(32, scorer_score_count);
  }

  // custom order (no scorer)
  {
    docs_t docs{
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
      17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32
    };
    irs::order order;
    auto& sort = order.add<tests::sort::custom_sort>(false);

    sort.prepare_field_collector_ = []()->irs::sort::field_collector::ptr { return nullptr; };
    sort.prepare_scorer = [](
        const irs::sub_reader&,
        const irs::term_reader&,
        const irs::byte_type*,
        irs::byte_type*,
        const irs::attribute_provider&) -> irs::score_function {
      return { nullptr, nullptr };
    };
    sort.prepare_term_collector_ = []()->irs::sort::term_collector::ptr { return nullptr; };
    check_query(irs::all(), order, docs, rdr, false);
  }

  // frequency order
  {
    docs_t docs{
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
      17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32
    };
    irs::order order;

    order.add<tests::sort::frequency_sort>(false);
    check_query(irs::all(), order, docs, rdr);
  }

  // bm25 order
  {
    docs_t docs{
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
      17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32
    };
    irs::order order;

    order.add(true, irs::scorers::get("bm25", irs::type<irs::text_format::json>::get(), irs::string_ref::NIL));
    check_query(irs::all(), order, docs, rdr);
  }

  // tfidf order
  {
    docs_t docs{
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
      17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32
    };
    irs::order order;

    order.add(true, irs::scorers::get("tfidf", irs::type<irs::text_format::json>::get(), irs::string_ref::NIL));
    check_query(irs::all(), order, docs, rdr);
  }
}

INSTANTIATE_TEST_CASE_P(
  all_filter_test,
  all_filter_test_case,
  ::testing::Combine(
    ::testing::Values(
      &tests::memory_directory,
      &tests::fs_directory,
      &tests::mmap_directory
    ),
    ::testing::Values("1_0")
  ),
  tests::to_string
);

}

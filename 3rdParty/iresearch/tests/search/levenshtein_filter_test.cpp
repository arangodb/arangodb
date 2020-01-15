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
#include "filter_test_case_base.hpp"
#include "search/levenshtein_filter.hpp"

class levenshtein_filter_test_case : public tests::filter_test_case_base { };

TEST(by_edit_distance_test, ctor) {
  irs::by_edit_distance q;
  ASSERT_EQ(irs::by_edit_distance::type(), q.type());
  ASSERT_EQ(0, q.max_distance());
  ASSERT_FALSE(q.with_transpositions());
  ASSERT_TRUE(q.term().empty());
  ASSERT_TRUE(q.field().empty());
  ASSERT_EQ(irs::no_boost(), q.boost());
}

TEST(by_edit_distance_test, equal) {
  irs::by_edit_distance q;
  q.field("field").max_distance(1).with_transpositions(true).term("bar");

  ASSERT_EQ(q, irs::by_edit_distance().field("field").max_distance(1).with_transpositions(true).term("bar"));
  ASSERT_NE(q, irs::by_edit_distance().field("field").max_distance(1).term("bar"));
  ASSERT_NE(q, irs::by_edit_distance().field("field1").max_distance(1).with_transpositions(true).term("bar"));
  ASSERT_NE(q, irs::by_edit_distance().field("field").max_distance(1).with_transpositions(true).term("bar1"));
  ASSERT_NE(q, irs::by_edit_distance().field("field").with_transpositions(true).term("bar"));
  ASSERT_NE(q, irs::by_prefix().field("field").term("bar"));
}

TEST(by_edit_distance_test, boost) {
  // no boost
  {
    irs::by_edit_distance q;
    q.field("field").term("bar*");

    auto prepared = q.prepare(irs::sub_reader::empty());
    ASSERT_EQ(irs::no_boost(), prepared->boost());
  }

  // with boost
  {
    irs::boost_t boost = 1.5f;

    irs::by_edit_distance q;
    q.field("field").term("bar*");
    q.boost(boost);

    auto prepared = q.prepare(irs::sub_reader::empty());
    ASSERT_EQ(boost, prepared->boost());
  }
}

#ifndef IRESEARCH_DLL

TEST(by_edit_distance_test, test_type_of_prepared_query) {
  // term query
  {
    auto lhs = irs::by_term().field("foo").term("bar").prepare(irs::sub_reader::empty());
    auto rhs = irs::by_edit_distance().field("foo").term("bar").prepare(irs::sub_reader::empty());
    ASSERT_EQ(typeid(*lhs), typeid(*rhs));
  }
}

#endif

class by_edit_distance_test_case : public tests::filter_test_case_base { };

TEST_P(by_edit_distance_test_case, test_filter) {
  // add data
  {
    tests::json_doc_generator gen(
      resource("levenshtein_sequential.json"),
      &tests::generic_json_field_factory
    );
    add_segment(gen);
  }

  auto rdr = open_reader();

  // empty query
  check_query(irs::by_edit_distance(), docs_t{}, costs_t{0}, rdr);
  check_query(irs::by_edit_distance().field("title"), docs_t{}, costs_t{0}, rdr);
  check_query(irs::by_edit_distance().field("title").max_distance(3), docs_t{}, costs_t{0}, rdr);
  check_query(irs::by_edit_distance().field("title").max_distance(3).with_transpositions(true), docs_t{}, costs_t{0}, rdr);

  // term query
  check_query(irs::by_edit_distance().field("title").term("alphabet"), docs_t{1}, costs_t{1}, rdr);
}

INSTANTIATE_TEST_CASE_P(
  by_edit_distance_test,
  by_edit_distance_test_case,
  ::testing::Combine(
    ::testing::Values(
      &tests::memory_directory,
      &tests::fs_directory,
      &tests::mmap_directory
    ),
    ::testing::Values("1_0", "1_1", "1_2")
  ),
  tests::to_string
);

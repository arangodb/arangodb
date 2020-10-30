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
#include "filter_test_case_base.hpp"

#include "search/boost_sort.hpp"
#include "search/terms_filter.hpp"

namespace {

irs::by_terms make_filter(
    const irs::string_ref& field,
    const std::vector<std::pair<irs::string_ref, irs::boost_t>>& terms) {
  irs::by_terms q;
  *q.mutable_field() = field;
  for (auto& term : terms) {
    q.mutable_options()->terms.emplace(
      irs::ref_cast<irs::byte_type>(term.first),
      term.second);
  }
  return q;
}

}

TEST(by_terms_test, options) {
  irs::by_terms_options opts;
  ASSERT_TRUE(opts.terms.empty());
}

TEST(by_terms_test, ctor) {
  irs::by_terms q;
  ASSERT_EQ(irs::type<irs::by_terms>::id(), q.type());
  ASSERT_EQ(irs::by_terms_options{}, q.options());
  ASSERT_TRUE(q.field().empty());
  ASSERT_EQ(irs::no_boost(), q.boost());
}

TEST(by_terms_test, equal) {
  const irs::by_terms q0 = make_filter("field", { { "bar", 0.5f }, {"baz", 0.25f} });
  const irs::by_terms q1 = make_filter("field", { { "bar", 0.5f }, {"baz", 0.25f} });
  ASSERT_EQ(q0, q1);
  ASSERT_EQ(q0.hash(), q1.hash());

  const irs::by_terms q2 = make_filter("field1", { { "bar", 0.5f }, {"baz", 0.25f} });
  ASSERT_NE(q0, q2);

  const irs::by_terms q3 = make_filter("field", { { "bar1", 0.5f }, {"baz", 0.25f} });
  ASSERT_NE(q0, q3);

  const irs::by_terms q4 = make_filter("field", { { "bar", 0.5f }, {"baz", 0.5f} });
  ASSERT_NE(q0, q4);
}

TEST(by_terms_test, boost) {
  // no boost
  {
    irs::by_terms q = make_filter("field", { { "bar", 0.5f }, {"baz", 0.25f} });

    auto prepared = q.prepare(irs::sub_reader::empty());
    ASSERT_EQ(irs::no_boost(), prepared->boost());
  }

  // with boost
  {
    irs::boost_t boost = 1.5f;

    irs::by_terms q = make_filter("field", { { "bar", 0.5f }, {"baz", 0.25f} });
    q.boost(boost);

    auto prepared = q.prepare(irs::sub_reader::empty());
    ASSERT_EQ(boost, prepared->boost());
  }
}

class terms_filter_test_case : public tests::filter_test_case_base { };

TEST_P(terms_filter_test_case, simple_sequential_order) {
  // add segment
  {
    tests::json_doc_generator gen(
      resource("simple_sequential.json"),
      &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();
  ASSERT_EQ(1, rdr.size());

  // empty prefix test collector call count for field/term/finish
  {
    const docs_t docs{ 1, 21, 31, 32 };
    costs_t costs{ docs.size() };
    irs::order order;

    size_t collect_field_count = 0;
    size_t collect_term_count = 0;
    size_t finish_count = 0;
    auto& scorer = order.add<tests::sort::custom_sort>(false);

    scorer.collector_collect_field = [&collect_field_count](
        const irs::sub_reader&, const irs::term_reader&)->void{
      ++collect_field_count;
    };
    scorer.collector_collect_term = [&collect_term_count](
        const irs::sub_reader&,
        const irs::term_reader&,
        const irs::attribute_provider&)->void{
      ++collect_term_count;
    };
    scorer.collectors_collect_ = [&finish_count](
        irs::byte_type*,
        const irs::index_reader&,
        const irs::sort::field_collector*,
        const irs::sort::term_collector*)->void {
      ++finish_count;
    };
    scorer.prepare_field_collector_ = [&scorer]()->irs::sort::field_collector::ptr {
      return irs::memory::make_unique<tests::sort::custom_sort::prepared::field_collector>(scorer);
    };
    scorer.prepare_term_collector_ = [&scorer]()->irs::sort::term_collector::ptr {
      return irs::memory::make_unique<tests::sort::custom_sort::prepared::term_collector>(scorer);
    };

    const auto filter = make_filter("prefix", { {"abcd", 1.f}, {"abcd", 1.f}, {"abc", 1.f}, {"abcy", 1.f} });

    check_query(filter, order, docs, rdr);
    ASSERT_EQ(1, collect_field_count); // 1 fields in 1 segment
    ASSERT_EQ(3, collect_term_count); // 3 different terms
    ASSERT_EQ(3, finish_count); // 3 unque terms
  }

  // check boost
  {
    const docs_t docs{ 21, 31, 32, 1 };
    const costs_t costs{ docs.size() };
    irs::order order;
    order.add<irs::boost_sort>(true);

    const auto filter = make_filter("prefix", { {"abcd", 0.5f}, {"abcd", 1.f}, {"abc", 1.f}, {"abcy", 1.f} });

    check_query(filter, order, docs, rdr);
  }

  // check negative boost
  {
    const docs_t docs{ 21, 31, 32, 1 };
    const costs_t costs{ docs.size() };
    irs::order order;
    order.add<irs::boost_sort>(true);

    const auto filter = make_filter("prefix", { {"abcd", -1.f}, {"abcd", 0.5f}, {"abc", 0.65}, {"abcy", 0.5f} });

    check_query(filter, order, docs, rdr);
  }
}

TEST_P(terms_filter_test_case, simple_sequential) {
  // add segment
  {
    tests::json_doc_generator gen(
      resource("simple_sequential_utf8.json"),
      &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();
  ASSERT_EQ(1, rdr.size());
  auto& segment = rdr[0];

  // empty query
  check_query(irs::by_terms(), docs_t{}, costs_t{0}, rdr);

  // empty field
  check_query(make_filter("", { { "xyz", 0.5f} }), docs_t{}, costs_t{0}, rdr);

  // invalid field
  check_query(make_filter("same1", { { "xyz", 0.5f} }), docs_t{}, costs_t{0}, rdr);

  // invalid term
  check_query(make_filter("same", { { "invalid_term", 0.5f} }), docs_t{}, costs_t{0}, rdr);

  // no value requested to match
  check_query(make_filter("duplicated", { } ), docs_t{}, costs_t{0}, rdr);

  // match all
  {
    docs_t result;
    for(size_t i = 0; i < 32; ++i) {
      result.push_back(irs::doc_id_t((irs::type_limits<irs::type_t::doc_id_t>::min)() + i));
    }
    costs_t costs{ result.size() };
    const auto filter = make_filter("same", { { "xyz", 1.f } });
    check_query(filter, result, costs, rdr);

    // test visit
    tests::empty_filter_visitor visitor;
    const auto* reader = segment.field("same");
    ASSERT_NE(nullptr, reader);
    irs::by_terms::visit(segment, *reader, filter.options().terms, visitor);
    ASSERT_EQ(1, visitor.prepare_calls_counter());
    ASSERT_EQ(1, visitor.visit_calls_counter());
    ASSERT_EQ(
      (std::vector<std::pair<irs::string_ref, irs::boost_t>>{{"xyz", 1.f}}),
      visitor.term_refs<char>());
  }

  // match all
  {
    docs_t result;
    for(size_t i = 0; i < 32; ++i) {
      result.push_back(irs::doc_id_t((irs::type_limits<irs::type_t::doc_id_t>::min)() + i));
    }
    costs_t costs{ result.size() };
    const auto filter = make_filter("same", { { "xyz", 1.f }, { "invalid_term", 0.5f} });
    check_query(filter, result, costs, rdr);

    // test visit
    tests::empty_filter_visitor visitor;
    const auto* reader = segment.field("same");
    ASSERT_NE(nullptr, reader);
    irs::by_terms::visit(segment, *reader, filter.options().terms, visitor);
    ASSERT_EQ(1, visitor.prepare_calls_counter());
    ASSERT_EQ(1, visitor.visit_calls_counter());
    ASSERT_EQ(
      (std::vector<std::pair<irs::string_ref, irs::boost_t>>{{"xyz", 1.f}}),
      visitor.term_refs<char>());
  }

  // match something
  {
    const docs_t result{ 1, 21, 31, 32 };
    const costs_t costs{ result.size() };
    const auto filter = make_filter("prefix", { { "abcd", 1.f }, {"abc", 0.5f}, {"abcy", 0.5f} });
    check_query(filter, result, costs, rdr);

    // test visit
    tests::empty_filter_visitor visitor;
    const auto* reader = segment.field("prefix");
    ASSERT_NE(nullptr, reader);
    irs::by_terms::visit(segment, *reader, filter.options().terms, visitor);
    ASSERT_EQ(1, visitor.prepare_calls_counter());
    ASSERT_EQ(3, visitor.visit_calls_counter());
    ASSERT_EQ(
      (std::vector<std::pair<irs::string_ref, irs::boost_t>>{{"abc", 0.5f}, {"abcd", 1.f}, {"abcy", 0.5f}}),
      visitor.term_refs<char>());
  }

  // duplicate terms are not allowed
  {
    const docs_t result{ 1, 21, 31, 32 };
    const costs_t costs{ result.size() };
    const auto filter = make_filter("prefix", { { "abcd", 1.f }, { "abcd", 0.f }, {"abc", 0.5f}, {"abcy", 0.5f} });
    check_query(filter, result, costs, rdr);

    // test visit
    tests::empty_filter_visitor visitor;
    const auto* reader = segment.field("prefix");
    ASSERT_NE(nullptr, reader);
    irs::by_terms::visit(segment, *reader, filter.options().terms, visitor);
    ASSERT_EQ(1, visitor.prepare_calls_counter());
    ASSERT_EQ(3, visitor.visit_calls_counter());
    ASSERT_EQ(
      (std::vector<std::pair<irs::string_ref, irs::boost_t>>{{"abc", 0.5f}, {"abcd", 1.f}, {"abcy", 0.5f}}),
      visitor.term_refs<char>());
  }

  // test non existent term
  {
    const docs_t result{ 1, 21, 31, 32 };
    const costs_t costs{ result.size() };
    const auto filter = make_filter("prefix", { { "abcd", 1.f }, { "invalid_term", 0.f }, {"abc", 0.5f}, {"abcy", 0.5f} });
    check_query(filter, result, costs, rdr);

    // test visit
    tests::empty_filter_visitor visitor;
    const auto* reader = segment.field("prefix");
    ASSERT_NE(nullptr, reader);
    irs::by_terms::visit(segment, *reader, filter.options().terms, visitor);
    ASSERT_EQ(1, visitor.prepare_calls_counter());
    ASSERT_EQ(3, visitor.visit_calls_counter());
    ASSERT_EQ(
      (std::vector<std::pair<irs::string_ref, irs::boost_t>>{{"abc", 0.5f}, {"abcd", 1.f}, {"abcy", 0.5f}}),
      visitor.term_refs<char>());
  }
}

INSTANTIATE_TEST_CASE_P(
  terms_filter_test,
  terms_filter_test_case,
  ::testing::Combine(
    ::testing::Values(
      &tests::memory_directory,
      &tests::fs_directory,
      &tests::mmap_directory
    ),
    ::testing::Values(tests::format_info{"1_0"},
                      tests::format_info{"1_1", "1_0"},
                      tests::format_info{"1_2", "1_0"},
                      tests::format_info{"1_3", "1_0"})
  ),
  tests::to_string
);

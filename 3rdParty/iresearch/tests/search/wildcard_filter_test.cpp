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
#include "search/wildcard_filter.hpp"

#ifndef IRESEARCH_DLL
#include "search/term_filter.hpp"
#include "search/all_filter.hpp"
#include "search/prefix_filter.hpp"
#include "search/multiterm_query.hpp"
#endif

namespace {

template<typename Filter = irs::by_wildcard>
Filter make_filter(
    const irs::string_ref& field,
    const irs::string_ref term) {
  Filter q;
  *q.mutable_field() = field;
  q.mutable_options()->term = irs::ref_cast<irs::byte_type>(term);
  return q;
}

}

TEST(by_wildcard_test, options) {
  irs::by_wildcard_options opts;
  ASSERT_TRUE(opts.term.empty());
  ASSERT_EQ(1024, opts.scored_terms_limit);
}

TEST(by_wildcard_test, ctor) {
  irs::by_wildcard q;
  ASSERT_EQ(irs::type<irs::by_wildcard>::id(), q.type());
  ASSERT_EQ(irs::by_wildcard_options{}, q.options());
  ASSERT_TRUE(q.field().empty());
  ASSERT_EQ(irs::no_boost(), q.boost());
}

TEST(by_wildcard_test, equal) {
  const irs::by_wildcard q = make_filter("field", "bar*");

  ASSERT_EQ(q, make_filter("field", "bar*"));
  ASSERT_NE(q, make_filter("field1", "bar*"));
  ASSERT_NE(q, make_filter("field", "bar"));

  irs::by_wildcard q1 = make_filter("field", "bar*");
  q1.mutable_options()->scored_terms_limit = 100;
  ASSERT_NE(q, q1);
}

TEST(by_wildcard_test, boost) {
  // no boost
  {
    irs::by_wildcard q = make_filter("field", "bar*");

    auto prepared = q.prepare(irs::sub_reader::empty());
    ASSERT_EQ(irs::no_boost(), prepared->boost());
  }

  // with boost
  {
    irs::boost_t boost = 1.5f;

    irs::by_wildcard q = make_filter("field", "bar*");
    q.boost(boost);

    auto prepared = q.prepare(irs::sub_reader::empty());
    ASSERT_EQ(boost, prepared->boost());
  }
}

#ifndef IRESEARCH_DLL

TEST(by_wildcard_test, test_type_of_prepared_query) {
  // term query
  {
    auto lhs = make_filter<irs::by_term>("foo", "bar").prepare(irs::sub_reader::empty());
    auto rhs = make_filter("foo", "bar").prepare(irs::sub_reader::empty());
    ASSERT_EQ(typeid(*lhs), typeid(*rhs));
  }

  // term query
  {
    auto lhs = make_filter<irs::by_term>("foo", "").prepare(irs::sub_reader::empty());
    auto rhs = make_filter("foo", "").prepare(irs::sub_reader::empty());
    ASSERT_EQ(typeid(*lhs), typeid(*rhs));
  }

  // term query
  {
    auto lhs = make_filter<irs::by_term>("foo", "foo%").prepare(irs::sub_reader::empty());
    auto rhs = make_filter("foo", "foo\\%").prepare(irs::sub_reader::empty());
    ASSERT_EQ(typeid(*lhs), typeid(*rhs));
  }

  // prefix query
  {
    auto lhs = make_filter<irs::by_prefix>("foo", "bar").prepare(irs::sub_reader::empty());
    auto rhs = make_filter("foo", "bar%").prepare(irs::sub_reader::empty());
    ASSERT_EQ(typeid(*lhs), typeid(*rhs));
  }

  // prefix query
  {
    auto lhs = make_filter<irs::by_prefix>("foo", "bar").prepare(irs::sub_reader::empty());
    auto rhs = make_filter("foo", "bar%%").prepare(irs::sub_reader::empty());
    ASSERT_EQ(typeid(*lhs), typeid(*rhs));
  }

  // term query
  {
    auto lhs = make_filter<irs::by_term>("foo", "bar%").prepare(irs::sub_reader::empty());
    auto rhs = make_filter("foo", "bar\\%").prepare(irs::sub_reader::empty());
    ASSERT_EQ(typeid(*lhs), typeid(*rhs));
  }

  // all query
  {
    auto lhs = make_filter<irs::by_prefix>("foo", "").prepare(irs::sub_reader::empty());
    auto rhs = make_filter("foo", "%").prepare(irs::sub_reader::empty());
    ASSERT_EQ(typeid(*lhs), typeid(*rhs));
  }

  // all query
  {
    auto lhs = make_filter<irs::by_prefix>("foo", "").prepare(irs::sub_reader::empty());
    auto rhs = make_filter("foo", "%%").prepare(irs::sub_reader::empty());
    ASSERT_EQ(typeid(*lhs), typeid(*rhs));
  }

  // term query
  {
    auto lhs = make_filter<irs::by_term>("foo", "%").prepare(irs::sub_reader::empty());
    auto rhs = make_filter("foo", "\\%").prepare(irs::sub_reader::empty());
    ASSERT_EQ(typeid(*lhs), typeid(*rhs));
  }
}

#endif

class wildcard_filter_test_case : public tests::filter_test_case_base { };

TEST_P(wildcard_filter_test_case, simple_sequential_order) {
  // add segment
  {
    tests::json_doc_generator gen(
      resource("simple_sequential.json"),
      &tests::generic_json_field_factory);
    add_segment( gen );
  }

  auto rdr = open_reader();

  // empty query
  check_query(irs::by_wildcard(), docs_t{}, costs_t{0}, rdr);

  // empty prefix test collector call count for field/term/finish
  {
    docs_t docs{ 1, 4, 9, 16, 21, 24, 26, 29, 31, 32 };
    costs_t costs{ docs.size() };
    irs::order order;

    size_t collect_field_count = 0;
    size_t collect_term_count = 0;
    size_t finish_count = 0;
    auto& scorer = order.add<tests::sort::custom_sort>(false);

    scorer.collector_collect_field = [&collect_field_count](const irs::sub_reader&, const irs::term_reader&)->void{
      ++collect_field_count;
    };
    scorer.collector_collect_term = [&collect_term_count](const irs::sub_reader&, const irs::term_reader&, const irs::attribute_provider&)->void{
      ++collect_term_count;
    };
    scorer.collectors_collect_ = [&finish_count](irs::byte_type*, const irs::index_reader&, const irs::sort::field_collector*, const irs::sort::term_collector*)->void {
      ++finish_count;
    };
    scorer.prepare_field_collector_ = [&scorer]()->irs::sort::field_collector::ptr {
      return irs::memory::make_unique<tests::sort::custom_sort::prepared::field_collector>(scorer);
    };
    scorer.prepare_term_collector_ = [&scorer]()->irs::sort::term_collector::ptr {
      return irs::memory::make_unique<tests::sort::custom_sort::prepared::term_collector>(scorer);
    };
    check_query(make_filter("prefix", "%"), order, docs, rdr);
    ASSERT_EQ(9, collect_field_count); // 9 fields (1 per term since treated as a disjunction) in 1 segment
    ASSERT_EQ(9, collect_term_count); // 9 different terms
    ASSERT_EQ(9, finish_count); // 9 unque terms
  }

  // match all
  {
    docs_t docs{ 31, 32, 1, 4, 9, 16, 21, 24, 26, 29 };
    costs_t costs{ docs.size() };
    irs::order order;
    order.add<tests::sort::frequency_sort>(false);

    check_query(make_filter("prefix", "%"), order, docs, rdr);
  }

  // prefix
  {
    docs_t docs{ 31, 32, 1, 4, 16, 21, 26, 29 };
    costs_t costs{ docs.size() };
    irs::order order;
    order.add<tests::sort::frequency_sort>(false);

    check_query(make_filter("prefix", "a%"), order, docs, rdr);
  }
}

TEST_P(wildcard_filter_test_case, simple_sequential) {
  // add segment
  {
    tests::json_doc_generator gen(
      resource("simple_sequential_utf8.json"),
      &tests::generic_json_field_factory);
    add_segment( gen );
  }

  auto rdr = open_reader();

  // empty query
  check_query(irs::by_wildcard(), docs_t{}, costs_t{0}, rdr);

  // empty field
  check_query(make_filter("", "xyz%"), docs_t{}, costs_t{0}, rdr);

  // invalid field
  check_query(make_filter("same1", "xyz%"), docs_t{}, costs_t{0}, rdr);

  // invalid prefix
  check_query(make_filter("same", "xyz_invalid%"), docs_t{}, costs_t{0}, rdr);

  // empty pattern - no match
  check_query(make_filter("duplicated", ""), docs_t{}, costs_t{0}, rdr);

  // match all
  {
    docs_t result;
    for(size_t i = 0; i < 32; ++i) {
      result.push_back(irs::doc_id_t((irs::type_limits<irs::type_t::doc_id_t>::min)() + i));
    }

    costs_t costs{ result.size() };

    check_query(make_filter("same", "%"), result, costs, rdr);
    check_query(make_filter("same", "___"), result, costs, rdr);
    check_query(make_filter("same", "%_"), result, costs, rdr);
    check_query(make_filter("same", "_%"), result, costs, rdr);
    check_query(make_filter("same", "x_%"), result, costs, rdr);
    check_query(make_filter("same", "__z"), result, costs, rdr);
    check_query(make_filter("same", "%_z"), result, costs, rdr);
    check_query(make_filter("same", "x%_"), result, costs, rdr);
    check_query(make_filter("same", "x_%"), result, costs, rdr);
    check_query(make_filter("same", "x_z"), result, costs, rdr);
    check_query(make_filter("same", "x%z"), result, costs, rdr);
    check_query(make_filter("same", "_yz"), result, costs, rdr);
    check_query(make_filter("same", "%yz"), result, costs, rdr);
    check_query(make_filter("same", "xyz"), result, costs, rdr);
  }

  // match nothing
  check_query(make_filter("prefix", "ab\\%"), docs_t{}, costs_t{0}, rdr);
  check_query(make_filter("same", "x\\_z"), docs_t{}, costs_t{0}, rdr);
  check_query(make_filter("same", "x\\%z"), docs_t{}, costs_t{0}, rdr);
  check_query(make_filter("same", "_"), docs_t{}, costs_t{0}, rdr);

  // escaped prefix
  {
    docs_t result{10, 11};
    costs_t costs{ result.size() };

    check_query(make_filter("prefix", "ab\\\\%"), result, costs, rdr);
  }

  // escaped term
  {
    docs_t result{10};
    costs_t costs{ result.size() };

    check_query(make_filter("prefix", "ab\\\\\\%"), result, costs, rdr);
  }

  // escaped term
  {
    docs_t result{11};
    costs_t costs{ result.size() };

    check_query(make_filter("prefix", "ab\\\\\\\\%"), result, costs, rdr);
  }

  // valid prefix
  {
    docs_t result;
    for(size_t i = 0; i < 32; ++i) {
      result.push_back(irs::doc_id_t((irs::type_limits<irs::type_t::doc_id_t>::min)() + i));
    }

    costs_t costs{ result.size() };

    check_query(make_filter("same", "xyz%"), result, costs, rdr);
  }

  // pattern
  {
    docs_t docs{ 2, 3, 8, 14, 17, 19, 24 };
    costs_t costs{ docs.size() };

    check_query(make_filter("duplicated", "v_z%"), docs, costs, rdr);
    check_query(make_filter("duplicated", "v%c"), docs, costs, rdr);
    check_query(make_filter("duplicated", "v%%%%%c"), docs, costs, rdr);
    check_query(make_filter("duplicated", "%c"), docs, costs, rdr);
    check_query(make_filter("duplicated", "%_c"), docs, costs, rdr);
  }

  // pattern
  {
    docs_t docs{ 1, 4, 9, 21, 26, 31, 32 };
    costs_t costs{ docs.size() };

    check_query(make_filter("prefix", "%c%"), docs, costs, rdr);
    check_query(make_filter("prefix", "%c%%"), docs, costs, rdr);
    check_query(make_filter("prefix", "%%%%c%%"), docs, costs, rdr);
    check_query(make_filter("prefix", "%%c%"), docs, costs, rdr);
    check_query(make_filter("prefix", "%%c%%"), docs, costs, rdr);
  }

  // single digit prefix
  {
    docs_t docs{ 1, 5, 11, 21, 27, 31 };
    costs_t costs{ docs.size() };

    check_query(make_filter("duplicated", "a%"), docs, costs, rdr);
  }

  check_query(make_filter("name", "!%"), docs_t{28}, costs_t{1}, rdr);
  check_query(make_filter("prefix", "b%"), docs_t{9, 24}, costs_t{2}, rdr);

  // multiple digit prefix
  {
    docs_t docs{ 2, 3, 8, 14, 17, 19, 24 };
    costs_t costs{ docs.size() };

    check_query(make_filter("duplicated", "vcz%"), docs, costs, rdr);
    check_query(make_filter("duplicated", "vcz%%%%%"), docs, costs, rdr);
  }

  {
    docs_t docs{ 1, 4, 21, 26, 31, 32 };
    costs_t costs{ docs.size() };
    check_query(make_filter("prefix", "abc%"), docs, costs, rdr);
  }

  {
    docs_t docs{ 1, 4, 21, 26, 31, 32 };
    costs_t costs{ docs.size() };

    check_query(make_filter("prefix", "abc%"), docs, costs, rdr);
    check_query(make_filter("prefix", "abc%%"), docs, costs, rdr);
  }

  {
    docs_t docs{ 1, 4, 16, 26 };
    costs_t costs{ docs.size() };

    check_query(make_filter("prefix", "a%d%"), docs, costs, rdr);
    check_query(make_filter("prefix", "a%d%%"), docs, costs, rdr);
  }

  {
    docs_t docs{ 1, 26 };
    costs_t costs{ docs.size() };

    check_query(make_filter("utf8", "\x25\xD0\xB9"), docs, costs, rdr);
    check_query(make_filter("utf8", "\x25\x25\xD0\xB9"), docs, costs, rdr);
  }

  {
    docs_t docs{ 26 };
    costs_t costs{ docs.size() };

    check_query(make_filter("utf8", "\xD0\xB2\x25\xD0\xB9"), docs, costs, rdr);
    check_query(make_filter("utf8", "\xD0\xB2\x25\x25\xD0\xB9"), docs, costs, rdr);
  }

  {
    docs_t docs{ 1, 3 };
    costs_t costs{ docs.size() };

    check_query(make_filter("utf8", "\xD0\xBF\x25"), docs, costs, rdr);
    check_query(make_filter("utf8", "\xD0\xBF\x25\x25"), docs, costs, rdr);
  }

  // whole word
  check_query(make_filter("prefix", "bateradsfsfasdf"), docs_t{24}, costs_t{1}, rdr);
}

TEST_P(wildcard_filter_test_case, visit) {
  // add segment
  {
    tests::json_doc_generator gen(
      resource("simple_sequential.json"),
      &tests::generic_json_field_factory);
    add_segment(gen);
  }

  std::string fld = "prefix";
  irs::string_ref field = irs::string_ref(fld);

  // read segment
  auto index = open_reader();
  ASSERT_EQ(1, index.size());
  auto& segment = index[0];
  // get term dictionary for field
  const auto* reader = segment.field(field);
  ASSERT_NE(nullptr, reader);

  {
    auto term = irs::ref_cast<irs::byte_type>(irs::string_ref("abc"));
    tests::empty_filter_visitor visitor;
    auto field_visitor = irs::by_wildcard::visitor(term);
    ASSERT_TRUE(field_visitor);
    field_visitor(segment, *reader, visitor);
    ASSERT_EQ(1, visitor.prepare_calls_counter());
    ASSERT_EQ(1, visitor.visit_calls_counter());
    ASSERT_EQ(
      (std::vector<std::pair<irs::string_ref, irs::boost_t>>{
        {"abc", irs::no_boost()},
      }),
      visitor.term_refs<char>());

    visitor.reset();
  }

  {
    auto prefix = irs::ref_cast<irs::byte_type>(irs::string_ref("ab%"));
    tests::empty_filter_visitor visitor;
    auto field_visitor = irs::by_wildcard::visitor(prefix);
    ASSERT_TRUE(field_visitor);
    field_visitor(segment, *reader, visitor);
    ASSERT_EQ(1, visitor.prepare_calls_counter());
    ASSERT_EQ(6, visitor.visit_calls_counter());
    ASSERT_EQ(
      (std::vector<std::pair<irs::string_ref, irs::boost_t>>{
        {"abc", irs::no_boost()},
        {"abcd", irs::no_boost()},
        {"abcde", irs::no_boost()},
        {"abcdrer", irs::no_boost()},
        {"abcy", irs::no_boost()},
        {"abde", irs::no_boost()}
      }),
      visitor.term_refs<char>());

    visitor.reset();
  }

  {
    auto wildcard = irs::ref_cast<irs::byte_type>(irs::string_ref("a_c%"));
    tests::empty_filter_visitor visitor;
    auto field_visitor = irs::by_wildcard::visitor(wildcard);
    ASSERT_TRUE(field_visitor);
    field_visitor(segment, *reader, visitor);
    ASSERT_EQ(1, visitor.prepare_calls_counter());
    ASSERT_EQ(5, visitor.visit_calls_counter());
    ASSERT_EQ(
      (std::vector<std::pair<irs::string_ref, irs::boost_t>>{
        {"abc", irs::no_boost()},
        {"abcd", irs::no_boost()},
        {"abcde", irs::no_boost()},
        {"abcdrer", irs::no_boost()},
        {"abcy", irs::no_boost()},
      }),
      visitor.term_refs<char>());

    visitor.reset();
  }
}

INSTANTIATE_TEST_CASE_P(
  wildcard_filter_test,
  wildcard_filter_test_case,
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

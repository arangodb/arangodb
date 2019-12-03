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
#endif

class wildcard_filter_test_case : public tests::filter_test_case_base {
 protected:
  //void by_prefix_order() {
  //  // add segment
  //  {
  //    tests::json_doc_generator gen(
  //      resource("simple_sequential.json"),
  //      &tests::generic_json_field_factory);
  //    add_segment( gen );
  //  }

  //  auto rdr = open_reader();

  //  // empty query
  //  check_query(irs::by_prefix(), docs_t{}, costs_t{0}, rdr);

  //  // empty prefix test collector call count for field/term/finish
  //  {
  //    docs_t docs{ 1, 4, 9, 16, 21, 24, 26, 29, 31, 32 };
  //    costs_t costs{ docs.size() };
  //    irs::order order;

  //    size_t collect_field_count = 0;
  //    size_t collect_term_count = 0;
  //    size_t finish_count = 0;
  //    auto& scorer = order.add<tests::sort::custom_sort>(false);

  //    scorer.collector_collect_field = [&collect_field_count](const irs::sub_reader&, const irs::term_reader&)->void{
  //      ++collect_field_count;
  //    };
  //    scorer.collector_collect_term = [&collect_term_count](const irs::sub_reader&, const irs::term_reader&, const irs::attribute_view&)->void{
  //      ++collect_term_count;
  //    };
  //    scorer.collectors_collect_ = [&finish_count](irs::byte_type*, const irs::index_reader&, const irs::sort::field_collector*, const irs::sort::term_collector*)->void {
  //      ++finish_count;
  //    };
  //    scorer.prepare_field_collector_ = [&scorer]()->irs::sort::field_collector::ptr {
  //      return irs::memory::make_unique<tests::sort::custom_sort::prepared::collector>(scorer);
  //    };
  //    scorer.prepare_term_collector_ = [&scorer]()->irs::sort::term_collector::ptr {
  //      return irs::memory::make_unique<tests::sort::custom_sort::prepared::collector>(scorer);
  //    };
  //    check_query(irs::by_prefix().field("prefix"), order, docs, rdr);
  //    ASSERT_EQ(9, collect_field_count); // 9 fields (1 per term since treated as a disjunction) in 1 segment
  //    ASSERT_EQ(9, collect_term_count); // 9 different terms
  //    ASSERT_EQ(9, finish_count); // 9 unque terms
  //  }

  //  // empty prefix
  //  {
  //    docs_t docs{ 31, 32, 1, 4, 9, 16, 21, 24, 26, 29 };
  //    costs_t costs{ docs.size() };
  //    irs::order order;

  //    order.add<tests::sort::frequency_sort>(false);
  //    check_query(irs::by_prefix().field("prefix"), order, docs, rdr);
  //  }

  //  //FIXME
  //  // empty prefix + scored_terms_limit
////    {
////      docs_t docs{ 31, 32, 1, 4, 9, 16, 21, 24, 26, 29 };
////      costs_t costs{ docs.size() };
////      irs::order order;
////
////      order.add<sort::frequency_sort>(false);
////      check_query(irs::by_prefix().field("prefix").scored_terms_limit(1), order, docs, rdr);
////    }
////
  //  // prefix
  //  {
  //    docs_t docs{ 31, 32, 1, 4, 16, 21, 26, 29 };
  //    costs_t costs{ docs.size() };
  //    irs::order order;

  //    order.add<tests::sort::frequency_sort>(false);
  //    check_query(irs::by_prefix().field("prefix").term("a"), order, docs, rdr);
  //  }
  //}

  //void by_prefix_schemas() {
  //  // write segments
  //  {
  //    auto writer = open_writer(irs::OM_CREATE);

  //    std::vector<tests::doc_generator_base::ptr> gens;
  //    gens.emplace_back(new tests::json_doc_generator(
  //      resource("AdventureWorks2014.json"),
  //      &tests::generic_json_field_factory
  //    ));
  //    gens.emplace_back(new tests::json_doc_generator(
  //      resource("AdventureWorks2014Edges.json"),
  //      &tests::generic_json_field_factory
  //    ));
  //    gens.emplace_back(new tests::json_doc_generator(
  //      resource("Northwnd.json"),
  //      &tests::generic_json_field_factory
  //    ));
  //    gens.emplace_back(new tests::json_doc_generator(
  //      resource("NorthwndEdges.json"),
  //      &tests::generic_json_field_factory
  //    ));
  //    add_segments(*writer, gens);
  //  }

  //  auto rdr = open_reader();

  //  check_query(irs::by_prefix().field("Name").term("Addr"), docs_t{1, 2, 77, 78}, rdr);
  //}
}; // wildcard_filter_test_case

TEST(by_wildcard_test, ctor) {
  irs::by_wildcard q;
  ASSERT_EQ(irs::by_wildcard::type(), q.type());
  ASSERT_TRUE(q.term().empty());
  ASSERT_TRUE(q.field().empty());
  ASSERT_EQ(irs::no_boost(), q.boost());
}

TEST(by_wildcard_test, equal) { 
  irs::by_wildcard q;
  q.field("field").term("bar*");

  ASSERT_EQ(q, irs::by_wildcard().field("field").term("bar*"));
  ASSERT_NE(q, irs::by_term().field("field").term("bar*"));
  ASSERT_NE(q, irs::by_term().field("field1").term("bar*"));
}

TEST(by_wildcard_test, boost) {
  // no boost
  {
    irs::by_wildcard q;
    q.field("field").term("bar*");

    auto prepared = q.prepare(irs::sub_reader::empty());
    ASSERT_EQ(irs::no_boost(), prepared->boost());
  }

  // with boost
  {
    irs::boost_t boost = 1.5f;

    irs::by_wildcard q;
    q.field("field").term("bar*");
    q.boost(boost);

    auto prepared = q.prepare(irs::sub_reader::empty());
    ASSERT_EQ(boost, prepared->boost());
  }
}

TEST(by_wildcard_test, wildcard_type) {
  ASSERT_EQ(irs::WildcardType::TERM, irs::wildcard_type(irs::ref_cast<irs::byte_type>(irs::string_ref("foo"))));
  ASSERT_EQ(irs::WildcardType::TERM, irs::wildcard_type(irs::ref_cast<irs::byte_type>(irs::string_ref("\\foo"))));
  ASSERT_EQ(irs::WildcardType::TERM, irs::wildcard_type(irs::ref_cast<irs::byte_type>(irs::string_ref("\\%foo"))));
  ASSERT_EQ(irs::WildcardType::TERM, irs::wildcard_type(irs::ref_cast<irs::byte_type>(irs::string_ref("\foo"))));
  ASSERT_EQ(irs::WildcardType::PREFIX, irs::wildcard_type(irs::ref_cast<irs::byte_type>(irs::string_ref("foo%"))));
  ASSERT_EQ(irs::WildcardType::PREFIX, irs::wildcard_type(irs::ref_cast<irs::byte_type>(irs::string_ref("foo%%"))));
  ASSERT_EQ(irs::WildcardType::WILDCARD, irs::wildcard_type(irs::ref_cast<irs::byte_type>(irs::string_ref("foo%_"))));
  ASSERT_EQ(irs::WildcardType::WILDCARD, irs::wildcard_type(irs::ref_cast<irs::byte_type>(irs::string_ref("foo%\\"))));
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

#ifndef IRESEARCH_DLL

TEST(by_wildcard_test, test_type_of_prepared_query) {
  // term query
  {
    auto lhs = irs::by_term().field("foo").term("bar").prepare(irs::sub_reader::empty());
    auto rhs = irs::by_wildcard().field("foo").term("bar").prepare(irs::sub_reader::empty());
    ASSERT_EQ(typeid(*lhs), typeid(*rhs));
  }

  // term query
  {
    auto lhs = irs::by_term().field("foo").term("").prepare(irs::sub_reader::empty());
    auto rhs = irs::by_wildcard().field("foo").term("").prepare(irs::sub_reader::empty());
    ASSERT_EQ(typeid(*lhs), typeid(*rhs));
  }

  // prefix query
  {
    auto lhs = irs::by_prefix().field("foo").term("bar").prepare(irs::sub_reader::empty());
    auto rhs = irs::by_wildcard().field("foo").term("bar%").prepare(irs::sub_reader::empty());
    ASSERT_EQ(typeid(*lhs), typeid(*rhs));
  }

  // prefix query
  {
    auto lhs = irs::by_prefix().field("foo").term("bar").prepare(irs::sub_reader::empty());
    auto rhs = irs::by_wildcard().field("foo").term("bar%%").prepare(irs::sub_reader::empty());
    ASSERT_EQ(typeid(*lhs), typeid(*rhs));
  }

  // term query
  {
    auto lhs = irs::by_term().field("foo").term("bar%").prepare(irs::sub_reader::empty());
    auto rhs = irs::by_wildcard().field("foo").term("bar\\%").prepare(irs::sub_reader::empty());
    ASSERT_EQ(typeid(*lhs), typeid(*rhs));
  }

  // all query
  {
    auto lhs = irs::by_prefix().field("foo").prepare(irs::sub_reader::empty());
    auto rhs = irs::by_wildcard().field("foo").term("%").prepare(irs::sub_reader::empty());
    ASSERT_EQ(typeid(*lhs), typeid(*rhs));
  }

  // all query
  {
    auto lhs = irs::by_prefix().field("foo").prepare(irs::sub_reader::empty());
    auto rhs = irs::by_wildcard().field("foo").term("%%").prepare(irs::sub_reader::empty());
    ASSERT_EQ(typeid(*lhs), typeid(*rhs));
  }

  // term query
  {
    auto lhs = irs::by_term().field("foo").term("%").prepare(irs::sub_reader::empty());
    auto rhs = irs::by_wildcard().field("foo").term("\\%").prepare(irs::sub_reader::empty());
    ASSERT_EQ(typeid(*lhs), typeid(*rhs));
  }
}

#endif

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
  check_query(irs::by_prefix(), docs_t{}, costs_t{0}, rdr);

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
    scorer.collector_collect_term = [&collect_term_count](const irs::sub_reader&, const irs::term_reader&, const irs::attribute_view&)->void{
      ++collect_term_count;
    };
    scorer.collectors_collect_ = [&finish_count](irs::byte_type*, const irs::index_reader&, const irs::sort::field_collector*, const irs::sort::term_collector*)->void {
      ++finish_count;
    };
    scorer.prepare_field_collector_ = [&scorer]()->irs::sort::field_collector::ptr {
      return irs::memory::make_unique<tests::sort::custom_sort::prepared::collector>(scorer);
    };
    scorer.prepare_term_collector_ = [&scorer]()->irs::sort::term_collector::ptr {
      return irs::memory::make_unique<tests::sort::custom_sort::prepared::collector>(scorer);
    };
    check_query(irs::by_wildcard().field("prefix").term("%"), order, docs, rdr);
    ASSERT_EQ(9, collect_field_count); // 9 fields (1 per term since treated as a disjunction) in 1 segment
    ASSERT_EQ(9, collect_term_count); // 9 different terms
    ASSERT_EQ(9, finish_count); // 9 unque terms
  }

  // empty prefix
  {
    docs_t docs{ 31, 32, 1, 4, 9, 16, 21, 24, 26, 29 };
    costs_t costs{ docs.size() };
    irs::order order;

    order.add<tests::sort::frequency_sort>(false);
    check_query(irs::by_prefix().field("prefix"), order, docs, rdr);
  }

  //FIXME
  // empty prefix + scored_terms_limit
//    {
//      docs_t docs{ 31, 32, 1, 4, 9, 16, 21, 24, 26, 29 };
//      costs_t costs{ docs.size() };
//      irs::order order;
//
//      order.add<sort::frequency_sort>(false);
//      check_query(irs::by_prefix().field("prefix").scored_terms_limit(1), order, docs, rdr);
//    }
//
  // prefix
  {
    docs_t docs{ 31, 32, 1, 4, 16, 21, 26, 29 };
    costs_t costs{ docs.size() };
    irs::order order;

    order.add<tests::sort::frequency_sort>(false);
    check_query(irs::by_prefix().field("prefix").term("a"), order, docs, rdr);
  }
}

TEST_P(wildcard_filter_test_case, simple_sequential) {
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

  // empty field
  check_query(irs::by_wildcard().term("xyz%"), docs_t{}, costs_t{0}, rdr);

  // invalid field
  check_query(irs::by_wildcard().field("same1").term("xyz%"), docs_t{}, costs_t{0}, rdr);

  // invalid prefix
  check_query(irs::by_wildcard().field("same").term("xyz_invalid%"), docs_t{}, costs_t{0}, rdr);

  // empty pattern - no match
  check_query(irs::by_wildcard().field("duplicated"), docs_t{}, costs_t{0}, rdr);

  // match all
  {
    docs_t result;
    for(size_t i = 0; i < 32; ++i) {
      result.push_back(irs::doc_id_t((irs::type_limits<irs::type_t::doc_id_t>::min)() + i));
    }

    costs_t costs{ result.size() };

    check_query(irs::by_wildcard().field("same").term("%"), result, costs, rdr);
    check_query(irs::by_wildcard().field("same").term("___"), result, costs, rdr);
    check_query(irs::by_wildcard().field("same").term("%_"), result, costs, rdr);
    check_query(irs::by_wildcard().field("same").term("_%"), result, costs, rdr);
    check_query(irs::by_wildcard().field("same").term("x_%"), result, costs, rdr);
    check_query(irs::by_wildcard().field("same").term("__z"), result, costs, rdr);
    check_query(irs::by_wildcard().field("same").term("%_z"), result, costs, rdr);
    check_query(irs::by_wildcard().field("same").term("x%_"), result, costs, rdr);
    check_query(irs::by_wildcard().field("same").term("x_%"), result, costs, rdr);
    check_query(irs::by_wildcard().field("same").term("x_z"), result, costs, rdr);
    check_query(irs::by_wildcard().field("same").term("x%z"), result, costs, rdr);
    check_query(irs::by_wildcard().field("same").term("_yz"), result, costs, rdr);
    check_query(irs::by_wildcard().field("same").term("%yz"), result, costs, rdr);
    check_query(irs::by_wildcard().field("same").term("xyz"), result, costs, rdr);
  }

  // match nothing
  check_query(irs::by_wildcard().field("same").term("x\\_z"), docs_t{}, costs_t{0}, rdr);
  check_query(irs::by_wildcard().field("same").term("x\\%z"), docs_t{}, costs_t{0}, rdr);
  check_query(irs::by_wildcard().field("same").term("_"), docs_t{}, costs_t{0}, rdr);

  // valid prefix
  {
    docs_t result;
    for(size_t i = 0; i < 32; ++i) {
      result.push_back(irs::doc_id_t((irs::type_limits<irs::type_t::doc_id_t>::min)() + i));
    }

    costs_t costs{ result.size() };

    check_query(irs::by_wildcard().field("same").term("xyz%"), result, costs, rdr);
  }

  // pattern
  {
    docs_t docs{ 2, 3, 8, 14, 17, 19, 24 };
    costs_t costs{ docs.size() };

    check_query(irs::by_wildcard().field("duplicated").term("v_z%"), docs, costs, rdr);
    check_query(irs::by_wildcard().field("duplicated").term("v%c"), docs, costs, rdr);
    check_query(irs::by_wildcard().field("duplicated").term("v%%%%%c"), docs, costs, rdr);
    check_query(irs::by_wildcard().field("duplicated").term("%c"), docs, costs, rdr);
    check_query(irs::by_wildcard().field("duplicated").term("%_c"), docs, costs, rdr);
  }

  // pattern
  {
    docs_t docs{ 1, 4, 9, 21, 26, 31, 32 };
    costs_t costs{ docs.size() };

    check_query(irs::by_wildcard().field("prefix").term("%c%"), docs, costs, rdr);
    check_query(irs::by_wildcard().field("prefix").term("%c%%"), docs, costs, rdr);
    check_query(irs::by_wildcard().field("prefix").term("%%%%c%%"), docs, costs, rdr);
    check_query(irs::by_wildcard().field("prefix").term("%%c%"), docs, costs, rdr);
    check_query(irs::by_wildcard().field("prefix").term("%%c%%"), docs, costs, rdr);
  }

  // single digit prefix
  {
    docs_t docs{ 1, 5, 11, 21, 27, 31 };
    costs_t costs{ docs.size() };

    check_query(irs::by_wildcard().field("duplicated").term("a%"), docs, costs, rdr);
  }

  check_query(irs::by_wildcard().field("name").term("!%"), docs_t{28}, costs_t{1}, rdr);
  check_query(irs::by_wildcard().field("prefix").term("b%"), docs_t{9, 24}, costs_t{2}, rdr);

  // multiple digit prefix
  {
    docs_t docs{ 2, 3, 8, 14, 17, 19, 24 };
    costs_t costs{ docs.size() };

    check_query(irs::by_wildcard().field("duplicated").term("vcz%"), docs, costs, rdr);
    check_query(irs::by_wildcard().field("duplicated").term("vcz%%%%%"), docs, costs, rdr);
  }

  {
    docs_t docs{ 1, 4, 21, 26, 31, 32 };
    costs_t costs{ docs.size() };
    check_query(irs::by_wildcard().field("prefix").term("abc%"), docs, costs, rdr);
  }

  {
    docs_t docs{ 1, 4, 21, 26, 31, 32 };
    costs_t costs{ docs.size() };

    check_query(irs::by_wildcard().field("prefix").term("abc%"), docs, costs, rdr);
    check_query(irs::by_wildcard().field("prefix").term("abc%%"), docs, costs, rdr);
  }

  // whole word
  check_query(irs::by_wildcard().field("prefix").term("bateradsfsfasdf"), docs_t{24}, costs_t{1}, rdr);
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
    ::testing::Values("1_0", "1_1", "1_2")
  ),
  tests::to_string
);

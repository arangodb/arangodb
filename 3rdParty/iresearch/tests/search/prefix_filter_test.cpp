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
#include "search/prefix_filter.hpp"

NS_LOCAL

class prefix_filter_test_case : public tests::filter_test_case_base {
 protected:
  void by_prefix_order() {
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
      check_query(irs::by_prefix().field("prefix"), order, docs, rdr);
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

  void by_prefix_sequential() {
    /* add segment */
    {
      tests::json_doc_generator gen(
        resource("simple_sequential.json"),
        &tests::generic_json_field_factory);
      add_segment( gen );
    }

    auto rdr = open_reader();

    // empty query
    check_query(irs::by_prefix(), docs_t{}, costs_t{0}, rdr);

    // empty field
    check_query(irs::by_prefix().term("xyz"), docs_t{}, costs_t{0}, rdr);

    // invalid field
    check_query(irs::by_prefix().field("same1").term("xyz"), docs_t{}, costs_t{0}, rdr);

    // invalid prefix
    check_query(irs::by_prefix().field("same").term("xyz_invalid"), docs_t{}, costs_t{0}, rdr);

    // valid prefix
    {
      docs_t result;
      for(size_t i = 0; i < 32; ++i) {
        result.push_back(irs::doc_id_t((irs::type_limits<irs::type_t::doc_id_t>::min)() + i));
      }

      costs_t costs{ result.size() };

      check_query(irs::by_prefix().field("same").term("xyz"), result, costs, rdr);
    }

    // empty prefix : get all fields
    {
      docs_t docs{ 1, 2, 3, 5, 8, 11, 14, 17, 19, 21, 24, 27, 31 };
      costs_t costs{ docs.size() };

      check_query(irs::by_prefix().field("duplicated"), docs, costs, rdr);
    }

    // single digit prefix
    {
      docs_t docs{ 1, 5, 11, 21, 27, 31 };
      costs_t costs{ docs.size() };

      check_query(irs::by_prefix().field("duplicated").term("a"), docs, costs, rdr);
    }

    check_query(irs::by_prefix().field("name").term("!"), docs_t{28}, costs_t{1}, rdr);
    check_query(irs::by_prefix().field("prefix").term("b"), docs_t{9, 24}, costs_t{2}, rdr);

    // multiple digit prefix
    {
      docs_t docs{ 2, 3, 8, 14, 17, 19, 24 };
      costs_t costs{ docs.size() };

      check_query(irs::by_prefix().field("duplicated").term("vcz"), docs, costs, rdr);
    }

    {
      docs_t docs{ 1, 4, 21, 26, 31, 32 };
      costs_t costs{ docs.size() };
      check_query(irs::by_prefix().field("prefix").term("abc"), docs, costs, rdr);
    }

    {
      docs_t docs{ 1, 4, 21, 26, 31, 32 };
      costs_t costs{ docs.size() };

      check_query(irs::by_prefix().field("prefix").term("abc"), docs, costs, rdr);
    }

    // whole word
    check_query(irs::by_prefix().field("prefix").term("bateradsfsfasdf"), docs_t{24}, costs_t{1}, rdr);
  }

  void by_prefix_schemas() { 
    // write segments
    {
      auto writer = open_writer(irs::OM_CREATE);

      std::vector<tests::doc_generator_base::ptr> gens;
      gens.emplace_back(new tests::json_doc_generator(
        resource("AdventureWorks2014.json"),
        &tests::generic_json_field_factory
      ));
      gens.emplace_back(new tests::json_doc_generator(
        resource("AdventureWorks2014Edges.json"),
        &tests::generic_json_field_factory
      ));
      gens.emplace_back(new tests::json_doc_generator(
        resource("Northwnd.json"),
        &tests::generic_json_field_factory
      ));
      gens.emplace_back(new tests::json_doc_generator(
        resource("NorthwndEdges.json"),
        &tests::generic_json_field_factory
      ));
      add_segments(*writer, gens);
    }

    auto rdr = open_reader();

    check_query(irs::by_prefix().field("Name").term("Addr"), docs_t{1, 2, 77, 78}, rdr);
  }
}; // filter_test_case_base

TEST(by_prefix_test, ctor) {
  irs::by_prefix q;
  ASSERT_EQ(irs::by_prefix::type(), q.type());
  ASSERT_EQ("", q.field());
  ASSERT_TRUE(q.term().empty());
  ASSERT_EQ(irs::no_boost(), q.boost());
  ASSERT_EQ(1024, q.scored_terms_limit());
}

TEST(by_prefix_test, equal) {
  {
    irs::by_prefix q;
    q.field("field").term("term");

    ASSERT_EQ(q, irs::by_prefix().field("field").term("term"));
    ASSERT_EQ(q.hash(), irs::by_prefix().field("field").term("term").hash());
    ASSERT_NE(q, irs::by_prefix().field("field1").term("term"));
    ASSERT_NE(q, irs::by_prefix().scored_terms_limit(100).field("field").term("term"));
    ASSERT_NE(q, irs::by_term().field("field").term("term"));
  }

  {
    irs::by_prefix q;
    q.scored_terms_limit(100).field("field").term("term");

    ASSERT_EQ(q, irs::by_prefix().scored_terms_limit(100).field("field").term("term"));
    ASSERT_EQ(q.hash(), irs::by_prefix().scored_terms_limit(100).field("field").term("term").hash());
    ASSERT_NE(q, irs::by_prefix().scored_terms_limit(100).field("field1").term("term"));
    ASSERT_NE(q, irs::by_prefix().field("field").term("term"));
    ASSERT_NE(q, irs::by_term().field("field").term("term"));
  }
}

TEST(by_prefix_test, boost) {
  // no boost
  {
    irs::by_prefix q;
    q.field("field").term("term");

    auto prepared = q.prepare(irs::sub_reader::empty());
    ASSERT_EQ(irs::no_boost(), prepared->boost());
  }

  // with boost
  {
    irs::boost_t boost = 1.5f;
    irs::by_prefix q;
    q.field("field").term("term");
    q.boost(boost);

    auto prepared = q.prepare(irs::sub_reader::empty());
    ASSERT_EQ(boost, prepared->boost());
  }
}

TEST_P(prefix_filter_test_case, by_prefix) {
  by_prefix_order();
  by_prefix_sequential();
  by_prefix_schemas();
}

INSTANTIATE_TEST_CASE_P(
  prefix_filter_test,
  prefix_filter_test_case,
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

NS_END

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

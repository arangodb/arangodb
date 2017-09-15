//
// IResearch search engine 
// 
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

#include "tests_shared.hpp"
#include "filter_test_case_base.hpp"
#include "search/prefix_filter.hpp"
#include "store/memory_directory.hpp"
#include "formats/formats_10.hpp"

namespace ir = iresearch;

namespace tests {

class prefix_filter_test_case : public filter_test_case_base {
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
    check_query(ir::by_prefix(), docs_t{}, costs_t{0}, rdr);

    // empty prefix test collector call count for field/term/finish
    {
      docs_t docs{ 1, 4, 9, 16, 21, 24, 26, 29, 31, 32 };
      costs_t costs{ docs.size() };
      irs::order order;

      size_t collect_count = 0;
      size_t finish_count = 0;
      auto& scorer = order.add<sort::custom_sort>();
      scorer.collector_collect = [&collect_count](const irs::sub_reader&, const irs::term_reader&, const irs::attribute_view&)->void{
        ++collect_count;
      };
      scorer.collector_finish = [&finish_count](irs::attribute_store&, const irs::index_reader&)->void{
        ++finish_count;
      };
      scorer.prepare_collector = [&scorer]()->irs::sort::collector::ptr{
        return irs::memory::make_unique<sort::custom_sort::prepared::collector>(scorer);
      };
      check_query(irs::by_prefix().field("prefix"), order, docs, rdr);
      ASSERT_EQ(9, collect_count);
      ASSERT_EQ(9, finish_count); // 9 unque terms
    }

    // empty prefix
    {
      docs_t docs{ 31, 32, 1, 4, 9, 16, 21, 24, 26, 29 };
      costs_t costs{ docs.size() };
      ir::order order;

      order.add<sort::frequency_sort>();
      check_query(ir::by_prefix().field("prefix"), order, docs, rdr);
    }

    // empty prefix + scored_terms_limit
    {
      docs_t docs{ 31, 32, 1, 4, 9, 16, 21, 24, 26, 29 };
      costs_t costs{ docs.size() };
      ir::order order;

      order.add<sort::frequency_sort>();
      check_query(ir::by_prefix().field("prefix").scored_terms_limit(1), order, docs, rdr);
    }

    // prefix
    {
      docs_t docs{ 31, 32, 1, 4, 16, 21, 26, 29 };
      costs_t costs{ docs.size() };
      ir::order order;

      order.add<sort::frequency_sort>();
      check_query(ir::by_prefix().field("prefix").term("a"), order, docs, rdr);
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
    check_query(ir::by_prefix(), docs_t{}, costs_t{0}, rdr);

    // empty field
    check_query(ir::by_prefix().term("xyz"), docs_t{}, costs_t{0}, rdr);

    // invalid field
    check_query(ir::by_prefix().field("same1").term("xyz"), docs_t{}, costs_t{0}, rdr);

    // invalid prefix
    check_query(ir::by_prefix().field("same").term("xyz_invalid"), docs_t{}, costs_t{0}, rdr);

    // valid prefix
    {
      docs_t result;
      for(size_t i = 0; i < 32; ++i) {
        result.push_back(ir::doc_id_t((ir::type_limits<ir::type_t::doc_id_t>::min)() + i));
      }

      costs_t costs{ result.size() };

      check_query(ir::by_prefix().field("same").term("xyz"), result, costs, rdr);
    }

    // empty prefix : get all fields
    {
      docs_t docs{ 1, 2, 3, 5, 8, 11, 14, 17, 19, 21, 24, 27, 31 };
      costs_t costs{ docs.size() };

      check_query(ir::by_prefix().field("duplicated"), docs, costs, rdr);
    }

    // single digit prefix
    {
      docs_t docs{ 1, 5, 11, 21, 27, 31 };
      costs_t costs{ docs.size() };

      check_query(ir::by_prefix().field("duplicated").term("a"), docs, costs, rdr);
    }

    check_query(ir::by_prefix().field("name").term("!"), docs_t{28}, costs_t{1}, rdr);
    check_query(ir::by_prefix().field("prefix").term("b"), docs_t{9, 24}, costs_t{2}, rdr);

    // multiple digit prefix
    {
      docs_t docs{ 2, 3, 8, 14, 17, 19, 24 };
      costs_t costs{ docs.size() };

      check_query(ir::by_prefix().field("duplicated").term("vcz"), docs, costs, rdr);
    }

    {
      docs_t docs{ 1, 4, 21, 26, 31, 32 };
      costs_t costs{ docs.size() };
      check_query(ir::by_prefix().field("prefix").term("abc"), docs, costs, rdr);
    }

    {
      docs_t docs{ 1, 4, 21, 26, 31, 32 };
      costs_t costs{ docs.size() };

      check_query(ir::by_prefix().field("prefix").term("abc"), docs, costs, rdr);
    }

    // whole word
    check_query(ir::by_prefix().field("prefix").term("bateradsfsfasdf"), docs_t{24}, costs_t{1}, rdr);
  }

  void by_prefix_schemas() { 
    // write segments
    {
      auto writer = open_writer(iresearch::OM_CREATE);

      std::vector<doc_generator_base::ptr> gens;
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

    check_query(ir::by_prefix().field("Name").term("Addr"), docs_t{1, 2, 77, 78}, rdr);
  }
}; // filter_test_case_base

} // tests

// ----------------------------------------------------------------------------
// --SECTION--                                             by_prefix base tests
// ----------------------------------------------------------------------------

TEST(by_prefix_test, ctor) {
  ir::by_prefix q;
  ASSERT_EQ(ir::by_prefix::type(), q.type());
  ASSERT_EQ("", q.field());
  ASSERT_TRUE(q.term().empty());
  ASSERT_EQ(ir::boost::no_boost(), q.boost());
  ASSERT_EQ(1024, q.scored_terms_limit());
}

TEST(by_prefix_test, equal) {
  {
    ir::by_prefix q;
    q.field("field").term("term");

    ASSERT_EQ(q, ir::by_prefix().field("field").term("term"));
    ASSERT_EQ(q.hash(), ir::by_prefix().field("field").term("term").hash());
    ASSERT_NE(q, ir::by_prefix().field("field1").term("term"));
    ASSERT_NE(q, ir::by_prefix().scored_terms_limit(100).field("field").term("term"));
    ASSERT_NE(q, ir::by_term().field("field").term("term"));
  }

  {
    ir::by_prefix q;
    q.scored_terms_limit(100).field("field").term("term");

    ASSERT_EQ(q, ir::by_prefix().scored_terms_limit(100).field("field").term("term"));
    ASSERT_EQ(q.hash(), ir::by_prefix().scored_terms_limit(100).field("field").term("term").hash());
    ASSERT_NE(q, ir::by_prefix().scored_terms_limit(100).field("field1").term("term"));
    ASSERT_NE(q, ir::by_prefix().field("field").term("term"));
    ASSERT_NE(q, ir::by_term().field("field").term("term"));
  }
}

TEST(by_prefix_test, boost) {
  // no boost
  {
    ir::by_prefix q;
    q.field("field").term("term");

    auto prepared = q.prepare(tests::empty_index_reader::instance());
    ASSERT_EQ(ir::boost::no_boost(), ir::boost::extract(prepared->attributes()));
  }

  // with boost
  {
    iresearch::boost::boost_t boost = 1.5f;
    ir::by_prefix q;
    q.field("field").term("term");
    q.boost(boost);

    auto prepared = q.prepare(tests::empty_index_reader::instance());
    ASSERT_EQ(boost, ir::boost::extract(prepared->attributes()));
  }
}

// ----------------------------------------------------------------------------
// --SECTION--                           memory_directory + iresearch_format_10
// ----------------------------------------------------------------------------

class memory_prefix_filter_test_case : public tests::prefix_filter_test_case {
protected:
  virtual ir::directory* get_directory() override {
    return new ir::memory_directory();
  }

  virtual ir::format::ptr get_codec() override {
    static ir::version10::format FORMAT;
    return ir::format::ptr(&FORMAT, [](ir::format*)->void{});
  }
};

TEST_F(memory_prefix_filter_test_case, by_prefix) {
  by_prefix_order();
  by_prefix_sequential();
  by_prefix_schemas();
}

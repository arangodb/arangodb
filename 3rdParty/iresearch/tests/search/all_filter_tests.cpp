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
#include "search/all_filter.hpp"
#include "store/memory_directory.hpp"
#include "store/fs_directory.hpp"
#include "formats/formats.hpp"
#include "search/score.hpp"

namespace ir = iresearch;

namespace tests {

class all_filter_test_case : public filter_test_case_base {
protected:
  void all_sequential() {
    // add segment
    {
      tests::json_doc_generator gen(
         resource("simple_sequential.json"),
         &tests::generic_json_field_factory);
      add_segment( gen );
    }

    auto rdr = open_reader();

    docs_t docs{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 };
    std::vector<irs::cost::cost_t> cost{ docs.size() };

    check_query(ir::all(), docs, cost, rdr);
  }

  void all_order() {
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
      docs_t docs{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 };

      check_query(irs::all(), docs, costs_t{docs.size()}, rdr);
    }

    // custom order
    {
      docs_t docs{ 1, 4, 5, 16, 17, 20, 21, 2, 3, 6, 7, 18, 19, 22, 23, 8, 9, 12, 13, 24, 25, 28, 29, 10, 11, 14, 15, 26, 27, 30, 31, 32 };
      irs::order order;
      size_t collector_collect_count = 0;
      size_t collector_finish_count = 0;
      size_t scorer_score_count = 0;
      auto& sort = order.add<sort::custom_sort>();

      sort.collector_collect = [&collector_collect_count](const irs::sub_reader&, const irs::term_reader&, const irs::attribute_view&)->void {
        ++collector_collect_count;
      };
      sort.collector_finish = [&collector_finish_count](irs::attribute_store&, const irs::index_reader&)->void {
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
      ASSERT_EQ(0, collector_collect_count); // should not be executed
      ASSERT_EQ(1, collector_finish_count);
      ASSERT_EQ(32, scorer_score_count);
    }

    // custom order (no scorer)
    {
      docs_t docs{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 };
      irs::order order;
      auto& sort = order.add<sort::custom_sort>();

      sort.prepare_collector = []()->irs::sort::collector::ptr { return nullptr; };
      sort.prepare_scorer = [](const irs::sub_reader&, const irs::term_reader&, const irs::attribute_store&, const irs::attribute_view&)->irs::sort::scorer::ptr { return nullptr; };

      check_query(irs::all(), order, docs, rdr);
    }

    // frequency order
    {
      docs_t docs{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 };
      irs::order order;

      order.add<sort::frequency_sort>();
      check_query(irs::all(), order, docs, rdr);
    }

    // bm25 order
    {
      docs_t docs{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 };
      irs::order order;

      order.add(irs::scorers::get("bm25", irs::string_ref::nil));
      check_query(irs::all(), order, docs, rdr);
    }

    // tfidf order
    {
      docs_t docs{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 };
      irs::order order;

      order.add(irs::scorers::get("tfidf", irs::string_ref::nil));
      check_query(irs::all(), order, docs, rdr);
    }
  }
}; // all_filter_test_case

} // tests 

// ----------------------------------------------------------------------------
// --SECTION--                           memory_directory + iresearch_format_10
// ----------------------------------------------------------------------------

class memory_all_filter_test_case : public tests::all_filter_test_case {
protected:
  virtual ir::directory* get_directory() override {
    return new ir::memory_directory();
  }

  virtual ir::format::ptr get_codec() override {
    return ir::formats::get("1_0");
  }
}; // memory_all_filter_test_case

TEST_F( memory_all_filter_test_case, all ) {
  all_sequential();
  all_order();
}

// ----------------------------------------------------------------------------
// --SECTION--                               fs_directory + iresearch_format_10
// ----------------------------------------------------------------------------

class fs_all_filter_test_case : public tests::all_filter_test_case {
protected:
  virtual ir::directory* get_directory() override {
    const fs::path dir = fs::path( test_dir() ).append( "index" );
    return new iresearch::fs_directory(dir.string());
  }

  virtual ir::format::ptr get_codec() override {
    return ir::formats::get("1_0");
  }
}; // fs_all_filter_test_case

TEST_F( fs_all_filter_test_case, all ) {
  all_sequential();
  all_order();
}

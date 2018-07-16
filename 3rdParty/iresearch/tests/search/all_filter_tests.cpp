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
#include "store/memory_directory.hpp"
#include "store/fs_directory.hpp"
#include "formats/formats.hpp"
#include "search/score.hpp"

NS_BEGIN(tests)

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

    check_query(irs::all(), docs, cost, rdr);
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
      auto& sort = order.add<sort::custom_sort>(false);

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
      auto& sort = order.add<sort::custom_sort>(false);

      sort.prepare_collector = []()->irs::sort::collector::ptr { return nullptr; };
      sort.prepare_scorer = [](const irs::sub_reader&, const irs::term_reader&, const irs::attribute_store&, const irs::attribute_view&)->irs::sort::scorer::ptr { return nullptr; };

      check_query(irs::all(), order, docs, rdr);
    }

    // frequency order
    {
      docs_t docs{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 };
      irs::order order;

      order.add<sort::frequency_sort>(false);
      check_query(irs::all(), order, docs, rdr);
    }

    // bm25 order
    {
      docs_t docs{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 };
      irs::order order;

      order.add(true, irs::scorers::get("bm25", irs::text_format::json, irs::string_ref::NIL));
      check_query(irs::all(), order, docs, rdr);
    }

    // tfidf order
    {
      docs_t docs{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 };
      irs::order order;

      order.add(true, irs::scorers::get("tfidf", irs::text_format::json, irs::string_ref::NIL));
      check_query(irs::all(), order, docs, rdr);
    }
  }
}; // all_filter_test_case

NS_END // tests

// ----------------------------------------------------------------------------
// --SECTION--                           memory_directory + iresearch_format_10
// ----------------------------------------------------------------------------

class memory_all_filter_test_case : public tests::all_filter_test_case {
protected:
  virtual irs::directory* get_directory() override {
    return new irs::memory_directory();
  }

  virtual irs::format::ptr get_codec() override {
    return irs::formats::get("1_0");
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
  virtual irs::directory* get_directory() override {
    auto dir = test_dir();

    dir /= "index";

    return new iresearch::fs_directory(dir.utf8());
  }

  virtual irs::format::ptr get_codec() override {
    return irs::formats::get("1_0");
  }
}; // fs_all_filter_test_case

TEST_F( fs_all_filter_test_case, all ) {
  all_sequential();
  all_order();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
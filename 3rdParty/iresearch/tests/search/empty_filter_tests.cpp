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
#include "store/memory_directory.hpp"
#include "store/fs_directory.hpp"

NS_LOCAL

class empty_filter_test_case: public tests::filter_test_case_base {
 protected:
  void empty_sequential() {
    // add segment
    {
      tests::json_doc_generator gen(
         resource("simple_sequential.json"),
         &tests::generic_json_field_factory);
      add_segment(gen);
    }

    auto rdr = open_reader();

    std::vector<irs::cost::cost_t> cost{ 0 };

    check_query(irs::empty(), docs_t{}, cost, rdr);
  }
};

NS_END

// ----------------------------------------------------------------------------
// --SECTION--                           memory_directory + iresearch_format_10
// ----------------------------------------------------------------------------

class memory_empty_filter_test_case: public empty_filter_test_case {
 protected:
  virtual irs::directory* get_directory() override {
    return new irs::memory_directory();
  }

  virtual irs::format::ptr get_codec() override {
    return irs::formats::get("1_0");
  }
};

TEST_F(memory_empty_filter_test_case, empty) {
  empty_sequential();
}

// ----------------------------------------------------------------------------
// --SECTION--                               fs_directory + iresearch_format_10
// ----------------------------------------------------------------------------

class fs_empty_filter_test_case: public empty_filter_test_case {
 protected:
  virtual irs::directory* get_directory() override {
    auto dir = fs::path(test_dir()).append("index");

    return new irs::fs_directory(dir.string());
  }

  virtual irs::format::ptr get_codec() override {
    return irs::formats::get("1_0");
  }
};

TEST_F(fs_empty_filter_test_case, empty) {
  empty_sequential();
}

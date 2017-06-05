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
#include "formats/formats.hpp"

namespace ir = iresearch;

namespace tests {

class all_filter_test_case : public filter_test_case_base {
protected:
  void all_sequential() {
    /* add segment */
    {
      tests::json_doc_generator gen(
         resource("simple_sequential.json"),
         &tests::generic_json_field_factory);
      add_segment( gen );
    }

    auto rdr = open_reader();

    check_query(
      ir::all( ),
      docs_t{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
      rdr
    );
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
}

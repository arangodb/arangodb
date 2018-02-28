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
    auto dir = test_dir();

    dir /= "index";

    return new irs::fs_directory(dir.utf8());
  }

  virtual irs::format::ptr get_codec() override {
    return irs::formats::get("1_0");
  }
};

TEST_F(fs_empty_filter_test_case, empty) {
  empty_sequential();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
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

#include "filter_test_case_base.hpp"
#include "tests_shared.hpp"

namespace {

class empty_filter_test_case : public tests::FilterTestCaseBase {};

TEST_P(empty_filter_test_case, empty) {
  // add segment
  {
    tests::json_doc_generator gen(resource("simple_sequential.json"),
                                  &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  std::vector<irs::cost::cost_t> cost{0};

  CheckQuery(irs::Empty{}, Docs{}, cost, rdr);
}

static constexpr auto kTestDirs = tests::getDirectories<tests::kTypesDefault>();

INSTANTIATE_TEST_SUITE_P(empty_filter_test, empty_filter_test_case,
                         ::testing::Combine(::testing::ValuesIn(kTestDirs),
                                            ::testing::Values("1_0")),
                         empty_filter_test_case::to_string);

}  // namespace

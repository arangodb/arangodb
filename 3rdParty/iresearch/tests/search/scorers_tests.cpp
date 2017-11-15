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
#include "search/scorers.hpp"

TEST(scorers_tests, duplicate_register) {
  struct dummy_scorer: public irs::sort {
    DECLARE_SORT_TYPE() { static irs::sort::type_id type("dummy_scorer"); return type; }
    static ptr make(const irs::string_ref&) { return nullptr; }
    dummy_scorer(): irs::sort(dummy_scorer::type()) { }
  };

  static bool initial_expected = true;
  irs::scorer_registrar initial(dummy_scorer::type(), &dummy_scorer::make);
  ASSERT_EQ(!initial_expected, !initial);
  initial_expected = false; // next test iteration will not be able to register the same scorer
  irs::scorer_registrar duplicate(dummy_scorer::type(), &dummy_scorer::make);
  ASSERT_TRUE(!duplicate);
}

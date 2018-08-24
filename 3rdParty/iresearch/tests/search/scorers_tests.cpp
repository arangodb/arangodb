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
    static ptr make(const irs::string_ref&) { return ptr(new dummy_scorer()); }
    dummy_scorer(): irs::sort(dummy_scorer::type()) { }

    prepared::ptr prepare() const { return nullptr; }
  };

  static bool initial_expected = true;

  // check required for tests with repeat (static maps are not cleared between runs)
  if (initial_expected) {
    ASSERT_FALSE(irs::scorers::exists("dummy_scorer", irs::text_format::csv));
    ASSERT_FALSE(irs::scorers::exists("dummy_scorer", irs::text_format::json));
    ASSERT_FALSE(irs::scorers::exists("dummy_scorer", irs::text_format::text));
    ASSERT_FALSE(irs::scorers::exists("dummy_scorer", irs::text_format::xml));
    ASSERT_EQ(nullptr, irs::scorers::get("dummy_scorer", irs::text_format::csv, irs::string_ref::NIL));
    ASSERT_EQ(nullptr, irs::scorers::get("dummy_scorer", irs::text_format::json, irs::string_ref::NIL));
    ASSERT_EQ(nullptr, irs::scorers::get("dummy_scorer", irs::text_format::text, irs::string_ref::NIL));
    ASSERT_EQ(nullptr, irs::scorers::get("dummy_scorer", irs::text_format::xml, irs::string_ref::NIL));

    irs::scorer_registrar initial0(dummy_scorer::type(), irs::text_format::csv, &dummy_scorer::make);
    irs::scorer_registrar initial1(dummy_scorer::type(), irs::text_format::json, &dummy_scorer::make);
    irs::scorer_registrar initial2(dummy_scorer::type(), irs::text_format::text, &dummy_scorer::make);
    irs::scorer_registrar initial3(dummy_scorer::type(), irs::text_format::xml, &dummy_scorer::make);
    ASSERT_EQ(!initial_expected, !initial0);
    ASSERT_EQ(!initial_expected, !initial1);
    ASSERT_EQ(!initial_expected, !initial2);
    ASSERT_EQ(!initial_expected, !initial3);
  }

  initial_expected = false; // next test iteration will not be able to register the same scorer
  irs::scorer_registrar duplicate0(dummy_scorer::type(), irs::text_format::csv, &dummy_scorer::make);
  irs::scorer_registrar duplicate1(dummy_scorer::type(), irs::text_format::json, &dummy_scorer::make);
  irs::scorer_registrar duplicate2(dummy_scorer::type(), irs::text_format::text, &dummy_scorer::make);
  irs::scorer_registrar duplicate3(dummy_scorer::type(), irs::text_format::xml, &dummy_scorer::make);
  ASSERT_TRUE(!duplicate0);
  ASSERT_TRUE(!duplicate1);
  ASSERT_TRUE(!duplicate2);
  ASSERT_TRUE(!duplicate3);

  ASSERT_TRUE(irs::scorers::exists("dummy_scorer", irs::text_format::csv));
  ASSERT_TRUE(irs::scorers::exists("dummy_scorer", irs::text_format::json));
  ASSERT_TRUE(irs::scorers::exists("dummy_scorer", irs::text_format::text));
  ASSERT_TRUE(irs::scorers::exists("dummy_scorer", irs::text_format::xml));
  ASSERT_NE(nullptr, irs::scorers::get("dummy_scorer", irs::text_format::csv, irs::string_ref::NIL));
  ASSERT_NE(nullptr, irs::scorers::get("dummy_scorer", irs::text_format::json, irs::string_ref::NIL));
  ASSERT_NE(nullptr, irs::scorers::get("dummy_scorer", irs::text_format::text, irs::string_ref::NIL));
  ASSERT_NE(nullptr, irs::scorers::get("dummy_scorer", irs::text_format::xml, irs::string_ref::NIL));
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
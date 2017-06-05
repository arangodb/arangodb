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

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
#include "formats/formats.hpp"

TEST(formats_tests, duplicate_register) {
  struct dummy_format: public irs::format {
    DECLARE_FORMAT_TYPE() { static irs::format::type_id type("dummy_format"); return type; }
    static ptr make() { return nullptr; }
    dummy_format(): irs::format(dummy_format::type()) { }
  };

  static bool initial_expected = true;
  irs::format_registrar initial(dummy_format::type(), &dummy_format::make);
  ASSERT_EQ(!initial_expected, !initial);
  initial_expected = false; // next test iteration will not be able to register the same format
  irs::format_registrar duplicate(dummy_format::type(), &dummy_format::make);
  ASSERT_TRUE(!duplicate);
}

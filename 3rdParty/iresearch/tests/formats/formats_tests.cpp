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

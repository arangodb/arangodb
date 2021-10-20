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

#include "utils/attributes.hpp"

using namespace iresearch;

TEST(attributes_tests, duplicate_register) {
  struct dummy_attribute: public irs::attribute { };

  static bool initial_expected = true;

  // check required for tests with repeat (static maps are not cleared between runs)
  if (initial_expected) {
    ASSERT_FALSE(irs::attributes::exists(irs::type<dummy_attribute>::name()));
    ASSERT_FALSE(irs::attributes::get(irs::type<dummy_attribute>::get().name()));

    irs::attribute_registrar initial(irs::type<dummy_attribute>::get());
    ASSERT_EQ(!initial_expected, !initial);
  }

  initial_expected = false; // next test iteration will not be able to register the same attribute
  irs::attribute_registrar duplicate(irs::type<dummy_attribute>::get());
  ASSERT_TRUE(!duplicate);

  ASSERT_TRUE(irs::attributes::exists(irs::type<dummy_attribute>::get().name()));
  ASSERT_TRUE(irs::attributes::get(irs::type<dummy_attribute>::name()));
}

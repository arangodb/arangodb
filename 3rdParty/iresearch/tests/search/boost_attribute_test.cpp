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
#include "search/filter.hpp"

TEST(boost_attribute_test, consts) {
  ASSERT_EQ(1, irs::boost::boost_t(irs::boost::no_boost()));
}

TEST(boost_attribute_test, add_clear) {
  irs::attribute_store attrs;
  auto& boost = attrs.emplace<irs::boost>();
  ASSERT_FALSE(!boost);
  ASSERT_EQ(irs::boost::boost_t(irs::boost::no_boost()), boost->value);
  boost->value = 5;
  boost->clear();
  ASSERT_EQ(irs::boost::boost_t(irs::boost::no_boost()), boost->value);
}

TEST(boost_attribute_test, apply_extract) {
  irs::attribute_store attrs;
  ASSERT_EQ(irs::boost::boost_t(irs::boost::no_boost()), irs::boost::extract(attrs));

  irs::boost::boost_t value = 5;
  {
    irs::boost::apply(attrs, value);
    ASSERT_EQ(value, irs::boost::extract(attrs));
    auto& boost = const_cast<const irs::attribute_store&>(attrs).get<irs::boost>();
    ASSERT_FALSE(!boost);
    ASSERT_EQ(value, boost->value);
  }

  {
    irs::boost::boost_t new_value = 15;
    irs::boost::apply(attrs, new_value);
    ASSERT_EQ(value*new_value, irs::boost::extract(attrs));
    auto& boost = const_cast<const irs::attribute_store&>(attrs).get<irs::boost>();
    ASSERT_FALSE(!boost);
    ASSERT_EQ(value*new_value, boost->value);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
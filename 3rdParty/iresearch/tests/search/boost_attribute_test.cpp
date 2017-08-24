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
#include "search/filter.hpp"

namespace ir = iresearch;

TEST(boost_attribute_test, consts) {
  ASSERT_EQ(1, ir::boost::boost_t(ir::boost::no_boost()));
}

TEST(boost_attribute_test, add_clear) {
  irs::attribute_store attrs;
  auto& boost = attrs.emplace<irs::boost>();
  ASSERT_FALSE(!boost);
  ASSERT_EQ(ir::boost::boost_t(ir::boost::no_boost()), boost->value);
  boost->value = 5;
  boost->clear();
  ASSERT_EQ(ir::boost::boost_t(ir::boost::no_boost()), boost->value);
}

TEST(boost_attribute_test, apply_extract) {
  irs::attribute_store attrs;
  ASSERT_EQ(ir::boost::boost_t(ir::boost::no_boost()), ir::boost::extract(attrs));

  ir::boost::boost_t value = 5;
  {
    ir::boost::apply(attrs, value);
    ASSERT_EQ(value, ir::boost::extract(attrs));
    auto& boost = const_cast<const irs::attribute_store&>(attrs).get<irs::boost>();
    ASSERT_FALSE(!boost);
    ASSERT_EQ(value, boost->value);
  }

  {
    ir::boost::boost_t new_value = 15;
    ir::boost::apply(attrs, new_value);
    ASSERT_EQ(value*new_value, ir::boost::extract(attrs));
    auto& boost = const_cast<const irs::attribute_store&>(attrs).get<irs::boost>();
    ASSERT_FALSE(!boost);
    ASSERT_EQ(value*new_value, boost->value);
  }
}

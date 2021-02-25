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
#include "filter_test_case_base.hpp"

#include <limits>

TEST(cost_attribute_test, consts) {
  static_assert("iresearch::cost" == irs::type<irs::cost>::name());
  static_assert((std::numeric_limits<irs::cost::cost_t>::max)() == irs::cost::MAX);
}

TEST(cost_attribute_test, ctor) {
  irs::cost cost;
  ASSERT_EQ(0, cost.estimate());
}

TEST(cost_attribute_test, estimation) {
  irs::cost cost;
  ASSERT_EQ(0, cost.estimate());

  // explicit estimation
  {
    auto est = 7;

    // set estimation value and check
    {
      cost.value(est);
      ASSERT_EQ(est, cost.estimate());
    }
  }
  
  // implicit estimation
  {
    auto evaluated = false;
    auto est = 7;
  
    cost.rule([&evaluated, est]() {
      evaluated = true;
      return est;
    });
    ASSERT_FALSE(evaluated);
    ASSERT_EQ(est, cost.estimate());
    ASSERT_TRUE(evaluated);
  }
}

TEST(cost_attribute_test, lazy_estimation) {
  irs::cost cost;
  ASSERT_EQ(0, cost.estimate());

  auto evaluated = false;
  auto est = 7;

  // set estimation function and evaluate
  {
    evaluated = false;
    cost.rule([&evaluated, est]() {
      evaluated = true;
      return est;
    });
    ASSERT_FALSE(evaluated);
    ASSERT_EQ(est, cost.estimate());
    ASSERT_TRUE(evaluated);
  }

  // ensure value is cached
  {
    evaluated = false;
    ASSERT_EQ(est, cost.estimate());
    ASSERT_FALSE(evaluated);
  }

  // change estimation func
  {
    evaluated = false;
    cost.rule([&evaluated, est]() {
      evaluated = true;
      return est+1;
    });
    ASSERT_FALSE(evaluated);
    ASSERT_EQ(est+1, cost.estimate());
    ASSERT_TRUE(evaluated);
  }

  // set value directly
  {
    evaluated = false;
    cost.value(est+2);
    ASSERT_FALSE(evaluated);
    ASSERT_EQ(est+2, cost.estimate());
    ASSERT_FALSE(evaluated);
  }
}

TEST(cost_attribute_test, extract) {
  struct basic_attribute_provider : irs::attribute_provider {
    irs::attribute* get_mutable(irs::type_info::type_id type) noexcept {
      return type == irs::type<irs::cost>::id()
        ? cost : nullptr;
    }

    irs::cost* cost{};
  } attrs;

  ASSERT_EQ(
    irs::cost::cost_t(irs::cost::MAX),
    irs::cost::extract(attrs)
  );

  ASSERT_EQ(5, irs::cost::extract(attrs, 5));

  irs::cost cost;
  attrs.cost = &cost;

  auto est = 7;
  auto evaluated = false;

  // set estimation function and evaluate
  {
    cost.rule([&evaluated, est]() {
      evaluated = true;
      return est;
    });
    ASSERT_FALSE(evaluated);
    ASSERT_EQ(est, irs::cost::extract(attrs));
    ASSERT_TRUE(evaluated);
  }

  // change estimation func
  {
    evaluated = false;
    cost.rule([&evaluated, est]() {
      evaluated = true;
      return est+1;
    });
    ASSERT_FALSE(evaluated);
    ASSERT_EQ(est+1, irs::cost::extract(attrs, 3));
    ASSERT_TRUE(evaluated);
  }

  // clear
  {
    evaluated = false;
    ASSERT_EQ(est+1, irs::cost::extract(attrs, 3));
    ASSERT_FALSE(evaluated);
  }
}

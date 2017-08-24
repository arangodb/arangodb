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
#include "filter_test_case_base.hpp"

#include <limits>

namespace ir = iresearch;

TEST(cost_attribute_test, consts) {
  ASSERT_EQ(
    (std::numeric_limits<ir::cost::cost_t>::max)(),
    ir::cost::cost_t(ir::cost::MAX)
  );
}

TEST(cost_attribute_test, estimation) {
  irs::cost cost;
  ASSERT_FALSE(bool(cost.rule()));

  // explicit estimation
  {
    auto est = 7;

    // set estimation value and check
    {
      cost.value(est);
      ASSERT_TRUE(bool(cost.rule()));
      ASSERT_EQ(est, cost.estimate());
      ASSERT_EQ(est, cost.rule()());
    }

    // clear
    {
      cost.clear();
      ASSERT_TRUE(bool(cost.rule()));
      ASSERT_EQ(est, cost.estimate());
      ASSERT_EQ(est, cost.rule()());
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
    ASSERT_TRUE(bool(cost.rule()));
    ASSERT_FALSE(evaluated);
    ASSERT_EQ(est, cost.estimate());
    ASSERT_TRUE(evaluated);
  }
}

TEST(cost_attribute_test, lazy_estimation) {
  irs::cost cost;
  ASSERT_FALSE(bool(cost.rule()));

  auto evaluated = false;
  auto est = 7;

  /* set estimation function and evaluate */
  {
    cost.rule([&evaluated, est]() {
      evaluated = true;
      return est;
    });
    ASSERT_TRUE(bool(cost.rule()));
    ASSERT_FALSE(evaluated);
    ASSERT_EQ(est, cost.estimate());
    ASSERT_TRUE(evaluated);
  }

  /* change estimation func */
  {
    evaluated = false;
    cost.rule([&evaluated, est]() {
      evaluated = true;
      return est+1;
    });
    ASSERT_TRUE(bool(cost.rule()));
    ASSERT_FALSE(evaluated);
    ASSERT_EQ(est+1, cost.estimate());
    ASSERT_TRUE(evaluated);
  }

  /* clear */
  {
    evaluated = false;
    cost.clear();
    ASSERT_EQ(est+1, cost.estimate());
    /* evaluate again */
    ASSERT_TRUE(evaluated);
  }
}

TEST(cost_attribute_test, extract) {
  irs::attribute_view attrs;

  ASSERT_EQ(
    ir::cost::cost_t(ir::cost::MAX),
    ir::cost::extract(attrs)
  );

  ASSERT_EQ(5, ir::cost::extract(attrs, 5));


  irs::cost cost;
  attrs.emplace(cost);
  ASSERT_FALSE(bool(cost.rule()));

  auto est = 7;
  auto evaluated = false;

  // set estimation function and evaluate
  {
    cost.rule([&evaluated, est]() {
      evaluated = true;
      return est;
    });
    ASSERT_TRUE(bool(cost.rule()));
    ASSERT_FALSE(evaluated);
    ASSERT_EQ(est, ir::cost::extract(attrs));
    ASSERT_TRUE(evaluated);
  }

  // change estimation func
  {
    evaluated = false;
    cost.rule([&evaluated, est]() {
      evaluated = true;
      return est+1;
    });
    ASSERT_TRUE(bool(cost.rule()));
    ASSERT_FALSE(evaluated);
    ASSERT_EQ(est+1, ir::cost::extract(attrs, 3));
    ASSERT_TRUE(evaluated);
  }

  // clear
  {
    evaluated = false;
    cost.clear();
    ASSERT_EQ(est+1, ir::cost::extract(attrs, 3));
    /* evaluate again */
    ASSERT_TRUE(evaluated);
  }
}

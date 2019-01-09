////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "tests_shared.hpp"
#include "search/scorers.hpp"
#include "search/sort.hpp"

TEST(sort_tests, order_equal) {
  struct dummy_scorer0: public irs::sort {
    DECLARE_SORT_TYPE() { static irs::sort::type_id type("dummy_scorer0"); return type; }
    static ptr make() { return std::make_shared<dummy_scorer0>(); }
    dummy_scorer0(): irs::sort(dummy_scorer0::type()) { }
    virtual prepared::ptr prepare() const override { return nullptr; }
  };

  struct dummy_scorer1: public irs::sort {
    DECLARE_SORT_TYPE() { static irs::sort::type_id type("dummy_scorer1"); return type; }
    static ptr make() { return std::make_shared<dummy_scorer1>(); }
    dummy_scorer1(): irs::sort(dummy_scorer1::type()) { }
    virtual prepared::ptr prepare() const override { return nullptr; }
  };

  // empty == empty
  {
    irs::order ord0;
    irs::order ord1;
    ASSERT_TRUE(ord0 == ord1);
    ASSERT_FALSE(ord0 != ord1);
  }

  // empty == !empty
  {
    irs::order ord0;
    irs::order ord1;
    ord1.add<dummy_scorer1>(false);
    ASSERT_FALSE(ord0 == ord1);
    ASSERT_TRUE(ord0 != ord1);
  }

  // different sort types
  {
    irs::order ord0;
    irs::order ord1;
    ord0.add<dummy_scorer0>(false);
    ord1.add<dummy_scorer1>(false);
    ASSERT_FALSE(ord0 == ord1);
    ASSERT_TRUE(ord0 != ord1);
  }

  // different order same sort type
  {
    irs::order ord0;
    irs::order ord1;
    ord0.add<dummy_scorer0>(false);
    ord0.add<dummy_scorer1>(false);
    ord1.add<dummy_scorer1>(false);
    ord1.add<dummy_scorer0>(false);
    ASSERT_FALSE(ord0 == ord1);
    ASSERT_TRUE(ord0 != ord1);
  }

  // different number same sorts
  {
    irs::order ord0;
    irs::order ord1;
    ord0.add<dummy_scorer0>(false);
    ord1.add<dummy_scorer0>(false);
    ord1.add<dummy_scorer0>(false);
    ASSERT_FALSE(ord0 == ord1);
    ASSERT_TRUE(ord0 != ord1);
  }

  // different number different sorts
  {
    irs::order ord0;
    irs::order ord1;
    ord0.add<dummy_scorer0>(false);
    ord1.add<dummy_scorer1>(false);
    ord1.add<dummy_scorer1>(false);
    ASSERT_FALSE(ord0 == ord1);
    ASSERT_TRUE(ord0 != ord1);
  }

  // same sorts same types
  {
    irs::order ord0;
    irs::order ord1;
    ord0.add<dummy_scorer0>(false);
    ord0.add<dummy_scorer1>(false);
    ord1.add<dummy_scorer0>(false);
    ord1.add<dummy_scorer1>(false);
    ASSERT_TRUE(ord0 == ord1);
    ASSERT_FALSE(ord0 != ord1);
  }
}

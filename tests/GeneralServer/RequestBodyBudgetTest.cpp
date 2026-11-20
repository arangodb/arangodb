////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#include "GeneralServer/RequestBodyBudget.h"

#include "gtest/gtest.h"

#include <atomic>
#include <thread>
#include <vector>

using namespace arangodb;

class RequestBodyBudgetTest : public ::testing::Test {
 protected:
  metrics::Gauge<std::uint64_t> used{0, "used", "", ""};
  metrics::Counter rejected{0, "rejected", "", ""};
  RequestBodyBudget budget{used, rejected};
};

TEST_F(RequestBodyBudgetTest, bodies_that_fit_alone_dont_fit_together) {
  ASSERT_TRUE(budget.reserve(10, 16));
  EXPECT_FALSE(budget.reserve(10, 16));
  EXPECT_EQ(budget.used(), 10U);
  EXPECT_EQ(rejected.load(), 1U);

  budget.release(10);
  EXPECT_TRUE(budget.reserve(10, 16));
  EXPECT_EQ(budget.used(), 10U);
}

TEST_F(RequestBodyBudgetTest, limit_is_inclusive) {
  EXPECT_TRUE(budget.reserve(16, 16));
  EXPECT_FALSE(budget.reserve(1, 16));
}

TEST_F(RequestBodyBudgetTest, zero_limit_is_unlimited) {
  EXPECT_EQ(budget.admit(UINT64_MAX / 2, 0),
            RequestBodyBudget::Admission::kFits);
  EXPECT_TRUE(budget.reserve(UINT64_MAX / 2, 0));
  EXPECT_EQ(rejected.load(), 0U);
}

TEST_F(RequestBodyBudgetTest, admit_tells_never_from_not_now) {
  EXPECT_EQ(budget.admit(17, 16), RequestBodyBudget::Admission::kNeverFits);
  ASSERT_TRUE(budget.reserve(10, 16));
  EXPECT_EQ(budget.admit(6, 16), RequestBodyBudget::Admission::kFits);
  EXPECT_EQ(budget.admit(7, 16), RequestBodyBudget::Admission::kNotNow);
  EXPECT_EQ(budget.used(), 10U);
  EXPECT_EQ(rejected.load(), 2U);
}

TEST_F(RequestBodyBudgetTest, concurrent_reservations_never_exceed_limit) {
  constexpr std::uint64_t limit = 64;
  constexpr int perThread = 20000;
  std::atomic<bool> exceeded{false};
  std::atomic<std::uint64_t> granted{0};
  std::vector<std::thread> threads;
  for (int t = 0; t < 8; ++t) {
    threads.emplace_back([&] {
      for (int i = 0; i < perThread; ++i) {
        if (budget.reserve(7, limit)) {
          granted.fetch_add(1);
          if (budget.used() > limit) {
            exceeded = true;
          }
          budget.release(7);
        }
      }
    });
  }
  for (auto& t : threads) {
    t.join();
  }
  EXPECT_FALSE(exceeded);
  EXPECT_EQ(budget.used(), 0U);
  EXPECT_EQ(granted.load() + rejected.load(), 8U * perThread);
}

#ifndef ARANGODB_ENABLE_MAINTAINER_MODE
TEST_F(RequestBodyBudgetTest, over_release_clamps_at_zero) {
  ASSERT_TRUE(budget.reserve(5, 16));
  budget.release(10);
  EXPECT_EQ(budget.used(), 0U);
  EXPECT_TRUE(budget.reserve(16, 16));
}
#endif

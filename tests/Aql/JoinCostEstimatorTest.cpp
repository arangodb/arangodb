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

#include "gtest/gtest.h"

#include "Aql/Optimizer/Utils/JoinCostEstimator.h"

#include <cmath>
#include <limits>

using namespace arangodb::aql;

namespace arangodb::tests::aql {

TEST(JoinCostFunctionsTest, probe_cost_is_rows_times_log2_of_collection) {
  EXPECT_DOUBLE_EQ(probeCost(100.0, 1000.0), 100.0 * std::log2(1000.0));
  EXPECT_DOUBLE_EQ(probeCost(1000.0, 100.0), 1000.0 * std::log2(100.0));
}

TEST(JoinCostFunctionsTest, probe_cost_per_row_is_floored_at_one) {
  // log2(1) == 0 would make a join into a one-document collection free.
  EXPECT_DOUBLE_EQ(probeCost(50.0, 1.0), 50.0);
  EXPECT_DOUBLE_EQ(probeCost(50.0, 0.0), 50.0);
}

TEST(JoinCostFunctionsTest, scan_cost_is_rows_times_collection) {
  EXPECT_DOUBLE_EQ(scanCost(100.0, 1000.0), 100000.0);
  // an empty collection still costs one touch per outer row
  EXPECT_DOUBLE_EQ(scanCost(100.0, 0.0), 100.0);
}

TEST(JoinCostFunctionsTest, probing_beats_scanning_the_larger_side) {
  // scan the small collection, probe the large one
  double const scanSmallProbeLarge = 100.0 + probeCost(100.0, 1000.0);
  double const scanLargeProbeSmall = 1000.0 + probeCost(1000.0, 100.0);
  EXPECT_LT(scanSmallProbeLarge, scanLargeProbeSmall);
}

TEST(JoinCostFunctionsTest, estimates_clamp_to_a_finite_ceiling) {
  EXPECT_DOUBLE_EQ(clampEstimate(std::numeric_limits<double>::infinity()),
                   kMaxEstimate);
  EXPECT_DOUBLE_EQ(clampEstimate(std::nan("")), kMaxEstimate);
  EXPECT_DOUBLE_EQ(clampEstimate(1e300), kMaxEstimate);
  EXPECT_DOUBLE_EQ(clampEstimate(-5.0), 0.0);
  EXPECT_DOUBLE_EQ(clampEstimate(42.0), 42.0);
  EXPECT_DOUBLE_EQ(scanCost(1e200, 1e200), kMaxEstimate);
}

}  // namespace arangodb::tests::aql

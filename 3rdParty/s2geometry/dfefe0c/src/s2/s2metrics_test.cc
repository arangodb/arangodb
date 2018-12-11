// Copyright 2005 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

// Author: ericv@google.com (Eric Veach)

#include "s2/s2metrics.h"

#include <gtest/gtest.h>
#include "s2/s2coords.h"

// Note: obviously, I could have defined a bundle of metrics like this in the
// S2 class itself rather than just for testing.  However, it's not clear that
// this is useful other than for testing purposes, and I find
// S2::kMinWidth.GetLevelForMinValue(width) to be slightly more readable than
// than S2::kWidth.min().GetLevelForMinValue(width).  Also, there is no
// fundamental reason that we need to analyze the minimum, maximum, and average
// values of every metric; it would be perfectly reasonable to just define
// one of these.
template<int dim>
class MetricBundle {
 public:
  using Metric = S2::Metric<dim>;
  MetricBundle(const Metric& min, const Metric& max, const Metric& avg) :
    min_(min), max_(max), avg_(avg) {}
  const Metric& min_;
  const Metric& max_;
  const Metric& avg_;

 private:
  MetricBundle(const MetricBundle&) = delete;
  void operator=(const MetricBundle&) = delete;
};

template<int dim>
static void CheckMinMaxAvg(const MetricBundle<dim>& bundle) {
  EXPECT_LE(bundle.min_.deriv(), bundle.avg_.deriv());
  EXPECT_LE(bundle.avg_.deriv(), bundle.max_.deriv());
}

template<int dim>
static void CheckLessOrEqual(const MetricBundle<dim>& a,
                             const MetricBundle<dim>& b) {
  EXPECT_LE(a.min_.deriv(), b.min_.deriv());
  EXPECT_LE(a.max_.deriv(), b.max_.deriv());
  EXPECT_LE(a.avg_.deriv(), b.avg_.deriv());
}

TEST(S2, Metrics) {
  MetricBundle<1> angle_span(S2::kMinAngleSpan, S2::kMaxAngleSpan,
                             S2::kAvgAngleSpan);
  MetricBundle<1> width(S2::kMinWidth, S2::kMaxWidth, S2::kAvgWidth);
  MetricBundle<1> edge(S2::kMinEdge, S2::kMaxEdge, S2::kAvgEdge);
  MetricBundle<1> diag(S2::kMinDiag, S2::kMaxDiag, S2::kAvgDiag);
  MetricBundle<2> area(S2::kMinArea, S2::kMaxArea, S2::kAvgArea);

  // First, check that min <= avg <= max for each metric.
  CheckMinMaxAvg(angle_span);
  CheckMinMaxAvg(width);
  CheckMinMaxAvg(edge);
  CheckMinMaxAvg(diag);
  CheckMinMaxAvg(area);

  // Check that the maximum aspect ratio of an individual cell is consistent
  // with the global minimums and maximums.
  EXPECT_GE(S2::kMaxEdgeAspect, 1);
  EXPECT_LE(S2::kMaxEdgeAspect, S2::kMaxEdge.deriv() / S2::kMinEdge.deriv());
  EXPECT_GE(S2::kMaxDiagAspect, 1);
  EXPECT_LE(S2::kMaxDiagAspect, S2::kMaxDiag.deriv() / S2::kMinDiag.deriv());

  // Check various conditions that are provable mathematically.
  CheckLessOrEqual(width, angle_span);
  CheckLessOrEqual(width, edge);
  CheckLessOrEqual(edge, diag);

  EXPECT_GE(S2::kMinArea.deriv(),
            S2::kMinWidth.deriv() * S2::kMinEdge.deriv() - 1e-15);
  EXPECT_LE(S2::kMaxArea.deriv(),
            S2::kMaxWidth.deriv() * S2::kMaxEdge.deriv() + 1e-15);

  // GetLevelForMaxValue() and friends have built-in assertions, we just need
  // to call these functions to test them.
  //
  // We don't actually check that the metrics are correct here, e.g. that
  // GetMinWidth(10) is a lower bound on the width of cells at level 10.
  // It is easier to check these properties in s2cell_test, since
  // S2Cell has methods to compute the cell vertices, etc.

  for (int level = -2; level <= S2::kMaxCellLevel + 3; ++level) {
    double width = S2::kMinWidth.deriv() * pow(2, -level);
    if (level >= S2::kMaxCellLevel + 3) width = 0;

    // Check boundary cases (exactly equal to a threshold value).
    int expected_level = std::max(0, std::min(S2::kMaxCellLevel, level));
    EXPECT_EQ(S2::kMinWidth.GetLevelForMaxValue(width), expected_level);
    EXPECT_EQ(S2::kMinWidth.GetLevelForMinValue(width), expected_level);
    EXPECT_EQ(S2::kMinWidth.GetClosestLevel(width), expected_level);

    // Also check non-boundary cases.
    EXPECT_EQ(S2::kMinWidth.GetLevelForMaxValue(1.2 * width), expected_level);
    EXPECT_EQ(S2::kMinWidth.GetLevelForMinValue(0.8 * width), expected_level);
    EXPECT_EQ(S2::kMinWidth.GetClosestLevel(1.2 * width), expected_level);
    EXPECT_EQ(S2::kMinWidth.GetClosestLevel(0.8 * width), expected_level);

    // Same thing for area.
    double area = S2::kMinArea.deriv() * pow(4, -level);
    if (level <= -3) area = 0;
    EXPECT_EQ(S2::kMinArea.GetLevelForMaxValue(area), expected_level);
    EXPECT_EQ(S2::kMinArea.GetLevelForMinValue(area), expected_level);
    EXPECT_EQ(S2::kMinArea.GetClosestLevel(area), expected_level);
    EXPECT_EQ(S2::kMinArea.GetLevelForMaxValue(1.2 * area), expected_level);
    EXPECT_EQ(S2::kMinArea.GetLevelForMinValue(0.8 * area), expected_level);
    EXPECT_EQ(S2::kMinArea.GetClosestLevel(1.2 * area), expected_level);
    EXPECT_EQ(S2::kMinArea.GetClosestLevel(0.8 * area), expected_level);
  }
}

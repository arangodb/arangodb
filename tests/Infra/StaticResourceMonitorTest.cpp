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
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "infra/StaticResourceMonitor.h"

using namespace arangodb::infra;

// StaticResourceMonitor adds no behavior of its own beyond isStatic()==true; it
// is the minimal concrete type that exposes the protected ResourceMonitorBase
// increase/decrease/limit operations. These tests therefore exercise the
// ResourceMonitorBase contract through that minimal vehicle. The
// dynamic-monitor delta (parent forwarding, refcounting, retirement) lives in
// ResourceMonitorTest.

TEST(StaticResourceMonitorTest, default_state) {
  StaticResourceMonitor monitor;
  EXPECT_EQ(0u, monitor.requestedBytes());
  EXPECT_EQ(0u, monitor.actualBytes());
  EXPECT_EQ(0u, monitor.limit());
  EXPECT_TRUE(monitor.isStatic());
}

TEST(StaticResourceMonitorTest, increase_adds_to_counters) {
  StaticResourceMonitor monitor;
  EXPECT_TRUE(monitor.increase(100, 128));
  EXPECT_EQ(100u, monitor.requestedBytes());
  EXPECT_EQ(128u, monitor.actualBytes());
}

TEST(StaticResourceMonitorTest, decrease_subtracts_from_counters) {
  StaticResourceMonitor monitor;
  EXPECT_TRUE(monitor.increase(100, 128));
  monitor.decrease(40, 48);
  EXPECT_EQ(60u, monitor.requestedBytes());
  EXPECT_EQ(80u, monitor.actualBytes());
}

TEST(StaticResourceMonitorTest, multiple_increases_accumulate) {
  StaticResourceMonitor monitor;
  EXPECT_TRUE(monitor.increase(100, 128));
  EXPECT_TRUE(monitor.increase(200, 256));
  EXPECT_TRUE(monitor.increase(50, 64));
  EXPECT_EQ(350u, monitor.requestedBytes());
  EXPECT_EQ(448u, monitor.actualBytes());
}

TEST(StaticResourceMonitorTest, limit_can_be_set) {
  StaticResourceMonitor monitor;
  monitor.setLimit(1024);
  EXPECT_EQ(1024u, monitor.limit());
}

TEST(StaticResourceMonitorTest, increase_within_limit_succeeds) {
  StaticResourceMonitor monitor;
  monitor.setLimit(1024);
  EXPECT_TRUE(monitor.increase(512, 600));
  EXPECT_EQ(512u, monitor.requestedBytes());
}

TEST(StaticResourceMonitorTest, increase_exceeding_limit_fails) {
  StaticResourceMonitor monitor;
  monitor.setLimit(1024);
  EXPECT_TRUE(monitor.increase(512, 600));
  EXPECT_FALSE(monitor.increase(600, 700));
  EXPECT_EQ(512u, monitor.requestedBytes());
  EXPECT_EQ(600u, monitor.actualBytes());
}

TEST(StaticResourceMonitorTest, increase_at_exact_limit_succeeds) {
  StaticResourceMonitor monitor;
  monitor.setLimit(1024);
  EXPECT_TRUE(monitor.increase(900, 1024));
  EXPECT_EQ(900u, monitor.requestedBytes());
  EXPECT_EQ(1024u, monitor.actualBytes());
}

TEST(StaticResourceMonitorTest, zero_limit_means_unlimited) {
  StaticResourceMonitor monitor;
  EXPECT_TRUE(monitor.increase(1'000'000, 1'100'000));
}

TEST(StaticResourceMonitorTest, lowering_limit_below_usage_rejects_increase) {
  StaticResourceMonitor monitor;
  EXPECT_TRUE(monitor.increase(500, 600));

  // Lowering the limit below current usage does not retroactively shrink the
  // recorded usage, but every subsequent increase is rejected.
  monitor.setLimit(500);
  EXPECT_FALSE(monitor.increase(1, 1));
  EXPECT_EQ(500u, monitor.requestedBytes());
  EXPECT_EQ(600u, monitor.actualBytes());
}

TEST(StaticResourceMonitorTest, next_pointer_default_is_null) {
  StaticResourceMonitor monitor;
  EXPECT_EQ(nullptr, monitor.next());
}

TEST(StaticResourceMonitorTest, next_pointer_can_be_set) {
  StaticResourceMonitor a;
  StaticResourceMonitor b;
  a.setNext(&b);
  EXPECT_EQ(&b, a.next());
}

TEST(StaticResourceMonitorTest, decrease_to_zero) {
  StaticResourceMonitor monitor;
  EXPECT_TRUE(monitor.increase(100, 128));
  EXPECT_TRUE(monitor.increase(200, 256));
  monitor.decrease(100, 128);
  monitor.decrease(200, 256);
  EXPECT_EQ(0u, monitor.requestedBytes());
  EXPECT_EQ(0u, monitor.actualBytes());
}

TEST(StaticResourceMonitorTest,
     clearing_limit_allows_previously_rejected_increase) {
  StaticResourceMonitor monitor;
  monitor.setLimit(100);
  EXPECT_TRUE(monitor.increase(50, 80));
  EXPECT_FALSE(monitor.increase(50, 80));

  monitor.setLimit(0);
  EXPECT_TRUE(monitor.increase(50, 80));
  EXPECT_EQ(100u, monitor.requestedBytes());
  EXPECT_EQ(160u, monitor.actualBytes());
}

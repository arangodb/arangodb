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

#include "infra/ResourceMonitor.h"

using namespace arangodb::infra;

TEST(ResourceMonitorTest, reports_not_static) {
  ResourceMonitor monitor;
  EXPECT_FALSE(monitor.isStatic());
}

TEST(ResourceMonitorTest, default_constructed_has_null_parent) {
  ResourceMonitor monitor;
  EXPECT_EQ(nullptr, monitor.parent());
}

TEST(ResourceMonitorTest, constructed_with_parent) {
  StaticResourceMonitor parent;
  ResourceMonitor monitor(&parent);
  EXPECT_EQ(&parent, monitor.parent());
}

TEST(ResourceMonitorTest, default_refcount_is_zero) {
  ResourceMonitor monitor;
  EXPECT_EQ(0u, monitor.refCount());
}

TEST(ResourceMonitorTest, acquire_increments_refcount) {
  ResourceMonitor monitor;
  monitor.acquireRef();
  EXPECT_EQ(1u, monitor.refCount());
  monitor.acquireRef();
  EXPECT_EQ(2u, monitor.refCount());
}

TEST(ResourceMonitorTest, release_decrements_refcount) {
  ResourceMonitor monitor;
  monitor.acquireRef();
  monitor.acquireRef();
  monitor.releaseRef();
  EXPECT_EQ(1u, monitor.refCount());
  monitor.releaseRef();
  EXPECT_EQ(0u, monitor.refCount());
}

TEST(ResourceMonitorTest, default_is_not_retired) {
  ResourceMonitor monitor;
  EXPECT_FALSE(monitor.isRetired());
}

TEST(ResourceMonitorTest, retire_marks_as_retired) {
  ResourceMonitor monitor;
  monitor.retire();
  EXPECT_TRUE(monitor.isRetired());
}

TEST(ResourceMonitorTest, is_not_reclaimable_when_not_retired) {
  ResourceMonitor monitor;
  EXPECT_FALSE(monitor.isReclaimable());
}

TEST(ResourceMonitorTest, is_not_reclaimable_when_retired_with_bytes) {
  ResourceMonitor monitor;
  EXPECT_TRUE(monitor.increase(100, 128));
  monitor.retire();
  EXPECT_FALSE(monitor.isReclaimable());
}

TEST(ResourceMonitorTest, is_reclaimable_when_retired_with_zero_bytes) {
  ResourceMonitor monitor;
  monitor.retire();
  EXPECT_TRUE(monitor.isReclaimable());
}

TEST(ResourceMonitorTest, becomes_reclaimable_after_bytes_freed) {
  ResourceMonitor monitor;
  EXPECT_TRUE(monitor.increase(100, 128));
  monitor.retire();
  EXPECT_FALSE(monitor.isReclaimable());
  monitor.decrease(100, 128);
  EXPECT_TRUE(monitor.isReclaimable());
}

TEST(ResourceMonitorTest, reclaimable_depends_on_actual_bytes_not_requested) {
  ResourceMonitor monitor;
  EXPECT_TRUE(monitor.increase(100, 200));
  monitor.retire();
  monitor.decrease(0, 200);
  // requestedBytes is still 100, but actualBytes is 0 — reclaimable
  EXPECT_EQ(100u, monitor.requestedBytes());
  EXPECT_TRUE(monitor.isReclaimable());
}

TEST(ResourceMonitorTest, reclaimable_ignores_refcount) {
  ResourceMonitor monitor;
  monitor.acquireRef();
  monitor.retire();
  EXPECT_EQ(1u, monitor.refCount());
  EXPECT_TRUE(monitor.isReclaimable());
}

// The base counter/limit contract (including the exact-limit boundary and
// plain limit enforcement) is covered once in StaticResourceMonitorTest. Tests
// here cover only what ResourceMonitor adds on top: parent forwarding,
// refcounting, and retirement.

// --- parent forwarding ---

TEST(ResourceMonitorTest, increase_forwards_to_parent) {
  StaticResourceMonitor parent;
  ResourceMonitor monitor(&parent);
  EXPECT_TRUE(monitor.increase(100, 128));
  EXPECT_EQ(100u, parent.requestedBytes());
  EXPECT_EQ(128u, parent.actualBytes());
}

TEST(ResourceMonitorTest, decrease_forwards_to_parent) {
  StaticResourceMonitor parent;
  ResourceMonitor monitor(&parent);
  EXPECT_TRUE(monitor.increase(200, 256));
  monitor.decrease(80, 100);
  EXPECT_EQ(120u, parent.requestedBytes());
  EXPECT_EQ(156u, parent.actualBytes());
  EXPECT_EQ(120u, monitor.requestedBytes());
  EXPECT_EQ(156u, monitor.actualBytes());
}

TEST(ResourceMonitorTest, parent_limit_rejects_increase) {
  StaticResourceMonitor parent;
  parent.setLimit(500);
  ResourceMonitor monitor(&parent);
  EXPECT_TRUE(monitor.increase(300, 400));
  EXPECT_FALSE(monitor.increase(200, 200));
  EXPECT_EQ(300u, monitor.requestedBytes());
  EXPECT_EQ(400u, monitor.actualBytes());
  EXPECT_EQ(300u, parent.requestedBytes());
  EXPECT_EQ(400u, parent.actualBytes());
}

TEST(ResourceMonitorTest, child_limit_rejects_before_parent_is_touched) {
  StaticResourceMonitor parent;
  ResourceMonitor monitor(&parent);
  monitor.setLimit(200);
  EXPECT_TRUE(monitor.increase(100, 150));
  EXPECT_FALSE(monitor.increase(100, 100));
  EXPECT_EQ(100u, parent.requestedBytes());
  EXPECT_EQ(150u, parent.actualBytes());
}

TEST(ResourceMonitorTest, multiple_children_share_parent) {
  StaticResourceMonitor parent;
  ResourceMonitor m1(&parent);
  ResourceMonitor m2(&parent);
  EXPECT_TRUE(m1.increase(100, 128));
  EXPECT_TRUE(m2.increase(200, 256));
  EXPECT_EQ(300u, parent.requestedBytes());
  EXPECT_EQ(384u, parent.actualBytes());
  m1.decrease(100, 128);
  EXPECT_EQ(200u, parent.requestedBytes());
  EXPECT_EQ(256u, parent.actualBytes());
}

TEST(ResourceMonitorTest, no_parent_does_not_forward) {
  ResourceMonitor monitor;
  EXPECT_TRUE(monitor.increase(100, 128));
  monitor.decrease(100, 128);
  EXPECT_EQ(0u, monitor.requestedBytes());
}

TEST(ResourceMonitorTest,
     parent_limit_rejects_when_second_child_exhausts_budget) {
  StaticResourceMonitor parent;
  parent.setLimit(500);
  ResourceMonitor childA(&parent);
  ResourceMonitor childB(&parent);

  EXPECT_TRUE(childA.increase(300, 400));
  EXPECT_FALSE(childB.increase(100, 200));

  EXPECT_EQ(0u, childB.requestedBytes());
  EXPECT_EQ(0u, childB.actualBytes());
  EXPECT_EQ(300u, childA.requestedBytes());
  EXPECT_EQ(400u, childA.actualBytes());
  EXPECT_EQ(300u, parent.requestedBytes());
  EXPECT_EQ(400u, parent.actualBytes());
}

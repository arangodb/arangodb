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

#include "infra/ResourceMonitorRef.h"

using namespace arangodb::infra;

TEST(ResourceMonitorRefTest, default_constructed_is_null) {
  ResourceMonitorRef ref;
  EXPECT_EQ(nullptr, ref.get());
  EXPECT_FALSE(static_cast<bool>(ref));
}

TEST(ResourceMonitorRefTest, from_static_monitor_stores_pointer) {
  StaticResourceMonitor monitor;
  ResourceMonitorRef ref(monitor);
  EXPECT_EQ(&monitor, ref.get());
  EXPECT_TRUE(static_cast<bool>(ref));
}

TEST(ResourceMonitorRefTest, from_dynamic_monitor_acquires) {
  ResourceMonitor monitor;
  EXPECT_EQ(0u, monitor.refCount());
  ResourceMonitorRef ref(monitor);
  EXPECT_EQ(1u, monitor.refCount());
  EXPECT_EQ(&monitor, ref.get());
}

TEST(ResourceMonitorRefTest, destructor_releases_dynamic_monitor) {
  ResourceMonitor monitor;
  {
    ResourceMonitorRef ref(monitor);
    EXPECT_EQ(1u, monitor.refCount());
  }
  EXPECT_EQ(0u, monitor.refCount());
}

TEST(ResourceMonitorRefTest, move_constructor_transfers_ownership) {
  ResourceMonitor monitor;
  ResourceMonitorRef ref1(monitor);
  EXPECT_EQ(1u, monitor.refCount());

  ResourceMonitorRef ref2(std::move(ref1));
  EXPECT_EQ(1u, monitor.refCount());
  EXPECT_EQ(&monitor, ref2.get());
  EXPECT_EQ(nullptr, ref1.get());
}

TEST(ResourceMonitorRefTest, move_assignment_transfers_ownership) {
  ResourceMonitor monitor;
  ResourceMonitorRef ref1(monitor);
  ResourceMonitorRef ref2;

  ref2 = std::move(ref1);
  EXPECT_EQ(1u, monitor.refCount());
  EXPECT_EQ(&monitor, ref2.get());
  EXPECT_EQ(nullptr, ref1.get());
}

TEST(ResourceMonitorRefTest, move_assignment_releases_old_target) {
  ResourceMonitor mon1;
  ResourceMonitor mon2;
  ResourceMonitorRef ref1(mon1);
  ResourceMonitorRef ref2(mon2);

  EXPECT_EQ(1u, mon1.refCount());
  EXPECT_EQ(1u, mon2.refCount());

  ref2 = std::move(ref1);
  EXPECT_EQ(1u, mon1.refCount());
  EXPECT_EQ(0u, mon2.refCount());
  EXPECT_EQ(&mon1, ref2.get());
}

TEST(ResourceMonitorRefTest, copy_constructor_acquires_for_dynamic) {
  ResourceMonitor monitor;
  ResourceMonitorRef ref1(monitor);
  EXPECT_EQ(1u, monitor.refCount());

  ResourceMonitorRef ref2(ref1);
  EXPECT_EQ(2u, monitor.refCount());
  EXPECT_EQ(&monitor, ref2.get());
  EXPECT_EQ(&monitor, ref1.get());
}

TEST(ResourceMonitorRefTest, copy_constructor_no_acquire_for_static) {
  StaticResourceMonitor monitor;
  ResourceMonitorRef ref1(monitor);

  ResourceMonitorRef ref2(ref1);
  EXPECT_EQ(&monitor, ref1.get());
  EXPECT_EQ(&monitor, ref2.get());
}

TEST(ResourceMonitorRefTest, copy_assignment_acquires_for_dynamic) {
  ResourceMonitor monitor;
  ResourceMonitorRef ref1(monitor);
  ResourceMonitorRef ref2;

  ref2 = ref1;
  EXPECT_EQ(2u, monitor.refCount());
  EXPECT_EQ(&monitor, ref1.get());
  EXPECT_EQ(&monitor, ref2.get());
}

TEST(ResourceMonitorRefTest, copy_assignment_releases_old_target) {
  ResourceMonitor mon1;
  ResourceMonitor mon2;
  ResourceMonitorRef ref1(mon1);
  ResourceMonitorRef ref2(mon2);

  EXPECT_EQ(1u, mon1.refCount());
  EXPECT_EQ(1u, mon2.refCount());

  ref2 = ref1;
  EXPECT_EQ(2u, mon1.refCount());
  EXPECT_EQ(0u, mon2.refCount());
}

TEST(ResourceMonitorRefTest, self_copy_assignment_is_safe) {
  ResourceMonitor monitor;
  ResourceMonitorRef ref(monitor);
  EXPECT_EQ(1u, monitor.refCount());

  auto& self = ref;
  ref = self;
  EXPECT_EQ(1u, monitor.refCount());
  EXPECT_EQ(&monitor, ref.get());
}

TEST(ResourceMonitorRefTest, self_move_assignment_is_safe) {
  ResourceMonitor monitor;
  ResourceMonitorRef ref(monitor);
  EXPECT_EQ(1u, monitor.refCount());

  auto& self = ref;
  ref = std::move(self);
  EXPECT_EQ(1u, monitor.refCount());
  EXPECT_EQ(&monitor, ref.get());
}

TEST(ResourceMonitorRefTest, reset_releases_and_nulls) {
  ResourceMonitor monitor;
  ResourceMonitorRef ref(monitor);
  EXPECT_EQ(1u, monitor.refCount());

  ref.reset();
  EXPECT_EQ(0u, monitor.refCount());
  EXPECT_EQ(nullptr, ref.get());
  EXPECT_FALSE(static_cast<bool>(ref));
}

TEST(ResourceMonitorRefTest, reset_on_null_ref_is_noop) {
  ResourceMonitorRef ref;
  ref.reset();
  EXPECT_EQ(nullptr, ref.get());
}

TEST(ResourceMonitorRefTest, reset_on_static_ref_does_not_crash) {
  StaticResourceMonitor monitor;
  ResourceMonitorRef ref(monitor);
  ref.reset();
  EXPECT_EQ(nullptr, ref.get());
}

TEST(ResourceMonitorRefTest, move_from_static_ref) {
  StaticResourceMonitor monitor;
  ResourceMonitorRef ref1(monitor);
  ResourceMonitorRef ref2(std::move(ref1));
  EXPECT_EQ(&monitor, ref2.get());
  EXPECT_EQ(nullptr, ref1.get());
}

TEST(ResourceMonitorRefTest, copy_from_null_ref) {
  ResourceMonitorRef ref1;
  ResourceMonitorRef ref2(ref1);
  EXPECT_EQ(nullptr, ref2.get());
  EXPECT_FALSE(static_cast<bool>(ref2));
}

TEST(ResourceMonitorRefTest, move_assign_from_null_releases_old) {
  ResourceMonitor monitor;
  ResourceMonitorRef ref1(monitor);
  EXPECT_EQ(1u, monitor.refCount());

  ResourceMonitorRef ref2;
  ref1 = std::move(ref2);
  EXPECT_EQ(0u, monitor.refCount());
  EXPECT_EQ(nullptr, ref1.get());
}

TEST(ResourceMonitorRefTest, copy_assign_from_null_releases_old) {
  ResourceMonitor monitor;
  ResourceMonitorRef ref1(monitor);
  EXPECT_EQ(1u, monitor.refCount());

  ResourceMonitorRef ref2;
  ref1 = ref2;
  EXPECT_EQ(0u, monitor.refCount());
  EXPECT_EQ(nullptr, ref1.get());
}

TEST(ResourceMonitorRefTest, move_assign_static_over_dynamic_releases_old) {
  ResourceMonitor dynamic;
  StaticResourceMonitor staticMon;
  ResourceMonitorRef ref1(dynamic);
  ResourceMonitorRef ref2(staticMon);
  EXPECT_EQ(1u, dynamic.refCount());

  ref1 = std::move(ref2);
  EXPECT_EQ(0u, dynamic.refCount());
  EXPECT_EQ(&staticMon, ref1.get());
  EXPECT_EQ(nullptr, ref2.get());
}

TEST(ResourceMonitorRefTest, copy_assign_static_over_dynamic_releases_old) {
  ResourceMonitor dynamic;
  StaticResourceMonitor staticMon;
  ResourceMonitorRef ref1(dynamic);
  ResourceMonitorRef ref2(staticMon);
  EXPECT_EQ(1u, dynamic.refCount());

  ref1 = ref2;
  EXPECT_EQ(0u, dynamic.refCount());
  EXPECT_EQ(&staticMon, ref1.get());
  EXPECT_EQ(&staticMon, ref2.get());
}

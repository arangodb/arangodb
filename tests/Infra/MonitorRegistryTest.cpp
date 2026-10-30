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

#include "infra/MonitorRegistry.h"
#include "infra/ResourceMonitor.h"
#include "infra/StaticResourceMonitor.h"

#include <type_traits>

using namespace arangodb::infra;

// The registry has process lifetime, so a free() before main or during static
// destruction must still find it valid. Both properties are of the type itself,
// hence compile-time assertions.
static_assert(std::is_trivially_destructible_v<MonitorRegistry>);
constinit MonitorRegistry gConstinitRegistry;

TEST(MonitorRegistryTest, constant_initialized_registry_is_usable) {
  StaticResourceMonitor s;
  gConstinitRegistry.insert(s);
  EXPECT_EQ(1u, gConstinitRegistry.size());
}

TEST(MonitorRegistryTest, empty_registry_has_size_zero) {
  MonitorRegistry registry;
  EXPECT_EQ(0u, registry.size());
}

TEST(MonitorRegistryTest, insert_multiple_monitors) {
  MonitorRegistry registry;
  StaticResourceMonitor a;
  StaticResourceMonitor b;
  StaticResourceMonitor c;
  registry.insert(a);
  registry.insert(b);
  registry.insert(c);
  EXPECT_EQ(3u, registry.size());
}

TEST(MonitorRegistryTest, for_each_accumulates_counters) {
  MonitorRegistry registry;
  StaticResourceMonitor a;
  StaticResourceMonitor b;
  EXPECT_TRUE(a.increase(100, 128));
  EXPECT_TRUE(b.increase(200, 256));
  registry.insert(a);
  registry.insert(b);

  uint64_t totalRequested = 0;
  registry.forEach([&](ResourceMonitorBase const& m) {
    totalRequested += m.requestedBytes();
  });
  EXPECT_EQ(300u, totalRequested);
}

TEST(MonitorRegistryTest, for_each_on_empty_registry_does_nothing) {
  MonitorRegistry registry;
  bool called = false;
  registry.forEach([&](ResourceMonitorBase const&) { called = true; });
  EXPECT_FALSE(called);
}

TEST(MonitorRegistryTest, cleanup_on_empty_registry_returns_zero) {
  MonitorRegistry registry;
  EXPECT_EQ(0u, registry.cleanup());
}

// Cleanup skips the head node to avoid ABA with concurrent lock-free
// inserts. Tests use a static sentinel as head (inserted last = head).

TEST(MonitorRegistryTest, cleanup_removes_reclaimable_dynamic_monitor) {
  MonitorRegistry registry;
  StaticResourceMonitor sentinel;
  auto* d = new ResourceMonitor();
  registry.insert(*d);
  registry.insert(sentinel);
  d->retire();
  EXPECT_EQ(2u, registry.size());
  EXPECT_EQ(1u, registry.cleanup());
  EXPECT_EQ(1u, registry.size());
}

TEST(MonitorRegistryTest, cleanup_does_not_remove_non_retired_dynamic_monitor) {
  MonitorRegistry registry;
  StaticResourceMonitor sentinel;
  auto* d = new ResourceMonitor();
  registry.insert(*d);
  registry.insert(sentinel);
  EXPECT_EQ(0u, registry.cleanup());
  EXPECT_EQ(2u, registry.size());
  delete d;
}

TEST(MonitorRegistryTest,
     cleanup_does_not_remove_retired_monitor_with_nonzero_bytes) {
  MonitorRegistry registry;
  StaticResourceMonitor sentinel;
  auto* d = new ResourceMonitor();
  EXPECT_TRUE(d->increase(100, 128));
  d->retire();
  registry.insert(*d);
  registry.insert(sentinel);
  EXPECT_EQ(0u, registry.cleanup());
  EXPECT_EQ(2u, registry.size());

  d->decrease(100, 128);
  EXPECT_EQ(1u, registry.cleanup());
  EXPECT_EQ(1u, registry.size());
}

TEST(MonitorRegistryTest, cleanup_does_not_remove_static_monitors) {
  MonitorRegistry registry;
  StaticResourceMonitor s;
  registry.insert(s);
  EXPECT_EQ(0u, registry.cleanup());
  EXPECT_EQ(1u, registry.size());
}

TEST(MonitorRegistryTest, cleanup_removes_only_reclaimable_in_mixed_list) {
  MonitorRegistry registry;
  StaticResourceMonitor s;
  auto* d1 = new ResourceMonitor();
  auto* d2 = new ResourceMonitor();
  auto* d3 = new ResourceMonitor();

  registry.insert(s);
  registry.insert(*d1);
  registry.insert(*d2);
  registry.insert(*d3);
  EXPECT_EQ(4u, registry.size());

  d1->retire();
  d3->retire();

  // d3 is the head (last inserted) — skipped by cleanup
  EXPECT_EQ(1u, registry.cleanup());
  EXPECT_EQ(3u, registry.size());

  // Insert a new sentinel to push d3 off the head.
  StaticResourceMonitor s2;
  registry.insert(s2);
  EXPECT_EQ(1u, registry.cleanup());
  EXPECT_EQ(3u, registry.size());
  delete d2;
}

TEST(MonitorRegistryTest, second_cleanup_finds_nothing_after_full_cleanup) {
  MonitorRegistry registry;
  StaticResourceMonitor sentinel;
  auto* d = new ResourceMonitor();
  registry.insert(*d);
  registry.insert(sentinel);
  d->retire();
  EXPECT_EQ(1u, registry.cleanup());
  EXPECT_EQ(0u, registry.cleanup());
}

TEST(MonitorRegistryTest, insert_after_cleanup_works) {
  MonitorRegistry registry;
  StaticResourceMonitor sentinel;
  auto* d = new ResourceMonitor();
  registry.insert(*d);
  registry.insert(sentinel);
  d->retire();
  registry.cleanup();

  StaticResourceMonitor s;
  registry.insert(s);
  EXPECT_EQ(2u, registry.size());
}

TEST(MonitorRegistryTest, reclaimable_head_is_collected_once_displaced) {
  MonitorRegistry registry;
  auto* d = new ResourceMonitor();
  registry.insert(*d);
  d->retire();

  // While it is the head, a reclaimable monitor is skipped (ABA guard).
  EXPECT_EQ(0u, registry.cleanup());
  EXPECT_EQ(1u, registry.size());

  // Once a later insert displaces it from the head, it becomes collectable.
  StaticResourceMonitor sentinel;
  registry.insert(sentinel);
  EXPECT_EQ(1u, registry.cleanup());  // deletes d
  EXPECT_EQ(1u, registry.size());
}

TEST(MonitorRegistryTest,
     cleanup_removes_multiple_reclaimable_among_non_reclaimable) {
  MonitorRegistry registry;
  StaticResourceMonitor sentinel;

  auto* d1 = new ResourceMonitor();
  auto* d2 = new ResourceMonitor();
  auto* d3 = new ResourceMonitor();
  auto* d4 = new ResourceMonitor();
  StaticResourceMonitor s;

  // Insert order (bottom to top): d1, d2, s, d3, d4, sentinel
  // List order (head to tail): sentinel -> d4 -> d3 -> s -> d2 -> d1
  registry.insert(*d1);
  registry.insert(*d2);
  registry.insert(s);
  registry.insert(*d3);
  registry.insert(*d4);
  registry.insert(sentinel);
  EXPECT_EQ(6u, registry.size());

  // Retire d1, d3, d4 — d2 stays active
  d1->retire();
  d3->retire();
  d4->retire();

  // sentinel is head (skipped), d4 d3 d1 are reclaimable, d2 and s are not
  EXPECT_EQ(3u, registry.cleanup());
  EXPECT_EQ(3u, registry.size());

  uint64_t count = 0;
  registry.forEach([&](ResourceMonitorBase const&) { ++count; });
  EXPECT_EQ(3u, count);

  delete d2;
}

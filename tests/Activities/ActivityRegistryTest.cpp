////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////

#include "Activities/GenericActivity.h"
#include "Async/async.h"
#include "Containers/Concurrent/thread.h"
#include "Activities/Activity.h"
#include "Activities/Registry.h"
#include "Activities/RegistryGlobalVariable.h"
#include "Futures/Promise.h"
#include "Inspection/JsonPrintInspector.h"
#include "Logger/LogMacros.h"
#include "Futures/Utilities.h"

#include <gtest/gtest.h>
#include <coroutine>
#include <thread>
#include <unordered_map>
#include <thread>

using namespace arangodb;
using namespace arangodb::activities;

struct ActivityRegistryTest : ::testing::Test {
  ActivityRegistryTest() : scope(nullptr) {}
  void TearDown() override { registry.garbageCollect(); }

  Registry::ScopedCurrentlyExecutingActivity scope;
};

const auto ActivityRoot = ActivityHandle{nullptr};

TEST_F(ActivityRegistryTest, snapshot_does_not_include_dangling_activities) {
  {
    auto activity = activities::make<GenericActivity>("my generic activity",
                                                      GenericActivityData{});
  }

  // snapshot does not include activity
  auto snapshot = registry.snapshot();
  EXPECT_TRUE(snapshot.ok());
  EXPECT_EQ(velocypack::ArrayIterator(snapshot.get().slice()).size(), 0);

  // registry still includes activity
  EXPECT_EQ(registry.size(), 1);
  // but it is deleted after gc
  registry.garbageCollect();
  EXPECT_EQ(registry.size(), 0);
}

// all following tests are about parent-child-relationships

TEST_F(ActivityRegistryTest, has_no_parent_as_default) {
  auto activity = activities::make<GenericActivity>(
      "GenericActivity",
      GenericActivityData{{"id", "1234"}, {"some_other_key", "value"}});

  EXPECT_EQ(activity->type(), "GenericActivity");
  EXPECT_EQ(activity->parent(), nullptr);
  EXPECT_EQ(activity->copyData(),
            (GenericActivityData{{"id", "1234"}, {"some_other_key", "value"}}));
}

TEST_F(ActivityRegistryTest, registers_in_activity_registry) {
  auto a = activities::make<GenericActivity>("GenericActivity",
                                             GenericActivityData{});

  auto fa = registry.findActivityById(a->id());

  ASSERT_TRUE(fa.has_value());
  EXPECT_EQ(a->id(), fa.value()->id());
}

TEST_F(ActivityRegistryTest,
       currently_executing_activity_is_empty_per_default) {
  EXPECT_EQ(Registry::currentlyExecutingActivity(), ActivityRoot);
}

TEST_F(ActivityRegistryTest, scope_sets_currently_executing_activity) {
  EXPECT_EQ(Registry::currentlyExecutingActivity(), ActivityRoot);
  auto a = activities::make<GenericActivity>("GenericActivity",
                                             activities::GenericActivityData{});

  auto scope = Registry::ScopedCurrentlyExecutingActivity(a);
  auto current = Registry::currentlyExecutingActivity();

  ASSERT_EQ(a->id(), current->id());
  ASSERT_EQ(dynamic_cast<Activity*>(a.get()), current.get());
}

TEST_F(ActivityRegistryTest,
       scope_guard_does_not_capture_activity_over_threads) {
  EXPECT_EQ(Registry::currentlyExecutingActivity(), nullptr);

  auto activity = activities::make<GenericActivity>("GenericActivity",
                                                    GenericActivityData{});
  auto scopeGuard = Registry::ScopedCurrentlyExecutingActivity(activity);
  std::jthread(
      []() { EXPECT_EQ(Registry::currentlyExecutingActivity(), nullptr); });
}

TEST_F(ActivityRegistryTest,
       activity_created_in_parent_scope_knows_its_parent_automatically) {
  auto parent_activity = activities::make<GenericActivity>(
      "GenericActivity", GenericActivityData{});

  auto scope = Registry::ScopedCurrentlyExecutingActivity(parent_activity);
  auto child_activity = activities::make<GenericActivity>(
      "GenericActivity", GenericActivityData{});

  ASSERT_NE(child_activity->parent(), nullptr);
  ASSERT_EQ(child_activity->parentId().value(), parent_activity->id());
}

TEST_F(ActivityRegistryTest, activity_has_parent_of_most_recent_scope_guard) {
  ActivityHandle first_child_activity, second_child_activity,
      child_of_first_child_activity;  // need to live long enough to do
                                      // assertions

  auto parent_activity = activities::make<GenericActivity>(
      "GenericActivity", GenericActivityData{});
  {
    auto scope = Registry::ScopedCurrentlyExecutingActivity(parent_activity);

    first_child_activity = activities::make<GenericActivity>(
        "GenericActivity", GenericActivityData{});
    second_child_activity = activities::make<GenericActivity>(
        "GenericActivity", GenericActivityData{});

    {
      auto scope =
          Registry::ScopedCurrentlyExecutingActivity(first_child_activity);
      child_of_first_child_activity = activities::make<GenericActivity>(
          "GenericActivity", GenericActivityData{});
    }
  }

  ASSERT_NE(first_child_activity->parent(), nullptr);
  ASSERT_EQ(first_child_activity->parentId().value(), parent_activity->id());

  ASSERT_NE(second_child_activity->parent(), nullptr);
  ASSERT_EQ(second_child_activity->parentId().value(), parent_activity->id());

  ASSERT_NE(child_of_first_child_activity->parent(), nullptr);
  ASSERT_EQ(child_of_first_child_activity->parentId().value(),
            first_child_activity->id());
}

TEST_F(ActivityRegistryTest,
       scope_guard_resets_activity_when_going_out_of_scope) {
  auto a = activities::make<GenericActivity>("GenericActivity",
                                             GenericActivityData{});
  auto b = activities::make<GenericActivity>("GenericActivity",
                                             GenericActivityData{});

  EXPECT_EQ(Registry::currentlyExecutingActivity(), nullptr);
  {
    auto outerScopeGuard = Registry::ScopedCurrentlyExecutingActivity(a);
    EXPECT_EQ(Registry::currentlyExecutingActivity(), a);

    {
      auto innerScopeGuard = Registry::ScopedCurrentlyExecutingActivity(b);
      EXPECT_EQ(Registry::currentlyExecutingActivity(), b);
    }

    EXPECT_EQ(Registry::currentlyExecutingActivity(), a);
  }
  EXPECT_EQ(Registry::currentlyExecutingActivity(), nullptr);
}

TEST_F(ActivityRegistryTest,
       can_capture_currently_executing_activity_for_later_use) {
  auto outer =
      activities::make<GenericActivity>("outer", GenericActivityData{});
  auto outerGuard = Registry::ScopedCurrentlyExecutingActivity(outer);

  auto testee = withCurrentlyExecutingActivity(
      [current = Registry::currentlyExecutingActivity(), outer]() {
        EXPECT_EQ(Registry::currentlyExecutingActivity(), current);
        EXPECT_EQ(current, outer);
      });

  {
    auto inner =
        activities::make<GenericActivity>("inner", GenericActivityData{});
    auto innerGuard = Registry::ScopedCurrentlyExecutingActivity(inner);

    testee();
  }
}

TEST_F(ActivityRegistryTest,
       captured_executing_activity_connects_activities_over_threads) {
  auto activity =
      activities::make<GenericActivity>("activity", GenericActivityData{});
  auto guard = Registry::ScopedCurrentlyExecutingActivity(activity);

  std::jthread(withCurrentlyExecutingActivity(
      [current = Registry::currentlyExecutingActivity(), activity]() {
        EXPECT_EQ(Registry::currentlyExecutingActivity(), current);
        EXPECT_EQ(current, activity);
      }));
}

TEST_F(ActivityRegistryTest,
       captured_executing_activity_connects_activities_future_continuation) {
  EXPECT_EQ(Registry::currentlyExecutingActivity(), nullptr);

  auto activity = activities::make<GenericActivity>("GenericActivity",
                                                    GenericActivityData{});
  auto scopeGuard = Registry::ScopedCurrentlyExecutingActivity(activity);
  futures::Promise<int> p;
  auto f = p.getFuture().thenValue(
      withCurrentlyExecutingActivity([&activity](auto v) {
        EXPECT_EQ(Registry::currentlyExecutingActivity(), activity);
        return 8;
      }));

  auto anotherActivity =
      activities::make<GenericActivity>("inner", GenericActivityData{});
  auto anotherGuard =
      Registry::ScopedCurrentlyExecutingActivity(anotherActivity);
  EXPECT_EQ(Registry::currentlyExecutingActivity(), anotherActivity);

  p.setValue(42);  // executes thenValue clause
  EXPECT_EQ(f.waitAndGet(), 8);

  EXPECT_EQ(Registry::currentlyExecutingActivity(), anotherActivity);
}

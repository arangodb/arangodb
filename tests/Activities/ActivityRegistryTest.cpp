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
#include "Inspection/JsonPrintInspector.h"
#include "Logger/LogMacros.h"
#include <gtest/gtest.h>

#include <coroutine>
#include <thread>
#include <unordered_map>

using namespace arangodb;
using namespace arangodb::activities;

struct ActivityRegistryTest : ::testing::Test {
  ActivityRegistryTest() : scope(nullptr) {}
  void TearDown() override {
    registry.garbageCollect();
    //    EXPECT_EQ(get_all_activities().size(), 0);
  }

  Registry::ScopedCurrentlyExecutingActivity scope;
};

const auto ActivityRoot = ActivityHandle{nullptr};

TEST_F(ActivityRegistryTest, current_activity_is_nullptr) {
  EXPECT_EQ(Registry::currentlyExecutingActivity(), ActivityRoot);
}

TEST_F(ActivityRegistryTest, creates_registered_activity) {
  EXPECT_EQ(Registry::currentlyExecutingActivity(), ActivityRoot);

  auto a = activities::make<GenericActivity>(
      "GenericActivity", activities::GenericActivityData{{"test", "bla"}});

  auto fa = registry.findActivityById(a->id());

  ASSERT_TRUE(fa.has_value());
  EXPECT_EQ(a->id(), fa.value()->id());
}

TEST_F(ActivityRegistryTest, sets_current_activity) {
  EXPECT_EQ(Registry::currentlyExecutingActivity(), ActivityRoot);
  auto a = activities::make<GenericActivity>(
      "GenericActivity", activities::GenericActivityData{{"test", "bla"}});

  auto scope = Registry::ScopedCurrentlyExecutingActivity(a);
  auto current = Registry::currentlyExecutingActivity();

  ASSERT_EQ(a->id(), current->id());
  ASSERT_EQ(dynamic_cast<Activity*>(a.get()), current.get());
}

TEST_F(ActivityRegistryTest,
       a_base_activity_creates_a_root_activity_with_additional_information) {
  auto activity = activities::make<GenericActivity>(
      "GenericActivity",
      GenericActivityData{{"id", "1234"}, {"some_other_key", "value"}});

  EXPECT_EQ(activity->type(), "GenericActivity");
  EXPECT_EQ(activity->parent(), nullptr);
  EXPECT_EQ(activity->copyData(),
            (GenericActivityData{{"id", "1234"}, {"some_other_key", "value"}}));
}

TEST_F(ActivityRegistryTest, creates_a_child_activity) {
  auto parent_activity = activities::make<GenericActivity>(
      "GenericActivity", GenericActivityData{});

  auto scope = Registry::ScopedCurrentlyExecutingActivity(parent_activity);
  auto child_activity = activities::make<GenericActivity>(
      "GenericActivity", GenericActivityData{});

  ASSERT_NE(child_activity->parent(), nullptr);
  ASSERT_EQ(child_activity->parent()->id(), parent_activity->id());
}

TEST_F(ActivityRegistryTest, creates_a_child_activity_hierarchy) {
  ActivityHandle first_child_activity, second_child_activity,
      child_of_first_child_activity;

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
  ASSERT_EQ(first_child_activity->parent()->id(), parent_activity->id());

  ASSERT_NE(second_child_activity->parent(), nullptr);
  ASSERT_EQ(second_child_activity->parent()->id(), parent_activity->id());

  ASSERT_NE(child_of_first_child_activity->parent(), nullptr);
  ASSERT_EQ(child_of_first_child_activity->parent()->id(),
            first_child_activity->id());
}

TEST_F(ActivityRegistryTest, scope_guard_sets_resets_activity) {
  EXPECT_EQ(Registry::currentlyExecutingActivity(), nullptr);

  auto activity = activities::make<GenericActivity>("GenericActivity",
                                                    GenericActivityData{});
  {
    auto scopeGuard = Registry::ScopedCurrentlyExecutingActivity(activity);
    EXPECT_EQ(Registry::currentlyExecutingActivity(), activity);
  }
  EXPECT_EQ(Registry::currentlyExecutingActivity(), nullptr);
}

TEST_F(ActivityRegistryTest, nested_scopes_set_reset_activity) {
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

TEST_F(ActivityRegistryTest, with_set_current_activity) {
  auto a = activities::make<GenericActivity>("GenericActivity",
                                             GenericActivityData{});
  EXPECT_EQ(Registry::currentlyExecutingActivity(), ActivityRoot);

  auto scoped = withSetCurrentlyExecutingActivity(
      a, [a]() { EXPECT_EQ(Registry::currentlyExecutingActivity(), a); });

  EXPECT_EQ(Registry::currentlyExecutingActivity(), ActivityRoot);

  auto b = activities::make<GenericActivity>("GenericActivity",
                                             GenericActivityData{});
  auto scopeGuard = Registry::ScopedCurrentlyExecutingActivity(b);

  EXPECT_EQ(Registry::currentlyExecutingActivity(), b);

  scoped();

  EXPECT_EQ(Registry::currentlyExecutingActivity(), b);
}

TEST_F(ActivityRegistryTest, with_current_activity) {
  auto outer = activities::make<GenericActivity>("GenericActivity",
                                                 GenericActivityData{});
  auto inner = activities::make<GenericActivity>("GenericActivity",
                                                 GenericActivityData{});

  auto outerGuard = Registry::ScopedCurrentlyExecutingActivity(outer);
  auto testee = withCurrentlyExecutingActivity(
      [current = Registry::currentlyExecutingActivity()]() {
        EXPECT_EQ(Registry::currentlyExecutingActivity(), current);
      });

  {
    auto innerGuard = Registry::ScopedCurrentlyExecutingActivity(inner);
    testee();
  }
}

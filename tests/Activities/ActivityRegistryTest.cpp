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

#include "Activities/registry.h"
#include "Async/async.h"
#include "Containers/Concurrent/thread.h"
#include "Activities/activity.h"
#include "Activities/activity_registry_variable.h"
#include "Inspection/JsonPrintInspector.h"
#include <gtest/gtest.h>

#include <coroutine>
#include <thread>

using namespace arangodb;
using namespace arangodb::activities;

namespace {
auto get_all_activities() -> std::vector<ActivityInRegistrySnapshot> {
  std::vector<ActivityInRegistrySnapshot> activities;
  registry.for_node([&](ActivityInRegistrySnapshot activity) {
    activities.emplace_back(std::move(activity));
  });
  return activities;
}

}  // namespace

struct ActivityRegistryTest : ::testing::Test {
  ActivityRegistryTest() {
    Registry::setCurrentlyExecutingActivity(ActivityRoot);
  }
  void TearDown() override {
    get_thread_registry().garbage_collect();
    EXPECT_EQ(get_all_activities().size(), 0);
  }
};

TEST_F(ActivityRegistryTest, current_activity_is_nullptr) {
  EXPECT_EQ(Registry::currentlyExecutingActivity(), ActivityRoot);
}

TEST_F(ActivityRegistryTest, creates_activity) {
  EXPECT_EQ(Registry::currentlyExecutingActivity(), ActivityRoot);
  auto a = Activity("test activity", {{"test", "bla"}});

  auto all_activities = get_all_activities();
  auto s = std::find_if(std::begin(all_activities), std::end(all_activities),
                        [id = a.id()](auto i) { return i.id == id; });

  EXPECT_NE(s, std::end(all_activities));
  EXPECT_EQ(s->parent, ActivityRoot);
}

TEST_F(ActivityRegistryTest, sets_current_activity) {
  EXPECT_EQ(Registry::currentlyExecutingActivity(), ActivityRoot);
  auto a = Activity("test activity", {{"test", "bla"}});
  Registry::setCurrentlyExecutingActivity(a.id());

  auto current = Registry::currentlyExecutingActivity();

  ASSERT_EQ(a.id(), current);
}

TEST_F(ActivityRegistryTest,
       a_base_activity_creates_a_root_activity_with_additional_information) {
  EXPECT_EQ(Registry::currentlyExecutingActivity(), ActivityRoot);
  auto activity =
      Activity{"test activity", {{"id", "1234"}, {"some_other_key", "value"}}};

  auto all_activities = get_all_activities();
  auto specific =
      std::find(std::begin(all_activities), std::end(all_activities),
                ActivityInRegistrySnapshot{
                    .type = "test activity",
                    .state = State::Active,
                    .id = activity.id(),
                    .parent = ActivityRoot,
                    .metadata = {{"id", "1234"}, {"some_other_key", "value"}}});
  EXPECT_NE(specific, std::end(all_activities))
      << inspection::json(all_activities);
}

TEST_F(ActivityRegistryTest, creates_a_child_activity) {
  auto parent_activity = Activity{"parent activity", {}};
  auto child_activity = Activity{"child activity", {}, {parent_activity.id()}};

  EXPECT_EQ(get_all_activities(),
            (std::vector<ActivityInRegistrySnapshot>{
                (ActivityInRegistrySnapshot{
                    .type = "child activity",
                    .state = State::Active,
                    .id = child_activity.id(),
                    .parent = ActivityId{parent_activity.id()},
                    .metadata = {}}),
                (ActivityInRegistrySnapshot{.type = "parent activity",
                                            .state = State::Active,
                                            .id = parent_activity.id(),
                                            .parent = {ActivityRoot}})}));
}

TEST_F(ActivityRegistryTest, creates_a_child_activity_hierarchy) {
  auto parent_activity = Activity{"parent activity", {}};
  auto first_child_activity =
      Activity{"first child activity", {}, {parent_activity.id()}};
  auto second_child_activity =
      Activity{"second child activity", {}, {parent_activity.id()}};
  auto child_of_first_child_activity =
      Activity{"child of child activity", {}, {first_child_activity.id()}};

  EXPECT_EQ(get_all_activities(),
            (std::vector<ActivityInRegistrySnapshot>{
                (ActivityInRegistrySnapshot{
                    .type = "child of child activity",
                    .state = State::Active,
                    .id = child_of_first_child_activity.id(),
                    .parent = ActivityId{first_child_activity.id()}}),
                (ActivityInRegistrySnapshot{
                    .type = "second child activity",
                    .state = State::Active,
                    .id = second_child_activity.id(),
                    .parent = ActivityId{parent_activity.id()}}),
                (ActivityInRegistrySnapshot{
                    .type = "first child activity",
                    .state = State::Active,
                    .id = first_child_activity.id(),
                    .parent = ActivityId{parent_activity.id()}}),
                (ActivityInRegistrySnapshot{.type = "parent activity",
                                            .state = State::Active,
                                            .id = parent_activity.id(),
                                            .parent = ActivityRoot})}));
}

TEST_F(ActivityRegistryTest, scope_guard_sets_resets_activity) {
  auto a = Activity("activity", {});

  EXPECT_EQ(Registry::currentlyExecutingActivity(), ActivityRoot);
  {
    auto scopeGuard = Registry::ScopedCurrentlyExecutingActivity(a.id());
    EXPECT_EQ(Registry::currentlyExecutingActivity(), a.id());
  }
  EXPECT_EQ(Registry::currentlyExecutingActivity(), ActivityRoot);
}

TEST_F(ActivityRegistryTest, nested_scopes_set_reset_activity) {
  auto a = Activity("activity1", {});
  auto b = Activity("activity2", {});

  EXPECT_EQ(Registry::currentlyExecutingActivity(), ActivityRoot);
  {
    auto outerScopeGuard = Registry::ScopedCurrentlyExecutingActivity(a.id());
    EXPECT_EQ(Registry::currentlyExecutingActivity(), a.id());

    {
      auto innerScopeGuard = Registry::ScopedCurrentlyExecutingActivity(b.id());
      EXPECT_EQ(Registry::currentlyExecutingActivity(), b.id());
    }

    EXPECT_EQ(Registry::currentlyExecutingActivity(), a.id());
  }
  EXPECT_EQ(Registry::currentlyExecutingActivity(), ActivityRoot);
}

TEST_F(ActivityRegistryTest, with_set_current_activity) {
  auto a = Activity("activity to be in scope", {});
  EXPECT_EQ(Registry::currentlyExecutingActivity(), ActivityRoot);

  auto scoped = withSetCurrentlyExecutingActivity(a.id(), [id = a.id()]() {
    EXPECT_EQ(Registry::currentlyExecutingActivity(), id);
  });

  EXPECT_EQ(Registry::currentlyExecutingActivity(), ActivityRoot);

  auto b = Activity("activity to be current", {});
  auto scopeGuard = Registry::ScopedCurrentlyExecutingActivity(b.id());

  EXPECT_EQ(Registry::currentlyExecutingActivity(), b.id());

  scoped();

  EXPECT_EQ(Registry::currentlyExecutingActivity(), b.id());
}

TEST_F(ActivityRegistryTest, with_current_activity) {
  auto outer = Activity("outer activity", {});
  auto inner = Activity("inner activity", {});

  auto outerGuard = Registry::ScopedCurrentlyExecutingActivity(outer.id());
  auto testee = withCurrentlyExecutingActivity(
      [id = Registry::currentlyExecutingActivity()]() {
        EXPECT_EQ(Registry::currentlyExecutingActivity(), id);
      });

  {
    auto innerGuard = Registry::ScopedCurrentlyExecutingActivity(inner.id());
    testee();
  }
}

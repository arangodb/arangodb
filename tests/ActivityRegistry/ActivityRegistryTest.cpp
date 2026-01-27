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

#include "ActivityRegistry/registry.h"
#include "Async/async.h"
#include "Containers/Concurrent/thread.h"
#include "ActivityRegistry/activity.h"
#include "ActivityRegistry/activity_registry_variable.h"
#include "Inspection/JsonPrintInspector.h"
#include <gtest/gtest.h>

#include <coroutine>
#include <thread>

using namespace arangodb;
using namespace arangodb::activity_registry;

namespace {
auto get_all_activities() -> std::vector<ActivityInRegistrySnapshot> {
  std::vector<ActivityInRegistrySnapshot> activities;
  registry.for_node([&](ActivityInRegistrySnapshot activity) {
    activities.emplace_back(std::move(activity));
  });
  return activities;
}

}  // namespace

constexpr auto ActivityRoot = ActivityId{nullptr};

struct ActivityRegistryTest : ::testing::Test {
  ActivityRegistryTest() { Registry::setCurrentActivity(ActivityRoot); }
  void TearDown() override {
    get_thread_registry().garbage_collect();
    EXPECT_EQ(get_all_activities().size(), 0);
  }
};

TEST_F(ActivityRegistryTest, current_activity_is_nullptr) {
  EXPECT_EQ(Registry::currentActivity(), ActivityRoot);
}

TEST_F(ActivityRegistryTest, creates_activity) {
  EXPECT_EQ(Registry::currentActivity(), ActivityRoot);
  auto a = Activity("test activity", {{"test", "bla"}});

  auto all_activities = get_all_activities();
  auto s = std::find_if(std::begin(all_activities), std::end(all_activities),
                        [id = a.id()](auto i) { return i.id == id; });

  EXPECT_NE(s, std::end(all_activities));
  EXPECT_EQ(std::get<ActivityId>(s->parent), ActivityId{nullptr});
}

TEST_F(ActivityRegistryTest, sets_current_activity) {
  auto a = Activity("test activity", {{"test", "bla"}});
  Registry::setCurrentActivity(a.id());

  auto current = Registry::currentActivity();

  ASSERT_EQ(a.id(), current);
}

TEST_F(ActivityRegistryTest,
       a_base_activity_creates_a_root_activity_with_additional_information) {
  auto activity =
      Activity{"test activity", {{"id", "1234"}, {"some_other_key", "value"}}};

  auto all_activities = get_all_activities();
  auto specific =
      std::find(std::begin(all_activities), std::end(all_activities),
                ActivityInRegistrySnapshot{
                    .name = "test activity",
                    .state = State::Active,
                    .id = activity.id(),
                    .parent = {ActivityRoot},
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
                    .name = "child activity",
                    .state = State::Active,
                    .id = child_activity.id(),
                    .parent = {ActivityId{parent_activity.id()}},
                    .metadata = {}}),
                (ActivityInRegistrySnapshot{.name = "parent activity",
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
                    .name = "child of child activity",
                    .state = State::Active,
                    .id = child_of_first_child_activity.id(),
                    .parent = {ActivityId{first_child_activity.id()}}}),
                (ActivityInRegistrySnapshot{
                    .name = "second child activity",
                    .state = State::Active,
                    .id = second_child_activity.id(),
                    .parent = {ActivityId{parent_activity.id()}}}),
                (ActivityInRegistrySnapshot{
                    .name = "first child activity",
                    .state = State::Active,
                    .id = first_child_activity.id(),
                    .parent = {ActivityId{parent_activity.id()}}}),
                (ActivityInRegistrySnapshot{.name = "parent activity",
                                            .state = State::Active,
                                            .id = parent_activity.id(),
                                            .parent = {ActivityRoot}})}));
}

TEST_F(ActivityRegistryTest, scope_sets_activity) {
  auto a = Activity("activity to be in scope", {});

  auto scoped = withCurrentActivity(
      a.id(), [id = a.id()]() { EXPECT_EQ(Registry::currentActivity(), id); });

  auto b = Activity("activity to be current", {});
  Registry::setCurrentActivity(b.id());

  EXPECT_EQ(Registry::currentActivity(), b.id());

  scoped();

  EXPECT_EQ(Registry::currentActivity(), b.id());
}

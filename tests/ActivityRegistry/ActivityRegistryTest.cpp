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

struct MyActivity : public Activity {
  basics::SourceLocationSnapshot source_location;
  MyActivity(std::string name,
             std::source_location loc = std::source_location::current())
      : Activity{std::move(name), loc},
        source_location{basics::SourceLocationSnapshot::from(std::move(loc))} {}
};

}  // namespace

struct ActivityRegistryTest : ::testing::Test {
  void TearDown() override {
    get_thread_registry().garbage_collect();
    EXPECT_EQ(get_all_activities().size(), 0);
  }
};

TEST_F(ActivityRegistryTest, a_base_activity_creates_a_root_activity) {
  auto activity = MyActivity{"test activity"};

  EXPECT_EQ(
      get_all_activities(),
      (std::vector<ActivityInRegistrySnapshot>{(ActivityInRegistrySnapshot{
          .name = "test activity",
          .state = State::Running,
          .id = activity.id(),
          .parent = {RootActivity{}},
          .thread = basics::ThreadId::current(),
          .source_location = activity.source_location})}));
}

TEST_F(ActivityRegistryTest, creates_a_child_activity) {
  auto parent_activity = MyActivity{"parent activity"};
  auto child_activity = MyActivity{"child activity"};

  EXPECT_EQ(get_all_activities(),
            (std::vector<ActivityInRegistrySnapshot>{
                (ActivityInRegistrySnapshot{
                    .name = "child activity",
                    .state = State::Running,
                    .id = child_activity.id(),
                    .parent = {ActivityId{parent_activity.id()}},
                    .thread = basics::ThreadId::current(),
                    .source_location = child_activity.source_location}),
                (ActivityInRegistrySnapshot{
                    .name = "parent activity",
                    .state = State::Running,
                    .id = parent_activity.id(),
                    .parent = {RootActivity{}},
                    .thread = basics::ThreadId::current(),
                    .source_location = parent_activity.source_location})}));
}

TEST_F(ActivityRegistryTest, creates_a_child_activity_hierarchy) {
  auto parent_activity = MyActivity{"parent activity"};
  auto child_activity = MyActivity{"child activity"};
  auto child_of_child_activity = MyActivity{"child of child activity"};
  auto child_of_child_of_child_activity =
      MyActivity{"child of child of child activity"};

  EXPECT_EQ(
      get_all_activities(),
      (std::vector<ActivityInRegistrySnapshot>{
          (ActivityInRegistrySnapshot{
              .name = "child of child of child activity",
              .state = State::Running,
              .id = child_of_child_of_child_activity.id(),
              .parent = {ActivityId{child_of_child_activity.id()}},
              .thread = basics::ThreadId::current(),
              .source_location =
                  child_of_child_of_child_activity.source_location}),
          (ActivityInRegistrySnapshot{
              .name = "child of child activity",
              .state = State::Running,
              .id = child_of_child_activity.id(),
              .parent = {ActivityId{child_activity.id()}},
              .thread = basics::ThreadId::current(),
              .source_location = child_of_child_activity.source_location}),
          (ActivityInRegistrySnapshot{
              .name = "child activity",
              .state = State::Running,
              .id = child_activity.id(),
              .parent = {ActivityId{parent_activity.id()}},
              .thread = basics::ThreadId::current(),
              .source_location = child_activity.source_location}),
          (ActivityInRegistrySnapshot{
              .name = "parent activity",
              .state = State::Running,
              .id = parent_activity.id(),
              .parent = {RootActivity{}},
              .thread = basics::ThreadId::current(),
              .source_location = parent_activity.source_location})}));
}

TEST_F(ActivityRegistryTest, uses_correct_parent_activity) {
  auto parent_activity = MyActivity{"parent activity"};
  {
    auto first_child_activity = MyActivity{"first child activity"};

    EXPECT_EQ(get_all_activities(),
              (std::vector<ActivityInRegistrySnapshot>{
                  (ActivityInRegistrySnapshot{
                      .name = "first child activity",
                      .state = State::Running,
                      .id = first_child_activity.id(),
                      .parent = {ActivityId{parent_activity.id()}},
                      .thread = basics::ThreadId::current(),
                      .source_location = first_child_activity.source_location}),
                  (ActivityInRegistrySnapshot{
                      .name = "parent activity",
                      .state = State::Running,
                      .id = parent_activity.id(),
                      .parent = {RootActivity{}},
                      .thread = basics::ThreadId::current(),
                      .source_location = parent_activity.source_location})}));
  }
  get_thread_registry().garbage_collect();

  auto second_child_activity = MyActivity{"second child activity"};

  EXPECT_EQ(get_all_activities(),
            (std::vector<ActivityInRegistrySnapshot>{
                (ActivityInRegistrySnapshot{
                    .name = "second child activity",
                    .state = State::Running,
                    .id = second_child_activity.id(),
                    .parent = {ActivityId{parent_activity.id()}},
                    .thread = basics::ThreadId::current(),
                    .source_location = second_child_activity.source_location}),
                (ActivityInRegistrySnapshot{
                    .name = "parent activity",
                    .state = State::Running,
                    .id = parent_activity.id(),
                    .parent = {RootActivity{}},
                    .thread = basics::ThreadId::current(),
                    .source_location = parent_activity.source_location})}));
}

struct WaitSlot {
  void resume() {
    ready = true;
    _continuation.resume();
  }

  std::coroutine_handle<> _continuation;

  bool await_ready() { return ready; }
  void await_resume() {}
  void await_suspend(std::coroutine_handle<> continuation) {
    _continuation = continuation;
  }

  bool ready = false;
};

TEST_F(ActivityRegistryTest, a_base_activity_lives_as_long_as_its_child) {
  WaitSlot wait;
  ActivityInRegistrySnapshot parent_activity_snapshot;
  ActivityInRegistrySnapshot child_activity_snapshot;
  {
    auto parent_activity = MyActivity{"parent activity"};

    auto activities_in_registry = get_all_activities();
    EXPECT_EQ(activities_in_registry.size(), 1);
    EXPECT_EQ(activities_in_registry[0],
              (ActivityInRegistrySnapshot{
                  .name = "parent activity",
                  .state = State::Running,
                  .id = parent_activity.id(),
                  .parent = {RootActivity{}},
                  .thread = basics::ThreadId::current(),
                  .source_location = parent_activity.source_location}));
    parent_activity_snapshot = activities_in_registry[0];

    std::ignore = [&wait, parent_activity_snapshot,
                   &child_activity_snapshot]() -> async<void> {
      auto child_activity = MyActivity{"child activity"};

      auto activities_in_registry = get_all_activities();
      EXPECT_EQ(activities_in_registry.size(), 2);
      EXPECT_EQ(activities_in_registry[0],
                (ActivityInRegistrySnapshot{
                    .name = "child activity",
                    .state = State::Running,
                    .id = child_activity.id(),
                    .parent = {ActivityId{parent_activity_snapshot.id}},
                    .thread = basics::ThreadId::current(),
                    .source_location = child_activity.source_location}));
      child_activity_snapshot = activities_in_registry[0];
      EXPECT_EQ(activities_in_registry[1], parent_activity_snapshot);
      co_await wait;
      co_return;
    }();
  }

  // both activity-in-registries still exist:
  // child lives in suspended coroutine and references parent
  // although parent scope is deleted
  get_thread_registry().garbage_collect();  // does not do anything
  EXPECT_EQ(get_all_activities(),
            (std::vector<ActivityInRegistrySnapshot>{
                child_activity_snapshot,
                parent_activity_snapshot.update_state(State::Finished)}));

  // resume coroutine, mark child for deletion at end of coroutine and mark
  // parent for deletion at end of scope
  wait.resume();
}

TEST_F(ActivityRegistryTest, create_another_activity_after_child_suspended) {
  WaitSlot wait;
  ActivityInRegistrySnapshot parent_activity_snapshot;
  ActivityInRegistrySnapshot child_activity_snapshot;
  {
    auto parent_activity = MyActivity{"parent activity"};

    auto activities_in_registry = get_all_activities();
    EXPECT_EQ(activities_in_registry.size(), 1);
    EXPECT_EQ(activities_in_registry[0],
              (ActivityInRegistrySnapshot{
                  .name = "parent activity",
                  .state = State::Running,
                  .id = parent_activity.id(),
                  .parent = {RootActivity{}},
                  .thread = basics::ThreadId::current(),
                  .source_location = parent_activity.source_location}));
    parent_activity_snapshot = activities_in_registry[0];

    std::ignore = [&wait, parent_activity_snapshot,
                   &child_activity_snapshot]() -> async<void> {
      auto child_activity = MyActivity{"child activity"};

      auto activities_in_registry = get_all_activities();
      EXPECT_EQ(activities_in_registry.size(), 2);
      EXPECT_EQ(activities_in_registry[0],
                (ActivityInRegistrySnapshot{
                    .name = "child activity",
                    .state = State::Running,
                    .id = child_activity.id(),
                    .parent = {ActivityId{parent_activity_snapshot.id}},
                    .thread = basics::ThreadId::current(),
                    .source_location = child_activity.source_location}));
      child_activity_snapshot = activities_in_registry[0];
      EXPECT_EQ(activities_in_registry[1], parent_activity_snapshot);
      co_await wait;
      co_return;
    }();

    auto some_other_activity = MyActivity{"some other activity"};

    EXPECT_EQ(get_all_activities(),
              (std::vector<ActivityInRegistrySnapshot>{
                  (ActivityInRegistrySnapshot{
                      .name = "some other activity",
                      .state = State::Running,
                      .id = some_other_activity.id(),
                      .parent = {ActivityId{parent_activity.id()}},
                      .thread = basics::ThreadId::current(),
                      .source_location = some_other_activity.source_location}),
                  child_activity_snapshot, parent_activity_snapshot}));
  }

  auto another_activity = MyActivity{"another activity"};

  get_thread_registry().garbage_collect();  // deletes some_other_activity
  EXPECT_EQ(get_all_activities(),
            (std::vector<ActivityInRegistrySnapshot>{
                (ActivityInRegistrySnapshot{
                    .name = "another activity",
                    .state = State::Running,
                    .id = another_activity.id(),
                    .parent = {RootActivity{}},
                    .thread = basics::ThreadId::current(),
                    .source_location = another_activity.source_location}),
                child_activity_snapshot,
                parent_activity_snapshot.update_state(State::Finished)}));

  // resume coroutine, mark child for deletion at end of coroutine and mark
  // parent for deletion at end of scope
  wait.resume();
}

TEST_F(ActivityRegistryTest, hierarchy_with_different_scopes) {
  WaitSlot wait;
  ActivityInRegistrySnapshot parent_activity_snapshot;
  ActivityInRegistrySnapshot child_activity_snapshot;
  ActivityInRegistrySnapshot child_of_child_activity_snapshot;
  {
    auto parent_activity = MyActivity{"parent activity"};
    auto activities_in_registry = get_all_activities();
    EXPECT_EQ(activities_in_registry.size(), 1);
    EXPECT_EQ(activities_in_registry[0],
              (ActivityInRegistrySnapshot{
                  .name = "parent activity",
                  .state = State::Running,
                  .id = parent_activity.id(),
                  .parent = {RootActivity{}},
                  .thread = basics::ThreadId::current(),
                  .source_location = parent_activity.source_location}));
    parent_activity_snapshot = activities_in_registry[0];

    std::ignore = [&wait, parent_activity_snapshot, &child_activity_snapshot,
                   &child_of_child_activity_snapshot]() -> async<void> {
      auto child_activity = MyActivity{"child activity"};

      auto activities_in_registry = get_all_activities();
      EXPECT_EQ(activities_in_registry.size(), 2);
      EXPECT_EQ(activities_in_registry[0],
                (ActivityInRegistrySnapshot{
                    .name = "child activity",
                    .state = State::Running,
                    .id = child_activity.id(),
                    .parent = {ActivityId{parent_activity_snapshot.id}},
                    .thread = basics::ThreadId::current(),
                    .source_location = child_activity.source_location}));
      child_activity_snapshot = activities_in_registry[0];
      EXPECT_EQ(activities_in_registry[1], parent_activity_snapshot);

      co_await [&wait, parent_activity_snapshot, child_activity_snapshot,
                &child_of_child_activity_snapshot]() -> async<void> {
        auto child_of_child_activity = MyActivity{"child of child activity"};

        auto activities_in_registry = get_all_activities();
        EXPECT_EQ(activities_in_registry.size(), 3);
        EXPECT_EQ(
            activities_in_registry[0],
            (ActivityInRegistrySnapshot{
                .name = "child of child activity",
                .state = State::Running,
                .id = child_of_child_activity.id(),
                .parent = {ActivityId{child_activity_snapshot.id}},
                .thread = basics::ThreadId::current(),
                .source_location = child_of_child_activity.source_location}));
        child_of_child_activity_snapshot = activities_in_registry[0];
        EXPECT_EQ(activities_in_registry[1], child_activity_snapshot);
        EXPECT_EQ(activities_in_registry[2], parent_activity_snapshot);

        co_await wait;
        co_return;
      }();

      co_return;
    }();
  }

  // both activity-in-registries still exist:
  // child lives in suspended coroutine and references parent
  get_thread_registry().garbage_collect();  // does not do anything
  EXPECT_EQ(get_all_activities(),
            (std::vector<ActivityInRegistrySnapshot>{
                child_of_child_activity_snapshot, child_activity_snapshot,
                parent_activity_snapshot.update_state(State::Finished)}));

  // resume coroutine, mark child of child for deletion, child and parent are
  // marked for deletion when child of child is destroyed
  wait.resume();
}

TEST_F(ActivityRegistryTest,
       a_base_activity_lives_as_long_as_its_longest_living_child) {
  WaitSlot first_wait;
  WaitSlot second_wait;
  ActivityInRegistrySnapshot parent_activity_snapshot;
  ActivityInRegistrySnapshot first_child_activity_snapshot;
  ActivityInRegistrySnapshot second_child_activity_snapshot;
  ActivityInRegistrySnapshot child_of_second_child_activity_snapshot;
  {
    auto parent_activity = MyActivity{"parent activity"};
    auto activities_in_registry = get_all_activities();
    EXPECT_EQ(activities_in_registry.size(), 1);
    EXPECT_EQ(activities_in_registry[0],
              (ActivityInRegistrySnapshot{
                  .name = "parent activity",
                  .state = State::Running,
                  .id = parent_activity.id(),
                  .parent = {RootActivity{}},
                  .thread = basics::ThreadId::current(),
                  .source_location = parent_activity.source_location}));
    parent_activity_snapshot = activities_in_registry[0];

    std::ignore = [&first_wait, parent_activity_snapshot,
                   &first_child_activity_snapshot]() -> async<void> {
      auto child_activity = MyActivity{"first child activity"};

      auto activities_in_registry = get_all_activities();
      EXPECT_EQ(activities_in_registry.size(), 2);
      EXPECT_EQ(activities_in_registry[0],
                (ActivityInRegistrySnapshot{
                    .name = "first child activity",
                    .state = State::Running,
                    .id = child_activity.id(),
                    .parent = {ActivityId{parent_activity_snapshot.id}},
                    .thread = basics::ThreadId::current(),
                    .source_location = child_activity.source_location}));
      first_child_activity_snapshot = activities_in_registry[0];
      EXPECT_EQ(activities_in_registry[1], parent_activity_snapshot);

      co_await first_wait;
      co_return;
    }();

    auto second_child_activity = MyActivity{"second child activity"};
    activities_in_registry = get_all_activities();
    EXPECT_EQ(activities_in_registry.size(), 3);
    EXPECT_EQ(activities_in_registry[0],
              (ActivityInRegistrySnapshot{
                  .name = "second child activity",
                  .state = State::Running,
                  .id = second_child_activity.id(),
                  .parent = {ActivityId{parent_activity_snapshot.id}},
                  .thread = basics::ThreadId::current(),
                  .source_location = second_child_activity.source_location}));
    EXPECT_EQ(activities_in_registry[1], first_child_activity_snapshot);
    EXPECT_EQ(activities_in_registry[2], parent_activity_snapshot);
    second_child_activity_snapshot = activities_in_registry[0];

    std::ignore = [&second_wait, parent_activity_snapshot,
                   first_child_activity_snapshot,
                   second_child_activity_snapshot,
                   &child_of_second_child_activity_snapshot]() -> async<void> {
      auto child_of_child_activity =
          MyActivity{"child of second child activity"};

      auto activities_in_registry = get_all_activities();
      EXPECT_EQ(activities_in_registry.size(), 4);
      EXPECT_EQ(
          activities_in_registry[0],
          (ActivityInRegistrySnapshot{
              .name = "child of second child activity",
              .state = State::Running,
              .id = child_of_child_activity.id(),
              .parent = {ActivityId{second_child_activity_snapshot.id}},
              .thread = basics::ThreadId::current(),
              .source_location = child_of_child_activity.source_location}));
      child_of_second_child_activity_snapshot = activities_in_registry[0];
      EXPECT_EQ(activities_in_registry[1], second_child_activity_snapshot);
      EXPECT_EQ(activities_in_registry[2], first_child_activity_snapshot);
      EXPECT_EQ(activities_in_registry[3], parent_activity_snapshot);

      co_await second_wait;
      co_return;
    }();
  }

  // all four activity-in-registries still exist:
  // childs live in suspended coroutines and reference its parents
  get_thread_registry().garbage_collect();  // does not do anything
  EXPECT_EQ(get_all_activities(),
            (std::vector<ActivityInRegistrySnapshot>{
                child_of_second_child_activity_snapshot,
                second_child_activity_snapshot.update_state(State::Finished),
                first_child_activity_snapshot,
                parent_activity_snapshot.update_state(State::Finished)}));

  // marks child of second child and second child for deletion, parent is still
  // in scope
  second_wait.resume();
  get_thread_registry().garbage_collect();
  EXPECT_EQ(get_all_activities(),
            (std::vector<ActivityInRegistrySnapshot>{
                first_child_activity_snapshot,
                parent_activity_snapshot.update_state(State::Finished)}));

  // marks first child and parent for deletion
  first_wait.resume();
  get_thread_registry().garbage_collect();
  EXPECT_EQ(get_all_activities().size(), 0);
}

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
#include "TaskMonitoring/task.h"
#include "TaskMonitoring/task_registry_variable.h"
#include "Inspection/JsonPrintInspector.h"
#include <gtest/gtest.h>

#include <coroutine>
#include <thread>

using namespace arangodb;
using namespace arangodb::task_monitoring;

namespace {
auto get_all_tasks() -> std::vector<TaskSnapshot> {
  std::vector<TaskSnapshot> tasks;
  registry.for_node(
      [&](TaskSnapshot task) { tasks.emplace_back(std::move(task)); });
  return tasks;
}

struct MyTask : public Task {
  basics::SourceLocationSnapshot source_location;
  MyTask(std::string name,
         std::source_location loc = std::source_location::current())
      : Task{std::move(name), loc},
        source_location{basics::SourceLocationSnapshot::from(std::move(loc))} {}
};

}  // namespace

struct TaskRegistryTest : ::testing::Test {
  void TearDown() override {
    get_thread_registry().garbage_collect();
    EXPECT_EQ(get_all_tasks().size(), 0);
  }
};

TEST_F(TaskRegistryTest, a_base_task_creates_a_root_task) {
  auto task = MyTask{"test task"};

  EXPECT_EQ(get_all_tasks(), (std::vector<TaskSnapshot>{(TaskSnapshot{
                                 .name = "test task",
                                 .state = State::Running,
                                 .id = task.id(),
                                 .parent = {RootTask{}},
                                 .thread = basics::ThreadId::current(),
                                 .source_location = task.source_location})}));
}

TEST_F(TaskRegistryTest, creates_a_child_task) {
  auto parent_task = MyTask{"parent task"};
  auto child_task = MyTask{"child task"};

  EXPECT_EQ(
      get_all_tasks(),
      (std::vector<TaskSnapshot>{
          (TaskSnapshot{.name = "child task",
                        .state = State::Running,
                        .id = child_task.id(),
                        .parent = {TaskId{parent_task.id()}},
                        .thread = basics::ThreadId::current(),
                        .source_location = child_task.source_location}),
          (TaskSnapshot{.name = "parent task",
                        .state = State::Running,
                        .id = parent_task.id(),
                        .parent = {RootTask{}},
                        .thread = basics::ThreadId::current(),
                        .source_location = parent_task.source_location})}));
}

TEST_F(TaskRegistryTest, creates_a_child_task_hierarchy) {
  auto parent_task = MyTask{"parent task"};
  auto child_task = MyTask{"child task"};
  auto child_of_child_task = MyTask{"child of child task"};
  auto child_of_child_of_child_task = MyTask{"child of child of child task"};

  EXPECT_EQ(
      get_all_tasks(),
      (std::vector<TaskSnapshot>{
          (TaskSnapshot{
              .name = "child of child of child task",
              .state = State::Running,
              .id = child_of_child_of_child_task.id(),
              .parent = {TaskId{child_of_child_task.id()}},
              .thread = basics::ThreadId::current(),
              .source_location = child_of_child_of_child_task.source_location}),
          (TaskSnapshot{
              .name = "child of child task",
              .state = State::Running,
              .id = child_of_child_task.id(),
              .parent = {TaskId{child_task.id()}},
              .thread = basics::ThreadId::current(),
              .source_location = child_of_child_task.source_location}),
          (TaskSnapshot{.name = "child task",
                        .state = State::Running,
                        .id = child_task.id(),
                        .parent = {TaskId{parent_task.id()}},
                        .thread = basics::ThreadId::current(),
                        .source_location = child_task.source_location}),
          (TaskSnapshot{.name = "parent task",
                        .state = State::Running,
                        .id = parent_task.id(),
                        .parent = {RootTask{}},
                        .thread = basics::ThreadId::current(),
                        .source_location = parent_task.source_location})}));
}

TEST_F(TaskRegistryTest, uses_correct_parent_task) {
  auto parent_task = MyTask{"parent task"};
  {
    auto first_child_task = MyTask{"first child task"};

    EXPECT_EQ(
        get_all_tasks(),
        (std::vector<TaskSnapshot>{
            (TaskSnapshot{.name = "first child task",
                          .state = State::Running,
                          .id = first_child_task.id(),
                          .parent = {TaskId{parent_task.id()}},
                          .thread = basics::ThreadId::current(),
                          .source_location = first_child_task.source_location}),
            (TaskSnapshot{.name = "parent task",
                          .state = State::Running,
                          .id = parent_task.id(),
                          .parent = {RootTask{}},
                          .thread = basics::ThreadId::current(),
                          .source_location = parent_task.source_location})}));
  }
  get_thread_registry().garbage_collect();

  auto second_child_task = MyTask{"second child task"};

  EXPECT_EQ(
      get_all_tasks(),
      (std::vector<TaskSnapshot>{
          (TaskSnapshot{.name = "second child task",
                        .state = State::Running,
                        .id = second_child_task.id(),
                        .parent = {TaskId{parent_task.id()}},
                        .thread = basics::ThreadId::current(),
                        .source_location = second_child_task.source_location}),
          (TaskSnapshot{.name = "parent task",
                        .state = State::Running,
                        .id = parent_task.id(),
                        .parent = {RootTask{}},
                        .thread = basics::ThreadId::current(),
                        .source_location = parent_task.source_location})}));
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

TEST_F(TaskRegistryTest, a_base_task_lives_as_long_as_its_child) {
  WaitSlot wait;
  TaskSnapshot parent_task_snapshot;
  TaskSnapshot child_task_snapshot;
  {
    auto parent_task = MyTask{"parent task"};

    auto tasks_in_registry = get_all_tasks();
    EXPECT_EQ(tasks_in_registry.size(), 1);
    EXPECT_EQ(tasks_in_registry[0],
              (TaskSnapshot{.name = "parent task",
                            .state = State::Running,
                            .id = parent_task.id(),
                            .parent = {RootTask{}},
                            .thread = basics::ThreadId::current(),
                            .source_location = parent_task.source_location}));
    parent_task_snapshot = tasks_in_registry[0];

    std::ignore = [&wait, parent_task_snapshot,
                   &child_task_snapshot]() -> async<void> {
      auto child_task = MyTask{"child task"};

      auto tasks_in_registry = get_all_tasks();
      EXPECT_EQ(tasks_in_registry.size(), 2);
      EXPECT_EQ(tasks_in_registry[0],
                (TaskSnapshot{.name = "child task",
                              .state = State::Running,
                              .id = child_task.id(),
                              .parent = {TaskId{parent_task_snapshot.id}},
                              .thread = basics::ThreadId::current(),
                              .source_location = child_task.source_location}));
      child_task_snapshot = tasks_in_registry[0];
      EXPECT_EQ(tasks_in_registry[1], parent_task_snapshot);
      co_await wait;
      co_return;
    }();
  }

  // both task-in-registries still exist:
  // child lives in suspended coroutine and references parent
  // although parent scope is deleted
  get_thread_registry().garbage_collect();  // does not do anything
  EXPECT_EQ(get_all_tasks(),
            (std::vector<TaskSnapshot>{
                child_task_snapshot,
                parent_task_snapshot.update_state(State::Finished)}));

  // resume coroutine, mark child for deletion at end of coroutine and mark
  // parent for deletion at end of scope
  wait.resume();
}

TEST_F(TaskRegistryTest, create_another_task_after_child_suspended) {
  WaitSlot wait;
  TaskSnapshot parent_task_snapshot;
  TaskSnapshot child_task_snapshot;
  {
    auto parent_task = MyTask{"parent task"};

    auto tasks_in_registry = get_all_tasks();
    EXPECT_EQ(tasks_in_registry.size(), 1);
    EXPECT_EQ(tasks_in_registry[0],
              (TaskSnapshot{.name = "parent task",
                            .state = State::Running,
                            .id = parent_task.id(),
                            .parent = {RootTask{}},
                            .thread = basics::ThreadId::current(),
                            .source_location = parent_task.source_location}));
    parent_task_snapshot = tasks_in_registry[0];

    std::ignore = [&wait, parent_task_snapshot,
                   &child_task_snapshot]() -> async<void> {
      auto child_task = MyTask{"child task"};

      auto tasks_in_registry = get_all_tasks();
      EXPECT_EQ(tasks_in_registry.size(), 2);
      EXPECT_EQ(tasks_in_registry[0],
                (TaskSnapshot{.name = "child task",
                              .state = State::Running,
                              .id = child_task.id(),
                              .parent = {TaskId{parent_task_snapshot.id}},
                              .thread = basics::ThreadId::current(),
                              .source_location = child_task.source_location}));
      child_task_snapshot = tasks_in_registry[0];
      EXPECT_EQ(tasks_in_registry[1], parent_task_snapshot);
      co_await wait;
      co_return;
    }();

    auto some_other_task = MyTask{"some other task"};

    EXPECT_EQ(
        get_all_tasks(),
        (std::vector<TaskSnapshot>{
            (TaskSnapshot{.name = "some other task",
                          .state = State::Running,
                          .id = some_other_task.id(),
                          .parent = {TaskId{parent_task.id()}},
                          .thread = basics::ThreadId::current(),
                          .source_location = some_other_task.source_location}),
            child_task_snapshot, parent_task_snapshot}));
  }

  auto another_task = MyTask{"another task"};

  get_thread_registry().garbage_collect();  // deletes some_other_task
  EXPECT_EQ(get_all_tasks(),
            (std::vector<TaskSnapshot>{
                (TaskSnapshot{.name = "another task",
                              .state = State::Running,
                              .id = another_task.id(),
                              .parent = {RootTask{}},
                              .thread = basics::ThreadId::current(),
                              .source_location = another_task.source_location}),
                child_task_snapshot,
                parent_task_snapshot.update_state(State::Finished)}));

  // resume coroutine, mark child for deletion at end of coroutine and mark
  // parent for deletion at end of scope
  wait.resume();
}

TEST_F(TaskRegistryTest, hierarchy_with_different_scopes) {
  WaitSlot wait;
  TaskSnapshot parent_task_snapshot;
  TaskSnapshot child_task_snapshot;
  TaskSnapshot child_of_child_task_snapshot;
  {
    auto parent_task = MyTask{"parent task"};
    auto tasks_in_registry = get_all_tasks();
    EXPECT_EQ(tasks_in_registry.size(), 1);
    EXPECT_EQ(tasks_in_registry[0],
              (TaskSnapshot{.name = "parent task",
                            .state = State::Running,
                            .id = parent_task.id(),
                            .parent = {RootTask{}},
                            .thread = basics::ThreadId::current(),
                            .source_location = parent_task.source_location}));
    parent_task_snapshot = tasks_in_registry[0];

    std::ignore = [&wait, parent_task_snapshot, &child_task_snapshot,
                   &child_of_child_task_snapshot]() -> async<void> {
      auto child_task = MyTask{"child task"};

      auto tasks_in_registry = get_all_tasks();
      EXPECT_EQ(tasks_in_registry.size(), 2);
      EXPECT_EQ(tasks_in_registry[0],
                (TaskSnapshot{.name = "child task",
                              .state = State::Running,
                              .id = child_task.id(),
                              .parent = {TaskId{parent_task_snapshot.id}},
                              .thread = basics::ThreadId::current(),
                              .source_location = child_task.source_location}));
      child_task_snapshot = tasks_in_registry[0];
      EXPECT_EQ(tasks_in_registry[1], parent_task_snapshot);

      co_await [&wait, parent_task_snapshot, child_task_snapshot,
                &child_of_child_task_snapshot]() -> async<void> {
        auto child_of_child_task = MyTask{"child of child task"};

        auto tasks_in_registry = get_all_tasks();
        EXPECT_EQ(tasks_in_registry.size(), 3);
        EXPECT_EQ(tasks_in_registry[0],
                  (TaskSnapshot{
                      .name = "child of child task",
                      .state = State::Running,
                      .id = child_of_child_task.id(),
                      .parent = {TaskId{child_task_snapshot.id}},
                      .thread = basics::ThreadId::current(),
                      .source_location = child_of_child_task.source_location}));
        child_of_child_task_snapshot = tasks_in_registry[0];
        EXPECT_EQ(tasks_in_registry[1], child_task_snapshot);
        EXPECT_EQ(tasks_in_registry[2], parent_task_snapshot);

        co_await wait;
        co_return;
      }();

      co_return;
    }();
  }

  // both task-in-registries still exist:
  // child lives in suspended coroutine and references parent
  get_thread_registry().garbage_collect();  // does not do anything
  EXPECT_EQ(get_all_tasks(),
            (std::vector<TaskSnapshot>{
                child_of_child_task_snapshot, child_task_snapshot,
                parent_task_snapshot.update_state(State::Finished)}));

  // resume coroutine, mark child of child for deletion, child and parent are
  // marked for deletion when child of child is destroyed
  wait.resume();
}

TEST_F(TaskRegistryTest,
       a_base_task_lives_as_long_as_its_longest_living_child) {
  WaitSlot first_wait;
  WaitSlot second_wait;
  TaskSnapshot parent_task_snapshot;
  TaskSnapshot first_child_task_snapshot;
  TaskSnapshot second_child_task_snapshot;
  TaskSnapshot child_of_second_child_task_snapshot;
  {
    auto parent_task = MyTask{"parent task"};
    auto tasks_in_registry = get_all_tasks();
    EXPECT_EQ(tasks_in_registry.size(), 1);
    EXPECT_EQ(tasks_in_registry[0],
              (TaskSnapshot{.name = "parent task",
                            .state = State::Running,
                            .id = parent_task.id(),
                            .parent = {RootTask{}},
                            .thread = basics::ThreadId::current(),
                            .source_location = parent_task.source_location}));
    parent_task_snapshot = tasks_in_registry[0];

    std::ignore = [&first_wait, parent_task_snapshot,
                   &first_child_task_snapshot]() -> async<void> {
      auto child_task = MyTask{"first child task"};

      auto tasks_in_registry = get_all_tasks();
      EXPECT_EQ(tasks_in_registry.size(), 2);
      EXPECT_EQ(tasks_in_registry[0],
                (TaskSnapshot{.name = "first child task",
                              .state = State::Running,
                              .id = child_task.id(),
                              .parent = {TaskId{parent_task_snapshot.id}},
                              .thread = basics::ThreadId::current(),
                              .source_location = child_task.source_location}));
      first_child_task_snapshot = tasks_in_registry[0];
      EXPECT_EQ(tasks_in_registry[1], parent_task_snapshot);

      co_await first_wait;
      co_return;
    }();

    auto second_child_task = MyTask{"second child task"};
    tasks_in_registry = get_all_tasks();
    EXPECT_EQ(tasks_in_registry.size(), 3);
    EXPECT_EQ(
        tasks_in_registry[0],
        (TaskSnapshot{.name = "second child task",
                      .state = State::Running,
                      .id = second_child_task.id(),
                      .parent = {TaskId{parent_task_snapshot.id}},
                      .thread = basics::ThreadId::current(),
                      .source_location = second_child_task.source_location}));
    EXPECT_EQ(tasks_in_registry[1], first_child_task_snapshot);
    EXPECT_EQ(tasks_in_registry[2], parent_task_snapshot);
    second_child_task_snapshot = tasks_in_registry[0];

    std::ignore = [&second_wait, parent_task_snapshot,
                   first_child_task_snapshot, second_child_task_snapshot,
                   &child_of_second_child_task_snapshot]() -> async<void> {
      auto child_of_child_task = MyTask{"child of second child task"};

      auto tasks_in_registry = get_all_tasks();
      EXPECT_EQ(tasks_in_registry.size(), 4);
      EXPECT_EQ(tasks_in_registry[0],
                (TaskSnapshot{
                    .name = "child of second child task",
                    .state = State::Running,
                    .id = child_of_child_task.id(),
                    .parent = {TaskId{second_child_task_snapshot.id}},
                    .thread = basics::ThreadId::current(),
                    .source_location = child_of_child_task.source_location}));
      child_of_second_child_task_snapshot = tasks_in_registry[0];
      EXPECT_EQ(tasks_in_registry[1], second_child_task_snapshot);
      EXPECT_EQ(tasks_in_registry[2], first_child_task_snapshot);
      EXPECT_EQ(tasks_in_registry[3], parent_task_snapshot);

      co_await second_wait;
      co_return;
    }();
  }

  // all four task-in-registries still exist:
  // childs live in suspended coroutines and reference its parents
  get_thread_registry().garbage_collect();  // does not do anything
  EXPECT_EQ(get_all_tasks(),
            (std::vector<TaskSnapshot>{
                child_of_second_child_task_snapshot,
                second_child_task_snapshot.update_state(State::Finished),
                first_child_task_snapshot,
                parent_task_snapshot.update_state(State::Finished)}));

  // marks child of second child and second child for deletion, parent is still
  // in scope
  second_wait.resume();
  get_thread_registry().garbage_collect();
  EXPECT_EQ(get_all_tasks(),
            (std::vector<TaskSnapshot>{
                first_child_task_snapshot,
                parent_task_snapshot.update_state(State::Finished)}));

  // marks first child and parent for deletion
  first_wait.resume();
  get_thread_registry().garbage_collect();
  EXPECT_EQ(get_all_tasks().size(), 0);
}

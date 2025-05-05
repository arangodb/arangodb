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
  MyTask(std::string name, Task& task,
         std::source_location loc = std::source_location::current())
      : Task{std::move(name), task, loc},
        source_location{basics::SourceLocationSnapshot::from(std::move(loc))} {}
};
}  // namespace

struct TaskRegistryTest : ::testing::Test {
  void TearDown() override {
    // garbage collection has to run at most twice in order to clean everything
    // up on the current thread:
    // - when a child task scope is deleted, the child's task-in-registry is
    //   marked for deletion
    // - at this point its parent task scope can still exist, therefore it is
    //   not marked for deletion inside the child task scope destructor
    // - when then the parent task scope is deleted, the parent's
    //   task-in-registry is still referenced by the child's task-in-registry
    //   (which is not yet deleted), therefore it is not yet marked for deletion

    // the first gc run destroys the child's task-in-registry
    // which destroys the last reference to the parent's task-in-registry, which
    // is therfore marked for deletion (together with all remaining
    // task-in-registries higher up in the hierarchy that are not referenced by
    // any other tasks)
    get_thread_registry().garbage_collect();
    // the second gc run destroys the parent's task-in-registry (and possibly
    // other marked for deletion items)
    get_thread_registry().garbage_collect();
    EXPECT_EQ(get_all_tasks().size(), 0);
  }
};

TEST_F(TaskRegistryTest, a_base_task_creates_a_root_task) {
  auto task = MyTask{"test task"};

  EXPECT_EQ(get_all_tasks(), (std::vector<TaskSnapshot>{(TaskSnapshot{
                                 .name = "test task",
                                 .state = "running",
                                 .id = task.id(),
                                 .parent = {RootTask{}},
                                 .thread = basics::ThreadId::current(),
                                 .source_location = task.source_location})}));
}

TEST_F(TaskRegistryTest, creates_a_child_task) {
  auto parent_task = MyTask{"parent task"};
  auto child_task = MyTask{"child task", parent_task};

  EXPECT_EQ(
      get_all_tasks(),
      (std::vector<TaskSnapshot>{
          (TaskSnapshot{.name = "child task",
                        .state = "running",
                        .id = child_task.id(),
                        .parent = {TaskIdWrapper{parent_task.id()}},
                        .thread = basics::ThreadId::current(),
                        .source_location = child_task.source_location}),
          (TaskSnapshot{.name = "parent task",
                        .state = "running",
                        .id = parent_task.id(),
                        .parent = {RootTask{}},
                        .thread = basics::ThreadId::current(),
                        .source_location = parent_task.source_location})}));
}

TEST_F(TaskRegistryTest, creates_a_child_task_hierarchy) {
  auto parent_task = MyTask{"parent task"};
  auto child_task = MyTask{"child task", parent_task};
  auto child_of_child_task = MyTask{"child of child task", child_task};
  auto child_of_child_of_child_task =
      MyTask{"child of child of child task", child_of_child_task};

  EXPECT_EQ(
      get_all_tasks(),
      (std::vector<TaskSnapshot>{
          (TaskSnapshot{
              .name = "child of child of child task",
              .state = "running",
              .id = child_of_child_of_child_task.id(),
              .parent = {TaskIdWrapper{child_of_child_task.id()}},
              .thread = basics::ThreadId::current(),
              .source_location = child_of_child_of_child_task.source_location}),
          (TaskSnapshot{
              .name = "child of child task",
              .state = "running",
              .id = child_of_child_task.id(),
              .parent = {TaskIdWrapper{child_task.id()}},
              .thread = basics::ThreadId::current(),
              .source_location = child_of_child_task.source_location}),
          (TaskSnapshot{.name = "child task",
                        .state = "running",
                        .id = child_task.id(),
                        .parent = {TaskIdWrapper{parent_task.id()}},
                        .thread = basics::ThreadId::current(),
                        .source_location = child_task.source_location}),
          (TaskSnapshot{.name = "parent task",
                        .state = "running",
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
  {
    auto parent_task = MyTask{"parent task"};
    std::ignore = [&parent_task, &wait]() -> async<void> {
      auto child_task = MyTask{"child task", parent_task};
      EXPECT_EQ(
          get_all_tasks(),
          (std::vector<TaskSnapshot>{
              (TaskSnapshot{.name = "child task",
                            .state = "running",
                            .id = child_task.id(),
                            .parent = {TaskIdWrapper{parent_task.id()}},
                            .thread = basics::ThreadId::current(),
                            .source_location = child_task.source_location}),
              (TaskSnapshot{.name = "parent task",
                            .state = "running",
                            .id = parent_task.id(),
                            .parent = {RootTask{}},
                            .thread = basics::ThreadId::current(),
                            .source_location = parent_task.source_location})}));
      co_await wait;
      co_return;
    }();
  }

  // both task-in-registries still exist:
  // child lives in suspended coroutine and references parent
  get_thread_registry().garbage_collect();  // does not do anything
  EXPECT_EQ(get_all_tasks().size(), 2);

  // resume coroutine, mark child for deletion at end of coroutine and mark
  // parent for deletion at end of scope
  wait.resume();
}

TEST_F(TaskRegistryTest, hierarchy_with_different_scopes) {
  WaitSlot wait;
  {
    auto parent_task = MyTask{"parent task"};

    std::ignore = [&parent_task, &wait]() -> async<void> {
      auto child_task = MyTask{"child task", parent_task};
      EXPECT_EQ(
          get_all_tasks(),
          (std::vector<TaskSnapshot>{
              (TaskSnapshot{.name = "child task",
                            .state = "running",
                            .id = child_task.id(),
                            .parent = {TaskIdWrapper{parent_task.id()}},
                            .thread = basics::ThreadId::current(),
                            .source_location = child_task.source_location}),
              (TaskSnapshot{.name = "parent task",
                            .state = "running",
                            .id = parent_task.id(),
                            .parent = {RootTask{}},
                            .thread = basics::ThreadId::current(),
                            .source_location = parent_task.source_location})}));

      co_await [&parent_task, &child_task, &wait,
                first_child_task = get_all_tasks()[0]]() -> async<void> {
        auto child_of_child_task = MyTask{"child of child task", child_task};
        EXPECT_EQ(get_all_tasks(),
                  (std::vector<TaskSnapshot>{
                      (TaskSnapshot{.name = "child of child task",
                                    .state = "running",
                                    .id = child_of_child_task.id(),
                                    .parent = {TaskIdWrapper{child_task.id()}},
                                    .thread = basics::ThreadId::current(),
                                    .source_location =
                                        child_of_child_task.source_location}),
                      first_child_task,
                      (TaskSnapshot{
                          .name = "parent task",
                          .state = "running",
                          .id = parent_task.id(),
                          .parent = {RootTask{}},
                          .thread = basics::ThreadId::current(),
                          .source_location = parent_task.source_location})}));
        co_await wait;
        co_return;
      }();

      co_return;
    }();
  }

  // both task-in-registries still exist:
  // child lives in suspended coroutine and references parent
  get_thread_registry().garbage_collect();  // does not do anything
  EXPECT_EQ(get_all_tasks().size(), 3);

  // resume coroutine, mark child of child for deletion, child and parent are
  // marked for deletion when child of child is destroyed
  wait.resume();
}

TEST_F(TaskRegistryTest,
       a_base_task_lives_as_long_as_its_longest_living_child) {
  WaitSlot first_wait;
  WaitSlot second_wait;
  TaskSnapshot first_child_task;
  {
    auto parent_task = MyTask{"parent task"};
    auto a = [&parent_task, &first_wait, &first_child_task]() -> async<void> {
      auto child_task = MyTask{"first child task", parent_task};
      EXPECT_EQ(
          get_all_tasks(),
          (std::vector<TaskSnapshot>{
              (TaskSnapshot{.name = "first child task",
                            .state = "running",
                            .id = child_task.id(),
                            .parent = {TaskIdWrapper{parent_task.id()}},
                            .thread = basics::ThreadId::current(),
                            .source_location = child_task.source_location}),
              (TaskSnapshot{.name = "parent task",
                            .state = "running",
                            .id = parent_task.id(),
                            .parent = {RootTask{}},
                            .thread = basics::ThreadId::current(),
                            .source_location = parent_task.source_location})}));
      first_child_task = get_all_tasks()[0];
      co_await first_wait;
      co_return;
    }();
    auto b = [&parent_task, &second_wait, &first_child_task]() -> async<void> {
      auto child_task = MyTask{"second child task", parent_task};
      EXPECT_EQ(
          get_all_tasks(),
          (std::vector<TaskSnapshot>{
              (TaskSnapshot{.name = "second child task",
                            .state = "running",
                            .id = child_task.id(),
                            .parent = {TaskIdWrapper{parent_task.id()}},
                            .thread = basics::ThreadId::current(),
                            .source_location = child_task.source_location}),
              first_child_task,
              (TaskSnapshot{.name = "parent task",
                            .state = "running",
                            .id = parent_task.id(),
                            .parent = {RootTask{}},
                            .thread = basics::ThreadId::current(),
                            .source_location = parent_task.source_location})}));
      co_await second_wait;
      co_return;
    }();
  }

  // all three task-in-registries still exist:
  // childs live in suspended coroutines and reference parent
  get_thread_registry().garbage_collect();  // does not do anything
  EXPECT_EQ(get_all_tasks().size(), 3);

  // marks second child for deletion, parent is still in scope
  second_wait.resume();
  get_thread_registry().garbage_collect();
  EXPECT_EQ(get_all_tasks().size(), 2);
  EXPECT_EQ(get_all_tasks()[0].name, "first child task");
  EXPECT_EQ(get_all_tasks()[1].name, "parent task");

  // marks first child for deletion, parent is still referenced by both first
  // and second task-in-registry and will only be marked for deleted when both
  // are deleted
  first_wait.resume();
  get_thread_registry().garbage_collect();
  EXPECT_EQ(get_all_tasks().size(), 0);
}

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

struct MyBaseTask : public BaseTask {
  basics::SourceLocationSnapshot source_location;
  MyBaseTask(std::string name,
             std::source_location loc = std::source_location::current())
      : BaseTask{std::move(name), loc},
        source_location{basics::SourceLocationSnapshot::from(std::move(loc))} {}
};
struct MyChildTask : public ChildTask {
  basics::SourceLocationSnapshot source_location;
  MyChildTask(std::string name, Task& task,
              std::source_location loc = std::source_location::current())
      : ChildTask{std::move(name), task, loc},
        source_location{basics::SourceLocationSnapshot::from(std::move(loc))} {}
};
}  // namespace

struct TaskRegistryTest : ::testing::Test {
  void TearDown() override {
    // execute garbage collection on current thread
    get_thread_registry().garbage_collect();
    EXPECT_EQ(get_all_tasks().size(), 0);
  }
};

TEST_F(TaskRegistryTest, a_base_task_creates_a_root_task) {
  auto task = MyBaseTask{"test task"};

  EXPECT_EQ(get_all_tasks(), (std::vector<TaskSnapshot>{(TaskSnapshot{
                                 .name = "test task",
                                 .state = "running",
                                 .id = task.id(),
                                 .parent = {RootTask{}},
                                 .thread = basics::ThreadId::current(),
                                 .source_location = task.source_location})}));
}

TEST_F(TaskRegistryTest, creates_a_child_task) {
  {
    auto parent_task = MyBaseTask{"parent task"};
    auto child_task = MyChildTask{"child task", parent_task};

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
  // here child task-in-registry is marked for deletion
  // but parent task-in-registry is not because it is referenced inside the
  // child task-in-registry as its parent
  // therefore garbage collection has to run twice to clean up everything
  // - in the first run, the child task-in-registry is destroyed
  //   now the shared ptr to parent is empty, therefore its destructor is called
  //   this marks the parent task in registry for deletion
  // - in second run, the parent task-in-registry is destroyed
  get_thread_registry().garbage_collect();
  get_thread_registry().garbage_collect();
  EXPECT_EQ(get_all_tasks().size(), 0);
}

TEST_F(TaskRegistryTest, creates_a_child_task_hierarchy) {
  {
    auto parent_task = MyBaseTask{"parent task"};
    auto child_task = MyChildTask{"child task", parent_task};
    auto child_of_child_task = MyChildTask{"child of child task", child_task};

    EXPECT_EQ(
        get_all_tasks(),
        (std::vector<TaskSnapshot>{
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
  get_thread_registry().garbage_collect();
  get_thread_registry().garbage_collect();
  get_thread_registry().garbage_collect();
  EXPECT_EQ(get_all_tasks().size(), 0);
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
    auto parent_task = MyBaseTask{"parent task"};
    auto a = [&parent_task, &wait]() -> async<void> {
      auto child_task = MyChildTask{"child task", parent_task};
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
  get_thread_registry().garbage_collect();
  EXPECT_EQ(get_all_tasks().size(), 2);

  // resume coroutine, mark child for deletion and therefore also parent
  wait.resume();
  get_thread_registry().garbage_collect();
  get_thread_registry().garbage_collect();
  EXPECT_EQ(get_all_tasks().size(), 0);
}

TEST_F(TaskRegistryTest,
       a_base_task_lives_as_long_as_its_longest_living_child) {
  WaitSlot first_wait;
  WaitSlot second_wait;
  TaskSnapshot first_child_task;
  {
    auto parent_task = MyBaseTask{"parent task"};
    auto a = [&parent_task, &first_wait, &first_child_task]() -> async<void> {
      auto child_task = MyChildTask{"first child task", parent_task};
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
      auto child_task = MyChildTask{"second child task", parent_task};
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
  get_thread_registry().garbage_collect();
  EXPECT_EQ(get_all_tasks().size(), 3);

  // resume second coroutine, mark child for deletion
  second_wait.resume();
  get_thread_registry().garbage_collect();
  get_thread_registry().garbage_collect();
  EXPECT_EQ(get_all_tasks().size(), 2);

  // resume first coroutine, mark child for deletion and therefore also parent
  first_wait.resume();
  get_thread_registry().garbage_collect();
  get_thread_registry().garbage_collect();
  EXPECT_EQ(get_all_tasks().size(), 0);
}

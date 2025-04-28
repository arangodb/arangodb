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

#include "Containers/Concurrent/thread.h"
#include "Tasks/task.h"
#include "Tasks/task_registry_variable.h"
#include "Inspection/JsonPrintInspector.h"
#include <gtest/gtest.h>

#include <thread>

using namespace arangodb;
using namespace arangodb::task_registry;

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
  }
};

// TODO fix naming of base task vs root task
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

TEST_F(TaskRegistryTest, create_child_task) {
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

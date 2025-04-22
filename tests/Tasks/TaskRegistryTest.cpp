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
auto get_all_tasks(Registry& registry) -> std::vector<TaskSnapshot> {
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
}  // namespace

TEST(TaskRegistryTest,
     a_root_task_is_saved_as_a_parent_of_an_entry_point_task) {
  auto task = MyBaseTask{"test task"};

  auto all_tasks = get_all_tasks(registry);
  EXPECT_EQ(all_tasks, (std::vector<TaskSnapshot>{(TaskSnapshot{
                           .name = "test task",
                           .state = "created",
                           .id = task.id(),
                           .parent = {RootTask{}},
                           .thread = basics::ThreadId::current(),
                           .source_location = task.source_location})}));
}

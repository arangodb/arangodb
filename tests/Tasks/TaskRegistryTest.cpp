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

#include "Tasks/task_registry.h"
#include <gtest/gtest.h>

using namespace arangodb::task_registry;

namespace {
auto get_all_tasks(TaskRegistry& registry) -> std::vector<TaskSnapshot> {
  std::vector<TaskSnapshot> tasks;
  registry.for_task(
      [&](TaskSnapshot task) { tasks.emplace_back(std::move(task)); });
  return tasks;
}
}  // namespace

TEST(
    TaskRegistryTest,
    creating_a_root_task_registers_an_entry_point_task_with_the_root_task_as_parent) {
  auto registry = TaskRegistry{};

  auto base_task = registry.create_task("Task");

  ASSERT_EQ(get_all_tasks(registry),
            std::vector<TaskSnapshot>{(TaskSnapshot{
                .name = "entry point",
                .state = "created",
                .id = base_task->id(),
                .parent = ParentTaskSnapshot{RootTask{.name = "Task"}},
                .thread = ThreadId::current()})});
}

TEST(TaskRegistryTest, a_subtask_knows_its_parent) {
  auto registry = TaskRegistry{};

  auto base_task = registry.create_task("Task");
  auto sub_task = registry.create_subtask(base_task, "Subtask");

  ASSERT_EQ(
      get_all_tasks(registry),
      (std::vector<TaskSnapshot>{
          (TaskSnapshot{.name = "entry point",
                        .state = "created",
                        .id = base_task->id(),
                        .parent = ParentTaskSnapshot{RootTask{.name = "Task"}},
                        .thread = ThreadId::current()}),
          (TaskSnapshot{
              .name = "Subtask",
              .state = "created",
              .id = sub_task->id(),
              .parent = ParentTaskSnapshot{TaskIdWrapper{base_task->id()}},
              .thread = ThreadId::current()})}));
}

TEST(TaskRegistryTest, deleted_tasks_are_removed_from_registry) {
  auto registry = TaskRegistry{};

  {
    auto base_task = registry.create_task("Task");

    ASSERT_EQ(get_all_tasks(registry),
              (std::vector<TaskSnapshot>{(TaskSnapshot{
                  .name = "entry point",
                  .state = "created",
                  .id = base_task->id(),
                  .parent = ParentTaskSnapshot{RootTask{.name = "Task"}},
                  .thread = ThreadId::current()})}));
  }  // base_task is destroyed here

  // make sure that entry in TaskRegistry is really deleted

  ASSERT_EQ(get_all_tasks(registry), (std::vector<TaskSnapshot>{}));
}

TEST(TaskRegistryTest, subtask_prevents_task_from_being_destroyed) {
  auto registry = TaskRegistry{};

  {
    std::shared_ptr<Task> sub_task;
    void* base_task_id;
    {
      auto base_task = registry.create_task("Task");
      base_task_id = base_task->id();
      sub_task = registry.create_subtask(base_task, "Subtask");
    }

    ASSERT_EQ(get_all_tasks(registry),
              (std::vector<TaskSnapshot>{
                  (TaskSnapshot{
                      .name = "entry point",
                      .state = "created",
                      .id = base_task_id,
                      .parent = ParentTaskSnapshot{RootTask{.name = "Task"}},
                      .thread = ThreadId::current()}),
                  (TaskSnapshot{
                      .name = "Subtask",
                      .state = "created",
                      .id = sub_task->id(),
                      .parent = ParentTaskSnapshot{TaskIdWrapper{base_task_id}},
                      .thread = ThreadId::current()})}));
  }  // base_task is destroyed here

  // make sure that entry in TaskRegistry is really deleted

  ASSERT_EQ(get_all_tasks(registry), (std::vector<TaskSnapshot>{}));
}

TEST(TaskRegistryTest, scope_starts_and_ends_a_task) {
  auto registry = TaskRegistry{};
  auto base_task = registry.create_task("Task");

  {
    auto scope = base_task->start();

    ASSERT_EQ(get_all_tasks(registry),
              std::vector<TaskSnapshot>{(TaskSnapshot{
                  .name = "entry point",
                  .state = "running",
                  .id = base_task->id(),
                  .parent = ParentTaskSnapshot{RootTask{.name = "Task"}},
                  .thread = ThreadId::current()})});
  }

  ASSERT_EQ(get_all_tasks(registry),
            std::vector<TaskSnapshot>{(TaskSnapshot{
                .name = "entry point",
                .state = "done",
                .id = base_task->id(),
                .parent = ParentTaskSnapshot{RootTask{.name = "Task"}},
                .thread = ThreadId::current()})});
}

TEST(TaskRegistryTest, scope_makes_sure_that_task_thread_is_current_thread) {
  auto registry = TaskRegistry{};
  auto base_task = registry.create_task("Task");
  auto sub_task = registry.create_subtask(base_task, "Subtask", false);
  ASSERT_EQ(
      get_all_tasks(registry),
      (std::vector<TaskSnapshot>{
          (TaskSnapshot{.name = "entry point",
                        .state = "created",
                        .id = base_task->id(),
                        .parent = ParentTaskSnapshot{RootTask{.name = "Task"}},
                        .thread = ThreadId::current()}),
          (TaskSnapshot{
              .name = "Subtask",
              .state = "scheduled",
              .id = sub_task->id(),
              .parent = ParentTaskSnapshot{TaskIdWrapper{base_task->id()}},
              .thread = std::nullopt})}));

  {
    auto scope = sub_task->start();

    ASSERT_EQ(
        get_all_tasks(registry),
        (std::vector<TaskSnapshot>{
            (TaskSnapshot{
                .name = "entry point",
                .state = "created",
                .id = base_task->id(),
                .parent = ParentTaskSnapshot{RootTask{.name = "Task"}},
                .thread = ThreadId::current()}),
            (TaskSnapshot{
                .name = "Subtask",
                .state = "running",
                .id = sub_task->id(),
                .parent = ParentTaskSnapshot{TaskIdWrapper{base_task->id()}},
                .thread = ThreadId::current()})}));
  }

  ASSERT_EQ(
      get_all_tasks(registry),
      (std::vector<TaskSnapshot>{
          (TaskSnapshot{.name = "entry point",
                        .state = "created",
                        .id = base_task->id(),
                        .parent = ParentTaskSnapshot{RootTask{.name = "Task"}},
                        .thread = ThreadId::current()}),
          (TaskSnapshot{
              .name = "Subtask",
              .state = "done",
              .id = sub_task->id(),
              .parent = ParentTaskSnapshot{TaskIdWrapper{base_task->id()}},
              .thread = ThreadId::current()})}));
}

// TEST(TaskRegistryTest, task_can_update_its_state) {
//   auto registry = TaskRegistry{};
//   auto base_task = registry.create_task("Task");
//   auto sub_task = registry.create_subtask(base_task, "Subtask");

//   sub_task->update_state("running");

//   ASSERT_EQ(get_all_tasks(registry),
//             (std::vector<TaskSnapshot>{
//                 (TaskSnapshot{.name = "Task",
//                               .state = "created",
//                               .id = base_task->id(),
//                               .parent = nullptr,
//                               .thread = ThreadId::current()}),
//                 (TaskSnapshot{.name = "Subtask",
//                               .state = "running",
//                               .id = sub_task->id(),
//                               .parent = base_task->id(),
//                               .thread = ThreadId::current()})}));
// }

// TODO make this a death test
// TEST(TaskRegistryTest, a_non_running_subtask_cannot_update_its_state) {}

// TODO make this a death test
// TEST(TaskRegistryTest,
// state_cannot_be_updated_on_differnt_thread_than_running_thread) {}

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

TEST(TaskRegistryTest,
     a_root_task_is_saves_as_a_parent_of_an_entry_point_task) {
  auto registry = TaskRegistry{};

  auto base_scope = registry.start_task("Task");

  ASSERT_EQ(get_all_tasks(registry),
            std::vector<TaskSnapshot>{(TaskSnapshot{
                .name = "entry point",
                .state = "running",
                .id = base_scope.task()->id(),
                .parent = ParentTaskSnapshot{RootTask{.name = "Task"}},
                .thread = ThreadId::current()})});
}

TEST(TaskRegistryTest, scope_ends_a_task) {
  auto registry = TaskRegistry{};

  std::shared_ptr<Task> base_task;
  void* base_task_id;
  {
    auto base_scope = registry.start_task("Task");
    base_task = base_scope.task();
    base_task_id = base_task->id();
  }  // task is not yet destroyed

  ASSERT_EQ(get_all_tasks(registry),
            (std::vector<TaskSnapshot>{(TaskSnapshot{
                .name = "entry point",
                .state = "done",
                .id = base_task_id,
                .parent = ParentTaskSnapshot{RootTask{.name = "Task"}},
                .thread = ThreadId::current()})}));
}

TEST(TaskRegistryTest, a_subtask_knows_its_parent) {
  auto registry = TaskRegistry{};

  auto base_scope = registry.start_task("Task");
  auto sub_scope = registry.start_subtask(base_scope.task(), "Subtask");

  ASSERT_EQ(
      get_all_tasks(registry),
      (std::vector<TaskSnapshot>{
          (TaskSnapshot{.name = "entry point",
                        .state = "running",
                        .id = base_scope.task()->id(),
                        .parent = ParentTaskSnapshot{RootTask{.name = "Task"}},
                        .thread = ThreadId::current()}),
          (TaskSnapshot{.name = "Subtask",
                        .state = "running",
                        .id = sub_scope.task()->id(),
                        .parent = ParentTaskSnapshot{TaskIdWrapper{
                            base_scope.task()->id()}},
                        .thread = ThreadId::current()})}));
}

TEST(TaskRegistryTest, deleted_tasks_are_removed_from_registry) {
  auto registry = TaskRegistry{};

  {
    auto base_scope = registry.start_task("Task");

    ASSERT_EQ(get_all_tasks(registry),
              (std::vector<TaskSnapshot>{(TaskSnapshot{
                  .name = "entry point",
                  .state = "running",
                  .id = base_scope.task()->id(),
                  .parent = ParentTaskSnapshot{RootTask{.name = "Task"}},
                  .thread = ThreadId::current()})}));
  }  // base_task is destroyed here

  // make sure that entry in TaskRegistry is really deleted

  ASSERT_EQ(get_all_tasks(registry), (std::vector<TaskSnapshot>{}));
}

TEST(TaskRegistryTest, subtask_prevents_task_from_being_destroyed) {
  auto registry = TaskRegistry{};

  {
    std::shared_ptr<Task> base_task;
    void* base_task_id;
    {
      auto base_scope = registry.start_task("Task");
      base_task = base_scope.task();
      base_task_id = base_task->id();
    }  // task is not yet destroyed
    auto sub_scope = registry.start_subtask(std::move(base_task), "Subtask");

    ASSERT_EQ(get_all_tasks(registry),
              (std::vector<TaskSnapshot>{
                  (TaskSnapshot{
                      .name = "entry point",
                      .state = "done",
                      .id = base_task_id,
                      .parent = ParentTaskSnapshot{RootTask{.name = "Task"}},
                      .thread = ThreadId::current()}),
                  (TaskSnapshot{
                      .name = "Subtask",
                      .state = "running",
                      .id = sub_scope.task()->id(),
                      .parent = ParentTaskSnapshot{TaskIdWrapper{base_task_id}},
                      .thread = ThreadId::current()})}));
  }  // base_task is destroyed here

  // make sure that entry in TaskRegistry is really deleted

  ASSERT_EQ(get_all_tasks(registry), (std::vector<TaskSnapshot>{}));
}

TEST(TaskRegistryTest,
     a_scheduled_task_needs_to_be_started_manually_on_a_thread) {
  auto registry = TaskRegistry{};
  auto base_scope = registry.start_task("Task");
  auto sub_task = registry.schedule_subtask(base_scope.task(), "Subtask");
  ASSERT_EQ(
      get_all_tasks(registry),
      (std::vector<TaskSnapshot>{
          (TaskSnapshot{.name = "entry point",
                        .state = "running",
                        .id = base_scope.task()->id(),
                        .parent = ParentTaskSnapshot{RootTask{.name = "Task"}},
                        .thread = ThreadId::current()}),
          (TaskSnapshot{.name = "Subtask",
                        .state = "scheduled",
                        .id = sub_task->id(),
                        .parent = ParentTaskSnapshot{TaskIdWrapper{
                            base_scope.task()->id()}},
                        .thread = std::nullopt})}));

  {
    auto sub_scope = sub_task->start();

    ASSERT_EQ(get_all_tasks(registry),
              (std::vector<TaskSnapshot>{
                  (TaskSnapshot{
                      .name = "entry point",
                      .state = "running",
                      .id = base_scope.task()->id(),
                      .parent = ParentTaskSnapshot{RootTask{.name = "Task"}},
                      .thread = ThreadId::current()}),
                  (TaskSnapshot{.name = "Subtask",
                                .state = "running",
                                .id = sub_task->id(),
                                .parent = ParentTaskSnapshot{TaskIdWrapper{
                                    base_scope.task()->id()}},
                                .thread = ThreadId::current()})}));
  }

  ASSERT_EQ(
      get_all_tasks(registry),
      (std::vector<TaskSnapshot>{
          (TaskSnapshot{.name = "entry point",
                        .state = "running",
                        .id = base_scope.task()->id(),
                        .parent = ParentTaskSnapshot{RootTask{.name = "Task"}},
                        .thread = ThreadId::current()}),
          (TaskSnapshot{.name = "Subtask",
                        .state = "done",
                        .id = sub_task->id(),
                        .parent = ParentTaskSnapshot{TaskIdWrapper{
                            base_scope.task()->id()}},
                        .thread = ThreadId::current()})}));
}

TEST(TaskRegistryTest, task_scope_can_update_task_state) {
  auto registry = TaskRegistry{};
  auto base_scope = registry.start_task("Task");
  auto sub_task = registry.schedule_subtask(base_scope.task(), "Subtask");

  base_scope.update_state("Some other base state");

  auto sub_scope = sub_task->start();
  sub_scope.update_state("Some other sub state");

  ASSERT_EQ(
      get_all_tasks(registry),
      (std::vector<TaskSnapshot>{
          (TaskSnapshot{.name = "entry point",
                        .state = "Some other base state",
                        .id = base_scope.task()->id(),
                        .parent = ParentTaskSnapshot{RootTask{.name = "Task"}},
                        .thread = ThreadId::current()}),
          (TaskSnapshot{.name = "Subtask",
                        .state = "Some other sub state",
                        .id = sub_task->id(),
                        .parent = ParentTaskSnapshot{TaskIdWrapper{
                            base_scope.task()->id()}},
                        .thread = ThreadId::current()})}));
}

// TODO make this a death test
// TEST(TaskRegistryDeathTest, a_non_running_subtask_cannot_update_its_state) {}

// TODO make this a death test
// TEST(TaskRegistryTest,
// state_cannot_be_updated_on_differnt_thread_than_running_thread) {}

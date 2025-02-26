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

TEST(TaskRegistryTest, creates_a_task) {
  auto registry = TaskRegistry{};

  auto base_task = registry.create_task("Task");

  std::vector<TaskSnapshot> tasks;
  registry.for_task(
      [&](TaskSnapshot task) { tasks.emplace_back(std::move(task)); });

  ASSERT_EQ(tasks, std::vector<TaskSnapshot>{
                       (TaskSnapshot{.name = "Task",
                                     .state = "created",
                                     .id = base_task->id(),
                                     .parent = nullptr,
                                     .thread = ThreadId::current()})});
}

TEST(TaskRegistryTest, subtask_knows_its_parent) {
  auto registry = TaskRegistry{};

  auto base_task = registry.create_task("Task");
  auto sub_task = registry.create_task("Subtask", base_task);

  std::vector<TaskSnapshot> tasks;
  registry.for_task(
      [&](TaskSnapshot task) { tasks.emplace_back(std::move(task)); });

  ASSERT_EQ(tasks, (std::vector<TaskSnapshot>{
                       (TaskSnapshot{.name = "Task",
                                     .state = "created",
                                     .id = base_task->id(),
                                     .parent = nullptr,
                                     .thread = ThreadId::current()}),
                       (TaskSnapshot{.name = "Subtask",
                                     .state = "created",
                                     .id = sub_task->id(),
                                     .parent = base_task->id(),
                                     .thread = ThreadId::current()})}));
}

TEST(TaskRegistryTest, subtask_can_update_its_state) {
  auto registry = TaskRegistry{};
  auto base_task = registry.create_task("Task");
  auto sub_task = registry.create_task("Subtask", base_task);

  sub_task->update_state("running");

  std::vector<TaskSnapshot> tasks;
  registry.for_task(
      [&](TaskSnapshot task) { tasks.emplace_back(std::move(task)); });

  ASSERT_EQ(tasks, (std::vector<TaskSnapshot>{
                       (TaskSnapshot{.name = "Task",
                                     .state = "created",
                                     .id = base_task->id(),
                                     .parent = nullptr,
                                     .thread = ThreadId::current()}),
                       (TaskSnapshot{.name = "Subtask",
                                     .state = "running",
                                     .id = sub_task->id(),
                                     .parent = base_task->id(),
                                     .thread = ThreadId::current()})}));
}

TEST(TaskRegistryTest, deleted_tasks_are_removed_from_registry) {
  auto registry = TaskRegistry{};

  {
    auto base_task = registry.create_task("Task");

    std::vector<TaskSnapshot> tasks;
    registry.for_task(
        [&](TaskSnapshot task) { tasks.emplace_back(std::move(task)); });

    ASSERT_EQ(tasks, (std::vector<TaskSnapshot>{
                         (TaskSnapshot{.name = "Task",
                                       .state = "created",
                                       .id = base_task->id(),
                                       .parent = nullptr,
                                       .thread = ThreadId::current()})}));
  }  // base_task is destroyed here

  std::vector<TaskSnapshot> tasks;
  registry.for_task(
      [&](TaskSnapshot task) { tasks.emplace_back(std::move(task)); });
  // make sure that entry in TaskRegistry is really deleted

  ASSERT_EQ(tasks, (std::vector<TaskSnapshot>{}));
}

TEST(TaskRegistryTest, subtasks_prevent_task_from_being_destroyed) {
  auto registry = TaskRegistry{};

  {
    std::shared_ptr<Task> sub_task;
    void* base_task_id;
    {
      auto base_task = registry.create_task("Task");
      base_task_id = base_task->id();
      sub_task = registry.create_task("Subtask", base_task);
    }

    std::vector<TaskSnapshot> tasks;
    registry.for_task(
        [&](TaskSnapshot task) { tasks.emplace_back(std::move(task)); });

    ASSERT_EQ(tasks, (std::vector<TaskSnapshot>{
                         (TaskSnapshot{.name = "Task",
                                       .state = "created",
                                       .id = base_task_id,
                                       .parent = nullptr,
                                       .thread = ThreadId::current()}),
                         (TaskSnapshot{.name = "Subtask",
                                       .state = "created",
                                       .id = sub_task->id(),
                                       .parent = base_task_id,
                                       .thread = ThreadId::current()}),
                     }));
  }  // base_task is destroyed here

  std::vector<TaskSnapshot> tasks;
  registry.for_task(
      [&](TaskSnapshot task) { tasks.emplace_back(std::move(task)); });
  // make sure that entry in TaskRegistry is really deleted

  ASSERT_EQ(tasks, (std::vector<TaskSnapshot>{}));
}

TEST(TaskRegistryTest, task_scope_sets_state_to_done) {}

TEST(TaskRegistryTest, state_update_only_works_on_creation_thread) {}

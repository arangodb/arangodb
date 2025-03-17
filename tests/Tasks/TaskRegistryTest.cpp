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

#include <thread>

using namespace arangodb::task_registry;

namespace {
auto get_all_tasks(TaskRegistry& registry) -> std::vector<TaskSnapshot> {
  std::vector<TaskSnapshot> tasks;
  registry.for_task(
      [&](TaskSnapshot task) { tasks.emplace_back(std::move(task)); });
  return tasks;
}
struct TestTask {
  std::string name;
  std::string state;
  ParentTaskSnapshot parent;
  std::optional<ThreadId> thread = ThreadId::current();
  // std::optional<TransactionId> transaction = std::nullopt;
  bool operator==(TaskSnapshot const& snapshot) const {
    return name == snapshot.name;
  }
};
}  // namespace

TEST(TaskRegistryTest,
     a_root_task_is_saved_as_a_parent_of_an_entry_point_task) {
  auto registry = TaskRegistry{};

  auto task_scope = registry.start_task("Task");

  auto all_tasks = get_all_tasks(registry);
  ASSERT_EQ(all_tasks.size(), 1);
  ASSERT_EQ(all_tasks[0], (TestTask{.name = "Task",
                                    .state = "running",
                                    .parent = ParentTaskSnapshot{RootTask{}}}));
}

TEST(TaskRegistryTest, a_subtask_knows_its_parent) {
  auto registry = TaskRegistry{};

  auto base_scope = registry.start_task("Task");
  auto sub_scope = registry.start_subtask(base_scope, "Subtask");

  auto all_tasks = get_all_tasks(registry);
  ASSERT_EQ(all_tasks.size(), 2);
  ASSERT_EQ(all_tasks[0], (TestTask{.name = "Task",
                                    .state = "running",
                                    .parent = ParentTaskSnapshot{RootTask{}}}));
  ASSERT_EQ(
      all_tasks[1],
      (TestTask{.name = "Subtask",
                .state = "running",
                .parent = ParentTaskSnapshot{TaskIdWrapper{all_tasks[0].id}}}));
}

TEST(TaskRegistryTest,
     a_task_is_removed_from_the_registry_when_its_scope_is_destroyed) {
  auto registry = TaskRegistry{};

  {
    auto task_scope = registry.start_task("Task");

    auto all_tasks = get_all_tasks(registry);
    ASSERT_EQ(all_tasks.size(), 1);
  }

  auto all_tasks = get_all_tasks(registry);
  ASSERT_EQ(all_tasks.size(), 0);
}

TEST(TaskRegistryTest, subtask_prevents_task_from_being_destroyed) {
  auto registry = TaskRegistry{};

  {
    TaskScope sub_scope;
    {
      auto base_scope =
          registry.start_task("Task");  // task is not yet destroyed
      sub_scope = registry.start_subtask(base_scope, "Subtask");
    }  // task is not yet destroyed

    auto all_tasks = get_all_tasks(registry);
    ASSERT_EQ(all_tasks.size(), 2);
    ASSERT_EQ(all_tasks[0], (TestTask{.name = "Task",
                                      .state = "done",
                                      .parent = ParentTaskSnapshot{RootTask{}},
                                      .thread = ThreadId::current()}));
    ASSERT_EQ(all_tasks[1], (TestTask{.name = "Subtask",
                                      .state = "running",
                                      .parent = ParentTaskSnapshot{
                                          TaskIdWrapper{all_tasks[0].id}}}));
  }  // base_task is destroyed here

  ASSERT_EQ(get_all_tasks(registry).size(), 0);
}

TEST(TaskRegistryTest, task_scope_can_update_task_state) {
  auto registry = TaskRegistry{};
  auto base_scope = registry.start_task("Task");
  auto sub_scope = registry.start_subtask(base_scope, "Subtask");

  base_scope.update_state("Some other base state");
  sub_scope.update_state("Some other sub state");

  auto all_tasks = get_all_tasks(registry);
  ASSERT_EQ(all_tasks.size(), 2);
  ASSERT_EQ(all_tasks[0], (TestTask{.name = "Task",
                                    .state = "Some other base state",
                                    .parent = ParentTaskSnapshot{RootTask{}}}));
  ASSERT_EQ(
      all_tasks[1],
      (TestTask{.name = "Subtask",
                .state = "Some other sub state",
                .parent = ParentTaskSnapshot{TaskIdWrapper{all_tasks[0].id}}}));
}

// TODO make this a death test
// TEST(TaskRegistryTest,
//      state_cannot_be_updated_on_differnt_thread_than_running_thread) {
//   auto registry = TaskRegistry{};
//   auto base_scope = registry.start_task("Task");

//   std::jthread([base_scope = std::move(base_scope)]() mutable {
//     base_scope.update_state("Some other base state");  // will crash
//   });
// }

TEST(TaskRegistryTest,
     a_scheduled_task_needs_to_be_started_manually_on_a_thread) {
  auto registry = TaskRegistry{};
  auto base_scope = registry.start_task("Task");
  auto sub_scope_scheduled = registry.schedule_subtask(base_scope, "Subtask");

  auto all_tasks = get_all_tasks(registry);
  ASSERT_EQ(all_tasks.size(), 2);
  ASSERT_EQ(all_tasks[0], (TestTask{.name = "Task",
                                    .state = "running",
                                    .parent = ParentTaskSnapshot{RootTask{}},
                                    .thread = ThreadId::current()}));
  ASSERT_EQ(
      all_tasks[1],
      (TestTask{.name = "Subtask",
                .state = "scheduled",
                .parent = ParentTaskSnapshot{TaskIdWrapper{all_tasks[0].id}},
                .thread = std::nullopt}));

  {
    auto sub_scope = std::move(sub_scope_scheduled).start();

    all_tasks = get_all_tasks(registry);
    ASSERT_EQ(all_tasks.size(), 2);
    ASSERT_EQ(all_tasks[0], (TestTask{.name = "Task",
                                      .state = "running",
                                      .parent = ParentTaskSnapshot{RootTask{}},
                                      .thread = ThreadId::current()}));
    ASSERT_EQ(
        all_tasks[1],
        (TestTask{.name = "Subtask",
                  .state = "running",
                  .parent = ParentTaskSnapshot{TaskIdWrapper{all_tasks[0].id}},
                  .thread = ThreadId::current()}));
  }

  all_tasks = get_all_tasks(registry);
  ASSERT_EQ(all_tasks.size(), 1);
  ASSERT_EQ(all_tasks[0], (TestTask{.name = "Task",
                                    .state = "running",
                                    .parent = ParentTaskSnapshot{RootTask{}},
                                    .thread = ThreadId::current()}));
}

TEST(TaskRegistryTest, a_scheduled_task_can_start_tasks_when_running) {
  auto registry = TaskRegistry{};
  auto base_scope = registry.start_task("Task");
  auto sub_scope_scheduled = registry.schedule_subtask(base_scope, "Subtask");

  auto all_tasks = get_all_tasks(registry);
  ASSERT_EQ(all_tasks.size(), 2);
  ASSERT_EQ(all_tasks[0], (TestTask{.name = "Task",
                                    .state = "running",
                                    .parent = ParentTaskSnapshot{RootTask{}},
                                    .thread = ThreadId::current()}));
  ASSERT_EQ(
      all_tasks[1],
      (TestTask{.name = "Subtask",
                .state = "scheduled",
                .parent = ParentTaskSnapshot{TaskIdWrapper{all_tasks[0].id}},
                .thread = std::nullopt}));

  {
    TaskScope sub_sub_scope;
    {
      auto sub_scope = std::move(sub_scope_scheduled).start();
      sub_sub_scope = registry.start_subtask(sub_scope, "Subsubtask");

      all_tasks = get_all_tasks(registry);
      ASSERT_EQ(all_tasks.size(), 3);
      ASSERT_EQ(all_tasks[0],
                (TestTask{.name = "Task",
                          .state = "running",
                          .parent = ParentTaskSnapshot{RootTask{}},
                          .thread = ThreadId::current()}));
      ASSERT_EQ(
          all_tasks[1],
          (TestTask{
              .name = "Subtask",
              .state = "running",
              .parent = ParentTaskSnapshot{TaskIdWrapper{all_tasks[0].id}},
              .thread = ThreadId::current()}));
      ASSERT_EQ(
          all_tasks[2],
          (TestTask{
              .name = "Subsubtask",
              .state = "running",
              .parent = ParentTaskSnapshot{TaskIdWrapper{all_tasks[1].id}},
              .thread = ThreadId::current()}));
    }

    all_tasks = get_all_tasks(registry);
    ASSERT_EQ(all_tasks.size(), 3);
    ASSERT_EQ(all_tasks[0], (TestTask{.name = "Task",
                                      .state = "running",
                                      .parent = ParentTaskSnapshot{RootTask{}},
                                      .thread = ThreadId::current()}));
    ASSERT_EQ(
        all_tasks[1],
        (TestTask{.name = "Subtask",
                  .state = "done",
                  .parent = ParentTaskSnapshot{TaskIdWrapper{all_tasks[0].id}},
                  .thread = ThreadId::current()}));
    ASSERT_EQ(
        all_tasks[2],
        (TestTask{.name = "Subsubtask",
                  .state = "running",
                  .parent = ParentTaskSnapshot{TaskIdWrapper{all_tasks[1].id}},
                  .thread = ThreadId::current()}));
  }
  all_tasks = get_all_tasks(registry);
  ASSERT_EQ(all_tasks.size(), 1);
  ASSERT_EQ(all_tasks[0], (TestTask{.name = "Task",
                                    .state = "running",
                                    .parent = ParentTaskSnapshot{RootTask{}},
                                    .thread = ThreadId::current()}));
}

TEST(TaskRegistryTest, a_scheduled_task_can_update_its_state_when_running) {
  auto registry = TaskRegistry{};
  auto base_scope = registry.start_task("Task");
  auto sub_scope_scheduled = registry.schedule_subtask(base_scope, "Subtask");

  auto sub_scope = std::move(sub_scope_scheduled).start();
  sub_scope.update_state("Some other sub state");

  auto all_tasks = get_all_tasks(registry);
  ASSERT_EQ(all_tasks.size(), 2);
  ASSERT_EQ(all_tasks[0], (TestTask{.name = "Task",
                                    .state = "running",
                                    .parent = ParentTaskSnapshot{RootTask{}}}));
  ASSERT_EQ(
      all_tasks[1],
      (TestTask{.name = "Subtask",
                .state = "Some other sub state",
                .parent = ParentTaskSnapshot{TaskIdWrapper{all_tasks[0].id}}}));
}

TEST(TaskRegistryTest,
     can_start_a_task_that_refers_to_a_transaction_via_tid_as_its_parent) {
  auto registry = TaskRegistry{};

  auto base_scope = registry.start_transaction_task(TransactionId{12}, "Task");

  auto all_tasks = get_all_tasks(registry);
  ASSERT_EQ(all_tasks.size(), 1);
  ASSERT_EQ(all_tasks[0].parent, ParentTaskSnapshot{TransactionId{12}});
}

TEST(TaskRegistryTest, a_task_can_directly_belong_to_a_transaction) {
  auto registry = TaskRegistry{};
  auto base_scope = registry.start_task("Task");

  auto sub_scope =
      registry.start_subtask(base_scope, "Subtask", TransactionId{4});

  auto all_tasks = get_all_tasks(registry);
  ASSERT_EQ(all_tasks.size(), 2);
  ASSERT_EQ(all_tasks[1].name, "Subtask");
  ASSERT_EQ(all_tasks[1].transaction, TransactionId{4});
}

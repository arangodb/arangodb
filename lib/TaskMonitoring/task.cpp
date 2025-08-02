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
#include "TaskMonitoring/task.h"

#include "Assertions/ProdAssert.h"
#include "Containers/Concurrent/source_location.h"
#include "Containers/Concurrent/thread.h"
#include "Inspection/Format.h"
#include "TaskMonitoring/task_registry_variable.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include <atomic>
#include <optional>
#include <source_location>
#include <variant>
#include <optional>

// helper type for the visitor
namespace {
template<class... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;
}  // namespace

using namespace arangodb;
using namespace arangodb::task_monitoring;

auto arangodb::task_monitoring::PrintTo(TaskSnapshot const& task,
                                        std::ostream* os) -> void {
  // *os << task.id << "| " << task.name << " - " <<
  // inspection::json(task.parent);
  *os << inspection::json(task);
}

auto TaskInRegistry::snapshot() -> TaskSnapshot {
  std::string message;
  if (printer == nullptr) {
    message = "";
  } else {
    message = printer->to_string();
  }
  return TaskSnapshot{
      .name = name,
      .state = state,
      .message = message,
      .id = id(),
      .parent = std::visit(
          overloaded{
              [&](RootTask const& root) { return ParentTaskSnapshot{root}; },
              [&](NodeReference const& parent) {
                return ParentTaskSnapshot{TaskId{parent->data.id()}};
              }},
          parent),
      .thread = running_thread,
      .source_location = basics::SourceLocationSnapshot{
          .file_name = source_location.file_name(),
          .function_name = source_location.function_name(),
          .line = source_location.line()}};
}

namespace {
/**
   Marks itself for deletion and deletes a parent reference.

   Deleting the parent reference makes sure that
   a parent can directly be marked for deletion when all its children are
   marked. Otherwise, we need to wait for the garbage collection to delete the
   references, possibly requiring several garbage collection cycles to delete
   all hierarchy levels.
 */
auto mark_finished_nodes_for_deletion(Node* node) {
  auto specific_node =
      reinterpret_cast<containers::ThreadOwnedList<TaskInRegistry>::Node*>(
          node);

  // get rid of parent task and delete a shared reference
  specific_node->data.parent = {RootTask{}};

  // mark node for deletion needs to be last action on specific_node, because
  // then a garbage collection run can destroy the node at any time
  specific_node->list->mark_for_deletion(specific_node);
}
}  // namespace

Task::Task(std::string name, std::shared_ptr<TaskMessage> printer,
           bool isScheduled, std::source_location loc)
    : _node_in_registry{NodeReference(
          reinterpret_cast<Node*>(get_thread_registry().add([&]() {
            ParentTask parent;
            if (auto current = *get_current_task(); current != nullptr) {
              parent = {current->_node_in_registry};
            } else {
              parent = {RootTask{}};
            }
            if (isScheduled) {
              return TaskInRegistry::scheduled(
                  std::move(name), std::move(parent), printer, std::move(loc));
            } else {
              return TaskInRegistry::create(std::move(name), std::move(parent),
                                            printer, std::move(loc));
            }
          })),
          mark_finished_nodes_for_deletion)},
      parent{*get_current_task()} {
  *get_current_task() = this;
}

Task::~Task() {
  _node_in_registry->data.state.store(State::Finished,
                                      std::memory_order_relaxed);
  *get_current_task() = parent;
}

auto Task::id() -> TaskId { return _node_in_registry->data.id(); }

auto Task::source_location() -> basics::SourceLocationSnapshot {
  return basics::SourceLocationSnapshot::from(
      _node_in_registry->data.source_location);
}

auto Task::start() -> void {
  _node_in_registry->data.state.store(State::Running);
  _node_in_registry->data.running_thread.store(basics::ThreadId::current());
}

/**
   Function to get and set global thread local variable of the currently running
   task on the current thread

   Points to nullptr if there is no task running on the current thread.
 */
auto arangodb::task_monitoring::get_current_task() -> Task** {
  struct Guard {
    Task* task = nullptr;
  };
  // make sure that this is only created once on a thread
  static thread_local auto current = Guard{};
  return &current.task;
}

ThreadTask::ThreadTask(std::string name, std::function<void()> lambda,
                       std::shared_ptr<TaskMessage> printer,
                       std::source_location loc) {
  auto current_task_ptr = *get_current_task();
  std::jthread([current_task = current_task_ptr,
                // extend lifetime of task
                current_task_ref = current_task_ptr->_node_in_registry,
                name = std::move(name), lambda = std::move(lambda), printer,
                loc = std::move(loc)]() {
    *get_current_task() = current_task;
    auto task = Task{std::move(name), printer, false, std::move(loc)};
    lambda();
  });
}

ScheduledTask::ScheduledTask(std::string name, RequestLane lane,
                             std::function<void()> lambda,
                             std::shared_ptr<TaskMessage> printer,
                             std::source_location loc) {
  SchedulerFeature::SCHEDULER->queue(
      lane, [task = Task{std::move(name), printer, true, std::move(loc)},
             lambda]() mutable {
        task.start();
        lambda();
      });
}

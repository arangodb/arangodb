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

auto operator<<(std::ostream& out,
                arangodb::task_monitoring::TaskSnapshot const& task)
    -> std::ostream& {
  return out << inspection::json(task);
}

auto TaskInRegistry::snapshot() -> TaskSnapshot {
  return TaskSnapshot{
      .name = name,
      .state = state,
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

Task::Task(std::string name, std::source_location loc)
    : _node_in_registry{NodeReference(
          reinterpret_cast<Node*>(get_thread_registry().add([&]() {
            if (auto current = *get_current_task(); current != nullptr) {
              return TaskInRegistry::child(
                  std::move(name), current->_node_in_registry, std::move(loc));
            }
            return TaskInRegistry::root(std::move(name), std::move(loc));
          })),
          mark_finished_nodes_for_deletion)} {
  // remember its parent task to switch the current task back to the parent task
  // when this task is destroyed
  parent = *get_current_task();
  *get_current_task() = this;
}

Task::~Task() {
  _node_in_registry->data.state.store(State::Finished,
                                      std::memory_order_relaxed);
  *get_current_task() = parent;
}

auto Task::id() -> TaskId { return _node_in_registry->data.id(); }

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

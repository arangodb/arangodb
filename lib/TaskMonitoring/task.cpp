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

void arangodb::task_monitoring::PrintTo(const TaskSnapshot& task,
                                        std::ostream* os) {
  *os << inspection::json(task);
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
   Marks itself for deletion and all finishes parent nodes that do not have any
   other children.

   Loops over the hierarchy of parents of the given node. This function makes
   sure that these hierarchies are all marked for deletion together and can
   therefore be deleted in the next garbage collection run. Otherwise, a parent
   exists as long as its last child is not garbage collected because the child
   owns a shared reference to the parent, requiring as many garbage collection
   cycles as hierarchy levels to cleanup the given hierarchy.

   Some examples:

   If there are dependencies like (A └ B means A is parent of B)
     node_A
     └ node_B
       └ node (input to this function)
   and all of these nodes do not have any other references and are therefore
   finished, then this function makes sure that all of these node are marked for
   deletion.

   If we have dependencies like
     node_A
     ├ node_B
     └ node_C
       └ node (input to this function)
   with node_C and node beeing finished, this function will mark node_C and node
   for deletion but neither node_B nor node_A, independent of their state.
 */
auto mark_finished_nodes_for_deletion(Node* node) {
  auto current_node = node;
  while (true) {
    auto specific_node =
        reinterpret_cast<containers::ThreadOwnedList<TaskInRegistry>::Node*>(
            current_node);

    // make sure that we don't mark a node twice for deletion
    auto expected = false;
    if (not specific_node->data.isDeleted.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel)) {
      break;
    }

    // do not continue if node does not have a parent
    auto& parent = specific_node->data.parent;
    if (not std::holds_alternative<NodeReference>(parent)) {
      specific_node->list->mark_for_deletion(specific_node);
      break;
    }
    // do not continue if node is not the only child of parent
    auto& parent_ref = std::get<NodeReference>(parent);
    if (parent_ref.ref_count() != 1) {
      specific_node->list->mark_for_deletion(specific_node);
      break;
    }

    // continue with parent node
    current_node = parent_ref.get();

    // mark node for deletion needs to be last action on specific_node, because
    // then a garbage collection run can destroy the node at any time
    specific_node->list->mark_for_deletion(specific_node);
  }
}
}  // namespace

Task::Task(std::string name, std::source_location loc)
    : _node_in_registry{NodeReference::create(
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

auto Task::id() -> void* { return _node_in_registry->data.id(); }

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

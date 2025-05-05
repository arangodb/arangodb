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

// helper type for the visitor
namespace {
template<class... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;
}  // namespace

namespace arangodb::task_monitoring {

void PrintTo(const TaskSnapshot& task, std::ostream* os) {
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
                return ParentTaskSnapshot{TaskIdWrapper{parent->data.id()}};
              }},
          parent),
      .thread = running_thread,
      .source_location = basics::SourceLocationSnapshot{
          .file_name = source_location.file_name(),
          .function_name = source_location.function_name(),
          .line = source_location.line()}};
}

namespace {
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

    auto& parent = specific_node->data.parent;
    if (not std::holds_alternative<NodeReference>(parent)) {
      specific_node->list->mark_for_deletion(specific_node);
      break;
    }
    auto& parent_ref = std::get<NodeReference>(parent);
    if (parent_ref.ref_count() != 1) {
      specific_node->list->mark_for_deletion(specific_node);
      break;
    }
    // node is last reference to parent, therefore it can be marked for deletion
    current_node = parent_ref.get();

    specific_node->list->mark_for_deletion(specific_node);
  }
}
}  // namespace

Task::Task(std::string name, std::source_location loc)
    : _node_in_registry{NodeReference::create(
          reinterpret_cast<Node*>(get_thread_registry().add([&]() {
            return TaskInRegistry::root(std::move(name), std::move(loc));
          })),
          mark_finished_nodes_for_deletion)} {}

Task::Task(std::string name, Task& parent, std::source_location loc)
    : _node_in_registry{NodeReference::create(
          reinterpret_cast<Node*>(get_thread_registry().add([&]() {
            return TaskInRegistry::child(
                std::move(name), parent._node_in_registry, std::move(loc));
          })),
          mark_finished_nodes_for_deletion)} {}
Task::~Task() {
  _node_in_registry->data.state.store(State::Finished,
                                      std::memory_order_relaxed);
}

auto Task::id() -> void* { return _node_in_registry->data.id(); }

}  // namespace arangodb::task_monitoring

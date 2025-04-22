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
#include "Tasks/task.h"

#include "Assertions/ProdAssert.h"
#include "Containers/Concurrent/source_location.h"
#include "Containers/Concurrent/thread.h"
#include "Inspection/Format.h"
#include "Tasks/task_registry_variable.h"
#include <optional>
#include <source_location>

// helper type for the visitor
namespace {
template<class... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;
}  // namespace

namespace arangodb::task_registry {

void PrintTo(const TaskSnapshot& task, std::ostream* os) {
  *os << inspection::json(task);
}

auto TaskInRegistry::snapshot() -> TaskSnapshot {
  return TaskSnapshot{
      .name = name,
      .state = state,
      .id = id(),
      .parent = std::visit(
          overloaded{[&](RootTask root) { return ParentTaskSnapshot{root}; },
                     [&](std::shared_ptr<TaskInRegistry> parent) {
                       return ParentTaskSnapshot{TaskIdWrapper{parent.get()}};
                     },
                     [&](TransactionId transaction) {
                       return ParentTaskSnapshot{transaction};
                     }},
          parent),
      .transaction = transaction,
      .thread = running_thread,
      .source_location = basics::SourceLocationSnapshot{
          .file_name = source_location.file_name(),
          .function_name = source_location.function_name(),
          .line = source_location.line()}};
}

Task::Task(TaskInRegistry task_in_registry)
    : _node_in_registry{get_thread_registry().add(
          [&]() { return std::move(task_in_registry); })} {}

Task::~Task() {
  if (_node_in_registry != nullptr) {
    _node_in_registry->list->mark_for_deletion(_node_in_registry.get());
  }
}

auto Task::id() -> void* {
  if (_node_in_registry != nullptr) {
    return _node_in_registry->data.id();
  } else {
    return nullptr;
  }
}

auto Task::update_state(std::string_view state, std::source_location loc)
    -> void {
  if (_node_in_registry) {
    auto& task_data = _node_in_registry->data;
    auto current_thread = basics::ThreadId::current();
    ADB_PROD_ASSERT(current_thread == task_data.running_thread) << fmt::format(
        "TaskRegistry::update_state was called from thread {} but needs to be "
        "called from its owning thread {}. Called at {}. Task: {} ({}), {}",
        fmt::format("{}", inspection::json(current_thread)),
        fmt::format("{}", inspection::json(task_data.running_thread)),
        inspection::json(basics::SourceLocationSnapshot::from(loc)),
        task_data.name, state,
        inspection::json(
            basics::SourceLocationSnapshot::from(task_data.source_location)));
    task_data.state = state;
  }
}

BaseTask::BaseTask(std::string name, std::source_location loc)
    : Task{TaskInRegistry{.name = std::move(name),
                          .state = "created",
                          .parent = {ParentTask{RootTask{}}},
                          .running_thread = basics::ThreadId::current(),
                          .source_location = std::move(loc)}} {}

}  // namespace arangodb::task_registry

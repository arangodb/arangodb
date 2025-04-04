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
#include "Inspection/Format.h"
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

auto TaskInRegistry::create(std::string name, std::source_location loc)
    -> std::shared_ptr<TaskInRegistry> {
  struct MakeSharedTask : TaskInRegistry {
    MakeSharedTask(ParentTask parent, std::string name, std::string state,
                   std::source_location loc)
        : TaskInRegistry(parent, std::move(name), std::move(state),
                         std::nullopt, std::move(loc)) {}
  };
  return std::make_shared<MakeSharedTask>(
      ParentTask{RootTask{}}, std::move(name), "created", std::move(loc));
}
auto TaskInRegistry::subtask(TaskScope& parent, std::string name,
                             std::optional<TransactionId> transaction,
                             std::source_location loc)
    -> std::shared_ptr<TaskInRegistry> {
  struct MakeSharedTask : TaskInRegistry {
    MakeSharedTask(ParentTask parent, std::string name, std::string state,
                   std::optional<TransactionId> transaction,
                   std::source_location loc)
        : TaskInRegistry(parent, std::move(name), std::move(state),
                         std::move(transaction), std::move(loc)) {}
  };
  return std::make_shared<MakeSharedTask>(
      ParentTask{parent.task()}, std::move(name), "created",
      std::move(transaction), std::move(loc));
}
auto TaskInRegistry::scheduled(TaskScope& parent, std::string name,
                               std::source_location loc)
    -> std::shared_ptr<TaskInRegistry> {
  struct MakeSharedTask : TaskInRegistry {
    MakeSharedTask(ParentTask parent, std::string name, std::string state,
                   std::source_location loc)
        : TaskInRegistry(parent, std::move(name), std::move(state),
                         std::nullopt, std::move(loc)) {}
  };
  return std::make_shared<MakeSharedTask>(
      ParentTask{parent.task()}, std::move(name), "scheduled", std::move(loc));
}

auto TaskInRegistry::transaction_task(TransactionId transaction,
                                      std::string name,
                                      std::source_location loc)
    -> std::shared_ptr<TaskInRegistry> {
  struct MakeSharedTask : TaskInRegistry {
    MakeSharedTask(ParentTask parent, std::string name, std::string state,
                   std::source_location loc)
        : TaskInRegistry(parent, std::move(name), std::move(state),
                         std::nullopt, std::move(loc)) {}
  };
  return std::make_shared<MakeSharedTask>(ParentTask{std::move(transaction)},
                                          std::move(name), "created",
                                          std::move(loc));
}

TaskInRegistry::TaskInRegistry(ParentTask parent, std::string name,
                               std::string state,
                               std::optional<TransactionId> transaction,
                               std::source_location loc)
    : _name{std::move(name)},
      _state{std::move(state)},
      _parent{std::move(parent)},
      _transaction{std::move(transaction)},
      _source_location{std::move(loc)} {}

// TODO
// Task::~Task() {
//   if (_registry) {
//     _registry->garbage_collect();
//   }
// }

auto TaskInRegistry::update_state(std::string_view state,
                                  std::source_location loc) -> void {
  auto current_thread = basics::ThreadId::current();
  ADB_PROD_ASSERT(current_thread == _running_thread) << fmt::format(
      "TaskRegistry::update_state was called from thread {} but needs to be "
      "called from its owning thread {}. Called at {}. Task: {} ({}), {}",
      fmt::format("{}", inspection::json(current_thread)),
      fmt::format("{}", inspection::json(_running_thread)),
      inspection::json(basics::SourceLocationSnapshot::from(loc)), _name,
      _state,
      inspection::json(basics::SourceLocationSnapshot::from(_source_location)));
  _state = state;
}

auto TaskInRegistry::snapshot() -> TaskSnapshot {
  return TaskSnapshot{
      .name = _name,
      .state = _state,
      .id = id(),
      .parent = std::visit(
          overloaded{[&](RootTask root) { return ParentTaskSnapshot{root}; },
                     [&](std::shared_ptr<TaskInRegistry> parent) {
                       return ParentTaskSnapshot{TaskIdWrapper{parent.get()}};
                     },
                     [&](TransactionId transaction) {
                       return ParentTaskSnapshot{transaction};
                     }},
          _parent),
      .transaction = _transaction,
      .thread = _running_thread,
      .source_location = basics::SourceLocationSnapshot{
          .file_name = _source_location.file_name(),
          .function_name = _source_location.function_name(),
          .line = _source_location.line()}};
}

}  // namespace arangodb::task_registry

std::ostream& operator<<(std::ostream& os,
                         arangodb::task_registry::TaskSnapshot const& t) {
  os << fmt::format("{}", arangodb::inspection::json(t));
  return os;
}

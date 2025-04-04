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
#pragma once

#include "Containers/Concurrent/ThreadOwnedList.h"
#include "Containers/Concurrent/source_location.h"
#include "Containers/Concurrent/thread.h"
#include "Inspection/Types.h"
#include "fmt/format.h"

#include <cstdint>
#include <optional>
#include <string>

namespace arangodb::task_registry {

struct RootTask {
  bool operator==(RootTask const&) const = default;
};
template<typename Inspector>
auto inspect(Inspector& f, RootTask& x) {
  return f.object(x).fields();
}

struct TransactionId {
  std::uint64_t id;
  bool operator==(TransactionId const&) const = default;
};
template<typename Inspector>
auto inspect(Inspector& f, TransactionId& x) {
  return f.object(x).fields(f.field("tid", x.id));
}

struct TransactionTask {
  std::string name;
  TransactionId tid;
  bool operator==(TransactionTask const&) const = default;
};
template<typename Inspector>
auto inspect(Inspector& f, TransactionTask& x) {
  return f.object(x).fields(f.field("name", x.name), f.embedFields(x.tid));
}

struct TaskIdWrapper {
  void* id;
  bool operator==(TaskIdWrapper const&) const = default;
};
template<typename Inspector>
auto inspect(Inspector& f, TaskIdWrapper& x) {
  return f.object(x).fields(f.field("id", fmt::format("{}", x.id)));
}

struct ParentTaskSnapshot
    : std::variant<RootTask, TaskIdWrapper, TransactionId> {};
template<typename Inspector>
auto inspect(Inspector& f, ParentTaskSnapshot& x) {
  return f.variant(x).unqualified().alternatives(
      inspection::inlineType<RootTask>(),
      inspection::inlineType<TaskIdWrapper>(),
      inspection::inlineType<TransactionId>());
}

struct TaskSnapshot {
  std::string name;
  std::string state;
  void* id;
  ParentTaskSnapshot parent;
  std::optional<TransactionId> transaction;
  std::optional<basics::ThreadId> thread;
  basics::SourceLocationSnapshot source_location;
  bool operator==(TaskSnapshot const&) const = default;
};
template<typename Inspector>
auto inspect(Inspector& f, TaskSnapshot& x) {
  return f.object(x).fields(
      f.field("id", fmt::format("{}", x.id)), f.field("name", x.name),
      f.field("state", x.state), f.field("parent", x.parent),
      f.field("transaction", x.transaction), f.field("thread", x.thread),
      f.field("source_location", x.source_location));
}

struct TaskInRegistry;
struct ParentTask
    : std::variant<RootTask, std::shared_ptr<TaskInRegistry>, TransactionId> {};

struct TaskScope;
struct ScheduledTaskScope;

struct TaskInRegistry {
  using Snapshot = TaskSnapshot;
  static auto create(std::string name, std::source_location loc)
      -> std::shared_ptr<TaskInRegistry>;
  static auto subtask(TaskScope& parent, std::string name,
                      std::optional<TransactionId> transaction,
                      std::source_location loc)
      -> std::shared_ptr<TaskInRegistry>;
  static auto scheduled(TaskScope& parent, std::string name,
                        std::source_location)
      -> std::shared_ptr<TaskInRegistry>;
  static auto transaction_task(TransactionId transaction, std::string name,
                               std::source_location loc)
      -> std::shared_ptr<TaskInRegistry>;
  ~TaskInRegistry();
  auto id() -> void* { return this; }
  auto snapshot() -> TaskSnapshot;
  auto set_to_deleted() -> void {}

  friend TaskScope;
  friend ScheduledTaskScope;

 private:
  TaskInRegistry(ParentTask parent, std::string name, std::string state,
                 std::optional<TransactionId> transaction,
                 std::source_location loc);
  /**
     Update the state

     Can only be called on its own running thread, throws otherwise.
   */
  auto update_state(std::string_view state,
                    std::source_location loc = std::source_location::current())
      -> void;  // should only be called from scope

  std::string const _name;
  std::string _state;  // has to probably be atomic (for reading and writing
                       // concurrently on different threads), but is string...
  ParentTask _parent;
  std::optional<TransactionId> _transaction;  // stays constant
  std::optional<basics::ThreadId>
      _running_thread;  // proably has to also be atomic because
                        // changes for scheduled task
  std::source_location const _source_location;
  // possibly interesting other properties:
  // std::chrono::time_point<std::chrono::steady_clock> creation = std:;
};

/**
   A task in scope in a running task.

   The TaskScope sets the state of the corresponding task.
 */
struct TaskScope {
  // TODO possibly update source location of task in this constructor,
  TaskScope(std::shared_ptr<TaskInRegistry> task) : _task{task} {
    if (task) {
      _task->_running_thread = basics::ThreadId::current();
      _task->update_state("running");
    }
  }
  TaskScope() : _task{nullptr} {}
  TaskScope(TaskScope const&) = delete;
  TaskScope(TaskScope&&) = default;
  TaskScope& operator=(TaskScope&& other) = default;
  ~TaskScope() {
    if (_task) {
      _task->update_state("done");
    }
  }
  auto update_state(std::string_view state,
                    std::source_location loc = std::source_location::current())
      -> void {
    if (_task) {
      _task->update_state(std::move(state), std::move(loc));
    }
  }

  friend TaskInRegistry;

 private:
  auto task() -> std::shared_ptr<TaskInRegistry> { return _task; }
  std::shared_ptr<TaskInRegistry> _task;
};

struct ScheduledTaskScope {
  ScheduledTaskScope(std::shared_ptr<TaskInRegistry> task) : _task{task} {}
  ScheduledTaskScope(ScheduledTaskScope&&) = default;
  ScheduledTaskScope(ScheduledTaskScope const&) = delete;
  auto start() && -> TaskScope { return TaskScope{std::move(_task)}; }

 private:
  std::shared_ptr<TaskInRegistry> _task;
};

}  // namespace arangodb::task_registry

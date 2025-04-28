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
void PrintTo(const TaskSnapshot& task, std::ostream* os);

struct Node;
struct ParentNode {
  std::shared_ptr<Node> node;
};
struct ParentTask : std::variant<RootTask, ParentNode, TransactionId> {};

struct TaskScope;
struct ScheduledTaskScope;

/**
   The task object inside the registry
 */
struct TaskInRegistry {
  using Snapshot = TaskSnapshot;
  auto id() -> void* { return this; }
  auto snapshot() -> TaskSnapshot;
  auto set_to_deleted() -> void {}

  /**
     Update the state

     Can only be called on its own running thread, throws otherwise.
   */
  auto update_state(std::string_view state,
                    std::source_location loc = std::source_location::current())
      -> void;  // should only be called from scope

  std::string const name;
  std::string state;  // has to probably be atomic (for reading and writing
                      // concurrently on different threads), but is string...
  ParentTask parent;
  std::optional<TransactionId> transaction;  // stays constant
  std::optional<basics::ThreadId>
      running_thread;  // proably has to also be atomic because
                       // changes for scheduled task
  std::source_location const source_location;
  // possibly interesting other properties:
  // std::chrono::time_point<std::chrono::steady_clock> creation = std:;
};

struct Node : public containers::ThreadOwnedList<TaskInRegistry>::Node {
  using containers::ThreadOwnedList<TaskInRegistry>::Node::Node;
};

struct ChildTask;

/**
   This task adds an entry to the task registry on construction and mark the
   entry for deletion on destruction.
 */
struct Task {
  friend ChildTask;
  Task(Task&& other) = default;
  Task& operator=(Task&& other) = default;
  Task(Task const&) = delete;
  Task& operator=(Task const&) = delete;

  Task(TaskInRegistry task_in_registry);

  auto id() -> void*;
  auto update_state(std::string_view state,
                    std::source_location loc = std::source_location::current())
      -> void;

 private:
  std::shared_ptr<Node> _node_in_registry = nullptr;
};

/** Helper type to create a basic task */
// TODO automatically detect current task create
// - a base task if there is not current task on current thread
// - a child task if there exists a current task
struct BaseTask : public Task {
  BaseTask(std::string name,
           std::source_location = std::source_location::current());
};
struct ChildTask : public Task {
  ChildTask(std::string name, Task& parent,
            std::source_location = std::source_location::current());
};

}  // namespace arangodb::task_registry

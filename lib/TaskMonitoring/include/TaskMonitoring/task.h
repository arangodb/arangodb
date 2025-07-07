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

#include <atomic>
#include <cstdint>
#include <optional>
#include <source_location>
#include <string>

namespace arangodb::task_monitoring {

struct RootTask {
  bool operator==(RootTask const&) const = default;
};
template<typename Inspector>
auto inspect(Inspector& f, RootTask& x) {
  return f.object(x).fields();
}

struct TaskId {
  void* id;
  bool operator==(TaskId const&) const = default;
};
template<typename Inspector>
auto inspect(Inspector& f, TaskId& x) {
  return f.object(x).fields(f.field("id", fmt::format("{}", x.id)));
}

struct ParentTaskSnapshot : std::variant<RootTask, TaskId> {};
template<typename Inspector>
auto inspect(Inspector& f, ParentTaskSnapshot& x) {
  return f.variant(x).unqualified().alternatives(
      inspection::inlineType<RootTask>(), inspection::inlineType<TaskId>());
}

enum class State { Created = 0, Running, Finished, Deleted };
template<typename Inspector>
auto inspect(Inspector& f, State& x) {
  return f.enumeration(x).values(State::Created, "Created", State::Running,
                                 "Running", State::Finished, "Finished",
                                 State::Deleted, "Deleted");
}

struct TaskSnapshot {
  std::string name;
  State state;
  TaskId id;
  ParentTaskSnapshot parent;
  std::optional<basics::ThreadId> thread;
  basics::SourceLocationSnapshot source_location;
  bool operator==(TaskSnapshot const&) const = default;
  auto update_state(State new_state) -> TaskSnapshot& {
    state = new_state;
    return *this;
  }
};
template<typename Inspector>
auto inspect(Inspector& f, TaskSnapshot& x) {
  return f.object(x).fields(
      f.embedFields(x.id), f.field("name", x.name), f.field("state", x.state),
      f.field("parent", x.parent), f.field("thread", x.thread),
      f.field("source_location", x.source_location));
}

struct Node;
using NodeReference = std::shared_ptr<Node>;
struct ParentTask : std::variant<RootTask, NodeReference> {};

/**
   The task object inside the registry
 */
struct TaskInRegistry {
  using Snapshot = TaskSnapshot;
  auto id() -> TaskId { return TaskId{this}; }
  auto snapshot() -> TaskSnapshot;
  auto set_to_deleted() -> void {
    state.store(State::Deleted, std::memory_order_release);
  }
  static auto root(std::string name, std::source_location loc)
      -> TaskInRegistry {
    return TaskInRegistry{.name = std::move(name),
                          .state = State::Running,
                          .parent = ParentTask{RootTask{}},
                          .running_thread = basics::ThreadId::current(),
                          .source_location = std::move(loc)};
  }
  static auto child(std::string name, NodeReference parent,
                    std::source_location loc) -> TaskInRegistry {
    return TaskInRegistry{.name = std::move(name),
                          .state = State::Running,
                          .parent = ParentTask{std::move(parent)},
                          .running_thread = basics::ThreadId::current(),
                          .source_location = std::move(loc)};
  }

  std::string const name;
  std::atomic<State> state;
  ParentTask parent;
  std::optional<basics::ThreadId>
      running_thread;  // proably has to also be atomic because
                       // changes for scheduled task
  std::source_location const source_location;
  // possibly interesting other properties:
  // std::chrono::time_point<std::chrono::steady_clock> creation = std:;
};

/**
   A node in the task registry

   Use inheritance to circumvent problems with non-satified constraints for Node
   when used in ParentTask in TaskInRegistry
 */
struct Node : public containers::ThreadOwnedList<TaskInRegistry>::Node {};

/**
   This is a scope for an active task.

   It adds an entry to the task registry on construction and sets its
   state to finished on destruction.

   A task registry entry is marked for deletion (and will then be garbage
   collected at some point) when all its shared references are gone. A shared
   reference to a task registry entry is owned by a task and the children of
   their parent tasks. Therefore a task in the registry lives at least as long
   as its task scope or its longest living child.
*/
struct Task {
  Task(Task&& other) = delete;
  Task& operator=(Task&& other) = delete;
  Task(Task const&) = delete;
  Task& operator=(Task const&) = delete;

  Task(std::string name,
       std::source_location loc = std::source_location::current());
  ~Task();

  auto id() -> TaskId;

 private:
  Task* parent;
  NodeReference _node_in_registry;
};

auto get_current_task() -> Task**;

}  // namespace arangodb::task_monitoring

auto operator<<(std::ostream& out,
                arangodb::task_monitoring::TaskSnapshot const& task)
    -> std::ostream&;

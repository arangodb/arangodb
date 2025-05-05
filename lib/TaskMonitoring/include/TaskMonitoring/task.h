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
#include "shared_reference.h"

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

struct TaskIdWrapper {
  void* id;
  bool operator==(TaskIdWrapper const&) const = default;
};
template<typename Inspector>
auto inspect(Inspector& f, TaskIdWrapper& x) {
  return f.object(x).fields(f.field("id", fmt::format("{}", x.id)));
}

struct ParentTaskSnapshot : std::variant<RootTask, TaskIdWrapper> {};
template<typename Inspector>
auto inspect(Inspector& f, ParentTaskSnapshot& x) {
  return f.variant(x).unqualified().alternatives(
      inspection::inlineType<RootTask>(),
      inspection::inlineType<TaskIdWrapper>());
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
  void* id;
  ParentTaskSnapshot parent;
  std::optional<basics::ThreadId> thread;
  basics::SourceLocationSnapshot source_location;
  bool operator==(TaskSnapshot const&) const = default;
};
template<typename Inspector>
auto inspect(Inspector& f, TaskSnapshot& x) {
  return f.object(x).fields(f.field("id", fmt::format("{}", x.id)),
                            f.field("name", x.name), f.field("state", x.state),
                            f.field("parent", x.parent),
                            f.field("thread", x.thread),
                            f.field("source_location", x.source_location));
}
void PrintTo(const TaskSnapshot& task, std::ostream* os);

struct Node;
using NodeReference = SharedReference<Node>;
struct ParentTask : std::variant<RootTask, NodeReference> {};

struct Task;

/**
   The task object inside the registry
 */
struct TaskInRegistry {
  using Snapshot = TaskSnapshot;
  auto id() -> void* { return this; }
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
                          .parent = ParentTask{parent},
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
   Use inheritance to circumvent problems with non-satified constraints for Node
 */
struct Node : public containers::ThreadOwnedList<TaskInRegistry>::Node {};

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

  Task(std::string name,
       std::source_location loc = std::source_location::current());
  Task(std::string name, Task& parent,
       std::source_location loc = std::source_location::current());

  auto id() -> void*;

 private:
  NodeReference _node_in_registry;
};

}  // namespace arangodb::task_monitoring

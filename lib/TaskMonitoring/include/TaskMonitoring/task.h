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
#include "GeneralServer/RequestLane.h"
#include "Inspection/Types.h"
#include "fmt/format.h"

#include <atomic>
#include <cstdint>
#include <optional>
#include <source_location>
#include <string>
#include <thread>

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

enum class State { Created = 0, Scheduled, Running, Finished, Deleted };
template<typename Inspector>
auto inspect(Inspector& f, State& x) {
  return f.enumeration(x).values(
      State::Created, "Created", State::Scheduled, "Scheduled", State::Running,
      "Running", State::Finished, "Finished", State::Deleted, "Deleted");
}

struct TaskSnapshot {
  std::string name;
  State state;
  std::string message;
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
      f.field("message", x.message), f.field("parent", x.parent),
      f.field("thread", x.thread),
      f.field("source_location", x.source_location));
}
auto PrintTo(TaskSnapshot const& task, std::ostream* os) -> void;

struct Node;
using NodeReference = std::shared_ptr<Node>;
struct ParentTask : std::variant<RootTask, NodeReference> {};

/**
   A user-defined message type that can be used to provide additional
   information for a task.

   The to_string method is called when a snapshot of a task is created.
 */
struct TaskMessage {
  virtual ~TaskMessage() = default;
  virtual auto to_string() -> std::string { return ""; };
};

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
  static auto create(std::string name, ParentTask parent,
                     std::shared_ptr<TaskMessage> printer,
                     std::source_location loc) -> TaskInRegistry {
    return TaskInRegistry{.name = std::move(name),
                          .state = State::Running,
                          .parent = std::move(parent),
                          .running_thread = {basics::ThreadId::current()},
                          .source_location = std::move(loc),
                          .printer = printer};
  }
  static auto scheduled(std::string name, ParentTask parent,
                        std::shared_ptr<TaskMessage> printer,
                        std::source_location loc) -> TaskInRegistry {
    return TaskInRegistry{.name = std::move(name),
                          .state = State::Scheduled,
                          .parent = std::move(parent),
                          .running_thread = {std::nullopt},
                          .source_location = std::move(loc),
                          .printer = printer};
  }

  std::string const name;
  std::atomic<State> state;
  ParentTask parent;
  std::atomic<std::optional<basics::ThreadId>>
      running_thread;  // TODO will be changed to a lock-free ptr to a
                       // ThreadInfo in the future
  std::source_location const source_location;
  std::shared_ptr<TaskMessage> printer;
  // possibly interesting other properties:
  // std::chrono::time_point<std::chrono::steady_clock> creation = std:;
};

/**
   A node in the task registry

   Use inheritance to circumvent problems with non-satified constraints for Node
   when used in ParentTask in TaskInRegistry
 */
struct Node : public containers::ThreadOwnedList<TaskInRegistry>::Node {};

struct ThreadTask;
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
  friend struct ThreadTask;
  Task(Task&& other) = default;
  Task& operator=(Task&& other) = default;
  Task(Task const&) = delete;
  Task& operator=(Task const&) = delete;

  Task(std::string name, std::shared_ptr<TaskMessage> printer = nullptr,
       bool isScheduled = false,
       std::source_location loc = std::source_location::current());
  ~Task();

  auto id() -> TaskId;
  auto source_location() -> basics::SourceLocationSnapshot;
  auto start() -> void;

 private:
  NodeReference _node_in_registry;
  Task* parent;
};

auto get_current_task() -> Task**;

/**
   Executes the given lambda in a new thread as a new task.

   Creates a new task in the task registry. Its parent is the task that was
   running on the already existing thread. Inside the lambda, you can use
   *get_current_task() to get its task.
 */
struct ThreadTask {
  ThreadTask(std::string name, std::function<void()> lambda,
             std::shared_ptr<TaskMessage> printer = nullptr,
             std::source_location loc = std::source_location::current());
};

/**
   Schedules the given lamda on the given lane of the scheduler queue as a new
   task.

   Creates a new task in the task registry with a scheduled state. As soon as
   the lambda is executed, the state is updated to running.
 */
struct ScheduledTask {
  ScheduledTask(std::string name, RequestLane lane,
                std::function<void()> lambda,
                std::shared_ptr<TaskMessage> printer = nullptr,
                std::source_location loc = std::source_location::current());
};
}  // namespace arangodb::task_monitoring

#pragma once

#include <vector>
#include <source_location>
#include <string>
#include <memory>
#include <optional>
#include <chrono>
#include <iostream>
#include "Inspection/Format.h"
#include "Inspection/Types.h"
#include "Logger/LogMacros.h"
#include "fmt/format.h"

namespace arangodb::task_registry {

// TODO this was just copied from async_registry promise.h
struct ThreadId {
  static auto current() noexcept -> ThreadId;
  auto name() -> std::string;
  pthread_t posix_id;
  pid_t kernel_id;
  bool operator==(ThreadId const&) const = default;
};
template<typename Inspector>
auto inspect(Inspector& f, ThreadId& x) {
  return f.object(x).fields(f.field("LWPID", x.kernel_id),
                            f.field("name", x.name()));
}
struct SourceLocation {
  std::string_view file_name;
  std::string_view function_name;
  std::uint_least32_t line;
  bool operator==(SourceLocation const&) const = default;
  auto static from(std::source_location loc) -> SourceLocation {
    return SourceLocation{.file_name = loc.file_name(),
                          .function_name = loc.function_name(),
                          .line = loc.line()};
  }
};
template<typename Inspector>
auto inspect(Inspector& f, SourceLocation& x) {
  return f.object(x).fields(f.field("file_name", x.file_name),
                            f.field("line", x.line),
                            f.field("function_name", x.function_name));
}

struct RootTask {
  std::string name;
  bool operator==(RootTask const&) const = default;
};
template<typename Inspector>
auto inspect(Inspector& f, RootTask& x) {
  return f.object(x).fields(f.field("name", x.name));
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
  std::optional<ThreadId> thread;
  SourceLocation source_location;
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

struct Task;
struct ParentTask
    : std::variant<RootTask, std::shared_ptr<Task>, TransactionId> {};

struct TaskRegistry;
struct TaskScope;
struct ScheduledTaskScope;

struct Task {
  static auto create(std::string name, std::source_location loc,
                     TaskRegistry* registry) -> std::shared_ptr<Task>;
  static auto subtask(TaskScope& parent, std::string name,
                      std::optional<TransactionId> transaction,
                      std::source_location loc, TaskRegistry* registry)
      -> std::shared_ptr<Task>;
  static auto scheduled(TaskScope& parent, std::string name,
                        std::source_location, TaskRegistry* registry)
      -> std::shared_ptr<Task>;
  static auto transaction_task(TransactionId transaction, std::string name,
                               std::source_location loc, TaskRegistry* registry)
      -> std::shared_ptr<Task>;
  ~Task();
  auto id() -> void* { return this; }
  auto snapshot() -> TaskSnapshot;

  friend TaskScope;
  friend ScheduledTaskScope;

 private:
  Task(ParentTask parent, std::string name, std::string state,
       std::optional<TransactionId> transaction, std::source_location loc,
       TaskRegistry* registry);
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
  std::optional<ThreadId>
      _running_thread;  // proably has to also be atomic because
                        // changes for scheduled task
  std::source_location const _source_location;
  TaskRegistry* const _registry;
  // possibly interesting other properties:
  // std::chrono::time_point<std::chrono::steady_clock> creation = std:;
};

/**
   A task in scope in a running task.

   The TaskScope sets the state of the corresponding task.
 */
struct TaskScope {
  // TODO possibly update source location of task in this constructor,
  TaskScope(std::shared_ptr<Task> task) : _task{task} {
    if (task) {
      _task->_running_thread = ThreadId::current();
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

  friend Task;

 private:
  auto task() -> std::shared_ptr<Task> { return _task; }
  std::shared_ptr<Task> _task;
};

struct ScheduledTaskScope {
  ScheduledTaskScope(std::shared_ptr<Task> task) : _task{task} {}
  ScheduledTaskScope(ScheduledTaskScope&&) = default;
  ScheduledTaskScope(ScheduledTaskScope const&) = delete;
  auto start() && -> TaskScope { return TaskScope{std::move(_task)}; }

 private:
  std::shared_ptr<Task> _task;
};

struct TaskRegistry {
  /**
     Create an overall task and its corresponding entry point thread task

     Returns a scope for the task: the task is already running and is done when
     scope is deleted.
   */
  auto start_task(std::string name,
                  std::source_location loc = std::source_location::current())
      -> TaskScope;

  /**
     Create a subtask that directly starts running

     Returns a scope for the task: the task is already running and is done when
     scope is deleted.
  */
  auto start_subtask(TaskScope& parent, std::string name,
                     std::optional<TransactionId> transactionId = std::nullopt,
                     std::source_location loc = std::source_location::current())
      -> TaskScope;

  /**
     Create a scheduled subtask that is not yet running

     The user has to call task->start() to start the task and get a scope for
     this running task. Only a call to start will set the task thread
  */
  auto schedule_subtask(
      TaskScope& parent, std::string name,
      std::source_location loc = std::source_location::current())
      -> ScheduledTaskScope;

  /**
     Create a task that belongs to a currently open transaction
   */
  auto start_transaction_task(
      TransactionId transaction, std::string name,
      std::source_location loc = std::source_location::current()) -> TaskScope;

  auto garbage_collect() {
    auto guard = std::lock_guard(_mutex);
    std::erase_if(_tasks, [&](auto const& weak) { return weak.expired(); });
  }
  template<typename F>
  auto for_task(F&& function) noexcept -> void {
    for (auto const& maybe_task : _tasks) {
      if (auto const& task = maybe_task.lock()) {
        function(task->snapshot());
      } else {
        std::cout << "Something is still here" << std::endl;
      }
    }
  }
  // TODO just here for debugging purpose
  auto log(std::string_view message) -> void {
    std::vector<TaskSnapshot> tasks;
    for_task([&](task_registry::TaskSnapshot task) {
      tasks.emplace_back(std::move(task));
    });
    LOG_DEVEL << fmt::format("{}: {}", message,
                             arangodb::inspection::json(tasks));
  }

 private:
  std::vector<std::weak_ptr<Task>> _tasks;
  std::mutex _mutex;
};

}  // namespace arangodb::task_registry

std::ostream& operator<<(std::ostream& os,
                         arangodb::task_registry::TaskSnapshot const& t);

// - currently: hand task over in function parameters to update its state inside
//   or create subtask; perhaps: thread local var with current task: but has to
//   be updated when execution is suspended (e.g. in async or async RestHandler)

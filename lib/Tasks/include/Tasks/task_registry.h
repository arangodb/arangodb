#pragma once

#include <boost/smart_ptr/enable_shared_from_this.hpp>
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

struct RootTask {
  std::string name;
  bool operator==(RootTask const&) const = default;
};
template<typename Inspector>
auto inspect(Inspector& f, RootTask& x) {
  return f.object(x).fields(f.field("name", x.name));
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

struct TaskSnapshot {
  std::string name;
  std::string state;
  void* id;
  ParentTaskSnapshot parent;
  std::optional<ThreadId> thread;
  bool operator==(TaskSnapshot const&) const = default;
};
template<typename Inspector>
auto inspect(Inspector& f, TaskSnapshot& x) {
  return f.object(x).fields(f.field("id", fmt::format("{}", x.id)),
                            f.field("name", x.name), f.field("state", x.state),
                            f.field("parent", x.parent),
                            f.field("thread", x.thread));
}

struct Task;
struct ParentTask : std::variant<RootTask, std::shared_ptr<Task>> {};

struct TaskRegistry;
struct TaskScope;
struct Task : std::enable_shared_from_this<Task> {
  static auto make(ParentTask parent, std::string name, bool is_running,
                   TaskRegistry* registry) -> std::shared_ptr<Task>;
  ~Task();
  /**
     can only be called on creating thread
   */
  auto update_state(std::string state) -> void;
  auto start() -> TaskScope;
  auto id() -> void* { return this; }
  auto snapshot() -> TaskSnapshot;

 private:
  Task(ParentTask parent, std::string name, bool is_running,
       TaskRegistry* registry);
  std::string const _name;
  std::string _state =
      "created";  // has to probably be atomic (for reading and writing
                  // concurrently on different threads), but is string...
  ParentTask _parent;
  std::optional<ThreadId>
      _running_thread;  // proably has to also be atomic because
                        // changes for scheduled task
  TaskRegistry* const _registry;
  // possibly interesting other properties:
  // std::chrono::time_point<std::chrono::steady_clock> creation = std:;
  // std::source_location
};

// make update_state only callable from here
struct TaskScope {
  TaskScope(std::shared_ptr<Task> task) : _task{task} {
    if (task) {
      _task->update_state("running");
    }
  }
  ~TaskScope() {
    if (_task) {
      _task->update_state("done");
    }
  }
  std::shared_ptr<Task> _task;
};

struct TaskRegistry {
  auto create_task(std::string name) -> std::shared_ptr<Task> {
    auto task = Task::make(ParentTask{RootTask{.name = std::move(name)}},
                           "entry point", true, this);
    auto guard = std::lock_guard(_mutex);
    _tasks.emplace_back(task);
    return task;
  }
  auto create_subtask(std::shared_ptr<Task> parent, std::string name,
                      bool is_running = true) -> std::shared_ptr<Task> {
    auto task =
        Task::make(ParentTask{parent}, std::move(name), is_running, this);
    auto guard = std::lock_guard(_mutex);
    _tasks.emplace_back(task);
    return task;
  }
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

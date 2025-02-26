#pragma once

#include <vector>
#include <source_location>
#include <string>
#include <memory>
#include <optional>
#include <chrono>
#include <iostream>
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

struct TaskSnapshot {
  std::string name;
  std::string state;
  void* id;
  void* parent;
  ThreadId thread;
  bool operator==(TaskSnapshot const&) const = default;
};
template<typename Inspector>
auto inspect(Inspector& f, TaskSnapshot& x) {
  return f.object(x).fields(f.field("id", fmt::format("{}", x.id)),
                            f.field("name", x.name), f.field("state", x.state),
                            f.field("parent", fmt::format("{}", x.parent)),
                            f.field("thread", x.thread));
}

struct TaskRegistry;

struct Task {
  Task(std::string name, std::shared_ptr<Task> parent, TaskRegistry* registry)
      : _name{std::move(name)},
        _parent{parent},
        _thread{ThreadId::current()},
        _registry{registry} {}
  ~Task();
  /**
     can only be called on creating thread
   */
  auto update_state(std::string state) -> void;
  auto id() -> void* { return this; }
  auto snapshot() -> TaskSnapshot {
    return TaskSnapshot{.name = _name,
                        .state = _state,
                        .id = id(),
                        .parent = _parent.get(),
                        .thread = _thread};
  }
  std::string const _name;
  std::string _state =
      "created";  // has to probably be atomic (for reading and writing
                  // concurrently on different threads), but is string...
  std::shared_ptr<Task> const _parent;
  ThreadId _thread;  // proably has to also be atomic because changes for
                     // scheduled task
  TaskRegistry* const _registry;
  // std::chrono::time_point<std::chrono::steady_clock> creation = std:;
};

struct TaskScope {
  ~TaskScope() {
    if (auto const& task = _task.lock()) {
      task->update_state("done");
    }
  }
  std::weak_ptr<Task> _task;
};

struct TaskRegistry {
  auto create_task(
      std::string name, std::shared_ptr<Task> parent = nullptr,
      std::source_location location = std::source_location::current())
      -> std::shared_ptr<Task> {
    auto task = std::make_shared<Task>(std::move(name), parent, this);
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

  std::vector<std::weak_ptr<Task>> _tasks;
  std::mutex _mutex;
};

}  // namespace arangodb::task_registry

std::ostream& operator<<(std::ostream& os,
                         arangodb::task_registry::TaskSnapshot const& t);

// - currently: hand task over in function parameters to update its state inside
//   or create subtask; perhaps: thread local var with current task: but has to
//   be updated when execution is suspended (e.g. in async or async RestHandler)

// - a parent task is only deleted when all its subtasks are gone

// perhaps it makes sense to have a special overall-task
// - cannot update its state
// - has no owning thread (at least not showing it, it will be saved in thread
// registry of creation thread)
// a scheduler task is also something different: has formally an owning thread
// (the creation thread) but can run on a different one (has to be shown as
// output) and has no thread when scheduled and not running

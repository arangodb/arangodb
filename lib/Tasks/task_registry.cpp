#include "Tasks/task_registry.h"
#include <source_location>

#include "fmt/format.h"
#include "Basics/Thread.h"
#include "Assertions/ProdAssert.h"
#include "Inspection/Format.h"

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

auto ThreadId::current() noexcept -> ThreadId {
  return ThreadId{.posix_id = arangodb::Thread::currentThreadId(),
                  .kernel_id = arangodb::Thread::currentKernelThreadId()};
}
auto ThreadId::name() -> std::string {
  return std::string{ThreadNameFetcher{posix_id}.get()};
}

auto Task::make(ParentTask parent, std::string name, bool is_running,
                std::source_location loc, TaskRegistry* registry)
    -> std::shared_ptr<Task> {
  struct MakeSharedTask : Task {
    MakeSharedTask(ParentTask parent, std::string name, bool is_running,
                   std::source_location loc, TaskRegistry* registry)
        : Task(parent, std::move(name), is_running, std::move(loc), registry) {}
  };
  return std::make_shared<MakeSharedTask>(parent, std::move(name), is_running,
                                          std::move(loc), registry);
}

Task::Task(ParentTask parent, std::string name, bool is_running,
           std::source_location loc, TaskRegistry* registry)
    : _name{std::move(name)},
      _parent{std::move(parent)},
      _running_thread{is_running ? std::optional<ThreadId>{ThreadId::current()}
                                 : std::nullopt},
      _source_location{std::move(loc)},
      _registry{registry} {
  if (!is_running) {
    _state = "scheduled";
  }
}

Task::~Task() {
  if (_registry) {
    _registry->garbage_collect();
  }
}

auto Task::update_state(std::string_view state, std::source_location loc)
    -> void {
  auto current_thread = ThreadId::current();
  ADB_PROD_ASSERT(current_thread == _running_thread) << fmt::format(
      "TaskRegistry::update_state was called from thread {} but needs to be "
      "called from its owning thread {}. Called at {}. Task: {} ({}), {}",
      fmt::format("{}", inspection::json(current_thread)),
      fmt::format("{}", inspection::json(_running_thread)),
      inspection::json(SourceLocation::from(loc)), _name, _state,
      inspection::json(SourceLocation::from(_source_location)));
  _state = state;
}

auto Task::start() -> TaskScope {
  if (not _running_thread) {
    _running_thread = ThreadId::current();
  }
  return TaskScope{shared_from_this()};
}

auto Task::snapshot() -> TaskSnapshot {
  return TaskSnapshot{
      .name = _name,
      .state = _state,
      .id = id(),
      .parent = std::visit(
          overloaded{[&](RootTask root) { return ParentTaskSnapshot{root}; },
                     [&](std::shared_ptr<Task> parent) {
                       return ParentTaskSnapshot{TaskIdWrapper{parent.get()}};
                     }},
          _parent),
      .thread = _running_thread,
      .source_location =
          SourceLocation{.file_name = _source_location.file_name(),
                         .function_name = _source_location.function_name(),
                         .line = _source_location.line()}};
}

}  // namespace arangodb::task_registry

std::ostream& operator<<(std::ostream& os,
                         arangodb::task_registry::TaskSnapshot const& t) {
  os << fmt::format("{}", arangodb::inspection::json(t));
  return os;
}

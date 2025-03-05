#include "Tasks/task_registry.h"

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
                TaskRegistry* registry) -> std::shared_ptr<Task> {
  struct MakeSharedTask : Task {
    MakeSharedTask(ParentTask parent, std::string name, bool is_running,
                   TaskRegistry* registry)
        : Task(parent, std::move(name), is_running, registry) {}
  };
  return std::make_shared<MakeSharedTask>(parent, std::move(name), is_running,
                                          registry);
}

Task::Task(ParentTask parent, std::string name, bool is_running,
           TaskRegistry* registry)
    : _name{std::move(name)},
      _parent{std::move(parent)},
      _running_thread{is_running ? std::optional<ThreadId>{ThreadId::current()}
                                 : std::nullopt},
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

auto Task::update_state(std::string state) -> void {
  auto current_thread = ThreadId::current();
  ADB_PROD_ASSERT(current_thread == _running_thread)
      << "TaskRegistry::update_state was called from thread "
      << fmt::format("{}", inspection::json(current_thread))
      << " but needs to be called from its owning thread "
      << fmt::format("{}", inspection::json(_running_thread))
      << ". Task: " << _name << " (" << _state << ")";
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
      .thread = _running_thread};
}

}  // namespace arangodb::task_registry

std::ostream& operator<<(std::ostream& os,
                         arangodb::task_registry::TaskSnapshot const& t) {
  os << fmt::format("{}", arangodb::inspection::json(t));
  return os;
}

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

auto Task::create(std::string name, std::source_location loc,
                  TaskRegistry* registry) -> std::shared_ptr<Task> {
  struct MakeSharedTask : Task {
    MakeSharedTask(ParentTask parent, std::string name, std::string state,
                   std::source_location loc, TaskRegistry* registry)
        : Task(parent, std::move(name), std::move(state), std::nullopt,
               std::move(loc), registry) {}
  };
  return std::make_shared<MakeSharedTask>(ParentTask{RootTask{}},
                                          std::move(name), "created",
                                          std::move(loc), registry);
}
auto Task::subtask(TaskScope& parent, std::string name,
                   std::optional<TransactionId> transaction,
                   std::source_location loc, TaskRegistry* registry)
    -> std::shared_ptr<Task> {
  struct MakeSharedTask : Task {
    MakeSharedTask(ParentTask parent, std::string name, std::string state,
                   std::optional<TransactionId> transaction,
                   std::source_location loc, TaskRegistry* registry)
        : Task(parent, std::move(name), std::move(state),
               std::move(transaction), std::move(loc), registry) {}
  };
  return std::make_shared<MakeSharedTask>(
      ParentTask{parent.task()}, std::move(name), "created",
      std::move(transaction), std::move(loc), registry);
}
auto Task::scheduled(TaskScope& parent, std::string name,
                     std::source_location loc, TaskRegistry* registry)
    -> std::shared_ptr<Task> {
  struct MakeSharedTask : Task {
    MakeSharedTask(ParentTask parent, std::string name, std::string state,
                   std::source_location loc, TaskRegistry* registry)
        : Task(parent, std::move(name), std::move(state), std::nullopt,
               std::move(loc), registry) {}
  };
  return std::make_shared<MakeSharedTask>(ParentTask{parent.task()},
                                          std::move(name), "scheduled",
                                          std::move(loc), registry);
}

auto Task::transaction_task(TransactionId transaction, std::string name,
                            std::source_location loc, TaskRegistry* registry)
    -> std::shared_ptr<Task> {
  struct MakeSharedTask : Task {
    MakeSharedTask(ParentTask parent, std::string name, std::string state,
                   std::source_location loc, TaskRegistry* registry)
        : Task(parent, std::move(name), std::move(state), std::nullopt,
               std::move(loc), registry) {}
  };
  return std::make_shared<MakeSharedTask>(ParentTask{std::move(transaction)},
                                          std::move(name), "created",
                                          std::move(loc), registry);
}

Task::Task(ParentTask parent, std::string name, std::string state,
           std::optional<TransactionId> transaction, std::source_location loc,
           TaskRegistry* registry)
    : _name{std::move(name)},
      _state{std::move(state)},
      _parent{std::move(parent)},
      _transaction{std::move(transaction)},
      _source_location{std::move(loc)},
      _registry{registry} {}

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

auto Task::snapshot() -> TaskSnapshot {
  return TaskSnapshot{
      .name = _name,
      .state = _state,
      .id = id(),
      .parent = std::visit(
          overloaded{[&](RootTask root) { return ParentTaskSnapshot{root}; },
                     [&](std::shared_ptr<Task> parent) {
                       return ParentTaskSnapshot{TaskIdWrapper{parent.get()}};
                     },
                     [&](TransactionId transaction) {
                       return ParentTaskSnapshot{transaction};
                     }},
          _parent),
      .transaction = _transaction,
      .thread = _running_thread,
      .source_location =
          SourceLocation{.file_name = _source_location.file_name(),
                         .function_name = _source_location.function_name(),
                         .line = _source_location.line()}};
}

auto TaskRegistry::start_task(std::string name, std::source_location loc)
    -> TaskScope {
  auto task = Task::create(std::move(name), std::move(loc), this);
  auto guard = std::lock_guard(_mutex);
  _tasks.emplace_back(task);
  return TaskScope{task};
}

auto TaskRegistry::start_subtask(TaskScope& parent, std::string name,
                                 std::optional<TransactionId> transactionId,
                                 std::source_location loc) -> TaskScope {
  auto task = Task::subtask(parent, std::move(name), std::move(transactionId),
                            std::move(loc), this);
  auto guard = std::lock_guard(_mutex);
  _tasks.emplace_back(task);
  return TaskScope{task};
}

auto TaskRegistry::schedule_subtask(TaskScope& parent, std::string name,
                                    std::source_location loc)
    -> ScheduledTaskScope {
  auto task = Task::scheduled(parent, std::move(name), std::move(loc), this);
  auto guard = std::lock_guard(_mutex);
  _tasks.emplace_back(task);
  return ScheduledTaskScope{task};
}

auto TaskRegistry::start_transaction_task(TransactionId transaction,
                                          std::string name,
                                          std::source_location loc)
    -> TaskScope {
  auto task = Task::transaction_task(std::move(transaction), std::move(name),
                                     std::move(loc), this);
  auto guard = std::lock_guard(_mutex);
  _tasks.emplace_back(task);
  return TaskScope{task};
}

}  // namespace arangodb::task_registry

std::ostream& operator<<(std::ostream& os,
                         arangodb::task_registry::TaskSnapshot const& t) {
  os << fmt::format("{}", arangodb::inspection::json(t));
  return os;
}

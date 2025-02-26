#include "Tasks/task_registry.h"

#include "fmt/format.h"
#include "Basics/Thread.h"
#include "Assertions/ProdAssert.h"
#include "Inspection/Format.h"

namespace arangodb::task_registry {

auto ThreadId::current() noexcept -> ThreadId {
  return ThreadId{.posix_id = arangodb::Thread::currentThreadId(),
                  .kernel_id = arangodb::Thread::currentKernelThreadId()};
}
auto ThreadId::name() -> std::string {
  return std::string{ThreadNameFetcher{posix_id}.get()};
}

Task::~Task() {
  if (_registry) {
    _registry->garbage_collect();
  }
}

auto Task::update_state(std::string state) -> void {
  auto current_thread = ThreadId::current();
  ADB_PROD_ASSERT(current_thread == _thread)
      << "TaskRegistry::update_state was called from thread "
      << fmt::format("{}", inspection::json(current_thread))
      << " but needs to be called from its owning thread "
      << fmt::format("{}", inspection::json(_thread)) << ". Task: " << _name
      << " (" << _state << ")";
  _state = state;
}

}  // namespace arangodb::task_registry

std::ostream& operator<<(std::ostream& os,
                         arangodb::task_registry::TaskSnapshot const& t) {
  os << fmt::format("({}: {} -> {} ({}))", t.id, t.name, t.parent, t.state);
  return os;
}

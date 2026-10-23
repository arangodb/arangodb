#pragma once

#include <cstddef>
#include "Activities/RegistryGlobalVariable.h"

namespace arangodb::activities::tests {
inline auto garbageCollectAllOnCurrentThread() -> void {
  size_t deletedCount = 0;
  do {
    deletedCount = get_thread_registry().garbage_collect();
  } while (deletedCount > 0);
}
}  // namespace arangodb::activities::tests

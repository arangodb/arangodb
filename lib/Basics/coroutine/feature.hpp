#pragma once

#include "Basics/coroutine/thread_registry.h"

namespace arangodb::coroutine {

/**
   Registry of all active promises on this thread.
 */
thread_local ThreadRegistry* promise_registry;

}  // namespace arangodb::coroutine

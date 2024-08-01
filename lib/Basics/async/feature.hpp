#pragma once

#include "Basics/async/promise_registry.hpp"

namespace arangodb::coroutine {

/**
   Registry of all active promises on this thread.
 */
thread_local PromiseRegistry* promise_registry;

}  // namespace arangodb::coroutine

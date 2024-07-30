#pragma once

#include "Basics/async/promise_registry.hpp"

namespace arangodb::coroutine {

/**
   Registry of all active promises on this thread.
 */
thread_local std::shared_ptr<coroutine::PromiseRegistry> promise_registry;

}  // namespace arangodb::coroutine

#pragma once

#include "registry.h"

namespace arangodb::coroutine {

// TODO somehow get rid of this global variable
/**
   Global variable that holds all coroutines.
 */
extern Registry coroutine_registry;

/**
   Get registry of all active coroutine promises on this thread.
 */
auto get_thread_registry() noexcept -> ThreadRegistry&;

}  // namespace arangodb::coroutine

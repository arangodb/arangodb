////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_CLEANUPFUNCTIONS_H
#define ARANGODB_BASICS_CLEANUPFUNCTIONS_H 1

#include <functional>
#include <memory>
#include <vector>

#include "Basics/Common.h"

#include "Basics/Mutex.h"

namespace arangodb {
namespace basics {

/**
 * @brief A class to manage and handle cleanup functions on shutdown.
 * Thread-safe execution. It is meant to collect functions that should
 * be executed on every shutdown we can react to.
 * e.g. FATAL errors
 */
class CleanupFunctions {
  CleanupFunctions(CleanupFunctions const&) = delete;
  CleanupFunctions& operator=(CleanupFunctions const&) = delete;

  // Typedefs
 public:
  /**
   * @brief A cleanup function, will be called with the exit code
   * and some data that is generated during exit (just as TRI_ExitFunction_t).
   */
  typedef std::function<void(int, void*)> CleanupFunction;

  // Functions
 public:
  /**
   * @brief Register a new function to be executed during
   * any "expected" exit of the server (FATAL, CTRL+C, Shutdown)
   *
   * @param func The function to be executed.
   */
  static void registerFunction(std::unique_ptr<CleanupFunction> func);

  /**
   * @brief Execute all functions in _cleanupFunctions
   *
   * @param code The exit code provided
   * @param data data given during exit
   */
  static void run(int code, void* data);

 private:
  /**
   * @brief A lock for the cleanup functions.
   * Used to make sure they are only executed once.
   * This is NOT performance critical as those functions
   * only kick in on startup (insert) and shutdown (execute)
   */
  static Mutex _functionsMutex;

  /**
   * @brief A list of functions to be executed during cleanup
   */
  static std::vector<std::unique_ptr<CleanupFunction>> _cleanupFunctions;
};
}  // namespace basics
}  // namespace arangodb

#endif

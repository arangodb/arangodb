////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////

#include "Async/Registry/registry.h"
#include "Async/Registry/registry_variable.h"

#include <csignal>
#include <iostream>

using namespace arangodb::async_registry;

auto breakpoint() { raise(SIGINT); }

int main() {
  breakpoint();

  // empty registry
  auto thread_registry = registry.add_thread();
  auto expected = std::string("async registry");

  breakpoint();

  // one promise
  thread_registry->add_promise(Requester::current_thread(),
                               std::source_location::current());
  expected = std::string("async registry");

  breakpoint();

  // thread_registry->add_promise(Requester{promise->id()},
  //                              std::source_location::current());
  std::cout << "Hello after breakpoint " << expected << std::endl;
  // run_test(testee, expected) which internally calls breakpoint again

  return 0;
}

// tests
// - empty registry
// - one stacktrace with one entry
// - one stacktrace with complicated entries
//     |- async 3
//     |   |- async 2
//     |- async 1
//   - thread
// - several stacktracees with one entry each
// - several thread_registries (how will this be different?)

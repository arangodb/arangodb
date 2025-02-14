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

#include <csignal>
#include <iostream>

using namespace arangodb::async_registry;

auto breakpoint() { raise(SIGINT); }

int main() {
  // create a registry
  Registry registry;
  auto thread_registry = registry.add_thread();
  auto* promise = thread_registry->add_promise(Requester::current_thread(),
                                               std::source_location::current());
  const auto testee = 1;    // tuple of registries
  const auto expected = 3;  // for one registry, get a string version of it
  breakpoint();
  std::cout << "Hello after breakpoint" << testee + expected << std::endl;
  // run_test(testee, expected) which internally calls breakpoint again

  promise->mark_for_deletion();
  thread_registry->garbage_collect();
  return 0;
}

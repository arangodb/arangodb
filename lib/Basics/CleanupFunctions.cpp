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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "CleanupFunctions.h"

#include <mutex>

using namespace arangodb;
using namespace arangodb::basics;

// Init static class members
std::mutex CleanupFunctions::_functionsMutex;
std::vector<std::unique_ptr<CleanupFunctions::CleanupFunction>>
    CleanupFunctions::_cleanupFunctions;

void CleanupFunctions::registerFunction(
    std::unique_ptr<CleanupFunctions::CleanupFunction> func) {
  std::lock_guard locker{_functionsMutex};
  _cleanupFunctions.emplace_back(std::move(func));
}

void CleanupFunctions::run(int code, void* data) {
  std::lock_guard locker{_functionsMutex};
  for (auto const& func : _cleanupFunctions) {
    (*func)(code, data);
  }
  _cleanupFunctions.clear();
}

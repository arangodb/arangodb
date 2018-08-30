////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#include "CleanupFunctions.h"
#include "Basics/MutexLocker.h"

using namespace arangodb;
using namespace arangodb::basics;

// Init static class members
Mutex CleanupFunctions::_functionsMutex;
std::vector<std::unique_ptr<CleanupFunctions::CleanupFunction>> CleanupFunctions::_cleanupFunctions;

void CleanupFunctions::registerFunction(std::unique_ptr<CleanupFunctions::CleanupFunction> func) {
  MUTEX_LOCKER(locker, _functionsMutex);
  _cleanupFunctions.emplace_back(std::move(func));
}

void CleanupFunctions::run(int code, void* data) {
  MUTEX_LOCKER(locker, _functionsMutex);
  for (auto const& func : _cleanupFunctions) {
    (*func)(code, data);
  }
  _cleanupFunctions.clear();
}

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "UnshackledMutex.h"

#include "Basics/debugging.h"

void arangodb::basics::UnshackledMutex::lock() {
  auto guard = std::unique_lock(_mutex);
  _cv.wait(guard, [&] { return !_locked; });
  TRI_ASSERT(!_locked);
  _locked = true;
}

void arangodb::basics::UnshackledMutex::unlock() {
  {
    auto guard = std::unique_lock(_mutex);
    TRI_ASSERT(_locked);
    _locked = false;
  }
  _cv.notify_one();
}

auto arangodb::basics::UnshackledMutex::try_lock() -> bool {
  auto guard = std::unique_lock(_mutex, std::try_to_lock);
  if (guard.owns_lock() && !_locked) {
    _locked = true;
    return true;
  }

  return false;
}

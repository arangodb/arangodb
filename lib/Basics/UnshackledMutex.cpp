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

namespace arangodb::basics {

void UnshackledMutex::lock() noexcept {
  // cppcheck-suppress throwInNoexceptFunction
  auto func = +[](bool const* locked) noexcept { return !*locked; };
  absl::MutexLock guard{&_mutex,
                        absl::Condition{func, &std::as_const(_locked)}};
  _locked = true;
}

void UnshackledMutex::unlock() noexcept {
  absl::MutexLock guard{&_mutex};
  TRI_ASSERT(_locked);
  _locked = false;
}

bool UnshackledMutex::try_lock() noexcept {
  if (!_mutex.TryLock()) {
    return false;
  }
  bool const was_locked = std::exchange(_locked, true);
  _mutex.Unlock();
  return !was_locked;
}

}  // namespace arangodb::basics

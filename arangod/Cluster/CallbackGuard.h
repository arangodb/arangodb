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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_CLUSTER_CALLBACKGUARD_H
#define ARANGOD_CLUSTER_CALLBACKGUARD_H

#include <functional>

#include "Cluster/ClusterTypes.h"

namespace arangodb {
namespace cluster {

/// @brief If constructed with a callback, the given callback will be called
/// exactly once: Either during destruction, or when the object is overwritten
/// (via operator=()), or when it's explicitly cleared. It's not copyable,
/// but movable.
class CallbackGuard {
 public:
  // Calls the callback given callback upon destruction.
  // Allows only move semantics and no copy semantics.

  CallbackGuard();
  // IMPORTANT NOTE:
  // The passed callback should not throw exceptions, they will not be caught
  // here, but thrown by the destructor!
  explicit CallbackGuard(std::function<void(void)> callback);
  ~CallbackGuard();

  // Note that the move constructor of std::function is not noexcept until
  // C++20. Thus we cannot mark the constructors here noexcept.
  // NOLINTNEXTLINE(hicpp-noexcept-move,performance-noexcept-move-constructor)
  CallbackGuard(CallbackGuard&& other);
  // operator= additionally calls the _callback, and this can also throw.
  // NOLINTNEXTLINE(hicpp-noexcept-move,performance-noexcept-move-constructor)
  CallbackGuard& operator=(CallbackGuard&&);

  CallbackGuard(CallbackGuard const&) = delete;
  CallbackGuard& operator=(CallbackGuard const&) = delete;

  /// @brief Call the contained callback, then delete it.
  void callAndClear();

 private:
  void call();

  std::function<void(void)> _callback;
};

}}

#endif

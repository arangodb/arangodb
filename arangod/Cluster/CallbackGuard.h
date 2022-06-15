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

#pragma once

#include "Cluster/ClusterTypes.h"

#include <function2.hpp>

namespace arangodb::cluster {

/// @brief If constructed with a callback,
/// the given callback will be called exactly once, except clear:
/// Either during destruction, or when the object is overwritten.
class CallbackGuard {
 public:
  using Callback = fu2::unique_function<void() noexcept>;

  CallbackGuard() noexcept;
  explicit CallbackGuard(Callback callback) noexcept;
  ~CallbackGuard() noexcept;

  CallbackGuard(CallbackGuard&& other) noexcept;
  CallbackGuard& operator=(CallbackGuard&& other) noexcept;

  CallbackGuard(CallbackGuard const&) = delete;
  CallbackGuard& operator=(CallbackGuard const&) = delete;

  bool empty() const noexcept;

 private:
  void call() noexcept;

  Callback _callback;
};

}  // namespace arangodb::cluster

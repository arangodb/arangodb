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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "Cluster/CallbackGuard.h"

#include <utility>

namespace arangodb::cluster {

CallbackGuard::CallbackGuard() noexcept = default;

CallbackGuard::CallbackGuard(Callback callback) noexcept
    : _callback{std::move(callback)} {}

CallbackGuard::~CallbackGuard() noexcept { call(); }

CallbackGuard::CallbackGuard(CallbackGuard&& other) noexcept = default;

CallbackGuard& CallbackGuard::operator=(CallbackGuard&& other) noexcept {
  call();
  _callback = std::move(other._callback);
  return *this;
}

bool CallbackGuard::empty() const noexcept { return _callback.empty(); }

void CallbackGuard::call() noexcept {
  if (_callback) {
    _callback();
  }
}

}  // namespace arangodb::cluster

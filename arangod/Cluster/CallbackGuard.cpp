////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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

#include "CallbackGuard.h"

using namespace arangodb;
using namespace arangodb::cluster;

CallbackGuard::CallbackGuard() : _callback(nullptr) {}

CallbackGuard::CallbackGuard(std::function<void(void)> callback)
    : _callback(std::move(callback)) {}

// NOLINTNEXTLINE(hicpp-noexcept-move,performance-noexcept-move-constructor)
CallbackGuard::CallbackGuard(CallbackGuard&& other)
    : _callback(std::move(other._callback)) {
  other._callback = nullptr;
}

// NOLINTNEXTLINE(hicpp-noexcept-move,performance-noexcept-move-constructor)
CallbackGuard& CallbackGuard::operator=(CallbackGuard&& other) {
  call();
  _callback = std::move(other._callback);
  other._callback = nullptr;
  return *this;
}

CallbackGuard::~CallbackGuard() { call(); }

void CallbackGuard::callAndClear() {
  call();
  _callback = nullptr;
}

void CallbackGuard::call() {
  if (_callback) {
    _callback();
  }
}

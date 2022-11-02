////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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

#include "WaitForBag.h"

#include <Futures/Promise.h>
#include <Futures/Future.h>
#include <Futures/Unit.h>

using namespace arangodb;

auto WaitForBag::addWaitFor() -> futures::Future<futures::Unit> {
  using namespace arangodb::futures;
  return _waitForBag.emplace_back(Promise<Unit>{}).getFuture();
}

void WaitForBag::resolveAll() {
  for (auto& promise : _waitForBag) {
    TRI_ASSERT(promise.valid());
    promise.setValue();
  }
  _waitForBag.clear();
}

void WaitForBag::resolveAll(std::exception_ptr const& ex) {
  for (auto& promise : _waitForBag) {
    TRI_ASSERT(promise.valid());
    promise.setException(ex);
  }
  _waitForBag.clear();
}

auto WaitForBag::empty() const noexcept -> bool { return _waitForBag.empty(); }

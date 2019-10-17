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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "SharedQueryState.h"

#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"

#include "Logger/LogMacros.h"

using namespace arangodb;
using namespace arangodb::aql;

void SharedQueryState::invalidate() {
  std::lock_guard<std::mutex> guard(_mutex);
  _valid = false;
  _numNotifications += 1;
  _continueCallback = nullptr;
  // guard.unlock();
  _cv.notify_one();
}

/// this has to stay for a backwards-compatible AQL HTTP API (hasMore).
void SharedQueryState::waitForAsyncResponse() {
  std::unique_lock<std::mutex> guard(_mutex);
  _cv.wait(guard, [&] { return _numNotifications > 0; });
  TRI_ASSERT(_numNotifications > 0);
  _numNotifications--;
}

/// @brief setter for the continue handler:
///        We can either have a handler or a callback
void SharedQueryState::setContinueHandler(std::function<void()> const& handler) {
  std::lock_guard<std::mutex> guard(_mutex);
  _continueCallback = handler;
}

/// execute the _continueCallback. must hold _mutex,
bool SharedQueryState::executeContinueCallback() {
  if (!_valid) {
    return false;
  }
  
  TRI_ASSERT(_continueCallback);
  auto scheduler = SchedulerFeature::SCHEDULER;
  if (ADB_UNLIKELY(scheduler == nullptr)) {
    // We are shutting down
    return false;
  }
  
  // do NOT use scheduler->post(), can have high latency that
  //  then backs up libcurl callbacks to other objects
  return scheduler->queue(RequestLane::CLIENT_AQL,
                          [self = shared_from_this(),
                           cb = _continueCallback] () mutable {
    cb();
    std::lock_guard<std::mutex> guard(self->_mutex);
    if (--(self->_numNotifications) > 0) {
      self->executeContinueCallback();
    }
  });
}

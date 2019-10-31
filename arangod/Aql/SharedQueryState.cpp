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
  _numWakeups += 1;
  _wakeupCb = nullptr;
  _valid = false;
  // guard.unlock();
  _cv.notify_all();
}

/// this has to stay for a backwards-compatible AQL HTTP API (hasMore).
void SharedQueryState::waitForAsyncWakeup() {
  std::unique_lock<std::mutex> guard(_mutex);
  _cv.wait(guard, [&] { return _numWakeups > 0; });
  TRI_ASSERT(_numWakeups > 0);
  _numWakeups--;
}

/// @brief setter for the continue handler:
///        We can either have a handler or a callback
void SharedQueryState::setWakeupHandler(std::function<bool()> const& cb) {
  std::lock_guard<std::mutex> guard(_mutex);
  _wakeupCb = cb;
}

/// execute the _continueCallback. must hold _mutex,
bool SharedQueryState::executeWakeupCallback() {
  if (!_valid) {
    return false;
  }
  
  TRI_ASSERT(_wakeupCb);
  auto scheduler = SchedulerFeature::SCHEDULER;
  if (ADB_UNLIKELY(scheduler == nullptr)) {
    // We are shutting down
    return false;
  }
  TRI_ASSERT(_numWakeups > 0);
  
  return scheduler->queue(RequestLane::CLIENT_AQL,
                          [self = shared_from_this(),
                           cb = _wakeupCb] () mutable {
    
    bool cntn = true;
    while(cntn) {
      cntn = false;
      try {
        cntn = cb();
      } catch (std::exception const& ex) {
        LOG_TOPIC("e988a", WARN, Logger::QUERIES)
        << "Exception when continuing rest handler: " << ex.what();
      } catch (...) {
         LOG_TOPIC("e988b", WARN, Logger::QUERIES)
         << "Exception when continuing rest handler";
       }
      if (!cntn) {
        break;
      }
      std::lock_guard<std::mutex> guard(self->_mutex);
      TRI_ASSERT(self->_numWakeups > 0);
      cntn = (--(self->_numWakeups) > 0);
    }
  });
}

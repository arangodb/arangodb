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
  _cv.notify_all();
}

/// this has to stay for a backwards-compatible AQL HTTP API (hasMore).
void SharedQueryState::waitForAsyncWakeup() {
  std::unique_lock<std::mutex> guard(_mutex);
  TRI_ASSERT(!_wakeupCb);
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

void SharedQueryState::resetWakeupHandler() {
  std::lock_guard<std::mutex> guard(_mutex);
  _wakeupCb = nullptr;
}

/// execute the _continueCallback. must hold _mutex,
void SharedQueryState::execute() {
  TRI_ASSERT(_valid);
  
  if (!_wakeupCb) {
    _cv.notify_one();
    return;
  }

  auto scheduler = SchedulerFeature::SCHEDULER;
  if (ADB_UNLIKELY(scheduler == nullptr)) {
    // We are shutting down
    return;
  }
  TRI_ASSERT(_numWakeups > 0);
  
  if (_inWakeupCb) {
    return;
  }
  _inWakeupCb = true;

  bool queued = scheduler->queue(RequestLane::CLIENT_AQL,
                                 [self = shared_from_this(),
                                  cb = _wakeupCb]() mutable {

    while (true) {
      bool cntn = false;
      try {
        cntn = cb();
      } catch (std::exception const& ex) {
        LOG_TOPIC("e988a", WARN, Logger::QUERIES)
            << "Exception when continuing rest handler: " << ex.what();
      } catch (...) {
        LOG_TOPIC("e988b", WARN, Logger::QUERIES)
            << "Exception when continuing rest handler";
      }
      
      const uint32_t n = self->_numWakeups.fetch_sub(1);
      TRI_ASSERT(n > 0);

      if (!cntn || n == 1) {
        std::lock_guard<std::mutex> guard(self->_mutex);
        if (!cntn || self->_numWakeups.load() == 0) {
          self->_inWakeupCb = false;
          break;
        }
      }
    }
  });
  
  if (!queued) { // just invalidate
     _wakeupCb = nullptr;
     _valid = false;
     _inWakeupCb = false;
     _cv.notify_all();
  }
}

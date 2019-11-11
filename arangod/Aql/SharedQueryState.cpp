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

#include "Basics/ScopeGuard.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"

#include "Logger/LogMacros.h"

using namespace arangodb;
using namespace arangodb::aql;

void SharedQueryState::invalidate() {
  std::lock_guard<std::mutex> guard(_mutex);
  _wakeupCb = nullptr;
  _cbVersion++;
  _valid = false;
  _cv.notify_all();
}

/// this has to stay for a backwards-compatible AQL HTTP API (hasMore).
void SharedQueryState::waitForAsyncWakeup() {
//  LOG_DEVEL << "waiting " << _numWakeups << " " << this;

  std::unique_lock<std::mutex> guard(_mutex);
  if (!_valid) {
    return;
  }
  
  TRI_ASSERT(!_wakeupCb);
  _cv.wait(guard, [&] { return _numWakeups > 0 || !_valid; });
  TRI_ASSERT(_numWakeups > 0 || !_valid);
  _numWakeups--;
  
//  LOG_DEVEL << "wakeup " << _numWakeups << " " << this;;
}

/// @brief setter for the continue handler:
///        We can either have a handler or a callback
void SharedQueryState::setWakeupHandler(std::function<bool()> const& cb) {
  std::lock_guard<std::mutex> guard(_mutex);
  _wakeupCb = cb;
//  _numWakeups = 0;
  _cbVersion++;
  LOG_DEVEL << "setting wakeup handler " << this;
}

void SharedQueryState::resetWakeupHandler() {
  std::lock_guard<std::mutex> guard(_mutex);
  if (_wakeupCb) {
    LOG_DEVEL << "resetting wakeup handler " << this;
  }
  _wakeupCb = nullptr;
  _numWakeups = 0;
  _cbVersion++;
}

/// execute the _continueCallback. must hold _mutex,
void SharedQueryState::execute() {
  TRI_ASSERT(_valid);
  uint32_t n = _numWakeups++;

  if (!_wakeupCb) {
    LOG_DEVEL << "notify_one() " << this;
    _cv.notify_one();
    return;
  }

  if (n > 0) {
    LOG_DEVEL << "other handler already running " << this;
    return;
  }

  queueHandler();
}
  
void SharedQueryState::queueHandler() {
  
  if (_numWakeups == 0 || !_wakeupCb || !_valid) {
    bool c = !_wakeupCb;
    LOG_DEVEL << "queueHandler leave, numWakeups" << _numWakeups << " wakeup cb: " << (!c) << " valid: " << _valid << " " << this;
    return;
  }
  
  auto scheduler = SchedulerFeature::SCHEDULER;
  if (ADB_UNLIKELY(scheduler == nullptr)) {
    // We are shutting down
    return;
  }
      
  bool queued = scheduler->queue(RequestLane::CLIENT_AQL,
                                 [self = shared_from_this(),
                                  cb = _wakeupCb,
                                  v = _cbVersion]() {
//    auto guard = scopeGuard([&] {
//      std::unique_lock<std::mutex> lck(self->_mutex);
//      self->_inWakeupCb = false;
//    });
    
    std::unique_lock<std::mutex> lck(self->_mutex, std::defer_lock);

    do {
      bool cntn = false;
      try {
        cntn = cb();
      } catch (...) {}
      
      lck.lock();
      if (v == self->_cbVersion) {
        uint32_t c = self->_numWakeups--;
        if (c == 1 || !cntn || !self->_valid) {
          break;
        }
      } else {
        LOG_DEVEL << "outdated handler";
        return;
      }
      lck.unlock();
    } while (true);

    TRI_ASSERT(lck);
    self->queueHandler();
  });
  
  if (!queued) { // just invalidate
     _wakeupCb = nullptr;
     _valid = false;
     _cv.notify_all();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "SharedQueryState.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Exceptions.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/RequestLane.h"
#include "RestServer/QueryRegistryFeature.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "Logger/LogMacros.h"

using namespace arangodb;
using namespace arangodb::aql;

SharedQueryState::SharedQueryState(ArangodServer& server)
    : SharedQueryState(server, SchedulerFeature::SCHEDULER) {}

SharedQueryState::SharedQueryState(ArangodServer& server, Scheduler* scheduler)
    : _server(server),
      _scheduler(scheduler),
      _wakeupCb(nullptr),
      _numWakeups(0),
      _maxTasks(static_cast<unsigned>(
          _server.getFeature<QueryRegistryFeature>().maxParallelism())),
      _numTasks(0),
      _valid(true) {}

void SharedQueryState::invalidate() {
  {
    std::lock_guard<std::mutex> guard(_mutex);
    _wakeupCb = nullptr;
    _valid = false;
#if SHARED_STATE_LOGGING
    LOG_DEVEL << this << ", " << __func__ << ": invalidating everything";
#endif
  }
  _cv.notify_all();  // wakeup everyone else

  if (_numTasks.load() > 0) {
    std::unique_lock<std::mutex> guard(_mutex);
    _cv.wait(guard, [&] { return _numTasks.load() == 0; });
  }
}

bool SharedQueryState::consumeWakeup() {
  std::lock_guard<std::mutex> guard(_mutex);
#if SHARED_STATE_LOGGING
  LOG_DEVEL << this << ", " << __func__ << ": entered with " << _numWakeups;
#endif
  if (_numWakeups > 0) {
    --_numWakeups;
#if SHARED_STATE_LOGGING
    LOG_DEVEL << this << ", " << __func__ << ": decreased numWakeups to "
              << _numWakeups;
#endif
    return true;
  }
  return false;
}

/// this has to stay for a backwards-compatible AQL HTTP API (hasMore).
void SharedQueryState::waitForAsyncWakeup() {
  std::unique_lock<std::mutex> guard(_mutex);
  if (!_valid) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_KILLED);
  }

  TRI_ASSERT(!_wakeupCb);
#if SHARED_STATE_LOGGING
  LOG_DEVEL << this << ", " << __func__ << ": entered with " << _numWakeups;
#endif
  _cv.wait(guard, [&] { return _numWakeups > 0 || !_valid; });
  TRI_ASSERT(_numWakeups > 0 || !_valid);
  if (_valid) {
    TRI_ASSERT(_numWakeups > 0);
    --_numWakeups;
#if SHARED_STATE_LOGGING
    LOG_DEVEL << this << ", " << __func__ << ": decreased numWakeups to "
              << _numWakeups;
#endif
  } else {
#if SHARED_STATE_LOGGING
    LOG_DEVEL << this << ", " << __func__
              << ": did not decrease numWakeups. numWakeups: " << _numWakeups;
#endif
  }
}

/// @brief setter for the continue handler:
///        We can either have a handler or a callback
void SharedQueryState::setWakeupHandler(std::function<bool()> const& cb) {
  std::lock_guard<std::mutex> guard(_mutex);
  _wakeupCb = cb;
  // intentionally do not clobber _numWakeups here
#if SHARED_STATE_LOGGING
  LOG_DEVEL << this << ", " << __func__ << ": numWakeups: " << _numWakeups;
#endif
}

void SharedQueryState::resetWakeupHandler() {
  std::lock_guard<std::mutex> guard(_mutex);
  _wakeupCb = nullptr;
  // intentionally do not clobber _numWakeups here
#if SHARED_STATE_LOGGING
  LOG_DEVEL << this << ", " << __func__ << ": numWakeups: " << _numWakeups;
#endif
}

/// execute the _continueCallback. must hold _mutex,
void SharedQueryState::notifyWaiter(std::unique_lock<std::mutex>& guard) {
  TRI_ASSERT(guard);
  if (!_valid) {
#if SHARED_STATE_LOGGING
    LOG_DEVEL << this << ", " << __func__
              << ": not valid. numWakeups: " << _numWakeups;
#endif
    guard.unlock();
    _cv.notify_all();
    return;
  }

  unsigned n = _numWakeups++;
  TRI_ASSERT(n < 10000);
#if SHARED_STATE_LOGGING
  LOG_DEVEL << this << ", " << __func__
            << ": increased numWakeups to: " << _numWakeups;
#endif
  if (!_wakeupCb) {
#if SHARED_STATE_LOGGING
    LOG_DEVEL << this << ", " << __func__ << ": no wakeupCb. notifying one";
#endif
    guard.unlock();
    _cv.notify_one();
    return;
  }

  if (n > 0) {
#if SHARED_STATE_LOGGING
    LOG_DEVEL << this << ", " << __func__
              << ": n > 0 case, numWakeups to: " << _numWakeups;
#endif
    return;
  }

#if SHARED_STATE_LOGGING
  LOG_DEVEL << this << ", " << __func__
            << ": n == 0 case, numWakeups to: " << _numWakeups;
#endif
  queueHandler();
}

void SharedQueryState::queueHandler() {
  if (_numWakeups == 0 || !_wakeupCb || !_valid) {
#if SHARED_STATE_LOGGING
    LOG_DEVEL << this << ", " << __func__
              << ": exiting at initial if condition, numWakeups: "
              << _numWakeups << ", valid: " << _valid
              << ", wakeupCb: " << (_wakeupCb != nullptr);
#endif
    return;
  }

  if (ADB_UNLIKELY(_scheduler == nullptr)) {
    // We are shutting down
    return;
  }

  auto const lane = ServerState::instance()->isCoordinator()
                        ? RequestLane::CLUSTER_AQL_INTERNAL_COORDINATOR
                        : RequestLane::CLUSTER_AQL;

  bool queued =
      _scheduler->tryBoundedQueue(lane, [this, self = shared_from_this()]() {
        std::unique_lock<std::mutex> lck(self->_mutex);

        // check if we have a wakeup callback, and copy it under the
        // lock if we have one
        if (!_wakeupCb) {
#if SHARED_STATE_LOGGING
          LOG_DEVEL << this << ", " << __func__
                    << ": in scheduled lambda. no wakeupCb, numWakeups: "
                    << self->_numWakeups;
#endif
          return;
        }

        auto cb = _wakeupCb;
        // execute callback without holding the lock
        lck.unlock();

        [[maybe_unused]] bool cntn = false;
        try {
          cntn = cb();
#if SHARED_STATE_LOGGING
          LOG_DEVEL << this << ", " << __func__
                    << ": in scheduled lambda. executed cb, returned: " << cntn
                    << ", numWakeups: " << self->_numWakeups
                    << ", valid: " << self->_valid;
#endif
        } catch (...) {
        }

        // re-acquire the lock
        lck.lock();

        if (self->_numWakeups > 0) {
          --self->_numWakeups;
#if SHARED_STATE_LOGGING
          LOG_DEVEL << this << ", " << __func__
                    << ": valid, queueing handler, numWakeups: "
                    << self->_numWakeups;
#endif
          self->queueHandler();
        } else if (!_valid) {
#if SHARED_STATE_LOGGING
          LOG_DEVEL << this << ", " << __func__
                    << ": not valid, queueing handler, numWakeups: "
                    << self->_numWakeups;
#endif
          self->queueHandler();
        }
      });

  if (!queued) {
#if SHARED_STATE_LOGGING
    LOG_DEVEL << this << ", " << __func__
              << ": unable to queue handler, numWakeups: " << _numWakeups;
#endif
    // just invalidate
    _wakeupCb = nullptr;
    _valid = false;
    _cv.notify_all();
  }
}

bool SharedQueryState::queueAsyncTask(fu2::unique_function<void()> cb) {
  if (_scheduler) {
#if SHARED_STATE_LOGGING
    LOG_DEVEL << this << ", " << __func__ << ": numWakeups: " << _numWakeups;
#endif
    return _scheduler->tryBoundedQueue(RequestLane::CLUSTER_AQL, std::move(cb));
  }
  return false;
}

#ifdef ARANGODB_USE_GOOGLE_TESTS
bool SharedQueryState::noTasksRunning() const noexcept {
  return _numTasks.load() == 0;
}
#endif

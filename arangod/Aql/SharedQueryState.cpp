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

using namespace arangodb;
using namespace arangodb::aql;

SharedQueryState::SharedQueryState(ArangodServer& server)
    : SharedQueryState(server, SchedulerFeature::SCHEDULER) {}

SharedQueryState::SharedQueryState(ArangodServer& server, Scheduler* scheduler)
    : _server(server),
      _scheduler(scheduler),
      _wakeupCb(nullptr),
      _numWakeups(0),
      _cbVersion(0),
      _maxTasks(static_cast<unsigned>(
          _server.getFeature<QueryRegistryFeature>().maxParallelism())),
      _numTasks(0),
      _valid(true) {}

void SharedQueryState::invalidate() {
  {
    std::lock_guard<std::mutex> guard(_mutex);
    _wakeupCb = nullptr;
    _valid = false;
    ++_cbVersion;
  }
  _cv.notify_all();  // wakeup everyone else

  if (_numTasks.load() > 0) {
    std::unique_lock<std::mutex> guard(_mutex);
    _cv.wait(guard, [&] { return _numTasks.load() == 0; });
  }
}

bool SharedQueryState::consumeWakeup() {
  std::lock_guard<std::mutex> guard(_mutex);
  if (_numWakeups > 0) {
    --_numWakeups;
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
  _cv.wait(guard, [&] { return _numWakeups > 0 || !_valid; });
  TRI_ASSERT(_numWakeups > 0 || !_valid);
  if (_valid) {
    TRI_ASSERT(_numWakeups > 0);
    --_numWakeups;
  }
}

/// @brief setter for the continue handler:
///        We can either have a handler or a callback
void SharedQueryState::setWakeupHandler(std::function<bool()> const& cb) {
  std::lock_guard<std::mutex> guard(_mutex);
  _wakeupCb = cb;
  ++_cbVersion;
  // intentionally do not clobber _numWakeups here
}

void SharedQueryState::resetWakeupHandler() {
  std::lock_guard<std::mutex> guard(_mutex);
  _wakeupCb = nullptr;
  ++_cbVersion;
  // intentionally do not clobber _numWakeups here
}

/// execute the _continueCallback. must hold _mutex,
void SharedQueryState::notifyWaiter(std::unique_lock<std::mutex>& guard) {
  TRI_ASSERT(guard);
  if (!_valid) {
    guard.unlock();
    _cv.notify_all();
    return;
  }

  unsigned n = _numWakeups++;
  if (!_wakeupCb) {
    guard.unlock();
    _cv.notify_one();
    return;
  }

  if (n > 0) {
    return;
  }

  queueHandler();
}

void SharedQueryState::queueHandler() {
  if (_numWakeups == 0 || !_wakeupCb || !_valid) {
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
          return;
        }

        auto cb = _wakeupCb;
        // execute callback without holding the lock
        lck.unlock();

        bool cntn = false;
        try {
          cntn = cb();
        } catch (...) {
        }

        // re-acquire the lock
        lck.lock();

        if (!cntn || !self->_valid) {
          self->queueHandler();
        } else {
          unsigned c = self->_numWakeups--;
          TRI_ASSERT(c > 0);
          if (c == 1) {
            // we have consumed the last wakeup
            self->queueHandler();
          }
        }
      });

  if (!queued) {
    // just invalidate
    _wakeupCb = nullptr;
    _valid = false;
    _cv.notify_all();
  }
}

bool SharedQueryState::queueAsyncTask(fu2::unique_function<void()> cb) {
  if (_scheduler) {
    return _scheduler->tryBoundedQueue(RequestLane::CLUSTER_AQL, std::move(cb));
  }
  return false;
}

#ifdef ARANGODB_USE_GOOGLE_TESTS
bool SharedQueryState::noTasksRunning() const noexcept {
  return _numTasks.load() == 0;
}
#endif

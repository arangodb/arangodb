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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "SharedQueryState.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Exceptions.h"
#include "Cluster/ServerState.h"
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
      _wakeupCallback(nullptr),
      _numWakeups(0),
      _callbackVersion(0),
      _maxTasks(static_cast<unsigned>(
          _server.getFeature<QueryRegistryFeature>().maxParallelism())),
      _numTasks(0),
      _valid(true) {}

SharedQueryState::SharedQueryState(SharedQueryState const& other)
    : SharedQueryState(other._server, other._scheduler) {}

void SharedQueryState::invalidate() {
  {
    std::lock_guard<std::mutex> guard(_mutex);
    _wakeupCallback = nullptr;
    _callbackVersion++;
    _valid = false;
  }
  _cv.notify_all();  // wakeup everyone else

  if (_numTasks.load() > 0) {
    std::unique_lock<std::mutex> guard(_mutex);
    _cv.wait(guard, [&] { return _numTasks.load() == 0; });
  }
}

/// this has to stay for a backwards-compatible AQL HTTP API (hasMore).
void SharedQueryState::waitForAsyncWakeup() {
  std::unique_lock<std::mutex> guard(_mutex);
  if (!_valid) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_KILLED);
  }

  TRI_ASSERT(!_wakeupCallback);
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
  // whenever we update the wakeupCallback, we also have to increase the
  // callbackVersion and reset the numWakeups. Updating the callbackVersion is
  // necessary to ensure that wakeup handlers that are still queued realize that
  // they are no longer relevant (their associated RestHandler is already gone).
  // Resetting the numWakeups is necessary to ensure that later wakeups actually
  // schedule a new handler.
  _wakeupCallback = cb;
  _numWakeups = 0;
  _callbackVersion++;
}

void SharedQueryState::resetWakeupHandler() { setWakeupHandler(nullptr); }

/// execute the _continueCallback. must hold _mutex,
void SharedQueryState::notifyWaiter(std::unique_lock<std::mutex>& guard) {
  TRI_ASSERT(guard);
  if (!_valid) {
    guard.unlock();
    _cv.notify_all();
    return;
  }

  unsigned n = _numWakeups++;
  if (!_wakeupCallback) {
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
  if (_numWakeups == 0 || !_wakeupCallback || !_valid) {
    return;
  }

  if (ADB_UNLIKELY(_scheduler == nullptr)) {
    // We are shutting down
    return;
  }

  auto const lane = ServerState::instance()->isCoordinator()
                        ? RequestLane::CLUSTER_AQL_INTERNAL_COORDINATOR
                        : RequestLane::CLUSTER_AQL;

  // we capture the current values of wakeupCallback and callbackVersion at the
  // time we schedule the task. The callback has captured a shared_ptr to the
  // RestHandler, so it is always safe to call it. If the RestHandler has
  // already finished, the callback will simply do nothing and return
  // immediately. The callbackVersion allows us to realize that the captured
  // callback is no longer valid and simply return _without consuming a wakeup_.
  bool queued = _scheduler->tryBoundedQueue(
      lane, [self = shared_from_this(), cb = _wakeupCallback,
             version = _callbackVersion]() {
        std::unique_lock<std::mutex> lck(self->_mutex, std::defer_lock);

        do {
          bool cntn = false;
          try {
            cntn = cb();
          } catch (...) {
          }

          lck.lock();
          if (version == self->_callbackVersion) {
            unsigned c = self->_numWakeups--;
            TRI_ASSERT(c > 0);
            if (c == 1 || !cntn || !self->_valid) {
              break;
            }
          } else {
            return;
          }
          lck.unlock();
        } while (true);

        TRI_ASSERT(lck);
        self->queueHandler();
      });

  if (!queued) {  // just invalidate
    _wakeupCallback = nullptr;
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

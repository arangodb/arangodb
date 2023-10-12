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

#pragma once

#include <atomic>
#include <condition_variable>
#include <function2.hpp>

#include "RestServer/arangod.h"

#define SHARED_STATE_LOGGING 0
#include "Logger/LogMacros.h"

namespace arangodb {
class Scheduler;
namespace application_features {
class ApplicationServer;
}
namespace aql {

class SharedQueryState final
    : public std::enable_shared_from_this<SharedQueryState> {
 public:
  SharedQueryState(SharedQueryState const&) = delete;
  SharedQueryState& operator=(SharedQueryState const&) = delete;

  SharedQueryState(ArangodServer& server);
  SharedQueryState(ArangodServer& server, Scheduler* scheduler);
  SharedQueryState() = delete;
  ~SharedQueryState() = default;

  void invalidate();

  bool consumeWakeup();

  /// @brief executeAndWakeup is to be called on the query object to
  /// continue execution in this query part, if the query got paused
  /// because it is waiting for network responses. The idea is that a
  /// RemoteBlock that does an asynchronous cluster-internal request can
  /// register a callback with the asynchronous request and then return
  /// with the result `ExecutionState::WAITING`, which will bubble up
  /// the stack and eventually lead to a suspension of the work on the
  /// RestHandler. In the callback function one can first store the
  /// results in the RemoteBlock object and can then call this method on
  /// the query.
  /// This will lead to the following: The original request that led to
  /// the network communication will be rescheduled on the ioservice and
  /// continues its execution where it left off.
  template<typename F>
  void executeAndWakeup(F&& cb) {
    std::unique_lock<std::mutex> guard(_mutex);
    if (!_valid) {
#if SHARED_STATE_LOGGING
      LOG_DEVEL << this << ", " << __func__ << ", not valid";
#endif
      guard.unlock();
      _cv.notify_all();
      return;
    }

#if SHARED_STATE_LOGGING
    LOG_DEVEL << this << ", " << __func__ << ", executing cb";
#endif

    if (std::forward<F>(cb)()) {
      notifyWaiter(guard);
    }
  }

  template<typename F>
  void executeLocked(F&& cb) {
    std::unique_lock<std::mutex> guard(_mutex);
    if (!_valid) {
#if SHARED_STATE_LOGGING
      LOG_DEVEL << this << ", " << __func__ << ", not valid";
#endif
      guard.unlock();
      _cv.notify_all();
      return;
    }

#if SHARED_STATE_LOGGING
    LOG_DEVEL << this << ", " << __func__ << ", executing cb";
#endif
    std::forward<F>(cb)();
  }

  /// this has to stay for a backwards-compatible AQL HTTP API (hasMore).
  void waitForAsyncWakeup();

  /// @brief setter for the continue handler:
  ///        We can either have a handler or a callback
  void setWakeupHandler(std::function<bool()> const& cb);

  void resetWakeupHandler();

  /// execute a task in parallel if capacity is there
  template<typename F>
  bool asyncExecuteAndWakeup(F&& cb) {
    unsigned num = _numTasks.fetch_add(1);
#if SHARED_STATE_LOGGING
    LOG_DEVEL << this << ", " << __func__ << ", increased tasks to "
              << (num + 1);
#endif
    bool queued = false;
    if (num + 1 <= _maxTasks) {
#if SHARED_STATE_LOGGING
      LOG_DEVEL << this << ", " << __func__ << ", queueing async task";
#endif
      queued = queueAsyncTask([cb(std::forward<F>(cb)),
                               self = shared_from_this()] {
        if (self->_valid) {
          try {
#if SHARED_STATE_LOGGING
            LOG_DEVEL << this << ", " << __func__
                      << ", in async lambda, valid, executing callback";
#endif
            cb(true);
          } catch (...) {
            TRI_ASSERT(false);
          }
          std::unique_lock<std::mutex> guard(self->_mutex);
          auto v =
              self->_numTasks.fetch_sub(1);  // simon: intentionally under lock
          TRI_ASSERT(v > 0);
#if SHARED_STATE_LOGGING
          LOG_DEVEL << this << ", " << __func__
                    << ", in async lambda, valid, _numTasks: " << (v - 1)
                    << ", notifying waiter";
#endif
          self->notifyWaiter(guard);
        } else {  // need to wakeup everybody
          std::unique_lock<std::mutex> guard(self->_mutex);
          auto v =
              self->_numTasks.fetch_sub(1);  // simon: intentionally under lock
          TRI_ASSERT(v > 0);
#if SHARED_STATE_LOGGING
          LOG_DEVEL << this << ", " << __func__
                    << ", in async lambda, not valid, _numTasks: " << (v - 1)
                    << ", notifying all";
#endif
          guard.unlock();
          self->_cv.notify_all();
        }
      });
    }
    if (!queued) {
      // revert and execute directly
      auto v = _numTasks.fetch_sub(1);
      TRI_ASSERT(v > 0);
#if SHARED_STATE_LOGGING
      LOG_DEVEL
          << this << ", " << __func__
          << ", could not queue async task. executing cb directly, _numTasks: "
          << (v - 1);
#endif
      std::forward<F>(cb)(false);
    }
    return queued;
  }

#ifdef ARANGODB_USE_GOOGLE_TESTS
  bool noTasksRunning() const noexcept;
#endif

 private:
  /// execute the _continueCallback. must hold _mutex
  void notifyWaiter(std::unique_lock<std::mutex>& guard);
  void queueHandler();

  bool queueAsyncTask(fu2::unique_function<void()>);

  ArangodServer& _server;
  Scheduler* _scheduler;
  mutable std::mutex _mutex;
  std::condition_variable _cv;

  /// @brief a callback function which is used to implement continueAfterPause.
  /// Typically, the RestHandler using the Query object will put a closure
  /// in here, which continueAfterPause simply calls.
  std::function<bool()> _wakeupCb;

  unsigned _numWakeups;  // number of times

  unsigned const _maxTasks;
  std::atomic<unsigned> _numTasks;
  std::atomic<bool> _valid;
};

}  // namespace aql
}  // namespace arangodb

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

#pragma once

#include <atomic>
#include <condition_variable>
#include <function2.hpp>

#include "RestServer/arangod.h"

namespace arangodb {
class Scheduler;
namespace application_features {
class ApplicationServer;
}
namespace aql {

class SharedQueryState final
    : public std::enable_shared_from_this<SharedQueryState> {
 public:
  SharedQueryState(SharedQueryState const&);
  SharedQueryState& operator=(SharedQueryState const&) = delete;

  SharedQueryState(ArangodServer& server);
  SharedQueryState(ArangodServer& server, Scheduler* scheduler);
  SharedQueryState() = delete;
  ~SharedQueryState() = default;

  void setMaxTasks(unsigned maxTasks) { _maxTasks = maxTasks; }

  void invalidate();

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
      guard.unlock();
      _cv.notify_all();
      return;
    }

    if (std::forward<F>(cb)()) {
      notifyWaiter(guard);
    }
  }

  template<typename F>
  void executeLocked(F&& cb) {
    std::unique_lock<std::mutex> guard(_mutex);
    if (!_valid) {
      guard.unlock();
      _cv.notify_all();
      return;
    }
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
    // The atomic _numTasks counts the number of ongoing asynchronous
    // tasks. We need this for two purposes: One is to limit parallelism
    // so we need to know how many tasks we have already launched. But
    // Secondly, we want to wait for them to finish when the query is
    // shut down, in particular when it is killed or has run into an
    // exception. Note that this is *not* necessary for synchronous
    // tasks.
    // When _numTasks drops to 0, we need to wake up a thread which
    // is waiting for this on the condition variable _cv. We must
    // not miss this event, or else we might have a thread which is
    // waiting forever. The waiting thread uses a predicate to check
    // if _numTasks is 0, and only goes to sleep when it is not. This
    // happens under the mutex _mutex and releasing the mutex and going
    // to sleep is an atomic operation. Thus, to not miss the event that
    // _numTasks is reduced to zero, we must, whenever we decrement it,
    // do this under the mutex, and then, after releasing the mutex,
    // notify the condition variable _cv! Then either the decrement or
    // the going to sleep happens first (serialized by the mutex). If
    // the decrement happens first, the waiting thread is not even going
    // to sleep, if the going to sleep happens first, then we will wake
    // it up.
    unsigned num = _numTasks.fetch_add(1);
    if (num + 1 > _maxTasks) {
      // We first count down _numTasks to revert the counting up, since
      // we have not - after all - started a new async task. Then we run
      // the callback synchronously.
      std::unique_lock<std::mutex> guard(_mutex);
      _numTasks.fetch_sub(1);  // revert
      guard.unlock();
      _cv.notify_all();
      std::forward<F>(cb)(false);
      return false;
    }
    bool queued =
        queueAsyncTask([cb(std::forward<F>(cb)), self(shared_from_this())] {
          if (self->_valid) {
            bool triggerWakeUp = true;
            try {
              triggerWakeUp = cb(true);
            } catch (...) {
              TRI_ASSERT(false);
            }
            std::unique_lock<std::mutex> guard(self->_mutex);
            self->_numTasks.fetch_sub(1);  // simon: intentionally under lock
            if (triggerWakeUp) {
              self->notifyWaiter(guard);
            }
          } else {  // need to wakeup everybody
            std::unique_lock<std::mutex> guard(self->_mutex);
            self->_numTasks.fetch_sub(1);  // simon: intentionally under lock
            guard.unlock();
            self->_cv.notify_all();
          }
        });

    if (!queued) {
      // We first count down _numTasks to revert the counting up, since
      // we have not - after all - started a new async task. Then we run
      // the callback synchronously.
      std::unique_lock<std::mutex> guard(_mutex);
      _numTasks.fetch_sub(1);  // revert
      guard.unlock();
      _cv.notify_all();
      std::forward<F>(cb)(false);
    }
    return queued;
  }

#ifdef ARANGODB_USE_GOOGLE_TESTS
  bool noTasksRunning() const noexcept { return _numTasks.load() == 0; }
#endif

 private:
  /// execute the _continueCallback. must hold _mutex
  void notifyWaiter(std::unique_lock<std::mutex>& guard);
  void queueHandler();

  bool queueAsyncTask(fu2::unique_function<void()>);

 private:
  ArangodServer& _server;
  Scheduler* _scheduler;
  mutable std::mutex _mutex;
  std::condition_variable _cv;

  /// @brief a callback function which is used to implement continueAfterPause.
  /// Typically, the RestHandler using the Query object will put a closure
  /// in here, which continueAfterPause simply calls.
  std::function<bool()> _wakeupCallback;

  unsigned _numWakeups;  // number of times
  // The callbackVersion is used to identify callbacks that are associated with
  // a specific RestHandler. Every time a new callback is set, the version is
  // updated. That allows us to identify wakeups that are associated with a
  // callback who's RestHandler has already finished.
  unsigned _callbackVersion;

  unsigned _maxTasks;
  // Note that we are waiting for _numTasks to drop down to zero using
  // the condition Variable _cv above, which is protected by the mutex
  // _mutex above. Therefore, to avoid losing wakeups, it is necessary
  // to only ever reduce the value of _numTasks under the mutex and then
  // wake up the condition variable _cv!
  std::atomic<unsigned> _numTasks;
  std::atomic<bool> _valid;
};

}  // namespace aql
}  // namespace arangodb

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

#ifndef ARANGOD_AQL_SHARED_QUERY_STATE_H
#define ARANGOD_AQL_SHARED_QUERY_STATE_H 1

#include <atomic>
#include <condition_variable>
#include <functional>

namespace arangodb {
namespace aql {

class SharedQueryState final : public std::enable_shared_from_this<SharedQueryState> {
 public:
  SharedQueryState(SharedQueryState const&) = delete;
  SharedQueryState& operator=(SharedQueryState const&) = delete;

  SharedQueryState()
    : _wakeupCb(nullptr), _numWakeups(0), _cbVersion(0), _valid(true) {}

  ~SharedQueryState() = default;

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
  template <typename F>
  void executeAndWakeup(F&& cb) {
    std::lock_guard<std::mutex> guard(_mutex);
    if (!_valid) {
      _cv.notify_all();
      return;
    }

    if (std::forward<F>(cb)()) {
      execute();
    }
  }

  /// this has to stay for a backwards-compatible AQL HTTP API (hasMore).
  void waitForAsyncWakeup();

  /// @brief setter for the continue handler:
  ///        We can either have a handler or a callback
  void setWakeupHandler(std::function<bool()> const& cb);

  void resetWakeupHandler();

 private:
  /// execute the _continueCallback. must hold _mutex
  void execute();
  void queueHandler();

 private:
  mutable std::mutex _mutex;
  std::condition_variable _cv;

  /// @brief a callback function which is used to implement continueAfterPause.
  /// Typically, the RestHandler using the Query object will put a closure
  /// in here, which continueAfterPause simply calls.
  std::function<bool()> _wakeupCb;

  uint32_t _numWakeups;
  
  uint32_t _cbVersion;

  bool _valid;
};

}  // namespace aql
}  // namespace arangodb
#endif

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

#include "Basics/Common.h"
#include <condition_variable>

namespace arangodb {
namespace aql {

class SharedQueryState {
public:
  SharedQueryState(SharedQueryState const&) = delete;
  SharedQueryState& operator=(SharedQueryState const&) = delete;
  
  SharedQueryState()
  : _wasNotified(false),
  _hasHandler(false),
  _valid(true) {
  }
  
  ~SharedQueryState() = default;
  
  void invalidate();
  
  /// @brief continueAfterPause is to be called on the query object to
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
  bool execute(F&& cb) {
    std::lock_guard<std::mutex> guard(_mutex);
    if (!_valid) {
      return false;
    }
    
    bool res = std::forward<F>(cb)();
    if (_hasHandler) {
      if (ADB_UNLIKELY(!executeContinueCallback())) {
        return false; // likely shutting down
      }
    } else {
      _wasNotified = true;
      // simon: bad experience on macOS guard.unloack();
      _condition.notify_one();
    }
    
    return res;
  }
  
  /// this has to stay for a backwards-compatible AQL HTTP API (hasMore).
  void waitForAsyncResponse();
  
  /// @brief setter for the continue callback:
  ///        We can either have a handler or a callback
  void setContinueCallback() noexcept;
  
  /// @brief setter for the continue handler:
  ///        We can either have a handler or a callback
  void setContinueHandler(std::function<void()> const& handler);
  
private:
  
  /// execute the _continueCallback. must hold _mutex
  bool executeContinueCallback() const;
  
private:
  
  std::mutex _mutex;
  std::condition_variable _condition;
  
  /// @brief a callback function which is used to implement continueAfterPause.
  /// Typically, the RestHandler using the Query object will put a closure
  /// in here, which continueAfterPause simply calls.
  std::function<void()> _continueCallback;
  
  bool _wasNotified;
  
  /// @brief decide if the _continueCallback needs to be pushed onto the ioservice
  ///        or if it has to be executed in this thread.
  bool _hasHandler;
  
  bool _valid;
};
}}
#endif

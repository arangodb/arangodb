////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"

using namespace arangodb;
using namespace arangodb::aql;

void SharedQueryState::invalidate() {
  CONDITION_LOCKER(guard, _condition);
  _valid = false;
  _wasNotified = true;
  guard.signal();
}

/// this has to stay for a backwards-compatible AQL HTTP API (hasMore).
void SharedQueryState::waitForAsyncResponse() {
  CONDITION_LOCKER(guard, _condition);
  if (!_wasNotified) {
    _condition.wait();
  }
  _wasNotified = false;
}
  
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

bool SharedQueryState::execute(std::function<bool()> const& cb) {
  CONDITION_LOCKER(guard, _condition);
  if (!_valid) {
    return false;
  }

  bool res = cb();

  if (_hasHandler) {
    auto scheduler = SchedulerFeature::SCHEDULER;
    TRI_ASSERT(scheduler != nullptr);
    if (scheduler == nullptr) {
      // We are shutting down
      return false;
    }
    scheduler->post(_continueCallback, false);
  } else {
    _wasNotified = true;
    guard.signal();
  }

  return res;
}

/// @brief setter for the continue callback:
///        We can either have a handler or a callback
void SharedQueryState::setContinueCallback() noexcept {
  CONDITION_LOCKER(guard, _condition);
  _continueCallback = nullptr;
  _hasHandler = false;
}

/// @brief setter for the continue handler:
///        We can either have a handler or a callback
void SharedQueryState::setContinueHandler(std::function<void()> const& handler) {
  CONDITION_LOCKER(guard, _condition);
  _continueCallback = handler;
  _hasHandler = true;
}

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

#ifndef ARANGOD_AQL_SHARED_QUERY_STATE_H
#define ARANGOD_AQL_SHARED_QUERY_STATE_H 1

#include "Basics/Common.h"
#include "Basics/ConditionLocker.h"
#include "Basics/ConditionVariable.h"

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

  bool execute(std::function<bool()> const& cb);
  
  /// this has to stay for a backwards-compatible AQL HTTP API (hasMore).
  void waitForAsyncResponse();
  
  /// @brief setter for the continue callback:
  ///        We can either have a handler or a callback
  void setContinueCallback() noexcept;

  /// @brief setter for the continue handler:
  ///        We can either have a handler or a callback
  void setContinueHandler(std::function<void()> const& handler);

 private:
  basics::ConditionVariable _condition;
  
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

}
}

#endif

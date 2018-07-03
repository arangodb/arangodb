////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018-2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "WakeupQueryCallback.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/Query.h"
#include "Scheduler/SchedulerFeature.h"

using namespace arangodb;
using namespace arangodb::aql;

WakeupQueryCallback::WakeupQueryCallback(ExecutionBlock* initiator,
                                         Query* query)
    : _initiator(initiator), _query(query) {}

bool WakeupQueryCallback::operator()(ClusterCommResult* result) {
  TRI_ASSERT(_initiator != nullptr);
  TRI_ASSERT(_query != nullptr);
  // TODO Validate that _initiator and _query have not been deleted (ttl)
  // TODO Handle exceptions
  bool res = _initiator->handleAsyncResult(result);
  if (_query->hasHandler()) {
    auto scheduler = SchedulerFeature::SCHEDULER;
    TRI_ASSERT(scheduler != nullptr);
    if (scheduler == nullptr) {
      // We are shutting down
      return false;
    }
    scheduler->post(_query->continueHandler());
  } else {
    _query->continueAfterPause();
  }
  return res;
}

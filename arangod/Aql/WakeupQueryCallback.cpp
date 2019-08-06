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

using namespace arangodb;
using namespace arangodb::aql;

WakeupQueryCallback::WakeupQueryCallback(ExecutionBlock* initiator, Query* query)
    : _initiator(initiator), _query(query), _sharedState(query->sharedState()) {}

WakeupQueryCallback::~WakeupQueryCallback() {}

bool WakeupQueryCallback::operator()(ClusterCommResult* result) {
  return _sharedState->execute([result, this]() {
    TRI_ASSERT(_initiator != nullptr);
    TRI_ASSERT(_query != nullptr);
    // TODO Validate that _initiator and _query have not been deleted (ttl)
    // TODO Handle exceptions
    return _initiator->handleAsyncResult(result);
  });
}

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
/// @author Tobias Goedderz
/// @author Michael Hackstein
/// @author Heiko Kernbach
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_BLOCK_FETCHER_INTERFACES_H
#define ARANGOD_AQL_BLOCK_FETCHER_INTERFACES_H

#include "Aql/ExecutionState.h"

namespace arangodb {
namespace aql {

class AqlItemRow;

class SingleRowFetcher {
 public:
  SingleRowFetcher() = default;
  virtual ~SingleRowFetcher() = default;
  virtual std::pair<ExecutionState, AqlItemRow const*> fetchRow() = 0;
};

} // aql
} // arangodb
#endif

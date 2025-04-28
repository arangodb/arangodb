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
/// @author Michael Hackstein
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/QueryContext.h"

// This class serves as a wrapper around QueryContext to explicitly track where query killing
// is being used in the graph traversal code. It provides a single point of access to check
// if a query has been killed, making it easier to maintain and modify the query killing
// behavior if needed.
//
// While this adds a small layer of indirection, it helps with code clarity and maintainability.
// If profiling shows this wrapper causes significant overhead, we can remove it and use
// QueryContext directly.
//
// We can change this or discuss if this approach is not liked.

namespace arangodb::graph {

class QueryContextObserver {
 public:
  explicit QueryContextObserver(aql::QueryContext& query) : _query(query) {}
  
  [[nodiscard]] bool isKilled() const { return _query.killed(); }

 private:
  aql::QueryContext& _query;
};

}  // namespace arangodb::graph 
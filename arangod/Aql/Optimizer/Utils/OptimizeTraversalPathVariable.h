////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>

namespace arangodb::aql {
class TraversalNode;
struct Variable;

/// @brief checks if the path variable can be optimized away, or restricted
/// to some attributes (vertices, edges, weights)
bool optimizeTraversalPathVariable(
    Variable const* variable, TraversalNode* traversal,
    std::vector<Variable const*> const& pruneVars);

}  // namespace arangodb::aql

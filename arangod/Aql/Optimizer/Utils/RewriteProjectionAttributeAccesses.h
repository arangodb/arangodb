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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>

namespace arangodb::aql {
class ExecutionPlan;
class ExecutionNode;
struct Variable;
class Projections;

namespace optimizer {

// Allocate a fresh temporary variable for each projection that doesn't have
// one yet, and rewrite every reference to `searchVariable.attr` in the plan
// into a direct read of that variable. After this returns, the producer node
// `self` is responsible for filling the projection variables. Used by the
// optimizer rules that turn `doc.attr` accesses into output registers
// (optimizeProjections, materializeForEnumerateNear).
// The `index` parameter is forwarded to ExecutionNode::replaceAttributeAccess
// so callers like JoinNode can target a specific input slot; pass 0 for
// single-source nodes.
void rewriteProjectionAttributeAccesses(ExecutionPlan& plan,
                                        ExecutionNode* self,
                                        Variable const* searchVariable,
                                        Projections& projections,
                                        std::size_t index);

}  // namespace optimizer
}  // namespace arangodb::aql

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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/WalkerWorker.h"
#include "Aql/types.h"
#include "Containers/FlatHashMap.h"

namespace arangodb::aql {
struct AstNode;
class ExecutionPlan;

///
/// This code is inspired by TraversalConditionFinder, but tries to be more
/// transparent and simpler.
///
/// Walks the execution plan from leaf nodes upwards to find instances
/// of EnumeratePathsNodes whose path output variable `path` is filtered on
/// using a condition of the form
///
/// FOR p IN ENUMERATE_PATHS FROM v TO w GRAPH g
///   (?)
///   FILTER path.vertices[* ...] ALL == true
///   (?)
///   FILTER path.edges[* ...] ALL == true
///   (?)
///
struct EnumeratePathsFilterMatcher final
    : public WalkerWorker<ExecutionNode, WalkerUniqueness::NonUnique> {
 public:
  EnumeratePathsFilterMatcher(ExecutionPlan* plan);

  ~EnumeratePathsFilterMatcher() = default;

  bool before(ExecutionNode* node) override final;
  bool enterSubquery(ExecutionNode* node1, ExecutionNode* node2) override final;

  bool appliedChange() const noexcept { return _appliedChange; }

 private:
  ExecutionPlan* _plan;
  ::arangodb::containers::FlatHashMap<VariableId, AstNode const*>
      _filterConditions;
  ::arangodb::containers::FlatHashMap<VariableId, ExecutionNode*> _filterNodes;

  bool _appliedChange = false;
};

}  // namespace arangodb::aql

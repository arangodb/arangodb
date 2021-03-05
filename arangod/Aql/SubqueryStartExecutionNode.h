////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_SUBQUERY_START_EXECUTION_NODE_H
#define ARANGOD_AQL_SUBQUERY_START_EXECUTION_NODE_H 1

#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionNodeId.h"
#include "Aql/ExecutionPlan.h"

namespace arangodb {
namespace aql {

struct Variable;

class SubqueryStartNode : public ExecutionNode {
  friend class ExecutionNode;
  friend class ExecutionBlock;

 public:
  SubqueryStartNode(ExecutionPlan*, arangodb::velocypack::Slice const& base);
  SubqueryStartNode(ExecutionPlan* plan, ExecutionNodeId id, Variable const* subqueryOutVariable)
      : ExecutionNode(plan, id), _subqueryOutVariable(subqueryOutVariable) {}

  CostEstimate estimateCost() const override final;

  NodeType getType() const override final { return SUBQUERY_START; }

  void toVelocyPackHelper(arangodb::velocypack::Builder&, unsigned flags,
                          std::unordered_set<ExecutionNode const*>& seen) const override final;

  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine,
      std::unordered_map<ExecutionNode*, ExecutionBlock*> const&) const override;

  ExecutionNode* clone(ExecutionPlan* plan, bool withDependencies,
                       bool withProperties) const override final;

  bool isEqualTo(ExecutionNode const& other) const override final;

 private:
  /// @brief This is only required for Explain output.
  ///        it has no practical usage other then to print this information during explain.
  Variable const* _subqueryOutVariable;
};

}  // namespace aql
}  // namespace arangodb

#endif

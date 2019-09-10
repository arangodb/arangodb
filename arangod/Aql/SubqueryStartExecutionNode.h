////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2019 ArangoDB GmbH, Cologne, Germany
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
#include "Aql/ExecutionPlan.h"

namespace arangodb {
namespace aql {

class SubqueryStartNode : public ExecutionNode {
  friend class ExecutionNode;
  friend class ExecutionBlock;

 public:
  SubqueryStartNode(ExecutionPlan*, arangodb::velocypack::Slice const& base);
  SubqueryStartNode(ExecutionPlan* plan, size_t id)
      : ExecutionNode(plan, id), _subqueryDepth(0) {}

  /// @brief return the type of the node
  NodeType getType() const override final { return SUBQUERY_START; }

  /// @brief invalidate the cost estimate for the node and its dependencies
  void invalidateCost() override;

  /// @brief export to VelocyPack
  void toVelocyPackHelper(arangodb::velocypack::Builder&, unsigned flags) const override final;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine,
      std::unordered_map<ExecutionNode*, ExecutionBlock*> const&) const override;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan, bool withDependencies,
                       bool withProperties) const override final;

  /// @brief whether or not the subquery is a data-modification operation
  bool isModificationSubquery() const;

  /// @brief estimateCost
  CostEstimate estimateCost() const override final;

  bool isDeterministic() override final;

  bool isConst();
  bool mayAccessCollections();

 private:
  // TODO: needed?
  size_t _subqueryDepth;
};

} // namespace aql
} // namespace arangodb

#endif


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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNodeId.h"

#include <memory>

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

namespace aql {
class ExecutionBlock;
class ExecutionEngine;
class ExecutionNode;
class ExecutionPlan;
struct Variable;

/// @brief class SubqueryNode
/// From 3.8 onwards, SubqueryNodes are only used during query planning and
/// optimization, but will finally be replaced with SubqueryStartNode and
/// SubqueryEndNode nodes by the splice-subqueries optimizer rule.
class SubqueryNode : public ExecutionNode {
  friend class ExecutionNode;
  friend class ExecutionBlock;

 public:
  SubqueryNode(ExecutionPlan*, arangodb::velocypack::Slice base);

  SubqueryNode(ExecutionPlan* plan, ExecutionNodeId id, ExecutionNode* subquery,
               Variable const* outVariable);

  /// @brief return the type of the node
  NodeType getType() const override final;

  /// @brief return the amount of bytes used
  size_t getMemoryUsedBytes() const override final;

  /// @brief invalidate the cost estimate for the node and its dependencies
  void invalidateCost() override;

  /// @brief return the out variable
  Variable const* outVariable() const;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine) const override;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan,
                       bool withDependencies) const override final;

  /// @brief this is true iff the subquery contains a data-modification
  /// operation
  ///        NOTE that this is tested recursively, that is, if this subquery
  ///        contains a subquery that contains a modification operation, this is
  ///        true too.
  bool isModificationNode() const override;

  /// @brief getter for subquery
  ExecutionNode* getSubquery() const;

  /// @brief setter for subquery
  void setSubquery(ExecutionNode* subquery, bool forceOverwrite);

  /// @brief estimateCost
  CostEstimate estimateCost() const override final;

  /// @brief getVariablesUsedHere, modifying the set in-place
  void getVariablesUsedHere(VarSet& vars) const override final;

  /// @brief getVariablesSetHere
  std::vector<Variable const*> getVariablesSetHere() const override final;

  /// @brief replace the out variable, so we can adjust the name.
  void replaceOutVariable(Variable const* var);

  bool isDeterministic() override final;

  bool isConst();
  bool mayAccessCollections();

 protected:
  /// @brief export to VelocyPack
  void doToVelocyPack(arangodb::velocypack::Builder&,
                      unsigned flags) const override final;

 private:
  /// @brief we need to have an expression and where to write the result
  ExecutionNode* _subquery;

  /// @brief variable to write to
  Variable const* _outVariable;
};

}  // namespace aql
}  // namespace arangodb

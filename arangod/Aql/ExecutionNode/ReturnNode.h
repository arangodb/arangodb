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
class Expression;
struct Variable;

/// @brief class ReturnNode
class ReturnNode : public ExecutionNode {
  friend class ExecutionBlock;

  /// @brief constructors for various arguments, always with offset and limit
 public:
  ReturnNode(ExecutionPlan* plan, ExecutionNodeId id,
             Variable const* inVariable);

  ReturnNode(ExecutionPlan*, arangodb::velocypack::Slice base);

  /// @brief return the type of the node
  NodeType getType() const override final;

  /// @brief return the amount of bytes used
  size_t getMemoryUsedBytes() const override final;

  /// @brief tell the node to count the returned values
  void setCount();

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine) const override;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan,
                       bool withDependencies) const override final;

  /// @brief estimateCost
  CostEstimate estimateCost() const override final;

  void replaceVariables(std::unordered_map<VariableId, Variable const*> const&
                            replacements) override;

  /// @brief getVariablesUsedHere, modifying the set in-place
  void getVariablesUsedHere(VarSet& vars) const override final;

  AsyncPrefetchEligibility canUseAsyncPrefetching()
      const noexcept override final;

  Variable const* inVariable() const;

  void inVariable(Variable const* v);

  bool returnInheritedResults() const;

 protected:
  /// @brief export to VelocyPack
  void doToVelocyPack(arangodb::velocypack::Builder&,
                      unsigned flags) const override final;

 private:
  /// @brief the variable produced by Return
  Variable const* _inVariable;

  bool _count;
};

}  // namespace aql
}  // namespace arangodb

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

class Index;

namespace aql {
class ExecutionBlock;
class ExecutionEngine;
class ExecutionNode;
class ExecutionPlan;
class Expression;
struct Variable;

/// @brief class CalculationNode
class CalculationNode : public ExecutionNode {
  friend class ExecutionNode;
  friend class ExecutionBlock;

 public:
  CalculationNode(ExecutionPlan* plan, ExecutionNodeId id,
                  std::unique_ptr<Expression> expr,
                  Variable const* outVariable);

  CalculationNode(ExecutionPlan*, arangodb::velocypack::Slice base);

  ~CalculationNode();

  /// @brief return the type of the node
  NodeType getType() const override final;

  /// @brief return the amount of bytes used
  size_t getMemoryUsedBytes() const override final;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine) const override;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan,
                       bool withDependencies) const override final;

  /// @brief return out variable
  Variable const* outVariable() const;

  /// @brief return the expression. never a nullptr!
  Expression* expression() const;

  /// @brief estimateCost
  CostEstimate estimateCost() const override final;

  AsyncPrefetchEligibility canUseAsyncPrefetching()
      const noexcept override final;

  void replaceVariables(std::unordered_map<VariableId, Variable const*> const&
                            replacements) override;

  /// @brief replaces an attribute access in the internals of the execution
  /// node with a simple variable access
  void replaceAttributeAccess(ExecutionNode const* self,
                              Variable const* searchVariable,
                              std::span<std::string_view> attribute,
                              Variable const* replaceVariable,
                              size_t index) override;

  /// @brief getVariablesUsedHere, modifying the set in-place
  void getVariablesUsedHere(VarSet& vars) const override final;

  /// @brief getVariablesSetHere
  virtual std::vector<Variable const*> getVariablesSetHere()
      const override final;

  bool isDeterministic() override final;

 protected:
  /// @brief export to VelocyPack
  void doToVelocyPack(arangodb::velocypack::Builder&,
                      unsigned flags) const override final;

 private:
  /// @brief output variable to write to
  Variable const* _outVariable;

  /// @brief we need to have an expression and where to write the result
  std::unique_ptr<Expression> _expression;
};

}  // namespace aql
}  // namespace arangodb

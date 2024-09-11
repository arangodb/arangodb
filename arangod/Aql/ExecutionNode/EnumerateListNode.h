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

/// @brief class EnumerateListNode
class EnumerateListNode : public ExecutionNode {
  friend class ExecutionNode;
  friend class ExecutionBlock;

 public:
  EnumerateListNode(ExecutionPlan* plan, ExecutionNodeId id,
                    Variable const* inVariable, Variable const* outVariable);

  EnumerateListNode(ExecutionPlan*, arangodb::velocypack::Slice base);

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

  /// @brief the cost of an enumerate list node
  CostEstimate estimateCost() const override final;

  AsyncPrefetchEligibility canUseAsyncPrefetching()
      const noexcept override final;

  void replaceVariables(std::unordered_map<VariableId, Variable const*> const&
                            replacements) override;

  void replaceAttributeAccess(ExecutionNode const* self,
                              Variable const* searchVariable,
                              std::span<std::string_view> attribute,
                              Variable const* replaceVariable,
                              size_t index) override;

  /// @brief remember the condition to execute for early filtering
  void setFilter(std::unique_ptr<Expression> filter);

  /// @brief return the early pruning condition for the node
  Expression* filter() const noexcept { return _filter.get(); }

  /// @brief whether or not the node has an early pruning filter condition
  bool hasFilter() const noexcept { return _filter != nullptr; }

  /// @brief getVariablesUsedHere, modifying the set in-place
  void getVariablesUsedHere(VarSet& vars) const override final;

  /// @brief getVariablesSetHere
  std::vector<Variable const*> getVariablesSetHere() const override final;

  /// @brief return in variable
  Variable const* inVariable() const;

  /// @brief return out variable
  std::vector<Variable const*> outVariable() const;

  enum Mode {
    kEnumerateArray,   /// @brief yield entries of an array
    kEnumerateObject,  /// @brief yield key-value-pairs of an object
  };

  Mode getMode() const noexcept { return _mode; }

  void setEnumerateObject(Variable const* key, Variable const* value) noexcept;

 protected:
  /// @brief export to VelocyPack
  void doToVelocyPack(arangodb::velocypack::Builder&,
                      unsigned flags) const override final;

 private:
  /// @brief input variable to read from
  Variable const* _inVariable;

  /// @brief output variable to write to
  Variable const* _outVariable;

  /// @brief early filtering condition
  std::unique_ptr<Expression> _filter;

  /// @brief enumeration mode
  Mode _mode = kEnumerateArray;

  /// @brief variables used for yielding key-value pairs
  Variable const* _keyValuePairOutVars[2] = {nullptr, nullptr};
};

}  // namespace aql
}  // namespace arangodb

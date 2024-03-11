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

#include <string_view>

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

namespace materialize {
class MaterializeNode : public ExecutionNode {
 protected:
  MaterializeNode(ExecutionPlan* plan, ExecutionNodeId id,
                  aql::Variable const& inDocId,
                  aql::Variable const& outVariable,
                  aql::Variable const& oldDocVariable);

  MaterializeNode(ExecutionPlan* plan, arangodb::velocypack::Slice base);

 public:
  static constexpr std::string_view kMaterializeNodeMultiNodeParam =
      "multiNode";

  /// @brief return the type of the node
  NodeType getType() const override final { return ExecutionNode::MATERIALIZE; }

  /// @brief return the amount of bytes used
  size_t getMemoryUsedBytes() const override final;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine) const override = 0;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan,
                       bool withDependencies) const override = 0;

  CostEstimate estimateCost() const override final;

  /// @brief getVariablesUsedHere, modifying the set in-place
  void getVariablesUsedHere(VarSet& vars) const override;

  /// @brief getVariablesSetHere
  std::vector<Variable const*> getVariablesSetHere() const override;

  /// @brief return out variable
  aql::Variable const& outVariable() const noexcept { return *_outVariable; }

  aql::Variable const& docIdVariable() const noexcept {
    return *_inNonMaterializedDocId;
  }

  aql::Variable const& oldDocVariable() const noexcept {
    return *_oldDocVariable;
  }

  void setDocOutVariable(Variable const& var) noexcept { _outVariable = &var; }

 protected:
  /// @brief export to VelocyPack
  void doToVelocyPack(arangodb::velocypack::Builder& nodes,
                      unsigned flags) const override;

  /// @brief input variable non-materialized document ids
  aql::Variable const* _inNonMaterializedDocId;

  /// @brief the variable produced by materialization
  Variable const* _outVariable;

  /// @brief old document variable that is materialized
  Variable const* _oldDocVariable;
};

MaterializeNode* createMaterializeNode(ExecutionPlan* plan,
                                       arangodb::velocypack::Slice base);

}  // namespace materialize
}  // namespace aql
}  // namespace arangodb

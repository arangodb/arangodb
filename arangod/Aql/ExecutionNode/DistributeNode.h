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

#include "Aql/ExecutionNode/CollectionAccessingNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/ScatterNode.h"
#include "Aql/ExecutionNodeId.h"
#include "Aql/types.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <memory>
#include <vector>

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack
namespace aql {
class ExecutionBlock;
class ExecutionEngine;
class ExecutionPlan;
struct Collection;
struct Variable;

/// @brief class DistributeNode
class DistributeNode final : public ScatterNode,
                             public CollectionAccessingNode {
  friend class ExecutionBlock;

  /// @brief constructor with an id
 public:
  DistributeNode(ExecutionPlan* plan, ExecutionNodeId id,
                 ScatterNode::ScatterType type, Collection const* collection,
                 Variable const* variable, ExecutionNodeId targetNodeId);

  DistributeNode(ExecutionPlan*, arangodb::velocypack::Slice const& base);

  /// @brief return the type of the node
  NodeType getType() const override final { return DISTRIBUTE; }

  /// @brief return the amount of bytes used
  size_t getMemoryUsedBytes() const override final;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine) const override;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan,
                       bool withDependencies) const override final;

  void replaceVariables(std::unordered_map<VariableId, Variable const*> const&
                            replacements) override;

  /// @brief getVariablesUsedHere, modifying the set in-place
  void getVariablesUsedHere(VarSet& vars) const override final;

  /// @brief estimateCost
  CostEstimate estimateCost() const override final;

  Variable const* getVariable() const noexcept { return _variable; }

  void setVariable(Variable const* var);

  ExecutionNodeId getTargetNodeId() const noexcept { return _targetNodeId; }

  void addSatellite(aql::Collection*);

  std::vector<aql::Collection*> const getSatellites() const noexcept {
    return _satellites;
  }

 protected:
  /// @brief export to VelocyPack
  void doToVelocyPack(arangodb::velocypack::Builder&,
                      unsigned flags) const override final;

 private:
  std::vector<std::string> determineProjectionAttribute() const;

  /// @brief the variable we must inspect to know where to distribute
  Variable const* _variable;

  std::vector<std::string> _attribute;

  /// @brief the id of the target ExecutionNode this DistributeNode belongs to.
  ExecutionNodeId _targetNodeId;

  /// @brief List of Satellite collections this node needs to distribute data to
  /// in a satellite manner.
  std::vector<aql::Collection*> _satellites;
};

}  // namespace aql
}  // namespace arangodb

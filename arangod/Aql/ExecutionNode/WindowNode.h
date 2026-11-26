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

#include "Aql/AqlValue.h"
#include "Aql/CollectOptions.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/WindowBounds.h"
#include "Aql/ExecutionNodeId.h"
#include "Basics/datetime.h"

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace arangodb {
namespace velocypack {
class Slice;
}
namespace aql {

struct Aggregator;
class ExecutionBlock;
class ExecutionPlan;
class QueryWarnings;

/// @brief class WindowNode
class WindowNode : public ExecutionNode {
  friend class ExecutionNode;
  friend class ExecutionBlock;

 public:
  WindowNode(ExecutionPlan* plan, ExecutionNodeId id, WindowBounds&& b,
             Variable const* rangeVariable,
             std::vector<AggregateVarInfo> const& aggregateVariables);

  WindowNode(ExecutionPlan*, arangodb::velocypack::Slice base, WindowBounds&& b,
             Variable const* rangeVariable,
             std::vector<AggregateVarInfo> const& aggregateVariables);

  ~WindowNode() override;

  /// @brief return the type of the node
  NodeType getType() const override final;

  /// @brief return the amount of bytes used
  size_t getMemoryUsedBytes() const override final;

  /// @brief calculate the aggregate registers
  void calcAggregateRegisters(
      std::vector<std::pair<RegisterId, RegisterId>>& aggregateRegisters,
      RegIdSet& readableInputRegisters,
      RegIdSet& writeableOutputRegisters) const;

  void calcAggregateTypes(
      std::vector<std::unique_ptr<Aggregator>>& aggregateTypes,
      ResourceMonitor& resourceMonitor) const;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine) const override;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan,
                       bool withDependencies) const override final;

  /// @brief estimateCost
  CostEstimate estimateCost() const override final;

  void setAggregateVariables(
      std::vector<AggregateVarInfo> const& aggregateVariables);

  void replaceVariables(std::unordered_map<VariableId, Variable const*> const&
                            replacements) override;

  /// @brief getVariablesUsedHere, modifying the set in-place
  void getVariablesUsedHere(VarSet& vars) const override final;

  /// @brief getVariablesSetHere
  std::vector<Variable const*> getVariablesSetHere() const override final;

  // does this WINDOW need to look at rows following the current one
  bool needsFollowingRows() const;

 protected:
  /// @brief export to VelocyPack
  void doToVelocyPack(arangodb::velocypack::Builder&,
                      unsigned flags) const override final;

 private:
  WindowBounds _bounds;

  Variable const* _rangeVariable;

  /// @brief input/output variables for the aggregation (out, in)
  std::vector<AggregateVarInfo> _aggregateVariables;
};

}  // namespace aql
}  // namespace arangodb

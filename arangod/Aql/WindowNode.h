////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_WINDOW_NODE_H
#define ARANGOD_AQL_WINDOW_NODE_H 1

#include "Aql/AqlValue.h"
#include "Aql/CollectOptions.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionNodeId.h"

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
class ExecutionBlock;
class ExecutionPlan;
struct Aggregator;

struct WindowRange final {
  ~WindowRange();

 public:
  WindowRange();

 public:
  AqlValue preceding;
  AqlValue following;

 public:
  void toVelocyPack(velocypack::Builder& options) const;
  void fromVelocyPack(velocypack::Slice slice);
};

/// @brief class CollectNode
class WindowNode : public ExecutionNode {
  friend class ExecutionNode;
  friend class ExecutionBlock;
  friend class RedundantCalculationsReplacer;

 public:
  WindowNode(ExecutionPlan* plan, ExecutionNodeId id,
             WindowRange&& options, Variable const* rangeVariable,
             std::vector<AggregateVarInfo> const& aggregateVariables);

  WindowNode(ExecutionPlan*, arangodb::velocypack::Slice const& base,
             WindowRange&& options, Variable const* rangeVariable,
             std::vector<AggregateVarInfo> const& aggregateVariables);

  ~WindowNode() override;

  /// @brief return the type of the node
  NodeType getType() const final;

  /// @brief export to VelocyPack
  void toVelocyPackHelper(arangodb::velocypack::Builder&, unsigned flags,
                          std::unordered_set<ExecutionNode const*>& seen) const final;

  /// @brief calculate the aggregate registers
  void calcAggregateRegisters(std::vector<std::pair<RegisterId, RegisterId>>& aggregateRegisters,
                              RegIdSet& readableInputRegisters,
                              RegIdSet& writeableOutputRegisters) const;

  void calcAggregateTypes(std::vector<std::unique_ptr<Aggregator>>& aggregateTypes) const;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine,
      std::unordered_map<ExecutionNode*, ExecutionBlock*> const&) const override;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan, bool withDependencies,
                       bool withProperties) const final;

  /// @brief estimateCost
  CostEstimate estimateCost() const final;

  void setAggregateVariables(std::vector<AggregateVarInfo> const& aggregateVariables);

  /// @brief clear one of the aggregates
  void clearAggregates(std::function<bool(AggregateVarInfo const&)> cb);

  /// @brief get all aggregate variables (out, in)
  std::vector<AggregateVarInfo> const& aggregateVariables() const;

  /// @brief get all aggregate variables (out, in)
  std::vector<AggregateVarInfo>& aggregateVariables();

  /// @brief getVariablesUsedHere, modifying the set in-place
  void getVariablesUsedHere(VarSet& vars) const final;

  /// @brief getVariablesSetHere
  std::vector<Variable const*> getVariablesSetHere() const final;

  // does this WINDOW need to look at rows following the current one
  bool needsFollowingRows() const;

 private:
  WindowRange _range;

  Variable const* _rangeVariable;

  /// @brief input/output variables for the aggregation (out, in)
  std::vector<AggregateVarInfo> _aggregateVariables;
};

}  // namespace aql
}  // namespace arangodb

#endif

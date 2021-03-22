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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_WINDOW_NODE_H
#define ARANGOD_AQL_WINDOW_NODE_H 1

#include "Aql/AqlValue.h"
#include "Aql/CollectOptions.h"
#include "Aql/ExecutionNode.h"
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

/// utility class to calculate window bounds for Row / Range based windows
/// could probably also be part of the executor
class WindowBounds final {
 public:
  enum class Type { Row, Range };
  /// range based WINDOW row values
  struct Row {
    double value;
    double lowBound;
    double highBound;
    bool valid;
  };

  WindowBounds(Type type,
               AqlValue&& preceding,
               AqlValue&& following);
  WindowBounds(Type type, velocypack::Slice slice);
  ~WindowBounds();

 public:

  int64_t numPrecedingRows() const;
  int64_t numFollowingRows() const;

  bool needsFollowingRows() const;

  Row calcRow(AqlValue const& input, QueryWarnings& q) const;

 public:
  bool unboundedPreceding() const;
  void toVelocyPack(velocypack::Builder& options) const;

 private:
  enum class RangeType { Numeric, Date };
  const Type _type;

  int64_t _numPrecedingRows = 0;
  int64_t _numFollowingRows = 0;
  RangeType _rangeType = RangeType::Numeric;

  // contains calendar aware year + month
  arangodb::basics::ParsedDuration _precedingDuration;
  arangodb::basics::ParsedDuration _followingDuration;

  double _precedingNumber = 0.0;
  double _followingNumber = 0.0;
};

/// @brief class WindowNode
class WindowNode : public ExecutionNode {
  friend class ExecutionNode;
  friend class ExecutionBlock;
  friend class RedundantCalculationsReplacer;  // TODO: remove

 public:
  WindowNode(ExecutionPlan* plan, ExecutionNodeId id, WindowBounds&& b,
             Variable const* rangeVariable,
             std::vector<AggregateVarInfo> const& aggregateVariables);

  WindowNode(ExecutionPlan*, arangodb::velocypack::Slice const& base,
             WindowBounds&& b, Variable const* rangeVariable,
             std::vector<AggregateVarInfo> const& aggregateVariables);

  ~WindowNode() override;

  /// @brief return the type of the node
  NodeType getType() const override final;

  /// @brief export to VelocyPack
  void toVelocyPackHelper(arangodb::velocypack::Builder&, unsigned flags,
                          std::unordered_set<ExecutionNode const*>& seen) const override final;

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
                       bool withProperties) const override final;

  /// @brief estimateCost
  CostEstimate estimateCost() const override final;

  void setAggregateVariables(std::vector<AggregateVarInfo> const& aggregateVariables);

  /// @brief getVariablesUsedHere, modifying the set in-place
  void getVariablesUsedHere(VarSet& vars) const override final;

  /// @brief getVariablesSetHere
  std::vector<Variable const*> getVariablesSetHere() const override final;

  // does this WINDOW need to look at rows following the current one
  bool needsFollowingRows() const;

 private:
  WindowBounds _bounds;

  Variable const* _rangeVariable;

  /// @brief input/output variables for the aggregation (out, in)
  std::vector<AggregateVarInfo> _aggregateVariables;
};

}  // namespace aql
}  // namespace arangodb

#endif

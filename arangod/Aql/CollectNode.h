////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_COLLECT_NODE_H
#define ARANGOD_AQL_COLLECT_NODE_H 1

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
class RedundantCalculationsReplacer;
struct Aggregator;

/// @brief class CollectNode
class CollectNode : public ExecutionNode {
  friend class ExecutionNode;
  friend class ExecutionBlock;
  friend class RedundantCalculationsReplacer;

 public:
  CollectNode(ExecutionPlan* plan, ExecutionNodeId id, CollectOptions const& options,
              std::vector<std::pair<Variable const*, Variable const*>> const& groupVariables,
              std::vector<std::pair<Variable const*, std::pair<Variable const*, std::string>>> const& aggregateVariables,
              Variable const* expressionVariable, Variable const* outVariable,
              std::vector<Variable const*> const& keepVariables,
              std::unordered_map<VariableId, std::string const> const& variableMap,
              bool count, bool isDistinctCommand);

  CollectNode(ExecutionPlan*, arangodb::velocypack::Slice const& base,
              Variable const* expressionVariable, Variable const* outVariable,
              std::vector<Variable const*> const& keepVariables,
              std::unordered_map<VariableId, std::string const> const& variableMap,
              std::vector<std::pair<Variable const*, Variable const*>> const& collectVariables,
              std::vector<std::pair<Variable const*, std::pair<Variable const*, std::string>>> const& aggregateVariables,
              bool count, bool isDistinctCommand);

  ~CollectNode() override;

  /// @brief return the type of the node
  NodeType getType() const final;

  /// @brief whether or not the node requires an additional post SORT
  bool isDistinctCommand() const;

  /// @brief whether or not the node is specialized
  bool isSpecialized() const;

  /// @brief specialize the node
  void specialized();

  /// @brief return the aggregation method
  CollectOptions::CollectMethod aggregationMethod() const;

  /// @brief set the aggregation method
  void aggregationMethod(CollectOptions::CollectMethod method);

  /// @brief getOptions
  CollectOptions& getOptions();

  /// @brief export to VelocyPack
  void toVelocyPackHelper(arangodb::velocypack::Builder&, unsigned flags,
                          std::unordered_set<ExecutionNode const*>& seen) const final;

  /// @brief calculate the expression register
  void calcExpressionRegister(RegisterId& expressionRegister,
                              RegIdSet& writeableOutputRegisters) const;

  /// @brief calculate the collect register
  void calcCollectRegister(RegisterId& collectRegister,
                           RegIdSet& writeableOutputRegisters) const;

  /// @brief calculate the group registers
  void calcGroupRegisters(std::vector<std::pair<RegisterId, RegisterId>>& groupRegisters,
                          RegIdSet& readableInputRegisters,
                          RegIdSet& writeableOutputRegisters) const;

  /// @brief calculate the aggregate registers
  void calcAggregateRegisters(std::vector<std::pair<RegisterId, RegisterId>>& aggregateRegisters,
                              RegIdSet& readableInputRegisters,
                              RegIdSet& writeableOutputRegisters) const;

  void calcAggregateTypes(std::vector<std::unique_ptr<Aggregator>>& aggregateTypes) const;

  std::vector<std::pair<std::string, RegisterId>> calcInputVariableNames() const;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine,
      std::unordered_map<ExecutionNode*, ExecutionBlock*> const&) const override;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan, bool withDependencies,
                       bool withProperties) const final;

  /// @brief estimateCost
  CostEstimate estimateCost() const final;

  /// @brief whether or not the count flag is set
  bool count() const;
  /// @brief set the count option
  void count(bool value);

  bool hasOutVariableButNoCount() const;

  /// @brief whether or not the node has an outVariable (i.e. INTO ...)
  bool hasOutVariable() const;

  /// @brief return the out variable
  Variable const* outVariable() const;

  /// @brief clear the out variable
  void clearOutVariable();

  /// @brief clear all keep variables
  void clearKeepVariables();

  void setAggregateVariables(
      std::vector<std::pair<Variable const*, std::pair<Variable const*, std::string>>> const& aggregateVariables);

  /// @brief clear one of the aggregates
  void clearAggregates(
      std::function<bool(std::pair<Variable const*, std::pair<Variable const*, std::string>> const&)> cb);

  /// @brief whether or not the node has an expression variable (i.e. INTO ...
  /// = expr)
  bool hasExpressionVariable() const;

  /// @brief set the expression variable
  void expressionVariable(Variable const* variable);

  /// @brief return whether or not the collect has keep variables
  bool hasKeepVariables() const;

  /// @brief return the keep variables
  std::vector<Variable const*> const& keepVariables() const;

  /// @brief restrict the KEEP variables (which may also be the auto-collected
  /// variables of an unrestricted `INTO var`) to the passed `variables`.
  void restrictKeepVariables(std::unordered_set<const Variable*> const& variables);

  /// @brief return the variable map
  std::unordered_map<VariableId, std::string const> const& variableMap() const;

  /// @brief get all group variables (out, in)
  std::vector<std::pair<Variable const*, Variable const*>> const& groupVariables() const;

  /// @brief set all group variables (out, in)
  void groupVariables(std::vector<std::pair<Variable const*, Variable const*>> const& vars);

  /// @brief get all aggregate variables (out, in)
  std::vector<std::pair<Variable const*, std::pair<Variable const*, std::string>>> const& aggregateVariables() const;

  /// @brief get all aggregate variables (out, in)
  std::vector<std::pair<Variable const*, std::pair<Variable const*, std::string>>>& aggregateVariables();

  /// @brief getVariablesUsedHere, modifying the set in-place
  void getVariablesUsedHere(VarSet& vars) const final;

  /// @brief getVariablesSetHere
  std::vector<Variable const*> getVariablesSetHere() const final;

  static void calculateAccessibleUserVariables(ExecutionNode const& node,
                                               std::vector<Variable const*>& userVariables);

 private:
  /// @brief options for the aggregation
  CollectOptions _options;

  /// @brief input/output variables for the collection (out, in)
  std::vector<std::pair<Variable const*, Variable const*>> _groupVariables;

  /// @brief input/output variables for the aggregation (out, in)
  std::vector<std::pair<Variable const*, std::pair<Variable const*, std::string>>> _aggregateVariables;

  /// @brief input expression variable (might be null)
  Variable const* _expressionVariable;

  /// @brief output variable to write to (might be null)
  Variable const* _outVariable;

  /// @brief list of variables to keep if INTO is used
  std::vector<Variable const*> _keepVariables;

  /// @brief map of all variable ids and names (needed to construct group data)
  std::unordered_map<VariableId, std::string const> _variableMap;

  /// @brief COUNTing node?
  bool _count;

  /// @brief whether or not the node requires an additional post-SORT
  bool const _isDistinctCommand;

  /// @brief whether or not the node is specialized
  bool _specialized;
};

}  // namespace aql
}  // namespace arangodb

#endif

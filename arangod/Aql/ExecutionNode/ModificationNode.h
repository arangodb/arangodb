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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/ExecutionNode/CollectionAccessingNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNodeId.h"
#include "Aql/ModificationOptions.h"
#include "Aql/types.h"

namespace arangodb::aql {
struct Collection;
class ExecutionBlock;
class ExecutionPlan;
struct Variable;

/// @brief abstract base class for modification operations
class ModificationNode : public ExecutionNode, public CollectionAccessingNode {
  friend class ExecutionBlock;

  /// @brief constructor with a vocbase and a collection and options
 protected:
  ModificationNode(ExecutionPlan* plan, ExecutionNodeId id,
                   Collection const* collection,
                   ModificationOptions const& options,
                   Variable const* outVariableOld,
                   Variable const* outVariableNew)
      : ExecutionNode(plan, id),
        CollectionAccessingNode(collection),
        _options(options),
        _outVariableOld(outVariableOld),
        _outVariableNew(outVariableNew),
        _countStats(true),
        _producesResults(true) {}

  ModificationNode(ExecutionPlan*, arangodb::velocypack::Slice const& slice);

 public:
  /// @brief estimateCost
  /// Note that all the modifying nodes use this estimateCost method which is
  /// why we can make it final here.
  CostEstimate estimateCost() const override final;

  AsyncPrefetchEligibility canUseAsyncPrefetching()
      const noexcept override final;

  /// @brief data modification is non-deterministic
  bool isDeterministic() override final { return false; }

  /// @brief getOptions
  ModificationOptions const& getOptions() const { return _options; }

  /// @brief getOptions
  ModificationOptions& getOptions() { return _options; }

  /// @brief getVariablesSetHere
  std::vector<Variable const*> getVariablesSetHere() const override final;

  /// @brief return the "$OLD" out variable
  Variable const* getOutVariableOld() const { return _outVariableOld; }

  /// @brief return the "$NEW" out variable
  Variable const* getOutVariableNew() const { return _outVariableNew; }

  /// @brief clear the "$OLD" out variable
  void clearOutVariableOld() { _outVariableOld = nullptr; }

  /// @brief clear the "$NEW" out variable
  void clearOutVariableNew() { _outVariableNew = nullptr; }

  /// @brief set the "$OLD" out variable
  void setOutVariableOld(Variable const* oldVar) { _outVariableOld = oldVar; }

  /// @brief set the "$NEW" out variable
  void setOutVariableNew(Variable const* newVar) { _outVariableNew = newVar; }

  /// @brief whether or not the node produces results
  /// this is normally turned on unless an optimizer rule
  /// explicitly turns this off as a performance optimization
  bool producesResults() const noexcept { return _producesResults; }

  /// @brief whether or not the node produces results
  void producesResults(bool value) noexcept { _producesResults = value; }

  /// @brief whether or not the node is a data modification node
  bool isModificationNode() const override { return true; }

  /// @brief whether this node contributes to statistics. Only disabled in
  /// SmartGraph case
  bool countStats() const noexcept { return _countStats; }

  /// @brief Disable that this node is contributing to statistics. Only disabled
  /// in SmartGraph case
  void disableStatistics() noexcept { _countStats = false; }

 protected:
  /// @brief export to VelocyPack
  void doToVelocyPack(arangodb::velocypack::Builder&,
                      unsigned flags) const override;

  void cloneCommon(ModificationNode*) const;

  /// @brief modification operation options
  ModificationOptions _options;

  /// @brief output variable ($OLD)
  Variable const* _outVariableOld;

  /// @brief output variable ($NEW)
  Variable const* _outVariableNew;

  /// @brief whether this node contributes to statistics. Only disabled in
  /// SmartGraph case
  bool _countStats;

  /// @brief whether this node will pass through results from block above
  bool _producesResults;
};

}  // namespace arangodb::aql

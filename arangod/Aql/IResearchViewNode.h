////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_IRESEARCH__IRESEARCH_VIEW_NODE_H
#define ARANGOD_IRESEARCH__IRESEARCH_VIEW_NODE_H 1

#include "Aql/Condition.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionNodeId.h"
#include "Aql/LateMaterializedOptimizerRulesCommon.h"
#include "Aql/types.h"
#include "IResearch/IResearchOrderFactory.h"
#include "IResearch/IResearchViewSort.h"
#include "IResearch/IResearchViewStoredValues.h"
#include "types.h"
#include "utils/bit_utils.hpp"

namespace arangodb {
class LogicalView;

namespace aql {
struct Collection;
class ExecutionNode;
class ExecutionBlock;
class ExecutionEngine;
template<typename T> struct RegisterPlanT;
using RegisterPlan = RegisterPlanT<ExecutionNode>;
struct VarInfo;
}  // namespace aql

namespace iresearch {

bool filterConditionIsEmpty(aql::AstNode const* filterCondition) noexcept;

enum class MaterializeType {
  Undefined = 0,        // an undefined initial value
  NotMaterialize = 1,   // do not materialize a document
  LateMaterialize = 2,  // a document will be materialized later
  Materialize = 4,      // materialize a document
  UseStoredValues = 8   // use stored or sort column values
};

enum class CountApproximate {
  Exact = 0,  // skipAll should return exact number of records
  Cost = 1,   // iterator cost could be used as skipAllCount
};

ENABLE_BITMASK_ENUM(MaterializeType);

/// @brief class EnumerateViewNode
class IResearchViewNode final : public arangodb::aql::ExecutionNode {
  friend class arangodb::aql::RedundantCalculationsReplacer;

 public:
  /// @brief node options
  struct Options {
    /// @brief a list of data source CIDs to restrict a query
    ::arangodb::containers::HashSet<TRI_voc_cid_t> sources;

    /// @brief use the list of sources to restrict a query
    bool restrictSources{false};

    /// @brief sync view before querying to get the latest index snapshot
    bool forceSync{false};

    /// @brief try not to materialize documents
    bool noMaterialization{true};

    /// @brief condition optimization Auto - condition will be transformed to DNF.
    arangodb::aql::ConditionOptimization conditionOptimization{
        arangodb::aql::ConditionOptimization::Auto};

    /// @brief skipAll method for view
    CountApproximate countApproximate{CountApproximate::Exact};
  };  // Options

  IResearchViewNode(aql::ExecutionPlan& plan, aql::ExecutionNodeId id, TRI_vocbase_t& vocbase,
                    std::shared_ptr<const arangodb::LogicalView> const& view,
                    aql::Variable const& outVariable, aql::AstNode* filterCondition,
                    aql::AstNode* options, std::vector<Scorer>&& scorers);

  IResearchViewNode(aql::ExecutionPlan&, velocypack::Slice const& base);

  /// @brief return the type of the node
  NodeType getType() const override final { return ENUMERATE_IRESEARCH_VIEW; }

  /// @brief export to VelocyPack
  void toVelocyPackHelper(arangodb::velocypack::Builder&, unsigned,
                          std::unordered_set<ExecutionNode const*>& seen) const override final;

  /// @brief clone ExecutionNode recursively
  aql::ExecutionNode* clone(aql::ExecutionPlan* plan, bool withDependencies,
                            bool withProperties) const override final;

  /// @returns the list of the linked collections
  std::vector<std::reference_wrapper<aql::Collection const>> collections() const;

  /// @returns true if underlying view has no links
  bool empty() const noexcept;

  /// @brief the cost of an enumerate view node
  aql::CostEstimate estimateCost() const override final;

  /// @brief getVariablesSetHere
  std::vector<arangodb::aql::Variable const*> getVariablesSetHere() const override final;

  /// @brief return out variable
  arangodb::aql::Variable const& outVariable() const noexcept {
    return *_outVariable;
  }

  /// @brief return the database
  TRI_vocbase_t& vocbase() const noexcept { return _vocbase; }

  /// @brief return the view
  std::shared_ptr<const arangodb::LogicalView> const& view() const noexcept {
    return _view;
  }

  /// @brief return the filter condition to pass to the view
  arangodb::aql::AstNode const& filterCondition() const noexcept {
    TRI_ASSERT(_filterCondition);
    return *_filterCondition;
  }

  /// @brief set the filter condition to pass to the view
  void filterCondition(aql::AstNode const* node) noexcept;

  /// @brief return list of shards related to the view (cluster only)
  std::vector<std::string> const& shards() const noexcept { return _shards; }

  /// @brief return list of shards related to the view (cluster only)
  std::vector<std::string>& shards() noexcept { return _shards; }

  /// @brief return the scorers to pass to the view
  std::vector<Scorer> const& scorers() const noexcept { return _scorers; }

  /// @brief set the scorers to pass to the view
  void scorers(std::vector<Scorer>&& scorers) noexcept {
    _scorers = std::move(scorers);
  }

  /// @return sort condition satisfied by a sorted index
  std::pair<IResearchViewSort const*, size_t> const& sort() const noexcept {
    return _sort;
  }

  /// @brief set sort condition satisfied by a sorted index
  void sort(IResearchViewSort const* sort, size_t size) noexcept {
    _sort.first = sort;
    _sort.second = sort ? std::min(size, sort->size()) : 0;
  }

  /// @brief getVariablesUsedHere, modifying the set in-place
  void getVariablesUsedHere(aql::VarSet& vars) const override final;

  /// @brief returns IResearchViewNode options
  Options const& options() const noexcept { return _options; }

  /// @brief node volatility, determines how often query has
  ///        to be rebuilt during the execution
  /// @note first - node has nondeterministic/dependent (inside a loop)
  ///       filter condition
  ///       second - node has nondeterministic/dependent (inside a loop)
  ///       sort condition
  std::pair<bool, bool> volatility(bool force = false) const;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<aql::ExecutionBlock> createBlock(
      aql::ExecutionEngine& engine,
      std::unordered_map<aql::ExecutionNode*, aql::ExecutionBlock*> const&) const override;

  aql::RegIdSet calcInputRegs() const;

  bool isLateMaterialized() const noexcept {
    return _outNonMaterializedDocId != nullptr && _outNonMaterializedColPtr != nullptr;
  }

  void setLateMaterialized(aql::Variable const& colPtrVariable,
                           aql::Variable const& docIdVariable) noexcept {
    _outNonMaterializedDocId = &docIdVariable;
    _outNonMaterializedColPtr = &colPtrVariable;
  }

  bool noMaterialization() const noexcept { return _noMaterialization; }

  void setNoMaterialization() noexcept { _noMaterialization = true; }

  static constexpr ptrdiff_t SortColumnNumber {-1};

  // A variable with a field number in a column
  struct ViewVariable {
    size_t fieldNum;
    aql::Variable const* var;
  };

  // A variable with column and field numbers
  struct ViewVariableWithColumn : ViewVariable {
    ptrdiff_t columnNum;
  };

  using ViewValuesVars = std::unordered_map<ptrdiff_t, std::vector<ViewVariable>>;

  using ViewValuesRegisters = std::map<ptrdiff_t, std::map<size_t, aql::RegisterId>>;

  using ViewVarsInfo =
      std::unordered_map<std::vector<arangodb::basics::AttributeName> const*, ViewVariableWithColumn>;

  void setViewVariables(ViewVarsInfo const& viewVariables) {
    _outNonMaterializedViewVars.clear();
    for (auto const& viewVars : viewVariables) {
      _outNonMaterializedViewVars[viewVars.second.columnNum].emplace_back(
          ViewVariable{viewVars.second.fieldNum, viewVars.second.var});
    }
  }

  // The class is used for temporary saving of optimization rule data.
  // It contains document references that could be replaced in late
  // materialization and no materialization rules.
  class OptimizationState {
    using ViewVarsToBeReplaced = std::vector<aql::latematerialized::AstAndColumnFieldData>;

    /// @brief calculation node with ast nodes that can be replaced by view values (e.g. primary sort)
    std::unordered_map<aql::CalculationNode*, ViewVarsToBeReplaced> _nodesToChange;

    /// @brief is no document materialization possible
    bool _noDocMaterStatus = true;

   public:
    void saveCalcNodesForViewVariables(
        std::vector<aql::latematerialized::NodeWithAttrsColumn> const& nodesToChange);

    bool canVariablesBeReplaced(aql::CalculationNode* calclulationNode) const;

    ViewVarsInfo replaceViewVariables(std::vector<aql::CalculationNode*> const& calcNodes,
                                      arangodb::containers::HashSet<ExecutionNode*>& toUnlink);

    ViewVarsInfo replaceAllViewVariables(arangodb::containers::HashSet<ExecutionNode*>& toUnlink);

    void clearViewVariables() noexcept { _nodesToChange.clear(); }

    bool isNoDocumentMaterializationPossible() const noexcept {
      return _noDocMaterStatus;
    }

    void disableNoDocumentMaterialization() noexcept {
      _noDocMaterStatus = false;
    }
  };

  OptimizationState& state() noexcept { return _optState; }


 private:
  /// @brief the database
  TRI_vocbase_t& _vocbase;

  /// @brief view
  /// @note need shared_ptr to ensure view validity
  std::shared_ptr<const LogicalView> _view;

  /// @brief output variable to write to
  aql::Variable const* _outVariable;

  // Following two variables should be set in pairs.
  // Info is split between 2 registers to allow constructing
  // AqlValue with type VPACK_INLINE, which is much faster (no allocations!).
  // CollectionPtr is needed for materialization step -
  // as view could return documents from different collections.
  // We store raw ptr to collection as materialization is expected to happen
  // on same server (it is ensured by optimizer rule as network hop is expensive!)
  /// @brief output variable to write only non-materialized document ids
  aql::Variable const* _outNonMaterializedDocId;
  /// @brief output variable to write only non-materialized collection ids
  aql::Variable const* _outNonMaterializedColPtr;

  /// @brief output variables to non-materialized document view sort references
  ViewValuesVars _outNonMaterializedViewVars;

  /// @brief is no materialization should be applied
  bool _noMaterialization;

  OptimizationState _optState;

  /// @brief filter node to pass to the view
  aql::AstNode const* _filterCondition;

  /// @brief sort condition covered by the view
  /// first - sort condition
  /// second - number of sort buckets to use
  std::pair<IResearchViewSort const*, size_t> _sort{};

  /// @brief scorers related to the view
  std::vector<Scorer> _scorers;

  /// @brief list of shards involved, need this for the cluster
  std::vector<std::string> _shards;

  /// @brief volatility mask
  mutable int _volatilityMask{-1};

  /// @brief IResearchViewNode options
  Options _options;
};  // IResearchViewNode

}  // namespace iresearch
}  // namespace arangodb

#endif  // ARANGOD_IRESEARCH__ENUMERATE_VIEW_NODE_H

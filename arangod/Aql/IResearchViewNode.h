////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/Condition.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionNodeId.h"
#include "Aql/LateMaterializedOptimizerRulesCommon.h"
#include "Aql/types.h"
#include "Containers/FlatHashSet.h"
#include "Containers/FlatHashMap.h"
#include "Containers/SmallVector.h"
#include "IResearch/IResearchFilterOptimization.h"
#include "IResearch/IResearchOrderFactory.h"
#include "IResearch/IResearchViewSort.h"
#include "IResearch/IResearchViewStoredValues.h"
#include "VocBase/LogicalView.h"

#include "types.h"

#include "utils/bit_utils.hpp"

#include <unordered_map>

namespace arangodb {
class LogicalView;
namespace aql {
struct Collection;
class ExecutionNode;
class ExecutionBlock;
class ExecutionEngine;
template<typename T>
struct RegisterPlanT;
using RegisterPlan = RegisterPlanT<ExecutionNode>;
struct VarInfo;
}  // namespace aql
namespace iresearch {

bool isFilterConditionEmpty(aql::AstNode const* filterCondition) noexcept;

enum class MaterializeType {
  Undefined = 0,        // an undefined initial value
  NotMaterialize = 1,   // do not materialize a document
  LateMaterialize = 2,  // a document will be materialized later
  Materialize = 4,      // materialize a document
  UseStoredValues = 8   // use stored or sort column values
};

ENABLE_BITMASK_ENUM(MaterializeType);

enum class CountApproximate {
  Exact = 0,  // skipAll should return exact number of records
  Cost = 1,   // iterator cost could be used as skipAllCount
};

/// @brief class EnumerateViewNode
class IResearchViewNode final : public aql::ExecutionNode {
 public:
  /// @brief node options
  struct Options {
    /// @brief a list of data source CIDs to restrict a query
    containers::FlatHashSet<DataSourceId> sources;

    /// @brief condition optimization Auto - condition will be transformed to
    /// DNF.
    aql::ConditionOptimization conditionOptimization{
        aql::ConditionOptimization::Auto};

    /// @brief skipAll method for view
    CountApproximate countApproximate{CountApproximate::Exact};

    /// @brief iresearch filters optimization level
    FilterOptimization filterOptimization{FilterOptimization::MAX};

    /// @brief use the list of sources to restrict a query
    bool restrictSources{false};

    /// @brief sync view before querying to get the latest index snapshot
    bool forceSync{false};

    /// @brief try not to materialize documents
    bool noMaterialization{true};
  };

  IResearchViewNode(aql::ExecutionPlan& plan, aql::ExecutionNodeId id,
                    TRI_vocbase_t& vocbase,
                    std::shared_ptr<const LogicalView> view,
                    aql::Variable const& outVariable,
                    aql::AstNode* filterCondition, aql::AstNode* options,
                    std::vector<Scorer>&& scorers);

  IResearchViewNode(aql::ExecutionPlan&, velocypack::Slice base);

  /// @brief return the type of the node
  NodeType getType() const final { return ENUMERATE_IRESEARCH_VIEW; }

  /// @brief clone ExecutionNode recursively
  aql::ExecutionNode* clone(aql::ExecutionPlan* plan, bool withDependencies,
                            bool withProperties) const final;

  using Collections =
      std::vector<std::pair<std::reference_wrapper<aql::Collection const>,
                            LogicalView::Indexes>>;
  /// @returns the list of the linked collections
  Collections collections() const;

  /// @returns true if underlying view has no links
  bool empty() const noexcept;

  /// @brief the cost of an enumerate view node
  aql::CostEstimate estimateCost() const final;

  // TODO(MBkkt) Use containers::FlatHashMap
  void replaceVariables(
      std::unordered_map<aql::VariableId, aql::Variable const*> const&
          replacements) final;

  /// @brief getVariablesSetHere
  std::vector<aql::Variable const*> getVariablesSetHere() const final;

  /// @brief return out variable
  aql::Variable const& outVariable() const noexcept { return *_outVariable; }

  /// @brief return the database
  TRI_vocbase_t& vocbase() const noexcept { return _vocbase; }

  /// @brief return the view
  auto const& view() const noexcept { return _view; }

  /// @brief return the filter condition to pass to the view
  auto const& filterCondition() const noexcept { return *_filterCondition; }

  /// @brief set the filter condition to pass to the view
  void setFilterCondition(aql::AstNode const* node) noexcept;

  // we could merge if it is allowed in general and there are no scores - as
  // changing filters will affect score and we will lose backward compatibility
  FilterOptimization filterOptimization() const noexcept {
    if (!_scorers.empty()) {
      return FilterOptimization::NONE;
    }
    return _options.filterOptimization;
  }

  /// @brief return list of shards related to the view (cluster only)
  auto const& shards() const noexcept { return _shards; }

  /// @brief return list of shards related to the view (cluster only)
  auto& shards() noexcept { return _shards; }

  /// @brief return the scorers to pass to the view
  auto const& scorers() const noexcept { return _scorers; }

  /// @brief set the scorers to pass to the view
  void setScorers(std::vector<Scorer>&& scorers) noexcept {
    _scorers = std::move(scorers);
  }

  /// @return sort condition satisfied by a sorted index
  std::pair<iresearch::IResearchSortBase const*, size_t> sort() const noexcept {
    return {_sort.get(), _sortBuckets};
  }

  /// @brief set sort condition satisfied by a sorted index
  void setSort(IResearchSortBase const& sort, size_t buckets) noexcept {
    _sortBuckets = std::min(buckets, sort.size());
    _sort = std::shared_ptr<IResearchSortBase const>(
        &sort, [](IResearchSortBase const*) noexcept {});
  }

  /// @brief getVariablesUsedHere, modifying the set in-place
  void getVariablesUsedHere(aql::VarSet& vars) const final;

  /// @brief returns IResearchViewNode options
  Options const& options() const noexcept { return _options; }

  /// @brief node volatility, determines how often query has
  ///  to be rebuilt during the execution
  /// @return pair:
  ///  first - node has nondeterministic/dependent (inside a loop)
  ///   filter condition
  ///  second - node has nondeterministic/dependent (inside a loop)
  ///   sort condition
  std::pair<bool, bool> volatility(bool force = false) const;

  void setScorersSort(std::vector<std::pair<size_t, bool>>&& sort,
                      size_t limit) {
    _scorersSort = std::move(sort);
    _scorersSortLimit = limit;
  }

#ifdef ARANGODB_USE_GOOGLE_TESTS
  size_t getScorersSortLimit() const noexcept { return _scorersSortLimit; }

  auto getScorersSort() const noexcept { return std::span(_scorersSort); }
#endif

  /// @brief creates corresponding ExecutionBlock
  // TODO(MBkkt) Use containers::FlatHashMap
  std::unique_ptr<aql::ExecutionBlock> createBlock(
      aql::ExecutionEngine& engine,
      std::unordered_map<aql::ExecutionNode*, aql::ExecutionBlock*> const&)
      const final;

  aql::RegIdSet calcInputRegs() const;

  bool isLateMaterialized() const noexcept {
    return _outNonMaterializedDocId != nullptr &&
           _outNonMaterializedColPtr != nullptr;
  }

  void setLateMaterialized(aql::Variable const& colPtrVariable,
                           aql::Variable const& docIdVariable) noexcept {
    _outNonMaterializedDocId = &docIdVariable;
    _outNonMaterializedColPtr = &colPtrVariable;
  }

  bool isNoMaterialization() const noexcept { return _noMaterialization; }

  void setNoMaterialization() noexcept { _noMaterialization = true; }

  static constexpr ptrdiff_t kSortColumnNumber{-1};

  // A variable with a field number in a column
  struct ViewVariable {
    size_t fieldNum{0};
    aql::Variable const* var{nullptr};
  };

  // A variable with column and field numbers
  struct ViewVariableWithColumn : ViewVariable {
    ptrdiff_t columnNum{0};
  };

  using ViewValuesVars =
      containers::FlatHashMap<ptrdiff_t, std::vector<ViewVariable>>;

  using ViewValuesRegisters =
      std::map<ptrdiff_t, std::map<size_t, aql::RegisterId>>;

  using ViewVarsInfo =
      containers::FlatHashMap<std::vector<basics::AttributeName> const*,
                              ViewVariableWithColumn>;

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
    using ViewVarsToBeReplaced =
        std::vector<aql::latematerialized::AstAndColumnFieldData>;

    /// @brief calculation node with ast nodes that can be replaced by view
    /// values (e.g. primary sort)
    containers::FlatHashMap<aql::CalculationNode*, ViewVarsToBeReplaced>
        _nodesToChange;

    /// @brief is no document materialization possible
    bool _noDocMaterStatus{true};

   public:
    void saveCalcNodesForViewVariables(
        std::vector<aql::latematerialized::NodeWithAttrsColumn> const&
            nodesToChange);

    bool canVariablesBeReplaced(aql::CalculationNode* calculationNode) const;

    ViewVarsInfo replaceViewVariables(
        std::vector<aql::CalculationNode*> const& calcNodes,
        containers::HashSet<ExecutionNode*>& toUnlink);

    ViewVarsInfo replaceAllViewVariables(
        containers::HashSet<ExecutionNode*>& toUnlink);

    bool isNoDocumentMaterializationPossible() const noexcept {
      return _noDocMaterStatus;
    }

    void disableNoDocumentMaterialization() noexcept {
      _noDocMaterStatus = false;
    }
  };

  OptimizationState& state() noexcept { return _optState; }

 private:
  /// @brief export to VelocyPack
  void doToVelocyPack(velocypack::Builder&, unsigned) const final;

  /// @brief the database
  TRI_vocbase_t& _vocbase;

  /// @brief view
  /// @note need shared_ptr to ensure view validity
  std::shared_ptr<LogicalView const> _view;

  /// @brief output variable to write to
  aql::Variable const* _outVariable{nullptr};

  // Following two variables should be set in pairs.
  // Info is split between 2 registers to allow constructing
  // AqlValue with type VPACK_INLINE, which is much faster (no allocations!).
  // CollectionPtr is needed for materialization step -
  // as view could return documents from different collections.
  // We store raw ptr to collection as materialization is expected to happen
  // on same server (it is ensured by optimizer rule as network hop is
  // expensive!)
  /// @brief output variable to write only non-materialized document ids
  aql::Variable const* _outNonMaterializedDocId{nullptr};
  /// @brief output variable to write only non-materialized collection ids
  aql::Variable const* _outNonMaterializedColPtr{nullptr};

  /// @brief output variables to non-materialized document view sort references
  ViewValuesVars _outNonMaterializedViewVars;

  OptimizationState _optState;

  /// @brief filter node to pass to the view
  aql::AstNode const* _filterCondition{nullptr};

  /// @brief sort condition covered by the view
  /// _sort - sort condition
  /// _sortBuckets - number of sort buckets to use
  std::shared_ptr<IResearchSortBase const> _sort;
  size_t _sortBuckets{0};

  /// @brief stored values covered by the view
  std::shared_ptr<IResearchViewStoredValues const> _storedValues;

  /// @brief scorers related to the view
  std::vector<Scorer> _scorers;

  /// @brief list of shards involved, need this for the cluster
  containers::FlatHashMap<std::string, LogicalView::Indexes> _shards;

  /// @brief IResearchViewNode options
  Options _options;

  /// @brief internal order for scorers
  std::vector<std::pair<size_t, bool>> _scorersSort;
  size_t _scorersSortLimit{0};

  /// @brief volatility mask
  mutable int _volatilityMask{-1};

  /// @brief is no materialization should be applied
  bool _noMaterialization{false};
};

}  // namespace iresearch
}  // namespace arangodb

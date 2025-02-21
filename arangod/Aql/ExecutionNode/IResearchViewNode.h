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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/Condition.h"
#include "Aql/ExecutionNode/DataAccessingNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNodeId.h"
#include "Aql/Optimizer/Rule/OptimizerRulesLateMaterializedCommon.h"
#include "Aql/types.h"
#include "Cluster/Utils/ShardID.h"
#include "Containers/FlatHashMap.h"
#include "Containers/FlatHashSet.h"
#include "IResearch/IResearchFilterOptimization.h"
#include "IResearch/IResearchOrderFactory.h"
#include "IResearch/IResearchViewSort.h"
#include "IResearch/IResearchViewStoredValues.h"
#include "VocBase/LogicalView.h"

#include "utils/bit_utils.hpp"

#include <function2.hpp>

#include <span>
#include <string_view>
#include <unordered_map>

namespace arangodb {
class LogicalView;
namespace aql {
struct Collection;
class ExecutionBlock;
class ExecutionEngine;
template<typename T>
struct RegisterPlanT;
using RegisterPlan = RegisterPlanT<ExecutionNode>;
struct VarInfo;

using FieldRegisters = std::map<size_t, RegisterId>;
}  // namespace aql
namespace iresearch {

class SearchMeta;

bool isFilterConditionEmpty(aql::AstNode const* filterCondition) noexcept;

/// @returns true if a given node is located inside a loop or subquery
bool isInInnerLoopOrSubquery(aql::ExecutionNode const& node);

// in loop or non-deterministic
bool hasDependencies(aql::ExecutionPlan const& plan, aql::AstNode const& node,
                     aql::Variable const& ref, aql::VarSet& vars,
                     // TODO(MBkkt) fu2::function_view
                     std::function<bool(aql::Variable const*)> callback);

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

struct HeapSortElement {
  bool isScore() const noexcept {
    // if fieldNumber is max then it is scored idx.
    // Stored Column idx otherwise.
    return fieldNumber == std::numeric_limits<size_t>::max();
  }

#ifdef ARANGODB_USE_GOOGLE_TESTS
  auto operator<=>(HeapSortElement const&) const noexcept = default;
#endif

  std::string postfix;
  ptrdiff_t source{0};
  size_t fieldNumber{std::numeric_limits<size_t>::max()};
  bool ascending{true};
};

class IResearchViewNode final : public aql::ExecutionNode,
                                public aql::DataAccessingNode {
 public:
  // Node options
  struct Options {
    // List of data source CIDs to restrict a query
    containers::FlatHashSet<DataSourceId> sources;

    // Condition optimization
    //  Auto - condition will be transformed to DNF.
    aql::ConditionOptimization conditionOptimization{
        aql::ConditionOptimization::kAuto};

    // `skipAll` method for view
    CountApproximate countApproximate{CountApproximate::Exact};

    // iresearch filters optimization level
    FilterOptimization filterOptimization{FilterOptimization::MAX};

    // max number of threads to process segments in parallel.
    size_t parallelism{1};

    // Use the list of sources to restrict a query.
    bool restrictSources{false};

    // Sync view before querying to get the latest index snapshot.
    bool forceSync{false};

    // Try not to materialize documents.
    bool noMaterialization{true};
  };

  static IResearchViewNode* getByVar(aql::ExecutionPlan const& plan,
                                     aql::Variable const& var) noexcept;

  IResearchViewNode(aql::ExecutionPlan& plan, aql::ExecutionNodeId id,
                    TRI_vocbase_t& vocbase,
                    std::shared_ptr<const LogicalView> view,
                    aql::Variable const& outVariable,
                    aql::AstNode* filterCondition, aql::AstNode* options,
                    std::vector<SearchFunc>&& scorers);

  IResearchViewNode(aql::ExecutionPlan&, velocypack::Slice base);

  // Return the type of the node.
  NodeType getType() const final { return ENUMERATE_IRESEARCH_VIEW; }

  /// @brief return the amount of bytes used
  size_t getMemoryUsedBytes() const override final;

  // Clone ExecutionNode recursively.
  aql::ExecutionNode* clone(aql::ExecutionPlan* plan,
                            bool withDependencies) const final;

  using Collections =
      std::vector<std::pair<std::reference_wrapper<aql::Collection const>,
                            LogicalView::Indexes>>;

  aql::Collection const* collection() const final;
  bool isUsedAsSatellite() const final;
  void useAsSatelliteOf(aql::ExecutionNodeId) final;
  aql::Collection const* prototypeCollection() const final;
  void setPrototype(arangodb::aql::Collection const* prototypeCollection,
                    arangodb::aql::Variable const* prototypeOutVariable) final;

  // Returns the list of the linked collections.
  Collections collections() const;

  // Returns true if underlying view has no links.
  bool empty() const noexcept;

  // The cost of an enumerate view node.
  aql::CostEstimate estimateCost() const final;

  aql::AsyncPrefetchEligibility canUseAsyncPrefetching()
      const noexcept override final;

  // TODO(MBkkt) Use containers::FlatHashMap
  void replaceVariables(
      std::unordered_map<aql::VariableId, aql::Variable const*> const&
          replacements) final;

  void replaceAttributeAccess(aql::ExecutionNode const* self,
                              aql::Variable const* searchVariable,
                              std::span<std::string_view> attribute,
                              aql::Variable const* replaceVariable,
                              size_t index) override;

  std::vector<aql::Variable const*> getVariablesSetHere() const final;

  // Return out variable.
  aql::Variable const& outVariable() const noexcept { return *_outVariable; }

  // Return the database.
  TRI_vocbase_t& vocbase() const noexcept { return _vocbase; }

  // Return the view.
  auto const& view() const noexcept { return _view; }

  // Return the meta.
  auto const& meta() const noexcept { return _meta; }

  // Return the filter condition to pass to the view.
  auto const& filterCondition() const noexcept { return *_filterCondition; }

  // Set the filter condition to pass to the view.
  void setFilterCondition(aql::AstNode const* node) noexcept;

  // we could merge if it is allowed in general and there are no scores - as
  // changing filters will affect score and we will lose backward compatibility
  FilterOptimization filterOptimization() const noexcept {
    if (!_scorers.empty()) {
      return FilterOptimization::NONE;
    }
    return _options.filterOptimization;
  }

  // Return list of shards related to the view (cluster only).
  auto const& shards() const noexcept { return _shards; }

  // Return list of shards related to the view (cluster only).
  auto& shards() noexcept { return _shards; }

  // Return the scorers to pass to the view.
  auto& scorers() noexcept { return _scorers; }
  auto const& scorers() const noexcept { return _scorers; }

  // Return current snapshot key
  void const* getSnapshotKey() const noexcept;

  // Set the scorers to pass to the view.
  void setScorers(std::vector<SearchFunc>&& scorers) noexcept {
    _scorers = std::move(scorers);
  }

  // Return sort condition satisfied by a sorted index.
  std::pair<iresearch::IResearchSortBase const*, size_t> sort() const noexcept {
    return {_sort, _sortBuckets};
  }

  // Set sort condition satisfied by a sorted index.
  void setSort(IResearchSortBase const& sort, size_t buckets) noexcept {
    _sortBuckets = std::min(buckets, sort.size());
    _sort = &sort;
  }

  // getVariablesUsedHere, modifying the set in-place.
  void getVariablesUsedHere(aql::VarSet& vars) const final;

  // Return IResearchViewNode options.
  Options const& options() const noexcept { return _options; }

  // Node volatility, determines how often query has
  //  to be rebuilt during the execution.
  // Return pair:
  //  first - node has nondeterministic/dependent (inside a loop)
  //   filter condition
  //  second - node has nondeterministic/dependent (inside a loop)
  //   sort condition
  std::pair<bool, bool> volatility(bool force = false) const;

  void setHeapSort(std::vector<HeapSortElement>&& sort, size_t limit) {
    _heapSort = std::move(sort);
    _heapSortLimit = limit;
  }

#ifdef ARANGODB_USE_GOOGLE_TESTS
  size_t getHeapSortLimit() const noexcept { return _heapSortLimit; }

  auto getHeapSort() const noexcept { return std::span(_heapSort); }
#endif

  // Creates corresponding ExecutionBlock.
  std::unique_ptr<aql::ExecutionBlock> createBlock(
      aql::ExecutionEngine& engine) const final;

  aql::RegIdSet calcInputRegs() const;

  bool hasOffsetInfo() const noexcept {
    // TODO(MBkkt) should be different if we use same register for late
    // materialization and offset info
    return _outSearchDocId != nullptr;
  }

  aql::Variable const* searchDocIdVar() const noexcept {
    return _outSearchDocId;
  }

  void setSearchDocIdVar(aql::Variable const& var) noexcept {
    _outSearchDocId = &var;
  }

  bool isLateMaterialized() const noexcept {
    return _outNonMaterializedDocId != nullptr;
  }

  bool isHeapSort() const noexcept { return !_heapSort.empty(); }

  void setLateMaterialized(aql::Variable const& docIdVariable) noexcept {
    _outNonMaterializedDocId = &docIdVariable;
  }

  bool isNoMaterialization() const noexcept { return _noMaterialization; }

  void setNoMaterialization() noexcept { _noMaterialization = true; }

  void setImmutableParts(uint32_t count) noexcept { _immutableParts = count; }

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

  using ViewValuesRegisters = std::map<ptrdiff_t, aql::FieldRegisters>;

  using ViewVarsInfo =
      containers::FlatHashMap<std::vector<basics::AttributeName> const*,
                              ViewVariableWithColumn>;

  std::pair<ptrdiff_t, size_t> getSourceColumnInfo(
      aql::VariableId id) const noexcept;

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
        std::span<aql::CalculationNode* const> calcNodes,
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

  [[nodiscard]] bool isBuilding() const;

 private:
  // Export to VelocyPack.
  void doToVelocyPack(velocypack::Builder&, unsigned) const final;

  // The database.
  TRI_vocbase_t& _vocbase;

  // Underlying view.
  // Need shared_ptr to ensure view validity.
  std::shared_ptr<LogicalView const> _view;
  std::shared_ptr<SearchMeta const> _meta;

  // Output variable to write ArangoSearch document identifiers
  // composed of segment id (8 bytes) and document id (4 bytes).
  // Both values fit AqlValue with type VPACK_INLINE which avoids
  // heap allocations.
  aql::Variable const* _outSearchDocId{nullptr};

  // Output variable to write to.
  aql::Variable const* _outVariable{nullptr};

  // Output variable to write only non-materialized document ids.
  // We store SearchDoc encoded here as AqlValue vpack inline
  aql::Variable const* _outNonMaterializedDocId{nullptr};

  // Output variables to non-materialized document view sort references.
  ViewValuesVars _outNonMaterializedViewVars;

  OptimizationState _optState;

  // Filter node to be processed by the view.
  aql::AstNode const* _filterCondition{nullptr};

  // Sort condition covered by the view
  // Sort condition.
  IResearchSortBase const* _sort{nullptr};
  // Number of sort buckets to use.
  size_t _sortBuckets{0};

  // Scorers related to the view.
  std::vector<SearchFunc> _scorers;

  // List of shards involved, need this for the cluster.
  containers::FlatHashMap<ShardID, LogicalView::Indexes> _shards;

  // Node options.
  Options _options;

  // Internal order for scorers.
  std::vector<HeapSortElement> _heapSort;
  size_t _heapSortLimit{0};

  // Volatility mask
  mutable int _volatilityMask{-1};

  uint32_t _immutableParts{0};

  // Whether "no materialization" rule should be applied
  bool _noMaterialization{false};

  // Optimizing time support.
  // Intentionally not serialized/copied
  bool _isUsedAsSatellite{false};
  arangodb::aql::Collection const* _prototypeCollection{};
};

}  // namespace iresearch
}  // namespace arangodb

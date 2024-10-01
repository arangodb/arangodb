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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/ExecutionNode/CollectionAccessingNode.h"
#include "Aql/ExecutionNode/DocumentProducingNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNodeId.h"
#include "Aql/Projections.h"
#include "Aql/RegisterPlan.h"
#include "Aql/types.h"
#include "Indexes/IndexIterator.h"
#include "Transaction/Methods.h"
#include "VocBase/Identifiers/IndexId.h"

#include <memory>
#include <string_view>
#include <vector>

namespace arangodb::aql {

struct Collection;
class Condition;
class ExecutionBlock;
class ExecutionEngine;
class ExecutionPlan;
class Expression;
class Projections;
struct NonConstExpressionContainer;

template<typename T>
struct RegisterPlanT;
using RegisterPlan = RegisterPlanT<ExecutionNode>;

/// @brief class IndexNode
class IndexNode : public ExecutionNode,
                  public DocumentProducingNode,
                  public CollectionAccessingNode {
  friend class ExecutionBlock;

 public:
  enum class Strategy {
    // no need to produce any result. we can scan over the index
    // but do not have to look into its values
    kNoResult,

    // index covers all projections of the query. we can get
    // away with reading data from the index only
    kCovering,

    // index covers the IndexNode's filter condition only,
    // but not the rest of the query. that means we can use the
    // index data to evaluate the IndexNode's post-filter condition,
    // for any entries that pass the filter, we don't read the document
    // from the storage engine
    kCoveringFilterScanOnly,

    // index covers the IndexNode's filter condition only,
    // but not the rest of the query. that means we can use the
    // index data to evaluate the IndexNode's post-filter condition,
    // but for any entries that pass the filter, we will need to
    // read the full documents in addition
    kCoveringFilterOnly,

    // index does not cover the required data. we will need to
    // read the full documents for all index entries
    kDocument,

    // late materialization
    kLateMaterialized,

    // we only need to count the number of index entries
    kCount
  };

  IndexNode(ExecutionPlan* plan, ExecutionNodeId id,
            aql::Collection const* collection, Variable const* outVariable,
            std::vector<transaction::Methods::IndexHandle> const& indexes,
            bool allCoveredByOneIndex, std::unique_ptr<Condition> condition,
            IndexIteratorOptions const&);

  IndexNode(ExecutionPlan*, arangodb::velocypack::Slice const& base);

  ~IndexNode() override;

  /// @brief return the type of the node
  NodeType getType() const override final;

  /// @brief return the amount of bytes used
  size_t getMemoryUsedBytes() const override final;

  /// @brief return the condition for the node
  Condition* condition() const;

  /// @brief whether or not all indexes are accessed in reverse order
  IndexIteratorOptions options() const;

  /// @brief return name for strategy
  static std::string_view strategyName(Strategy strategy) noexcept;

  /// @brief set reverse mode
  void setAscending(bool value);

  /// @brief set limit
  void setLimit(uint64_t value) noexcept;

  /// @brief return if the index node has limit
  bool hasLimit() const noexcept;

  /// @brief whether or not the index node needs a post sort of the results
  /// of multiple shards in the cluster (via a GatherNode).
  /// not all queries that use an index will need to produce a sorted result
  /// (e.g. if the index is used only for filtering)
  bool needsGatherNodeSort() const;
  void needsGatherNodeSort(bool value);

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine) const override;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan,
                       bool withDependencies) const override final;

  /// @brief replaces variables in the internals of the execution node
  /// replacements are { old variable id => new variable }
  void replaceVariables(std::unordered_map<VariableId, Variable const*> const&
                            replacements) override;

  void replaceAttributeAccess(ExecutionNode const* self,
                              Variable const* searchVariable,
                              std::span<std::string_view> attribute,
                              Variable const* replaceVariable,
                              size_t index) override;

  /// @brief getVariablesSetHere
  std::vector<Variable const*> getVariablesSetHere() const override final;

  /// @brief getVariablesUsedHere, modifying the set in-place
  void getVariablesUsedHere(VarSet& vars) const override final;

  /// @brief estimateCost
  CostEstimate estimateCost() const override final;

  AsyncPrefetchEligibility canUseAsyncPrefetching()
      const noexcept override final;

  /// @brief getIndexes, hand out the indexes used
  std::vector<transaction::Methods::IndexHandle> const& getIndexes() const;

  bool isLateMaterialized() const noexcept {
    return _outNonMaterializedDocId != nullptr;
  }

  bool canApplyLateDocumentMaterializationRule() const {
    return isProduceResult() && !_projections.usesCoveringIndex();
  }

  bool isDeterministic() override final {
    return canReadOwnWrites() == ReadOwnWrites::no;
  }

  bool isAllCoveredByOneIndex() const noexcept { return _allCoveredByOneIndex; }

  struct IndexVariable {
    size_t indexFieldNum;
    Variable const* var;
  };

  using IndexFilterCoveringVars = std::unordered_map<Variable const*, size_t>;
  using IndexValuesVars = std::pair<IndexId, IndexFilterCoveringVars>;

  using IndexValuesRegisters =
      std::pair<IndexId, std::unordered_map<size_t, RegisterId>>;

  using IndexVarsInfo =
      std::unordered_map<std::vector<arangodb::basics::AttributeName> const*,
                         IndexVariable>;

  void setLateMaterialized(aql::Variable const* docIdVariable,
                           IndexId commonIndexId,
                           IndexVarsInfo const& indexVariables);

  void setProjections(Projections projections) override;

  /// @brief remember the condition to execute for early filtering
  void setFilter(std::unique_ptr<Expression> filter) override;

  // prepare projections for usage with an index
  void prepareProjections();

  bool recalculateProjections(ExecutionPlan*) override;

  bool isProduceResult() const override;

  aql::Variable const* getLateMaterializedDocIdOutVar() const;

  // returns the single index pointer if the IndexNode uses a single index,
  // nullptr otherwise
  [[nodiscard]] transaction::Methods::IndexHandle getSingleIndex() const;

 protected:
  /// @brief export to VelocyPack
  void doToVelocyPack(arangodb::velocypack::Builder&,
                      unsigned flags) const override final;

 private:
  void updateProjectionsIndexInfo();

  /// @brief determine the IndexNode strategy
  Strategy strategy() const;

  NonConstExpressionContainer buildNonConstExpressions() const;

  /// @brief adds a UNIQUE() to a dynamic IN condition
  arangodb::aql::AstNode* makeUnique(AstNode*) const;

  /// @brief the index
  std::vector<transaction::Methods::IndexHandle> _indexes;

  /// @brief the index(es) condition
  std::unique_ptr<Condition> _condition;

  /// @brief the index sort order - this is the same order for all indexes
  bool _needsGatherNodeSort;

  /// @brief We have single index and this index covered whole condition
  bool _allCoveredByOneIndex;

  /// @brief if the projections are fully covered by the index attributes
  std::optional<bool> _indexCoversProjections;

  /// @brief the index iterator options - same for all indexes
  IndexIteratorOptions _options;

  /// @brief output variable to write only non-materialized document ids
  aql::Variable const* _outNonMaterializedDocId;
};

}  // namespace arangodb::aql

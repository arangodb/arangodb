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

#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNodeId.h"
#include "Aql/Projections.h"
#include "Aql/types.h"
#include "Indexes/Index.h"
#include "Indexes/IndexIterator.h"
#include "Transaction/Methods.h"

#include <memory>
#include <vector>

namespace arangodb::aql {

struct Collection;
class Condition;
class ExecutionBlock;
class ExecutionEngine;
class ExecutionPlan;

// not yet supported:
// - IndexIteratorOptions: sorted, ascending, evalFCalls, useCache, waitForSync,
// limit, lookahead
// - reverse iteration
class JoinNode : public ExecutionNode {
  friend class ExecutionBlock;

 public:
  struct IndexInfo {
    aql::Collection const* collection;
    std::string usedShard;
    Variable const* outVariable;
    std::unique_ptr<Condition> condition;
    std::unique_ptr<Expression> filter;
    transaction::Methods::IndexHandle index;
    Projections projections;
    Projections filterProjections;
    bool usedAsSatellite{false};  // TODO maybe use CollectionAccess class
    bool producesOutput{true};
    bool isLateMaterialized{false};
    bool isUniqueStream{false};
    Variable const* outDocIdVariable = nullptr;
    std::vector<std::unique_ptr<Expression>> expressions;
    std::vector<size_t> usedKeyFields;
    std::vector<size_t> constantFields;
  };

  JoinNode(ExecutionPlan* plan, ExecutionNodeId id,
           std::vector<IndexInfo> indexInfos, IndexIteratorOptions const&);

  JoinNode(ExecutionPlan*, arangodb::velocypack::Slice const& base);

  ~JoinNode() override;

  /// @brief return the type of the node
  NodeType getType() const override final;

  /// @brief return the amount of bytes used
  size_t getMemoryUsedBytes() const override final;

  /// @brief whether or not all indexes are accessed in reverse order
  IndexIteratorOptions options() const;

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

  /// @brief getIndexesInfos, hand out the index infos
  std::vector<IndexInfo> const& getIndexInfos() const;
  std::vector<IndexInfo>& getIndexInfos();

  bool isDeterministic() override final;

 protected:
  /// @brief export to VelocyPack
  void doToVelocyPack(arangodb::velocypack::Builder&,
                      unsigned flags) const override final;

 private:
  Index::FilterCosts costsForIndexInfo(IndexInfo const& info) const;

  /// @brief the index infos
  std::vector<IndexInfo> _indexInfos;

  /// @brief the index iterator options - same for all indexes
  IndexIteratorOptions _options;
};

}  // namespace arangodb::aql

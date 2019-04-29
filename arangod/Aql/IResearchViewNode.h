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

#include "Aql/Collection.h"
#include "Aql/ExecutionNode.h"
#include "IResearch/IResearchOrderFactory.h"
#include "IResearch/IResearchViewSort.h"

namespace arangodb {

namespace aql {
struct Collection;
class ExecutionBlock;
class ExecutionEngine;
}  // namespace aql

namespace iresearch {

/// @brief class EnumerateViewNode
class IResearchViewNode final : public arangodb::aql::ExecutionNode {
  friend class arangodb::aql::RedundantCalculationsReplacer;

 public:
  /// @brief node options
  struct Options {
    /// @brief a list of data source CIDs to restrict a query
    arangodb::HashSet<TRI_voc_cid_t> sources;

    /// @brief use the list of sources to restrict a query
    bool restrictSources{false};

    /// @brief sync view before querying to get the latest index snapshot
    bool forceSync{false};
  };  // Options

  IResearchViewNode(aql::ExecutionPlan& plan, size_t id, TRI_vocbase_t& vocbase,
                    std::shared_ptr<const arangodb::LogicalView> const& view,
                    aql::Variable const& outVariable, aql::AstNode* filterCondition,
                    aql::AstNode* options, std::vector<Scorer>&& scorers);

  IResearchViewNode(aql::ExecutionPlan&, velocypack::Slice const& base);

  /// @brief return the type of the node
  NodeType getType() const override final { return ENUMERATE_IRESEARCH_VIEW; }

  /// @brief export to VelocyPack
  void toVelocyPackHelper(arangodb::velocypack::Builder&, unsigned) const override final;

  /// @brief clone ExecutionNode recursively
  aql::ExecutionNode* clone(aql::ExecutionPlan* plan, bool withDependencies,
                            bool withProperties) const override final;

  /// @returns the list of the linked collections + view itself
  std::vector<std::reference_wrapper<aql::Collection const>> collections() const;

  /// @returns true if underlying view has no links
  bool empty() const noexcept;

  /// @brief the cost of an enumerate view node
  aql::CostEstimate estimateCost() const override final;

  /// @brief getVariablesSetHere
  std::vector<arangodb::aql::Variable const*> getVariablesSetHere() const override final {
    std::vector<arangodb::aql::Variable const*> vars(1 + _scorers.size());

    *std::transform(_scorers.begin(), _scorers.end(), vars.begin(),
                    [](auto const& scorer) { return scorer.var; }) = _outVariable;

    return vars;
  }

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

  /// @brief return true if the filter condition is empty
  bool filterConditionIsEmpty() const noexcept;

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
  IResearchViewSort const* sort() const noexcept {
    return _sort;
  }

  /// @brief set sort condition satisfied by a sorted index
  void sort(IResearchViewSort const* sort) noexcept {
    _sort = sort;
  }

  /// @brief getVariablesUsedHere, modifying the set in-place
  void getVariablesUsedHere(arangodb::HashSet<aql::Variable const*>& vars) const override final;

  /// @brief returns IResearchViewNode options
  Options const& options() const noexcept { return _options; }

  /// @brief node volatility, determines how often query has
  ///        to be rebuilt during the execution
  /// @note first - node has nondeterministic/dependent (inside a loop)
  ///       filter condition
  ///       second - node has nondeterministic/dependent (inside a loop)
  ///       sort condition
  std::pair<bool, bool> volatility(bool force = false) const;

  void planNodeRegisters(std::vector<aql::RegisterId>& nrRegsHere,
                         std::vector<aql::RegisterId>& nrRegs,
                         std::unordered_map<aql::VariableId, VarInfo>& varInfo,
                         unsigned int& totalNrRegs, unsigned int depth) const;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<aql::ExecutionBlock> createBlock(
      aql::ExecutionEngine& engine,
      std::unordered_map<aql::ExecutionNode*, aql::ExecutionBlock*> const&) const override;

  std::shared_ptr<std::unordered_set<aql::RegisterId>> calcInputRegs() const;

 private:
  /// @brief the database
  TRI_vocbase_t& _vocbase;

  /// @brief view
  /// @note need shared_ptr to ensure view validity
  std::shared_ptr<const LogicalView> _view;

  /// @brief output variable to write to
  aql::Variable const* _outVariable;

  /// @brief filter node to pass to the view
  aql::AstNode const* _filterCondition;

  /// @brief sort condition covered by the view
  IResearchViewSort const* _sort{};

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

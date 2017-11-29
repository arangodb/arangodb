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

#ifndef ARANGOD_IRESEARCH__ENUMERATE_VIEW_NODE_H
#define ARANGOD_IRESEARCH__ENUMERATE_VIEW_NODE_H 1

#include "Aql/ExecutionNode.h"

namespace arangodb {
namespace iresearch {

/// @brief class EnumerateViewNode
class IResearchViewNode final : public arangodb::aql::ExecutionNode {
  friend class arangodb::aql::ExecutionNode;
  friend class arangodb::aql::ExecutionBlock;
  friend class EnumerateViewBlock;
  friend class arangodb::aql::RedundantCalculationsReplacer;

 public:
  IResearchViewNode(
      arangodb::aql::ExecutionPlan* plan,
      size_t id,
      TRI_vocbase_t* vocbase,
      std::shared_ptr<arangodb::LogicalView> view,
      arangodb::aql::Variable const* outVariable,
      arangodb::aql::Condition* condition,
      std::shared_ptr<arangodb::aql::SortCondition> sortCondition)
    : arangodb::aql::ExecutionNode(plan, id),
      _vocbase(vocbase),
      _view(view),
      _outVariable(outVariable),
      _condition(condition),
      _sortCondition(sortCondition) {
    TRI_ASSERT(_vocbase);
    TRI_ASSERT(_view);
    TRI_ASSERT(_outVariable);
  }

  IResearchViewNode(
    arangodb::aql::ExecutionPlan*,
    arangodb::velocypack::Slice const& base
  );

  virtual ~IResearchViewNode();

  /// @brief return the type of the node
  NodeType getType() const override final {
    return ENUMERATE_IRESEARCH_VIEW;
  }

  /// @brief export to VelocyPack
  void toVelocyPackHelper(
    arangodb::velocypack::Builder&,
    bool
  ) const override final;

  /// @brief clone ExecutionNode recursively
  aql::ExecutionNode* clone(
    aql::ExecutionPlan* plan,
    bool withDependencies,
    bool withProperties
  ) const override final;

  /// @brief the cost of an enumerate list node
  double estimateCost(size_t&) const override final;

  /// @brief getVariablesSetHere
  std::vector<arangodb::aql::Variable const*> getVariablesSetHere() const override final {
    return {_outVariable};
  }

  /// @brief return out variable
  arangodb::aql::Variable const* outVariable() const noexcept {
    return _outVariable;
  }

  /// @brief return the database
  TRI_vocbase_t* vocbase() const noexcept {
    return _vocbase; 
  }

  /// @brief return the view
  std::shared_ptr<arangodb::LogicalView> view() const noexcept { 
    return _view;
  }

  /// @brief return the filter node
  aql::Condition* condition() const noexcept { return _condition; }

  /// @brief return the condition to pass to the view
  std::shared_ptr<aql::SortCondition> sortCondition() const noexcept {
    return _sortCondition;
  }

  std::unique_ptr<ViewIterator> iterator(
    transaction::Methods& trx,
    aql::ExpressionContext& ctx
  ) const;

  /// @brief getVariablesUsedHere, returning a vector
  std::vector<aql::Variable const*> getVariablesUsedHere() const override final;

  /// @brief getVariablesUsedHere, modifying the set in-place
  void getVariablesUsedHere(
    std::unordered_set<aql::Variable const*>& vars
  ) const override final;

  /// @brief node has nondeterministic filter condition or located inside a loop
  bool volatile_state() const;

 private:
  /// @brief the database
  TRI_vocbase_t* _vocbase;

  /// @brief collection
  std::shared_ptr<LogicalView> _view;

  /// @brief output variable to write to
  aql::Variable const* _outVariable;

  /// @brief filter node to pass to view
  aql::Condition* _condition;

  /// @brief sortCondition to pass to the view
  std::shared_ptr<aql::SortCondition> _sortCondition;
}; // EnumerateViewNode

} // iresearch
} // arangodb

#endif // ARANGOD_IRESEARCH__ENUMERATE_VIEW_NODE_H

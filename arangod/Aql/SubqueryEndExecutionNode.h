////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////


#ifndef ARANGOD_AQL_SUBQUERY_END_EXECUTION_NODE_H
#define ARANGOD_AQL_SUBQUERY_END_EXECUTION_NODE_H 1

#include "Aql/ExecutionNode.h"

class SubqueryEndNode : public ExecutionNode {
  friend class ExecutionNode;
  friend class ExecutionBlock;

 public:
  SubqueryNode(ExecutionPlan*, arangodb::velocypack::Slice const& base);

  SubqueryNode(ExecutionPlan* plan, size_t id, ExecutionNode* subquery, Variable const* outVariable)
      : ExecutionNode(plan, id), _subquery(subquery), _outVariable(outVariable) {
    TRI_ASSERT(_subquery != nullptr);
    TRI_ASSERT(_outVariable != nullptr);
  }

  /// @brief return the type of the node
  NodeType getType() const override final { return SUBQUERY; }

  /// @brief invalidate the cost estimate for the node and its dependencies
  void invalidateCost() override;

  /// @brief return the out variable
  Variable const* outVariable() const { return _outVariable; }

  /// @brief export to VelocyPack
  void toVelocyPackHelper(arangodb::velocypack::Builder&, unsigned flags) const override final;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine,
      std::unordered_map<ExecutionNode*, ExecutionBlock*> const&) const override;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan, bool withDependencies,
                       bool withProperties) const override final;

  /// @brief whether or not the subquery is a data-modification operation
  bool isModificationSubquery() const;

  /// @brief getter for subquery
  ExecutionNode* getSubquery() const { return _subquery; }

  /// @brief setter for subquery
  void setSubquery(ExecutionNode* subquery, bool forceOverwrite) {
    TRI_ASSERT(subquery != nullptr);
    TRI_ASSERT((forceOverwrite && _subquery != nullptr) ||
               (!forceOverwrite && _subquery == nullptr));
    _subquery = subquery;
  }

  /// @brief estimateCost
  CostEstimate estimateCost() const override final;

  /// @brief getVariablesUsedHere, modifying the set in-place
  void getVariablesUsedHere(arangodb::HashSet<Variable const*>& vars) const override final;

  /// @brief getVariablesSetHere
  std::vector<Variable const*> getVariablesSetHere() const override final {
    return std::vector<Variable const*>{_outVariable};
  }

  /// @brief replace the out variable, so we can adjust the name.
  void replaceOutVariable(Variable const* var);

  bool isDeterministic() override final;

  bool isConst();
  bool mayAccessCollections();

 private:
  /// @brief we need to have an expression and where to write the result
  ExecutionNode* _subquery;

  /// @brief variable to write to
  Variable const* _outVariable;
};

class SubqueryEndNode : public ExecutionNode {
  friend class ExecutionNode;
  friend class ExecutionBlock;

 public:
  SubqueryNode(ExecutionPlan*, arangodb::velocypack::Slice const& base);

  SubqueryNode(ExecutionPlan* plan, size_t id, ExecutionNode* subquery, Variable const* outVariable)
      : ExecutionNode(plan, id), _subquery(subquery), _outVariable(outVariable) {
    TRI_ASSERT(_subquery != nullptr);
    TRI_ASSERT(_outVariable != nullptr);
  }

  /// @brief return the type of the node
  NodeType getType() const override final { return SUBQUERY; }

  /// @brief invalidate the cost estimate for the node and its dependencies
  void invalidateCost() override;

  /// @brief return the out variable
  Variable const* outVariable() const { return _outVariable; }

  /// @brief export to VelocyPack
  void toVelocyPackHelper(arangodb::velocypack::Builder&, unsigned flags) const override final;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine,
      std::unordered_map<ExecutionNode*, ExecutionBlock*> const&) const override;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan, bool withDependencies,
                       bool withProperties) const override final;

  /// @brief whether or not the subquery is a data-modification operation
  bool isModificationSubquery() const;

  /// @brief getter for subquery
  ExecutionNode* getSubquery() const { return _subquery; }

  /// @brief setter for subquery
  void setSubquery(ExecutionNode* subquery, bool forceOverwrite) {
    TRI_ASSERT(subquery != nullptr);
    TRI_ASSERT((forceOverwrite && _subquery != nullptr) ||
               (!forceOverwrite && _subquery == nullptr));
    _subquery = subquery;
  }

  /// @brief estimateCost
  CostEstimate estimateCost() const override final;

  /// @brief getVariablesUsedHere, modifying the set in-place
  void getVariablesUsedHere(arangodb::HashSet<Variable const*>& vars) const override final;

  /// @brief getVariablesSetHere
  std::vector<Variable const*> getVariablesSetHere() const override final {
    return std::vector<Variable const*>{_outVariable};
  }

  /// @brief replace the out variable, so we can adjust the name.
  void replaceOutVariable(Variable const* var);

  bool isDeterministic() override final;

  bool isConst();
  bool mayAccessCollections();

 private:
  /// @brief we need to have an expression and where to write the result
  ExecutionNode* _subquery;

  /// @brief variable to write to
  Variable const* _outVariable;
};


#endif


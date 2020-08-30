////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_TRAVERSAL_NODE_H
#define ARANGOD_AQL_TRAVERSAL_NODE_H 1

#include "Aql/Condition.h"
#include "Aql/GraphNode.h"
#include "Aql/Graphs.h"
#include "VocBase/LogicalCollection.h"

namespace arangodb {

namespace aql {
struct Collection;
}

namespace graph {
struct BaseOptions;
}

namespace traverser {
struct TraverserOptions;
}

namespace aql {

/// @brief class TraversalNode
class TraversalNode : public virtual GraphNode {
  class TraversalEdgeConditionBuilder final : public EdgeConditionBuilder {
   private:
    /// @brief reference to the outer traversal node
    TraversalNode const* _tn;

   protected:
    // Create the _fromCondition for the first time.
    void buildFromCondition() override;

    // Create the _toCondition for the first time.
    void buildToCondition() override;

   public:
    explicit TraversalEdgeConditionBuilder(TraversalNode const*);

    TraversalEdgeConditionBuilder(TraversalNode const*, arangodb::velocypack::Slice const&);

    TraversalEdgeConditionBuilder(TraversalNode const*, TraversalEdgeConditionBuilder const*);

    ~TraversalEdgeConditionBuilder() = default;

    void toVelocyPack(arangodb::velocypack::Builder&, bool);
  };

  friend class ExecutionBlock;
  friend class RedundantCalculationsReplacer;

  /// @brief constructor with a vocbase and a collection name
 public:
  TraversalNode(ExecutionPlan* plan, ExecutionNodeId id, TRI_vocbase_t* vocbase,
                AstNode const* direction, AstNode const* start,
                AstNode const* graph, std::unique_ptr<Expression> pruneExpression,
                std::unique_ptr<graph::BaseOptions> options);

  TraversalNode(ExecutionPlan* plan, arangodb::velocypack::Slice const& base);

  ~TraversalNode();

  /// @brief Internal constructor to clone the node.
  TraversalNode(ExecutionPlan* plan, ExecutionNodeId id, TRI_vocbase_t* vocbase,
                std::vector<Collection*> const& edgeColls,
                std::vector<Collection*> const& vertexColls, Variable const* inVariable,
                std::string const& vertexId, TRI_edge_direction_e defaultDirection,
                std::vector<TRI_edge_direction_e> const& directions,
                std::unique_ptr<graph::BaseOptions> options, graph::Graph const* graph);

 protected:
  /// @brief Clone constructor, used for constructors of derived classes.
  /// Does not clone recursively, does not clone properties (`other.plan()` is
  /// expected to be the same as `plan)`, and does not register this node in the
  /// plan.
  /// When allowAlreadyBuiltCopy is true, allows copying a node which options
  /// are already prepared. prepareOptions() has to be called on the copy if
  /// the options were already prepared on other (_optionsBuilt)!
  TraversalNode(ExecutionPlan& plan, TraversalNode const& other,
                bool allowAlreadyBuiltCopy = false);

 public:
  /// @brief return the type of the node
  NodeType getType() const override final { return TRAVERSAL; }

  /// @brief export to VelocyPack
  void toVelocyPackHelper(arangodb::velocypack::Builder&, unsigned flags,
                          std::unordered_set<ExecutionNode const*>& seen) const override final;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine,
      std::unordered_map<ExecutionNode*, ExecutionBlock*> const&) const override;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan, bool withDependencies,
                       bool withProperties) const override;

  /// @brief Test if this node uses an in variable or constant
  bool usesInVariable() const { return _inVariable != nullptr; }

  /// @brief getVariablesUsedHere
  void getVariablesUsedHere(VarSet& result) const override final {
    for (auto const& condVar : _conditionVariables) {
      if (condVar != getTemporaryVariable()) {
        result.emplace(condVar);
      }
    }
    for (auto const& pruneVar : _pruneVariables) {
      if (pruneVar != vertexOutVariable() && pruneVar != edgeOutVariable() &&
          pruneVar != pathOutVariable()) {
        result.emplace(pruneVar);
      }
    }
    if (usesInVariable()) {
      result.emplace(_inVariable);
    }
  }

  /// @brief getVariablesSetHere
  std::vector<Variable const*> getVariablesSetHere() const override final {
    std::vector<Variable const*> vars;
    if (usesVertexOutVariable()) {
      vars.emplace_back(vertexOutVariable());
    }
    if (usesEdgeOutVariable()) {
      vars.emplace_back(edgeOutVariable());
    }
    if (usesPathOutVariable()) {
      vars.emplace_back(pathOutVariable());
    }
    return vars;
  }

  /// @brief checks if the path out variable is used
  bool usesPathOutVariable() const;

  /// @brief return the path out variable
  Variable const* pathOutVariable() const;

  /// @brief set the path out variable
  void setPathOutput(Variable const* outVar);

  /// @brief return the in variable
  Variable const* inVariable() const;

  std::string const getStartVertex() const;

  void setInVariable(Variable const* inVariable);

  /// @brief remember the condition to execute for early traversal abortion.
  void setCondition(std::unique_ptr<Condition> condition);

  /// @brief return the condition for the node
  Condition* condition() const { return _condition.get(); }

  /// @brief which variable? -1 none, 0 Edge, 1 Vertex, 2 path
  int checkIsOutVariable(size_t variableId) const;

  /// @brief check whether an access is inside the specified range
  bool isInRange(uint64_t, bool) const;

  /// @brief register a filter condition on a given search depth.
  ///        If this condition is not fulfilled a traversal will abort.
  ///        The condition will contain the local variable for it's accesses.
  void registerCondition(bool, uint64_t, AstNode const*);

  /// @brief register a filter condition for all search depths
  ///        If this condition is not fulfilled a traversal will abort.
  ///        The condition will contain the local variable for it's accesses.
  void registerGlobalCondition(bool, AstNode const*);

  bool allDirectionsEqual() const;

  void getConditionVariables(std::vector<Variable const*>&) const override;

  void getPruneVariables(std::vector<Variable const*>&) const;

  /// @brief Compute the traversal options containing the expressions
  ///        MUST! be called after optimization and before creation
  ///        of blocks.
  void prepareOptions() override;

  // @brief Get reference to the Prune expression.
  //        You are not responsible for it!
  Expression* pruneExpression() const { return _pruneExpression.get(); }

  /// @brief Overrides GraphNode::options() with a more specific return type
  ///  (casts graph::BaseOptions* into traverser::TraverserOptions*)
  auto options() const -> traverser::TraverserOptions*;

 private:
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  void checkConditionsDefined() const;
#endif

  void traversalCloneHelper(ExecutionPlan& plan, TraversalNode& c, bool withProperties) const;

 private:
  /// @brief vertex output variable
  Variable const* _pathOutVariable;

  /// @brief input variable only used if _vertexId is unused
  Variable const* _inVariable;

  /// @brief input vertexId only used if _inVariable is unused
  std::string _vertexId;

  /// @brief early abort traversal conditions:
  std::unique_ptr<Condition> _condition;

  /// @brief variables that are inside of the condition
  VarSet _conditionVariables;

  /// @brief The hard coded condition on _from
  AstNode* _fromCondition;

  /// @brief The hard coded condition on _to
  AstNode* _toCondition;

  /// @brief The condition given in PRUNE (might be empty)
  std::unique_ptr<Expression> _pruneExpression;

  /// @brief The global edge condition. Does not contain
  ///        _from and _to checks
  std::vector<AstNode const*> _globalEdgeConditions;

  /// @brief The global vertex condition
  std::vector<AstNode const*> _globalVertexConditions;

  /// @brief List of all depth specific conditions for edges
  std::unordered_map<uint64_t, std::unique_ptr<TraversalEdgeConditionBuilder>> _edgeConditions;

  /// @brief List of all depth specific conditions for vertices
  std::unordered_map<uint64_t, AstNode*> _vertexConditions;

  /// @brief the hashSet for variables used in pruning
  VarSet _pruneVariables;
};

}  // namespace aql
}  // namespace arangodb

#endif

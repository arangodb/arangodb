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

#include "Aql/EdgeConditionBuilder.h"
#include "Aql/ExecutionNode/GraphNode.h"
#include "Aql/Executor/TraversalExecutor.h"
#include "Aql/IndexHint.h"
#include "Aql/PruneExpressionEvaluator.h"
#include "Graph/Types/UniquenessLevel.h"
#include "VocBase/LogicalCollection.h"

#include <memory>
#include <unordered_map>
#include <vector>

namespace arangodb {

namespace aql {
struct Collection;
class Condition;
}  // namespace aql

namespace graph {
struct BaseOptions;
struct IndexAccessor;
}  // namespace graph

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

    TraversalEdgeConditionBuilder(TraversalNode const* tn,
                                  arangodb::velocypack::Slice condition);

    TraversalEdgeConditionBuilder(TraversalNode const* tn,
                                  TraversalEdgeConditionBuilder const* other);

    ~TraversalEdgeConditionBuilder() = default;

    void toVelocyPack(arangodb::velocypack::Builder& builder, bool verbose);
  };

  friend class ExecutionBlock;

  /// @brief constructor with a vocbase and a collection name
 public:
  TraversalNode(ExecutionPlan* plan, ExecutionNodeId id, TRI_vocbase_t* vocbase,
                AstNode const* direction, AstNode const* start,
                AstNode const* graph,
                std::unique_ptr<Expression> pruneExpression,
                std::unique_ptr<graph::BaseOptions> options);

  TraversalNode(ExecutionPlan* plan, arangodb::velocypack::Slice base);

  ~TraversalNode();

  /// @brief Internal constructor to clone the node.
  TraversalNode(ExecutionPlan* plan, ExecutionNodeId id, TRI_vocbase_t* vocbase,
                std::vector<Collection*> const& edgeColls,
                std::vector<Collection*> const& vertexColls,
                Variable const* inVariable, std::string vertexId,
                TRI_edge_direction_e defaultDirection,
                std::vector<TRI_edge_direction_e> const& directions,
                std::unique_ptr<graph::BaseOptions> options,
                graph::Graph const* graph);

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

  /// @brief return the amount of bytes used
  size_t getMemoryUsedBytes() const override final;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine) const override;

  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine,
      std::vector<std::pair<Variable const*, RegisterId>>&&,
      std::function<
          void(std::shared_ptr<aql::PruneExpressionEvaluator>&)> const&,
      std::function<
          void(std::shared_ptr<aql::PruneExpressionEvaluator>&)> const&,
      const std::unordered_map<TraversalExecutorInfosHelper::OutputName,
                               RegisterId,
                               TraversalExecutorInfosHelper::OutputNameHash>&,
      RegisterId, RegisterInfos,
      std::unordered_map<ServerID, aql::EngineId> const*,
      bool isSmart = false) const;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan,
                       bool withDependencies) const override;

  /// @brief Test if this node uses an in variable or constant
  bool usesInVariable() const { return _inVariable != nullptr; }

  void replaceVariables(std::unordered_map<VariableId, Variable const*> const&
                            replacements) override;

  void replaceAttributeAccess(ExecutionNode const* self,
                              Variable const* searchVariable,
                              std::span<std::string_view> attribute,
                              Variable const* replaceVariable,
                              size_t index) override;

  /// @brief getVariablesUsedHere
  void getVariablesUsedHere(VarSet& result) const override final;

  /// @brief getVariablesSetHere
  std::vector<Variable const*> getVariablesSetHere() const override final;

  /// @brief checks if the path out variable is used by other nodes
  bool isPathOutVariableUsedLater() const;

  /// @brief checks if the path out variable is used by other nodes
  /// or accessed in a prune expression
  bool isPathOutVariableAccessed() const;

  /// @brief checks if the edge out variable is used by other nodes
  /// or accessed in a prune expression
  bool isEdgeOutVariableAccessed() const;

  /// @brief checks if the vertex out variable is used by other nodes
  /// or accessed in a prune expression
  bool isVertexOutVariableAccessed() const;

  /// @brief return the path out variable
  Variable const* pathOutVariable() const;

  /// @brief set the path out variable
  void setPathOutput(Variable const* outVar);

  /// @brief return the in variable
  Variable const* inVariable() const;

  std::string getStartVertex() const;

  void setInVariable(Variable const* inVariable);

  /// @brief remember the condition to execute for early traversal abortion.
  void setCondition(std::unique_ptr<Condition> condition);

  /// @brief return the condition for the node
  Condition const* condition() const { return _condition.get(); }

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

  /// @brief register a filter condition to be applied before the result is
  /// returned.
  ///        This condition validates the edge
  void registerPostFilterCondition(AstNode const* condition);

  void getConditionVariables(std::vector<Variable const*>&) const override;

  void getPruneVariables(std::vector<Variable const*>&) const;

  void getPostFilterVariables(std::vector<Variable const*>&) const;

  /// @brief Compute the traversal options containing the expressions
  ///        MUST! be called after optimization and before creation
  ///        of blocks.
  void prepareOptions() override;

  std::vector<arangodb::graph::IndexAccessor> buildIndexAccessor(
      TraversalEdgeConditionBuilder& conditionBuilder) const;
  std::vector<arangodb::graph::IndexAccessor> buildUsedIndexes() const;
  std::unordered_map<uint64_t, std::vector<arangodb::graph::IndexAccessor>>
  buildUsedDepthBasedIndexes() const;

  /// @brief Overrides GraphNode::options() with a more specific return type
  ///  (casts graph::BaseOptions* into traverser::TraverserOptions*)
  auto options() const -> traverser::TraverserOptions*;

  // @brief Get reference to the Prune expression.
  //        You are not responsible for it!
  Expression* pruneExpression() const { return _pruneExpression.get(); }

  // @brief Get reference to the postFilter expression.
  //        You are not responsible for it!
  Expression* postFilterExpression() const;

 protected:
  /// @brief export to VelocyPack
  void doToVelocyPack(arangodb::velocypack::Builder&,
                      unsigned flags) const override final;

 private:
  // validates collections in OPTIONS against contents of named graph (GRAPH
  // attribute). throws if invalid collections are used.
  void validateCollections() const;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  void checkConditionsDefined() const;
#endif

  void traversalCloneHelper(ExecutionPlan& plan, TraversalNode& c) const;

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
  /// Note about memory: No need to track memory here separately. Inner
  /// AstNodes and ExecutionNodes are already been kept into account.
  std::unordered_map<uint64_t, std::unique_ptr<TraversalEdgeConditionBuilder>>
      _edgeConditions;

  /// @brief List of all depth specific conditions for vertices
  /// Note about memory: No need to track memory here separately.
  /// AstNodes are already been kept into account.
  std::unordered_map<uint64_t, AstNode*> _vertexConditions;

  /// @brief the hashSet for variables used in pruning
  VarSet _pruneVariables;

  /// @brief conditions to be applied before returning the result
  std::vector<AstNode const*> _postFilterConditions;

  /// @brief The condition used as PostFilter (might be empty)
  mutable std::unique_ptr<Expression> _postFilterExpression;

  /// @brief the hashSet for variables used in postFilter
  VarSet _postFilterVariables;

  graph::ClusterBaseProviderOptions getClusterBaseProviderOptions(
      traverser::TraverserOptions* opts,
      std::vector<std::pair<Variable const*, RegisterId>> const&
          filterConditionVariables) const;
  graph::SingleServerBaseProviderOptions getSingleServerBaseProviderOptions(
      traverser::TraverserOptions* opts,
      std::vector<std::pair<Variable const*, RegisterId>> const&
          filterConditionVariables) const;
};

}  // namespace aql
}  // namespace arangodb

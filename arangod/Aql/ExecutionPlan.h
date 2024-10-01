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

#include "Aql/CollectOptions.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNodeId.h"
#include "Aql/IndexHint.h"
#include "Aql/ModificationOptions.h"
#include "Aql/RegisterPlan.h"
#include "Aql/SortElement.h"
#include "Aql/types.h"
#include "Containers/FlatHashMap.h"
#include "Containers/HashSet.h"
#include "Containers/SmallVector.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>
#include <unordered_set>

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

namespace aql {
class Ast;
struct AstNode;
class CalculationNode;
class CollectNode;
class Collections;
class ExecutionNode;
struct OptimizerRule;
class QueryContext;
class SubqueryNode;

class ExecutionPlan {
 public:
  /// @brief create the plan
  /// note: tracking memory usage requires accessing the Ast/Query objects,
  /// which can be inherently unsafe when running within the gtest unit tests.
  explicit ExecutionPlan(Ast* ast, bool trackMemoryUsage);
  /// @brief whether or not memory usage should be tracked for this plan.

  /// @brief destroy the plan, frees all assigned nodes
  ~ExecutionPlan();

  /// @brief maximum number of execution nodes allowed per query
  /// (at the time the initial execution plan is created). we have to limit
  /// this to prevent super-long runtimes for query optimization and
  /// execution)
  static constexpr uint64_t maxPlanNodes = 4000;

  /// @brief create an execution plan from an AST
  /// note: tracking memory usage requires accessing the Ast/Query objects,
  /// which can be inherently unsafe when running within the gtest unit tests.
  static std::unique_ptr<ExecutionPlan> instantiateFromAst(
      Ast*, bool trackMemoryUsage);

  /// @brief process the list of collections in a VelocyPack
  static void getCollectionsFromVelocyPack(aql::Collections& colls,
                                           velocypack::Slice slice);

  /// @brief create an execution plan from VelocyPack
  static std::unique_ptr<ExecutionPlan> instantiateFromVelocyPack(
      Ast* ast, velocypack::Slice slice);

  /// @brief whether or not the exclusive flag is set in the write options
  static bool hasExclusiveAccessOption(AstNode const* node);

  std::unique_ptr<ExecutionPlan> clone(Ast* ast);

  /// @brief clone the plan by recursively cloning starting from the root
  std::unique_ptr<ExecutionPlan> clone();

  // build flags for plan serialization
  static unsigned buildSerializationFlags(bool verbose, bool includeInternals,
                                          bool explainRegisters) noexcept;

  /// @brief export to VelocyPack
  void toVelocyPack(velocypack::Builder& builder, unsigned flags,
                    std::function<void(velocypack::Builder&)> const&
                        serializeQueryData) const;

  /// @brief check if the plan is empty
  bool empty() const noexcept { return (_root == nullptr); }

  /// @brief note that an optimizer rule was applied
  void addAppliedRule(int level);

  /// @brief check if a specific optimizer rule was applied
  bool hasAppliedRule(int level) const noexcept;

  /// @brief check if a specific rule is disabled
  bool isDisabledRule(int rule) const noexcept;

  bool hasForcedIndexHints() const noexcept { return _hasForcedIndexHints; }

  /// @brief enable a specific rule
  void enableRule(int rule);

  /// @brief disable a specific rule
  void disableRule(int rule);

  /// @brief increase number of async prefetch nodes
  void increaseAsyncPrefetchNodes() noexcept;
  /// @brief decrease number of async prefetch nodes
  void decreaseAsyncPrefetchNodes() noexcept;

  size_t asyncPrefetchNodes() const noexcept;

  /// @brief returns the first unsatisfied forced index hint, if
  /// one exists. otherwise returns an empty index hint
  IndexHint firstUnsatisfiedForcedIndexHint() const;

  /// @brief return the next value for a node id
  ExecutionNodeId nextId();

  /// @brief get a node by its id
  ExecutionNode* getNodeById(ExecutionNodeId id) const;

  std::unordered_map<ExecutionNodeId, ExecutionNode*> const& getNodesById()
      const;

  /// @brief check if the node is the root node
  bool isRoot(ExecutionNode const* node) const noexcept {
    return _root == node;
  }

  /// @brief get the root node
  ExecutionNode* root() const noexcept {
    TRI_ASSERT(_root != nullptr);
    return _root;
  }

  /// @brief set the root node
  void root(ExecutionNode* node, bool force = false) {
    if (!force) {
      TRI_ASSERT(_root == nullptr);
    }
    _root = node;
  }

  /// @brief invalidate all cost estimations in the plan
  void invalidateCost() {
    TRI_ASSERT(_root != nullptr);
    _root->invalidateCost();
  }

  /// @brief get the estimated cost . . .
  CostEstimate getCost() {
    TRI_ASSERT(_root != nullptr);
    // Costs are only valid if the traversal options are prepared
    // if they already are this is a noop.
    prepareTraversalOptions();
    return _root->getCost();
  }

  /// @brief this can be called by the optimizer to tell that the
  /// plan is temporarily in an invalid state
  void setValidity(bool value) noexcept { _planValid = value; }

/// @brief show an overview over the plan
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  void show() const;
#endif

  /// @brief note this node for being excluded from producing scatter/gather
  /// nodes
  void excludeFromScatterGather(ExecutionNode const* node) {
    _excludeFromScatterGather.emplace(node);
  }

  bool shouldExcludeFromScatterGather(ExecutionNode const* node) const {
    return _excludeFromScatterGather.contains(node);
  }

  /// @brief get the node where variable with id <id> is introduced . . .
  ExecutionNode* getVarSetBy(VariableId id) const {
    auto it = _varSetBy.find(id);

    if (it == _varSetBy.end()) {
      return nullptr;
    }
    return (*it).second;
  }

  /// @brief find nodes of a certain type
  void findNodesOfType(containers::SmallVector<ExecutionNode*, 8>& result,
                       ExecutionNode::NodeType, bool enterSubqueries) const;

  /// @brief find nodes of certain types
  void findNodesOfType(containers::SmallVector<ExecutionNode*, 8>& result,
                       std::initializer_list<ExecutionNode::NodeType> const&,
                       bool enterSubqueries) const;

  /// @brief find unique nodes of certain types
  void findUniqueNodesOfType(
      containers::SmallVector<ExecutionNode*, 8>& result,
      std::initializer_list<ExecutionNode::NodeType> const&,
      bool enterSubqueries) const;

  /// @brief find all end nodes in a plan
  void findEndNodes(containers::SmallVector<ExecutionNode*, 8>& result,
                    bool enterSubqueries) const;

  /// @brief determine and set _varsUsedLater and _varSetBy
  void findVarUsage();

  /// @brief determine if the above are already set
  bool varUsageComputed() const;

  /// @brief determine if the above are already set
  void setVarUsageComputed() noexcept { _varUsageComputed = true; }

  /// @brief flush var usage calculation
  void clearVarUsageComputed() noexcept { _varUsageComputed = false; }

  /// @brief static analysis
  void planRegisters(ExplainRegisterPlan = ExplainRegisterPlan::No);

  /// @brief find all variables that are populated with data from collections
  void findCollectionAccessVariables();

  void prepareTraversalOptions();

  /// @brief unlinkNodes, note that this does not delete the removed
  /// nodes and that one cannot remove the root node of the plan.
  void unlinkNodes(std::unordered_set<ExecutionNode*> const& toUnlink);
  void unlinkNodes(
      ::arangodb::containers::HashSet<ExecutionNode*> const& toUnlink);

  /// @brief unlinkNode, note that this does not delete the removed
  /// node and that one cannot remove the root node of the plan.
  void unlinkNode(ExecutionNode*, bool allowUnlinkingRoot = false);

  /// @brief register a node with the plan
  ExecutionNode* registerNode(std::unique_ptr<ExecutionNode>);

  /// @brief add a node to the plan, will delete node if addition
  /// fails and throw an exception
  ExecutionNode* registerNode(ExecutionNode*);

  template<typename Node, typename... Args>
  Node* createNode(Args&&...);

  /// @brief add a subquery to the plan, will call registerNode internally
  SubqueryNode* registerSubquery(SubqueryNode*);

  /// @brief replaceNode, note that <newNode> must be registered with the plan
  /// before this method is called, also this does not delete the old
  /// node and that one cannot replace the root node of the plan.
  void replaceNode(ExecutionNode* oldNode, ExecutionNode* newNode);

  /// @brief insert <newNode> as a new (the first!) dependency of
  /// <oldNode> and make the former first dependency of <oldNode> a
  /// dependency of <newNode> (and no longer a direct dependency of
  /// <oldNode>).
  /// <newNode> must be registered with the plan before this method is called.
  void insertDependency(ExecutionNode* oldNode, ExecutionNode* newNode);

  /// @brief insert node directly after previous
  /// will remove previous as a dependency from its parents and
  /// add newNode as a dependency. <newNode> must be registered with the plan
  void insertAfter(ExecutionNode* previous, ExecutionNode* newNode);

  /// @brief insert node directly before current
  void insertBefore(ExecutionNode* current, ExecutionNode* newNode);

  /// @brief get ast
  Ast* getAst() const noexcept { return _ast; }

  /// @brief resolves a variable alias, e.g. fn(tmp) -> "a.b" for the following:
  ///  LET tmp = a.b
  ///  LET x = tmp
  AstNode const* resolveVariableAlias(AstNode const* node) const;

  /// @brief creates an anonymous calculation node for an arbitrary expression
  ExecutionNode* createTemporaryCalculation(AstNode const*, ExecutionNode*);

  /// @brief create an execution plan from an abstract syntax tree node
  ExecutionNode* fromNode(AstNode const*);

  /// @brief create an execution plan from VPack
  ExecutionNode* fromSlice(velocypack::Slice const& slice);

  /// @brief whether or not the plan contains at least one node of this type
  bool contains(ExecutionNode::NodeType) const;

  /// @brief increase the node counter for the type
  void increaseCounter(ExecutionNode const& node) noexcept;

  bool fullCount() const noexcept;

  /// @brief parses modification options from an AST node
  static ModificationOptions parseModificationOptions(
      QueryContext& query, std::string_view operationNode, AstNode const* node,
      bool addWarnings);

  /// @brief registers a warning for an invalid OPTIONS attribute
  static void invalidOptionAttribute(QueryContext& query,
                                     std::string_view errorReason,
                                     std::string_view operationName,
                                     std::string_view name);

 private:
  template<WalkerUniqueness U>
  /// @brief find nodes of certain types
  void findNodesOfType(containers::SmallVector<ExecutionNode*, 8>& result,
                       std::initializer_list<ExecutionNode::NodeType> const&,
                       bool enterSubqueries) const;

  /// @brief creates a calculation node
  ExecutionNode* createCalculation(Variable*, AstNode const*, ExecutionNode*);

  /// @brief get the subquery node from an expression
  /// this will return a nullptr if the expression does not refer to a subquery
  SubqueryNode* getSubqueryFromExpression(AstNode const*) const;

  /// @brief get the output variable from a node
  Variable const* getOutVariable(ExecutionNode const*) const;

  /// @brief creates an anonymous COLLECT node (for a DISTINCT)
  CollectNode* createAnonymousCollect(CalculationNode const*);

  /// @brief create modification options by parsing an AST node
  /// and adding plan specific options.
  ModificationOptions createModificationOptions(std::string_view operationName,
                                                AstNode const*);

  /// @brief create COLLECT options from an AST node
  CollectOptions createCollectOptions(AstNode const*);

  /// @brief adds "previous" as dependency to "plan", returns "plan"
  ExecutionNode* addDependency(ExecutionNode*, ExecutionNode*);

  /// @brief create an execution plan element from an AST FOR (non-view) node
  ExecutionNode* fromNodeFor(ExecutionNode*, AstNode const*);

  /// @brief create an execution plan element from an AST FOR (view) node
  ExecutionNode* fromNodeForView(ExecutionNode*, AstNode const*);

  /// @brief create an execution plan element from an AST TRAVERAL node
  ExecutionNode* fromNodeTraversal(ExecutionNode*, AstNode const*);

  /// @brief create an execution plan element from an AST SHORTEST PATH node
  ExecutionNode* fromNodeShortestPath(ExecutionNode*, AstNode const*);

  /// @brief create an execution plan element from an AST K-SHORTEST PATHS node
  ExecutionNode* fromNodeEnumeratePaths(ExecutionNode*, AstNode const*);

  /// @brief create an execution plan element from an AST FILTER node
  ExecutionNode* fromNodeFilter(ExecutionNode*, AstNode const*);

  /// @brief create an execution plan element from an AST LET node
  ExecutionNode* fromNodeLet(ExecutionNode*, AstNode const*);

  /// @brief create an execution plan element from an AST SORT node
  ExecutionNode* fromNodeSort(ExecutionNode*, AstNode const*);

  /// @brief create an execution plan element from an AST COLLECT node
  ExecutionNode* fromNodeCollect(ExecutionNode*, AstNode const*);

  /// @brief create an execution plan element from an AST LIMIT node
  ExecutionNode* fromNodeLimit(ExecutionNode*, AstNode const*);

  /// @brief create an execution plan element from an AST RETURN node
  ExecutionNode* fromNodeReturn(ExecutionNode*, AstNode const*);

  /// @brief create an execution plan element from an AST REMOVE node
  ExecutionNode* fromNodeRemove(ExecutionNode*, AstNode const*);

  /// @brief create an execution plan element from an AST INSERT node
  ExecutionNode* fromNodeInsert(ExecutionNode*, AstNode const*);

  /// @brief create an execution plan element from an AST UPDATE node
  ExecutionNode* fromNodeUpdate(ExecutionNode*, AstNode const*);

  /// @brief create an execution plan element from an AST REPLACE node
  ExecutionNode* fromNodeReplace(ExecutionNode*, AstNode const*);

  /// @brief create an execution plan element from an AST UPSERT node
  ExecutionNode* fromNodeUpsert(ExecutionNode*, AstNode const*);

  /// @brief create an execution plan element from an AST WINDOW node
  ExecutionNode* fromNodeWindow(ExecutionNode*, AstNode const*);

  /// @brief create an vertex element for graph nodes
  AstNode const* parseTraversalVertexNode(ExecutionNode*&, AstNode const*);

  std::vector<AggregateVarInfo> prepareAggregateVars(ExecutionNode** previous,
                                                     AstNode const* node);

  /// @brief map from node id to the actual node
  std::unordered_map<ExecutionNodeId, ExecutionNode*> _ids;

  /// @brief root node of the plan
  ExecutionNode* _root;

  /// @brief get the node where a variable is introduced.
  containers::FlatHashMap<VariableId, ExecutionNode*> _varSetBy;

  /// @brief which optimizer rules were applied for a plan
  std::vector<int> _appliedRules;

  /// @brief which optimizer rules were disabled for a plan
  containers::HashSet<int> _disabledRules;

  /// @brief whether or not memory usage should be tracked for this plan.
  /// note: tracking memory usage requires accessing the Ast/Query objects,
  /// which can be inherently unsafe when running within the gtest unit tests.
  bool const _trackMemoryUsage;

  /// @brief if the plan is supposed to be in a valid state
  /// this will always be true, except while a plan is handed to
  /// the optimizer while applying optimizer rules
  bool _planValid;

  /// @brief flag to indicate whether the variable usage is computed
  bool _varUsageComputed;

  // Flag there are collection nodes with forceIndexHint:true
  bool _hasForcedIndexHints{false};

  /// @brief current nesting level while building the plan
  int _nestingLevel;

  /// @brief auto-increment sequence for node ids
  ExecutionNodeId _nextId;

  /// @brief the ast
  Ast* _ast;

  /// @brief which top-level LIMIT node will get its fullCount attribute set
  ExecutionNode* _lastLimitNode;

  /// @brief a lookup map for all subqueries created
  containers::FlatHashMap<VariableId, ExecutionNode*> _subqueries;

  /// @brief these nodes will be excluded from building scatter/gather
  /// "diamonds" later
  std::unordered_set<ExecutionNode const*> _excludeFromScatterGather;

  /// @brief number of nodes used in the plan, by type
  std::array<uint32_t, ExecutionNode::MAX_NODE_TYPE_VALUE> _typeCounts;

  size_t _asyncPrefetchNodes;
};

}  // namespace aql
}  // namespace arangodb

template<typename Node, typename... Args>
Node* ::arangodb::aql::ExecutionPlan::createNode(Args&&... args) {
  auto node = std::make_unique<Node>(std::forward<Args>(args)...);
  return ExecutionNode::castTo<Node*>(registerNode(std::move(node)));
}

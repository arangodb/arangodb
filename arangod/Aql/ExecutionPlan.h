////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_EXECUTION_PLAN_H
#define ARANGOD_AQL_EXECUTION_PLAN_H 1

#include "Basics/Common.h"
#include "Aql/CollectOptions.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ModificationOptions.h"
#include "Aql/types.h"
#include "Basics/SmallVector.h"

#include <array>

namespace arangodb {
namespace velocypack {
class Slice;
}

namespace aql {
class Ast;
struct AstNode;
class CalculationNode;
class CollectNode;
class ExecutionNode;
class Query;

class ExecutionPlan {
 public:
  /// @brief create the plan
  explicit ExecutionPlan(Ast*);

  /// @brief destroy the plan, frees all assigned nodes
  ~ExecutionPlan();

 public:
  /// @brief create an execution plan from an AST
  static std::unique_ptr<ExecutionPlan> instantiateFromAst(Ast*);

  /// @brief process the list of collections in a VelocyPack
  static void getCollectionsFromVelocyPack(Ast* ast,
                                           arangodb::velocypack::Slice const);

  /// @brief create an execution plan from VelocyPack
  static ExecutionPlan* instantiateFromVelocyPack(
      Ast* ast, arangodb::velocypack::Slice const);

  /// @brief whether or not the exclusive flag is set in the write options
  static bool hasExclusiveAccessOption(AstNode const* node);

  ExecutionPlan* clone(Ast*);

  /// @brief clone the plan by recursively cloning starting from the root
  ExecutionPlan* clone();

  /// @brief create an execution plan identical to this one
  ///   keep the memory of the plan on the query object specified.
  ExecutionPlan* clone(Query const&);

  /// @brief export to VelocyPack
  std::shared_ptr<arangodb::velocypack::Builder> toVelocyPack(Ast*, bool verbose) const;

  void toVelocyPack(arangodb::velocypack::Builder&, Ast*, bool verbose) const;

  /// @brief check if the plan is empty
  inline bool empty() const { return (_root == nullptr); }

  bool isResponsibleForInitialize() const { return _isResponsibleForInitialize; }

  /// @brief note that an optimizer rule was applied
  inline void addAppliedRule(int level) { _appliedRules.emplace_back(level); }

  /// @brief get a list of all applied rules
  std::vector<std::string> getAppliedRules() const;

  /// @brief return the next value for a node id
  inline size_t nextId() { return ++_nextId; }

  /// @brief get a node by its id
  ExecutionNode* getNodeById(size_t id) const;

  /// @brief check if the node is the root node
  inline bool isRoot(ExecutionNode const* node) const { return _root == node; }

  /// @brief get the root node
  inline ExecutionNode* root() const {
    TRI_ASSERT(_root != nullptr);
    return _root;
  }

  /// @brief set the root node
  inline void root(ExecutionNode* node, bool force = false) {
    if (!force) {
      TRI_ASSERT(_root == nullptr);
    }
    _root = node;
  }

  /// @brief invalidate all cost estimations in the plan
  inline void invalidateCost() {
    TRI_ASSERT(_root != nullptr);
    _root->invalidateCost();
  }

  /// @brief get the estimated cost . . .
  CostEstimate getCost() {
    TRI_ASSERT(_root != nullptr);
    return _root->getCost();
  }

  /// @brief returns true if a plan is so simple that optimizations would
  /// probably cost more than simply executing the plan
  bool isDeadSimple() const;

/// @brief show an overview over the plan
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  void show();
#endif

  /// @brief note this node for being excluded from producing scatter/gather nodes
  void excludeFromScatterGather(ExecutionNode const* node) {
    _excludeFromScatterGather.emplace(node);
  }

  bool shouldExcludeFromScatterGather(ExecutionNode const* node) const {
    return (_excludeFromScatterGather.find(node) != _excludeFromScatterGather.end());
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
  void findNodesOfType(SmallVector<ExecutionNode*>& result,
                       ExecutionNode::NodeType,
                       bool enterSubqueries);

  /// @brief find nodes of a certain types
  void findNodesOfType(SmallVector<ExecutionNode*>& result,
      std::vector<ExecutionNode::NodeType> const&, bool enterSubqueries);

  /// @brief find all end nodes in a plan
  void findEndNodes(SmallVector<ExecutionNode*>& result,
                    bool enterSubqueries) const;

  /// @brief determine and set _varsUsedLater and _valid and _varSetBy
  void findVarUsage();

  /// @brief determine if the above are already set
  bool varUsageComputed() const;

  /// @brief determine if the above are already set
  void setVarUsageComputed() { _varUsageComputed = true; }

  /// @brief flush var usage calculation
  void clearVarUsageComputed() { _varUsageComputed = false; }

  /// @brief static analysis
  void planRegisters() { _root->planRegisters(); }

  /// @brief unlinkNodes, note that this does not delete the removed
  /// nodes and that one cannot remove the root node of the plan.
  void unlinkNodes(std::unordered_set<ExecutionNode*> const& toUnlink);

  /// @brief unlinkNode, note that this does not delete the removed
  /// node and that one cannot remove the root node of the plan.
  void unlinkNode(ExecutionNode*, bool allowUnlinkingRoot = false);

  /// @brief register a node with the plan
  ExecutionNode* registerNode(std::unique_ptr<ExecutionNode>);

  /// @brief add a node to the plan, will delete node if addition
  /// fails and throw an exception
  ExecutionNode* registerNode(ExecutionNode*);

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
  inline Ast* getAst() const { return _ast; }

  /// @brief creates an anonymous calculation node for an arbitrary expression
  ExecutionNode* createTemporaryCalculation(AstNode const*, ExecutionNode*);

  /// @brief create an execution plan from an abstract syntax tree node
  ExecutionNode* fromNode(AstNode const*);

  /// @brief create an execution plan from VPack
  ExecutionNode* fromSlice(velocypack::Slice const& slice);

  /// @brief whether or not the plan contains at least one node of this type
  bool contains(ExecutionNode::NodeType type) const;

  /// @brief increase the node counter for the type
  void increaseCounter(ExecutionNode::NodeType type) noexcept;

 private:
  /// @brief creates a calculation node
  ExecutionNode* createCalculation(Variable*, Variable const*, AstNode const*,
                                   ExecutionNode*);

  /// @brief get the subquery node from an expression
  /// this will return a nullptr if the expression does not refer to a subquery
  SubqueryNode* getSubqueryFromExpression(AstNode const*) const;

  /// @brief get the output variable from a node
  Variable const* getOutVariable(ExecutionNode const*) const;

  /// @brief creates an anonymous COLLECT node (for a DISTINCT)
  CollectNode* createAnonymousCollect(CalculationNode const*);

  /// @brief create modification options by parsing an AST node
  /// and adding plan specific options.
  ModificationOptions createModificationOptions(AstNode const*);

 public:
  /// @brief parses modification options form an AST node
  static ModificationOptions parseModificationOptions(AstNode const*);

 private:
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

  /// @brief create an execution plan element from an AST FILTER node
  ExecutionNode* fromNodeFilter(ExecutionNode*, AstNode const*);

  /// @brief create an execution plan element from an AST LET node
  ExecutionNode* fromNodeLet(ExecutionNode*, AstNode const*);

  /// @brief create an execution plan element from an AST SORT node
  ExecutionNode* fromNodeSort(ExecutionNode*, AstNode const*);

  /// @brief create an execution plan element from an AST COLLECT node
  ExecutionNode* fromNodeCollect(ExecutionNode*, AstNode const*);

  /// @brief create an execution plan element from an AST COLLECT node, COUNT
  ExecutionNode* fromNodeCollectCount(ExecutionNode*, AstNode const*);

  /// @brief create an execution plan element from an AST COLLECT node,
  /// AGGREGATE
  ExecutionNode* fromNodeCollectAggregate(ExecutionNode*, AstNode const*);

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

  /// @brief create an vertex element for graph nodes
  AstNode const* parseTraversalVertexNode(ExecutionNode*&, AstNode const*);

 private:
  /// @brief map from node id to the actual node
  std::unordered_map<size_t, ExecutionNode*> _ids;

  /// @brief root node of the plan
  ExecutionNode* _root;

  /// @brief get the node where a variable is introducted.
  std::unordered_map<VariableId, ExecutionNode*> _varSetBy;

  /// @brief which optimizer rules were applied for a plan
  std::vector<int> _appliedRules;

  /// @brief flag to indicate whether the variable usage is computed
  bool _varUsageComputed;

  bool _isResponsibleForInitialize;

  /// @brief current nesting level while building the plan
  int _nestingLevel;

  /// @brief auto-increment sequence for node ids
  size_t _nextId;

  /// @brief the ast
  Ast* _ast;

  /// @brief which top-level LIMIT node will get its fullCount attribute set
  ExecutionNode* _lastLimitNode;

  /// @brief a lookup map for all subqueries created
  std::unordered_map<VariableId, ExecutionNode*> _subqueries;

  /// @brief these nodes will be excluded from building scatter/gather "diamonds" later
  std::unordered_set<ExecutionNode const*> _excludeFromScatterGather;

  /// @brief number of nodes used in the plan, by type
  std::array<uint32_t, ExecutionNode::MAX_NODE_TYPE_VALUE> _typeCounts;
};
}
}

#endif

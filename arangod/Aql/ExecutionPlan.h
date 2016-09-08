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
#include "Aql/Query.h"
#include "Aql/types.h"
#include "Basics/SmallVector.h"

namespace arangodb {
namespace aql {

class Ast;
struct AstNode;
class CalculationNode;
class CollectNode;
class ExecutionNode;

class ExecutionPlan {
 public:
  /// @brief create the plan
  explicit ExecutionPlan(Ast*);

  /// @brief destroy the plan, frees all assigned nodes
  ~ExecutionPlan();

 public:
  /// @brief create an execution plan from an AST
  static ExecutionPlan* instantiateFromAst(Ast*);

  /// @brief process the list of collections in a VelocyPack
  static void getCollectionsFromVelocyPack(Ast* ast,
                                           arangodb::velocypack::Slice const);

  /// @brief create an execution plan from VelocyPack
  static ExecutionPlan* instantiateFromVelocyPack(
      Ast* ast, arangodb::velocypack::Slice const);

  /// @brief clone the plan by recursively cloning starting from the root
  ExecutionPlan* clone();

  /// @brief create an execution plan identical to this one
  ///   keep the memory of the plan on the query object specified.
  ExecutionPlan* clone(Query const&);
  
  /// @brief export to VelocyPack
  std::shared_ptr<arangodb::velocypack::Builder> toVelocyPack(Ast*, bool) const;
  
  void toVelocyPack(arangodb::velocypack::Builder&, Ast*, bool) const;

  /// @brief check if the plan is empty
  inline bool empty() const { return (_root == nullptr); }

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
    return _root->invalidateCost();
  }

  /// @brief get the estimated cost . . .
  inline double getCost() {
    TRI_ASSERT(_root != nullptr);
    size_t nrItems;
    return _root->getCost(nrItems);
  }

  /// @brief returns true if a plan is so simple that optimizations would
  /// probably cost more than simply executing the plan
  bool isDeadSimple() const;

/// @brief show an overview over the plan
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  void show();
#endif

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

/// @brief check linkage
#if 0
  void checkLinkage();
#endif

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
  void unlinkNode(ExecutionNode*, bool = false);

  /// @brief add a node to the plan, will delete node if addition fails and
  /// throw an exception, in addition, the pointer is set to nullptr such
  /// that another delete does not hurt
  ExecutionNode* registerNode(ExecutionNode*);

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

  /// @brief get ast
  inline Ast* getAst() const { return _ast; }

  /// @brief creates an anonymous calculation node for an arbitrary expression
  ExecutionNode* createTemporaryCalculation(AstNode const*, ExecutionNode*);

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

  /// @brief create modification options from an AST node
  ModificationOptions createModificationOptions(AstNode const*);

  /// @brief create COLLECT options from an AST node
  CollectOptions createCollectOptions(AstNode const*);

  /// @brief adds "previous" as dependency to "plan", returns "plan"
  ExecutionNode* addDependency(ExecutionNode*, ExecutionNode*);

  /// @brief create an execution plan element from an AST FOR node
  ExecutionNode* fromNodeFor(ExecutionNode*, AstNode const*);

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

  /// @brief create an execution plan from an abstract syntax tree node
  ExecutionNode* fromNode(AstNode const*);

  /// @brief create an execution plan from VPack
  ExecutionNode* fromSlice(VPackSlice const& slice);

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

  /// @brief auto-increment sequence for node ids
  size_t _nextId;

  /// @brief the ast
  Ast* _ast;

  /// @brief whether or not the next LIMIT node will get its fullCount attribute
  /// set
  ExecutionNode* _lastLimitNode;

  /// @brief a lookup map for all subqueries created
  std::unordered_map<VariableId, ExecutionNode*> _subqueries;
};
}
}

#endif

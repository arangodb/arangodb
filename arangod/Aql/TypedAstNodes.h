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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/AstNode.h"
#include "Aql/Variable.h"
#include "Aql/Quantifier.h"
#include "Graph/PathType.h"

#include <span>

namespace arangodb::aql {
struct Function;
}

namespace arangodb::aql::ast {

struct TypedAstNode {
  explicit TypedAstNode(AstNode const* node) : _node(node) {
    TRI_ASSERT(_node != nullptr);
  }

 protected:
  AstNode const* _node;
};

struct ForNode : TypedAstNode {
  explicit ForNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_FOR) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 3)
        << "expected 3 members in NODE_TYPE_FOR, found " << node->numMembers();
  }

  Variable const* getVariable() const {
    auto n = _node->getMember(0);
    TRI_ASSERT(n && n->type == NODE_TYPE_VARIABLE);
    return static_cast<Variable const*>(n->getData());
  }

  AstNode const* getExpression() const { return _node->getMember(1); }

  AstNode const* getOptions() const { return _node->getMember(2); }
};

struct ForViewNode : TypedAstNode {
  explicit ForViewNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_FOR_VIEW) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 4)
        << "expected 4 members in NODE_TYPE_FOR_VIEW, found "
        << node->numMembers();
  }

  Variable const* getVariable() const {
    auto n = _node->getMember(0);
    TRI_ASSERT(n && n->type == NODE_TYPE_VARIABLE);
    return static_cast<Variable const*>(n->getData());
  }

  AstNode const* getExpression() const { return _node->getMember(1); }

  AstNode const* getSearch() const { return _node->getMember(2); }

  AstNode const* getOptions() const { return _node->getMember(3); }
};

struct LetNode : TypedAstNode {
  explicit LetNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_LET) << node->getTypeString();
    TRI_ASSERT(node->numMembers() >= 2)
        << "expected at least 2 members in NODE_TYPE_LET, found "
        << node->numMembers();
  }

  Variable* getVariable() const {
    auto n = _node->getMember(0);
    TRI_ASSERT(n && n->type == NODE_TYPE_VARIABLE);
    return static_cast<Variable*>(n->getData());
  }

  AstNode const* getExpression() const { return _node->getMember(1); }
};

struct FilterNode : TypedAstNode {
  explicit FilterNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_FILTER) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 1)
        << "expected 1 member in NODE_TYPE_FILTER, found "
        << node->numMembers();
  }

  AstNode const* getExpression() const { return _node->getMember(0); }
};

struct ReturnNode : TypedAstNode {
  explicit ReturnNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_RETURN) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 1)
        << "expected 1 member in NODE_TYPE_RETURN, found "
        << node->numMembers();
  }

  AstNode const* getExpression() const { return _node->getMember(0); }
};

struct RemoveNode : TypedAstNode {
  explicit RemoveNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_REMOVE) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 4)
        << "expected 4 members in NODE_TYPE_REMOVE, found "
        << node->numMembers();
  }
  AstNode const* getOptions() const { return _node->getMember(0); }

  AstNode const* getCollection() const { return _node->getMember(1); }

  AstNode const* getExpression() const { return _node->getMember(2); }

  AstNode const* getOldVariable() const { return _node->getMember(3); }
};

struct InsertNode : TypedAstNode {
  explicit InsertNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_INSERT) << node->getTypeString();
    TRI_ASSERT(node->numMembers() >= 4)
        << "expected at least 4 members in NODE_TYPE_INSERT, found "
        << node->numMembers();
    TRI_ASSERT(node->numMembers() <= 5)
        << "expected at most 5 members in NODE_TYPE_INSERT, found "
        << node->numMembers();
  }

  AstNode const* getOptions() const { return _node->getMember(0); }

  AstNode const* getCollection() const { return _node->getMember(1); }

  AstNode const* getExpression() const { return _node->getMember(2); }

  AstNode const* getNewVariable() const { return _node->getMember(3); }

  bool hasOldVariable() const { return _node->numMembers() == 5; }

  AstNode const* getOldVariable() const {
    return hasOldVariable() ? _node->getMember(4) : nullptr;
  }
};

struct UpdateNode : TypedAstNode {
  explicit UpdateNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_UPDATE) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 6)
        << "expected 6 members in NODE_TYPE_UPDATE, found "
        << node->numMembers();
  }

  AstNode const* getOptions() const { return _node->getMember(0); }

  AstNode const* getCollection() const { return _node->getMember(1); }

  AstNode const* getDocumentExpression() const { return _node->getMember(2); }

  AstNode const* getKeyExpression() const { return _node->getMember(3); }

  Variable const* getOldVariable() const {
    return static_cast<Variable*>(_node->getMember(4)->getData());
  }

  Variable const* getNewVariable() const {
    return static_cast<Variable*>(_node->getMember(5)->getData());
  }
};

struct ReplaceNode : TypedAstNode {
  explicit ReplaceNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_REPLACE) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 6)
        << "expected 6 members in NODE_TYPE_REPLACE, found "
        << node->numMembers();
  }

  AstNode const* getOptions() const { return _node->getMember(0); }

  AstNode const* getCollection() const { return _node->getMember(1); }

  AstNode const* getDocumentExpression() const { return _node->getMember(2); }

  AstNode const* getKeyExpression() const { return _node->getMember(3); }

  Variable const* getOldVariable() const {
    return static_cast<Variable*>(_node->getMember(4)->getData());
  }

  Variable const* getNewVariable() const {
    return static_cast<Variable*>(_node->getMember(5)->getData());
  }
};

struct UpsertNode : TypedAstNode {
  explicit UpsertNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_UPSERT) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 7)
        << "expected 7 members in NODE_TYPE_UPSERT, found "
        << node->numMembers();
  }

  AstNode const* getOptions() const { return _node->getMember(0); }

  AstNode const* getCollection() const { return _node->getMember(1); }

  AstNode const* getDocumentExpression() const { return _node->getMember(2); }

  AstNode const* getInsertExpression() const { return _node->getMember(3); }

  AstNode const* getUpdateExpression() const { return _node->getMember(4); }

  Variable const* getOldVariable() const {
    return static_cast<Variable*>(_node->getMember(5)->getData());
  }

  Variable const* getNewVariable() const {
    return static_cast<Variable*>(_node->getMember(6)->getData());
  }

  bool canReadOwnWrites() const {
    return _node->hasFlag(AstNodeFlagType::FLAG_READ_OWN_WRITES);
  }
};

struct CollectNode : TypedAstNode {
  explicit CollectNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_COLLECT) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 6)
        << "expected 6 members in NODE_TYPE_COLLECT, found "
        << node->numMembers();
  }

  AstNode const* getOptions() const { return _node->getMember(0); }

  AstNode const* getGroups() const { return _node->getMember(1); }

  AstNode const* getAggregates() const { return _node->getMember(2); }

  AstNode const* getInto() const { return _node->getMember(3); }

  AstNode const* getIntoExpression() const { return _node->getMember(4); }

  AstNode const* getKeepVariables() const { return _node->getMember(5); }
};

struct SortNode : TypedAstNode {
  explicit SortNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_SORT) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 1)
        << "expected 1 member in NODE_TYPE_SORT, found " << node->numMembers();
  }

  AstNode const* getElements() const { return _node->getMember(0); }
};

struct SortElementNode : TypedAstNode {
  explicit SortElementNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_SORT_ELEMENT) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 2)
        << "expected 2 members in NODE_TYPE_SORT_ELEMENT, found "
        << node->numMembers();
  }

  AstNode const* getExpression() const { return _node->getMember(0); }

  AstNode const* getAscending() const { return _node->getMember(1); }
};

struct LimitNode : TypedAstNode {
  explicit LimitNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_LIMIT) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 2)
        << "expected 2 members in NODE_TYPE_LIMIT, found "
        << node->numMembers();
  }

  AstNode const* getOffset() const { return _node->getMember(0); }

  AstNode const* getCount() const { return _node->getMember(1); }
};

struct WindowNode : TypedAstNode {
  explicit WindowNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_WINDOW) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 3)
        << "expected 3 members in NODE_TYPE_WINDOW, found "
        << node->numMembers();
  }

  AstNode const* getSpec() const { return _node->getMember(0); }

  AstNode const* getRangeVar() const { return _node->getMember(1); }

  AstNode const* getAggregates() const { return _node->getMember(2); }
};

struct DistinctNode : TypedAstNode {
  explicit DistinctNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_DISTINCT) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 1)
        << "expected 1 member in NODE_TYPE_DISTINCT, found "
        << node->numMembers();
  }

  AstNode const* getExpression() const { return _node->getMember(0); }
};

struct TraversalNode : TypedAstNode {
  // the first 5 members are used by traversal internally.
  // The members 6-8, where 5 and 6 are optional, are used
  // as out variables.
  explicit TraversalNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_TRAVERSAL) << node->getTypeString();
    TRI_ASSERT(node->numMembers() >= 6)
        << "expected at least 6 members in NODE_TYPE_TRAVERSAL, found "
        << node->numMembers();
    TRI_ASSERT(node->numMembers() <= 8)
        << "expected at most 8 members in NODE_TYPE_TRAVERSAL, found "
        << node->numMembers();
  }

  AstNode const* getDirection() const { return _node->getMember(0); }

  AstNode const* getStart() const { return _node->getMember(1); }

  AstNode* getGraph() const { return _node->getMember(2); }

  AstNode* getPruneExpression() const { return _node->getMember(3); }

  AstNode const* getOptions() const { return _node->getMember(4); }

  std::span<AstNode* const> getVariables() const {
    auto& members = _node->getMemberList();
    return std::span<AstNode* const>(members.data() + 5, members.size() - 5);
  }
};

struct ShortestPathNode : TypedAstNode {
  // the first 4 members are used by shortest_path internally.
  // The members 5-6, where 6 is optional, are used
  // as out variables.
  explicit ShortestPathNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_SHORTEST_PATH) << node->getTypeString();
    TRI_ASSERT(node->numMembers() >= 6)
        << "expected at least 6 members in NODE_TYPE_SHORTEST_PATH, found "
        << node->numMembers();
    TRI_ASSERT(node->numMembers() <= 7)
        << "expected at most 7 members in NODE_TYPE_SHORTEST_PATH, found "
        << node->numMembers();
  }

  AstNode const* getDirection() const { return _node->getMember(0); }

  AstNode const* getStart() const { return _node->getMember(1); }

  AstNode const* getTarget() const { return _node->getMember(2); }

  AstNode* getGraph() const { return _node->getMember(3); }

  AstNode const* getOptions() const { return _node->getMember(4); }

  std::span<AstNode* const> getVariables() const {
    auto& members = _node->getMemberList();
    return std::span<AstNode* const>(members.data() + 5, members.size() - 5);
  }
};

struct EnumeratePathsNode : TypedAstNode {
  explicit EnumeratePathsNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_ENUMERATE_PATHS)
        << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 7)
        << "expected members in NODE_TYPE_ENUMERATE_PATHS, found "
        << node->numMembers();
  }

  graph::PathType::Type getPathType() const {
    return static_cast<graph::PathType::Type>(
        _node->getMember(0)->getIntValue());
  }

  AstNode const* getDirection() const { return _node->getMember(1); }

  AstNode const* getStart() const { return _node->getMember(2); }

  AstNode const* getTarget() const { return _node->getMember(3); }

  AstNode* getGraph() const { return _node->getMember(4); }

  AstNode const* getOptions() const { return _node->getMember(5); }

  Variable const* getVariable() const {
    auto n = _node->getMember(6);
    TRI_ASSERT(n && n->type == NODE_TYPE_VARIABLE);
    return static_cast<Variable const*>(n->getData());
  }
};

struct WithNode : TypedAstNode {
  explicit WithNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_WITH) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 1)
        << "expected 1 member in NODE_TYPE_WITH, found " << node->numMembers();
  }

  AstNode const* getCollections() const { return _node->getMember(0); }
};

struct VariableNode : TypedAstNode {
  explicit VariableNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_VARIABLE) << node->getTypeString();
  }

  Variable const* getVariable() const {
    return static_cast<Variable const*>(_node->getData());
  }
};

struct AssignNode : TypedAstNode {
  explicit AssignNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_ASSIGN) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 2)
        << "expected 2 members in NODE_TYPE_ASSIGN, found "
        << node->numMembers();
  }

  Variable const* getVariable() const {
    auto n = _node->getMember(0);
    TRI_ASSERT(n && n->type == NODE_TYPE_VARIABLE);
    return static_cast<Variable const*>(n->getData());
  }

  AstNode const* getExpression() const { return _node->getMember(1); }
};

struct UnaryOperatorNode : TypedAstNode {
  explicit UnaryOperatorNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_OPERATOR_UNARY_PLUS ||
               node->type == NODE_TYPE_OPERATOR_UNARY_MINUS ||
               node->type == NODE_TYPE_OPERATOR_UNARY_NOT)
        << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 1)
        << "expected 1 member in unary operator, found " << node->numMembers();
  }

  AstNode const* getOperand() const { return _node->getMember(0); }
};

struct UnaryArithmeticOperatorNode : UnaryOperatorNode {
  explicit UnaryArithmeticOperatorNode(AstNode const* node)
      : UnaryOperatorNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_OPERATOR_UNARY_PLUS ||
               node->type == NODE_TYPE_OPERATOR_UNARY_MINUS)
        << node->getTypeString();
  }

  bool isPlus() const { return _node->type == NODE_TYPE_OPERATOR_UNARY_PLUS; }
  bool isMinus() const { return _node->type == NODE_TYPE_OPERATOR_UNARY_MINUS; }
};

struct UnaryNotOperatorNode : UnaryOperatorNode {
  explicit UnaryNotOperatorNode(AstNode const* node) : UnaryOperatorNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_OPERATOR_UNARY_NOT)
        << node->getTypeString();
  }
};

struct BinaryOperatorNode : TypedAstNode {
  explicit BinaryOperatorNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT((node->type >= NODE_TYPE_OPERATOR_BINARY_AND &&
                node->type <= NODE_TYPE_OPERATOR_BINARY_NIN) ||
               (node->type >= NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ &&
                node->type <= NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN));
    TRI_ASSERT(node->numMembers() == 2)
        << "expected 2 members in binary operator, found "
        << node->numMembers();
  }

  AstNode* getLeft() const { return _node->getMember(0); }

  AstNode* getRight() const { return _node->getMember(1); }
};

/// @brief Logical binary operator node wrapper (AND, OR)
struct LogicalOperatorNode : BinaryOperatorNode {
  explicit LogicalOperatorNode(AstNode const* node) : BinaryOperatorNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_OPERATOR_BINARY_AND ||
               node->type == NODE_TYPE_OPERATOR_BINARY_OR)
        << node->getTypeString();
  }

  bool isAnd() const { return _node->type == NODE_TYPE_OPERATOR_BINARY_AND; }
  bool isOr() const { return _node->type == NODE_TYPE_OPERATOR_BINARY_OR; }
};

/// @brief Arithmetic binary operator node wrapper (+, -, *, /, %)
struct ArithmeticOperatorNode : BinaryOperatorNode {
  explicit ArithmeticOperatorNode(AstNode const* node)
      : BinaryOperatorNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_OPERATOR_BINARY_PLUS ||
               node->type == NODE_TYPE_OPERATOR_BINARY_MINUS ||
               node->type == NODE_TYPE_OPERATOR_BINARY_TIMES ||
               node->type == NODE_TYPE_OPERATOR_BINARY_DIV ||
               node->type == NODE_TYPE_OPERATOR_BINARY_MOD)
        << node->getTypeString();
  }

  bool isPlus() const { return _node->type == NODE_TYPE_OPERATOR_BINARY_PLUS; }
  bool isMinus() const {
    return _node->type == NODE_TYPE_OPERATOR_BINARY_MINUS;
  }
  bool isTimes() const {
    return _node->type == NODE_TYPE_OPERATOR_BINARY_TIMES;
  }
  bool isDiv() const { return _node->type == NODE_TYPE_OPERATOR_BINARY_DIV; }
  bool isMod() const { return _node->type == NODE_TYPE_OPERATOR_BINARY_MOD; }
};

/// @brief Relational binary operator node wrapper (==, !=, <, <=, >, >=, IN,
/// NOT IN)
struct RelationalOperatorNode : BinaryOperatorNode {
  explicit RelationalOperatorNode(AstNode const* node)
      : BinaryOperatorNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_OPERATOR_BINARY_EQ ||
               node->type == NODE_TYPE_OPERATOR_BINARY_NE ||
               node->type == NODE_TYPE_OPERATOR_BINARY_LT ||
               node->type == NODE_TYPE_OPERATOR_BINARY_LE ||
               node->type == NODE_TYPE_OPERATOR_BINARY_GT ||
               node->type == NODE_TYPE_OPERATOR_BINARY_GE ||
               node->type == NODE_TYPE_OPERATOR_BINARY_IN ||
               node->type == NODE_TYPE_OPERATOR_BINARY_NIN)
        << node->getTypeString();
  }

  bool isEqual() const { return _node->type == NODE_TYPE_OPERATOR_BINARY_EQ; }
  bool isNotEqual() const {
    return _node->type == NODE_TYPE_OPERATOR_BINARY_NE;
  }
  bool isLessThan() const {
    return _node->type == NODE_TYPE_OPERATOR_BINARY_LT;
  }
  bool isLessEqual() const {
    return _node->type == NODE_TYPE_OPERATOR_BINARY_LE;
  }
  bool isGreaterThan() const {
    return _node->type == NODE_TYPE_OPERATOR_BINARY_GT;
  }
  bool isGreaterEqual() const {
    return _node->type == NODE_TYPE_OPERATOR_BINARY_GE;
  }
  bool isIn() const { return _node->type == NODE_TYPE_OPERATOR_BINARY_IN; }
  bool isNotIn() const { return _node->type == NODE_TYPE_OPERATOR_BINARY_NIN; }
};

struct TernaryOperatorNode : TypedAstNode {
  explicit TernaryOperatorNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_OPERATOR_TERNARY)
        << node->getTypeString();
    TRI_ASSERT(node->numMembers() >= 2 && node->numMembers() <= 3)
        << "expected at least 2 and at most 3 members in "
           "NODE_TYPE_OPERATOR_TERNARY, found "
        << node->numMembers();
  }

  AstNode* getCondition() const { return _node->getMember(0); }

  bool hasTrueExpr() const { return _node->numMembers() == 3; }

  /// For 2-member ternary: returns the condition (same as getCondition())
  /// For 3-member ternary: returns the true expression
  AstNode* getTrueExpr() const {
    return hasTrueExpr() ? _node->getMember(1) : getCondition();
  }

  /// For 2-member ternary: returns the false expression (member 1)
  /// For 3-member ternary: returns the false expression (member 2)
  AstNode* getFalseExpr() const {
    return hasTrueExpr() ? _node->getMember(2) : _node->getMember(1);
  }
};

struct NaryOperatorNode : TypedAstNode {
  explicit NaryOperatorNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_OPERATOR_NARY_AND ||
               node->type == NODE_TYPE_OPERATOR_NARY_OR)
        << node->getTypeString();
  }

  std::span<AstNode* const> getOperands() const {
    return _node->getMemberList();
  }
};

struct SubqueryNode : TypedAstNode {
  explicit SubqueryNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_SUBQUERY) << node->getTypeString();
  }

  AstNode const* getSubquery() const { return _node->getMember(0); }
};

struct AttributeAccessNode : TypedAstNode {
  explicit AttributeAccessNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_ATTRIBUTE_ACCESS)
        << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 1)
        << "expected 1 member in NODE_TYPE_ATTRIBUTE_ACCESS, found "
        << node->numMembers();
  }

  AstNode const* getObject() const { return _node->getMember(0); }

  std::string_view getAttributeName() const { return _node->getStringView(); }
};

struct BoundAttributeAccessNode : TypedAstNode {
  explicit BoundAttributeAccessNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_BOUND_ATTRIBUTE_ACCESS)
        << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 2)
        << "expected 2 members in NODE_TYPE_BOUND_ATTRIBUTE_ACCESS, found "
        << node->numMembers();
  }

  AstNode const* getObject() const { return _node->getMember(0); }

  AstNode* getAttributeName() const { return _node->getMember(1); }
};

struct IndexedAccessNode : TypedAstNode {
  explicit IndexedAccessNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_INDEXED_ACCESS) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 2)
        << "expected 2 members in NODE_TYPE_INDEXED_ACCESS, found "
        << node->numMembers();
  }

  AstNode const* getObject() const { return _node->getMember(0); }

  AstNode const* getIndex() const { return _node->getMember(1); }
};

struct IteratorNode : TypedAstNode {
  explicit IteratorNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_ITERATOR) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 2)
        << "expected 2 members in NODE_TYPE_ITERATOR, found "
        << node->numMembers();
  }

  Variable const* getVariable() const {
    auto n = _node->getMember(0);
    TRI_ASSERT(n && n->type == NODE_TYPE_VARIABLE);
    return static_cast<Variable const*>(n->getData());
  }

  AstNode const* getExpression() const { return _node->getMember(1); }
};

struct ExpansionNode : TypedAstNode {
  explicit ExpansionNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_EXPANSION) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 5)
        << "expected 5 members in NODE_TYPE_EXPANSION, found "
        << node->numMembers();
  }

  int64_t getLevels() const { return _node->getIntValue(); }

  IteratorNode getIterator() const { return IteratorNode(_node->getMember(0)); }

  AstNode const* getExpression() const { return _node->getMember(1); }

  AstNode const* getFilter() const { return _node->getMember(2); }

  AstNode const* getProjection() const { return _node->getMember(3); }

  AstNode const* getOptions() const { return _node->getMember(4); }
};

struct ValueNode : TypedAstNode {
  explicit ValueNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_VALUE) << node->getTypeString();
  }

  AstNodeValueType getValueType() const { return _node->value.type; }

  bool isNullValue() const { return getValueType() == VALUE_TYPE_NULL; }

  bool isBoolValue() const { return getValueType() == VALUE_TYPE_BOOL; }

  bool isIntValue() const { return getValueType() == VALUE_TYPE_INT; }

  bool isDoubleValue() const { return getValueType() == VALUE_TYPE_DOUBLE; }

  bool isStringValue() const { return getValueType() == VALUE_TYPE_STRING; }

  uint32_t getStringLength() const { return _node->getStringLength(); }

  bool getBoolValue() const { return _node->getBoolValue(); }

  int64_t getIntValue() const { return _node->getIntValue(); }

  double getDoubleValue() const { return _node->getDoubleValue(); }

  std::string_view getStringValue() const { return _node->getStringView(); }
};

struct ArrayNode : TypedAstNode {
  explicit ArrayNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_ARRAY) << node->getTypeString();
  }

  std::span<AstNode* const> getElements() const {
    return _node->getMemberList();
  }
};

struct ObjectNode : TypedAstNode {
  explicit ObjectNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_OBJECT) << node->getTypeString();
  }

  std::span<AstNode* const> getElements() const {
    return _node->getMemberList();
  }
};

struct ObjectElementNode : TypedAstNode {
  explicit ObjectElementNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_OBJECT_ELEMENT) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 1)
        << "expected 1 member in NODE_TYPE_OBJECT_ELEMENT, found "
        << node->numMembers();
  }

  std::string_view getAttributeName() const { return _node->getStringView(); }

  AstNode* getValue() const { return _node->getMember(0); }
};

struct CalculatedObjectElementNode : TypedAstNode {
  explicit CalculatedObjectElementNode(AstNode const* node)
      : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_CALCULATED_OBJECT_ELEMENT)
        << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 2)
        << "expected 2 members in NODE_TYPE_CALCULATED_OBJECT_ELEMENT, found "
        << node->numMembers();
  }

  AstNode const* getKey() const { return _node->getMember(0); }

  AstNode const* getValue() const { return _node->getMember(1); }
};

struct CollectionNode : TypedAstNode {
  explicit CollectionNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_COLLECTION) << node->getTypeString();
  }

  std::string_view getCollectionName() const { return _node->getStringView(); }
};

struct ViewNode : TypedAstNode {
  explicit ViewNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_VIEW) << node->getTypeString();
  }

  std::string_view getViewName() const { return _node->getStringView(); }
};

struct ReferenceNode : TypedAstNode {
  explicit ReferenceNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_REFERENCE) << node->getTypeString();
  }

  Variable const* getVariable() const {
    return static_cast<Variable const*>(_node->getData());
  }
};

struct ParameterNode : TypedAstNode {
  explicit ParameterNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_PARAMETER) << node->getTypeString();
  }

  std::string_view getParameterName() const { return _node->getStringView(); }
};

struct ParameterDatasourceNode : TypedAstNode {
  explicit ParameterDatasourceNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_PARAMETER_DATASOURCE)
        << node->getTypeString();
  }

  std::string_view getParameterName() const { return _node->getStringView(); }
};

struct FunctionCallNode : TypedAstNode {
  explicit FunctionCallNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_FCALL ||
               node->type == NODE_TYPE_FCALL_USER)
        << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 1)
        << "expected 1 member in function call, found " << node->numMembers();
  }

  std::string_view getFunctionName() const { return _node->getStringView(); }

  AstNode* getArguments() const { return _node->getMember(0); }

  Function* getFunction() const {
    return static_cast<Function*>(_node->getData());
  }
};

struct RangeNode : TypedAstNode {
  explicit RangeNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_RANGE) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 2)
        << "expected 2 members in NODE_TYPE_RANGE, found "
        << node->numMembers();
  }

  AstNode const* getStart() const { return _node->getMember(0); }

  AstNode const* getEnd() const { return _node->getMember(1); }
};

struct ArrayLimitNode : TypedAstNode {
  explicit ArrayLimitNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_ARRAY_LIMIT) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 2)
        << "expected 2 members in NODE_TYPE_ARRAY_LIMIT, found "
        << node->numMembers();
  }

  AstNode const* getOffset() const { return _node->getMember(0); }

  AstNode const* getCount() const { return _node->getMember(1); }
};

struct ArrayFilterNode : TypedAstNode {
  explicit ArrayFilterNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_ARRAY_FILTER) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 2)
        << "expected 2 members in NODE_TYPE_ARRAY_FILTER, found "
        << node->numMembers();
  }

  AstNode const* getQuantifier() const { return _node->getMember(0); }

  AstNode const* getFilter() const { return _node->getMember(1); }
};

struct DestructuringNode : TypedAstNode {
  explicit DestructuringNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_DESTRUCTURING) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 1)
        << "expected 1 member in NODE_TYPE_DESTRUCTURING, found "
        << node->numMembers();
  }

  AstNode const* getValue() const { return _node->getMember(0); }

  bool isObjectDestructuring() const { return _node->getBoolValue(); }
};

struct CollectionListNode : TypedAstNode {
  explicit CollectionListNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_COLLECTION_LIST)
        << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 1)
        << "expected 1 member in NODE_TYPE_COLLECTION_LIST, found "
        << node->numMembers();
  }

  AstNode const* getCollections() const { return _node->getMember(0); }
};

struct DirectionNode : TypedAstNode {
  explicit DirectionNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_DIRECTION) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 2)
        << "expected 2 members in NODE_TYPE_DIRECTION, found "
        << node->numMembers();
  }

  AstNode const* getDirection() const { return _node->getMember(0); }

  AstNode const* getSteps() const { return _node->getMember(1); }
};

struct QuantifierNode : TypedAstNode {
  explicit QuantifierNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_QUANTIFIER) << node->getTypeString();
  }

  Quantifier::Type getType() const {
    return static_cast<Quantifier::Type>(_node->getIntValue(true));
  }

  bool isAll() const noexcept { return getType() == Quantifier::Type::kAll; }

  bool isAny() const noexcept { return getType() == Quantifier::Type::kAny; }

  bool isNone() const noexcept { return getType() == Quantifier::Type::kNone; }

  bool isAtLeast() const noexcept {
    return getType() == Quantifier::Type::kAtLeast;
  }

  bool hasValue() const noexcept { return _node->numMembers() == 1; }

  AstNode const* getValue() const { return _node->getMember(0); }
};

struct AggregationsNode : TypedAstNode {
  explicit AggregationsNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_AGGREGATIONS) << node->getTypeString();
  }

  ArrayNode getAggregates() const { return ArrayNode(_node->getMember(0)); }
};

struct NopNode : TypedAstNode {
  explicit NopNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_NOP) << node->getTypeString();
  }
};

struct PassthruNode : TypedAstNode {
  explicit PassthruNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_PASSTHRU) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 1)
        << "expected 1 member in NODE_TYPE_PASSTHRU, found "
        << node->numMembers();
  }

  AstNode* getWrappedNode() const { return _node->getMember(0); }
};

}  // namespace arangodb::aql::ast

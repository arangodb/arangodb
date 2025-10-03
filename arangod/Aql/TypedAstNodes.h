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

#include <span>

namespace arangodb::aql {
struct Function;
}

namespace arangodb::aql::ast {

/// @brief Base struct for immutable typed AST node wrappers
struct TypedAstNode {
  explicit TypedAstNode(AstNode const* node) : _node(node) {
    TRI_ASSERT(_node != nullptr);
  }

 protected:
  AstNode const* _node;
};

/// @brief FOR loop node wrapper
struct ForNode : TypedAstNode {
  explicit ForNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_FOR) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 3)
        << "expected 3 members in NODE_TYPE_FOR, found " << node->numMembers();
  }

  /// @brief Get the loop variable
  Variable const* getVariable() const {
    auto n = _node->getMember(0);
    TRI_ASSERT(n && n->type == NODE_TYPE_VARIABLE);
    return static_cast<Variable const*>(n->getData());
  }

  /// @brief Get the expression to iterate over
  AstNode const* getExpression() const { return _node->getMember(1); }

  /// @brief Get the options
  AstNode const* getOptions() const { return _node->getMember(2); }
};

/// @brief FOR VIEW node wrapper
struct ForViewNode : TypedAstNode {
  explicit ForViewNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_FOR_VIEW) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 4)
        << "expected 4 members in NODE_TYPE_FOR_VIEW, found "
        << node->numMembers();
  }

  /// @brief Get the loop variable
  Variable const* getVariable() const {
    auto n = _node->getMember(0);
    TRI_ASSERT(n && n->type == NODE_TYPE_VARIABLE);
    return static_cast<Variable const*>(n->getData());
  }

  /// @brief Get the expression to iterate over
  AstNode const* getExpression() const { return _node->getMember(1); }

  /// @brief Get the search expression
  AstNode const* getSearch() const { return _node->getMember(2); }

  /// @brief Get the options
  AstNode const* getOptions() const { return _node->getMember(3); }
};

/// @brief LET assignment node wrapper
struct LetNode : TypedAstNode {
  explicit LetNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_LET) << node->getTypeString();
    TRI_ASSERT(node->numMembers() >= 2)
        << "expected at least 2 members in NODE_TYPE_LET, found "
        << node->numMembers();
  }

  /// @brief Get the variable being assigned
  Variable* getVariable() const {
    auto n = _node->getMember(0);
    TRI_ASSERT(n && n->type == NODE_TYPE_VARIABLE);
    return static_cast<Variable*>(n->getData());
  }

  /// @brief Get the expression being assigned
  AstNode const* getExpression() const { return _node->getMember(1); }
};

/// @brief FILTER node wrapper
struct FilterNode : TypedAstNode {
  explicit FilterNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_FILTER) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 1)
        << "expected 1 member in NODE_TYPE_FILTER, found "
        << node->numMembers();
  }

  /// @brief Get the filter expression
  AstNode const* getExpression() const { return _node->getMember(0); }
};

/// @brief RETURN node wrapper
struct ReturnNode : TypedAstNode {
  explicit ReturnNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_RETURN) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 1)
        << "expected 1 member in NODE_TYPE_RETURN, found "
        << node->numMembers();
  }

  /// @brief Get the return expression
  AstNode const* getExpression() const { return _node->getMember(0); }
};

/// @brief REMOVE node wrapper
struct RemoveNode : TypedAstNode {
  explicit RemoveNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_REMOVE) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 4)
        << "expected 4 members in NODE_TYPE_REMOVE, found "
        << node->numMembers();
  }
  /// @brief Get the options
  AstNode const* getOptions() const { return _node->getMember(0); }

  /// @brief Get the collection
  AstNode const* getCollection() const { return _node->getMember(1); }

  /// @brief Get the expression
  AstNode const* getExpression() const { return _node->getMember(2); }

  /// @brief Get the OLD variable
  AstNode const* getOldVariable() const { return _node->getMember(3); }
};

/// @brief INSERT node wrapper
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

  /// @brief Get the options
  AstNode const* getOptions() const { return _node->getMember(0); }

  /// @brief Get the collection
  AstNode const* getCollection() const { return _node->getMember(1); }

  /// @brief Get the document expression
  AstNode const* getExpression() const { return _node->getMember(2); }

  /// @brief Get the NEW variable
  AstNode const* getNewVariable() const { return _node->getMember(3); }

  bool hasOldVariable() const { return _node->numMembers() == 5; }

  /// @brief Get the OLD variable (only present if returnOld is true)
  AstNode const* getOldVariable() const {
    return hasOldVariable() ? _node->getMember(4) : nullptr;
  }
};

/// @brief UPDATE node wrapper
struct UpdateNode : TypedAstNode {
  explicit UpdateNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_UPDATE) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 6)
        << "expected 6 members in NODE_TYPE_UPDATE, found "
        << node->numMembers();
  }

  /// @brief Get the options
  AstNode const* getOptions() const { return _node->getMember(0); }

  /// @brief Get the collection
  AstNode const* getCollection() const { return _node->getMember(1); }

  /// @brief Get the document expression
  AstNode const* getDocumentExpression() const { return _node->getMember(2); }

  /// @brief Get the key expression
  AstNode const* getKeyExpression() const { return _node->getMember(3); }

  /// @brief Get the OLD variable
  Variable const* getOldVariable() const {
    return static_cast<Variable*>(_node->getMember(4)->getData());
  }

  /// @brief Get the NEW variable
  Variable const* getNewVariable() const {
    return static_cast<Variable*>(_node->getMember(5)->getData());
  }
};

/// @brief REPLACE node wrapper
struct ReplaceNode : TypedAstNode {
  explicit ReplaceNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_REPLACE) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 6)
        << "expected 6 members in NODE_TYPE_REPLACE, found "
        << node->numMembers();
  }

  /// @brief Get the options
  AstNode const* getOptions() const { return _node->getMember(0); }

  /// @brief Get the collection
  AstNode const* getCollection() const { return _node->getMember(1); }

  /// @brief Get the document expression
  AstNode const* getDocumentExpression() const { return _node->getMember(2); }

  /// @brief Get the key expression
  AstNode const* getKeyExpression() const { return _node->getMember(3); }

  /// @brief Get the OLD variable
  Variable const* getOldVariable() const {
    return static_cast<Variable*>(_node->getMember(4)->getData());
  }

  /// @brief Get the NEW variable
  Variable const* getNewVariable() const {
    return static_cast<Variable*>(_node->getMember(5)->getData());
  }
};

/// @brief UPSERT node wrapper
struct UpsertNode : TypedAstNode {
  explicit UpsertNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_UPSERT) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 7)
        << "expected 7 members in NODE_TYPE_UPSERT, found "
        << node->numMembers();
  }

  /// @brief Get the options
  AstNode const* getOptions() const { return _node->getMember(0); }

  /// @brief Get the collection
  AstNode const* getCollection() const { return _node->getMember(1); }

  /// @brief Get the document expression
  AstNode const* getDocumentExpression() const { return _node->getMember(2); }

  /// @brief Get the insert expression
  AstNode const* getInsertExpression() const { return _node->getMember(3); }

  /// @brief Get the update expression
  AstNode const* getUpdateExpression() const { return _node->getMember(4); }

  /// @brief Get the OLD variable
  Variable const* getOldVariable() const {
    return static_cast<Variable*>(_node->getMember(5)->getData());
  }

  /// @brief Get the NEW variable
  Variable const* getNewVariable() const {
    return static_cast<Variable*>(_node->getMember(6)->getData());
  }

  /// @brief Check if can read own writes
  bool canReadOwnWrites() const {
    return _node->hasFlag(AstNodeFlagType::FLAG_READ_OWN_WRITES);
  }
};

/// @brief COLLECT node wrapper
struct CollectNode : TypedAstNode {
  explicit CollectNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_COLLECT) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 6)
        << "expected 6 members in NODE_TYPE_COLLECT, found "
        << node->numMembers();
  }

  /// @brief Get the options
  AstNode const* getOptions() const { return _node->getMember(0); }

  /// @brief Get the group variables
  AstNode const* getGroups() const { return _node->getMember(1); }

  /// @brief Get the aggregates
  AstNode const* getAggregates() const { return _node->getMember(2); }

  /// @brief Get the INTO target
  AstNode const* getInto() const { return _node->getMember(3); }

  /// @brief Get the INTO expression
  AstNode const* getIntoExpression() const { return _node->getMember(4); }

  /// @brief Get the KEEP variables
  AstNode const* getKeepVariables() const { return _node->getMember(5); }
};

/// @brief SORT node wrapper
struct SortNode : TypedAstNode {
  explicit SortNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_SORT) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 1)
        << "expected 1 member in NODE_TYPE_SORT, found " << node->numMembers();
  }

  /// @brief Get the sort elements
  AstNode const* getElements() const { return _node->getMember(0); }
};

/// @brief SORT ELEMENT node wrapper
struct SortElementNode : TypedAstNode {
  explicit SortElementNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_SORT_ELEMENT) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 2)
        << "expected 2 members in NODE_TYPE_SORT_ELEMENT, found "
        << node->numMembers();
  }

  /// @brief Get the sort expression
  AstNode const* getExpression() const { return _node->getMember(0); }

  /// @brief Get the sort direction (true = ascending)
  AstNode const* getAscending() const { return _node->getMember(1); }
};

/// @brief LIMIT node wrapper
struct LimitNode : TypedAstNode {
  explicit LimitNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_LIMIT) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 2)
        << "expected 2 members in NODE_TYPE_LIMIT, found "
        << node->numMembers();
  }

  /// @brief Get the offset
  AstNode const* getOffset() const { return _node->getMember(0); }

  /// @brief Get the count
  AstNode const* getCount() const { return _node->getMember(1); }
};

/// @brief WINDOW node wrapper
struct WindowNode : TypedAstNode {
  explicit WindowNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_WINDOW) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 3)
        << "expected 3 members in NODE_TYPE_WINDOW, found "
        << node->numMembers();
  }

  /// @brief Get the window specification
  AstNode const* getSpec() const { return _node->getMember(0); }

  /// @brief Get the range variable
  AstNode const* getRangeVar() const { return _node->getMember(1); }

  /// @brief Get the aggregates
  AstNode const* getAggregates() const { return _node->getMember(2); }
};

/// @brief DISTINCT node wrapper
struct DistinctNode : TypedAstNode {
  explicit DistinctNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_DISTINCT) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 1)
        << "expected 1 member in NODE_TYPE_DISTINCT, found "
        << node->numMembers();
  }

  /// @brief Get the distinct expression
  AstNode const* getExpression() const { return _node->getMember(0); }
};

/// @brief TRAVERSAL node wrapper
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

  /// @brief Get the direction
  AstNode const* getDirection() const { return _node->getMember(0); }

  /// @brief Get the start vertex
  AstNode const* getStart() const { return _node->getMember(1); }

  /// @brief Get the graph
  AstNode* getGraph() const { return _node->getMember(2); }

  /// @brief Get the prune expression
  AstNode* getPruneExpression() const { return _node->getMember(3); }

  /// @brief Get the options
  AstNode const* getOptions() const { return _node->getMember(4); }

  /// @brief Get all output variables (1-3 variables)
  std::span<AstNode* const> getVariables() const {
    auto& members = _node->getMemberList();
    return std::span<AstNode* const>(members.data() + 5, members.size() - 5);
  }
};

/// @brief SHORTEST PATH node wrapper
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

  /// @brief Get the direction
  AstNode const* getDirection() const { return _node->getMember(0); }

  /// @brief Get the start vertex
  AstNode const* getStart() const { return _node->getMember(1); }

  /// @brief Get the target vertex
  AstNode const* getTarget() const { return _node->getMember(2); }

  /// @brief Get the graph
  AstNode* getGraph() const { return _node->getMember(3); }

  /// @brief Get the options
  AstNode const* getOptions() const { return _node->getMember(4); }

  /// @brief Get all output variables (1-2 variables)
  std::span<AstNode* const> getVariables() const {
    auto& members = _node->getMemberList();
    return std::span<AstNode* const>(members.data() + 5, members.size() - 5);
  }
};

/// @brief ENUMERATE PATHS node wrapper
struct EnumeratePathsNode : TypedAstNode {
  explicit EnumeratePathsNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_ENUMERATE_PATHS)
        << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 7)
        << "expected members in NODE_TYPE_ENUMERATE_PATHS, found "
        << node->numMembers();
  }

  /// @brief Get the path type
  int64_t getPathType() const { return _node->getMember(0)->getIntValue(); }

  /// @brief Get the direction
  AstNode const* getDirection() const { return _node->getMember(1); }

  /// @brief Get the start vertex
  AstNode const* getStart() const { return _node->getMember(2); }

  /// @brief Get the target vertex
  AstNode const* getTarget() const { return _node->getMember(3); }

  /// @brief Get the graph
  AstNode* getGraph() const { return _node->getMember(4); }

  /// @brief Get the options
  AstNode const* getOptions() const { return _node->getMember(5); }

  /// @brief Get the output variable
  Variable const* getVariable() const {
    auto n = _node->getMember(6);
    TRI_ASSERT(n && n->type == NODE_TYPE_VARIABLE);
    return static_cast<Variable const*>(n->getData());
  }
};

/// @brief WITH node wrapper
struct WithNode : TypedAstNode {
  explicit WithNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_WITH) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 1)
        << "expected 1 member in NODE_TYPE_WITH, found " << node->numMembers();
  }

  /// @brief Get the collections
  AstNode const* getCollections() const { return _node->getMember(0); }
};

/// @brief Variable node wrapper
struct VariableNode : TypedAstNode {
  explicit VariableNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_VARIABLE) << node->getTypeString();
  }

  /// @brief Get the variable as a Variable object
  Variable const* getVariable() const {
    return static_cast<Variable const*>(_node->getData());
  }
};

/// @brief Assignment node wrapper
struct AssignNode : TypedAstNode {
  explicit AssignNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_ASSIGN) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 2)
        << "expected 2 members in NODE_TYPE_ASSIGN, found "
        << node->numMembers();
  }

  /// @brief Get the variable being assigned
  Variable const* getVariable() const {
    auto n = _node->getMember(0);
    TRI_ASSERT(n && n->type == NODE_TYPE_VARIABLE);
    return static_cast<Variable const*>(n->getData());
  }

  /// @brief Get the expression being assigned
  AstNode const* getExpression() const { return _node->getMember(1); }
};

/// @brief Unary operator node wrapper
struct UnaryOperatorNode : TypedAstNode {
  explicit UnaryOperatorNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_OPERATOR_UNARY_PLUS ||
               node->type == NODE_TYPE_OPERATOR_UNARY_MINUS ||
               node->type == NODE_TYPE_OPERATOR_UNARY_NOT)
        << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 1)
        << "expected 1 member in unary operator, found " << node->numMembers();
  }

  /// @brief Get the operand
  AstNode const* getOperand() const { return _node->getMember(0); }
};

/// @brief Binary operator node wrapper
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

  /// @brief Get the left operand
  AstNode* getLeft() const { return _node->getMember(0); }

  /// @brief Get the right operand
  AstNode* getRight() const { return _node->getMember(1); }
};

/// @brief Ternary operator node wrapper
struct TernaryOperatorNode : TypedAstNode {
  explicit TernaryOperatorNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_OPERATOR_TERNARY)
        << node->getTypeString();
    TRI_ASSERT(node->numMembers() >= 2 && node->numMembers() <= 3)
        << "expected at least 2 and at most 3 members in "
           "NODE_TYPE_OPERATOR_TERNARY, found "
        << node->numMembers();
  }

  /// @brief Get the condition
  AstNode* getCondition() const { return _node->getMember(0); }

  bool hasTrueExpr() const { return _node->numMembers() == 3; }

  /// @brief Get the true expression
  /// For 2-member ternary: returns the condition (same as getCondition())
  /// For 3-member ternary: returns the true expression
  AstNode* getTrueExpr() const {
    return hasTrueExpr() ? _node->getMember(1) : getCondition();
  }

  /// @brief Get the false expression
  /// For 2-member ternary: returns the false expression (member 1)
  /// For 3-member ternary: returns the false expression (member 2)
  AstNode* getFalseExpr() const {
    return hasTrueExpr() ? _node->getMember(2) : _node->getMember(1);
  }
};

/// @brief N-ary operator node wrapper
struct NaryOperatorNode : TypedAstNode {
  explicit NaryOperatorNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_OPERATOR_NARY_AND ||
               node->type == NODE_TYPE_OPERATOR_NARY_OR)
        << node->getTypeString();
  }

  /// @brief Get all operands
  std::span<AstNode* const> getOperands() const {
    return _node->getMemberList();
  }
};

/// @brief Subquery node wrapper
struct SubqueryNode : TypedAstNode {
  explicit SubqueryNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_SUBQUERY) << node->getTypeString();
  }

  /// @brief Get the subquery AST
  AstNode const* getSubquery() const { return _node->getMember(0); }
};

/// @brief Attribute access node wrapper
struct AttributeAccessNode : TypedAstNode {
  explicit AttributeAccessNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_ATTRIBUTE_ACCESS)
        << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 1)
        << "expected 1 member in NODE_TYPE_ATTRIBUTE_ACCESS, found "
        << node->numMembers();
  }

  /// @brief Get the object being accessed
  AstNode const* getObject() const { return _node->getMember(0); }

  /// @brief Get the attribute name
  std::string_view getAttributeName() const { return _node->getStringView(); }
};

/// @brief Bound attribute access node wrapper
struct BoundAttributeAccessNode : TypedAstNode {
  explicit BoundAttributeAccessNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_BOUND_ATTRIBUTE_ACCESS)
        << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 2)
        << "expected 2 members in NODE_TYPE_BOUND_ATTRIBUTE_ACCESS, found "
        << node->numMembers();
  }

  /// @brief Get the object being accessed
  AstNode const* getObject() const { return _node->getMember(0); }

  /// @brief Get the attribute name parameter
  AstNode* getAttributeName() const { return _node->getMember(1); }
};

/// @brief Indexed access node wrapper
struct IndexedAccessNode : TypedAstNode {
  explicit IndexedAccessNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_INDEXED_ACCESS) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 2)
        << "expected 2 members in NODE_TYPE_INDEXED_ACCESS, found "
        << node->numMembers();
  }

  /// @brief Get the object being accessed
  AstNode const* getObject() const { return _node->getMember(0); }

  /// @brief Get the index expression
  AstNode const* getIndex() const { return _node->getMember(1); }
};

/// @brief Expansion node wrapper
struct ExpansionNode : TypedAstNode {
  explicit ExpansionNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_EXPANSION) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 5)
        << "expected 5 members in NODE_TYPE_EXPANSION, found "
        << node->numMembers();
  }

  /// @brief Get the levels
  int64_t getLevels() const { return _node->getIntValue(); }

  /// @brief Get the iterator
  AstNode const* getIterator() const { return _node->getMember(0); }

  /// @brief Get the expression
  AstNode const* getExpression() const { return _node->getMember(1); }

  /// @brief Get the filter
  AstNode const* getFilter() const { return _node->getMember(2); }

  /// @brief Get the projection
  AstNode const* getProjection() const { return _node->getMember(3); }

  /// @brief Get the options
  AstNode const* getOptions() const { return _node->getMember(4); }
};

/// @brief Iterator node wrapper
struct IteratorNode : TypedAstNode {
  explicit IteratorNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_ITERATOR) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 2)
        << "expected 2 members in NODE_TYPE_ITERATOR, found "
        << node->numMembers();
  }

  /// @brief Get the variable
  Variable const* getVariable() const {
    auto n = _node->getMember(0);
    TRI_ASSERT(n && n->type == NODE_TYPE_VARIABLE);
    return static_cast<Variable const*>(n->getData());
  }

  /// @brief Get the expression
  AstNode const* getExpression() const { return _node->getMember(1); }
};

/// @brief Value node wrapper
struct ValueNode : TypedAstNode {
  explicit ValueNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_VALUE) << node->getTypeString();
  }

  /// @brief Get the value type
  AstNodeValueType getValueType() const { return _node->value.type; }

  bool isNullValue() const { return getValueType() == VALUE_TYPE_NULL; }

  bool isBoolValue() const { return getValueType() == VALUE_TYPE_BOOL; }

  bool isIntValue() const { return getValueType() == VALUE_TYPE_INT; }

  bool isDoubleValue() const { return getValueType() == VALUE_TYPE_DOUBLE; }

  bool isStringValue() const { return getValueType() == VALUE_TYPE_STRING; }

  uint32_t getStringLength() const { return _node->getStringLength(); }

  /// @brief Get the bool value
  bool getBoolValue() const { return _node->getBoolValue(); }

  /// @brief Get the int value
  int64_t getIntValue() const { return _node->getIntValue(); }

  /// @brief Get the double value
  double getDoubleValue() const { return _node->getDoubleValue(); }

  /// @brief Get the string value
  std::string_view getStringValue() const { return _node->getStringView(); }
};

/// @brief Array node wrapper
struct ArrayNode : TypedAstNode {
  explicit ArrayNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_ARRAY) << node->getTypeString();
  }

  /// @brief Get all array elements
  std::span<AstNode* const> getElements() const {
    return _node->getMemberList();
  }
};

/// @brief Object node wrapper
struct ObjectNode : TypedAstNode {
  explicit ObjectNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_OBJECT) << node->getTypeString();
  }

  /// @brief Get all object elements
  std::span<AstNode* const> getElements() const {
    return _node->getMemberList();
  }
};

/// @brief Object element node wrapper
struct ObjectElementNode : TypedAstNode {
  explicit ObjectElementNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_OBJECT_ELEMENT) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 1)
        << "expected 1 member in NODE_TYPE_OBJECT_ELEMENT, found "
        << node->numMembers();
  }

  /// @brief Get the attribute name
  std::string_view getAttributeName() const { return _node->getStringView(); }

  /// @brief Get the value
  AstNode* getValue() const { return _node->getMember(0); }
};

/// @brief Calculated object element node wrapper
struct CalculatedObjectElementNode : TypedAstNode {
  explicit CalculatedObjectElementNode(AstNode const* node)
      : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_CALCULATED_OBJECT_ELEMENT)
        << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 2)
        << "expected 2 members in NODE_TYPE_CALCULATED_OBJECT_ELEMENT, found "
        << node->numMembers();
  }

  /// @brief Get the key expression
  AstNode const* getKey() const { return _node->getMember(0); }

  /// @brief Get the value expression
  AstNode const* getValue() const { return _node->getMember(1); }
};

/// @brief Collection node wrapper
struct CollectionNode : TypedAstNode {
  explicit CollectionNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_COLLECTION) << node->getTypeString();
  }

  /// @brief Get the collection name
  std::string_view getCollectionName() const { return _node->getStringView(); }
};

/// @brief View node wrapper
struct ViewNode : TypedAstNode {
  explicit ViewNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_VIEW) << node->getTypeString();
  }

  /// @brief Get the view name
  std::string_view getViewName() const { return _node->getStringView(); }
};

/// @brief Reference node wrapper
struct ReferenceNode : TypedAstNode {
  explicit ReferenceNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_REFERENCE) << node->getTypeString();
  }

  /// @brief Get the variable as a Variable object
  Variable const* getVariable() const {
    return static_cast<Variable const*>(_node->getData());
  }
};

/// @brief Parameter node wrapper
struct ParameterNode : TypedAstNode {
  explicit ParameterNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_PARAMETER) << node->getTypeString();
  }

  /// @brief Get the parameter name
  std::string_view getParameterName() const { return _node->getStringView(); }
};

/// @brief Datasource parameter node wrapper
struct ParameterDatasourceNode : TypedAstNode {
  explicit ParameterDatasourceNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_PARAMETER_DATASOURCE)
        << node->getTypeString();
  }

  /// @brief Get the parameter name
  std::string_view getParameterName() const { return _node->getStringView(); }
};

/// @brief Function call node wrapper
struct FunctionCallNode : TypedAstNode {
  explicit FunctionCallNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_FCALL ||
               node->type == NODE_TYPE_FCALL_USER)
        << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 1)
        << "expected 1 member in function call, found " << node->numMembers();
  }

  /// @brief Get the function name
  std::string_view getFunctionName() const { return _node->getStringView(); }

  /// @brief Get the arguments
  AstNode* getArguments() const { return _node->getMember(0); }

  Function* getFunction() const {
    return static_cast<Function*>(_node->getData());
  }
};

/// @brief Range node wrapper
struct RangeNode : TypedAstNode {
  explicit RangeNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_RANGE) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 2)
        << "expected 2 members in NODE_TYPE_RANGE, found "
        << node->numMembers();
  }

  /// @brief Get the start expression
  AstNode const* getStart() const { return _node->getMember(0); }

  /// @brief Get the end expression
  AstNode const* getEnd() const { return _node->getMember(1); }
};

/// @brief Array limit node wrapper
struct ArrayLimitNode : TypedAstNode {
  explicit ArrayLimitNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_ARRAY_LIMIT) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 2)
        << "expected 2 members in NODE_TYPE_ARRAY_LIMIT, found "
        << node->numMembers();
  }

  /// @brief Get the offset
  AstNode const* getOffset() const { return _node->getMember(0); }

  /// @brief Get the count
  AstNode const* getCount() const { return _node->getMember(1); }
};

/// @brief Array filter node wrapper
struct ArrayFilterNode : TypedAstNode {
  explicit ArrayFilterNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_ARRAY_FILTER) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 2)
        << "expected 2 members in NODE_TYPE_ARRAY_FILTER, found "
        << node->numMembers();
  }

  /// @brief Get the quantifier
  AstNode const* getQuantifier() const { return _node->getMember(0); }

  /// @brief Get the filter expression
  AstNode const* getFilter() const { return _node->getMember(1); }
};

/// @brief Destructuring node wrapper
struct DestructuringNode : TypedAstNode {
  explicit DestructuringNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_DESTRUCTURING) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 1)
        << "expected 1 member in NODE_TYPE_DESTRUCTURING, found "
        << node->numMembers();
  }

  /// @brief Get the value being destructured
  AstNode const* getValue() const { return _node->getMember(0); }

  /// @brief Check if this is object destructuring
  bool isObjectDestructuring() const { return _node->getBoolValue(); }
};

/// @brief Collection list node wrapper
struct CollectionListNode : TypedAstNode {
  explicit CollectionListNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_COLLECTION_LIST)
        << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 1)
        << "expected 1 member in NODE_TYPE_COLLECTION_LIST, found "
        << node->numMembers();
  }

  /// @brief Get the collections
  AstNode const* getCollections() const { return _node->getMember(0); }
};

/// @brief Direction node wrapper
struct DirectionNode : TypedAstNode {
  explicit DirectionNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_DIRECTION) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 2)
        << "expected 2 members in NODE_TYPE_DIRECTION, found "
        << node->numMembers();
  }

  /// @brief Get the direction
  AstNode const* getDirection() const { return _node->getMember(0); }

  /// @brief Get the steps
  AstNode const* getSteps() const { return _node->getMember(1); }
};

/// @brief Quantifier node wrapper
struct QuantifierNode : TypedAstNode {
  explicit QuantifierNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_QUANTIFIER) << node->getTypeString();
  }

  /// @brief Get the quantifier type
  Quantifier::Type getType() const {
    return static_cast<Quantifier::Type>(_node->getIntValue());
  }

  bool isAll() const noexcept { return getType() == Quantifier::Type::kAll; }

  bool isAny() const noexcept { return getType() == Quantifier::Type::kAny; }

  bool isNone() const noexcept { return getType() == Quantifier::Type::kNone; }

  bool isAtLeast() const noexcept {
    return getType() == Quantifier::Type::kAtLeast;
  }

  bool hasValue() const noexcept { return _node->numMembers() == 1; }

  /// @brief Get the value
  AstNode const* getValue() const { return _node->getMember(0); }
};

/// @brief Aggregations node wrapper
struct AggregationsNode : TypedAstNode {
  explicit AggregationsNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_AGGREGATIONS) << node->getTypeString();
  }

  /// @brief Get all aggregates
  ArrayNode getAggregates() const { return ArrayNode(_node->getMember(0)); }
};

/// @brief Example node wrapper
struct ExampleNode : TypedAstNode {
  explicit ExampleNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_EXAMPLE) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 2)
        << "expected 2 members in NODE_TYPE_EXAMPLE, found "
        << node->numMembers();
  }

  /// @brief Get the variable
  AstNode const* getVariable() const { return _node->getMember(0); }

  /// @brief Get the example object
  AstNode const* getExample() const { return _node->getMember(1); }
};

/// @brief NOP node wrapper
struct NopNode : TypedAstNode {
  explicit NopNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_NOP) << node->getTypeString();
  }
};

/// @brief Passthru node wrapper
struct PassthruNode : TypedAstNode {
  explicit PassthruNode(AstNode const* node) : TypedAstNode(node) {
    TRI_ASSERT(node->type == NODE_TYPE_PASSTHRU) << node->getTypeString();
    TRI_ASSERT(node->numMembers() == 1)
        << "expected 1 member in NODE_TYPE_PASSTHRU, found "
        << node->numMembers();
  }

  /// @brief Get the wrapped node
  AstNode* getWrappedNode() const { return _node->getMember(0); }
};

}  // namespace arangodb::aql::ast

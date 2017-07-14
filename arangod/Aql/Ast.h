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

#ifndef ARANGOD_AQL_AST_H
#define ARANGOD_AQL_AST_H 1

#include "Basics/Common.h"
#include "Aql/AstNode.h"
#include "Aql/BindParameters.h"
#include "Aql/Scopes.h"
#include "Aql/Variable.h"
#include "Aql/VariableGenerator.h"
#include "Transaction/Methods.h"
#include "VocBase/AccessMode.h"

#include <functional>

namespace arangodb {
namespace velocypack {
class Slice;
}

namespace aql {

class Query;
class VariableGenerator;

typedef std::unordered_map<Variable const*, std::unordered_set<std::string>>
    TopLevelAttributes;

/// @brief the AST
class Ast {
  friend class Condition;

 public:
  /// @brief create the AST
  explicit Ast(Query*);

  /// @brief destroy the AST
  ~Ast();

 public:
  /// @brief return the query
  inline Query* query() const { return _query; }

  /// @brief return the variable generator
  inline VariableGenerator* variables() { return &_variables; }
  
  /// @brief return the root of the AST
  inline AstNode const* root() const { return _root; }

  /// @brief begin a subquery
  void startSubQuery() {
    // insert a new root node
    AstNodeType type;

    if (_queries.empty()) {
      // root node of query
      type = NODE_TYPE_ROOT;
    } else {
      // sub query node
      type = NODE_TYPE_SUBQUERY;
    }

    auto root = createNode(type);

    // save the root node
    _queries.emplace_back(root);

    // set the current root node if everything went well
    _root = root;
  }

  /// @brief end a subquery
  AstNode* endSubQuery() {
    // get the current root node
    AstNode* root = _queries.back();
    // remove it from the stack
    _queries.pop_back();

    // set root node to previous root node
    _root = _queries.back();

    // return the root node we just popped from the stack
    return root;
  }

  /// @brief whether or not we currently are in a subquery
  bool isInSubQuery() const { return (_queries.size() > 1); }

  /// @brief return a copy of our own bind parameters
  std::unordered_set<std::string> bindParameters() const {
    return std::unordered_set<std::string>(_bindParameters);
  }

  /// @brief get the query scopes
  inline Scopes* scopes() { return &_scopes; }

  /// @brief track the write collection
  inline void addWriteCollection(AstNode const* node) {
    TRI_ASSERT(node->type == NODE_TYPE_COLLECTION ||
               node->type == NODE_TYPE_PARAMETER);

    _writeCollections.emplace_back(node);
  }

  /// @brief whether or not function calls may access collection documents
  bool functionsMayAccessDocuments() const {
    return _functionsMayAccessDocuments;
  }

  /// @brief whether or not the query contains a traversal
  bool containsTraversal() const { return _containsTraversal; }

  /// @brief convert the AST into VelocyPack
  std::shared_ptr<arangodb::velocypack::Builder> toVelocyPack(bool) const;

  /// @brief add an operation to the root node
  void addOperation(AstNode*);

  /// @brief find the bottom-most expansion subnodes (if any)
  AstNode const* findExpansionSubNode(AstNode const*) const;

  /// @brief create an AST passthru node
  /// note: this type of node is only used during parsing and optimized away
  /// later
  AstNode* createNodePassthru(AstNode const*);

  /// @brief create an AST example node
  AstNode* createNodeExample(AstNode const*, AstNode const*);

  /// @brief create an AST for node
  AstNode* createNodeFor(char const*, size_t, AstNode const*, bool);

  /// @brief create an AST let node, without an IF condition
  AstNode* createNodeLet(char const*, size_t, AstNode const*, bool);

  /// @brief create an AST let node, without creating a variable
  AstNode* createNodeLet(AstNode const*, AstNode const*);

  /// @brief create an AST let node, with an IF condition
  AstNode* createNodeLet(char const*, size_t, AstNode const*, AstNode const*);

  /// @brief create an AST filter node
  AstNode* createNodeFilter(AstNode const*);

  /// @brief create an AST filter node for an UPSERT query
  AstNode* createNodeUpsertFilter(AstNode const*, AstNode const*);

  /// @brief create an AST return node
  AstNode* createNodeReturn(AstNode const*);

  /// @brief create an AST remove node
  AstNode* createNodeRemove(AstNode const*, AstNode const*, AstNode const*);

  /// @brief create an AST insert node
  AstNode* createNodeInsert(AstNode const*, AstNode const*, AstNode const*);

  /// @brief create an AST update node
  AstNode* createNodeUpdate(AstNode const*, AstNode const*, AstNode const*,
                            AstNode const*);

  /// @brief create an AST replace node
  AstNode* createNodeReplace(AstNode const*, AstNode const*, AstNode const*,
                             AstNode const*);

  /// @brief create an AST upsert node
  AstNode* createNodeUpsert(AstNodeType, AstNode const*, AstNode const*,
                            AstNode const*, AstNode const*, AstNode const*);

  /// @brief create an AST distinct node
  AstNode* createNodeDistinct(AstNode const*);

  /// @brief create an AST collect node
  AstNode* createNodeCollect(AstNode const*, AstNode const*, AstNode const*,
                             AstNode const*, AstNode const*, AstNode const*);

  /// @brief create an AST collect node, COUNT INTO
  AstNode* createNodeCollectCount(AstNode const*, char const*, size_t length,
                                  AstNode const*);

  /// @brief create an AST sort node
  AstNode* createNodeSort(AstNode const*);

  /// @brief create an AST sort element node
  AstNode* createNodeSortElement(AstNode const*, AstNode const*);

  /// @brief create an AST limit node
  AstNode* createNodeLimit(AstNode const*, AstNode const*);

  /// @brief create an AST assign node
  AstNode* createNodeAssign(char const*, size_t, AstNode const*);

  /// @brief create an AST variable node
  AstNode* createNodeVariable(char const*, size_t, bool);

  /// @brief create an AST collection node
  AstNode* createNodeCollection(char const*, AccessMode::Type);

  /// @brief create an AST reference node
  AstNode* createNodeReference(char const*, size_t);

  /// @brief create an AST reference node
  AstNode* createNodeReference(std::string const&);

  /// @brief create an AST reference node
  AstNode* createNodeReference(Variable const*);

  /// @brief create an AST parameter node
  AstNode* createNodeParameter(char const*, size_t);

  /// @brief create an AST quantifier node
  AstNode* createNodeQuantifier(int64_t);

  /// @brief create an AST unary operator
  AstNode* createNodeUnaryOperator(AstNodeType, AstNode const*);

  /// @brief create an AST binary operator
  AstNode* createNodeBinaryOperator(AstNodeType, AstNode const*,
                                    AstNode const*);

  /// @brief create an AST binary array operator
  AstNode* createNodeBinaryArrayOperator(AstNodeType, AstNode const*,
                                         AstNode const*, AstNode const*);

  /// @brief create an AST ternary operator
  AstNode* createNodeTernaryOperator(AstNode const*, AstNode const*,
                                     AstNode const*);

  /// @brief create an AST attribute access node
  AstNode* createNodeAttributeAccess(AstNode const*, char const*, size_t);

  /// @brief create an AST attribute access node w/ bind parameter
  AstNode* createNodeBoundAttributeAccess(AstNode const*, AstNode const*);

  /// @brief create an AST index access node
  AstNode* createNodeIndexedAccess(AstNode const*, AstNode const*);

  /// @brief create an AST array limit node (offset, count)
  AstNode* createNodeArrayLimit(AstNode const*, AstNode const*);

  /// @brief create an AST expansion node
  AstNode* createNodeExpansion(int64_t, AstNode const*, AstNode const*,
                               AstNode const*, AstNode const*, AstNode const*);

  /// @brief create an AST iterator node
  AstNode* createNodeIterator(char const*, size_t, AstNode const*);

  /// @brief create an AST null value node
  static AstNode* createNodeValueNull();

  /// @brief create an AST bool value node
  static AstNode* createNodeValueBool(bool);

  /// @brief create an AST int value node
  AstNode* createNodeValueInt(int64_t);

  /// @brief create an AST double value node
  AstNode* createNodeValueDouble(double);

  /// @brief create an AST string value node
  AstNode* createNodeValueString(char const*, size_t);

  /// @brief create an AST array node
  AstNode* createNodeArray();

  AstNode* createNodeArray(size_t members);

  /// @brief create an AST unique array node, AND-merged from two other arrays
  AstNode* createNodeIntersectedArray(AstNode const*, AstNode const*);

  /// @brief create an AST unique array node, OR-merged from two other arrays
  AstNode* createNodeUnionizedArray(AstNode const*, AstNode const*);

  /// @brief create an AST object node
  AstNode* createNodeObject();

  /// @brief create an AST object element node
  AstNode* createNodeObjectElement(char const*, size_t, AstNode const*);

  /// @brief create an AST calculated object element node
  AstNode* createNodeCalculatedObjectElement(AstNode const*, AstNode const*);

  /// @brief create an AST collection pair node
  AstNode* createNodeCollectionPair(AstNode const*, AstNode const*);
 
  /// @brief create an AST with collections node
  AstNode* createNodeWithCollections (AstNode const*);

  /// @brief create an AST collection list node
  AstNode* createNodeCollectionList(AstNode const*);

  /// @brief create an AST direction node
  AstNode* createNodeDirection(uint64_t, uint64_t);

  /// @brief create an AST direction node
  AstNode* createNodeDirection(uint64_t, AstNode const*);

  /// @brief create an AST direction node
  AstNode* createNodeCollectionDirection(uint64_t, AstNode const*);

  /// @brief create an AST traversal node with only vertex variable
  AstNode* createNodeTraversal(char const*, size_t, AstNode const*,
                               AstNode const*, AstNode const*, AstNode const*);

  /// @brief create an AST traversal node with vertex and edge variable
  AstNode* createNodeTraversal(char const*, size_t, char const*, size_t,
                               AstNode const*, AstNode const*, AstNode const*,
                               AstNode const*);

  /// @brief create an AST traversal node with vertex, edge and path variable
  AstNode* createNodeTraversal(char const*, size_t, char const*, size_t,
                               char const*, size_t, AstNode const*,
                               AstNode const*, AstNode const*, AstNode const*);

  /// @brief create an AST shortest path node with only vertex variable
  AstNode* createNodeShortestPath(char const*, size_t, uint64_t, AstNode const*,
                                  AstNode const*, AstNode const*,
                                  AstNode const*);

  /// @brief create an AST shortest path node with vertex and edge variable
  AstNode* createNodeShortestPath(char const*, size_t, char const*, size_t,
                                  uint64_t, AstNode const*, AstNode const*,
                                  AstNode const*, AstNode const*);

  /// @brief create an AST function call node
  AstNode* createNodeFunctionCall(char const*, AstNode const*);

  /// @brief create an AST range node
  AstNode* createNodeRange(AstNode const*, AstNode const*);

  /// @brief create an AST nop node
  AstNode* createNodeNop();

  /// @brief get the AST nop node
  static AstNode* getNodeNop();

  /// @brief create an AST n-ary operator
  AstNode* createNodeNaryOperator(AstNodeType);

  /// @brief create an AST n-ary operator
  AstNode* createNodeNaryOperator(AstNodeType, AstNode const*);

  /// @brief injects bind parameters into the AST
  void injectBindParameters(BindParameters&);

  /// @brief replace variables
  AstNode* replaceVariables(
      AstNode*, std::unordered_map<VariableId, Variable const*> const&);

  /// @brief replace a variable reference in the expression with another
  /// expression (e.g. inserting c = `a + b` into expression `c + 1` so the
  /// latter
  /// becomes `a + b + 1`
  AstNode* replaceVariableReference(AstNode*, Variable const*, AstNode const*);

  /// @brief optimizes the AST
  void validateAndOptimize();

  /// @brief determines the variables referenced in an expression
  static void getReferencedVariables(AstNode const*,
                                     std::unordered_set<Variable const*>&);

  /// @brief count how many times a variable is referenced in an expression
  static size_t countReferences(AstNode const*, Variable const*);

  /// @brief determines the top-level attributes in an expression, grouped by
  /// variable
  static TopLevelAttributes getReferencedAttributes(AstNode const*, bool&);
  
  static bool populateSingleAttributeAccess(AstNode const* node,
                                            Variable const* variable,
                                            std::vector<std::string>& attributeName);

  static bool variableOnlyUsedForSingleAttributeAccess(AstNode const* node,
                                                       Variable const* variable,
                                                       std::vector<std::string> const& attributeName);
  
  /// @brief replace an attribute access with just the variable
  static AstNode* replaceAttributeAccess(AstNode* node,
                                         Variable const* variable,
                                         std::vector<std::string> const& attributeName);

  /// @brief recursively clone a node
  AstNode* clone(AstNode const*);

  /// @brief deduplicate an array
  /// will return the original node if no modifications were made, and a new
  /// node if the array contained modifications
  AstNode const* deduplicateArray(AstNode const*);

  /// @brief check if an operator is reversible
  static bool IsReversibleOperator(AstNodeType);

  /// @brief get the reversed operator for a comparison operator
  static AstNodeType ReverseOperator(AstNodeType);

  /// @brief get the n-ary operator type equivalent for a binary operator type
  static AstNodeType NaryOperatorType(AstNodeType);

  /// @brief create an AST node from vpack
  AstNode* nodeFromVPack(arangodb::velocypack::Slice const&, bool);
  
  /// @brief resolve an attribute access
  static AstNode const* resolveConstAttributeAccess(AstNode const*);

  /// @brief traverse the AST using a depth-first visitor
  static AstNode* traverseAndModify(AstNode*,
                                    std::function<AstNode*(AstNode*, void*)>,
                                    void*);

 private:
  /// @brief make condition from example
  AstNode* makeConditionFromExample(AstNode const*);

  /// @brief create a number node for an arithmetic result, integer
  AstNode* createArithmeticResultNode(int64_t);

  /// @brief create a number node for an arithmetic result, double
  AstNode* createArithmeticResultNode(double);

  /// @brief executes an expression with constant parameters
  AstNode* executeConstExpression(AstNode const*);

  /// @brief optimizes the unary operators + and -
  /// the unary plus will be converted into a simple value node if the operand
  /// of
  /// the operation is a constant number
  AstNode* optimizeUnaryOperatorArithmetic(AstNode*);

  /// @brief optimizes the unary operator NOT with a non-constant expression
  AstNode* optimizeNotExpression(AstNode*);

  /// @brief optimizes the unary operator NOT
  AstNode* optimizeUnaryOperatorLogical(AstNode*);

  /// @brief optimizes the binary logical operators && and ||
  AstNode* optimizeBinaryOperatorLogical(AstNode*, bool);

  /// @brief optimizes the binary relational operators <, <=, >, >=, ==, != and
  /// IN
  AstNode* optimizeBinaryOperatorRelational(AstNode*);

  /// @brief optimizes the binary arithmetic operators +, -, *, / and %
  AstNode* optimizeBinaryOperatorArithmetic(AstNode*);

  /// @brief optimizes the ternary operator
  AstNode* optimizeTernaryOperator(AstNode*);

  /// @brief optimizes an attribute access
  AstNode* optimizeAttributeAccess(AstNode*, std::unordered_map<Variable const*, AstNode const*> const&);

  /// @brief optimizes a call to a built-in function
  AstNode* optimizeFunctionCall(AstNode*);

  /// @brief optimizes a reference to a variable
  AstNode* optimizeReference(AstNode*);

  /// @brief optimizes indexed access, e.g. a[0] or a['foo']
  AstNode* optimizeIndexedAccess(AstNode*);

  /// @brief optimizes the LET statement
  AstNode* optimizeLet(AstNode*);

  /// @brief optimizes the FILTER statement
  AstNode* optimizeFilter(AstNode*);

  /// @brief optimizes the FOR statement
  /// no real optimizations are done here, but we do an early check if the
  /// FOR loop operand is actually a list
  AstNode* optimizeFor(AstNode*);

  /// @brief optimizes an object literal or an object expression
  AstNode* optimizeObject(AstNode*);

public:
  /// @brief traverse the AST, using pre- and post-order visitors
  static AstNode* traverseAndModify(AstNode*,
                                    std::function<bool(AstNode const*, void*)>,
                                    std::function<AstNode*(AstNode*, void*)>,
                                    std::function<void(AstNode const*, void*)>,
                                    void*);

  /// @brief traverse the AST, using pre- and post-order visitors
  static void traverseReadOnly(AstNode const*,
                               std::function<void(AstNode const*, void*)>,
                               std::function<void(AstNode const*, void*)>,
                               std::function<void(AstNode const*, void*)>,
                               void*);

  /// @brief traverse the AST using a depth-first visitor, with const nodes
  static void traverseReadOnly(AstNode const*,
                               std::function<void(AstNode const*, void*)>,
                               void*);
private:
  /// @brief normalize a function name
  std::pair<std::string, bool> normalizeFunctionName(char const*);

  /// @brief create a node of the specified type
  AstNode* createNode(AstNodeType);

 public:
  /// @brief negated comparison operators
  static std::unordered_map<int, AstNodeType> const NegatedOperators;

  /// @brief reverse comparison operators
  static std::unordered_map<int, AstNodeType> const ReversedOperators;

 private:
  /// @brief the query
  Query* _query;

  /// @brief all scopes used in the query
  Scopes _scopes;

  /// @brief generator for variables
  VariableGenerator _variables;

  /// @brief the bind parameters we found in the query
  std::unordered_set<std::string> _bindParameters;

  /// @brief root node of the AST
  AstNode* _root;

  /// @brief root nodes of queries and subqueries
  std::vector<AstNode*> _queries;

  /// @brief which collection is going to be modified in the query
  std::vector<AstNode const*> _writeCollections;

  /// @brief whether or not function calls may access collection data
  bool _functionsMayAccessDocuments;

  /// @brief whether or not the query contains a traversal
  bool _containsTraversal;

  /// @brief a singleton no-op node instance
  static AstNode const NopNode;

  /// @brief a singleton null node instance
  static AstNode const NullNode;

  /// @brief a singleton false node instance
  static AstNode const FalseNode;

  /// @brief a singleton true node instance
  static AstNode const TrueNode;

  /// @brief a singleton zero node instance
  static AstNode const ZeroNode;

  /// @brief a singleton empty string node instance
  static AstNode const EmptyStringNode;
};
}
}

#endif

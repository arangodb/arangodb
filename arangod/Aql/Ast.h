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

#include <functional>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <velocypack/Builder.h>

#include "Aql/AstNode.h"
#include "Aql/AstResources.h"
#include "Aql/AttributeNamePath.h"
#include "Aql/Quantifier.h"
#include "Aql/Scopes.h"
#include "Aql/VariableGenerator.h"
#include "Aql/types.h"
#include "Basics/AttributeNameParser.h"
#include "Basics/ResourceUsage.h"
#include "Containers/FlatHashMap.h"
#include "Containers/FlatHashSet.h"
#include "Containers/SmallVector.h"
#include "Graph/PathType.h"
#include "VocBase/AccessMode.h"

namespace arangodb {
class CollectionNameResolver;

namespace transaction {
class Methods;
}

namespace velocypack {
class Slice;
}

namespace aql {
class BindParameters;
class QueryContext;
class AqlFunctionsInternalCache;
struct Variable;

/// @brief type for Ast flags
using AstPropertiesFlagsType = uint32_t;

/// @brief flags for customizing Ast building process
enum AstPropertyFlag : AstPropertiesFlagsType {
  AST_FLAG_DEFAULT = 0x00000000,     // Regular Ast
  NON_CONST_PARAMETERS = 0x00000001  // Parameters are considered non-const
};

/// @brief the AST
class Ast {
  friend class Condition;

 public:
  Ast(Ast const&) = delete;
  Ast& operator=(Ast const&) = delete;

  /// @brief create the AST
  explicit Ast(QueryContext&, AstPropertiesFlagsType flags =
                                  AstPropertyFlag::AST_FLAG_DEFAULT);

  /// @brief destroy the AST
  ~Ast();

  struct ValidateAndOptimizeOptions {
    // try to optimize non-cacheable expressions as well. this is true for
    // most use cases, but we cannot activate this for computed values, as
    // we need a freshly calculated value every time.
    bool optimizeNonCacheable = true;
    // whether or not to optimize function calls
    bool optimizeFunctionCalls = true;
  };

  // frees all data
  void clear() noexcept;

  // frees most data (keeps a bit of memory around to avoid later
  // re-allocations)
  void clearMost() noexcept;

  /// @brief maximum nesting level for expressions
  static constexpr uint64_t maxExpressionNesting = 500;

  /// @brief return the query
  QueryContext const& query() const noexcept { return _query; }
  QueryContext& query() noexcept { return _query; }

  AstResources const& resources() const noexcept { return _resources; }
  AstResources& resources() noexcept { return _resources; }

  /// @brief return the variable generator
  VariableGenerator* variables();
  VariableGenerator const* variables() const;

  /// @brief return the root of the AST
  AstNode const* root() const;

  /// @brief begin a subquery
  void startSubQuery();

  /// @brief end a subquery
  AstNode* endSubQuery();

  /// @brief whether or not we currently are in a subquery
  bool isInSubQuery() const noexcept;

  /// @brief return a copy of our own bind parameters
  std::unordered_set<std::string> bindParameterNames() const;

  BindParameterVariableMapping bindParameterVariables() const;

  /// @brief get the query scopes
  Scopes* scopes();

  /// @brief track the write collection
  void addWriteCollection(AstNode const* node, bool isExclusiveAccess);

  /// @brief whether or not function calls may access collection documents
  bool functionsMayAccessDocuments() const noexcept;

  /// @brief whether or not the query contains a traversal
  bool containsTraversal() const noexcept;

  /// @brief whether or not query contains any modification operations
  bool containsModificationNode() const noexcept;
  void setContainsModificationNode() noexcept;
  bool containsUpsertNode() const noexcept;
  void setContainsUpsertNode() noexcept;
  void setContainsParallelNode() noexcept;
  bool containsAsyncPrefetch() const noexcept;
  void setContainsAsyncPrefetch() noexcept;

  bool canApplyParallelism() const noexcept {
    return _containsParallelNode && !_willUseV8 && !_containsModificationNode;
  }

  /// @brief convert the AST into VelocyPack
  void toVelocyPack(velocypack::Builder& builder, bool verbose) const;

  /// @brief add an operation to the root node
  void addOperation(AstNode*);

  /// @brief find the bottom-most expansion subnodes (if any)
  AstNode const* findExpansionSubNode(AstNode const*) const;

  /// @brief create a node from the velocypack data
  AstNode* createNode(velocypack::Slice slice);

  /// @brief create an AST passthru node
  /// note: this type of node is only used during parsing and optimized away
  /// later
  AstNode* createNodePassthru(AstNode const*);

  /// @brief create an AST example node
  AstNode* createNodeExample(AstNode const*, AstNode const*);

  /// @brief create an AST subquery node
  AstNode* createNodeSubquery();

  /// @brief create an AST for node as part of an UPSERT
  AstNode* createNodeForUpsert(char const* variableName, size_t nameLength,
                               AstNode const* expression,
                               bool isUserDefinedVariable);

  /// @brief create an AST for (non-view) node, using an existing output
  /// variable
  AstNode* createNodeFor(Variable* variable, AstNode const* expression,
                         AstNode const* options);

  /// @brief create an AST for (view) node, using an existing out variable
  AstNode* createNodeForView(Variable* variable, AstNode const* expression,
                             AstNode const* search, AstNode const* options);

  /// @brief create an AST let node, without an IF condition
  AstNode* createNodeLet(char const*, size_t, AstNode const*, bool);

  /// @brief create an AST let node, without creating a variable
  AstNode* createNodeLet(AstNode const*, AstNode const*);

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
  AstNode* createNodeUpsert(AstNodeType type, AstNode const* docVariable,
                            AstNode const* insertExpression,
                            AstNode const* updateExpression,
                            AstNode const* collection, AstNode const* options,
                            bool canReadOwnWrites);

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

  /// @brief create an AST window node
  AstNode* createNodeWindow(AstNode const* spec, AstNode const* rangeVar,
                            AstNode const* assignments);

  /// @brief create an AST assign node
  AstNode* createNodeAssign(char const*, size_t, AstNode const*);

  /// @brief create an AST variable node
  AstNode* createNodeVariable(std::string_view name, bool isUserDefined);

  /// @brief create an AST datasource
  /// this function will return either an AST collection or an AST view node
  /// if failIfDoesNotExist is true, the function will throw if the specified
  /// data source does not exist
  AstNode* createNodeDataSource(CollectionNameResolver const& resolver,
                                std::string_view name,
                                AccessMode::Type accessType, bool validateName,
                                bool failIfDoesNotExist);

  /// @brief create an AST collection node
  AstNode* createNodeCollection(CollectionNameResolver const& resolver,
                                std::string_view name,
                                AccessMode::Type accessType);

  /// @brief create an AST reference node
  AstNode* createNodeReference(std::string_view variableName);

  /// @brief create an AST reference node
  AstNode* createNodeReference(Variable const* variable);

  /// @brief create an AST subquery reference node
  AstNode* createNodeSubqueryReference(std::string_view variableName,
                                       AstNode const*);

  /// @brief create an AST parameter node for a value literal
  AstNode* createNodeParameter(std::string_view name);

  /// @brief create an AST parameter node for a datasource
  AstNode* createNodeParameterDatasource(std::string_view name);

  /// @brief create an AST quantifier node
  AstNode* createNodeQuantifier(Quantifier::Type type);

  /// @brief create an AST quantifier node
  AstNode* createNodeQuantifier(Quantifier::Type type, AstNode const* value);

  /// @brief create an AST unary operator
  AstNode* createNodeUnaryOperator(AstNodeType, AstNode const*);

  /// @brief create an AST binary operator
  AstNode* createNodeBinaryOperator(AstNodeType, AstNode const*,
                                    AstNode const*);

  /// @brief create an AST binary array operator
  AstNode* createNodeBinaryArrayOperator(AstNodeType, AstNode const*,
                                         AstNode const*, AstNode const*);

  /// @brief create an AST ternary operator
  AstNode* createNodeTernaryOperator(AstNode const*, AstNode const*);
  AstNode* createNodeTernaryOperator(AstNode const*, AstNode const*,
                                     AstNode const*);

  /// @brief create an AST variable access
  AstNode* createNodeAccess(Variable const*,
                            std::vector<basics::AttributeName> const&);

  /// @brief create an AST attribute access node
  /// note that the caller must make sure that char* data remains valid!
  AstNode* createNodeAttributeAccess(AstNode const*, std::string_view name);

  /// @brief create an AST attribute access node for multiple accesses
  AstNode* createNodeAttributeAccess(
      AstNode const*, std::vector<std::string_view> const& parts);
  AstNode* createNodeAttributeAccess(AstNode const*,
                                     std::vector<std::string> const& parts);
  AstNode* createNodeAttributeAccess(
      AstNode const* node, std::vector<basics::AttributeName> const& attrs);

  /// @brief create an AST attribute access node w/ bind parameter
  AstNode* createNodeBoundAttributeAccess(AstNode const*, AstNode const*);

  /// @brief create an AST index access node
  AstNode* createNodeIndexedAccess(AstNode const*, AstNode const*);

  /// @brief create an AST array limit node (offset, count)
  AstNode* createNodeArrayLimit(AstNode const*, AstNode const*);

  /// @brief create an AST array filter node (quantifier, filter)
  AstNode* createNodeArrayFilter(AstNode const*, AstNode const*);

  /// @brief create an AST boolean expansion node
  AstNode* createNodeBooleanExpansion(int64_t, AstNode const*, AstNode const*,
                                      AstNode const*);

  /// @brief create an AST expansion node
  AstNode* createNodeExpansion(int64_t, AstNode const*, AstNode const*,
                               AstNode const*, AstNode const*, AstNode const*);

  /// @brief create an AST iterator node
  AstNode* createNodeIterator(char const*, size_t, AstNode const*);

  /// @brief create an AST null value node
  AstNode* createNodeValueNull();

  /// @brief create an AST bool value node
  AstNode* createNodeValueBool(bool);

  /// @brief create an AST int value node
  AstNode* createNodeValueInt(int64_t);

  /// @brief create an AST double value node
  AstNode* createNodeValueDouble(double);

  /// @brief create an AST string value node
  AstNode* createNodeValueString(char const*, size_t);

  /// @brief create an AST string value node with forced non-constness
  AstNode* createNodeValueMutableString(char const*, size_t);

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
  AstNode* createNodeObjectElement(std::string_view name, AstNode const*);

  /// @brief create an AST calculated object element node
  AstNode* createNodeCalculatedObjectElement(AstNode const*, AstNode const*);

  /// @brief create an AST destructuring node
  AstNode* createNodeDestructuring(AstNode const* value, bool isObject);

  /// @brief create an AST with collections node
  AstNode* createNodeWithCollections(AstNode const*,
                                     CollectionNameResolver const& resolver);

  /// @brief create an AST collection list node
  AstNode* createNodeCollectionList(AstNode const*,
                                    CollectionNameResolver const& resolver);

  /// @brief create an AST direction node
  AstNode* createNodeDirection(uint64_t, uint64_t);

  /// @brief create an AST direction node
  AstNode* createNodeDirection(uint64_t, AstNode const*);

  /// @brief create an AST direction node
  AstNode* createNodeCollectionDirection(uint64_t, AstNode const*);

  /// @brief create an AST options node:
  //  Will either return Noop noed, if the input is nullptr
  //  Otherwise return the input node.
  AstNode const* createNodeOptions(AstNode const*) const;

  /// @brief create an AST traversal node
  AstNode* createNodeTraversal(AstNode const*, AstNode const*);

  /// @brief create an AST shortest path node
  AstNode* createNodeShortestPath(AstNode const*, AstNode const*);

  /// @brief create an AST k-shortest paths node
  AstNode* createNodeEnumeratePaths(graph::PathType::Type type, AstNode const*,
                                    AstNode const*);

  /// @brief create an AST function call node
  AstNode* createNodeFunctionCall(std::string_view functionName,
                                  AstNode const* arguments,
                                  bool allowInternalFunctions);

  /// @brief create an AST function call node for aggregate functions
  AstNode* createNodeAggregateFunctionCall(std::string_view functionName,
                                           AstNode const* arguments);

  /// @brief create an AST range node
  AstNode* createNodeRange(AstNode const*, AstNode const*);

  /// @brief create an AST nop node
  AstNode* createNodeNop();

  /// @brief create an AST n-ary operator
  AstNode* createNodeNaryOperator(AstNodeType);

  /// @brief create an AST n-ary operator
  AstNode* createNodeNaryOperator(AstNodeType, AstNode const*);

  /// @brief injects first-stage bind parameter values into the AST
  /// (i.e. collection bind parameters and bound attribute names,
  /// e.g. @@foo and `doc.@attr`).
  void injectBindParametersFirstStage(BindParameters& parameters,
                                      CollectionNameResolver const& resolver);

  /// @brief injects second-stage bind parameter values into the AST
  /// (i.e. all value bind parameters)
  void injectBindParametersSecondStage(BindParameters& parameters);

  /// @brief replaces bind parameters with special variables. This is used
  /// for query plan caching.
  void replaceBindParametersWithVariables(BindParameters& parameters);

  /// @brief replaces bind parameters with special variables. This is used
  /// for query plan caching.
  void replaceBindParametersWithValues(BindParameters& parameters);

  /// @brief replace variables
  ///        the unlock parameter will unlock the variable node before it
  ///        replaces the variable. This unlock is potentially dangerous if the
  ///        Node is linked somewhere else, so use with care and only if you are
  ///        sure that you hold the ONLY reference to this node.
  static AstNode* replaceVariables(
      AstNode* node, std::unordered_map<VariableId, Variable const*> const&,
      bool unlockNodes = false);

  /// @brief replace a variable reference in the expression with another
  /// expression (e.g. inserting c = `a + b` into expression `c + 1` so the
  /// latter
  /// becomes `a + b + 1`
  static AstNode* replaceVariableReference(AstNode*, Variable const*,
                                           AstNode const*);

  static size_t validatedParallelism(AstNode const* value);

  static size_t extractParallelism(AstNode const* optionsNode);

  /// @brief optimizes the AST
  void validateAndOptimize(transaction::Methods&,
                           ValidateAndOptimizeOptions const& options);

  /// @brief determines the variables referenced in an expression
  static void getReferencedVariables(AstNode const*, VarSet&);

  static bool isVarsUsed(AstNode const*, VarSet const&);

  /// @brief count how many times a variable is referenced in an expression
  static size_t countReferences(AstNode const*, Variable const*);

  /// @brief determines the attributes and subattributes used in an expression
  /// for the specified variable
  static bool getReferencedAttributesRecursive(
      AstNode const*, Variable const*, std::string_view expectedAttribute,
      containers::FlatHashSet<aql::AttributeNamePath>&,
      arangodb::ResourceMonitor& resourceMonitor);

  /// @brief replace an attribute access with just the variable
  static AstNode* replaceAttributeAccess(
      Ast* ast, AstNode* node, Variable const* searchVariable,
      std::span<std::string_view> attributeName,
      Variable const* replaceVariable);

  /// @brief recursively clone a node
  AstNode* clone(AstNode const*);

  /// @brief clone a node, but do not recursively clone subtree
  AstNode* shallowCopyForModify(AstNode const*);

  /// @brief deduplicate an array
  /// will return the original node if no modifications were made, and a new
  /// node if the array contained modifications
  AstNode const* deduplicateArray(AstNode const*);

  /// @brief check if an operator is reversible
  static bool isReversibleOperator(AstNodeType) noexcept;

  /// @brief get the reversed operator for a comparison operator.
  /// throws when called for an operator that is not reversible.
  static AstNodeType reverseOperator(AstNodeType type);

  /// throws when called for an operator that is not negatable.
  static AstNodeType negateOperator(AstNodeType type);

  /// @brief get the n-ary operator type equivalent for a binary operator type.
  /// throws when called for an invalid operator type.
  static AstNodeType naryOperatorType(AstNodeType);

  /// @brief return whether this is an `AND` operator
  static bool isAndOperatorType(AstNodeType type) noexcept;

  /// @brief return whether this is an `OR` operator
  static bool isOrOperatorType(AstNodeType type) noexcept;

  /// @brief create an AST node from vpack
  AstNode* nodeFromVPack(velocypack::Slice, bool copyStringValues);

  /// @brief resolve an attribute access
  AstNode const* resolveConstAttributeAccess(AstNode const*);

  /// @brief resolve an attribute access, static version
  /// if isValid is set to true, then the returned value is to be trusted. if
  /// isValid is set to false, then the returned value is not to be trued and
  /// the the result is equivalent to an AQL `null` value
  static AstNode const* resolveConstAttributeAccess(AstNode const*,
                                                    bool& isValid);

  /// @brief optimizes the unary operators + and -
  /// the unary plus will be converted into a simple value node if the operand
  /// of the operation is a constant number
  AstNode* optimizeUnaryOperatorArithmetic(AstNode*);

  AstNode const* getSubqueryForVariable(Variable const* variable) const;

  /** Make sure to replace the AstNode* you pass into TraverseAndModify
   *  if it was changed. This is necessary because the function itself
   *  has only access to the node but not its parent / owner.
   */
  /// @brief traverse the AST, using pre- and post-order visitors
  static AstNode* traverseAndModify(AstNode*,
                                    std::function<bool(AstNode const*)> const&,
                                    std::function<AstNode*(AstNode*)> const&,
                                    std::function<void(AstNode const*)> const&);

  /// @brief traverse the AST using a depth-first visitor
  static AstNode* traverseAndModify(AstNode*,
                                    std::function<AstNode*(AstNode*)> const&);

  /// @brief traverse the AST, using pre- and post-order visitors
  static void traverseReadOnly(AstNode const*,
                               std::function<bool(AstNode const*)> const&,
                               std::function<void(AstNode const*)> const&);

  /// @brief traverse the AST using a depth-first visitor, with const nodes
  static void traverseReadOnly(AstNode const*,
                               std::function<void(AstNode const*)> const&);

  /// @brief normalize a function name
  static std::pair<std::string, bool> normalizeFunctionName(
      std::string_view name);

 private:
  /// @brief replace a bind parameter with its value equivalent.
  AstNode* replaceValueBindParameter(AstNode* node, BindParameters& parameters);

  /// @brief make condition from example
  AstNode* makeConditionFromExample(AstNode const*);

  /// @brief create a number node for an arithmetic result, integer
  AstNode* createArithmeticResultNode(int64_t);

  /// @brief create a number node for an arithmetic result, double
  AstNode* createArithmeticResultNode(double);

  /// @brief optimizes the unary operator NOT with a non-constant expression
  AstNode* optimizeNotExpression(AstNode*);

  /// @brief optimizes the unary operator NOT
  AstNode* optimizeUnaryOperatorLogical(AstNode*);

  /// @brief optimizes the binary logical operators && and ||
  AstNode* optimizeBinaryOperatorLogical(AstNode*, bool);

  /// @brief optimizes the binary relational operators <, <=, >, >=, ==, != and
  /// IN
  AstNode* optimizeBinaryOperatorRelational(transaction::Methods&,
                                            AqlFunctionsInternalCache&,
                                            AstNode*);

  /// @brief optimizes the binary arithmetic operators +, -, *, / and %
  AstNode* optimizeBinaryOperatorArithmetic(AstNode*);

  /// @brief optimizes the ternary operator
  AstNode* optimizeTernaryOperator(AstNode*);

  /// @brief optimizes an attribute access
  AstNode* optimizeAttributeAccess(
      AstNode*, std::unordered_map<Variable const*, AstNode const*> const&);

  /// @brief optimizes a call to a built-in function
  AstNode* optimizeFunctionCall(transaction::Methods&,
                                AqlFunctionsInternalCache&, AstNode*,
                                ValidateAndOptimizeOptions const& options);

  /// @brief optimizes indexed access, e.g. a[0] or a['foo']
  AstNode* optimizeIndexedAccess(
      AstNode* node, std::unordered_map<Variable const*, AstNode const*> const&
                         variableDefinitions);

  /// @brief optimizes the FILTER statement
  AstNode* optimizeFilter(AstNode*);

  /// @brief optimizes the FOR statement
  /// no real optimizations are done here, but we do an early check if the
  /// FOR loop operand is actually a list
  AstNode* optimizeFor(AstNode*);

  /// @brief optimizes an object literal or an object expression
  AstNode* optimizeObject(AstNode*);

  /// @brief create a node of the specified type
  AstNode* createNode(AstNodeType);

  /// @brief validate the name of the given datasource
  /// in case validation fails, will throw an exception
  void validateDataSourceName(std::string_view name, bool validateStrict);

  /// @brief create an AST collection node
  /// private function, does no validation
  AstNode* createNodeCollectionNoValidation(std::string_view name,
                                            AccessMode::Type accessType);

  void extractCollectionsFromGraph(BindParameters& parameter,
                                   AstNode* graphNode);

  /// @brief copies node payload from node into copy. this is *not* copying
  /// the subnodes
  void copyPayload(AstNode const* node, AstNode* copy) const;

  bool hasFlag(AstPropertyFlag flag) const noexcept {
    return ((_astFlags & static_cast<decltype(_astFlags)>(flag)) != 0);
  }

 private:
  /// @brief the query
  QueryContext& _query;

  AstResources _resources;

  /// @brief all scopes used in the query
  Scopes _scopes;

  /// @brief generator for variables
  VariableGenerator _variables;

  /// @brief the bind parameters we found in the query
  std::unordered_set<std::string> _bindParameters;

  /// @brief root node of the AST
  AstNode* _root;

  /// @brief root nodes of queries and subqueries. this container is added
  /// to whenever we enter a subquery, but it is removed from when a subquery
  /// is left
  containers::SmallVector<AstNode*, 4> _queries;

  /// @brief all subqueries used in the query
  containers::FlatHashMap<VariableId, AstNode const*> _subqueries;

  /// @brief which collection is going to be modified in the query
  /// maps from NODE_TYPE_COLLECTION/NODE_TYPE_PARAMETER_DATASOURCE to
  /// whether the collection is used in exclusive mode
  std::vector<std::pair<AstNode const*, bool>> _writeCollections;

  /// @brief whether or not function calls may access collection data
  bool _functionsMayAccessDocuments;

  /// @brief whether or not the query contains a traversal
  bool _containsTraversal;

  /// @brief whether or not the query contains bind parameters
  bool _containsBindParameters;

  /// @brief contains INSERT / UPDATE / REPLACE / REMOVE / UPSERT
  bool _containsModificationNode;

  /// @brief contains UPSERT
  bool _containsUpsertNode;

  /// @brief contains a parallel traversal
  bool _containsParallelNode;

  /// @brief whether or not a part of the query uses async prefetching
  bool _containsAsyncPrefetch;

  /// @brief query makes use of V8 function(s)
  bool _willUseV8;

  /// @brief special node types that are used often and for which no memory
  /// allocation will be needed. The node types are singletons in an AST,
  /// so they may be referenced from multiple places.
  struct SpecialNodes {
    SpecialNodes();

    ~SpecialNodes() = default;

    /// @brief a singleton no-op node instance
    AstNode NopNode;

    /// @brief a singleton null node instance
    AstNode NullNode;

    /// @brief a singleton false node instance
    AstNode FalseNode;

    /// @brief a singleton true node instance
    AstNode TrueNode;

    /// @brief a singleton zero node instance
    AstNode ZeroNode;

    /// @brief a singleton empty string node instance
    AstNode EmptyStringNode;
  };

  SpecialNodes const _specialNodes;

  /// @brief ast flags
  AstPropertiesFlagsType _astFlags;

  /// @brief variables that bind parameters were replaced with
  BindParameterVariableMapping _bindParameterVariables;
};

}  // namespace aql
}  // namespace arangodb

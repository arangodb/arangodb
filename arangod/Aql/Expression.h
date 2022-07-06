////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include <v8.h>
#include <velocypack/Slice.h>

#include "Aql/types.h"
#include "Containers/HashSet.h"

namespace arangodb {
namespace transaction {
class Methods;
}

namespace velocypack {
class Builder;
}

namespace aql {
class AqlItemBlock;
struct AqlValue;
class Ast;
struct AstNode;
class AttributeAccessor;
class ExecutionPlan;
class ExpressionContext;
class QueryContext;
struct Variable;

/// @brief AqlExpression, used in execution plans and execution blocks
class Expression {
 public:
  enum ExpressionType : uint32_t {
    UNPROCESSED,
    JSON,
    SIMPLE,
    ATTRIBUTE_ACCESS
  };

  Expression(Expression const&) = delete;
  Expression& operator=(Expression const&) = delete;
  Expression() = delete;

  /// @brief constructor, using an AST start node
  Expression(Ast*, AstNode*);

  /// @brief constructor, using VPack
  Expression(Ast*, arangodb::velocypack::Slice const&);

  ~Expression();

  /// @brief replace the root node
  void replaceNode(AstNode* node);

  /// @brief get the underlying AST
  Ast* ast() const noexcept;

  /// @brief get the underlying AST node
  AstNode const* node() const;

  /// @brief get the underlying AST node
  AstNode* nodeForModification() const;

  /// @brief whether or not the expression can safely run on a DB server
  bool canRunOnDBServer(bool isOneShard);

  /// @brief whether or not the expression is deterministic
  bool isDeterministic();

  /// @brief whether or not the expression will use V8
  bool willUseV8();

  /// @brief clone the expression, needed to clone execution plans
  std::unique_ptr<Expression> clone(Ast* ast, bool deepCopy = false);

  /// @brief return all variables used in the expression
  void variables(VarSet&) const;

  /// @brief return a VelocyPack representation of the expression
  void toVelocyPack(arangodb::velocypack::Builder& builder, bool verbose) const;

  /// @brief execute the expression
  AqlValue execute(ExpressionContext* ctx, bool& mustDestroy);

  /// @brief get expression type as string
  std::string typeString();

  // @brief invoke JavaScript aql functions with args as param.
  static AqlValue invokeV8Function(ExpressionContext& expressionContext,
                                   std::string const& jsName,
                                   std::string const& ucInvokeFN,
                                   char const* AFN, bool rethrowV8Exception,
                                   size_t callArgs, v8::Handle<v8::Value>* args,
                                   bool& mustDestroy);

  /// @brief check whether this is an attribute access of any degree (e.g. a.b,
  /// a.b.c, ...)
  bool isAttributeAccess() const;

  /// @brief check whether this is only a reference access
  bool isReference() const;

  /// @brief check whether this is a constant node
  bool isConstant() const;

  /// @brief stringify an expression
  /// note that currently stringification is only supported for certain node
  /// types
  void stringify(std::string& buffer) const;

  /// @brief stringify an expression, if it is not too long
  /// if the stringified version becomes too long, this method will throw
  /// note that currently stringification is only supported for certain node
  /// types
  void stringifyIfNotTooLong(std::string& buffer) const;

  /// @brief replace variables in the expression with other variables
  void replaceVariables(std::unordered_map<VariableId, Variable const*> const&);

  /// @brief replace a variable reference in the expression with another
  /// expression (e.g. inserting c = `a + b` into expression `c + 1` so the
  /// latter becomes `a + b + 1`
  void replaceVariableReference(Variable const*, AstNode const*);

  void replaceAttributeAccess(Variable const*,
                              std::vector<std::string> const& attribute);

  /// @brief reset internal attributes after variables in the expression were
  /// changed
  void invalidateAfterReplacements();

  // prepare the expression for execution
  void prepareForExecution();

 private:
  // free the internal data structures
  void freeInternals() noexcept;

  // find a value in an array
  static bool findInArray(AqlValue const&, AqlValue const&,
                          velocypack::Options const* vopts, AstNode const*);

  // analyze the expression (determine its type)
  void determineType();

  // init the accessor specialization
  void initAccessor();

  // execute an expression of type SIMPLE
  static AqlValue executeSimpleExpression(ExpressionContext& ctx,
                                          AstNode const*, bool& mustDestroy,
                                          bool doCopy);

  // execute an expression of type SIMPLE with ATTRIBUTE ACCESS
  static AqlValue executeSimpleExpressionAttributeAccess(ExpressionContext& ctx,
                                                         AstNode const*,
                                                         bool& mustDestroy,
                                                         bool doCopy);

  // execute an expression of type SIMPLE with INDEXED ACCESS
  static AqlValue executeSimpleExpressionIndexedAccess(ExpressionContext& ctx,
                                                       AstNode const*,
                                                       bool& mustDestroy,
                                                       bool doCopy);

  // execute an expression of type SIMPLE with ARRAY
  static AqlValue executeSimpleExpressionArray(ExpressionContext& ctx,
                                               AstNode const*,
                                               bool& mustDestroy);

  // execute an expression of type SIMPLE with OBJECT
  static AqlValue executeSimpleExpressionObject(ExpressionContext& ctx,
                                                AstNode const*,
                                                bool& mustDestroy);

  // execute an expression of type SIMPLE with VALUE
  static AqlValue executeSimpleExpressionValue(ExpressionContext& ctx,
                                               AstNode const*,
                                               bool& mustDestroy);

  // execute an expression of type SIMPLE with REFERENCE
  static AqlValue executeSimpleExpressionReference(ExpressionContext& ctx,
                                                   AstNode const*,
                                                   bool& mustDestroy,
                                                   bool doCopy);

  // execute an expression of type SIMPLE with FCALL, dispatcher
  static AqlValue executeSimpleExpressionFCall(ExpressionContext& ctx,
                                               AstNode const*,
                                               bool& mustDestroy);

  // execute an expression of type SIMPLE with FCALL, CXX variant
  static AqlValue executeSimpleExpressionFCallCxx(ExpressionContext& ctx,
                                                  AstNode const*,
                                                  bool& mustDestroy);

  // execute an expression of type SIMPLE with FCALL, JavaScript variant
  static AqlValue executeSimpleExpressionFCallJS(ExpressionContext& ctx,
                                                 AstNode const*,
                                                 bool& mustDestroy);

  // execute an expression of type SIMPLE with RANGE
  static AqlValue executeSimpleExpressionRange(ExpressionContext& ctx,
                                               AstNode const*,
                                               bool& mustDestroy);

  // execute an expression of type SIMPLE with NOT
  static AqlValue executeSimpleExpressionNot(ExpressionContext& ctx,
                                             AstNode const*, bool& mustDestroy);

  // execute an expression of type SIMPLE with +
  static AqlValue executeSimpleExpressionPlus(ExpressionContext& ctx,
                                              AstNode const*,
                                              bool& mustDestroy);

  // execute an expression of type SIMPLE with -
  static AqlValue executeSimpleExpressionMinus(ExpressionContext& ctx,
                                               AstNode const*,
                                               bool& mustDestroy);

  // execute an expression of type SIMPLE with AND
  static AqlValue executeSimpleExpressionAnd(ExpressionContext& ctx,
                                             AstNode const*, bool& mustDestroy);

  // execute an expression of type SIMPLE with OR
  static AqlValue executeSimpleExpressionOr(ExpressionContext& ctx,
                                            AstNode const*, bool& mustDestroy);

  // execute an expression of type SIMPLE with NARY AND or OR
  static AqlValue executeSimpleExpressionNaryAndOr(ExpressionContext& ctx,
                                                   AstNode const*,
                                                   bool& mustDestroy);

  // execute an expression of type SIMPLE with COMPARISON
  static AqlValue executeSimpleExpressionComparison(ExpressionContext& ctx,
                                                    AstNode const*,
                                                    bool& mustDestroy);

  // execute an expression of type SIMPLE with ARRAY COMPARISON
  static AqlValue executeSimpleExpressionArrayComparison(ExpressionContext& ctx,
                                                         AstNode const*,
                                                         bool& mustDestroy);

  // execute an expression of type SIMPLE with TERNARY
  static AqlValue executeSimpleExpressionTernary(ExpressionContext& ctx,
                                                 AstNode const*,
                                                 bool& mustDestroy);

  // execute an expression of type SIMPLE with EXPANSION
  static AqlValue executeSimpleExpressionExpansion(ExpressionContext& ctx,
                                                   AstNode const*,
                                                   bool& mustDestroy);

  // execute an expression of type SIMPLE with EXPANSION
  static AqlValue executeSimpleExpressionIterator(ExpressionContext& ctx,
                                                  AstNode const*,
                                                  bool& mustDestroy);

  // execute an expression of type SIMPLE with BINARY_* (+, -, * , /, %)
  static AqlValue executeSimpleExpressionArithmetic(ExpressionContext& ctx,
                                                    AstNode const*,
                                                    bool& mustDestroy);

  // the AST
  Ast* _ast;

  // the AST node that contains the expression to execute
  AstNode* _node;

  // if the expression is a constant, it will be stored as plain JSON instead
  union {
    uint8_t* _data;
    AttributeAccessor* _accessor;
  };

  // type of expression
  ExpressionType _type;
};

}  // namespace aql
}  // namespace arangodb

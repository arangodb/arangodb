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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_EXPRESSION_H
#define ARANGOD_AQL_EXPRESSION_H 1

#include "Basics/Common.h"
#include "Aql/AstNode.h"
#include "Aql/Range.h"
#include "Aql/Variable.h"
#include "Aql/types.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <v8.h>

namespace arangodb {
namespace transaction {
class Methods;
}

namespace basics {
class StringBuffer;
}

namespace aql {
class AqlItemBlock;
struct AqlValue;
class Ast;
class AttributeAccessor;
class ExecutionPlan;
class ExpressionContext;
class Query;

/// @brief AqlExpression, used in execution plans and execution blocks
class Expression {
 public:
  enum ExpressionType : uint32_t { UNPROCESSED, JSON, SIMPLE, ATTRIBUTE_ACCESS };

  Expression(Expression const&) = delete;
  Expression& operator=(Expression const&) = delete;
  Expression() = delete;

  /// @brief constructor, using an AST start node
  Expression(ExecutionPlan* plan, Ast*, AstNode*);

  /// @brief constructor, using VPack
  Expression(ExecutionPlan* plan, Ast*, arangodb::velocypack::Slice const&);

  ~Expression();

  /// @brief replace the root node
  void replaceNode(AstNode* node) {
    if (node != _node) {
      _node = node;
      invalidateAfterReplacements();
    }
  }

  /// @brief get the underlying AST node
  inline AstNode const* node() const { return _node; }

  /// @brief get the underlying AST node
  inline AstNode* nodeForModification() const { return _node; }

  /// @brief whether or not the expression can safely run on a DB server
  inline bool canRunOnDBServer() {
    if (_type == UNPROCESSED) {
      initExpression();
    }
    return _canRunOnDBServer;
  }

  /// @brief whether or not the expression is deterministic
  inline bool isDeterministic() {
    if (_type == UNPROCESSED) {
      initExpression();
    }
    return _isDeterministic;
  }

  /// @brief whether or not the expression will use V8
  inline bool willUseV8() {
    if (_type == UNPROCESSED) {
      initExpression();
    }
    return _willUseV8;
  }

  /// @brief clone the expression, needed to clone execution plans
  Expression* clone(ExecutionPlan* plan, Ast* ast) {
    // We do not need to copy the _ast, since it is managed by the
    // query object and the memory management of the ASTs
    return new Expression(plan, ast != nullptr ? ast : _ast, _node);
  }

  /// @brief return all variables used in the expression
  void variables(std::unordered_set<Variable const*>&) const;

  /// @brief return a VelocyPack representation of the expression
  void toVelocyPack(arangodb::velocypack::Builder& builder, bool verbose) const {
    _node->toVelocyPack(builder, verbose);
  }

  /// @brief execute the expression
  AqlValue execute(transaction::Methods* trx, ExpressionContext* ctx,
                   bool& mustDestroy);

  /// @brief get expression type as string
  std::string typeString() {
    if (_type == UNPROCESSED) {
      initExpression();
    }

    switch (_type) {
      case JSON:
        return "json";
      case SIMPLE:
        return "simple";
      case ATTRIBUTE_ACCESS:
        return "attribute";
      case UNPROCESSED: {
      }
    }
    TRI_ASSERT(false);
    return "unknown";
  }

  // @brief invoke JavaScript aql functions with args as param.
  static AqlValue invokeV8Function(arangodb::aql::ExpressionContext* expressionContext,
                                   transaction::Methods* trx,
                                   std::string const& jsName,
                                   std::string const& ucInvokeFN,
                                   char const* AFN,
                                   bool rethrowV8Exception,
                                   size_t callArgs,
                                   v8::Handle<v8::Value>* args,
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
  void stringify(arangodb::basics::StringBuffer*) const;

  /// @brief stringify an expression, if it is not too long
  /// if the stringified version becomes too long, this method will throw
  /// note that currently stringification is only supported for certain node
  /// types
  void stringifyIfNotTooLong(arangodb::basics::StringBuffer*) const;

  /// @brief replace variables in the expression with other variables
  void replaceVariables(std::unordered_map<VariableId, Variable const*> const&);

  /// @brief replace a variable reference in the expression with another
  /// expression (e.g. inserting c = `a + b` into expression `c + 1` so the
  /// latter becomes `a + b + 1`
  void replaceVariableReference(Variable const*, AstNode const*);

  void replaceAttributeAccess(Variable const*, std::vector<std::string> const& attribute);

  /// @brief invalidates an expression
  /// this only has an effect for V8-using functions, which need to be created,
  /// used and destroyed in the same context. when a V8 function is used across
  /// multiple V8 contexts, it must be invalidated in between
  void invalidate();

  void setVariable(Variable const* variable, arangodb::velocypack::Slice value) {
    _variables.emplace(variable, value);
  }

  void clearVariable(Variable const* variable) { _variables.erase(variable); }

 private:
  /// @brief free the internal data structures
  void freeInternals() noexcept;

  /// @brief reset internal attributes after variables in the expression were changed
  void invalidateAfterReplacements();

  /// @brief find a value in an array
  bool findInArray(AqlValue const&, AqlValue const&,
                   transaction::Methods*,
                   AstNode const*) const;

  void initConstantExpression();
  void initSimpleExpression();

  /// @brief analyze the expression (determine its type etc.)
  void initExpression();

  /// @brief build the expression (if appropriate, compile it into
  /// executable code)
  void buildExpression(transaction::Methods*);

  /// @brief execute an expression of type SIMPLE
  AqlValue executeSimpleExpression(AstNode const*,
                                   transaction::Methods*,
                                   bool& mustDestroy, bool);

  /// @brief execute an expression of type SIMPLE with ATTRIBUTE ACCESS
  AqlValue executeSimpleExpressionAttributeAccess(
      AstNode const*, transaction::Methods*, bool& mustDestroy, bool doCopy);

  /// @brief execute an expression of type SIMPLE with INDEXED ACCESS
  AqlValue executeSimpleExpressionIndexedAccess(
      AstNode const*, transaction::Methods*,
      bool& mustDestroy, bool doCopy);

  /// @brief execute an expression of type SIMPLE with ARRAY
  AqlValue executeSimpleExpressionArray(AstNode const*,
                                        transaction::Methods*,
                                        bool& mustDestroy);

  /// @brief execute an expression of type SIMPLE with OBJECT
  AqlValue executeSimpleExpressionObject(AstNode const*,
                                         transaction::Methods*,
                                         bool& mustDestroy);

  /// @brief execute an expression of type SIMPLE with VALUE
  AqlValue executeSimpleExpressionValue(AstNode const*,
                                        transaction::Methods*,
                                        bool& mustDestroy);

  /// @brief execute an expression of type SIMPLE with REFERENCE
  AqlValue executeSimpleExpressionReference(AstNode const*,
                                            transaction::Methods*,
                                            bool& mustDestroy,
                                            bool);

  /// @brief execute an expression of type SIMPLE with FCALL, dispatcher
  AqlValue executeSimpleExpressionFCall(AstNode const*,
                                        transaction::Methods*,
                                        bool& mustDestroy);

  /// @brief execute an expression of type SIMPLE with FCALL, CXX variant
  AqlValue executeSimpleExpressionFCallCxx(AstNode const*,
                                           transaction::Methods*,
                                           bool& mustDestroy);
  /// @brief execute an expression of type SIMPLE with FCALL, JavaScript variant
  AqlValue executeSimpleExpressionFCallJS(AstNode const*,
                                          transaction::Methods*,
                                          bool& mustDestroy);

  /// @brief execute an expression of type SIMPLE with RANGE
  AqlValue executeSimpleExpressionRange(AstNode const*,
                                        transaction::Methods*,
                                        bool& mustDestroy);

  /// @brief execute an expression of type SIMPLE with NOT
  AqlValue executeSimpleExpressionNot(AstNode const*, transaction::Methods*,
                                       bool& mustDestroy);

  /// @brief execute an expression of type SIMPLE with +
  AqlValue executeSimpleExpressionPlus(AstNode const*, transaction::Methods*,
                                       bool& mustDestroy);

  /// @brief execute an expression of type SIMPLE with -
  AqlValue executeSimpleExpressionMinus(AstNode const*, transaction::Methods*,
                                        bool& mustDestroy);

  /// @brief execute an expression of type SIMPLE with AND
  AqlValue executeSimpleExpressionAnd(AstNode const*,
                                      transaction::Methods*,
                                      bool& mustDestroy);

  /// @brief execute an expression of type SIMPLE with OR
  AqlValue executeSimpleExpressionOr(AstNode const*,
                                     transaction::Methods*,
                                     bool& mustDestroy);

  /// @brief execute an expression of type SIMPLE with NARY AND or OR
  AqlValue executeSimpleExpressionNaryAndOr(AstNode const*,
                                            transaction::Methods*,
                                            bool& mustDestroy);

  /// @brief execute an expression of type SIMPLE with COMPARISON
  AqlValue executeSimpleExpressionComparison(
      AstNode const*, transaction::Methods*,
      bool& mustDestroy);

  /// @brief execute an expression of type SIMPLE with ARRAY COMPARISON
  AqlValue executeSimpleExpressionArrayComparison(
      AstNode const*, transaction::Methods*,
      bool& mustDestroy);

  /// @brief execute an expression of type SIMPLE with TERNARY
  AqlValue executeSimpleExpressionTernary(AstNode const*,
                                          transaction::Methods*,
                                          bool& mustDestroy);

  /// @brief execute an expression of type SIMPLE with EXPANSION
  AqlValue executeSimpleExpressionExpansion(AstNode const*,
                                            transaction::Methods*,
                                            bool& mustDestroy);

  /// @brief execute an expression of type SIMPLE with EXPANSION
  AqlValue executeSimpleExpressionIterator(AstNode const*,
                                           transaction::Methods*,
                                           bool& mustDestroy);

  /// @brief execute an expression of type SIMPLE with BINARY_* (+, -, * , /, %)
  AqlValue executeSimpleExpressionArithmetic(
      AstNode const*, transaction::Methods*,
      bool& mustDestroy);

 private:
  /// @brief the query execution plan. note: this may be a nullptr for expressions
  /// created in the early optimization stage!
  ExecutionPlan* _plan;

  /// @brief the AST
  Ast* _ast;

  /// @brief the AST node that contains the expression to execute
  AstNode* _node;

  /// if the expression is a constant, it will be stored as plain JSON instead
  union {
    uint8_t* _data;
    AttributeAccessor* _accessor;
  };

  /// @brief type of expression
  ExpressionType _type;

  /// @brief whether or not the expression can be run safely on a DB server
  bool _canRunOnDBServer;

  /// @brief whether or not the expression is deterministic
  bool _isDeterministic;

  /// @brief whether or not the expression will make use of V8
  bool _willUseV8;

  /// @brief the top-level attributes used in the expression, grouped
  /// by variable name
  std::unordered_map<Variable const*, std::unordered_set<std::string>>
      _attributes;

  /// @brief variables only temporarily valid during execution
  std::unordered_map<Variable const*, arangodb::velocypack::Slice> _variables;

  ExpressionContext* _expressionContext;
};

}  // namespace arangodb::aql
}  // namespace arangodb

#endif

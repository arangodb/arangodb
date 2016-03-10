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

#include "Expression.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/AqlValue.h"
#include "Aql/Ast.h"
#include "Aql/AttributeAccessor.h"
#include "Aql/Executor.h"
#include "Aql/Quantifier.h"
#include "Aql/Query.h"
#include "Aql/V8Expression.h"
#include "Aql/Variable.h"
#include "Basics/Exceptions.h"
#include "Basics/JsonHelper.h"
#include "Basics/StringBuffer.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/json.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::aql;
using Json = arangodb::basics::Json;
using JsonHelper = arangodb::basics::JsonHelper;
using VelocyPackHelper = arangodb::basics::VelocyPackHelper;

////////////////////////////////////////////////////////////////////////////////
/// @brief register warning
////////////////////////////////////////////////////////////////////////////////

static void RegisterWarning(arangodb::aql::Ast const* ast,
                            char const* functionName, int code) {
  std::string msg;

  if (code == TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH) {
    msg = arangodb::basics::Exception::FillExceptionString(code, functionName);
  } else {
    msg.append("in function '");
    msg.append(functionName);
    msg.append("()': ");
    msg.append(TRI_errno_string(code));
  }

  ast->query()->registerWarning(code, msg.c_str());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create the expression
////////////////////////////////////////////////////////////////////////////////

Expression::Expression(Ast* ast, AstNode* node)
    : _ast(ast),
      _executor(_ast->query()->executor()),
      _node(node),
      _type(UNPROCESSED),
      _canThrow(true),
      _canRunOnDBServer(false),
      _isDeterministic(false),
      _hasDeterminedAttributes(false),
      _built(false),
      _attributes(),
      _buffer(TRI_UNKNOWN_MEM_ZONE) {
  TRI_ASSERT(_ast != nullptr);
  TRI_ASSERT(_executor != nullptr);
  TRI_ASSERT(_node != nullptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an expression from JSON
////////////////////////////////////////////////////////////////////////////////

Expression::Expression(Ast* ast, arangodb::basics::Json const& json)
    : Expression(ast, new AstNode(ast, json.get("expression"))) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the expression
////////////////////////////////////////////////////////////////////////////////

Expression::~Expression() {
  if (_built) {
    switch (_type) {
      case JSON:
        TRI_ASSERT(_data != nullptr);
        delete[] _data;
        break;

      case ATTRIBUTE: {
        TRI_ASSERT(_accessor != nullptr);
        delete _accessor;
        break;
      }

      case V8:
        delete _func;
        break;

      case SIMPLE:
      case UNPROCESSED: {
        // nothing to do
        break;
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return all variables used in the expression
////////////////////////////////////////////////////////////////////////////////

void Expression::variables(std::unordered_set<Variable const*>& result) const {
  Ast::getReferencedVariables(_node, result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute the expression
////////////////////////////////////////////////////////////////////////////////

AqlValue$ Expression::execute(arangodb::AqlTransaction* trx,
                              AqlItemBlock const* argv, size_t startPos,
                              std::vector<Variable const*> const& vars,
                              std::vector<RegisterId> const& regs) {
  if (!_built) {
    buildExpression();
  }

  TRI_ASSERT(_type != UNPROCESSED);
  TRI_ASSERT(_built);

  // and execute
  switch (_type) {
    case JSON: {
      // TODO
      TRI_ASSERT(_data != nullptr);
      return AqlValue$(_data);
    }

    case SIMPLE: {
      return executeSimpleExpression(_node, trx, argv, startPos,
                                     vars, regs, true);
    }

    case ATTRIBUTE: {
      TRI_ASSERT(_accessor != nullptr);
      return _accessor->get(trx, argv, startPos, vars, regs);
    }

    case V8: {
      TRI_ASSERT(_func != nullptr);
      ISOLATE;
      return _func->execute(isolate, _ast->query(), trx, argv, startPos, vars, regs);
    }

    case UNPROCESSED: {
      // fall-through to exception
    }
  }

  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                 "invalid simple expression");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replace variables in the expression with other variables
////////////////////////////////////////////////////////////////////////////////

void Expression::replaceVariables(
    std::unordered_map<VariableId, Variable const*> const& replacements) {
  _node = _ast->clone(_node);
  TRI_ASSERT(_node != nullptr);

  _node = _ast->replaceVariables(const_cast<AstNode*>(_node), replacements);
  invalidate();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replace a variable reference in the expression with another
/// expression (e.g. inserting c = `a + b` into expression `c + 1` so the latter
/// becomes `a + b + 1`
////////////////////////////////////////////////////////////////////////////////

void Expression::replaceVariableReference(Variable const* variable,
                                          AstNode const* node) {
  _node = _ast->clone(_node);
  TRI_ASSERT(_node != nullptr);

  _node = _ast->replaceVariableReference(const_cast<AstNode*>(_node), variable,
                                         node);
  invalidate();

  if (_type == ATTRIBUTE) {
    if (_built) {
      delete _accessor;
      _accessor = nullptr;
      _built = false;
    }
    // must even set back the expression type so the expression will be analyzed
    // again
    _type = UNPROCESSED;
  }

  const_cast<AstNode*>(_node)->clearFlags();
  _attributes.clear();
  _hasDeterminedAttributes = false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief invalidates an expression
/// this only has an effect for V8-based functions, which need to be created,
/// used and destroyed in the same context. when a V8 function is used across
/// multiple V8 contexts, it must be invalidated in between
////////////////////////////////////////////////////////////////////////////////

void Expression::invalidate() {
  if (_type == V8) {
    // V8 expressions need a special handling
    if (_built) {
      delete _func;
      _func = nullptr;
      _built = false;
    }
  }
  // we do not need to invalidate the other expression type
  // expression data will be freed in the destructor
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find a value in an AQL list node
/// this performs either a binary search (if the node is sorted) or a
/// linear search (if the node is not sorted)
////////////////////////////////////////////////////////////////////////////////

bool Expression::findInArray(AqlValue$ const& left, AqlValue$ const& right,
                             arangodb::AqlTransaction* trx,
                             AstNode const* node) const {
  TRI_ASSERT(right.isArray());

  size_t const n = right.length();

  if (n > 3 && 
      (node->getMember(1)->isSorted() ||
      ((node->type == NODE_TYPE_OPERATOR_BINARY_IN || 
        node->type == NODE_TYPE_OPERATOR_BINARY_NIN) && node->getBoolValue()))) {
    // node values are sorted. can use binary search
    size_t l = 0;
    size_t r = n - 1;

    while (true) {
      // determine midpoint
      size_t m = l + ((r - l) / 2);

      int compareResult = AqlValue$::Compare(trx, left, right.at(m, false), true);

      if (compareResult == 0) {
        // item found in the list
        return true;
      }

      if (compareResult < 0) {
        if (m == 0) {
          // not found
          return false;
        }
        r = m - 1;
      } else {
        l = m + 1;
      }
      if (r < l) {
        return false;
      }
    }
  } 
    
  // use linear search
  for (size_t i = 0; i < n; ++i) {
    int compareResult = AqlValue$::Compare(trx, left, right.at(i, false), false);

    if (compareResult == 0) {
      // item found in the list
      return true;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief analyze the expression (determine its type etc.)
////////////////////////////////////////////////////////////////////////////////

void Expression::analyzeExpression() {
  TRI_ASSERT(_type == UNPROCESSED);
  TRI_ASSERT(_built == false);

  if (_node->isConstant()) {
    // expression is a constant value
    _type = JSON;
    _canThrow = false;
    _canRunOnDBServer = true;
    _isDeterministic = true;
    _data = nullptr;
  } else if (_node->isSimple()) {
    // expression is a simple expression
    _type = SIMPLE;
    _canThrow = _node->canThrow();
    _canRunOnDBServer = _node->canRunOnDBServer();
    _isDeterministic = _node->isDeterministic();

    if (_node->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
      TRI_ASSERT(_node->numMembers() == 1);
      auto member = _node->getMemberUnchecked(0);
      std::vector<char const*> parts{
          static_cast<char const*>(_node->getData())};

      while (member->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
        parts.insert(parts.begin(),
                     static_cast<char const*>(member->getData()));
        member = member->getMemberUnchecked(0);
      }

      if (member->type == NODE_TYPE_REFERENCE) {
        auto v = static_cast<Variable const*>(member->getData());

        // specialize the simple expression into an attribute accessor
        _accessor = new AttributeAccessor(parts, v);
        _type = ATTRIBUTE;
        _built = true;
      }
    }
  } else {
    // expression is a V8 expression
    _type = V8;
    _canThrow = _node->canThrow();
    _canRunOnDBServer = _node->canRunOnDBServer();
    _isDeterministic = _node->isDeterministic();
    _func = nullptr;

    if (!_hasDeterminedAttributes) {
      // determine all top-level attributes used in expression only once
      // as this might be expensive
      _hasDeterminedAttributes = true;

      bool isSafeForOptimization;
      _attributes =
          Ast::getReferencedAttributes(_node, isSafeForOptimization);

      if (!isSafeForOptimization) {
        _attributes.clear();
        // unfortunately there are not only top-level attribute accesses but
        // also other accesses, e.g. the index values or accesses of the whole
        // value.
        // for example, we cannot optimize LET x = a +1 or LET x = a[0], but LET
        // x = a._key
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief build the expression
////////////////////////////////////////////////////////////////////////////////

void Expression::buildExpression() {
  TRI_ASSERT(!_built);

  if (_type == UNPROCESSED) {
    analyzeExpression();
  }

  if (_type == JSON) {
    TRI_ASSERT(_data == nullptr);
    // generate a constant value
    VPackBuilder builder;
    _node->toVelocyPackValue(builder);

    _data = new uint8_t[builder.size()];
    memcpy(_data, builder.data(), builder.size());
  } else if (_type == V8) {
    // generate a V8 expression
    _func = _executor->generateExpression(_node);

    // optimizations for the generated function
    if (_func != nullptr && !_attributes.empty()) {
      // pass which variables do not need to be fully constructed
      _func->setAttributeRestrictions(_attributes);
    }
  }

  _built = true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute an expression of type SIMPLE, the convention is that
/// the resulting AqlValue will be destroyed outside eventually
////////////////////////////////////////////////////////////////////////////////

AqlValue$ Expression::executeSimpleExpression(
    AstNode const* node, arangodb::AqlTransaction* trx, AqlItemBlock const* argv, size_t startPos,
    std::vector<Variable const*> const& vars,
    std::vector<RegisterId> const& regs, bool doCopy) {
  switch (node->type) {
    case NODE_TYPE_ATTRIBUTE_ACCESS:
      return executeSimpleExpressionAttributeAccess(node, trx, argv, startPos,
                                                    vars, regs);
    case NODE_TYPE_INDEXED_ACCESS:
      return executeSimpleExpressionIndexedAccess(node, trx, argv, startPos,
                                                  vars, regs);
    case NODE_TYPE_ARRAY:
      return executeSimpleExpressionArray(node, trx, argv, startPos, vars,
                                          regs);
    case NODE_TYPE_OBJECT:
      return executeSimpleExpressionObject(node, trx, argv, startPos, vars,
                                           regs);
    case NODE_TYPE_VALUE:
      return executeSimpleExpressionValue(node);
    case NODE_TYPE_REFERENCE:
      return executeSimpleExpressionReference(node, trx, argv, startPos,
                                              vars, regs, doCopy);
    case NODE_TYPE_FCALL:
      return executeSimpleExpressionFCall(node, trx, argv, startPos, vars,
                                          regs);
    case NODE_TYPE_RANGE:
      return executeSimpleExpressionRange(node, trx, argv, startPos, vars,
                                          regs);
    case NODE_TYPE_OPERATOR_UNARY_NOT:
      return executeSimpleExpressionNot(node, trx, argv, startPos, vars, regs);
    case NODE_TYPE_OPERATOR_BINARY_AND:
    case NODE_TYPE_OPERATOR_BINARY_OR:
      return executeSimpleExpressionAndOr(node, trx, argv, startPos, vars,
                                          regs);
    case NODE_TYPE_OPERATOR_BINARY_EQ:
    case NODE_TYPE_OPERATOR_BINARY_NE:
    case NODE_TYPE_OPERATOR_BINARY_LT:
    case NODE_TYPE_OPERATOR_BINARY_LE:
    case NODE_TYPE_OPERATOR_BINARY_GT:
    case NODE_TYPE_OPERATOR_BINARY_GE:
    case NODE_TYPE_OPERATOR_BINARY_IN:
    case NODE_TYPE_OPERATOR_BINARY_NIN:
      return executeSimpleExpressionComparison(node, trx, argv, startPos, vars,
                                               regs);
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_NE:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_LT:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_LE:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_GT:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_GE:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_IN:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN:
      return executeSimpleExpressionArrayComparison(node, trx, argv, startPos, vars,
                                                    regs);
    case NODE_TYPE_OPERATOR_TERNARY:
      return executeSimpleExpressionTernary(node, trx, argv, startPos, vars,
                                            regs);
    case NODE_TYPE_EXPANSION:
      return executeSimpleExpressionExpansion(node, trx, argv, startPos, vars,
                                              regs);
    case NODE_TYPE_ITERATOR:
      return executeSimpleExpressionIterator(node, trx, argv,
                                             startPos, vars, regs);
    case NODE_TYPE_OPERATOR_BINARY_PLUS:
    case NODE_TYPE_OPERATOR_BINARY_MINUS:
    case NODE_TYPE_OPERATOR_BINARY_TIMES:
    case NODE_TYPE_OPERATOR_BINARY_DIV:
    case NODE_TYPE_OPERATOR_BINARY_MOD:
      return executeSimpleExpressionArithmetic(node, trx, argv, startPos, vars,
                                               regs);
    default:
      std::string msg("unhandled type '");
      msg.append(node->getTypeString());
      msg.append("' in executeSimpleExpression()");
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, msg.c_str());
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether this is an attribute access of any degree (e.g. a.b,
/// a.b.c, ...)
////////////////////////////////////////////////////////////////////////////////

bool Expression::isAttributeAccess() const {
  return _node->isAttributeAccessForVariable();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether this is a reference access
////////////////////////////////////////////////////////////////////////////////

bool Expression::isReference() const {
  return (_node->type == arangodb::aql::NODE_TYPE_REFERENCE);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether this is a constant node
////////////////////////////////////////////////////////////////////////////////

bool Expression::isConstant() const { return _node->isConstant(); }

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify an expression
/// note that currently stringification is only supported for certain node types
////////////////////////////////////////////////////////////////////////////////

void Expression::stringify(arangodb::basics::StringBuffer* buffer) const {
  _node->stringify(buffer, true, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify an expression
/// note that currently stringification is only supported for certain node types
////////////////////////////////////////////////////////////////////////////////

void Expression::stringifyIfNotTooLong(
    arangodb::basics::StringBuffer* buffer) const {
  _node->stringify(buffer, true, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute an expression of type SIMPLE with ATTRIBUTE ACCESS
////////////////////////////////////////////////////////////////////////////////

AqlValue$ Expression::executeSimpleExpressionAttributeAccess(
    AstNode const* node, arangodb::AqlTransaction* trx,
    AqlItemBlock const* argv, size_t startPos,
    std::vector<Variable const*> const& vars,
    std::vector<RegisterId> const& regs) {
  // object lookup, e.g. users.name
  TRI_ASSERT(node->numMembers() == 1);

  auto member = node->getMemberUnchecked(0);
  auto name = static_cast<char const*>(node->getData());

  AqlValue$ result = executeSimpleExpression(member, trx, argv,
                                             startPos, vars, regs, false);

  AqlValueGuard guard(result);
  return result.get(name, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute an expression of type SIMPLE with INDEXED ACCESS
////////////////////////////////////////////////////////////////////////////////

AqlValue$ Expression::executeSimpleExpressionIndexedAccess(
    AstNode const* node, arangodb::AqlTransaction* trx,
    AqlItemBlock const* argv, size_t startPos,
    std::vector<Variable const*> const& vars,
    std::vector<RegisterId> const& regs) {
  // array lookup, e.g. users[0]
  // note: it depends on the type of the value whether an array lookup or an
  // object lookup is performed
  // for example, if the value is an object, then its elements might be accessed
  // like this:
  // users['name'] or even users['0'] (as '0' is a valid attribute name, too)
  // if the value is an array, then string indexes might also be used and will
  // be converted to integers, e.g.
  // users['0'] is the same as users[0], users['-2'] is the same as users[-2]
  // etc.
  TRI_ASSERT(node->numMembers() == 2);

  auto member = node->getMember(0);
  auto index = node->getMember(1);

  AqlValue$ result = executeSimpleExpression(member, trx, argv,
                                             startPos, vars, regs, false);

  AqlValueGuard guard(result);

  if (result.isArray()) {
    AqlValue$ indexResult = executeSimpleExpression(
        index, trx, argv, startPos, vars, regs, false);

    AqlValueGuard guard(indexResult);

    if (indexResult.isNumber()) {
      return result.at(indexResult.toInt64(), true);
    }
     
    if (indexResult.isString()) {
      std::string const value = indexResult.slice().copyString();

      try {
        // stoll() might throw an exception if the string is not a number
        int64_t position = static_cast<int64_t>(std::stoll(value));
        return result.at(position, true);
      } catch (...) {
        // no number found.
      }
    } 
      
    // fall-through to returning null
  } else if (result.isObject()) {
    AqlValue$ indexResult = executeSimpleExpression(
        index, trx, argv, startPos, vars, regs, false);
    
    AqlValueGuard guard(indexResult);

    if (indexResult.isNumber()) {
      std::string const indexString = std::to_string(indexResult.toInt64());
      return result.get(indexString, true);
    }
     
    if (indexResult.isString()) {
      std::string const indexString = indexResult.slice().copyString();
      return result.get(indexString, true);
    } 

    // fall-through to returning null
  }

  return AqlValue$(VelocyPackHelper::NullValue());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute an expression of type SIMPLE with ARRAY
////////////////////////////////////////////////////////////////////////////////

AqlValue$ Expression::executeSimpleExpressionArray(
    AstNode const* node, arangodb::AqlTransaction* trx,
    AqlItemBlock const* argv, size_t startPos,
    std::vector<Variable const*> const& vars,
    std::vector<RegisterId> const& regs) {
  if (node->isConstant()) {
    // this will not create a copy
    return AqlValue$(node->computeValue().begin()); 
  }

  size_t const n = node->numMembers();

  VPackBuilder builder;
  builder.openArray();

  for (size_t i = 0; i < n; ++i) {
    auto member = node->getMemberUnchecked(i);
    AqlValue$ result = executeSimpleExpression(member, trx, argv,
                                               startPos, vars, regs, false);

    AqlValueGuard guard(result);
    result.toVelocyPack(trx, builder);
  }

  builder.close();
  return AqlValue$(builder);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute an expression of type SIMPLE with OBJECT
////////////////////////////////////////////////////////////////////////////////

AqlValue$ Expression::executeSimpleExpressionObject(
    AstNode const* node, arangodb::AqlTransaction* trx,
    AqlItemBlock const* argv, size_t startPos,
    std::vector<Variable const*> const& vars,
    std::vector<RegisterId> const& regs) {
  if (node->isConstant()) {
    // this will not create a copy
    return AqlValue$(node->computeValue().begin()); 
  }

  VPackBuilder builder;
  builder.openObject();

  size_t const n = node->numMembers();
  for (size_t i = 0; i < n; ++i) {
    auto member = node->getMemberUnchecked(i);
    TRI_ASSERT(member->type == NODE_TYPE_OBJECT_ELEMENT);
    // key
    builder.add(VPackValue(std::string(member->getStringValue(), member->getStringLength())));

    // value
    member = member->getMember(0);

    AqlValue$ result = executeSimpleExpression(member, trx, argv,
                                               startPos, vars, regs, false);
    AqlValueGuard guard(result);
    result.toVelocyPack(trx, builder);
  }

  builder.close();
  return AqlValue$(builder);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute an expression of type SIMPLE with VALUE
////////////////////////////////////////////////////////////////////////////////

AqlValue$ Expression::executeSimpleExpressionValue(AstNode const* node) {
  // this will not create a copy
  return AqlValue$(node->computeValue().begin()); 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute an expression of type SIMPLE with REFERENCE
////////////////////////////////////////////////////////////////////////////////

AqlValue$ Expression::executeSimpleExpressionReference(
    AstNode const* node, arangodb::AqlTransaction* trx,
    AqlItemBlock const* argv, size_t startPos,
    std::vector<Variable const*> const& vars,
    std::vector<RegisterId> const& regs, bool doCopy) {
  auto v = static_cast<Variable const*>(node->getData());

  {
    auto it = _variables.find(v);

    if (it != _variables.end()) {
      return AqlValue$((*it).second);
    }
  }

  size_t i = 0;
  for (auto it = vars.begin(); it != vars.end(); ++it, ++i) {
    if ((*it)->name == v->name) {
      if (doCopy) {
        return argv->getValueReference(startPos, regs[i]).clone();
      }
      return argv->getValueReference(startPos, regs[i]);
    }
  }

  std::string msg("variable not found '");
  msg.append(v->name);
  msg.append("' in executeSimpleExpression()");
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, msg.c_str());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute an expression of type SIMPLE with RANGE
////////////////////////////////////////////////////////////////////////////////

AqlValue$ Expression::executeSimpleExpressionRange(
    AstNode const* node, arangodb::AqlTransaction* trx,
    AqlItemBlock const* argv, size_t startPos,
    std::vector<Variable const*> const& vars,
    std::vector<RegisterId> const& regs) {

  auto low = node->getMember(0);
  auto high = node->getMember(1);
  AqlValue$ resultLow = executeSimpleExpression(low, trx, argv,
                                                startPos, vars, regs, false);

  AqlValueGuard guardLow(resultLow);

  AqlValue$ resultHigh = executeSimpleExpression(
      high, trx, argv, startPos, vars, regs, false);
  
  AqlValueGuard guardHigh(resultHigh);
 
  return AqlValue$(resultLow.toInt64(), resultHigh.toInt64());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute an expression of type SIMPLE with FCALL
////////////////////////////////////////////////////////////////////////////////

AqlValue$ Expression::executeSimpleExpressionFCall(
    AstNode const* node, arangodb::AqlTransaction* trx,
    AqlItemBlock const* argv, size_t startPos,
    std::vector<Variable const*> const& vars,
    std::vector<RegisterId> const& regs) {
  // some functions have C++ handlers
  // check if the called function has one
  auto func = static_cast<Function*>(node->getData());
  TRI_ASSERT(func->implementation != nullptr);

  auto member = node->getMemberUnchecked(0);
  TRI_ASSERT(member->type == NODE_TYPE_ARRAY);

  VPackBuilder builder;
  size_t const n = member->numMembers();

  VPackFunctionParameters parameters;
  parameters.reserve(n);

  try {
    for (size_t i = 0; i < n; ++i) {
      auto arg = member->getMemberUnchecked(i);

      if (arg->type == NODE_TYPE_COLLECTION) {
        builder.clear();
        builder.add(VPackValue(
              std::string(arg->getStringValue(), arg->getStringLength())));
        parameters.emplace_back(AqlValue$(builder));
      } else {
        parameters.emplace_back(executeSimpleExpression(arg, trx, argv,
              startPos, vars, regs, false));
      }
    }

    AqlValue$ a = func->implementation(_ast->query(), trx, parameters);

    for (auto& it : parameters) {
      it.destroy();
    }
    return a;
  } catch (...) {
    // prevent leak and rethrow error
    for (auto& it : parameters) {
      it.destroy();
    }
    throw;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute an expression of type SIMPLE with NOT
////////////////////////////////////////////////////////////////////////////////

AqlValue$ Expression::executeSimpleExpressionNot(
    AstNode const* node, arangodb::AqlTransaction* trx,
    AqlItemBlock const* argv, size_t startPos,
    std::vector<Variable const*> const& vars,
    std::vector<RegisterId> const& regs) {
  AqlValue$ operand =
      executeSimpleExpression(node->getMember(0), trx, argv,
                              startPos, vars, regs, false);

  bool const operandIsTrue = operand.toBoolean();
  operand.destroy();
  return AqlValue$(!operandIsTrue);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute an expression of type SIMPLE with AND or OR
////////////////////////////////////////////////////////////////////////////////

AqlValue$ Expression::executeSimpleExpressionAndOr(
    AstNode const* node, arangodb::AqlTransaction* trx,
    AqlItemBlock const* argv, size_t startPos,
    std::vector<Variable const*> const& vars,
    std::vector<RegisterId> const& regs) {
  AqlValue$ left =
      executeSimpleExpression(node->getMember(0), trx, argv,
                              startPos, vars, regs, true);

  if (node->type == NODE_TYPE_OPERATOR_BINARY_AND) {
    // AND
    if (left.toBoolean()) {
      // left is true => return right
      left.destroy();
      return executeSimpleExpression(node->getMember(1), trx, argv,
                                     startPos, vars, regs, true);
    }

    // left is false, return left
    return left;
  } 
    
  // OR
  if (left.toBoolean()) {
    // left is true => return left
    return left;
  }

  // left is false => return right
  left.destroy();
  return executeSimpleExpression(node->getMember(1), trx, argv,
                                 startPos, vars, regs, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute an expression of type SIMPLE with COMPARISON
////////////////////////////////////////////////////////////////////////////////

AqlValue$ Expression::executeSimpleExpressionComparison(
    AstNode const* node, arangodb::AqlTransaction* trx,
    AqlItemBlock const* argv, size_t startPos,
    std::vector<Variable const*> const& vars,
    std::vector<RegisterId> const& regs) {
  AqlValue$ left =
      executeSimpleExpression(node->getMember(0), trx, argv,
                              startPos, vars, regs, false);
  AqlValue$ right =
      executeSimpleExpression(node->getMember(1), trx, argv,
                              startPos, vars, regs, false);

  if (node->type == NODE_TYPE_OPERATOR_BINARY_IN ||
      node->type == NODE_TYPE_OPERATOR_BINARY_NIN) {
    // IN and NOT IN
    if (!right.isArray()) {
      // right operand must be a list, otherwise we return false
      // do not throw, but return "false" instead
      left.destroy();
      right.destroy();
      return AqlValue$(false);
    }

    bool result = findInArray(left, right, trx, node);

    if (node->type == NODE_TYPE_OPERATOR_BINARY_NIN) {
      // revert the result in case of a NOT IN
      result = !result;
    }
      
    left.destroy();
    right.destroy();

    return AqlValue$(result);
  }

  // all other comparison operators...

  // for equality and non-equality we can use a binary comparison
  bool compareUtf8 = (node->type != NODE_TYPE_OPERATOR_BINARY_EQ &&
                      node->type != NODE_TYPE_OPERATOR_BINARY_NE);

  int compareResult = AqlValue$::Compare(trx, left, right, compareUtf8);

  left.destroy();
  right.destroy();

  switch (node->type) {
    case NODE_TYPE_OPERATOR_BINARY_EQ:
      return AqlValue$(compareResult == 0);
    case NODE_TYPE_OPERATOR_BINARY_NE:
      return AqlValue$(compareResult != 0);
    case NODE_TYPE_OPERATOR_BINARY_LT:
      return AqlValue$(compareResult < 0);
    case NODE_TYPE_OPERATOR_BINARY_LE:
      return AqlValue$(compareResult <= 0);
    case NODE_TYPE_OPERATOR_BINARY_GT:
      return AqlValue$(compareResult > 0);
    case NODE_TYPE_OPERATOR_BINARY_GE:
      return AqlValue$(compareResult >= 0);
    default:
      std::string msg("unhandled type '");
      msg.append(node->getTypeString());
      msg.append("' in executeSimpleExpression()");
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, msg.c_str());
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute an expression of type SIMPLE with ARRAY COMPARISON
////////////////////////////////////////////////////////////////////////////////

AqlValue$ Expression::executeSimpleExpressionArrayComparison(
    AstNode const* node, arangodb::AqlTransaction* trx,
    AqlItemBlock const* argv, size_t startPos,
    std::vector<Variable const*> const& vars,
    std::vector<RegisterId> const& regs) {
  AqlValue$ left =
      executeSimpleExpression(node->getMember(0), trx, argv,
                              startPos, vars, regs, false);
  AqlValue$ right =
      executeSimpleExpression(node->getMember(1), trx, argv,
                              startPos, vars, regs, false);

  if (!left.isArray()) {
    // left operand must be an array
    left.destroy();
    right.destroy();
    // do not throw, but return "false" instead
    return AqlValue$(false);
  }
  
  if (node->type == NODE_TYPE_OPERATOR_BINARY_ARRAY_IN ||
      node->type == NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN) {
    // IN and NOT IN
    if (!right.isArray()) {
      // right operand must be an array, otherwise we return false
      left.destroy();
      right.destroy();
      // do not throw, but return "false" instead
      return AqlValue$(false);
    }
  }

  size_t const n = left.length();
  std::pair<size_t, size_t> requiredMatches = Quantifier::RequiredMatches(n, node->getMember(2));

  TRI_ASSERT(requiredMatches.first <= requiredMatches.second);
 
  // for equality and non-equality we can use a binary comparison
  bool const compareUtf8 = (node->type != NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ &&
                            node->type != NODE_TYPE_OPERATOR_BINARY_ARRAY_NE);

  bool overallResult = true;
  size_t matches = 0;
  size_t numLeft = n;

  for (size_t i = 0; i < n; ++i) {
    AqlValue$ leftItemValue = left.at(i, false);
    bool result;

    // IN and NOT IN
    if (node->type == NODE_TYPE_OPERATOR_BINARY_ARRAY_IN ||
        node->type == NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN) {
      result = findInArray(leftItemValue, right, trx, node);
    
      if (node->type == NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN) {
        // revert the result in case of a NOT IN
        result = !result;
      }
    }
    else {
      // other operators
      int compareResult = AqlValue$::Compare(trx, leftItemValue, right, compareUtf8);

      result = false;
      switch (node->type) {
        case NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ:
          result = (compareResult == 0);
          break;
        case NODE_TYPE_OPERATOR_BINARY_ARRAY_NE:
          result = (compareResult != 0);
          break;
        case NODE_TYPE_OPERATOR_BINARY_ARRAY_LT:
          result = (compareResult < 0);
          break;
        case NODE_TYPE_OPERATOR_BINARY_ARRAY_LE:
          result = (compareResult <= 0);
          break;
        case NODE_TYPE_OPERATOR_BINARY_ARRAY_GT:
          result = (compareResult > 0);
          break;
        case NODE_TYPE_OPERATOR_BINARY_ARRAY_GE:
          result = (compareResult >= 0);
          break;
        default:
          TRI_ASSERT(false);
      }
    }
    
    --numLeft;
      
    if (result) {
      ++matches;
      if (matches > requiredMatches.second) {
        // too many matches
        overallResult = false;
        break;
      }
      if (matches >= requiredMatches.first && matches + numLeft <= requiredMatches.second) {
        // enough matches
        overallResult = true;
        break;
      }
    }
    else {
      if (matches + numLeft < requiredMatches.first) {
        // too few matches
        overallResult = false;
        break;
      }
    }
  }
      
  left.destroy();
  right.destroy();
  return AqlValue$(overallResult);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute an expression of type SIMPLE with TERNARY
////////////////////////////////////////////////////////////////////////////////

AqlValue$ Expression::executeSimpleExpressionTernary(
    AstNode const* node, arangodb::AqlTransaction* trx,
    AqlItemBlock const* argv, size_t startPos,
    std::vector<Variable const*> const& vars,
    std::vector<RegisterId> const& regs) {
  AqlValue$ condition =
      executeSimpleExpression(node->getMember(0), trx, argv,
                              startPos, vars, regs, false);

  size_t position;
  if (condition.toBoolean()) {
    // return true part
    position = 1;
  }
  else {
    // return false part
    position = 2;
  }
  condition.destroy();

  return executeSimpleExpression(node->getMember(position), trx, argv,
                                 startPos, vars, regs, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute an expression of type SIMPLE with EXPANSION
////////////////////////////////////////////////////////////////////////////////

AqlValue$ Expression::executeSimpleExpressionExpansion(
    AstNode const* node, arangodb::AqlTransaction* trx,
    AqlItemBlock const* argv, size_t startPos,
    std::vector<Variable const*> const& vars,
    std::vector<RegisterId> const& regs) {
  TRI_ASSERT(node->numMembers() == 5);

  // LIMIT
  int64_t offset = 0;
  int64_t count = INT64_MAX;

  auto limitNode = node->getMember(3);

  if (limitNode->type != NODE_TYPE_NOP) {
    AqlValue$ sub =
        executeSimpleExpression(limitNode->getMember(0), trx,
                                argv, startPos, vars, regs, false);
    offset = sub.toInt64();
    sub.destroy();

    sub = executeSimpleExpression(limitNode->getMember(1), trx,
                                  argv, startPos, vars, regs, false);
    count = sub.toInt64();
    sub.destroy();
  }
    
  if (offset < 0 || count <= 0) {
    // no items to return... can already stop here
    return AqlValue$(VelocyPackHelper::ArrayValue());
  }

  // FILTER
  AstNode const* filterNode = node->getMember(2);

  if (filterNode->type == NODE_TYPE_NOP) {
    filterNode = nullptr;
  } else if (filterNode->isConstant()) {
    if (filterNode->isTrue()) {
      // filter expression is always true
      filterNode = nullptr;
    } else {
      // filter expression is always false
      return AqlValue$(VelocyPackHelper::ArrayValue());
    }
  }

  auto iterator = node->getMember(0);
  auto variable = static_cast<Variable*>(iterator->getMember(0)->getData());
  auto levels = node->getIntValue(true);

  AqlValue$ value;

  if (levels > 1) {
    // flatten value...
    value = executeSimpleExpression(node->getMember(0), trx,
                                    argv, startPos, vars, regs, false);
      
    if (!value.isArray()) {
      value.destroy();
      return AqlValue$(VelocyPackHelper::ArrayValue());
    }
    
    VPackBuilder builder;
    builder.openArray();
      
    // generate a new temporary for the flattened array
    std::function<void(AqlValue$ const&, int64_t)> flatten =
        [&](AqlValue$ const& v, int64_t level) {
          if (!v.isArray()) {
            return;
          }

          size_t const n = v.length();
          for (size_t i = 0; i < n; ++i) {
            AqlValue$ item = v.at(i, false);
            bool const isArray = item.isArray();

            if (!isArray || level == levels) {
              builder.add(item.slice());
            } else if (isArray && level < levels) {
              flatten(item, level + 1);
            }
            item.destroy();
          }
        };

    flatten(value, 1);
    value.destroy();
    builder.close();

    value = AqlValue$(builder);
  } else {
    value = executeSimpleExpression(node->getMember(0), trx,
                                    argv, startPos, vars, regs, false);

    if (!value.isArray()) {
      value.destroy();
      return AqlValue$(VelocyPackHelper::ArrayValue());
    }
  }

  // RETURN
  // the default is to return array member unmodified
  AstNode const* projectionNode = node->getMember(1);

  if (node->getMember(4)->type != NODE_TYPE_NOP) {
    // return projection
    projectionNode = node->getMember(4);
  }

  VPackBuilder builder;
  builder.openArray();

  size_t const n = value.length();
  for (size_t i = 0; i < n; ++i) {
    AqlValue$ item = value.at(i, false);
    AqlValueMaterializer materializer(trx);
    setVariable(variable, materializer.slice(item));

    bool takeItem = true;

    if (filterNode != nullptr) {
      // have a filter
      AqlValue$ sub = executeSimpleExpression(filterNode, trx,
                                              argv, startPos, vars, regs, false);
      takeItem = sub.toBoolean();
      sub.destroy();
    }

    if (takeItem && offset > 0) {
      // there is an offset in place
      --offset;
      takeItem = false;
    }

    if (takeItem) {
      AqlValue$ sub =
          executeSimpleExpression(projectionNode, trx, argv,
                                  startPos, vars, regs, false);
      sub.toVelocyPack(trx, builder);
      sub.destroy();
    }

    clearVariable(variable);
    item.destroy();

    if (takeItem && count > 0) {
      // number of items to pick was restricted
      if (--count == 0) {
        // done
        break;
      }
    }
  }

  builder.close();
  value.destroy();
  return AqlValue$(builder);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute an expression of type SIMPLE with ITERATOR
////////////////////////////////////////////////////////////////////////////////

AqlValue$ Expression::executeSimpleExpressionIterator(
    AstNode const* node, 
    arangodb::AqlTransaction* trx, AqlItemBlock const* argv, size_t startPos,
    std::vector<Variable const*> const& vars,
    std::vector<RegisterId> const& regs) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 2);

  return executeSimpleExpression(node->getMember(1), trx, argv,
                                 startPos, vars, regs, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute an expression of type SIMPLE with BINARY_* (+, -, * , /, %)
////////////////////////////////////////////////////////////////////////////////

AqlValue$ Expression::executeSimpleExpressionArithmetic(
    AstNode const* node, arangodb::AqlTransaction* trx,
    AqlItemBlock const* argv, size_t startPos,
    std::vector<Variable const*> const& vars,
    std::vector<RegisterId> const& regs) {
  AqlValue$ lhs = executeSimpleExpression(node->getMember(0),
                                          trx, argv, startPos, vars, regs, true);

  AqlValue$ rhs = executeSimpleExpression(node->getMember(1),
                                          trx, argv, startPos, vars, regs, true);

  if (lhs.isObject() || rhs.isObject()) {
    lhs.destroy();
    rhs.destroy();
    return AqlValue$(VelocyPackHelper::NullValue());
  }

  bool failed = false;
  double const l = lhs.toDouble(failed);
  lhs.destroy();

  if (failed) {
    return AqlValue$(VelocyPackHelper::NullValue());
  }

  double const r = rhs.toDouble(failed);
  rhs.destroy();

  if (failed) {
    return AqlValue$(VelocyPackHelper::NullValue());
  }

  VPackBuilder builder;

  switch (node->type) {
    case NODE_TYPE_OPERATOR_BINARY_PLUS:
      builder.add(VPackValue(l + r));
      return AqlValue$(builder);
    case NODE_TYPE_OPERATOR_BINARY_MINUS:
      builder.add(VPackValue(l - r));
      return AqlValue$(builder);
    case NODE_TYPE_OPERATOR_BINARY_TIMES:
      builder.add(VPackValue(l * r));
      return AqlValue$(builder);
    case NODE_TYPE_OPERATOR_BINARY_DIV:
      if (r == 0.0) {
        RegisterWarning(_ast, "/", TRI_ERROR_QUERY_DIVISION_BY_ZERO);
        return AqlValue$(VelocyPackHelper::NullValue());
      }
      return AqlValue$(builder);
    case NODE_TYPE_OPERATOR_BINARY_MOD:
      if (r == 0.0) {
        RegisterWarning(_ast, "/", TRI_ERROR_QUERY_DIVISION_BY_ZERO);
        return AqlValue$(VelocyPackHelper::NullValue());
      }
      builder.add(VPackValue(fmod(l, r)));
      return AqlValue$(builder);
    default:
      return AqlValue$(VelocyPackHelper::NullValue());
  }
}

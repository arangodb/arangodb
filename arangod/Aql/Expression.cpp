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
#include "Aql/ExpressionContext.h"
#include "Aql/BaseExpressionContext.h"
#include "Aql/Function.h"
#include "Aql/Functions.h"
#include "Aql/Quantifier.h"
#include "Aql/Query.h"
#include "Aql/V8Executor.h"
#include "Aql/V8Expression.h"
#include "Aql/Variable.h"
#include "Basics/Exceptions.h"
#include "Basics/StringBuffer.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/VPackStringBufferAdapter.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;
using VelocyPackHelper = arangodb::basics::VelocyPackHelper;

namespace {

/// @brief register warning
static void RegisterWarning(arangodb::aql::Ast const* ast,
                            char const* functionName, int code) {
  std::string msg;

  if (code == TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH) {
    msg = arangodb::basics::Exception::FillExceptionString(code, functionName);
  } else {
    if (strlen(functionName) <= 2) {
      // only an operator but no "real" function
      msg.append("in operator ");
      msg.append(functionName);
      msg.append(": ");
    } else {
      msg.append("in function '");
      msg.append(functionName);
      msg.append("()': ");
    }
    msg.append(TRI_errno_string(code));
  }

  ast->query()->registerWarning(code, msg.c_str());
}

}

/// @brief create the expression
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
      _expressionContext(nullptr) {
  TRI_ASSERT(_ast != nullptr);
  TRI_ASSERT(_executor != nullptr);
  TRI_ASSERT(_node != nullptr);
}

/// @brief create an expression from VPack
Expression::Expression(Ast* ast, arangodb::velocypack::Slice const& slice)
    : Expression(ast, new AstNode(ast, slice.get("expression"))) {}

/// @brief destroy the expression
Expression::~Expression() {
  if (_built) {
    switch (_type) {
      case JSON:
        TRI_ASSERT(_data != nullptr);
        delete[] _data;
        break;

      case ATTRIBUTE_SYSTEM: 
      case ATTRIBUTE_DYNAMIC: {
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

/// @brief return all variables used in the expression
void Expression::variables(std::unordered_set<Variable const*>& result) const {
  Ast::getReferencedVariables(_node, result);
}

/// @brief execute the expression
AqlValue Expression::execute(transaction::Methods* trx, ExpressionContext* ctx,
                             bool& mustDestroy) {
  if (!_built) {
    buildExpression(trx);
  }

  TRI_ASSERT(_type != UNPROCESSED);
  TRI_ASSERT(_built);
  _expressionContext = ctx;

  // and execute
  switch (_type) {
    case JSON: {
      mustDestroy = false;
      TRI_ASSERT(_data != nullptr);
      return AqlValue(_data);
    }

    case SIMPLE: {
      return executeSimpleExpression(_node, trx, mustDestroy, true);
    }

    case ATTRIBUTE_SYSTEM: {
      TRI_ASSERT(_accessor != nullptr);
      return _accessor->getSystem(trx, ctx, mustDestroy);
    }

    case ATTRIBUTE_DYNAMIC: {
      TRI_ASSERT(_accessor != nullptr);
      return _accessor->getDynamic(trx, ctx, mustDestroy);
    }

    case V8: {
      TRI_ASSERT(_func != nullptr);
      ISOLATE;
      return _func->execute(isolate, _ast->query(), trx, ctx, mustDestroy);
    }

    case UNPROCESSED: {
      // fall-through to exception
    }
  }

  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                 "invalid simple expression");

}

/// @brief execute the expression
/// TODO DEPRECATED
AqlValue Expression::execute(transaction::Methods* trx,
                             AqlItemBlock const* argv, size_t startPos,
                             std::vector<Variable const*> const& vars,
                             std::vector<RegisterId> const& regs,
                             bool& mustDestroy) {
  BaseExpressionContext ctx(startPos, argv, vars, regs);
  return execute(trx, &ctx, mustDestroy);
}

/// @brief replace variables in the expression with other variables
void Expression::replaceVariables(
    std::unordered_map<VariableId, Variable const*> const& replacements) {
  _node = _ast->clone(_node);
  TRI_ASSERT(_node != nullptr);

  _node = _ast->replaceVariables(const_cast<AstNode*>(_node), replacements);
  
  if ((_type == ATTRIBUTE_SYSTEM || _type == ATTRIBUTE_DYNAMIC) && _accessor != nullptr) {
    _accessor->replaceVariable(replacements);
  }

  invalidate();
}

/// @brief replace a variable reference in the expression with another
/// expression (e.g. inserting c = `a + b` into expression `c + 1` so the latter
/// becomes `a + b + 1`
void Expression::replaceVariableReference(Variable const* variable,
                                          AstNode const* node) {
  _node = _ast->clone(_node);
  TRI_ASSERT(_node != nullptr);

  _node = _ast->replaceVariableReference(const_cast<AstNode*>(_node), variable,
                                         node);
  invalidate();

  if (_type == ATTRIBUTE_SYSTEM || _type == ATTRIBUTE_DYNAMIC) {
    if (_built) {
      delete _accessor;
      _accessor = nullptr;
      _built = false;
    }
    // must even set back the expression type so the expression will be analyzed
    // again
    _type = UNPROCESSED;
  } else if (_type == SIMPLE) {
    // must rebuild the expression completely, as it may have changed drastically
    _built = false;
    _type = UNPROCESSED;
    _node->clearFlagsRecursive(); // recursively delete the node's flags
  }

  const_cast<AstNode*>(_node)->clearFlags();
  _attributes.clear();
  _hasDeterminedAttributes = false;
}

void Expression::replaceAttributeAccess(Variable const* variable,
                                        std::vector<std::string> const& attribute) {
  _node = _ast->clone(_node);
  TRI_ASSERT(_node != nullptr);

  _node = _ast->replaceAttributeAccess(const_cast<AstNode*>(_node), variable, attribute);
  invalidate();

  if (_type == ATTRIBUTE_SYSTEM || _type == ATTRIBUTE_DYNAMIC) {
    if (_built) {
      delete _accessor;
      _accessor = nullptr;
      _built = false;
    }
    // must even set back the expression type so the expression will be analyzed
    // again
    _type = UNPROCESSED;
  } else if (_type == SIMPLE) {
    // must rebuild the expression completely, as it may have changed drastically
    _built = false;
    _type = UNPROCESSED;
    _node->clearFlagsRecursive(); // recursively delete the node's flags
  }

  const_cast<AstNode*>(_node)->clearFlags();
  _attributes.clear();
  _hasDeterminedAttributes = false;
}

/// @brief invalidates an expression
/// this only has an effect for V8-based functions, which need to be created,
/// used and destroyed in the same context. when a V8 function is used across
/// multiple V8 contexts, it must be invalidated in between
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

/// @brief find a value in an AQL list node
/// this performs either a binary search (if the node is sorted) or a
/// linear search (if the node is not sorted)
bool Expression::findInArray(AqlValue const& left, AqlValue const& right,
                             transaction::Methods* trx,
                             AstNode const* node) const {
  TRI_ASSERT(right.isArray());

  size_t const n = right.length();

  if (n >= AstNode::SortNumberThreshold && 
      (node->getMember(1)->isSorted() ||
      ((node->type == NODE_TYPE_OPERATOR_BINARY_IN || 
        node->type == NODE_TYPE_OPERATOR_BINARY_NIN) && node->getBoolValue()))) {
    // node values are sorted. can use binary search
    size_t l = 0;
    size_t r = n - 1;

    while (true) {
      // determine midpoint
      size_t m = l + ((r - l) / 2);

      bool localMustDestroy;
      AqlValue a = right.at(trx, m, localMustDestroy, false);
      AqlValueGuard guard(a, localMustDestroy);

      int compareResult = AqlValue::Compare(trx, left, a, true);

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

  // if right operand of IN/NOT IN is a range, we can use an optimized search
  if (right.isRange()) {
    // but only if left operand is a number
    if (!left.isNumber()) {
      // a non-number will never be contained in the range
      return false;
    }

    // check if conversion to int64 would be lossy
    int64_t value = left.toInt64(trx);
    if (left.toDouble(trx) == static_cast<double>(value)) {
      // no loss
      Range const* r = right.range();
      TRI_ASSERT(r != nullptr);

      return r->isIn(value);
    }
    // fall-through to linear search
  }
    
  // use linear search
  for (size_t i = 0; i < n; ++i) {
    bool mustDestroy;
    AqlValue a = right.at(trx, i, mustDestroy, false);
    AqlValueGuard guard(a, mustDestroy);

    int compareResult = AqlValue::Compare(trx, left, a, false);

    if (compareResult == 0) {
      // item found in the list
      return true;
    }
  }

  return false;
}

/// @brief analyze the expression (determine its type etc.)
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
      std::vector<std::string> parts{_node->getString()};

      while (member->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
        parts.insert(parts.begin(), member->getString());
        member = member->getMemberUnchecked(0);
      }

      if (member->type == NODE_TYPE_REFERENCE) {
        auto v = static_cast<Variable const*>(member->getData());

        // specialize the simple expression into an attribute accessor
        _accessor = new AttributeAccessor(std::move(parts), v);
        if (_accessor->isDynamic()) {
          _type = ATTRIBUTE_DYNAMIC;
        } else {
          _type = ATTRIBUTE_SYSTEM;
        }
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

/// @brief build the expression
void Expression::buildExpression(transaction::Methods* trx) {
  TRI_ASSERT(!_built);

  if (_type == UNPROCESSED) {
    analyzeExpression();
  }

  if (_type == JSON) {
    TRI_ASSERT(_data == nullptr);
    // generate a constant value
    transaction::BuilderLeaser builder(trx);
    _node->toVelocyPackValue(*builder.get());

    _data = new uint8_t[static_cast<size_t>(builder->size())];
    memcpy(_data, builder->data(), static_cast<size_t>(builder->size()));
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

/// @brief execute an expression of type SIMPLE, the convention is that
/// the resulting AqlValue will be destroyed outside eventually
AqlValue Expression::executeSimpleExpression(
    AstNode const* node, transaction::Methods* trx, 
    bool& mustDestroy, bool doCopy) {

  switch (node->type) {
    case NODE_TYPE_ATTRIBUTE_ACCESS:
      return executeSimpleExpressionAttributeAccess(node, trx, mustDestroy);
    case NODE_TYPE_INDEXED_ACCESS:
      return executeSimpleExpressionIndexedAccess(node, trx, mustDestroy);
    case NODE_TYPE_ARRAY:
      return executeSimpleExpressionArray(node, trx, mustDestroy);
    case NODE_TYPE_OBJECT:
      return executeSimpleExpressionObject(node, trx, mustDestroy);
    case NODE_TYPE_VALUE:
      return executeSimpleExpressionValue(node, trx, mustDestroy);
    case NODE_TYPE_REFERENCE:
      return executeSimpleExpressionReference(node, trx, mustDestroy, doCopy);
    case NODE_TYPE_FCALL:
      return executeSimpleExpressionFCall(node, trx, mustDestroy);
    case NODE_TYPE_RANGE:
      return executeSimpleExpressionRange(node, trx, mustDestroy);
    case NODE_TYPE_OPERATOR_UNARY_NOT:
      return executeSimpleExpressionNot(node, trx, mustDestroy);
    case NODE_TYPE_OPERATOR_UNARY_PLUS:
      return executeSimpleExpressionPlus(node, trx, mustDestroy);
    case NODE_TYPE_OPERATOR_UNARY_MINUS:
      return executeSimpleExpressionMinus(node, trx, mustDestroy);
    case NODE_TYPE_OPERATOR_BINARY_AND:
      return executeSimpleExpressionAnd(node, trx, mustDestroy);
    case NODE_TYPE_OPERATOR_BINARY_OR:
      return executeSimpleExpressionOr(node, trx, mustDestroy);
    case NODE_TYPE_OPERATOR_BINARY_EQ:
    case NODE_TYPE_OPERATOR_BINARY_NE:
    case NODE_TYPE_OPERATOR_BINARY_LT:
    case NODE_TYPE_OPERATOR_BINARY_LE:
    case NODE_TYPE_OPERATOR_BINARY_GT:
    case NODE_TYPE_OPERATOR_BINARY_GE:
    case NODE_TYPE_OPERATOR_BINARY_IN:
    case NODE_TYPE_OPERATOR_BINARY_NIN:
      return executeSimpleExpressionComparison(node, trx, mustDestroy);
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_NE:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_LT:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_LE:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_GT:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_GE:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_IN:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN:
      return executeSimpleExpressionArrayComparison(node, trx, mustDestroy);
    case NODE_TYPE_OPERATOR_TERNARY:
      return executeSimpleExpressionTernary(node, trx, mustDestroy);
    case NODE_TYPE_EXPANSION:
      return executeSimpleExpressionExpansion(node, trx, mustDestroy);
    case NODE_TYPE_ITERATOR:
      return executeSimpleExpressionIterator(node, trx, mustDestroy);
    case NODE_TYPE_OPERATOR_BINARY_PLUS:
    case NODE_TYPE_OPERATOR_BINARY_MINUS:
    case NODE_TYPE_OPERATOR_BINARY_TIMES:
    case NODE_TYPE_OPERATOR_BINARY_DIV:
    case NODE_TYPE_OPERATOR_BINARY_MOD:
      return executeSimpleExpressionArithmetic(node, trx, mustDestroy);
    case NODE_TYPE_OPERATOR_NARY_AND:
    case NODE_TYPE_OPERATOR_NARY_OR:
      return executeSimpleExpressionNaryAndOr(node, trx, mustDestroy);
    default:
      std::string msg("unhandled type '");
      msg.append(node->getTypeString());
      msg.append("' in executeSimpleExpression()");
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, msg);
  }
}

/// @brief check whether this is an attribute access of any degree (e.g. a.b,
/// a.b.c, ...)
bool Expression::isAttributeAccess() const {
  return _node->isAttributeAccessForVariable();
}

/// @brief check whether this is a reference access
bool Expression::isReference() const {
  return (_node->type == arangodb::aql::NODE_TYPE_REFERENCE);
}

/// @brief check whether this is a constant node
bool Expression::isConstant() const { return _node->isConstant(); }

/// @brief stringify an expression
/// note that currently stringification is only supported for certain node types
void Expression::stringify(arangodb::basics::StringBuffer* buffer) const {
  _node->stringify(buffer, true, false);
}

/// @brief stringify an expression
/// note that currently stringification is only supported for certain node types
void Expression::stringifyIfNotTooLong(
    arangodb::basics::StringBuffer* buffer) const {
  _node->stringify(buffer, true, true);
}

/// @brief execute an expression of type SIMPLE with ATTRIBUTE ACCESS
/// always creates a copy
AqlValue Expression::executeSimpleExpressionAttributeAccess(
    AstNode const* node, transaction::Methods* trx,
    bool& mustDestroy) {
  // object lookup, e.g. users.name
  TRI_ASSERT(node->numMembers() == 1);

  auto member = node->getMemberUnchecked(0);
  char const* name = static_cast<char const*>(node->getData());

  bool localMustDestroy;
  AqlValue result = executeSimpleExpression(member, trx, localMustDestroy, false);
  AqlValueGuard guard(result, localMustDestroy);

  return result.get(trx, std::string(name, node->getStringLength()), mustDestroy, true);
}

/// @brief execute an expression of type SIMPLE with INDEXED ACCESS
AqlValue Expression::executeSimpleExpressionIndexedAccess(
    AstNode const* node, transaction::Methods* trx,
    bool& mustDestroy) {
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

  auto member = node->getMemberUnchecked(0);
  auto index = node->getMemberUnchecked(1);
  
  mustDestroy = false; 
  AqlValue result = executeSimpleExpression(member, trx, mustDestroy, false);

  AqlValueGuard guard(result, mustDestroy);

  if (result.isArray()) {
    AqlValue indexResult = executeSimpleExpression(
        index, trx, mustDestroy, false);

    AqlValueGuard guard(indexResult, mustDestroy);

    if (indexResult.isNumber()) {
      return result.at(trx, indexResult.toInt64(trx), mustDestroy, true);
    }
     
    if (indexResult.isString()) {
      std::string const value = indexResult.slice().copyString();

      try {
        // stoll() might throw an exception if the string is not a number
        int64_t position = static_cast<int64_t>(std::stoll(value));
        return result.at(trx, position, mustDestroy, true);
      } catch (...) {
        // no number found.
      }
    } 
      
    // fall-through to returning null
  } else if (result.isObject()) {
    AqlValue indexResult = executeSimpleExpression(
        index, trx, mustDestroy, false);
    
    AqlValueGuard guard(indexResult, mustDestroy);

    if (indexResult.isNumber()) {
      std::string const indexString = std::to_string(indexResult.toInt64(trx));
      return result.get(trx, indexString, mustDestroy, true);
    }
     
    if (indexResult.isString()) {
      std::string const indexString = indexResult.slice().copyString();
      return result.get(trx, indexString, mustDestroy, true);
    } 

    // fall-through to returning null
  }

  return AqlValue(VelocyPackHelper::NullValue());
}

/// @brief execute an expression of type SIMPLE with ARRAY
AqlValue Expression::executeSimpleExpressionArray(
    AstNode const* node, transaction::Methods* trx,
    bool& mustDestroy) {
  
  mustDestroy = false;
  if (node->isConstant()) {
    // this will not create a copy
    return AqlValue(node->computeValue().begin()); 
  }

  size_t const n = node->numMembers();

  if (n == 0) {
    return AqlValue(VelocyPackHelper::EmptyArrayValue());
  }

  transaction::BuilderLeaser builder(trx);
  builder->openArray();

  for (size_t i = 0; i < n; ++i) {
    auto member = node->getMemberUnchecked(i);
    bool localMustDestroy = false;
    AqlValue result = executeSimpleExpression(member, trx, localMustDestroy, false);
    AqlValueGuard guard(result, localMustDestroy);
    result.toVelocyPack(trx, *builder.get(), false);
  }

  builder->close();
  mustDestroy = true; // AqlValue contains builder contains dynamic data
  return AqlValue(builder.get());
}

/// @brief execute an expression of type SIMPLE with OBJECT
AqlValue Expression::executeSimpleExpressionObject(
    AstNode const* node, transaction::Methods* trx,
    bool& mustDestroy) {

  mustDestroy = false;
  if (node->isConstant()) {
    // this will not create a copy
    return AqlValue(node->computeValue().begin()); 
  }
  
  size_t const n = node->numMembers();

  if (n == 0) {
    return AqlValue(VelocyPackHelper::EmptyObjectValue());
  }

  // unordered map to make object keys unique afterwards
  std::unordered_map<std::string, size_t> uniqueKeyValues;
  bool isUnique = true;
  bool const mustCheckUniqueness = node->mustCheckUniqueness();

  transaction::BuilderLeaser builder(trx);
  builder->openObject();

  for (size_t i = 0; i < n; ++i) {
    auto member = node->getMemberUnchecked(i);

    // key
    if (member->type == NODE_TYPE_CALCULATED_OBJECT_ELEMENT) {
      bool localMustDestroy;
      AqlValue result = executeSimpleExpression(member->getMember(0), trx, localMustDestroy, false);
      AqlValueGuard guard(result, localMustDestroy);

      // make sure key is a string, and convert it if not
      transaction::StringBufferLeaser buffer(trx);
      arangodb::basics::VPackStringBufferAdapter adapter(buffer->stringBuffer());

      AqlValueMaterializer materializer(trx);
      VPackSlice slice = materializer.slice(result, false);

      Functions::Stringify(trx, adapter, slice);
      
      builder->add(VPackValuePair(buffer->begin(), buffer->length(), VPackValueType::String));

      if (mustCheckUniqueness) {
        std::string key(buffer->begin(), buffer->length());

        // note each individual object key name with latest value position
        auto it = uniqueKeyValues.find(key);

        if (it == uniqueKeyValues.end()) {
          // unique key
          uniqueKeyValues.emplace(std::move(key), i);
        } else {
          // duplicate key
          (*it).second = i;
          isUnique = false;
        }
      }

      // value
      member = member->getMember(1);
    } else {
      TRI_ASSERT(member->type == NODE_TYPE_OBJECT_ELEMENT);

      builder->add(VPackValuePair(member->getStringValue(), member->getStringLength(), VPackValueType::String));

      if (mustCheckUniqueness) {
        std::string key(member->getString());

        // note each individual object key name with latest value position
        auto it = uniqueKeyValues.find(key);

        if (it == uniqueKeyValues.end()) {
          // unique key
          uniqueKeyValues.emplace(std::move(key), i);
        } else {
          // duplicate key
          (*it).second = i;
          isUnique = false;
        }
      }
    
      // value
      member = member->getMember(0);
    }

    bool localMustDestroy;
    AqlValue result = executeSimpleExpression(member, trx, localMustDestroy, false);
    AqlValueGuard guard(result, localMustDestroy);
    result.toVelocyPack(trx, *builder.get(), false);
  }

  builder->close();
    
  mustDestroy = true; // AqlValue contains builder contains dynamic data

  if (!isUnique) {
    // must make the object keys unique now
    
    // we must have at least two members...
    TRI_ASSERT(n > 1);
    
    VPackSlice nonUnique = builder->slice();
     
    transaction::BuilderLeaser unique(trx);
    unique->openObject();

    size_t pos = 0;
    // iterate over all attributes of the non-unique object
    VPackObjectIterator it(nonUnique, true);
    while (it.valid()) {
      // key should be a string. ignore it if not
      VPackSlice key = it.key();
      if (key.isString()) {
        // check if this instance of the key is the one we need to hand out
        auto it2 = uniqueKeyValues.find(key.copyString());
        if (it2 != uniqueKeyValues.end()) {
          // key is in map. this is always expected
          if (pos == (*it2).second) {
            // this is the correct occurrence of the key
            unique->add((*it2).first, it.value());
          }
        }
      }
      // advance iterator and position
      it.next();
      ++pos;
    }

    unique->close();
    
    return AqlValue(*unique.get());
  }
    
  return AqlValue(*builder.get());
}

/// @brief execute an expression of type SIMPLE with VALUE
AqlValue Expression::executeSimpleExpressionValue(AstNode const* node,
                                                  transaction::Methods* trx,
                                                  bool& mustDestroy) {
  // this will not create a copy
  mustDestroy = false;
  return AqlValue(node->computeValue().begin()); 
}

/// @brief execute an expression of type SIMPLE with REFERENCE
AqlValue Expression::executeSimpleExpressionReference(
    AstNode const* node, transaction::Methods* trx,
    bool& mustDestroy, bool doCopy) {

  mustDestroy = false;
  auto v = static_cast<Variable const*>(node->getData());
  TRI_ASSERT(v != nullptr);

  if (!_variables.empty()) {
    auto it = _variables.find(v);

    if (it != _variables.end()) {
      // copy the slice we found
      mustDestroy = true;
      return AqlValue((*it).second);
    }
  }
  return _expressionContext->getVariableValue(v, doCopy, mustDestroy);
}

/// @brief execute an expression of type SIMPLE with RANGE
AqlValue Expression::executeSimpleExpressionRange(
    AstNode const* node, transaction::Methods* trx, bool& mustDestroy) {

  auto low = node->getMember(0);
  auto high = node->getMember(1);
  mustDestroy = false;

  AqlValue resultLow = executeSimpleExpression(low, trx, mustDestroy, false);

  AqlValueGuard guardLow(resultLow, mustDestroy);

  AqlValue resultHigh = executeSimpleExpression(
      high, trx, mustDestroy, false);
  
  AqlValueGuard guardHigh(resultHigh, mustDestroy);
 
  mustDestroy = true; // as we're creating a new range object
  return AqlValue(resultLow.toInt64(trx), resultHigh.toInt64(trx));
}

/// @brief execute an expression of type SIMPLE with FCALL
AqlValue Expression::executeSimpleExpressionFCall(
    AstNode const* node, transaction::Methods* trx, bool& mustDestroy) {

  mustDestroy = false;
  // only some functions have C++ handlers
  // check that the called function actually has one
  auto func = static_cast<Function*>(node->getData());
  TRI_ASSERT(func->implementation != nullptr);

  auto member = node->getMemberUnchecked(0);
  TRI_ASSERT(member->type == NODE_TYPE_ARRAY);

  size_t const n = member->numMembers();

  // use stack-based allocation for the first few function call
  // parameters. this saves a few heap allocations per function
  // call invocation 
  SmallVector<AqlValue>::allocator_type::arena_type arena;
  VPackFunctionParameters parameters{arena};
 
  // same here
  SmallVector<uint64_t>::allocator_type::arena_type arena2;
  SmallVector<uint64_t> destroyParameters{arena2};
  parameters.reserve(n);
  destroyParameters.reserve(n);

  try {
    for (size_t i = 0; i < n; ++i) {
      auto arg = member->getMemberUnchecked(i);

      if (arg->type == NODE_TYPE_COLLECTION) {
        parameters.emplace_back(arg->getStringValue(), arg->getStringLength());
        destroyParameters.push_back(1);
      } else {
        bool localMustDestroy;
        AqlValue a = executeSimpleExpression(arg, trx, localMustDestroy, false);
        parameters.emplace_back(a);
        destroyParameters.push_back(localMustDestroy ? 1 : 0);
      }
    }

    TRI_ASSERT(parameters.size() == destroyParameters.size());
    TRI_ASSERT(parameters.size() == n);

    AqlValue a = func->implementation(_ast->query(), trx, parameters);
    mustDestroy = true; // function result is always dynamic

    for (size_t i = 0; i < n; ++i) {
      if (destroyParameters[i]) {
        parameters[i].destroy();
      }
    }
    return a;
  } catch (...) {
    // prevent leak and rethrow error
    for (size_t i = 0; i < parameters.size(); ++i) {
      if (destroyParameters[i]) {
        parameters[i].destroy();
      }
    }
    throw;
  }
}

/// @brief execute an expression of type SIMPLE with NOT
AqlValue Expression::executeSimpleExpressionNot(
    AstNode const* node, transaction::Methods* trx, bool& mustDestroy) {

  mustDestroy = false;
  AqlValue operand =
      executeSimpleExpression(node->getMember(0), trx, mustDestroy, false);

  AqlValueGuard guard(operand, mustDestroy);
  bool const operandIsTrue = operand.toBoolean();

  mustDestroy = false; // only a boolean
  return AqlValue(!operandIsTrue);
}

/// @brief execute an expression of type SIMPLE with +
AqlValue Expression::executeSimpleExpressionPlus(AstNode const* node,
                                                 transaction::Methods* trx,
                                                 bool& mustDestroy) {
  mustDestroy = false;
  AqlValue operand =
      executeSimpleExpression(node->getMember(0), trx, mustDestroy, false);

  AqlValueGuard guard(operand, mustDestroy);
  
  if (operand.isNumber()) {
    VPackSlice const s = operand.slice();

    if (s.isSmallInt() || s.isInt()) {
      // can use int64
      return AqlValue(s.getNumber<int64_t>());
    } else if (s.isUInt()) {
      // can use uint64
      return AqlValue(s.getUInt());
    }
    // fallthrouh intentional
  }

  // use a double value for all other cases
  bool failed = false;
  double value = operand.toDouble(trx, failed);

  if (failed) {
    value = 0.0;
  }
  
  return AqlValue(+value);
}

/// @brief execute an expression of type SIMPLE with -
AqlValue Expression::executeSimpleExpressionMinus(AstNode const* node,
                                                  transaction::Methods* trx,
                                                  bool& mustDestroy) {
  mustDestroy = false;
  AqlValue operand =
      executeSimpleExpression(node->getMember(0), trx, mustDestroy, false);

  AqlValueGuard guard(operand, mustDestroy);
    
  if (operand.isNumber()) {
    VPackSlice const s = operand.slice();
    if (s.isSmallInt()) {
      // can use int64
      return AqlValue(-s.getNumber<int64_t>());
    } else if (s.isInt()) {
      int64_t v = s.getNumber<int64_t>();
      if (v != INT64_MIN) {
        // can use int64
        return AqlValue(-v);
      }
    }
    // fallthrouh intentional
  }
 
  // TODO: handle integer values separately here 
  bool failed = false;
  double value = operand.toDouble(trx, failed);

  if (failed) {
    value = 0.0;
  }
  
  return AqlValue(-value);
}

/// @brief execute an expression of type SIMPLE with AND
AqlValue Expression::executeSimpleExpressionAnd(
    AstNode const* node, transaction::Methods* trx, bool& mustDestroy) {

  AqlValue left =
      executeSimpleExpression(node->getMemberUnchecked(0), trx, mustDestroy, true);

  if (left.toBoolean()) {
    // left is true => return right
    if (mustDestroy) { left.destroy(); }
    return executeSimpleExpression(node->getMemberUnchecked(1), trx, mustDestroy, true);
  }

  // left is false, return left
  return left;
}

/// @brief execute an expression of type SIMPLE with OR
AqlValue Expression::executeSimpleExpressionOr(
    AstNode const* node, transaction::Methods* trx, bool& mustDestroy) {

  AqlValue left =
      executeSimpleExpression(node->getMemberUnchecked(0), trx, mustDestroy, true);

  if (left.toBoolean()) {
    // left is true => return left
    return left;
  }

  // left is false => return right
  if (mustDestroy) { left.destroy(); }
  return executeSimpleExpression(node->getMemberUnchecked(1), trx, mustDestroy, true);
}

/// @brief execute an expression of type SIMPLE with AND or OR
AqlValue Expression::executeSimpleExpressionNaryAndOr(
    AstNode const* node, transaction::Methods* trx, bool& mustDestroy) {
  size_t count = node->numMembers();
  if (count == 0) {
    // There is nothing to evaluate. So this is always true
    mustDestroy = true;
    return AqlValue(true);
  }

  // AND
  if (node->type == NODE_TYPE_OPERATOR_NARY_AND) {
    for (size_t i = 0; i < count; ++i) {
      AqlValue check =
          executeSimpleExpression(node->getMemberUnchecked(i), trx, mustDestroy, true);
      if (!check.toBoolean()) {
        // Check is false. Return it.
        return check;
      }

      if (mustDestroy) {
        check.destroy();
      }
    }
    mustDestroy = true;
    return AqlValue(true);
  }

  // OR
  for (size_t i = 0; i < count; ++i) {
    AqlValue check =
        executeSimpleExpression(node->getMemberUnchecked(i), trx, mustDestroy, true);
    if (check.toBoolean()) {
      // Check is true. Return it.
      return check;
    }

    if (mustDestroy) {
      check.destroy();
    }
  }
  mustDestroy = true;
  return AqlValue(false);
}

/// @brief execute an expression of type SIMPLE with COMPARISON
AqlValue Expression::executeSimpleExpressionComparison(
    AstNode const* node, transaction::Methods* trx, bool& mustDestroy) {

  AqlValue left =
      executeSimpleExpression(node->getMemberUnchecked(0), trx, mustDestroy, false);
  AqlValueGuard guardLeft(left, mustDestroy);
    
  AqlValue right =
      executeSimpleExpression(node->getMemberUnchecked(1), trx, mustDestroy, false);
  AqlValueGuard guardRight(right, mustDestroy);

  mustDestroy = false; // we're returning a boolean only

  if (node->type == NODE_TYPE_OPERATOR_BINARY_IN ||
      node->type == NODE_TYPE_OPERATOR_BINARY_NIN) {
    // IN and NOT IN
    if (!right.isArray()) {
      // right operand must be an array, otherwise we return false
      // do not throw, but return "false" instead
      return AqlValue(false);
    }

    bool result = findInArray(left, right, trx, node);

    if (node->type == NODE_TYPE_OPERATOR_BINARY_NIN) {
      // revert the result in case of a NOT IN
      result = !result;
    }
      
    return AqlValue(result);
  }

  // all other comparison operators...

  // for equality and non-equality we can use a binary comparison
  bool compareUtf8 = (node->type != NODE_TYPE_OPERATOR_BINARY_EQ &&
                      node->type != NODE_TYPE_OPERATOR_BINARY_NE);

  int compareResult = AqlValue::Compare(trx, left, right, compareUtf8);

  switch (node->type) {
    case NODE_TYPE_OPERATOR_BINARY_EQ:
      return AqlValue(compareResult == 0);
    case NODE_TYPE_OPERATOR_BINARY_NE:
      return AqlValue(compareResult != 0);
    case NODE_TYPE_OPERATOR_BINARY_LT:
      return AqlValue(compareResult < 0);
    case NODE_TYPE_OPERATOR_BINARY_LE:
      return AqlValue(compareResult <= 0);
    case NODE_TYPE_OPERATOR_BINARY_GT:
      return AqlValue(compareResult > 0);
    case NODE_TYPE_OPERATOR_BINARY_GE:
      return AqlValue(compareResult >= 0);
    default:
      std::string msg("unhandled type '");
      msg.append(node->getTypeString());
      msg.append("' in executeSimpleExpression()");
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, msg);
  }
}

/// @brief execute an expression of type SIMPLE with ARRAY COMPARISON
AqlValue Expression::executeSimpleExpressionArrayComparison(
    AstNode const* node, transaction::Methods* trx, bool& mustDestroy) {

  AqlValue left =
      executeSimpleExpression(node->getMember(0), trx, mustDestroy, false);
  AqlValueGuard guardLeft(left, mustDestroy);

  AqlValue right =
      executeSimpleExpression(node->getMember(1), trx, mustDestroy, false);
  AqlValueGuard guardRight(right, mustDestroy);

  mustDestroy = false; // we're returning a boolean only

  if (!left.isArray()) {
    // left operand must be an array
    // do not throw, but return "false" instead
    return AqlValue(false);
  }
  
  if (node->type == NODE_TYPE_OPERATOR_BINARY_ARRAY_IN ||
      node->type == NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN) {
    // IN and NOT IN
    if (!right.isArray()) {
      // right operand must be an array, otherwise we return false
      // do not throw, but return "false" instead
      return AqlValue(false);
    }
  }

  size_t const n = left.length();
  
  if (n == 0) {
    if (Quantifier::IsAllOrNone(node->getMember(2))) {
      // [] ALL ... 
      // [] NONE ...
      return AqlValue(true);
    } else {
      // [] ANY ...
      return AqlValue(false);
    }
  }

  std::pair<size_t, size_t> requiredMatches = Quantifier::RequiredMatches(n, node->getMember(2));

  TRI_ASSERT(requiredMatches.first <= requiredMatches.second);
 
  // for equality and non-equality we can use a binary comparison
  bool const compareUtf8 = (node->type != NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ &&
                            node->type != NODE_TYPE_OPERATOR_BINARY_ARRAY_NE);

  bool overallResult = true;
  size_t matches = 0;
  size_t numLeft = n;

  for (size_t i = 0; i < n; ++i) {
    bool localMustDestroy;
    AqlValue leftItemValue = left.at(trx, i, localMustDestroy, false);
    AqlValueGuard guard(leftItemValue, localMustDestroy);

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
      int compareResult = AqlValue::Compare(trx, leftItemValue, right, compareUtf8);

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
  
  TRI_ASSERT(!mustDestroy); 
  return AqlValue(overallResult);
}

/// @brief execute an expression of type SIMPLE with TERNARY
AqlValue Expression::executeSimpleExpressionTernary(
    AstNode const* node, transaction::Methods* trx, bool& mustDestroy) {

  AqlValue condition =
      executeSimpleExpression(node->getMember(0), trx, mustDestroy, false);

  AqlValueGuard guardCondition(condition, mustDestroy);
  
  size_t position;
  if (condition.toBoolean()) {
    // return true part
    position = 1;
  }
  else {
    // return false part
    position = 2;
  }

  return executeSimpleExpression(node->getMemberUnchecked(position), trx, mustDestroy, true);
}

/// @brief execute an expression of type SIMPLE with EXPANSION
AqlValue Expression::executeSimpleExpressionExpansion(
    AstNode const* node, transaction::Methods* trx, bool& mustDestroy) {
  TRI_ASSERT(node->numMembers() == 5);
  mustDestroy = false;

  // LIMIT
  int64_t offset = 0;
  int64_t count = INT64_MAX;

  auto limitNode = node->getMember(3);

  if (limitNode->type != NODE_TYPE_NOP) {
    bool localMustDestroy;
    AqlValue subOffset =
        executeSimpleExpression(limitNode->getMember(0), trx, localMustDestroy, false);
    offset = subOffset.toInt64(trx);
    if (localMustDestroy) { subOffset.destroy(); }

    AqlValue subCount = executeSimpleExpression(limitNode->getMember(1), trx, localMustDestroy, false);
    count = subCount.toInt64(trx);
    if (localMustDestroy) { subCount.destroy(); }
  }

  if (offset < 0 || count <= 0) {
    // no items to return... can already stop here
    return AqlValue(VelocyPackHelper::EmptyArrayValue());
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
      return AqlValue(VelocyPackHelper::EmptyArrayValue());
    }
  }

  auto iterator = node->getMember(0);
  auto variable = static_cast<Variable*>(iterator->getMember(0)->getData());
  auto levels = node->getIntValue(true);

  AqlValue value;

  if (levels > 1) {
    // flatten value...
    bool localMustDestroy;
    AqlValue a = executeSimpleExpression(node->getMember(0), trx, localMustDestroy, false);

    AqlValueGuard guard(a, localMustDestroy);
      
    if (!a.isArray()) {
      TRI_ASSERT(!mustDestroy);
      return AqlValue(VelocyPackHelper::EmptyArrayValue());
    }
    
    VPackBuilder builder;
    builder.openArray();
      
    // generate a new temporary for the flattened array
    std::function<void(AqlValue const&, int64_t)> flatten =
        [&](AqlValue const& v, int64_t level) {
          if (!v.isArray()) {
            return;
          }

          size_t const n = v.length();
          for (size_t i = 0; i < n; ++i) {
            bool localMustDestroy;
            AqlValue item = v.at(trx, i, localMustDestroy, false);
            AqlValueGuard guard(item, localMustDestroy);

            bool const isArray = item.isArray();

            if (!isArray || level == levels) {
              builder.add(item.slice());
            } else if (isArray && level < levels) {
              flatten(item, level + 1);
            }
          }
        };

    flatten(a, 1);
    builder.close();

    mustDestroy = true; // builder = dynamic data
    value = AqlValue(builder);
  } else {
    bool localMustDestroy;
    AqlValue a = executeSimpleExpression(node->getMember(0), trx, localMustDestroy, false);
    
    AqlValueGuard guard(a, localMustDestroy);

    if (!a.isArray()) {
      TRI_ASSERT(!mustDestroy);
      return AqlValue(VelocyPackHelper::EmptyArrayValue());
    }

    mustDestroy = localMustDestroy; // maybe we need to destroy...
    guard.steal(); // guard is not responsible anymore
    value = a;
  }

  AqlValueGuard guard(value, mustDestroy);

  // RETURN
  // the default is to return array member unmodified
  AstNode const* projectionNode = node->getMember(1);

  if (node->getMember(4)->type != NODE_TYPE_NOP) {
    // return projection
    projectionNode = node->getMember(4);
  }

  if (filterNode == nullptr && 
      projectionNode->type == NODE_TYPE_REFERENCE &&
      value.isArray() && 
      offset == 0 &&
      count == INT64_MAX) {
    // no filter and no projection... we can return the array as it is
    auto other = static_cast<Variable const*>(projectionNode->getData());

    if (other->id == variable->id) {
      // simplify `v[*]` to just `v` if it's already an array
      mustDestroy = true;
      guard.steal();
      return value; 
    }
  }

  VPackBuilder builder;
  builder.openArray();

  size_t const n = value.length();
  for (size_t i = 0; i < n; ++i) {
    bool localMustDestroy;
    AqlValue item = value.at(trx, i, localMustDestroy, false);
    AqlValueGuard guard(item, localMustDestroy);

    AqlValueMaterializer materializer(trx);
    setVariable(variable, materializer.slice(item, false));

    bool takeItem = true;

    if (filterNode != nullptr) {
      // have a filter
      bool localMustDestroy;
      AqlValue sub = executeSimpleExpression(filterNode, trx, localMustDestroy, false);

      takeItem = sub.toBoolean();
      if (localMustDestroy) { sub.destroy(); }
    }

    if (takeItem && offset > 0) {
      // there is an offset in place
      --offset;
      takeItem = false;
    }

    if (takeItem) {
      bool localMustDestroy;
      AqlValue sub =
          executeSimpleExpression(projectionNode, trx, localMustDestroy, false);
      sub.toVelocyPack(trx, builder, false);
      if (localMustDestroy) { sub.destroy(); }
    }

    clearVariable(variable);

    if (takeItem && count > 0) {
      // number of items to pick was restricted
      if (--count == 0) {
        // done
        break;
      }
    }
  }

  builder.close();
  mustDestroy = true;
  return AqlValue(builder); // builder = dynamic data
}

/// @brief execute an expression of type SIMPLE with ITERATOR
AqlValue Expression::executeSimpleExpressionIterator(
    AstNode const* node, transaction::Methods* trx, bool& mustDestroy) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 2);

  return executeSimpleExpression(node->getMember(1), trx, mustDestroy, true);
}

/// @brief execute an expression of type SIMPLE with BINARY_* (+, -, * , /, %)
AqlValue Expression::executeSimpleExpressionArithmetic(
    AstNode const* node, transaction::Methods* trx, bool& mustDestroy) {

  AqlValue lhs = executeSimpleExpression(node->getMemberUnchecked(0), trx, mustDestroy, true);
  AqlValueGuard guardLhs(lhs, mustDestroy);

  AqlValue rhs = executeSimpleExpression(node->getMemberUnchecked(1), trx, mustDestroy, true);
  AqlValueGuard guardRhs(rhs, mustDestroy);

  mustDestroy = false;

  bool failed = false;
  double l = lhs.toDouble(trx, failed);

  if (failed) {
    TRI_ASSERT(!mustDestroy);
    l = 0.0;
  }

  double r = rhs.toDouble(trx, failed);

  if (failed) {
    TRI_ASSERT(!mustDestroy);
    r = 0.0;
  }

  if (r == 0.0) {
    if (node->type == NODE_TYPE_OPERATOR_BINARY_DIV) {
      // division by zero
      RegisterWarning(_ast, "/", TRI_ERROR_QUERY_DIVISION_BY_ZERO);
      TRI_ASSERT(!mustDestroy);
      return AqlValue(VelocyPackHelper::NullValue());
    } else if (node->type == NODE_TYPE_OPERATOR_BINARY_MOD) {
      // modulo zero
      RegisterWarning(_ast, "%", TRI_ERROR_QUERY_DIVISION_BY_ZERO);
      TRI_ASSERT(!mustDestroy);
      return AqlValue(VelocyPackHelper::NullValue());
    }
  }

  mustDestroy = false;
  double result;

  switch (node->type) {
    case NODE_TYPE_OPERATOR_BINARY_PLUS:
      result = l + r;
      break;
    case NODE_TYPE_OPERATOR_BINARY_MINUS:
      result = l - r;
      break;
    case NODE_TYPE_OPERATOR_BINARY_TIMES:
      result = l * r;
      break;
    case NODE_TYPE_OPERATOR_BINARY_DIV:
      result = l / r;
      break;
    case NODE_TYPE_OPERATOR_BINARY_MOD:
      result = fmod(l, r);
      break;
    default:
      return AqlValue(VelocyPackHelper::ZeroValue());
  }
  
  // this will convert NaN, +inf & -inf to null
  return AqlValue(result);
}

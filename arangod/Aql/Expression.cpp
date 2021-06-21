////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Function.h"
#include "Aql/Functions.h"
#include "Aql/Quantifier.h"
#include "Aql/QueryContext.h"
#include "Aql/Range.h"
#include "Aql/V8Executor.h"
#include "Aql/Variable.h"
#include "Aql/AqlValueMaterializer.h"
#include "Basics/Exceptions.h"
#include "Basics/NumberUtils.h"
#include "Basics/StringBuffer.h"
#include "Basics/VPackStringBufferAdapter.h"
#include "Basics/VelocyPackHelper.h"
#include "Transaction/Context.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "V8/v8-globals.h"
#include "V8/v8-vpack.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/StringRef.h>
#include <velocypack/velocypack-aliases.h>

#include <v8.h>

using namespace arangodb;
using namespace arangodb::aql;
using VelocyPackHelper = arangodb::basics::VelocyPackHelper;

/// @brief create the expression
Expression::Expression(Ast* ast, AstNode* node)
    : _ast(ast), 
      _node(node), 
      _data(nullptr), 
      _type(UNPROCESSED), 
      _expressionContext(nullptr) {
  TRI_ASSERT(_ast != nullptr);
  TRI_ASSERT(_node != nullptr);

  TRI_ASSERT(_data == nullptr);
  TRI_ASSERT(_accessor == nullptr);

  determineType();
  TRI_ASSERT(_type != UNPROCESSED);
}

/// @brief create an expression from VPack
Expression::Expression(Ast* ast, arangodb::velocypack::Slice const& slice)
    : Expression(ast, ast->createNode(slice.get("expression"))) {
  TRI_ASSERT(_type != UNPROCESSED);
}

/// @brief destroy the expression
Expression::~Expression() { freeInternals(); }

/// @brief return all variables used in the expression
void Expression::variables(VarSet& result) const {
  Ast::getReferencedVariables(_node, result);
}

/// @brief execute the expression
AqlValue Expression::execute(ExpressionContext* ctx, bool& mustDestroy) {
  prepareForExecution(ctx);

  TRI_ASSERT(_type != UNPROCESSED);
      
  _expressionContext = ctx;
  auto guard = scopeGuard([this] {
    _expressionContext = nullptr;
  });

  // and execute
  switch (_type) {
    case JSON: {
      mustDestroy = false;
      TRI_ASSERT(_data != nullptr);
      return AqlValue(_data);
    }

    case SIMPLE: {
      return executeSimpleExpression(_node, mustDestroy, true);
    }

    case ATTRIBUTE_ACCESS: {
      TRI_ASSERT(_accessor != nullptr);
      auto resolver = ctx->trx().resolver();
      TRI_ASSERT(resolver != nullptr);
      return _accessor->get(*resolver, ctx, mustDestroy);
    }

    case UNPROCESSED: {
      // fall-through to exception
    }
  }

  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid expression type");
}

/// @brief replace variables in the expression with other variables
void Expression::replaceVariables(std::unordered_map<VariableId, Variable const*> const& replacements) {
  _node = _ast->clone(_node);
  TRI_ASSERT(_node != nullptr);

  _node = Ast::replaceVariables(const_cast<AstNode*>(_node), replacements);

  if (_type == ATTRIBUTE_ACCESS && _accessor != nullptr) {
    _accessor->replaceVariable(replacements);
  } else {
    freeInternals();
  }
}

/// @brief replace a variable reference in the expression with another
/// expression (e.g. inserting c = `a + b` into expression `c + 1` so the latter
/// becomes `a + b + 1`
void Expression::replaceVariableReference(Variable const* variable, AstNode const* node) {
  _node = _ast->clone(_node);
  TRI_ASSERT(_node != nullptr);

  _node = Ast::replaceVariableReference(const_cast<AstNode*>(_node), variable, node);
  invalidateAfterReplacements();
}

void Expression::replaceAttributeAccess(Variable const* variable,
                                        std::vector<std::string> const& attribute) {
  _node = _ast->clone(_node);
  TRI_ASSERT(_node != nullptr);

  _node = Ast::replaceAttributeAccess(const_cast<AstNode*>(_node), variable, attribute);
  invalidateAfterReplacements();
}

/// @brief free the internal data structures
void Expression::freeInternals() noexcept {
  switch (_type) {
    case JSON:
      delete[] _data;
      _data = nullptr;
      break;

    case ATTRIBUTE_ACCESS: {
      delete _accessor;
      _accessor = nullptr;
      break;
    }

    case SIMPLE:
    case UNPROCESSED: {
      // nothing to do
      break;
    }
  }
}

/// @brief reset internal attributes after variables in the expression were
/// changed
void Expression::invalidateAfterReplacements() {
  if (_type == ATTRIBUTE_ACCESS || _type == SIMPLE) {
    freeInternals();
    _node->clearFlagsRecursive();  // recursively delete the node's flags
  }

  const_cast<AstNode*>(_node)->clearFlags();

  // must even set back the expression type so the expression will be analyzed
  // again
   _type = UNPROCESSED;
  determineType();
}

/// @brief find a value in an AQL array node
/// this performs either a binary search (if the node is sorted) or a
/// linear search (if the node is not sorted)
bool Expression::findInArray(AqlValue const& left, AqlValue const& right,
                             VPackOptions const* vopts, AstNode const* node) const {
  TRI_ASSERT(right.isArray());

  size_t const n = right.length();

  if (n >= AstNode::SortNumberThreshold &&
      (node->getMember(1)->isSorted() ||
       ((node->type == NODE_TYPE_OPERATOR_BINARY_IN || node->type == NODE_TYPE_OPERATOR_BINARY_NIN) &&
        node->getBoolValue()))) {
    // node values are sorted. can use binary search
    size_t l = 0;
    size_t r = n - 1;

    while (true) {
      // determine midpoint
      size_t m = l + ((r - l) / 2);

      bool localMustDestroy;
      AqlValue a = right.at(m, localMustDestroy, false);
      AqlValueGuard guard(a, localMustDestroy);

      int compareResult = AqlValue::Compare(vopts, left, a, true);

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
    int64_t value = left.toInt64();
    if (left.toDouble() == static_cast<double>(value)) {
      // no loss
      Range const* r = right.range();
      TRI_ASSERT(r != nullptr);

      return r->isIn(value);
    }
    // fall-through to linear search
  }
  
  // use linear search

  if (!right.isRange() && !left.isRange()) {
    // optimization for the case in which rhs is a Velocypack array, and we 
    // can simply use a VelocyPack iterator to walk through it. this will
    // be a lot more efficient than using right.at(i) in case the array is
    // of type Compact (without any index tables)
    VPackSlice const lhs(left.slice());

    VPackArrayIterator it(right.slice());
    while (it.valid()) {
      int compareResult = arangodb::basics::VelocyPackHelper::compare(lhs, it.value(), false, vopts);
    
      if (compareResult == 0) {
        // item found in the list
        return true;
      }
      it.next();
    }
  } else {
    for (size_t i = 0; i < n; ++i) {
      bool mustDestroy;
      AqlValue a = right.at(i, mustDestroy, false);
      AqlValueGuard guard(a, mustDestroy);

      int compareResult = AqlValue::Compare(vopts, left, a, false);

      if (compareResult == 0) {
        // item found in the list
        return true;
      }
    }
  }

  return false;
}

/// @brief analyze the expression (determine its type)
void Expression::determineType() {
  TRI_ASSERT(_type == UNPROCESSED);
  
  TRI_ASSERT(_data == nullptr);
  TRI_ASSERT(_accessor == nullptr);

  if (_node->isConstant()) {
    // expression is a constant value
    _data = nullptr;
    _type = JSON;
    return;
  }

  // expression is a simple expression
  _type = SIMPLE;

  if (_node->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
    // optimization for attribute accesses
    TRI_ASSERT(_node->numMembers() == 1);
    auto member = _node->getMemberUnchecked(0);

    while (member->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
      member = member->getMemberUnchecked(0);
    }

    if (member->type == NODE_TYPE_REFERENCE) {
      _type = ATTRIBUTE_ACCESS;
    }
  }
}

void Expression::initAccessor(ExpressionContext* ctx) {
  TRI_ASSERT(_type == ATTRIBUTE_ACCESS);
  TRI_ASSERT(_accessor == nullptr);
  
  TRI_ASSERT(_node->numMembers() == 1);
  auto member = _node->getMemberUnchecked(0);
  std::vector<std::string> parts{_node->getString()};

  while (member->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
    parts.insert(parts.begin(), member->getString());
    member = member->getMemberUnchecked(0);
  }

  TRI_ASSERT(member->type == NODE_TYPE_REFERENCE);
  auto v = static_cast<Variable const*>(member->getData());

  TRI_ASSERT(ctx != nullptr);
  bool const dataIsFromColl = ctx->isDataFromCollection(v);
  // specialize the simple expression into an attribute accessor
  _accessor = AttributeAccessor::create(std::move(parts), v, dataIsFromColl);
  TRI_ASSERT(_accessor != nullptr);
}

/// @brief prepare the expression for execution
void Expression::prepareForExecution(ExpressionContext* ctx) {
  TRI_ASSERT(_type != UNPROCESSED);

  switch (_type) {
    case JSON: {
      if (_data == nullptr) {
        // generate a constant value
        transaction::BuilderLeaser builder(&ctx->trx());
        _node->toVelocyPackValue(*builder.get());

        _data = new uint8_t[static_cast<size_t>(builder->size())];
        memcpy(_data, builder->data(), static_cast<size_t>(builder->size()));
      }
      break;
    }
    case ATTRIBUTE_ACCESS: {
      if (_accessor == nullptr) {
        initAccessor(ctx);
      }
      break;
    }
    default: {}
  }
}

/// @brief execute an expression of type SIMPLE, the convention is that
/// the resulting AqlValue will be destroyed outside eventually
AqlValue Expression::executeSimpleExpression(AstNode const* node, 
                                             bool& mustDestroy, bool doCopy) {
  switch (node->type) {
    case NODE_TYPE_ATTRIBUTE_ACCESS:
      return executeSimpleExpressionAttributeAccess(node, mustDestroy, doCopy);
    case NODE_TYPE_INDEXED_ACCESS:
      return executeSimpleExpressionIndexedAccess(node, mustDestroy, doCopy);
    case NODE_TYPE_ARRAY:
      return executeSimpleExpressionArray(node, mustDestroy);
    case NODE_TYPE_OBJECT:
      return executeSimpleExpressionObject(node, mustDestroy);
    case NODE_TYPE_VALUE:
      return executeSimpleExpressionValue(node, mustDestroy);
    case NODE_TYPE_REFERENCE:
      return executeSimpleExpressionReference(node, mustDestroy, doCopy);
    case NODE_TYPE_FCALL:
      return executeSimpleExpressionFCall(node, mustDestroy);
    case NODE_TYPE_FCALL_USER:
      return executeSimpleExpressionFCallJS(node, mustDestroy);
    case NODE_TYPE_RANGE:
      return executeSimpleExpressionRange(node, mustDestroy);
    case NODE_TYPE_OPERATOR_UNARY_NOT:
      return executeSimpleExpressionNot(node, mustDestroy);
    case NODE_TYPE_OPERATOR_UNARY_PLUS:
      return executeSimpleExpressionPlus(node, mustDestroy);
    case NODE_TYPE_OPERATOR_UNARY_MINUS:
      return executeSimpleExpressionMinus(node, mustDestroy);
    case NODE_TYPE_OPERATOR_BINARY_AND:
      return executeSimpleExpressionAnd(node, mustDestroy);
    case NODE_TYPE_OPERATOR_BINARY_OR:
      return executeSimpleExpressionOr(node, mustDestroy);
    case NODE_TYPE_OPERATOR_BINARY_EQ:
    case NODE_TYPE_OPERATOR_BINARY_NE:
    case NODE_TYPE_OPERATOR_BINARY_LT:
    case NODE_TYPE_OPERATOR_BINARY_LE:
    case NODE_TYPE_OPERATOR_BINARY_GT:
    case NODE_TYPE_OPERATOR_BINARY_GE:
    case NODE_TYPE_OPERATOR_BINARY_IN:
    case NODE_TYPE_OPERATOR_BINARY_NIN:
      return executeSimpleExpressionComparison(node, mustDestroy);
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_NE:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_LT:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_LE:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_GT:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_GE:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_IN:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN:
      return executeSimpleExpressionArrayComparison(node, mustDestroy);
    case NODE_TYPE_OPERATOR_TERNARY:
      return executeSimpleExpressionTernary(node, mustDestroy);
    case NODE_TYPE_EXPANSION:
      return executeSimpleExpressionExpansion(node, mustDestroy);
    case NODE_TYPE_ITERATOR:
      return executeSimpleExpressionIterator(node, mustDestroy);
    case NODE_TYPE_OPERATOR_BINARY_PLUS:
    case NODE_TYPE_OPERATOR_BINARY_MINUS:
    case NODE_TYPE_OPERATOR_BINARY_TIMES:
    case NODE_TYPE_OPERATOR_BINARY_DIV:
    case NODE_TYPE_OPERATOR_BINARY_MOD:
      return executeSimpleExpressionArithmetic(node, mustDestroy);
    case NODE_TYPE_OPERATOR_NARY_AND:
    case NODE_TYPE_OPERATOR_NARY_OR:
      return executeSimpleExpressionNaryAndOr(node, mustDestroy);
    case NODE_TYPE_COLLECTION:
    case NODE_TYPE_VIEW:
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_NOT_IMPLEMENTED,
          std::string("node type '") + node->getTypeString() + "' is not supported in expressions");

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
void Expression::stringifyIfNotTooLong(arangodb::basics::StringBuffer* buffer) const {
  _node->stringify(buffer, true, true);
}

/// @brief execute an expression of type SIMPLE with ATTRIBUTE ACCESS
/// always creates a copy
AqlValue Expression::executeSimpleExpressionAttributeAccess(AstNode const* node,
                                                            bool& mustDestroy, bool doCopy) {
  // object lookup, e.g. users.name
  TRI_ASSERT(node->numMembers() == 1);

  auto member = node->getMemberUnchecked(0);

  bool localMustDestroy;
  AqlValue result = executeSimpleExpression(member, localMustDestroy, false);
  AqlValueGuard guard(result, localMustDestroy);
  auto* resolver = _expressionContext->trx().resolver();
  TRI_ASSERT(resolver != nullptr);

  return result.get(
      *resolver, 
      arangodb::velocypack::StringRef(static_cast<char const*>(node->getData()), node->getStringLength()), 
      mustDestroy, 
      true
  );
}

/// @brief execute an expression of type SIMPLE with INDEXED ACCESS
AqlValue Expression::executeSimpleExpressionIndexedAccess(AstNode const* node,
                                                          bool& mustDestroy, bool doCopy) {
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
  AqlValue result = executeSimpleExpression(member, mustDestroy, false);

  AqlValueGuard guard(result, mustDestroy);

  if (result.isArray()) {
    AqlValue indexResult = executeSimpleExpression(index, mustDestroy, false);

    AqlValueGuard guardIdx(indexResult, mustDestroy);

    if (indexResult.isNumber()) {
      return result.at(indexResult.toInt64(), mustDestroy, true);
    }

    if (indexResult.isString()) {
      VPackSlice s = indexResult.slice();
      TRI_ASSERT(s.isString());
      VPackValueLength l;
      char const* p = s.getString(l);

      bool valid;
      int64_t position = NumberUtils::atoi<int64_t>(p, p + l, valid);
      if (valid) {
        return result.at(position, mustDestroy, true);
      }
      // no number found.
    }

    // fall-through to returning null
  } else if (result.isObject()) {
    AqlValue indexResult = executeSimpleExpression(index, mustDestroy, false);

    AqlValueGuard guardIdx(indexResult, mustDestroy);

    if (indexResult.isNumber()) {
      std::string const indexString = std::to_string(indexResult.toInt64());
      auto* resolver = _expressionContext->trx().resolver();
      TRI_ASSERT(resolver != nullptr);
      return result.get(*resolver, indexString, mustDestroy, true);
    }

    if (indexResult.isString()) {
      VPackValueLength l;
      char const* p = indexResult.slice().getStringUnchecked(l);
      auto* resolver = _expressionContext->trx().resolver();
      TRI_ASSERT(resolver != nullptr);
      return result.get(*resolver, arangodb::velocypack::StringRef(p, l), mustDestroy, true);
    }

    // fall-through to returning null
  }

  return AqlValue(AqlValueHintNull());
}

/// @brief execute an expression of type SIMPLE with ARRAY
AqlValue Expression::executeSimpleExpressionArray(AstNode const* node,
                                                  bool& mustDestroy) {
  mustDestroy = false;
  if (node->isConstant()) {
    // this will not create a copy
    uint8_t const* cv = node->computedValue();
    if (cv != nullptr) {
      return AqlValue(cv);
    }
    transaction::BuilderLeaser builder(&_expressionContext->trx());
    return AqlValue(node->computeValue(builder.get()).begin());
  }

  size_t const n = node->numMembers();

  if (n == 0) {
    return AqlValue(AqlValueHintEmptyArray());
  }
    
  auto& trx = _expressionContext->trx();

  transaction::BuilderLeaser builder(&trx);
  builder->openArray();

  for (size_t i = 0; i < n; ++i) {
    auto member = node->getMemberUnchecked(i);
    bool localMustDestroy = false;
    AqlValue result = executeSimpleExpression(member, localMustDestroy, false);
    AqlValueGuard guard(result, localMustDestroy);
    result.toVelocyPack(&trx.vpackOptions(), *builder.get(), /*resolveExternals*/false,
                        /*allowUnindexed*/false);
  }

  builder->close();
  mustDestroy = true;  // AqlValue contains builder contains dynamic data
  return AqlValue(builder->slice(), builder->size());
}

/// @brief execute an expression of type SIMPLE with OBJECT
AqlValue Expression::executeSimpleExpressionObject(AstNode const* node,
                                                   bool& mustDestroy) {
  auto& trx = _expressionContext->trx();
  auto& vopts = trx.vpackOptions();

  mustDestroy = false;
  if (node->isConstant()) {
    // this will not create a copy
    uint8_t const* cv = node->computedValue();
    if (cv != nullptr) {
      return AqlValue(cv);
    }
    transaction::BuilderLeaser builder(&trx);
    return AqlValue(node->computeValue(builder.get()).begin());
  }

  size_t const n = node->numMembers();

  if (n == 0) {
    return AqlValue(AqlValueHintEmptyObject());
  }

  // unordered set for tracking unique object keys
  std::unordered_set<std::string> keys;
  bool const mustCheckUniqueness = node->mustCheckUniqueness();

  transaction::BuilderLeaser builder(&trx);
  builder->openObject();

  for (size_t i = 0; i < n; ++i) {
    auto member = node->getMemberUnchecked(i);

    // process attribute key, taking into account duplicates
    if (member->type == NODE_TYPE_CALCULATED_OBJECT_ELEMENT) {
      bool localMustDestroy;
      AqlValue result =
          executeSimpleExpression(member->getMember(0), localMustDestroy, false);
      AqlValueGuard guard(result, localMustDestroy);

      // make sure key is a string, and convert it if not
      transaction::StringBufferLeaser buffer(&trx);
      arangodb::basics::VPackStringBufferAdapter adapter(buffer->stringBuffer());

      AqlValueMaterializer materializer(&vopts);
      VPackSlice slice = materializer.slice(result, false);

      Functions::Stringify(&vopts, adapter, slice);

      if (mustCheckUniqueness) {
        std::string key(buffer->begin(), buffer->length());

        // prevent duplicate keys from being used
        auto it = keys.find(key);

        if (it != keys.end()) {
          // duplicate key
          continue;
        }

        // unique key
        builder->add(VPackValue(key));
        if (i != n - 1) {
          // track usage of key
          keys.emplace(std::move(key));
        }
      } else {
        builder->add(VPackValuePair(buffer->begin(), buffer->length(), VPackValueType::String));
      }

      // value
      member = member->getMember(1);
    } else {
      TRI_ASSERT(member->type == NODE_TYPE_OBJECT_ELEMENT);

      if (mustCheckUniqueness) {
        std::string key(member->getString());

        // track each individual object key
        auto it = keys.find(key);

        if (it != keys.end()) {
          // duplicate key
          continue;
        }

        // unique key
        builder->add(VPackValue(key));
        if (i != n - 1) {
          // track usage of key
          keys.emplace(std::move(key));
        }
      } else {
        builder->add(VPackValuePair(member->getStringValue(),
                                    member->getStringLength(), VPackValueType::String));
      }

      // value
      member = member->getMember(0);
    }

    // add the attribute value
    bool localMustDestroy;
    AqlValue result = executeSimpleExpression(member, localMustDestroy, false);
    AqlValueGuard guard(result, localMustDestroy);
    result.toVelocyPack(&vopts, *builder.get(), /*resolveExternals*/false,
                        /*allowUnindexed*/false);
  }

  builder->close();

  mustDestroy = true;  // AqlValue contains builder contains dynamic data

  return AqlValue(builder->slice(), builder->size());
}

/// @brief execute an expression of type SIMPLE with VALUE
AqlValue Expression::executeSimpleExpressionValue(AstNode const* node,
                                                  bool& mustDestroy) {
  // this will not create a copy
  mustDestroy = false;
  uint8_t const* cv = node->computedValue();
  if (cv != nullptr) {
    return AqlValue(cv);
  }
  transaction::BuilderLeaser builder(&_expressionContext->trx());
  return AqlValue(node->computeValue(builder.get()).begin());
}

/// @brief execute an expression of type SIMPLE with REFERENCE
AqlValue Expression::executeSimpleExpressionReference(AstNode const* node,
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
AqlValue Expression::executeSimpleExpressionRange(AstNode const* node,
                                                  bool& mustDestroy) {
  auto low = node->getMember(0);
  auto high = node->getMember(1);
  mustDestroy = false;

  AqlValue resultLow = executeSimpleExpression(low, mustDestroy, false);

  AqlValueGuard guardLow(resultLow, mustDestroy);

  AqlValue resultHigh = executeSimpleExpression(high, mustDestroy, false);

  AqlValueGuard guardHigh(resultHigh, mustDestroy);

  mustDestroy = true; // as we're creating a new range object
  return AqlValue(resultLow.toInt64(), resultHigh.toInt64());
}

/// @brief execute an expression of type SIMPLE with FCALL, dispatcher
AqlValue Expression::executeSimpleExpressionFCall(AstNode const* node, bool& mustDestroy) {
  // only some functions have C++ handlers
  // check that the called function actually has one
  auto func = static_cast<Function*>(node->getData());
  TRI_ASSERT(func != nullptr);
  if (func->hasCxxImplementation()) {
    return executeSimpleExpressionFCallCxx(node, mustDestroy);
  }
  return executeSimpleExpressionFCallJS(node, mustDestroy);
}

/// @brief execute an expression of type SIMPLE with FCALL, CXX version
AqlValue Expression::executeSimpleExpressionFCallCxx(AstNode const* node,
                                                     bool& mustDestroy) {
  TRI_ASSERT(node != nullptr);
  mustDestroy = false;
  auto func = static_cast<Function*>(node->getData());
  TRI_ASSERT(func != nullptr);
  TRI_ASSERT(func->hasCxxImplementation());

  auto member = node->getMemberUnchecked(0);
  TRI_ASSERT(member->type == NODE_TYPE_ARRAY);

  struct FunctionParameters {
    // use stack-based allocation for the first few function call
    // parameters. this saves a few heap allocations per function
    // call invocation
    ::arangodb::containers::SmallVector<AqlValue>::allocator_type::arena_type arena;
    VPackFunctionParameters parameters{arena};

    // same here
    ::arangodb::containers::SmallVector<uint64_t>::allocator_type::arena_type arena2;
    ::arangodb::containers::SmallVector<uint64_t> destroyParameters{arena2};

    explicit FunctionParameters(size_t n) {
      parameters.reserve(n);
      destroyParameters.reserve(n);
    }

    ~FunctionParameters() {
      for (size_t i = 0; i < destroyParameters.size(); ++i) {
        if (destroyParameters[i]) {
          parameters[i].destroy();
        }
      }
    }
  };

  size_t const n = member->numMembers();

  FunctionParameters params(n);

  for (size_t i = 0; i < n; ++i) {
    auto arg = member->getMemberUnchecked(i);

    if (arg->type == NODE_TYPE_COLLECTION) {
      params.parameters.emplace_back(arg->getStringValue(), arg->getStringLength());
      params.destroyParameters.push_back(1);
    } else {
      bool localMustDestroy;
      params.parameters.emplace_back(
          executeSimpleExpression(arg, localMustDestroy, false));
      params.destroyParameters.push_back(localMustDestroy ? 1 : 0);
    }
  }

  TRI_ASSERT(params.parameters.size() == params.destroyParameters.size());
  TRI_ASSERT(params.parameters.size() == n);

  AqlValue a = func->implementation(_expressionContext, *node, params.parameters);
  mustDestroy = true;  // function result is always dynamic

  return a;
}

AqlValue Expression::invokeV8Function(ExpressionContext* expressionContext,
                                      std::string const& jsName,
                                      std::string const& ucInvokeFN, char const* AFN,
                                      bool rethrowV8Exception, size_t callArgs,
                                      v8::Handle<v8::Value>* args, bool& mustDestroy) {
  ISOLATE;
  auto current = isolate->GetCurrentContext()->Global();
  auto context = TRI_IGETC;

  v8::Handle<v8::Value> module =
    current->Get(context, TRI_V8_ASCII_STRING(isolate, "_AQL")).FromMaybe(v8::Local<v8::Value>());
  if (module.IsEmpty() || !module->IsObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "unable to find global _AQL module");
  }

  v8::Handle<v8::Value> function =
    v8::Handle<v8::Object>::Cast(module)->Get(context,
                                              TRI_V8_STD_STRING(isolate, jsName)
                                              ).FromMaybe(v8::Local<v8::Value>());
  if (function.IsEmpty() || !function->IsFunction()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        std::string("unable to find AQL function '") + jsName + "'");
  }

  // actually call the V8 function
  v8::TryCatch tryCatch(isolate);
  v8::Handle<v8::Value> result =
    v8::Handle<v8::Function>::Cast(function)->Call(context, current, static_cast<int>(callArgs), args).FromMaybe(v8::Local<v8::Value>());

  try {
    V8Executor::HandleV8Error(tryCatch, result, nullptr, false);
  } catch (arangodb::basics::Exception const& ex) {
    if (rethrowV8Exception || ex.code() == TRI_ERROR_QUERY_FUNCTION_NOT_FOUND) {
      throw;
    }
    std::string message("while invoking '");
    message += ucInvokeFN + "' via '" + AFN + "': " + ex.message();
    expressionContext->registerWarning(ex.code(), message.c_str());
    return AqlValue(AqlValueHintNull());
  }
  if (result.IsEmpty() || result->IsUndefined()) {
    return AqlValue(AqlValueHintNull());
  }

  auto& trx = expressionContext->trx();
  transaction::BuilderLeaser builder(&trx);

  // can throw
  TRI_V8ToVPack(isolate, *builder.get(), result, false);

  mustDestroy = true;  // builder = dynamic data
  return AqlValue(builder->slice(), builder->size());
}

/// @brief execute an expression of type SIMPLE, JavaScript variant
AqlValue Expression::executeSimpleExpressionFCallJS(AstNode const* node,
                                                    bool& mustDestroy) {
  auto member = node->getMemberUnchecked(0);
  TRI_ASSERT(member->type == NODE_TYPE_ARRAY);

  mustDestroy = false;

  {
    ISOLATE;
    TRI_ASSERT(isolate != nullptr);
    TRI_V8_CURRENT_GLOBALS_AND_SCOPE;
    auto context = TRI_IGETC;

    VPackOptions const& options = _expressionContext->trx().vpackOptions();

    auto old = v8g->_expressionContext;
    v8g->_expressionContext = _expressionContext;
    TRI_DEFER(v8g->_expressionContext = old);

    std::string jsName;
    size_t const n = member->numMembers();
    size_t callArgs = (node->type == NODE_TYPE_FCALL_USER ? 2 : n);
    auto args = std::make_unique<v8::Handle<v8::Value>[]>(callArgs);

    if (node->type == NODE_TYPE_FCALL_USER) {
      // a call to a user-defined function
      jsName = "FCALL_USER";
      v8::Handle<v8::Array> params = v8::Array::New(isolate, static_cast<int>(n));

      for (size_t i = 0; i < n; ++i) {
        auto arg = member->getMemberUnchecked(i);

        bool localMustDestroy;
        AqlValue a = executeSimpleExpression(arg, localMustDestroy, false);
        AqlValueGuard guard(a, localMustDestroy);

        params->Set(context, static_cast<uint32_t>(i), a.toV8(isolate, &options)).FromMaybe(false);
      }

      // function name
      args[0] = TRI_V8_STD_STRING(isolate, node->getString());
      // call parameters
      args[1] = params;
      // args[2] will be null
    } else {
      // a call to a built-in V8 function
      auto func = static_cast<Function*>(node->getData());
      TRI_ASSERT(func != nullptr);
      TRI_ASSERT(func->hasV8Implementation());
      jsName = "AQL_" + func->name;

      for (size_t i = 0; i < n; ++i) {
        auto arg = member->getMemberUnchecked(i);

        if (arg->type == NODE_TYPE_COLLECTION) {
          // parameter conversion for NODE_TYPE_COLLECTION here
          args[i] = TRI_V8_ASCII_PAIR_STRING(isolate, arg->getStringValue(),
                                             arg->getStringLength());
        } else {
          bool localMustDestroy;
          AqlValue a = executeSimpleExpression(arg, localMustDestroy, false);
          AqlValueGuard guard(a, localMustDestroy);

          args[i] = a.toV8(isolate, &options);
        }
      }
    }

    return invokeV8Function(_expressionContext, jsName, "", "", true,
                            callArgs, args.get(), mustDestroy);
  }
}

/// @brief execute an expression of type SIMPLE with NOT
AqlValue Expression::executeSimpleExpressionNot(AstNode const* node, bool& mustDestroy) {
  mustDestroy = false;
  AqlValue operand = executeSimpleExpression(node->getMember(0), mustDestroy, false);

  AqlValueGuard guard(operand, mustDestroy);
  bool const operandIsTrue = operand.toBoolean();

  mustDestroy = false;  // only a boolean
  return AqlValue(AqlValueHintBool(!operandIsTrue));
}

/// @brief execute an expression of type SIMPLE with +
AqlValue Expression::executeSimpleExpressionPlus(AstNode const* node, bool& mustDestroy) {
  mustDestroy = false;
  AqlValue operand = executeSimpleExpression(node->getMember(0), mustDestroy, false);

  AqlValueGuard guard(operand, mustDestroy);

  if (operand.isNumber()) {
    VPackSlice const s = operand.slice();

    if (s.isSmallInt() || s.isInt()) {
      // can use int64
      return AqlValue(AqlValueHintInt(s.getNumber<int64_t>()));
    } else if (s.isUInt()) {
      // can use uint64
      return AqlValue(AqlValueHintUInt(s.getUInt()));
    }
    // fallthrouh intentional
  }

  // use a double value for all other cases
  bool failed = false;
  double value = operand.toDouble(failed);

  if (failed) {
    value = 0.0;
  }

  return AqlValue(AqlValueHintDouble(+value));
}

/// @brief execute an expression of type SIMPLE with -
AqlValue Expression::executeSimpleExpressionMinus(AstNode const* node,
                                                  bool& mustDestroy) {
  mustDestroy = false;
  AqlValue operand = executeSimpleExpression(node->getMember(0), mustDestroy, false);

  AqlValueGuard guard(operand, mustDestroy);

  if (operand.isNumber()) {
    VPackSlice const s = operand.slice();
    if (s.isSmallInt()) {
      // can use int64
      return AqlValue(AqlValueHintInt(-s.getNumber<int64_t>()));
    } else if (s.isInt()) {
      int64_t v = s.getNumber<int64_t>();
      if (v != INT64_MIN) {
        // can use int64
        return AqlValue(AqlValueHintInt(-v));
      }
    } else if (s.isUInt()) {
      uint64_t v = s.getNumber<uint64_t>();
      if (v <= uint64_t(INT64_MAX)) {
        // can use int64 too
        int64_t v = s.getNumber<int64_t>();
        return AqlValue(AqlValueHintInt(-v));
      }
    }
    // fallthrough intentional
  }

  bool failed = false;
  double value = operand.toDouble(failed);

  if (failed) {
    value = 0.0;
  }

  return AqlValue(AqlValueHintDouble(-value));
}

/// @brief execute an expression of type SIMPLE with AND
AqlValue Expression::executeSimpleExpressionAnd(AstNode const* node, bool& mustDestroy) {
  AqlValue left =
      executeSimpleExpression(node->getMemberUnchecked(0), mustDestroy, true);

  if (left.toBoolean()) {
    // left is true => return right
    if (mustDestroy) {
      left.destroy();
    }
    return executeSimpleExpression(node->getMemberUnchecked(1), mustDestroy, true);
  }

  // left is false, return left
  return left;
}

/// @brief execute an expression of type SIMPLE with OR
AqlValue Expression::executeSimpleExpressionOr(AstNode const* node, bool& mustDestroy) {
  AqlValue left =
      executeSimpleExpression(node->getMemberUnchecked(0), mustDestroy, true);

  if (left.toBoolean()) {
    // left is true => return left
    return left;
  }

  // left is false => return right
  if (mustDestroy) {
    left.destroy();
  }
  return executeSimpleExpression(node->getMemberUnchecked(1), mustDestroy, true);
}

/// @brief execute an expression of type SIMPLE with AND or OR
AqlValue Expression::executeSimpleExpressionNaryAndOr(AstNode const* node,
                                                      bool& mustDestroy) {
  mustDestroy = false;
  size_t count = node->numMembers();
  if (count == 0) {
    // There is nothing to evaluate. So this is always true
    return AqlValue(AqlValueHintBool(true));
  }

  // AND
  if (node->type == NODE_TYPE_OPERATOR_NARY_AND) {
    for (size_t i = 0; i < count; ++i) {
      bool localMustDestroy = false;
      AqlValue check = executeSimpleExpression(node->getMemberUnchecked(i),
                                               localMustDestroy, false);
      bool result = check.toBoolean();

      if (localMustDestroy) {
        check.destroy();
      }

      if (!result) {
        // we are allowed to return early here, because this is only called
        // in the context of index lookups
        return AqlValue(AqlValueHintBool(false));
      }
    }
    return AqlValue(AqlValueHintBool(true));
  }

  // OR
  for (size_t i = 0; i < count; ++i) {
    bool localMustDestroy = false;
    AqlValue check = executeSimpleExpression(node->getMemberUnchecked(i),
                                             localMustDestroy, true);
    bool result = check.toBoolean();

    if (localMustDestroy) {
      check.destroy();
    }

    if (result) {
      // we are allowed to return early here, because this is only called
      // in the context of index lookups
      return AqlValue(AqlValueHintBool(true));
    }
  }

  // anything else... we shouldn't get here
  TRI_ASSERT(false);
  return AqlValue(AqlValueHintBool(false));
}

/// @brief execute an expression of type SIMPLE with COMPARISON
AqlValue Expression::executeSimpleExpressionComparison(AstNode const* node,
                                                       bool& mustDestroy) {
  AqlValue left =
      executeSimpleExpression(node->getMemberUnchecked(0), mustDestroy, false);
  AqlValueGuard guardLeft(left, mustDestroy);

  AqlValue right =
      executeSimpleExpression(node->getMemberUnchecked(1), mustDestroy, false);
  AqlValueGuard guardRight(right, mustDestroy);

  mustDestroy = false;  // we're returning a boolean only

  auto& vopts = _expressionContext->trx().vpackOptions();
  if (node->type == NODE_TYPE_OPERATOR_BINARY_IN || node->type == NODE_TYPE_OPERATOR_BINARY_NIN) {
    // IN and NOT IN
    if (!right.isArray()) {
      // right operand must be an array, otherwise we return false
      // do not throw, but return "false" instead
      return AqlValue(AqlValueHintBool(false));
    }

    bool result = findInArray(left, right, &vopts, node);

    if (node->type == NODE_TYPE_OPERATOR_BINARY_NIN) {
      // revert the result in case of a NOT IN
      result = !result;
    }

    return AqlValue(AqlValueHintBool(result));
  }

  // all other comparison operators...

  // for equality and non-equality we can use a binary comparison
  bool compareUtf8 = (node->type != NODE_TYPE_OPERATOR_BINARY_EQ &&
                      node->type != NODE_TYPE_OPERATOR_BINARY_NE);

  int compareResult = AqlValue::Compare(&vopts, left, right, compareUtf8);

  switch (node->type) {
    case NODE_TYPE_OPERATOR_BINARY_EQ:
      return AqlValue(AqlValueHintBool(compareResult == 0));
    case NODE_TYPE_OPERATOR_BINARY_NE:
      return AqlValue(AqlValueHintBool(compareResult != 0));
    case NODE_TYPE_OPERATOR_BINARY_LT:
      return AqlValue(AqlValueHintBool(compareResult < 0));
    case NODE_TYPE_OPERATOR_BINARY_LE:
      return AqlValue(AqlValueHintBool(compareResult <= 0));
    case NODE_TYPE_OPERATOR_BINARY_GT:
      return AqlValue(AqlValueHintBool(compareResult > 0));
    case NODE_TYPE_OPERATOR_BINARY_GE:
      return AqlValue(AqlValueHintBool(compareResult >= 0));
    default:
      std::string msg("unhandled type '");
      msg.append(node->getTypeString());
      msg.append("' in executeSimpleExpression()");
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, msg);
  }
}

/// @brief execute an expression of type SIMPLE with ARRAY COMPARISON
AqlValue Expression::executeSimpleExpressionArrayComparison(AstNode const* node, bool& mustDestroy) {
  auto& vopts = _expressionContext->trx().vpackOptions();

  AqlValue left = executeSimpleExpression(node->getMember(0), mustDestroy, false);
  AqlValueGuard guardLeft(left, mustDestroy);

  AqlValue right = executeSimpleExpression(node->getMember(1), mustDestroy, false);
  AqlValueGuard guardRight(right, mustDestroy);

  mustDestroy = false;  // we're returning a boolean only

  if (!left.isArray()) {
    // left operand must be an array
    // do not throw, but return "false" instead
    return AqlValue(AqlValueHintBool(false));
  }

  if (node->type == NODE_TYPE_OPERATOR_BINARY_ARRAY_IN ||
      node->type == NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN) {
    // IN and NOT IN
    if (!right.isArray()) {
      // right operand must be an array, otherwise we return false
      // do not throw, but return "false" instead
      return AqlValue(AqlValueHintBool(false));
    }
  }

  size_t const n = left.length();

  if (n == 0) {
    if (Quantifier::IsAllOrNone(node->getMember(2))) {
      // [] ALL ...
      // [] NONE ...
      return AqlValue(AqlValueHintBool(true));
    } else {
      // [] ANY ...
      return AqlValue(AqlValueHintBool(false));
    }
  }

  std::pair<size_t, size_t> requiredMatches =
      Quantifier::RequiredMatches(n, node->getMember(2));

  TRI_ASSERT(requiredMatches.first <= requiredMatches.second);

  // for equality and non-equality we can use a binary comparison
  bool const compareUtf8 = (node->type != NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ &&
                            node->type != NODE_TYPE_OPERATOR_BINARY_ARRAY_NE);

  bool overallResult = true;
  size_t matches = 0;
  size_t numLeft = n;

  for (size_t i = 0; i < n; ++i) {
    bool localMustDestroy;
    AqlValue leftItemValue = left.at(i, localMustDestroy, false);
    AqlValueGuard guard(leftItemValue, localMustDestroy);

    bool result = false;

    // IN and NOT IN
    if (node->type == NODE_TYPE_OPERATOR_BINARY_ARRAY_IN ||
        node->type == NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN) {
      result = findInArray(leftItemValue, right, &vopts, node);

      if (node->type == NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN) {
        // revert the result in case of a NOT IN
        result = !result;
      }
    } else {
      // other operators
      int compareResult = AqlValue::Compare(&vopts, leftItemValue, right, compareUtf8);

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
      if (matches >= requiredMatches.first &&
          matches + numLeft <= requiredMatches.second) {
        // enough matches
        overallResult = true;
        break;
      }
    } else {
      if (matches + numLeft < requiredMatches.first) {
        // too few matches
        overallResult = false;
        break;
      }
    }
  }

  TRI_ASSERT(!mustDestroy);
  return AqlValue(AqlValueHintBool(overallResult));
}

/// @brief execute an expression of type SIMPLE with TERNARY
AqlValue Expression::executeSimpleExpressionTernary(AstNode const* node,
                                                    bool& mustDestroy) {
  if (node->numMembers() == 2) {
    AqlValue condition =
      executeSimpleExpression(node->getMember(0), mustDestroy, true);
    AqlValueGuard guard(condition, mustDestroy);

    if (condition.toBoolean()) {
      guard.steal();
      return condition;
    }
    return executeSimpleExpression(node->getMemberUnchecked(1), mustDestroy, true);
  }
  
  TRI_ASSERT(node->numMembers() == 3);

  AqlValue condition =
      executeSimpleExpression(node->getMember(0), mustDestroy, false);

  AqlValueGuard guardCondition(condition, mustDestroy);

  size_t position;
  if (condition.toBoolean()) {
    // return true part
    position = 1;
  } else {
    // return false part
    position = 2;
  }

  return executeSimpleExpression(node->getMemberUnchecked(position), mustDestroy, true);
}

/// @brief execute an expression of type SIMPLE with EXPANSION
AqlValue Expression::executeSimpleExpressionExpansion(AstNode const* node, bool& mustDestroy) {
  TRI_ASSERT(node->numMembers() == 5);
  mustDestroy = false;

  // LIMIT
  int64_t offset = 0;
  int64_t count = INT64_MAX;

  auto limitNode = node->getMember(3);

  if (limitNode->type != NODE_TYPE_NOP) {
    bool localMustDestroy;
    AqlValue subOffset =
        executeSimpleExpression(limitNode->getMember(0), localMustDestroy, false);
    offset = subOffset.toInt64();
    if (localMustDestroy) {
      subOffset.destroy();
    }

    AqlValue subCount =
        executeSimpleExpression(limitNode->getMember(1), localMustDestroy, false);
    count = subCount.toInt64();
    if (localMustDestroy) {
      subCount.destroy();
    }
  }

  if (offset < 0 || count <= 0) {
    // no items to return... can already stop here
    return AqlValue(AqlValueHintEmptyArray());
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
      return AqlValue(AqlValueHintEmptyArray());
    }
  }

  auto iterator = node->getMember(0);
  auto variable = static_cast<Variable*>(iterator->getMember(0)->getData());
  auto levels = node->getIntValue(true);

  AqlValue value;

  if (levels > 1) {
    // flatten value...
    bool localMustDestroy;
    AqlValue a = executeSimpleExpression(node->getMember(0), localMustDestroy, false);

    AqlValueGuard guard(a, localMustDestroy);

    if (!a.isArray()) {
      TRI_ASSERT(!mustDestroy);
      return AqlValue(AqlValueHintEmptyArray());
    }

    VPackBuffer<uint8_t> buffer;
    VPackBuilder builder(buffer);
    builder.openArray();

    // generate a new temporary for the flattened array
    std::function<void(AqlValue const&, int64_t)> flatten = [&](AqlValue const& v,
                                                                int64_t level) {
      if (!v.isArray()) {
        return;
      }

      size_t const n = v.length();
      for (size_t i = 0; i < n; ++i) {
        bool localMustDestroy;
        AqlValue item = v.at(i, localMustDestroy, false);
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

    mustDestroy = true;  // builder = dynamic data
    value = AqlValue(std::move(buffer));
  } else {
    bool localMustDestroy;
    AqlValue a = executeSimpleExpression(node->getMember(0), localMustDestroy, false);

    AqlValueGuard guard(a, localMustDestroy);

    if (!a.isArray()) {
      TRI_ASSERT(!mustDestroy);
      return AqlValue(AqlValueHintEmptyArray());
    }

    mustDestroy = localMustDestroy;  // maybe we need to destroy...
    guard.steal();                   // guard is not responsible anymore
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

  if (filterNode == nullptr && projectionNode->type == NODE_TYPE_REFERENCE &&
      value.isArray() && offset == 0 && count == INT64_MAX) {
    // no filter and no projection... we can return the array as it is
    auto other = static_cast<Variable const*>(projectionNode->getData());

    if (other->id == variable->id) {
      // simplify `v[*]` to just `v` if it's already an array
      mustDestroy = true;
      guard.steal();
      return value;
    }
  }
  
  auto& vopts = _expressionContext->trx().vpackOptions();

  VPackBuffer<uint8_t> buffer;
  VPackBuilder builder(buffer);
  builder.openArray();

  size_t const n = value.length();
  for (size_t i = 0; i < n; ++i) {
    bool localMustDestroy;
    AqlValue item = value.at(i, localMustDestroy, false);
    AqlValueGuard guard(item, localMustDestroy);

    AqlValueMaterializer materializer(&vopts);
    setVariable(variable, materializer.slice(item, false));

    bool takeItem = true;

    if (filterNode != nullptr) {
      // have a filter
      AqlValue sub = executeSimpleExpression(filterNode, localMustDestroy, false);

      takeItem = sub.toBoolean();
      if (localMustDestroy) {
        sub.destroy();
      }
    }

    if (takeItem && offset > 0) {
      // there is an offset in place
      --offset;
      takeItem = false;
    }

    if (takeItem) {
      AqlValue sub = executeSimpleExpression(projectionNode, localMustDestroy, false);
      sub.toVelocyPack(&vopts, builder, /*resolveExternals*/false,
                       /*allowUnindexed*/false);
      if (localMustDestroy) {
        sub.destroy();
      }
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
  return AqlValue(std::move(buffer));  // builder = dynamic data
}

/// @brief execute an expression of type SIMPLE with ITERATOR
AqlValue Expression::executeSimpleExpressionIterator(AstNode const* node,
                                                     bool& mustDestroy) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 2);

  return executeSimpleExpression(node->getMember(1), mustDestroy, true);
}

/// @brief execute an expression of type SIMPLE with BINARY_* (+, -, * , /, %)
AqlValue Expression::executeSimpleExpressionArithmetic(AstNode const* node,
                                                       bool& mustDestroy) {
  AqlValue lhs =
      executeSimpleExpression(node->getMemberUnchecked(0), mustDestroy, true);
  AqlValueGuard guardLhs(lhs, mustDestroy);

  AqlValue rhs =
      executeSimpleExpression(node->getMemberUnchecked(1), mustDestroy, true);
  AqlValueGuard guardRhs(rhs, mustDestroy);

  mustDestroy = false;

  bool failed = false;
  double l = lhs.toDouble(failed);

  if (failed) {
    TRI_ASSERT(!mustDestroy);
    l = 0.0;
  }

  double r = rhs.toDouble(failed);

  if (failed) {
    TRI_ASSERT(!mustDestroy);
    r = 0.0;
  }
  
  if (r == 0.0) {
    if (node->type == NODE_TYPE_OPERATOR_BINARY_DIV || node->type == NODE_TYPE_OPERATOR_BINARY_MOD) {
      // division by zero
      TRI_ASSERT(!mustDestroy);
      std::string msg("in operator ");
      msg.append(node->type == NODE_TYPE_OPERATOR_BINARY_DIV ? "/" : "%");
      msg.append(": ");
      msg.append(TRI_errno_string(TRI_ERROR_QUERY_DIVISION_BY_ZERO));
      _expressionContext->registerWarning(TRI_ERROR_QUERY_DIVISION_BY_ZERO, msg.c_str());
      return AqlValue(AqlValueHintNull());
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
      return AqlValue(AqlValueHintZero());
  }

  // this will convert NaN, +inf & -inf to null
  return AqlValue(AqlValueHintDouble(result));
}

void Expression::replaceNode(AstNode* node) {
  TRI_ASSERT(node != nullptr);
  if (node != _node) {
    _node = node;
    invalidateAfterReplacements();
  }
}

Ast* Expression::ast() const noexcept { 
  TRI_ASSERT(_ast != nullptr);
  return _ast; 
}

AstNode const* Expression::node() const {
  TRI_ASSERT(_node != nullptr);
  return _node; 
}

AstNode* Expression::nodeForModification() const { 
  TRI_ASSERT(_node != nullptr);
  return _node; 
}

bool Expression::canRunOnDBServer(bool isOneShard) {
  TRI_ASSERT(_type != UNPROCESSED);
  return (_type == JSON || _node->canRunOnDBServer(isOneShard));
}

bool Expression::isDeterministic() {
  TRI_ASSERT(_type != UNPROCESSED);
  return (_type == JSON || _node->isDeterministic());
}

bool Expression::willUseV8() {
  TRI_ASSERT(_type != UNPROCESSED);
  return (_type == SIMPLE && _node->willUseV8());
}

std::unique_ptr<Expression> Expression::clone(Ast* ast) {
  // We do not need to copy the _ast, since it is managed by the
  // query object and the memory management of the ASTs
  return std::make_unique<Expression>(ast != nullptr ? ast : _ast, _node);
}

void Expression::toVelocyPack(arangodb::velocypack::Builder& builder, bool verbose) const {
  _node->toVelocyPack(builder, verbose);
}

std::string Expression::typeString() {
  TRI_ASSERT(_type != UNPROCESSED);

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

void Expression::setVariable(Variable const* variable, arangodb::velocypack::Slice value) {
  _variables.emplace(variable, value);
}

void Expression::clearVariable(Variable const* variable) { _variables.erase(variable); }

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

#include "Expression.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/AqlValue.h"
#include "Aql/Ast.h"
#include "Aql/AttributeAccessor.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Function.h"
#include "Aql/Functions.h"
#include "Aql/Quantifier.h"
#include "Aql/Query.h"
#include "Aql/QueryContext.h"
#include "Aql/QueryExpressionContext.h"
#include "Aql/Range.h"
#ifdef USE_V8
#include "Aql/V8ErrorHandler.h"
#endif
#include "Aql/Variable.h"
#include "Aql/AqlValueMaterializer.h"
#include "Basics/Exceptions.h"
#include "Basics/MemoryTypes/MemoryTypes.h"
#include "Basics/NumberUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Containers/FlatHashSet.h"
#include "Cluster/ServerState.h"
#include "Transaction/Context.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#ifdef USE_V8
#include "V8/v8-globals.h"
#include "V8/v8-vpack.h"
#endif

#include <absl/strings/str_cat.h>

#include <velocypack/Buffer.h>
#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Sink.h>
#include <velocypack/Slice.h>

#include <limits>

using namespace arangodb;
using namespace arangodb::aql;
using VelocyPackHelper = arangodb::basics::VelocyPackHelper;

/// @brief create the expression
Expression::Expression(Ast* ast, AstNode* node)
    : _ast(ast),
      _node(node),
      _data(nullptr),
      _type(ExpressionType::kUnprocessed),
      _resourceMonitor(ast->query().resourceMonitor()) {
  TRI_ASSERT(_ast != nullptr);
  TRI_ASSERT(_node != nullptr);

  TRI_ASSERT(_data == nullptr);
  TRI_ASSERT(_accessor == nullptr);

  determineType();
  TRI_ASSERT(_type != ExpressionType::kUnprocessed);
}

/// @brief create an expression from VPack
Expression::Expression(Ast* ast, arangodb::velocypack::Slice slice)
    : Expression(ast, ast->createNode(slice.get("expression"))) {
  TRI_ASSERT(_type != ExpressionType::kUnprocessed);
}

/// @brief destroy the expression
Expression::~Expression() { freeInternals(); }

/// @brief return all variables used in the expression
void Expression::variables(VarSet& result) const {
  Ast::getReferencedVariables(_node, result);
}

/// @brief execute the expression
AqlValue Expression::execute(ExpressionContext* ctx, bool& mustDestroy) {
  TRI_ASSERT(ctx != nullptr);
  prepareForExecution();

  TRI_ASSERT(_type != ExpressionType::kUnprocessed);

  // and execute
  switch (_type) {
    case ExpressionType::kJson: {
      mustDestroy = false;
      TRI_ASSERT(_data != nullptr);
      return AqlValue(_data);
    }

    case ExpressionType::kSimple: {
      return executeSimpleExpression(*ctx, _node, mustDestroy, true);
    }

    case ExpressionType::kAttributeAccess: {
      TRI_ASSERT(_accessor != nullptr);
      auto resolver = ctx->trx().resolver();
      TRI_ASSERT(resolver != nullptr);
      return _accessor->get(*resolver, ctx, mustDestroy);
    }

    case ExpressionType::kUnprocessed: {
      // fall-through to exception
    }
  }

  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid expression type");
}

/// @brief replace variables in the expression with other variables
void Expression::replaceVariables(
    std::unordered_map<VariableId, Variable const*> const& replacements) {
  _node = _ast->clone(_node);
  TRI_ASSERT(_node != nullptr);

  _node = Ast::replaceVariables(const_cast<AstNode*>(_node), replacements,
                                /*unlockNodes*/ false);

  if (_type == ExpressionType::kAttributeAccess && _accessor != nullptr) {
    _accessor->replaceVariable(replacements);
  } else {
    freeInternals();
  }
}

/// @brief replace a variable reference in the expression with another
/// expression (e.g. inserting c = `a + b` into expression `c + 1` so the latter
/// becomes `a + b + 1`
void Expression::replaceVariableReference(Variable const* variable,
                                          AstNode const* node) {
  _node = _ast->clone(_node);
  TRI_ASSERT(_node != nullptr);

  _node = Ast::replaceVariableReference(const_cast<AstNode*>(_node), variable,
                                        node);
  invalidateAfterReplacements();
}

void Expression::replaceAttributeAccess(Variable const* searchVariable,
                                        std::span<std::string_view> attribute,
                                        Variable const* replaceVariable) {
  _node = _ast->clone(_node);
  TRI_ASSERT(_node != nullptr);

  _node =
      Ast::replaceAttributeAccess(ast(), const_cast<AstNode*>(_node),
                                  searchVariable, attribute, replaceVariable);
  invalidateAfterReplacements();
}

/// @brief free the internal data structures
void Expression::freeInternals() noexcept {
  switch (_type) {
    case ExpressionType::kJson:
      _resourceMonitor.decreaseMemoryUsage(_usedBytesByData);
      velocypack_free(_data);
      _data = nullptr;
      _usedBytesByData = 0;
      break;

    case ExpressionType::kAttributeAccess: {
      delete _accessor;
      _accessor = nullptr;
      break;
    }

    case ExpressionType::kSimple:
    case ExpressionType::kUnprocessed: {
      // nothing to do
      break;
    }
  }
}

/// @brief reset internal attributes after variables in the expression were
/// changed
void Expression::invalidateAfterReplacements() {
  if (_type == ExpressionType::kAttributeAccess ||
      _type == ExpressionType::kSimple || _type == ExpressionType::kJson) {
    freeInternals();
    _node->clearFlagsRecursive();  // recursively delete the node's flags
  }

  const_cast<AstNode*>(_node)->clearFlags();

  // must even set back the expression type so the expression will be analyzed
  // again
  _type = ExpressionType::kUnprocessed;
  determineType();
}

/// @brief find a value in an AQL array node
/// this performs either a binary search (if the node is sorted) or a
/// linear search (if the node is not sorted)
bool Expression::findInArray(AqlValue const& left, AqlValue const& right,
                             VPackOptions const* vopts, AstNode const* node) {
  TRI_ASSERT(right.isArray());

  size_t const n = right.length();

  if (n >= AstNode::kSortNumberThreshold &&
      (node->getMember(1)->isSorted() ||
       ((node->type == NODE_TYPE_OPERATOR_BINARY_IN ||
         node->type == NODE_TYPE_OPERATOR_BINARY_NIN) &&
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
      int compareResult = arangodb::basics::VelocyPackHelper::compare(
          lhs, it.value(), false, vopts);

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
  TRI_ASSERT(_type == ExpressionType::kUnprocessed);

  TRI_ASSERT(_data == nullptr);
  TRI_ASSERT(_accessor == nullptr);

  if (_node->isConstant()) {
    // expression is a constant value
    _data = nullptr;
    _usedBytesByData = 0;
    _type = ExpressionType::kJson;
    return;
  }

  // expression is a simple expression
  _type = ExpressionType::kSimple;

  if (_node->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
    // optimization for attribute accesses
    TRI_ASSERT(_node->numMembers() == 1);
    auto member = _node->getMemberUnchecked(0);

    while (member->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
      member = member->getMemberUnchecked(0);
    }

    if (member->type == NODE_TYPE_REFERENCE) {
      _type = ExpressionType::kAttributeAccess;
    }
  }
}

void Expression::initAccessor() {
  TRI_ASSERT(_type == ExpressionType::kAttributeAccess);
  TRI_ASSERT(_accessor == nullptr);

  TRI_ASSERT(_node->numMembers() == 1);
  auto member = _node->getMemberUnchecked(0);
  std::vector<std::string> parts;
  parts.emplace_back(_node->getString());

  while (member->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
    parts.insert(parts.begin(), member->getString());
    member = member->getMemberUnchecked(0);
  }

  if (member->type != NODE_TYPE_REFERENCE) {
    // the accessor accesses something else than a variable/reference.
    // this is something we are not prepared for. so fall back to a
    // simple expression instead
    _type = ExpressionType::kSimple;
  } else {
    TRI_ASSERT(member->type == NODE_TYPE_REFERENCE);
    auto v = static_cast<Variable const*>(member->getData());

    // specialize the simple expression into an attribute accessor
    _accessor = AttributeAccessor::create(
        AttributeNamePath(std::move(parts), _resourceMonitor), v);
    TRI_ASSERT(_accessor != nullptr);
  }
}

/// @brief prepare the expression for execution, without an
/// ExpressionContext.
void Expression::prepareForExecution() {
  TRI_ASSERT(_type != ExpressionType::kUnprocessed);

  if (_type == ExpressionType::kJson && _data == nullptr) {
    // generate a constant value, using an on-stack Builder
    velocypack::Buffer<uint8_t> buffer;
    velocypack::Builder builder(buffer);
    _node->toVelocyPackValue(builder);

    auto bufferSize = buffer.size();
    {
      arangodb::ResourceUsageScope guard(_resourceMonitor, bufferSize);

      if (buffer.usesLocalMemory()) {
        // Buffer has data in its local memory. because we
        // don't want to keep the whole Buffer object, we allocate
        // the required space ourselves and copy things over.
        _data = static_cast<uint8_t*>(
            velocypack_malloc(static_cast<size_t>(bufferSize)));
        if (_data == nullptr) {
          // malloc returned a nullptr
          _usedBytesByData = 0;
          throw std::bad_alloc();
        }
        memcpy(_data, buffer.data(), static_cast<size_t>(bufferSize));
      } else {
        // we can simply steal the data from the Buffer. we
        // own the data now.
        _data = buffer.steal();
      }

      _usedBytesByData = bufferSize;
      guard.steal();
    }

  } else if (_type == ExpressionType::kAttributeAccess &&
             _accessor == nullptr) {
    initAccessor();
  }
}

// brief execute an expression of type ExpressionType::kSimple, the convention
// is that the resulting AqlValue will be destroyed outside eventually
AqlValue Expression::executeSimpleExpression(ExpressionContext& ctx,
                                             AstNode const* node,
                                             bool& mustDestroy, bool doCopy) {
  switch (node->type) {
    case NODE_TYPE_ATTRIBUTE_ACCESS:
      return executeSimpleExpressionAttributeAccess(ctx, node, mustDestroy,
                                                    doCopy);
    case NODE_TYPE_INDEXED_ACCESS:
      return executeSimpleExpressionIndexedAccess(ctx, node, mustDestroy,
                                                  doCopy);
    case NODE_TYPE_ARRAY:
      return executeSimpleExpressionArray(ctx, node, mustDestroy);
    case NODE_TYPE_OBJECT:
      return executeSimpleExpressionObject(ctx, node, mustDestroy);
    case NODE_TYPE_VALUE:
      return executeSimpleExpressionValue(ctx, node, mustDestroy);
    case NODE_TYPE_REFERENCE:
      return executeSimpleExpressionReference(ctx, node, mustDestroy, doCopy);
    case NODE_TYPE_FCALL:
      return executeSimpleExpressionFCall(ctx, node, mustDestroy);
    case NODE_TYPE_FCALL_USER:
      return executeSimpleExpressionFCallJS(ctx, node, mustDestroy);
    case NODE_TYPE_RANGE:
      return executeSimpleExpressionRange(ctx, node, mustDestroy);
    case NODE_TYPE_OPERATOR_UNARY_NOT:
      return executeSimpleExpressionNot(ctx, node, mustDestroy);
    case NODE_TYPE_OPERATOR_UNARY_PLUS:
      return executeSimpleExpressionPlus(ctx, node, mustDestroy);
    case NODE_TYPE_OPERATOR_UNARY_MINUS:
      return executeSimpleExpressionMinus(ctx, node, mustDestroy);
    case NODE_TYPE_OPERATOR_BINARY_AND:
      return executeSimpleExpressionAnd(ctx, node, mustDestroy);
    case NODE_TYPE_OPERATOR_BINARY_OR:
      return executeSimpleExpressionOr(ctx, node, mustDestroy);
    case NODE_TYPE_OPERATOR_BINARY_EQ:
    case NODE_TYPE_OPERATOR_BINARY_NE:
    case NODE_TYPE_OPERATOR_BINARY_LT:
    case NODE_TYPE_OPERATOR_BINARY_LE:
    case NODE_TYPE_OPERATOR_BINARY_GT:
    case NODE_TYPE_OPERATOR_BINARY_GE:
    case NODE_TYPE_OPERATOR_BINARY_IN:
    case NODE_TYPE_OPERATOR_BINARY_NIN:
      return executeSimpleExpressionComparison(ctx, node, mustDestroy);
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_NE:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_LT:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_LE:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_GT:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_GE:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_IN:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN:
      return executeSimpleExpressionArrayComparison(ctx, node, mustDestroy);
    case NODE_TYPE_OPERATOR_TERNARY:
      return executeSimpleExpressionTernary(ctx, node, mustDestroy);
    case NODE_TYPE_EXPANSION:
      return executeSimpleExpressionExpansion(ctx, node, mustDestroy);
    case NODE_TYPE_ITERATOR:
      return executeSimpleExpressionIterator(ctx, node, mustDestroy);
    case NODE_TYPE_OPERATOR_BINARY_PLUS:
    case NODE_TYPE_OPERATOR_BINARY_MINUS:
    case NODE_TYPE_OPERATOR_BINARY_TIMES:
    case NODE_TYPE_OPERATOR_BINARY_DIV:
    case NODE_TYPE_OPERATOR_BINARY_MOD:
      return executeSimpleExpressionArithmetic(ctx, node, mustDestroy);
    case NODE_TYPE_OPERATOR_NARY_AND:
    case NODE_TYPE_OPERATOR_NARY_OR:
      return executeSimpleExpressionNaryAndOr(ctx, node, mustDestroy);
    case NODE_TYPE_COLLECTION:
    case NODE_TYPE_VIEW:
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_NOT_IMPLEMENTED,
          absl::StrCat("node type '", node->getTypeString(),
                       "' is not supported in expressions"));

    default:
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL,
          absl::StrCat(
              "unhandled type '", node->getTypeString(),
              "' in executeSimpleExpression(): ", AstNode::toString(node)));
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
void Expression::stringify(std::string& buffer) const {
  _node->stringify(buffer, false);
}

/// @brief stringify an expression
/// note that currently stringification is only supported for certain node types
void Expression::stringifyIfNotTooLong(std::string& buffer) const {
  _node->stringify(buffer, true);
}

// execute an expression of type ExpressionType::kSimple with ATTRIBUTE ACCESS
// always creates a copy
AqlValue Expression::executeSimpleExpressionAttributeAccess(
    ExpressionContext& ctx, AstNode const* node, bool& mustDestroy,
    bool doCopy) {
  // object lookup, e.g. users.name
  TRI_ASSERT(node->numMembers() == 1);

  auto member = node->getMemberUnchecked(0);

  bool localMustDestroy;
  AqlValue result =
      executeSimpleExpression(ctx, member, localMustDestroy, false);
  AqlValueGuard guard(result, localMustDestroy);
  auto* resolver = ctx.trx().resolver();
  TRI_ASSERT(resolver != nullptr);

  return result.get(*resolver,
                    std::string_view(static_cast<char const*>(node->getData()),
                                     node->getStringLength()),
                    mustDestroy, true);
}

// execute an expression of type ExpressionType::kSimple with INDEXED ACCESS
AqlValue Expression::executeSimpleExpressionIndexedAccess(
    ExpressionContext& ctx, AstNode const* node, bool& mustDestroy,
    bool doCopy) {
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
  AqlValue result = executeSimpleExpression(ctx, member, mustDestroy, false);

  AqlValueGuard guard(result, mustDestroy);

  if (result.isArray()) {
    AqlValue indexResult =
        executeSimpleExpression(ctx, index, mustDestroy, false);

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
    AqlValue indexResult =
        executeSimpleExpression(ctx, index, mustDestroy, false);

    AqlValueGuard guardIdx(indexResult, mustDestroy);

    if (indexResult.isNumber()) {
      std::string const indexString = std::to_string(indexResult.toInt64());
      auto* resolver = ctx.trx().resolver();
      TRI_ASSERT(resolver != nullptr);
      return result.get(*resolver, indexString, mustDestroy, true);
    }

    if (indexResult.isString()) {
      VPackValueLength l;
      char const* p = indexResult.slice().getStringUnchecked(l);
      auto* resolver = ctx.trx().resolver();
      TRI_ASSERT(resolver != nullptr);
      return result.get(*resolver, std::string_view(p, l), mustDestroy, true);
    }

    // fall-through to returning null
  }

  return AqlValue(AqlValueHintNull());
}

// execute an expression of type ExpressionType::kSimple with ARRAY
AqlValue Expression::executeSimpleExpressionArray(ExpressionContext& ctx,
                                                  AstNode const* node,
                                                  bool& mustDestroy) {
  mustDestroy = false;
  if (node->isConstant()) {
    // this will not create a copy
    uint8_t const* cv = node->computedValue();
    if (cv != nullptr) {
      return AqlValue(cv);
    }
    transaction::BuilderLeaser builder(&ctx.trx());
    return AqlValue(node->computeValue(builder.get()).begin());
  }

  size_t const n = node->numMembers();

  if (n == 0) {
    return AqlValue(AqlValueHintEmptyArray());
  }

  auto& trx = ctx.trx();

  transaction::BuilderLeaser builder(&trx);
  builder->openArray();

  for (size_t i = 0; i < n; ++i) {
    auto member = node->getMemberUnchecked(i);
    bool localMustDestroy = false;
    AqlValue result =
        executeSimpleExpression(ctx, member, localMustDestroy, false);
    AqlValueGuard guard(result, localMustDestroy);
    result.toVelocyPack(&trx.vpackOptions(), *builder.get(),
                        /*allowUnindexed*/ false);
  }

  builder->close();
  mustDestroy = true;  // AqlValue contains builder contains dynamic data
  return AqlValue(builder->slice(), builder->size());
}

// execute an expression of type ExpressionType::kSimple with OBJECT
AqlValue Expression::executeSimpleExpressionObject(ExpressionContext& ctx,
                                                   AstNode const* node,
                                                   bool& mustDestroy) {
  auto& trx = ctx.trx();
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
  containers::FlatHashSet<std::string> keys;
  bool const mustCheckUniqueness = node->mustCheckUniqueness();

  transaction::BuilderLeaser builder(&trx);
  builder->openObject();

  transaction::StringLeaser buffer(&trx);
  arangodb::velocypack::StringSink adapter(buffer.get());

  for (size_t i = 0; i < n; ++i) {
    auto member = node->getMemberUnchecked(i);

    // process attribute key, taking into account duplicates
    if (member->type == NODE_TYPE_CALCULATED_OBJECT_ELEMENT) {
      bool localMustDestroy;
      AqlValue result = executeSimpleExpression(ctx, member->getMember(0),
                                                localMustDestroy, false);
      AqlValueGuard guard(result, localMustDestroy);

      // make sure key is a string, and convert it if not
      AqlValueMaterializer materializer(&vopts);
      VPackSlice slice = materializer.slice(result);

      buffer->clear();
      functions::stringify(&vopts, adapter, slice);

      if (mustCheckUniqueness) {
        // prevent duplicate keys from being used
        auto it = keys.find(*buffer);

        if (it != keys.end()) {
          // duplicate key
          continue;
        }

        // unique key
        builder->add(VPackValue(*buffer));
        if (i != n - 1) {
          // track usage of key
          keys.emplace(*buffer);
        }
      } else {
        builder->add(VPackValue(*buffer));
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
        builder->add(VPackValue(member->getStringView()));
      }

      // value
      member = member->getMember(0);
    }

    // add the attribute value
    bool localMustDestroy;
    AqlValue result =
        executeSimpleExpression(ctx, member, localMustDestroy, false);
    AqlValueGuard guard(result, localMustDestroy);
    result.toVelocyPack(&vopts, *builder.get(), /*allowUnindexed*/ false);
  }

  builder->close();

  mustDestroy = true;  // AqlValue contains builder contains dynamic data

  return AqlValue(builder->slice(), builder->size());
}

// execute an expression of type ExpressionType::kSimple with VALUE
AqlValue Expression::executeSimpleExpressionValue(ExpressionContext& ctx,
                                                  AstNode const* node,
                                                  bool& mustDestroy) {
  // this will not create a copy
  mustDestroy = false;
  uint8_t const* cv = node->computedValue();
  if (cv != nullptr) {
    return AqlValue(cv);
  }
  transaction::BuilderLeaser builder(&ctx.trx());
  return AqlValue(node->computeValue(builder.get()).begin());
}

// execute an expression of type ExpressionType::kSimple with REFERENCE
AqlValue Expression::executeSimpleExpressionReference(ExpressionContext& ctx,
                                                      AstNode const* node,
                                                      bool& mustDestroy,
                                                      bool doCopy) {
  mustDestroy = false;
  auto v = static_cast<Variable const*>(node->getData());
  TRI_ASSERT(v != nullptr);
  return ctx.getVariableValue(v, doCopy, mustDestroy);
}

// execute an expression of type ExpressionType::kSimple with RANGE
AqlValue Expression::executeSimpleExpressionRange(ExpressionContext& ctx,
                                                  AstNode const* node,
                                                  bool& mustDestroy) {
  auto low = node->getMember(0);
  auto high = node->getMember(1);
  mustDestroy = false;

  AqlValue resultLow = executeSimpleExpression(ctx, low, mustDestroy, false);

  AqlValueGuard guardLow(resultLow, mustDestroy);

  AqlValue resultHigh = executeSimpleExpression(ctx, high, mustDestroy, false);

  AqlValueGuard guardHigh(resultHigh, mustDestroy);

  mustDestroy = true;  // as we're creating a new range object
  return AqlValue(resultLow.toInt64(), resultHigh.toInt64());
}

// execute an expression of type ExpressionType::kSimple with FCALL, dispatcher
AqlValue Expression::executeSimpleExpressionFCall(ExpressionContext& ctx,
                                                  AstNode const* node,
                                                  bool& mustDestroy) {
  // only some functions have C++ handlers
  // check that the called function actually has one
  auto func = static_cast<Function*>(node->getData());
  TRI_ASSERT(func != nullptr);
  // likely because call js function anyway will be slow
  if (ADB_LIKELY(func->hasCxxImplementation())) {
    return executeSimpleExpressionFCallCxx(ctx, node, mustDestroy);
  }
  return executeSimpleExpressionFCallJS(ctx, node, mustDestroy);
}

// execute an expression of type ExpressionType::kSimple with FCALL, CXX version
AqlValue Expression::executeSimpleExpressionFCallCxx(ExpressionContext& ctx,
                                                     AstNode const* node,
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
    containers::SmallVector<AqlValue, 8> parameters;

    // same here
    containers::SmallVector<uint64_t, 8> destroyParameters;

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
      params.parameters.emplace_back(arg->getStringView());
      params.destroyParameters.push_back(1);
    } else {
      bool localMustDestroy;
      params.parameters.emplace_back(
          executeSimpleExpression(ctx, arg, localMustDestroy, false));
      params.destroyParameters.push_back(localMustDestroy ? 1 : 0);
    }
  }

  TRI_ASSERT(params.parameters.size() == params.destroyParameters.size());
  TRI_ASSERT(params.parameters.size() == n);

  AqlValue a = func->implementation(&ctx, *node, params.parameters);
  mustDestroy = true;  // function result is always dynamic

  return a;
}

#ifdef USE_V8
AqlValue Expression::invokeV8Function(
    ExpressionContext& ctx, std::string const& jsName, v8::Isolate* isolate,
    std::string const& ucInvokeFN, char const* AFN, bool rethrowV8Exception,
    size_t callArgs, v8::Handle<v8::Value>* args, bool& mustDestroy) {
  TRI_ASSERT(isolate->InContext());
  v8::Handle<v8::Context> context = isolate->GetCurrentContext();
  TRI_ASSERT(!context.IsEmpty());
  auto global = context->Global();

  v8::Handle<v8::Value> module =
      global->Get(context, TRI_V8_ASCII_STRING(isolate, "_AQL"))
          .FromMaybe(v8::Local<v8::Value>());
  if (module.IsEmpty() || !module->IsObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "unable to find global _AQL module");
  }

  v8::Handle<v8::Value> function =
      v8::Handle<v8::Object>::Cast(module)
          ->Get(context, TRI_V8_STD_STRING(isolate, jsName))
          .FromMaybe(v8::Local<v8::Value>());
  if (function.IsEmpty() || !function->IsFunction()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        absl::StrCat("unable to find AQL function '", jsName, "'"));
  }

  // actually call the V8 function
  v8::TryCatch tryCatch(isolate);
  v8::Handle<v8::Value> result =
      v8::Handle<v8::Function>::Cast(function)
          ->Call(context, global, static_cast<int>(callArgs), args)
          .FromMaybe(v8::Local<v8::Value>());

  try {
    handleV8Error(tryCatch, result);
  } catch (basics::Exception const& ex) {
    if (rethrowV8Exception || ex.code() == TRI_ERROR_QUERY_FUNCTION_NOT_FOUND) {
      throw;
    }
    std::string message = absl::StrCat("while invoking '", ucInvokeFN,
                                       "' via '", AFN, "': ", ex.message());
    ctx.registerWarning(ex.code(), message.c_str());
    return AqlValue(AqlValueHintNull());
  }
  if (result.IsEmpty() || result->IsUndefined()) {
    return AqlValue(AqlValueHintNull());
  }

  auto& trx = ctx.trx();
  transaction::BuilderLeaser builder(&trx);

  // can throw
  TRI_V8ToVPack(isolate, *builder.get(), result, false);

  mustDestroy = true;  // builder = dynamic data
  return AqlValue(builder->slice(), builder->size());
}
#endif

// execute an expression of type ExpressionType::kSimple, JavaScript variant
AqlValue Expression::executeSimpleExpressionFCallJS(ExpressionContext& ctx,
                                                    AstNode const* node,
                                                    bool& mustDestroy) {
  if (ServerState::instance()->isDBServer()) {
    // we actually should not get here, but in case we do due to some changes,
    // it is better to abort the query with a proper error rather than crashing
    // with assertion failure or segfault.
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_NOT_IMPLEMENTED,
        "user-defined functions cannot be executed on DB-Servers");
  }

#ifdef USE_V8
  auto member = node->getMemberUnchecked(0);
  TRI_ASSERT(member->type == NODE_TYPE_ARRAY);

  mustDestroy = false;

  {
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    TRI_ASSERT(isolate != nullptr);

    std::string jsName;
    if (node->type == NODE_TYPE_FCALL_USER) {
      jsName = "FCALL_USER";
    } else {
      auto func = static_cast<Function*>(node->getData());
      TRI_ASSERT(func != nullptr);
      TRI_ASSERT(func->hasV8Implementation());
      jsName = absl::StrCat("AQL_", func->name);
    }

    TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(
        isolate->GetData(arangodb::V8PlatformFeature::V8_DATA_SLOT));

    TRI_ASSERT(v8g != nullptr);
    if (v8g == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL,
          absl::StrCat("no V8 available when executing call to function ",
                       jsName));
    }

    auto queryCtx = dynamic_cast<QueryExpressionContext*>(&ctx);
    if (queryCtx == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL, "unable to cast into QueryExpressionContext");
    }

    auto query = dynamic_cast<Query*>(&queryCtx->query());
    if (query == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "unable to cast into Query");
    }

    VPackOptions const& options = ctx.trx().vpackOptions();

    auto old = v8g->_expressionContext;
    v8g->_expressionContext = &ctx;
    auto sg =
        arangodb::scopeGuard([&]() noexcept { v8g->_expressionContext = old; });

    AqlValue funcRes;
    query->runInV8ExecutorContext([&](v8::Isolate* isolate) {
      v8::HandleScope scope(isolate);

      size_t const n = member->numMembers();
      size_t callArgs = (node->type == NODE_TYPE_FCALL_USER ? 2 : n);
      auto args = std::make_unique<v8::Handle<v8::Value>[]>(callArgs);

      if (node->type == NODE_TYPE_FCALL_USER) {
        // a call to a user-defined function
        v8::Handle<v8::Context> context = isolate->GetCurrentContext();

        v8::Handle<v8::Array> params =
            v8::Array::New(isolate, static_cast<int>(n));

        for (size_t i = 0; i < n; ++i) {
          auto arg = member->getMemberUnchecked(i);

          bool localMustDestroy;
          AqlValue a =
              executeSimpleExpression(ctx, arg, localMustDestroy, false);
          AqlValueGuard guard(a, localMustDestroy);

          params
              ->Set(context, static_cast<uint32_t>(i),
                    a.toV8(isolate, &options))
              .FromMaybe(false);
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

        for (size_t i = 0; i < n; ++i) {
          auto arg = member->getMemberUnchecked(i);

          if (arg->type == NODE_TYPE_COLLECTION) {
            // parameter conversion for NODE_TYPE_COLLECTION here
            args[i] = TRI_V8_ASCII_PAIR_STRING(isolate, arg->getStringValue(),
                                               arg->getStringLength());
          } else {
            bool localMustDestroy;
            AqlValue a =
                executeSimpleExpression(ctx, arg, localMustDestroy, false);
            AqlValueGuard guard(a, localMustDestroy);

            args[i] = a.toV8(isolate, &options);
          }
        }
      }

      funcRes = invokeV8Function(ctx, jsName, isolate, "", "", true, callArgs,
                                 args.get(), mustDestroy);
    });
    return funcRes;
  }
#else
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_NOT_IMPLEMENTED,
      "this version of ArangoDB is built without JavaScript support");
#endif
}

// execute an expression of type ExpressionType::kSimple with NOT
AqlValue Expression::executeSimpleExpressionNot(ExpressionContext& ctx,
                                                AstNode const* node,
                                                bool& mustDestroy) {
  mustDestroy = false;
  AqlValue operand =
      executeSimpleExpression(ctx, node->getMember(0), mustDestroy, false);

  AqlValueGuard guard(operand, mustDestroy);
  bool const operandIsTrue = operand.toBoolean();

  mustDestroy = false;  // only a boolean
  return AqlValue(AqlValueHintBool(!operandIsTrue));
}

// execute an expression of type ExpressionType::kSimple with +
AqlValue Expression::executeSimpleExpressionPlus(ExpressionContext& ctx,
                                                 AstNode const* node,
                                                 bool& mustDestroy) {
  mustDestroy = false;
  AqlValue operand =
      executeSimpleExpression(ctx, node->getMember(0), mustDestroy, false);

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

// execute an expression of type ExpressionType::kSimple with -
AqlValue Expression::executeSimpleExpressionMinus(ExpressionContext& ctx,
                                                  AstNode const* node,
                                                  bool& mustDestroy) {
  mustDestroy = false;
  AqlValue operand =
      executeSimpleExpression(ctx, node->getMember(0), mustDestroy, false);

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

// execute an expression of type ExpressionType::kSimple with AND
AqlValue Expression::executeSimpleExpressionAnd(ExpressionContext& ctx,
                                                AstNode const* node,
                                                bool& mustDestroy) {
  AqlValue left = executeSimpleExpression(ctx, node->getMemberUnchecked(0),
                                          mustDestroy, true);

  if (left.toBoolean()) {
    // left is true => return right
    if (mustDestroy) {
      left.destroy();
    }
    return executeSimpleExpression(ctx, node->getMemberUnchecked(1),
                                   mustDestroy, true);
  }

  // left is false, return left
  return left;
}

// execute an expression of type ExpressionType::kSimple with OR
AqlValue Expression::executeSimpleExpressionOr(ExpressionContext& ctx,
                                               AstNode const* node,
                                               bool& mustDestroy) {
  AqlValue left = executeSimpleExpression(ctx, node->getMemberUnchecked(0),
                                          mustDestroy, true);

  if (left.toBoolean()) {
    // left is true => return left
    return left;
  }

  // left is false => return right
  if (mustDestroy) {
    left.destroy();
  }
  return executeSimpleExpression(ctx, node->getMemberUnchecked(1), mustDestroy,
                                 true);
}

// execute an expression of type ExpressionType::kSimple with AND or OR
AqlValue Expression::executeSimpleExpressionNaryAndOr(ExpressionContext& ctx,
                                                      AstNode const* node,
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
      AqlValue check = executeSimpleExpression(ctx, node->getMemberUnchecked(i),
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
    AqlValue check = executeSimpleExpression(ctx, node->getMemberUnchecked(i),
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

// execute an expression of type ExpressionType::kSimple with COMPARISON
AqlValue Expression::executeSimpleExpressionComparison(ExpressionContext& ctx,
                                                       AstNode const* node,
                                                       bool& mustDestroy) {
  AqlValue left = executeSimpleExpression(ctx, node->getMemberUnchecked(0),
                                          mustDestroy, false);
  AqlValueGuard guardLeft(left, mustDestroy);

  AqlValue right = executeSimpleExpression(ctx, node->getMemberUnchecked(1),
                                           mustDestroy, false);
  AqlValueGuard guardRight(right, mustDestroy);

  mustDestroy = false;  // we're returning a boolean only

  auto const& vopts = ctx.trx().vpackOptions();
  if (node->type == NODE_TYPE_OPERATOR_BINARY_IN ||
      node->type == NODE_TYPE_OPERATOR_BINARY_NIN) {
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

// execute an expression of type ExpressionType::kSimple with ARRAY COMPARISON
AqlValue Expression::executeSimpleExpressionArrayComparison(
    ExpressionContext& ctx, AstNode const* node, bool& mustDestroy) {
  auto const& vopts = ctx.trx().vpackOptions();

  AqlValue left =
      executeSimpleExpression(ctx, node->getMember(0), mustDestroy, false);
  AqlValueGuard guardLeft(left, mustDestroy);

  AqlValue right =
      executeSimpleExpression(ctx, node->getMember(1), mustDestroy, false);
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

  AstNode const* q = node->getMember(2);
  if (n == 0) {
    if (Quantifier::isAll(q) || Quantifier::isNone(q)) {
      // [] ALL ...
      // [] NONE ...
      return AqlValue(AqlValueHintBool(true));
    } else if (Quantifier::isAny(q)) {
      // [] ANY ...
      return AqlValue(AqlValueHintBool(false));
    }
  }

  int64_t atLeast = 0;
  if (Quantifier::isAtLeast(q)) {
    // evaluate expression for AT LEAST
    TRI_ASSERT(q->numMembers() == 1);

    bool mustDestroy = false;
    AqlValue atLeastValue =
        executeSimpleExpression(ctx, q->getMember(0), mustDestroy, false);
    AqlValueGuard guard(atLeastValue, mustDestroy);
    atLeast = atLeastValue.toInt64();
    if (atLeast < 0) {
      atLeast = 0;
    }
    TRI_ASSERT(atLeast >= 0);
  }

  std::pair<size_t, size_t> requiredMatches =
      Quantifier::requiredMatches(n, q, static_cast<size_t>(atLeast));

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
      int compareResult =
          AqlValue::Compare(&vopts, leftItemValue, right, compareUtf8);

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

// execute an expression of type ExpressionType::kSimple with TERNARY
AqlValue Expression::executeSimpleExpressionTernary(ExpressionContext& ctx,
                                                    AstNode const* node,
                                                    bool& mustDestroy) {
  if (node->numMembers() == 2) {
    AqlValue condition =
        executeSimpleExpression(ctx, node->getMember(0), mustDestroy, true);
    AqlValueGuard guard(condition, mustDestroy);

    if (condition.toBoolean()) {
      guard.steal();
      return condition;
    }
    return executeSimpleExpression(ctx, node->getMemberUnchecked(1),
                                   mustDestroy, true);
  }

  TRI_ASSERT(node->numMembers() == 3);

  AqlValue condition =
      executeSimpleExpression(ctx, node->getMember(0), mustDestroy, false);

  AqlValueGuard guardCondition(condition, mustDestroy);

  size_t position;
  if (condition.toBoolean()) {
    // return true part
    position = 1;
  } else {
    // return false part
    position = 2;
  }

  return executeSimpleExpression(ctx, node->getMemberUnchecked(position),
                                 mustDestroy, true);
}

// execute an expression of type ExpressionType::kSimple with EXPANSION
AqlValue Expression::executeSimpleExpressionExpansion(ExpressionContext& ctx,
                                                      AstNode const* node,
                                                      bool& mustDestroy) {
  TRI_ASSERT(node->numMembers() == 5);
  mustDestroy = false;

  // LIMIT
  int64_t offset = 0;
  int64_t count = INT64_MAX;

  bool const isBoolean = node->hasFlag(FLAG_BOOLEAN_EXPANSION);

  auto limitNode = node->getMember(3);

  if (limitNode->type != NODE_TYPE_NOP) {
    TRI_ASSERT(!isBoolean);

    bool localMustDestroy;
    AqlValue subOffset = executeSimpleExpression(ctx, limitNode->getMember(0),
                                                 localMustDestroy, false);
    offset = subOffset.toInt64();
    if (localMustDestroy) {
      subOffset.destroy();
    }

    AqlValue subCount = executeSimpleExpression(ctx, limitNode->getMember(1),
                                                localMustDestroy, false);
    count = subCount.toInt64();
    if (localMustDestroy) {
      subCount.destroy();
    }

    if (offset < 0 || count <= 0) {
      // no items to return... can already stop here
      return AqlValue(AqlValueHintEmptyArray());
    }
  }

  // quantifier and FILTER
  AstNode const* quantifierAndFilterNode = node->getMember(2);

  AstNode const* quantifierNode = nullptr;
  AstNode const* filterNode = nullptr;

  if (quantifierAndFilterNode == nullptr ||
      quantifierAndFilterNode->type == NODE_TYPE_NOP) {
    filterNode = nullptr;
  } else {
    if (quantifierAndFilterNode->type == NODE_TYPE_ARRAY_FILTER) {
      // 3.10 format: we get an ARRAY_FILTER node, which contains
      // both a quantifier and the filter condition
      TRI_ASSERT(quantifierAndFilterNode->type == NODE_TYPE_ARRAY_FILTER);
      TRI_ASSERT(quantifierAndFilterNode->numMembers() == 2);

      quantifierNode = quantifierAndFilterNode->getMember(0);
      TRI_ASSERT(quantifierNode != nullptr);

      filterNode = quantifierAndFilterNode->getMember(1);

      if (!isBoolean && filterNode->isConstant()) {
        if (filterNode->isTrue()) {
          // filter expression is always true
          filterNode = nullptr;
        } else {
          // filter expression is always false
          return AqlValue(AqlValueHintEmptyArray());
        }
      }
    } else {
      // pre-3.10 format: we get the filter condition as the only value
      TRI_ASSERT(quantifierNode == nullptr);
      filterNode = quantifierAndFilterNode;
    }
  }

  auto iterator = node->getMember(0);
  auto variable = static_cast<Variable*>(iterator->getMember(0)->getData());
  auto levels = node->getIntValue(true);

  AqlValue value;

  if (levels > 1) {
    // flatten value...
    bool localMustDestroy;
    AqlValue a = executeSimpleExpression(ctx, node->getMember(0),
                                         localMustDestroy, false);

    AqlValueGuard guard(a, localMustDestroy);

    if (!a.isArray()) {
      TRI_ASSERT(!mustDestroy);
      if (isBoolean) {
        return AqlValue(AqlValueHintBool(false));
      }
      return AqlValue(AqlValueHintEmptyArray());
    }

    VPackBuffer<uint8_t> buffer;
    VPackBuilder builder(buffer);
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
    AqlValue a = executeSimpleExpression(ctx, node->getMember(0),
                                         localMustDestroy, false);

    AqlValueGuard guard(a, localMustDestroy);

    if (!a.isArray()) {
      TRI_ASSERT(!mustDestroy);
      if (isBoolean) {
        return AqlValue(AqlValueHintBool(false));
      }
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
    TRI_ASSERT(!isBoolean);
    projectionNode = node->getMember(4);
  }

  if (filterNode == nullptr && projectionNode->type == NODE_TYPE_REFERENCE &&
      value.isArray() && offset == 0 && count == INT64_MAX && !isBoolean) {
    // no filter and no limit... we can return the array as it is
    auto other = static_cast<Variable const*>(projectionNode->getData());

    if (other->id == variable->id) {
      // simplify `v[*]` to just `v` if it's already an array
      mustDestroy = true;
      guard.steal();
      return value;
    }
  }

  auto const& vopts = ctx.trx().vpackOptions();

  VPackBuffer<uint8_t> buffer;
  VPackBuilder builder(buffer);

  if (!isBoolean) {
    builder.openArray();
  }

  size_t const n = value.length();

  // relevant only in case isBoolean = true
  size_t minRequiredItems = 0;
  size_t maxRequiredItems = 0;
  size_t takenItems = 0;

  if (quantifierNode == nullptr || quantifierNode->type == NODE_TYPE_NOP) {
    // no quantifier. assume we need at least 1 item
    minRequiredItems = 1;
    maxRequiredItems = std::numeric_limits<decltype(maxRequiredItems)>::max();
  } else {
    // note: quantifierNode can be a NODE_TYPE_QUANTIFIER (ALL|ANY|NONE),
    // a number (e.g. 3), or a range (e.g. 1..5)
    auto getRangeBound = [&ctx](AstNode const* node) -> int64_t {
      bool localMustDestroy;
      AqlValue sub =
          executeSimpleExpression(ctx, node, localMustDestroy, false);
      int64_t value = sub.toInt64();
      if (value < 0) {
        value = 0;
      }
      value = static_cast<size_t>(value);
      if (localMustDestroy) {
        sub.destroy();
      }
      return value;
    };

    if (quantifierNode->type == NODE_TYPE_QUANTIFIER) {
      // ALL|ANY|NONE|AT LEAST
      int64_t atLeast = 0;
      if (Quantifier::isAtLeast(quantifierNode)) {
        // evaluate expression for AT LEAST
        TRI_ASSERT(quantifierNode->numMembers() == 1);

        bool mustDestroy = false;
        AqlValue atLeastValue = executeSimpleExpression(
            ctx, quantifierNode->getMember(0), mustDestroy, false);
        AqlValueGuard guard(atLeastValue, mustDestroy);
        atLeast = atLeastValue.toInt64();
        if (atLeast < 0) {
          atLeast = 0;
        }
        TRI_ASSERT(atLeast >= 0);
      }
      std::tie(minRequiredItems, maxRequiredItems) =
          Quantifier::requiredMatches(n, quantifierNode,
                                      static_cast<size_t>(atLeast));
    } else if (quantifierNode->type == NODE_TYPE_RANGE) {
      // range
      TRI_ASSERT(quantifierNode->numMembers() == 2);

      minRequiredItems = getRangeBound(quantifierNode->getMember(0));
      maxRequiredItems = getRangeBound(quantifierNode->getMember(1));
    } else {
      // exact value
      minRequiredItems = maxRequiredItems = getRangeBound(quantifierNode);
    }
  }

  for (size_t i = 0; i < n; ++i) {
    bool localMustDestroy;
    AqlValue item = value.at(i, localMustDestroy, false);
    AqlValueGuard guard(item, localMustDestroy);

    AqlValueMaterializer materializer(&vopts);
    // register temporary variable in context
    ctx.setVariable(variable, materializer.slice(item));

    bool takeItem = true;

    try {
      if (filterNode != nullptr) {
        // have a filter
        AqlValue sub =
            executeSimpleExpression(ctx, filterNode, localMustDestroy, false);

        takeItem = sub.toBoolean();
        if (localMustDestroy) {
          sub.destroy();
        }
      }

      if (takeItem && offset > 0) {
        TRI_ASSERT(!isBoolean);
        // there is an offset in place
        --offset;
        takeItem = false;
      }

      if (takeItem) {
        ++takenItems;

        if (!isBoolean) {
          AqlValue sub = executeSimpleExpression(ctx, projectionNode,
                                                 localMustDestroy, false);
          sub.toVelocyPack(&vopts, builder, /*allowUnindexed*/ false);
          if (localMustDestroy) {
            sub.destroy();
          }
        }
      }
      ctx.clearVariable(variable);
    } catch (...) {
      // always unregister the variable from the context
      ctx.clearVariable(variable);
      throw;
    }

    if (takeItem && count > 0) {
      // number of items to pick was restricted
      if (--count == 0) {
        // done
        break;
      }
    }
  }

  if (isBoolean) {
    return AqlValue(AqlValueHintBool(takenItems >= minRequiredItems &&
                                     takenItems <= maxRequiredItems));
  }

  builder.close();
  mustDestroy = true;
  return AqlValue(std::move(buffer));  // builder = dynamic data
}

// execute an expression of type ExpressionType::kSimple with ITERATOR
AqlValue Expression::executeSimpleExpressionIterator(ExpressionContext& ctx,
                                                     AstNode const* node,
                                                     bool& mustDestroy) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 2);

  return executeSimpleExpression(ctx, node->getMember(1), mustDestroy, true);
}

// execute an expression of type ExpressionType::kSimple with BINARY_* (+, -, *
// , /, %)
AqlValue Expression::executeSimpleExpressionArithmetic(ExpressionContext& ctx,
                                                       AstNode const* node,
                                                       bool& mustDestroy) {
  AqlValue lhs = executeSimpleExpression(ctx, node->getMemberUnchecked(0),
                                         mustDestroy, true);
  AqlValueGuard guardLhs(lhs, mustDestroy);

  AqlValue rhs = executeSimpleExpression(ctx, node->getMemberUnchecked(1),
                                         mustDestroy, true);
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
    if (node->type == NODE_TYPE_OPERATOR_BINARY_DIV ||
        node->type == NODE_TYPE_OPERATOR_BINARY_MOD) {
      // division by zero
      TRI_ASSERT(!mustDestroy);
      ctx.registerWarning(
          TRI_ERROR_QUERY_DIVISION_BY_ZERO,
          absl::StrCat(
              "in operator ",
              (node->type == NODE_TYPE_OPERATOR_BINARY_DIV ? "/" : "%"), ": ",
              TRI_errno_string(TRI_ERROR_QUERY_DIVISION_BY_ZERO)));
      return AqlValue(AqlValueHintNull());
    }
  }

  mustDestroy = false;
  double result = 0.0;

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
      TRI_ASSERT(r != 0.0);
      result = l / r;
      break;
    case NODE_TYPE_OPERATOR_BINARY_MOD:
      result = fmod(l, r);
      break;
    default:
      break;
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
  TRI_ASSERT(_type != ExpressionType::kUnprocessed);
  return (_type == ExpressionType::kJson ||
          _node->canRunOnDBServer(isOneShard));
}

bool Expression::isDeterministic() {
  TRI_ASSERT(_type != ExpressionType::kUnprocessed);
  return (_type == ExpressionType::kJson || _node->isDeterministic());
}

bool Expression::willUseV8() {
  TRI_ASSERT(_type != ExpressionType::kUnprocessed);
  return (_type == ExpressionType::kSimple && _node->willUseV8());
}

bool Expression::canBeUsedInPrune(bool isOneShard, std::string& errorReason) {
  errorReason.clear();

  if (willUseV8()) {
    errorReason = "JavaScript expressions cannot be used inside PRUNE";
    return false;
  }
  if (!canRunOnDBServer(isOneShard)) {
    errorReason =
        "PRUNE expression contains a function that cannot be used on "
        "DB-Servers";
    return false;
  }

  TRI_ASSERT(errorReason.empty());
  return true;
}

std::unique_ptr<Expression> Expression::clone(Ast* ast, bool deepCopy) {
  // We do not need to copy the _ast, since it is managed by the
  // query object and the memory management of the ASTs
  if (ast == nullptr) {
    ast = _ast;
  }
  TRI_ASSERT(ast != nullptr);

  AstNode* source = _node;
  TRI_ASSERT(source != nullptr);
  if (deepCopy) {
    source = source->clone(ast);
  }
  return std::make_unique<Expression>(ast, source);
}

void Expression::toVelocyPack(arangodb::velocypack::Builder& builder,
                              bool verbose) const {
  _node->toVelocyPack(builder, verbose);
}

std::string_view Expression::typeString() {
  TRI_ASSERT(_type != ExpressionType::kUnprocessed);

  switch (_type) {
    case ExpressionType::kJson:
      return "json";
    case ExpressionType::kSimple:
      return "simple";
    case ExpressionType::kAttributeAccess:
      return "attribute";
    case ExpressionType::kUnprocessed: {
    }
  }
  TRI_ASSERT(false);
  return "unknown";
}

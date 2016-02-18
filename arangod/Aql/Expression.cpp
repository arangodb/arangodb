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

#include "Aql/Expression.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/AqlValue.h"
#include "Aql/Ast.h"
#include "Aql/AttributeAccessor.h"
#include "Aql/Executor.h"
#include "Aql/Quantifier.h"
#include "Aql/V8Expression.h"
#include "Aql/Variable.h"
#include "Basics/Exceptions.h"
#include "Basics/JsonHelper.h"
#include "Basics/StringBuffer.h"
#include "Basics/json.h"
#include "VocBase/document-collection.h"
#include "VocBase/shaped-json.h"

using namespace arangodb::aql;
using Json = arangodb::basics::Json;
using JsonHelper = arangodb::basics::JsonHelper;

////////////////////////////////////////////////////////////////////////////////
/// @brief "constant" global object for NULL which can be shared by all
/// expressions but must never be freed
////////////////////////////////////////////////////////////////////////////////

TRI_json_t const Expression::NullJson = {TRI_JSON_NULL, {false}};

////////////////////////////////////////////////////////////////////////////////
/// @brief "constant" global object for TRUE which can be shared by all
/// expressions but must never be freed
////////////////////////////////////////////////////////////////////////////////

TRI_json_t const Expression::TrueJson = {TRI_JSON_BOOLEAN, {true}};

////////////////////////////////////////////////////////////////////////////////
/// @brief "constant" global object for FALSE which can be shared by all
/// expressions but must never be freed
////////////////////////////////////////////////////////////////////////////////

TRI_json_t const Expression::FalseJson = {TRI_JSON_BOOLEAN, {false}};

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
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, _data);
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

AqlValue Expression::execute(arangodb::AqlTransaction* trx,
                             AqlItemBlock const* argv, size_t startPos,
                             std::vector<Variable const*> const& vars,
                             std::vector<RegisterId> const& regs,
                             TRI_document_collection_t const** collection) {
  if (!_built) {
    buildExpression();
  }

  TRI_ASSERT(_type != UNPROCESSED);
  TRI_ASSERT(_built);

  // and execute
  switch (_type) {
    case JSON: {
      TRI_ASSERT(_data != nullptr);
      return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, _data, Json::NOFREE));
    }

    case SIMPLE: {
      return executeSimpleExpression(_node, collection, trx, argv, startPos,
                                     vars, regs, true);
    }

    case ATTRIBUTE: {
      TRI_ASSERT(_accessor != nullptr);
      return _accessor->get(trx, argv, startPos, vars, regs);
    }

    case V8: {
      TRI_ASSERT(_func != nullptr);
      try {
        ISOLATE;
        // Dump the expression in question
        // std::cout << arangodb::basics::Json(TRI_UNKNOWN_MEM_ZONE,
        // _node->toJson(TRI_UNKNOWN_MEM_ZONE, true)).toString()<< "\n";
        return _func->execute(isolate, _ast->query(), trx, argv, startPos, vars,
                              regs);
      } catch (arangodb::basics::Exception& ex) {
        if (_ast->query()->verboseErrors()) {
          ex.addToMessage(" while evaluating expression ");
          auto json = _node->toJson(TRI_UNKNOWN_MEM_ZONE, false);

          if (json != nullptr) {
            ex.addToMessage(arangodb::basics::JsonHelper::toString(json));
            TRI_Free(TRI_UNKNOWN_MEM_ZONE, json);
          }
        }
        throw;
      }
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

bool Expression::findInArray(AqlValue const& left, AqlValue const& right,
                             TRI_document_collection_t const* leftCollection,
                             TRI_document_collection_t const* rightCollection,
                             arangodb::AqlTransaction* trx,
                             AstNode const* node) const {
  TRI_ASSERT(right.isArray());

  size_t const n = right.arraySize();

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
      auto arrayItem = right.extractArrayMember(trx, rightCollection, m, false);
      AqlValue arrayItemValue(&arrayItem);

      int compareResult = AqlValue::Compare(trx, left, leftCollection,
                                            arrayItemValue, nullptr, false);

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
  } else {
    // use linear search
    for (size_t i = 0; i < n; ++i) {
      // do not copy the list element we're looking at
      auto arrayItem = right.extractArrayMember(trx, rightCollection, i, false);
      AqlValue arrayItemValue(&arrayItem);

      int compareResult = AqlValue::Compare(trx, left, leftCollection,
                                            arrayItemValue, nullptr, false);

      if (compareResult == 0) {
        // item found in the list
        return true;
      }
    }

    return false;
  }
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
    _data = _node->toJsonValue(TRI_UNKNOWN_MEM_ZONE);

    if (_data == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "invalid json in simple expression");
    }
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

AqlValue Expression::executeSimpleExpression(
    AstNode const* node, TRI_document_collection_t const** collection,
    arangodb::AqlTransaction* trx, AqlItemBlock const* argv, size_t startPos,
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
      return executeSimpleExpressionReference(node, collection, argv, startPos,
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
      return executeSimpleExpressionIterator(node, collection, trx, argv,
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

AqlValue Expression::executeSimpleExpressionAttributeAccess(
    AstNode const* node, arangodb::AqlTransaction* trx,
    AqlItemBlock const* argv, size_t startPos,
    std::vector<Variable const*> const& vars,
    std::vector<RegisterId> const& regs) {
  // object lookup, e.g. users.name
  TRI_ASSERT(node->numMembers() == 1);

  auto member = node->getMemberUnchecked(0);
  auto name = static_cast<char const*>(node->getData());

  TRI_document_collection_t const* myCollection = nullptr;
  AqlValue result = executeSimpleExpression(member, &myCollection, trx, argv,
                                            startPos, vars, regs, false);

  auto j = result.extractObjectMember(trx, myCollection, name, true, _buffer);
  result.destroy();
  return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, j.steal()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute an expression of type SIMPLE with INDEXED ACCESS
////////////////////////////////////////////////////////////////////////////////

AqlValue Expression::executeSimpleExpressionIndexedAccess(
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

  TRI_document_collection_t const* myCollection = nullptr;
  AqlValue result = executeSimpleExpression(member, &myCollection, trx, argv,
                                            startPos, vars, regs, false);

  if (result.isArray()) {
    TRI_document_collection_t const* myCollection2 = nullptr;
    AqlValue indexResult = executeSimpleExpression(
        index, &myCollection2, trx, argv, startPos, vars, regs, false);

    if (indexResult.isNumber()) {
      auto j = result.extractArrayMember(trx, myCollection,
                                         indexResult.toInt64(), true);
      indexResult.destroy();
      result.destroy();
      return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, j.steal()));
    } else if (indexResult.isString()) {
      auto value(indexResult.toString());
      indexResult.destroy();

      try {
        // stoll() might throw an exception if the string is not a number
        int64_t position = static_cast<int64_t>(std::stoll(value.c_str()));
        auto j = result.extractArrayMember(trx, myCollection, position, true);
        result.destroy();
        return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, j.steal()));
      } catch (...) {
        // no number found.
      }
    } else {
      indexResult.destroy();
    }

    // fall-through to returning null
  } else if (result.isObject()) {
    TRI_document_collection_t const* myCollection2 = nullptr;
    AqlValue indexResult = executeSimpleExpression(
        index, &myCollection2, trx, argv, startPos, vars, regs, false);

    if (indexResult.isNumber()) {
      auto&& indexString = std::to_string(indexResult.toInt64());
      auto j = result.extractObjectMember(trx, myCollection,
                                          indexString.c_str(), true, _buffer);
      indexResult.destroy();
      result.destroy();
      return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, j.steal()));
    } else if (indexResult.isString()) {
      auto&& value = indexResult.toString();
      indexResult.destroy();

      auto j = result.extractObjectMember(trx, myCollection, value.c_str(),
                                          true, _buffer);
      result.destroy();
      return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, j.steal()));
    } else {
      indexResult.destroy();
    }

    // fall-through to returning null
  }
  result.destroy();

  return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, &NullJson, Json::NOFREE));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute an expression of type SIMPLE with ARRAY
////////////////////////////////////////////////////////////////////////////////

AqlValue Expression::executeSimpleExpressionArray(
    AstNode const* node, arangodb::AqlTransaction* trx,
    AqlItemBlock const* argv, size_t startPos,
    std::vector<Variable const*> const& vars,
    std::vector<RegisterId> const& regs) {
  if (node->isConstant()) {
    auto json = node->computeJson();

    if (json == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }

    // we do not own the JSON but the node does!
    return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, json, Json::NOFREE));
  }

  size_t const n = node->numMembers();
  auto array = std::make_unique<Json>(Json::Array, n);

  for (size_t i = 0; i < n; ++i) {
    auto member = node->getMemberUnchecked(i);
    TRI_document_collection_t const* myCollection = nullptr;

    AqlValue result = executeSimpleExpression(member, &myCollection, trx, argv,
                                              startPos, vars, regs, false);
    array->add(result.toJson(trx, myCollection, true));
    result.destroy();
  }

  return AqlValue(array.release());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute an expression of type SIMPLE with OBJECT
////////////////////////////////////////////////////////////////////////////////

AqlValue Expression::executeSimpleExpressionObject(
    AstNode const* node, arangodb::AqlTransaction* trx,
    AqlItemBlock const* argv, size_t startPos,
    std::vector<Variable const*> const& vars,
    std::vector<RegisterId> const& regs) {
  if (node->isConstant()) {
    auto json = node->computeJson();

    if (json == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }

    // we do not own the JSON but the node does!
    return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, json, Json::NOFREE));
  }

  size_t const n = node->numMembers();
  auto object = std::make_unique<Json>(Json::Object, n);

  for (size_t i = 0; i < n; ++i) {
    auto member = node->getMemberUnchecked(i);
    TRI_document_collection_t const* myCollection = nullptr;

    TRI_ASSERT(member->type == NODE_TYPE_OBJECT_ELEMENT);
    auto key = member->getStringValue();
    member = member->getMember(0);

    AqlValue result = executeSimpleExpression(member, &myCollection, trx, argv,
                                              startPos, vars, regs, false);
    object->set(key, result.toJson(trx, myCollection, true));
    result.destroy();
  }
  return AqlValue(object.release());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute an expression of type SIMPLE with VALUE
////////////////////////////////////////////////////////////////////////////////

AqlValue Expression::executeSimpleExpressionValue(AstNode const* node) {
  auto json = node->computeJson();

  if (json == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  // we do not own the JSON but the node does!
  return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, json, Json::NOFREE));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute an expression of type SIMPLE with REFERENCE
////////////////////////////////////////////////////////////////////////////////

AqlValue Expression::executeSimpleExpressionReference(
    AstNode const* node, TRI_document_collection_t const** collection,
    AqlItemBlock const* argv, size_t startPos,
    std::vector<Variable const*> const& vars,
    std::vector<RegisterId> const& regs, bool doCopy) {
  auto v = static_cast<Variable const*>(node->getData());

  {
    auto it = _variables.find(v);

    if (it != _variables.end()) {
      *collection = nullptr;
      auto copy = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, (*it).second);

      if (copy == nullptr) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
      }

      return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, copy));
    }
  }

  size_t i = 0;
  for (auto it = vars.begin(); it != vars.end(); ++it, ++i) {
    if ((*it)->name == v->name) {
      TRI_ASSERT(collection != nullptr);

      // save the collection info
      *collection = argv->getDocumentCollection(regs[i]);

      if (doCopy) {
        return argv->getValueReference(startPos, regs[i]).clone();
      }

      // AqlValue.destroy() will be called for the returned value soon,
      // so we must not return the original AqlValue from the AqlItemBlock here
      return argv->getValueReference(startPos, regs[i]).shallowClone();
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

AqlValue Expression::executeSimpleExpressionRange(
    AstNode const* node, arangodb::AqlTransaction* trx,
    AqlItemBlock const* argv, size_t startPos,
    std::vector<Variable const*> const& vars,
    std::vector<RegisterId> const& regs) {
  TRI_document_collection_t const* leftCollection = nullptr;
  TRI_document_collection_t const* rightCollection = nullptr;

  auto low = node->getMember(0);
  auto high = node->getMember(1);
  AqlValue resultLow = executeSimpleExpression(low, &leftCollection, trx, argv,
                                               startPos, vars, regs, false);
  AqlValue resultHigh = executeSimpleExpression(
      high, &rightCollection, trx, argv, startPos, vars, regs, false);
  AqlValue res = AqlValue(resultLow.toInt64(), resultHigh.toInt64());
  resultLow.destroy();
  resultHigh.destroy();

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute an expression of type SIMPLE with FCALL
////////////////////////////////////////////////////////////////////////////////

AqlValue Expression::executeSimpleExpressionFCall(
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

  size_t const n = member->numMembers();
  FunctionParameters parameters;
  parameters.reserve(n);

  try {
    for (size_t i = 0; i < n; ++i) {
      TRI_document_collection_t const* myCollection = nullptr;
      auto arg = member->getMemberUnchecked(i);

      if (arg->type == NODE_TYPE_COLLECTION) {
        parameters.emplace_back(
            AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, arg->getStringValue(),
                              arg->getStringLength())),
            nullptr);
      } else {
        parameters.emplace_back(
            executeSimpleExpression(arg, &myCollection, trx, argv, startPos,
                                    vars, regs, false),
            myCollection);
      }
    }

    auto res2 = func->implementation(_ast->query(), trx, parameters);

    for (auto& it : parameters) {
      it.first.destroy();
    }
    return res2;
  } catch (...) {
    // prevent leak and rethrow error
    for (auto& it : parameters) {
      it.first.destroy();
    }
    throw;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute an expression of type SIMPLE with NOT
////////////////////////////////////////////////////////////////////////////////

AqlValue Expression::executeSimpleExpressionNot(
    AstNode const* node, arangodb::AqlTransaction* trx,
    AqlItemBlock const* argv, size_t startPos,
    std::vector<Variable const*> const& vars,
    std::vector<RegisterId> const& regs) {
  TRI_document_collection_t const* myCollection = nullptr;
  AqlValue operand =
      executeSimpleExpression(node->getMember(0), &myCollection, trx, argv,
                              startPos, vars, regs, false);

  bool const operandIsTrue = operand.isTrue();
  operand.destroy();
  return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE,
                           operandIsTrue ? &FalseJson : &TrueJson,
                           Json::NOFREE));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute an expression of type SIMPLE with AND or OR
////////////////////////////////////////////////////////////////////////////////

AqlValue Expression::executeSimpleExpressionAndOr(
    AstNode const* node, arangodb::AqlTransaction* trx,
    AqlItemBlock const* argv, size_t startPos,
    std::vector<Variable const*> const& vars,
    std::vector<RegisterId> const& regs) {
  TRI_document_collection_t const* leftCollection = nullptr;
  AqlValue left =
      executeSimpleExpression(node->getMember(0), &leftCollection, trx, argv,
                              startPos, vars, regs, true);
  TRI_document_collection_t const* rightCollection = nullptr;
  AqlValue right =
      executeSimpleExpression(node->getMember(1), &rightCollection, trx, argv,
                              startPos, vars, regs, true);

  if (node->type == NODE_TYPE_OPERATOR_BINARY_AND) {
    // AND
    if (left.isTrue()) {
      // left is true => return right
      left.destroy();
      return right;
    }

    // left is false, return left
    right.destroy();
    return left;
  } else {
    // OR
    if (left.isTrue()) {
      // left is true => return left
      right.destroy();
      return left;
    }

    // left is false => return right
    left.destroy();
    return right;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute an expression of type SIMPLE with COMPARISON
////////////////////////////////////////////////////////////////////////////////

AqlValue Expression::executeSimpleExpressionComparison(
    AstNode const* node, arangodb::AqlTransaction* trx,
    AqlItemBlock const* argv, size_t startPos,
    std::vector<Variable const*> const& vars,
    std::vector<RegisterId> const& regs) {
  TRI_document_collection_t const* leftCollection = nullptr;
  AqlValue left =
      executeSimpleExpression(node->getMember(0), &leftCollection, trx, argv,
                              startPos, vars, regs, false);
  TRI_document_collection_t const* rightCollection = nullptr;
  AqlValue right =
      executeSimpleExpression(node->getMember(1), &rightCollection, trx, argv,
                              startPos, vars, regs, false);

  if (node->type == NODE_TYPE_OPERATOR_BINARY_IN ||
      node->type == NODE_TYPE_OPERATOR_BINARY_NIN) {
    // IN and NOT IN
    if (!right.isArray()) {
      // right operand must be a list, otherwise we return false
      left.destroy();
      right.destroy();
      // do not throw, but return "false" instead
      return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, &FalseJson, Json::NOFREE));
    }

    bool result =
        findInArray(left, right, leftCollection, rightCollection, trx, node);

    if (node->type == NODE_TYPE_OPERATOR_BINARY_NIN) {
      // revert the result in case of a NOT IN
      result = !result;
    }

    left.destroy();
    right.destroy();

    return AqlValue(new arangodb::basics::Json(result));
  }

  // all other comparison operators...

  // for equality and non-equality we can use a binary comparison
  bool compareUtf8 = (node->type != NODE_TYPE_OPERATOR_BINARY_EQ &&
                      node->type != NODE_TYPE_OPERATOR_BINARY_NE);

  int compareResult = AqlValue::Compare(trx, left, leftCollection, right,
                                        rightCollection, compareUtf8);
  left.destroy();
  right.destroy();
  switch (node->type) {
    case NODE_TYPE_OPERATOR_BINARY_EQ:
      return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE,
                               (compareResult == 0) ? &TrueJson : &FalseJson,
                               Json::NOFREE));
    case NODE_TYPE_OPERATOR_BINARY_NE:
      return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE,
                               (compareResult != 0) ? &TrueJson : &FalseJson,
                               Json::NOFREE));
    case NODE_TYPE_OPERATOR_BINARY_LT:
      return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE,
                               (compareResult < 0) ? &TrueJson : &FalseJson,
                               Json::NOFREE));
    case NODE_TYPE_OPERATOR_BINARY_LE:
      return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE,
                               (compareResult <= 0) ? &TrueJson : &FalseJson,
                               Json::NOFREE));
    case NODE_TYPE_OPERATOR_BINARY_GT:
      return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE,
                               (compareResult > 0) ? &TrueJson : &FalseJson,
                               Json::NOFREE));
    case NODE_TYPE_OPERATOR_BINARY_GE:
      return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE,
                               (compareResult >= 0) ? &TrueJson : &FalseJson,
                               Json::NOFREE));
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

AqlValue Expression::executeSimpleExpressionArrayComparison(
    AstNode const* node, arangodb::AqlTransaction* trx,
    AqlItemBlock const* argv, size_t startPos,
    std::vector<Variable const*> const& vars,
    std::vector<RegisterId> const& regs) {
  TRI_document_collection_t const* leftCollection = nullptr;
  AqlValue left =
      executeSimpleExpression(node->getMember(0), &leftCollection, trx, argv,
                              startPos, vars, regs, false);
  TRI_document_collection_t const* rightCollection = nullptr;
  AqlValue right =
      executeSimpleExpression(node->getMember(1), &rightCollection, trx, argv,
                              startPos, vars, regs, false);

  if (!left.isArray()) {
    // left operand must be an array
    left.destroy();
    right.destroy();
    // do not throw, but return "false" instead
    return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, &FalseJson, Json::NOFREE));
  }
  
  if (node->type == NODE_TYPE_OPERATOR_BINARY_ARRAY_IN ||
      node->type == NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN) {
    // IN and NOT IN
    if (!right.isArray()) {
      // right operand must be a list, otherwise we return false
      left.destroy();
      right.destroy();
      // do not throw, but return "false" instead
      return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, &FalseJson, Json::NOFREE));
    }
  }

  size_t const n = left.arraySize();
  std::pair<size_t, size_t> requiredMatches = Quantifier::RequiredMatches(n, node->getMember(2));

  TRI_ASSERT(requiredMatches.first <= requiredMatches.second);
 
  // for equality and non-equality we can use a binary comparison
  bool const compareUtf8 = (node->type != NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ &&
                            node->type != NODE_TYPE_OPERATOR_BINARY_ARRAY_NE);

  bool overallResult = true;
  size_t matches = 0;
  size_t numLeft = n;
  
  for (size_t i = 0; i < n; ++i) {
    auto leftItem = left.extractArrayMember(trx, leftCollection, static_cast<int64_t>(i), false);
    AqlValue leftItemValue(&leftItem);
    bool result;

    // IN and NOT IN
    if (node->type == NODE_TYPE_OPERATOR_BINARY_ARRAY_IN ||
        node->type == NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN) {
      result =
          findInArray(leftItemValue, right, nullptr, rightCollection, trx, node);
    
      if (node->type == NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN) {
        // revert the result in case of a NOT IN
        result = !result;
      }
    }
    else {
      // other operators
      int compareResult = AqlValue::Compare(trx, leftItemValue, nullptr,
                                            right, rightCollection, compareUtf8);

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
  return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, overallResult ? &TrueJson : &FalseJson, Json::NOFREE));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute an expression of type SIMPLE with TERNARY
////////////////////////////////////////////////////////////////////////////////

AqlValue Expression::executeSimpleExpressionTernary(
    AstNode const* node, arangodb::AqlTransaction* trx,
    AqlItemBlock const* argv, size_t startPos,
    std::vector<Variable const*> const& vars,
    std::vector<RegisterId> const& regs) {
  TRI_document_collection_t const* myCollection = nullptr;
  AqlValue condition =
      executeSimpleExpression(node->getMember(0), &myCollection, trx, argv,
                              startPos, vars, regs, false);

  bool const isTrue = condition.isTrue();
  condition.destroy();
  if (isTrue) {
    // return true part
    return executeSimpleExpression(node->getMember(1), &myCollection, trx, argv,
                                   startPos, vars, regs, true);
  }

  // return false part
  return executeSimpleExpression(node->getMember(2), &myCollection, trx, argv,
                                 startPos, vars, regs, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute an expression of type SIMPLE with EXPANSION
////////////////////////////////////////////////////////////////////////////////

AqlValue Expression::executeSimpleExpressionExpansion(
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
    TRI_document_collection_t const* subCollection = nullptr;
    AqlValue sub =
        executeSimpleExpression(limitNode->getMember(0), &subCollection, trx,
                                argv, startPos, vars, regs, false);
    offset = sub.toInt64();
    sub.destroy();

    subCollection = nullptr;
    sub = executeSimpleExpression(limitNode->getMember(1), &subCollection, trx,
                                  argv, startPos, vars, regs, false);
    count = sub.toInt64();
    sub.destroy();
  }

  if (offset < 0 || count <= 0) {
    // no items to return... can already stop here
    return AqlValue(new arangodb::basics::Json(arangodb::basics::Json::Array));
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
      return AqlValue(
          new arangodb::basics::Json(arangodb::basics::Json::Array));
    }
  }

  auto iterator = node->getMember(0);
  auto variable = static_cast<Variable*>(iterator->getMember(0)->getData());
  auto levels = node->getIntValue(true);

  AqlValue value;

  if (levels > 1) {
    // flatten value...

    // generate a new temporary for the flattened array
    auto flattened = std::make_unique<Json>(Json::Array);

    TRI_document_collection_t const* myCollection = nullptr;
    value = executeSimpleExpression(node->getMember(0), &myCollection, trx,
                                    argv, startPos, vars, regs, false);

    if (!value.isArray()) {
      value.destroy();
      return AqlValue(
          new arangodb::basics::Json(arangodb::basics::Json::Array));
    }

    std::function<void(TRI_json_t const*, int64_t)> flatten =
        [&](TRI_json_t const* json, int64_t level) {
          if (!TRI_IsArrayJson(json)) {
            return;
          }

          size_t const n = TRI_LengthArrayJson(json);

          for (size_t i = 0; i < n; ++i) {
            auto item = static_cast<TRI_json_t const*>(
                TRI_AtVector(&json->_value._objects, i));

            bool const isArray = TRI_IsArrayJson(item);

            if (!isArray || level == levels) {
              auto copy = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, item);

              if (copy == nullptr) {
                THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
              }

              flattened->add(copy);
            } else if (isArray && level < levels) {
              flatten(item, level + 1);
            }
          }
        };

    auto subJson = value.toJson(trx, myCollection, false);
    flatten(subJson.json(), 1);
    value.destroy();

    value = AqlValue(flattened.release());
  } else {
    TRI_document_collection_t const* myCollection = nullptr;
    value = executeSimpleExpression(node->getMember(0), &myCollection, trx,
                                    argv, startPos, vars, regs, false);

    if (!value.isArray()) {
      // must cast value to array first
      value.destroy();
      return AqlValue(
          new arangodb::basics::Json(arangodb::basics::Json::Array));
    }
  }

  // RETURN
  // the default is to return array member unmodified
  AstNode const* projectionNode = node->getMember(1);

  if (node->getMember(4)->type != NODE_TYPE_NOP) {
    // return projection
    projectionNode = node->getMember(4);
  }

  size_t const n = value.arraySize();
  auto array = std::make_unique<Json>(Json::Array, n);

  for (size_t i = 0; i < n; ++i) {
    // TODO: check why we must copy the array member. will crash without
    // copying!
    TRI_document_collection_t const* myCollection = nullptr;
    auto arrayItem = value.extractArrayMember(trx, myCollection, i, true);

    setVariable(variable, arrayItem.json());

    bool takeItem = true;

    if (filterNode != nullptr) {
      // have a filter
      TRI_document_collection_t const* subCollection = nullptr;
      AqlValue sub = executeSimpleExpression(filterNode, &subCollection, trx,
                                             argv, startPos, vars, regs, false);
      takeItem = sub.isTrue();
      sub.destroy();
    }

    if (takeItem && offset > 0) {
      // there is an offset in place
      --offset;
      takeItem = false;
    }

    if (takeItem) {
      TRI_document_collection_t const* subCollection = nullptr;
      AqlValue sub =
          executeSimpleExpression(projectionNode, &subCollection, trx, argv,
                                  startPos, vars, regs, true);
      array->add(sub.toJson(trx, subCollection, true));
      sub.destroy();
    }

    clearVariable(variable);

    arrayItem.destroy();

    if (takeItem && count > 0) {
      // number of items to pick was restricted
      if (--count == 0) {
        // done
        break;
      }
    }
  }

  value.destroy();
  return AqlValue(array.release());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute an expression of type SIMPLE with ITERATOR
////////////////////////////////////////////////////////////////////////////////

AqlValue Expression::executeSimpleExpressionIterator(
    AstNode const* node, TRI_document_collection_t const** collection,
    arangodb::AqlTransaction* trx, AqlItemBlock const* argv, size_t startPos,
    std::vector<Variable const*> const& vars,
    std::vector<RegisterId> const& regs) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 2);

  *collection = nullptr;
  return executeSimpleExpression(node->getMember(1), collection, trx, argv,
                                 startPos, vars, regs, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute an expression of type SIMPLE with BINARY_* (+, -, * , /, %)
////////////////////////////////////////////////////////////////////////////////

AqlValue Expression::executeSimpleExpressionArithmetic(
    AstNode const* node, arangodb::AqlTransaction* trx,
    AqlItemBlock const* argv, size_t startPos,
    std::vector<Variable const*> const& vars,
    std::vector<RegisterId> const& regs) {
  TRI_document_collection_t const* leftCollection = nullptr;
  AqlValue lhs = executeSimpleExpression(node->getMember(0), &leftCollection,
                                         trx, argv, startPos, vars, regs, true);

  if (lhs.isObject()) {
    lhs.destroy();
    return AqlValue(new Json(Json::Null));
  }

  TRI_document_collection_t const* rightCollection = nullptr;
  AqlValue rhs = executeSimpleExpression(node->getMember(1), &rightCollection,
                                         trx, argv, startPos, vars, regs, true);

  if (rhs.isObject()) {
    lhs.destroy();
    rhs.destroy();
    return AqlValue(new Json(Json::Null));
  }

  bool failed = false;
  double l = lhs.toNumber(failed);
  lhs.destroy();

  if (failed) {
    rhs.destroy();
    return AqlValue(new Json(Json::Null));
  }

  double r = rhs.toNumber(failed);
  rhs.destroy();

  if (failed) {
    return AqlValue(new Json(Json::Null));
  }

  switch (node->type) {
    case NODE_TYPE_OPERATOR_BINARY_PLUS:
      return AqlValue(new Json(l + r));
    case NODE_TYPE_OPERATOR_BINARY_MINUS:
      return AqlValue(new Json(l - r));
    case NODE_TYPE_OPERATOR_BINARY_TIMES:
      return AqlValue(new Json(l * r));
    case NODE_TYPE_OPERATOR_BINARY_DIV:
      if (r == 0) {
        RegisterWarning(_ast, "/", TRI_ERROR_QUERY_DIVISION_BY_ZERO);
        return AqlValue(new Json(Json::Null));
      }
      return AqlValue(new Json(l / r));
    case NODE_TYPE_OPERATOR_BINARY_MOD:
      return AqlValue(new Json(fmod(l, r)));
    default:
      return AqlValue(new Json(Json::Null));
  }
}

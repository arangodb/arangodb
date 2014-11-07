////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, expression
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Aql/Expression.h"
#include "Aql/AqlValue.h"
#include "Aql/Ast.h"
#include "Aql/Executor.h"
#include "Aql/V8Expression.h"
#include "Aql/Variable.h"
#include "Basics/JsonHelper.h"
#include "Basics/StringBuffer.h"
#include "Basics/json.h"
#include "ShapedJson/shaped-json.h"
#include "VocBase/document-collection.h"
#include "Utils/Exception.h"

using namespace triagens::aql;
using Json = triagens::basics::Json;
using JsonHelper = triagens::basics::JsonHelper;

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the expression
////////////////////////////////////////////////////////////////////////////////

Expression::Expression (Ast* ast,
                        AstNode const* node)
  : _ast(ast),
    _executor(_ast->query()->executor()),
    _node(node),
    _type(UNPROCESSED),
    _canThrow(true),
    _canRunOnDBServer(false),
    _isDeterministic(false),
    _built(false) {

  TRI_ASSERT(_ast != nullptr);
  TRI_ASSERT(_executor != nullptr);
  TRI_ASSERT(_node != nullptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an expression from JSON
////////////////////////////////////////////////////////////////////////////////

Expression::Expression (Ast* ast,
                        triagens::basics::Json const& json)
  : Expression(ast, new AstNode(ast, json.get("expression"))) {

}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the expression
////////////////////////////////////////////////////////////////////////////////

Expression::~Expression () {
  if (_built) {
    if (_type == V8) {
      delete _func;
      _func = nullptr;
    }
    else if (_type == JSON) {
      TRI_ASSERT(_data != nullptr);
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, _data);
      _data = nullptr;
      // _json is freed automatically by AqlItemBlock
    }
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return all variables used in the expression
////////////////////////////////////////////////////////////////////////////////

std::unordered_set<Variable*> Expression::variables () const {
  return Ast::getReferencedVariables(_node);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute the expression
////////////////////////////////////////////////////////////////////////////////

AqlValue Expression::execute (triagens::arango::AqlTransaction* trx,
                              std::vector<TRI_document_collection_t const*>& docColls,
                              std::vector<AqlValue>& argv,
                              size_t startPos,
                              std::vector<Variable*> const& vars,
                              std::vector<RegisterId> const& regs) {

  if (! _built) {
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

    case V8: {
      TRI_ASSERT(_func != nullptr);
      try {
        // Dump the expression in question  
        // std::cout << triagens::basics::Json(TRI_UNKNOWN_MEM_ZONE, _node->toJson(TRI_UNKNOWN_MEM_ZONE, true)).toString()<< "\n";
        return _func->execute(_ast->query(), trx, docColls, argv, startPos, vars, regs);
      }
      catch (triagens::arango::Exception& ex) {
        if (_ast->query()->verboseErrors()) {
          ex.addToMessage(" while evaluating expression ");
          auto json = _node->toJson(TRI_UNKNOWN_MEM_ZONE, false);
          if (json != nullptr) {
            ex.addToMessage(triagens::basics::JsonHelper::toString(json));
            TRI_Free(TRI_UNKNOWN_MEM_ZONE, json);
          }
        }
        throw;
      }
    }

    case SIMPLE: {
      TRI_document_collection_t const* myCollection = nullptr;
      return executeSimpleExpression(_node, &myCollection, trx, docColls, argv, startPos, vars, regs);
    }
 
    case UNPROCESSED: {
      // fall-through to exception
    }
  }
      
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid simple expression");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replace variables in the expression with other variables
////////////////////////////////////////////////////////////////////////////////

void Expression::replaceVariables (std::unordered_map<VariableId, Variable const*> const& replacements) {
  _node = _ast->clone(_node);
  TRI_ASSERT(_node != nullptr);

  _ast->replaceVariables(const_cast<AstNode*>(_node), replacements);
 
  invalidate(); 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief invalidates an expression
/// this only has an effect for V8-based functions, which need to be created,
/// used and destroyed in the same context. when a V8 function is used across
/// multiple V8 contexts, it must be invalidated in between
////////////////////////////////////////////////////////////////////////////////

void Expression::invalidate () {
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

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief find a value in a list node
/// this performs either a binary search (if the node is sorted) or a
/// linear search (if the node is not sorted)
////////////////////////////////////////////////////////////////////////////////

bool Expression::findInList (AqlValue const& left, 
                             AqlValue const& right, 
                             TRI_document_collection_t const* leftCollection, 
                             TRI_document_collection_t const* rightCollection,
                             triagens::arango::AqlTransaction* trx,
                             AstNode const* node) const {
  TRI_ASSERT_EXPENSIVE(right.isList());
 
  size_t const n = right.listSize();

  if (node->getMember(1)->isSorted()) {
    // node values are sorted. can use binary search
    size_t l = 0;
    size_t r = n - 1;

    while (true) {
      // determine midpoint
      size_t m = l + ((r - l) / 2);
      auto listItem = right.extractListMember(trx, rightCollection, m, false);
      AqlValue listItemValue(&listItem);

      int compareResult = AqlValue::Compare(trx, left, leftCollection, listItemValue, nullptr);

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
      }
      else {
        l = m + 1;
      }
      if (r < l) {
        return false;
      }
    }
  }
  else {
    // use linear search
    for (size_t i = 0; i < n; ++i) {
      // do not copy the list element we're looking at
      auto listItem = right.extractListMember(trx, rightCollection, i, false);
      AqlValue listItemValue(&listItem);

      int compareResult = AqlValue::Compare(trx, left, leftCollection, listItemValue, nullptr);

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

void Expression::analyzeExpression () {
  TRI_ASSERT(_type == UNPROCESSED);
  TRI_ASSERT(_built == false);

  if (_node->isConstant()) {
    // expression is a constant value
    _type = JSON;
    _canThrow = false;
    _canRunOnDBServer = true;
    _isDeterministic = true;
    _data = nullptr;
  }
  else if (_node->isSimple()) {
    // expression is a simple expression
    _type = SIMPLE;
    _canThrow = _node->canThrow();
    _canRunOnDBServer = _node->canRunOnDBServer();
    _isDeterministic = _node->isDeterministic();
  }
  else {
    // expression is a V8 expression
    _type = V8;
    _canThrow = _node->canThrow();
    _canRunOnDBServer = _node->canRunOnDBServer();
    _isDeterministic = _node->isDeterministic();
    _func = nullptr;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief build the expression
////////////////////////////////////////////////////////////////////////////////

void Expression::buildExpression () {
  TRI_ASSERT(! _built);

  if (_type == UNPROCESSED) {
    analyzeExpression();
  }

  if (_type == JSON) {
    // generate a constant value
    _data = _node->toJsonValue(TRI_UNKNOWN_MEM_ZONE);

    if (_data == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid json in simple expression");
    }
  }
  else if (_type == V8) {
    // generate a V8 expression
    _func = _executor->generateExpression(_node);
  }

  _built = true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute an expression of type SIMPLE, the convention is that
/// the resulting AqlValue will be destroyed outside eventually
////////////////////////////////////////////////////////////////////////////////

AqlValue Expression::executeSimpleExpression (AstNode const* node,
                                              TRI_document_collection_t const** collection, 
                                              triagens::arango::AqlTransaction* trx,
                                              std::vector<TRI_document_collection_t const*>& docColls,
                                              std::vector<AqlValue>& argv,
                                              size_t startPos,
                                              std::vector<Variable*> const& vars,
                                              std::vector<RegisterId> const& regs) {
  if (node->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
    // array lookup, e.g. users.name
    TRI_ASSERT(node->numMembers() == 1);

    auto member = node->getMember(0);
    auto name = static_cast<char const*>(node->getData());

    TRI_document_collection_t const* myCollection = nullptr;
    AqlValue result = executeSimpleExpression(member, &myCollection, trx, docColls, argv, startPos, vars, regs);

    auto j = result.extractArrayMember(trx, myCollection, name);
    result.destroy();
    return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, j.steal()));
  }
  
  else if (node->type == NODE_TYPE_INDEXED_ACCESS) {
    // list lookup, e.g. users[0]
    // note: it depends on the type of the value whether a list lookup or an array lookup is performed
    // for example, if the value is an array, then its elements might be access like this:
    // users['name'] or even users['0'] (as '0' is a valid attribute name, too)
    // if the value is a list, then string indexes might also be used and will be converted to integers, e.g.
    // users['0'] is the same as users[0], users['-2'] is the same as users[-2] etc.
    TRI_ASSERT(node->numMembers() == 2);

    auto member = node->getMember(0);
    auto index = node->getMember(1);

    TRI_document_collection_t const* myCollection = nullptr;
    AqlValue result = executeSimpleExpression(member, &myCollection, trx, docColls, argv, startPos, vars, regs);

    if (result.isList()) {
      if (index->isNumericValue()) {
        auto j = result.extractListMember(trx, myCollection, index->getIntValue(), true);
        result.destroy();
        return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, j.steal()));
      }
      else if (index->isStringValue()) {
        char const* p = index->getStringValue();
        TRI_ASSERT(p != nullptr);

        try {
          // stoll() might throw an exception if the string is not a number
          int64_t position = static_cast<int64_t>(std::stoll(p));
          auto j = result.extractListMember(trx, myCollection, position, true);
          result.destroy();
          return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, j.steal()));
        }
        catch (...) {
          // no number found. 
        }
      }
      // fall-through to returning null
    }
    else if (result.isArray()) {
      if (index->isNumericValue()) {
        std::string const indexString = std::to_string(index->getIntValue());
        auto j = result.extractArrayMember(trx, myCollection, indexString.c_str());
        result.destroy();
        return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, j.steal()));
      }
      else if (index->isStringValue()) {
        char const* p = index->getStringValue();
        TRI_ASSERT(p != nullptr);

        auto j = result.extractArrayMember(trx, myCollection, p);
        result.destroy();
        return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, j.steal()));
      }
      // fall-through to returning null
    }
    result.destroy();
      
    return AqlValue(new Json(Json::Null));
  }
  
  else if (node->type == NODE_TYPE_LIST) {
    if (node->isConstant()) {
      auto json = node->computeJson();

      if (json == nullptr) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
      }

      // we do not own the JSON but the node does!
      return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, json, Json::NOFREE));
    }

    size_t const n = node->numMembers();
    auto list = new Json(Json::List, n);

    try {
      for (size_t i = 0; i < n; ++i) {
        auto member = node->getMember(i);
        TRI_document_collection_t const* myCollection = nullptr;

        AqlValue result = executeSimpleExpression(member, &myCollection, trx, docColls, argv, startPos, vars, regs);
        list->add(result.toJson(trx, myCollection));
        result.destroy();
      }

      return AqlValue(list);
    }
    catch (...) {
      delete list;
      throw;
    }
  }

  else if (node->type == NODE_TYPE_ARRAY) {
    if (node->isConstant()) {
      auto json = node->computeJson();

      if (json == nullptr) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
      }

      // we do not own the JSON but the node does!
      return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, json, Json::NOFREE));
    }

    size_t const n = node->numMembers();
    auto resultArray = new Json(Json::Array, n);

    try {
      for (size_t i = 0; i < n; ++i) {
        auto member = node->getMember(i);
        TRI_document_collection_t const* myCollection = nullptr;

        TRI_ASSERT(member->type == NODE_TYPE_ARRAY_ELEMENT);
        auto key = member->getStringValue();
        member = member->getMember(0);

        AqlValue result = executeSimpleExpression(member, &myCollection, trx, docColls, argv, startPos, vars, regs);
        resultArray->set(key, result.toJson(trx, myCollection));
        result.destroy();
      }
      return AqlValue(resultArray);
    }
    catch (...) {
      delete resultArray;
      throw;
    }
  }

  else if (node->type == NODE_TYPE_VALUE) {
    auto json = node->computeJson();

    if (json == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }

    // we do not own the JSON but the node does!
    return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, json, Json::NOFREE)); 
  }

  else if (node->type == NODE_TYPE_REFERENCE) {
    auto v = static_cast<Variable*>(node->getData());

    size_t i = 0;
    for (auto it = vars.begin(); it != vars.end(); ++it, ++i) {
      if ((*it)->name == v->name) {
        TRI_ASSERT(collection != nullptr);

        // save the collection info
        *collection = docColls[regs[i]]; 
        return argv[startPos + regs[i]].clone();
      }
    }
    // fall-through to exception
  }
  
  else if (node->type == NODE_TYPE_FCALL) {
    // some functions have C++ handlers
    // check if the called function has one
    auto func = static_cast<Function*>(node->getData());
    TRI_ASSERT(func->implementation != nullptr);

    TRI_document_collection_t const* myCollection = nullptr;
    auto member = node->getMember(0);
    AqlValue result = executeSimpleExpression(member, &myCollection, trx, docColls, argv, startPos, vars, regs);

    auto res2 = func->implementation(trx, myCollection, result);
    result.destroy();
    return res2;
  }

  else if (node->type == NODE_TYPE_RANGE) {
    TRI_document_collection_t const* leftCollection = nullptr;
    TRI_document_collection_t const* rightCollection = nullptr;

    auto low = node->getMember(0);
    auto high = node->getMember(1);
    AqlValue resultLow  = executeSimpleExpression(low, &leftCollection, trx, docColls, argv, startPos, vars, regs);
    AqlValue resultHigh = executeSimpleExpression(high, &rightCollection, trx, docColls, argv, startPos, vars, regs);
    AqlValue res = AqlValue(resultLow.toInt64(), resultHigh.toInt64());
    resultLow.destroy();
    resultHigh.destroy();

    return res;
  }
  
  else if (node->type == NODE_TYPE_OPERATOR_UNARY_NOT) {
    TRI_document_collection_t const* myCollection = nullptr;
    AqlValue operand = executeSimpleExpression(node->getMember(0), &myCollection, trx, docColls, argv, startPos, vars, regs);
    
    bool const operandIsTrue = operand.isTrue();
    operand.destroy();
    return AqlValue(new triagens::basics::Json(! operandIsTrue));
  }
  
  else if (node->type == NODE_TYPE_OPERATOR_BINARY_AND ||
           node->type == NODE_TYPE_OPERATOR_BINARY_OR) {
    TRI_document_collection_t const* leftCollection = nullptr;
    AqlValue left  = executeSimpleExpression(node->getMember(0), &leftCollection, trx, docColls, argv, startPos, vars, regs);
    TRI_document_collection_t const* rightCollection = nullptr;
    AqlValue right = executeSimpleExpression(node->getMember(1), &rightCollection, trx, docColls, argv, startPos, vars, regs);
    
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
    }
    else {
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
  
  else if (node->type == NODE_TYPE_OPERATOR_BINARY_EQ ||
           node->type == NODE_TYPE_OPERATOR_BINARY_NE ||
           node->type == NODE_TYPE_OPERATOR_BINARY_LT ||
           node->type == NODE_TYPE_OPERATOR_BINARY_LE ||
           node->type == NODE_TYPE_OPERATOR_BINARY_GT ||
           node->type == NODE_TYPE_OPERATOR_BINARY_GE ||
           node->type == NODE_TYPE_OPERATOR_BINARY_IN ||
           node->type == NODE_TYPE_OPERATOR_BINARY_NIN) {
    TRI_document_collection_t const* leftCollection = nullptr;
    AqlValue left  = executeSimpleExpression(node->getMember(0), &leftCollection, trx, docColls, argv, startPos, vars, regs);
    TRI_document_collection_t const* rightCollection = nullptr;
    AqlValue right = executeSimpleExpression(node->getMember(1), &rightCollection, trx, docColls, argv, startPos, vars, regs);

    if (node->type == NODE_TYPE_OPERATOR_BINARY_IN ||
        node->type == NODE_TYPE_OPERATOR_BINARY_NIN) {
      // IN and NOT IN
      if (! right.isList()) {
        // right operand must be a list, otherwise we return false
        left.destroy();
        right.destroy();
        // do not throw, but return "false" instead
        return AqlValue(new triagens::basics::Json(false));
      }
   
      bool result = findInList(left, right, leftCollection, rightCollection, trx, node); 

      if (node->type == NODE_TYPE_OPERATOR_BINARY_NIN) {
        // revert the result in case of a NOT IN
        result = ! result;
      }
       
      left.destroy();
      right.destroy();
    
      return AqlValue(new triagens::basics::Json(result));
    }

    // all other comparison operators
    int compareResult = AqlValue::Compare(trx, left, leftCollection, right, rightCollection);
    left.destroy();
    right.destroy();

    if (node->type == NODE_TYPE_OPERATOR_BINARY_EQ) {
      return AqlValue(new triagens::basics::Json(compareResult == 0));
    }
    else if (node->type == NODE_TYPE_OPERATOR_BINARY_NE) {
      return AqlValue(new triagens::basics::Json(compareResult != 0));
    }
    else if (node->type == NODE_TYPE_OPERATOR_BINARY_LT) {
    return AqlValue(new triagens::basics::Json(compareResult < 0));
    }
    else if (node->type == NODE_TYPE_OPERATOR_BINARY_LE) {
      return AqlValue(new triagens::basics::Json(compareResult <= 0));
    }
    else if (node->type == NODE_TYPE_OPERATOR_BINARY_GT) {
      return AqlValue(new triagens::basics::Json(compareResult > 0));
    }
    else if (node->type == NODE_TYPE_OPERATOR_BINARY_GE) {
      return AqlValue(new triagens::basics::Json(compareResult >= 0));
    }
    // fall-through intentional
  }
  
  else if (node->type == NODE_TYPE_OPERATOR_TERNARY) {
    TRI_document_collection_t const* myCollection = nullptr;
    AqlValue condition  = executeSimpleExpression(node->getMember(0), &myCollection, trx, docColls, argv, startPos, vars, regs);

    bool const isTrue = condition.isTrue();
    condition.destroy();
    if (isTrue) {
      // return true part
      return executeSimpleExpression(node->getMember(1), &myCollection, trx, docColls, argv, startPos, vars, regs);
    }
    
    // return false part  
    return executeSimpleExpression(node->getMember(2), &myCollection, trx, docColls, argv, startPos, vars, regs);
  }
 
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unhandled type in simple expression");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether this is an attribute access of any degree (e.g. a.b, 
/// a.b.c, ...)
////////////////////////////////////////////////////////////////////////////////

bool Expression::isAttributeAccess () const {
  auto expNode = _node;

  if (expNode->type != triagens::aql::NODE_TYPE_ATTRIBUTE_ACCESS) {
    return false;
  }

  while (expNode->type == triagens::aql::NODE_TYPE_ATTRIBUTE_ACCESS) {
    expNode = expNode->getMember(0);
  }
  
  return (expNode->type == triagens::aql::NODE_TYPE_REFERENCE);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether this is a reference access
////////////////////////////////////////////////////////////////////////////////

bool Expression::isReference () const {
  return (_node->type == triagens::aql::NODE_TYPE_REFERENCE);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief this gives you ("variable.access", "Reference")
/// call isSimpleAccessReference in advance to ensure no exceptions.
////////////////////////////////////////////////////////////////////////////////

std::pair<std::string, std::string> Expression::getMultipleAttributes() {
  if (! isSimple()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "getAccessNRef works only on simple expressions!");
  }

  auto expNode = _node;
  std::vector<std::string> attributeVector;

  if (expNode->type != triagens::aql::NODE_TYPE_ATTRIBUTE_ACCESS) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "getAccessNRef works only on simple expressions!");
  }

  while (expNode->type == triagens::aql::NODE_TYPE_ATTRIBUTE_ACCESS) {
    attributeVector.push_back(expNode->getStringValue());
    expNode = expNode->getMember(0);
  }
  
  std::string attributeVectorStr;
  for (auto oneAttr = attributeVector.rbegin();
       oneAttr != attributeVector.rend();
       ++oneAttr) {
    if (! attributeVectorStr.empty()) {
      attributeVectorStr += std::string(".");
    }
    attributeVectorStr += *oneAttr;
  }

  if (expNode->type != triagens::aql::NODE_TYPE_REFERENCE) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "getAccessNRef works only on simple expressions!");
  }

  auto variable = static_cast<Variable*>(expNode->getData());

  return std::make_pair(variable->name, attributeVectorStr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify an expression
/// note that currently stringification is only supported for certain node types
////////////////////////////////////////////////////////////////////////////////

void Expression::stringify (triagens::basics::StringBuffer* buffer) const {
  _node->stringify(buffer, true);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

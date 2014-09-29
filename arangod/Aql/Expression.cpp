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
#include "Aql/Types.h"
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
    _isDeterministic(false) {

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
  if (_type == V8) {
    delete _func;
  }
  else if (_type == JSON) {
    TRI_ASSERT(_data != nullptr);
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, _data);
    // _json is freed automatically by AqlItemBlock
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

AqlValue Expression::execute (AQL_TRANSACTION_V8* trx,
                              std::vector<TRI_document_collection_t const*>& docColls,
                              std::vector<AqlValue>& argv,
                              size_t startPos,
                              std::vector<Variable*> const& vars,
                              std::vector<RegisterId> const& regs) {
  if (_type == UNPROCESSED) {
    analyzeExpression();
  }

  TRI_ASSERT(_type != UNPROCESSED);

  // and execute
  switch (_type) {
    case JSON: {
      TRI_ASSERT(_data != nullptr);
      return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, _data, Json::NOFREE));
    }

    case V8: {
      TRI_ASSERT(_func != nullptr);
      try {
        return _func->execute(trx, docColls, argv, startPos, vars, regs);
      }
      catch (triagens::arango::Exception& ex) {
        ex.addToMessage("\nwhile evaluating expression");
        ex.addToMessage(_node->toInfoString(TRI_UNKNOWN_MEM_ZONE));
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
    
  if (_type == V8) {
    delete _func;
    _type = UNPROCESSED;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief analyze the expression (and, if appropriate, compile it into 
/// executable code)
////////////////////////////////////////////////////////////////////////////////

void Expression::analyzeExpression () {
  TRI_ASSERT(_type == UNPROCESSED);

  if (_node->isConstant()) {
    // generate a constant value
    _data = _node->toJsonValue(TRI_UNKNOWN_MEM_ZONE);

    if (_data == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid json in simple expression");
    }

    _type = JSON;
    _canThrow = false;
    _isDeterministic = true;
  }
  else if (_node->isSimple()) {
    _type = SIMPLE;
    _canThrow = _node->canThrow();
    _isDeterministic = _node->isDeterministic();
  }
  else {
    // generate a V8 expression
    _func = _executor->generateExpression(_node);
    _type = V8;
    _canThrow = _node->canThrow();
    _isDeterministic = _node->isDeterministic();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute an expression of type SIMPLE, the convention is that
/// the resulting AqlValue will be destroyed outside eventually
////////////////////////////////////////////////////////////////////////////////

AqlValue Expression::executeSimpleExpression (AstNode const* node,
                                              TRI_document_collection_t const** collection, 
                                              AQL_TRANSACTION_V8* trx,
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
        auto j = result.extractListMember(trx, myCollection, index->getIntValue());
        result.destroy();
        return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, j.steal()));
      }
      else if (index->isStringValue()) {
        char const* p = index->getStringValue();
        TRI_ASSERT(p != nullptr);

        try {
          // stoll() might throw an exception if the string is not a number
          int64_t position = static_cast<int64_t>(std::stoll(p));
          auto j = result.extractListMember(trx, myCollection, position);
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
  
  else if (node->type == NODE_TYPE_VALUE) {
    auto j = node->toJsonValue(TRI_UNKNOWN_MEM_ZONE);

    if (j == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }

    return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, j)); 
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
    AqlValue resultLow = executeSimpleExpression(low, &leftCollection, trx, docColls, argv, startPos, vars, regs);
    AqlValue resultHigh = executeSimpleExpression(high, &rightCollection, trx, docColls, argv, startPos, vars, regs);

    if (! resultLow.isNumber() || ! resultHigh.isNumber()) {
      resultLow.destroy();
      resultHigh.destroy();
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid data type for range");
    }
    
    AqlValue res = AqlValue(resultLow.toNumber<int64_t>(), resultHigh.toNumber<int64_t>());

    resultLow.destroy();
    resultHigh.destroy();

    return res;
  }
  
  else if (node->type == NODE_TYPE_OPERATOR_UNARY_NOT) {
    TRI_document_collection_t const* myCollection = nullptr;
    AqlValue operand = executeSimpleExpression(node->getMember(0), &myCollection, trx, docColls, argv, startPos, vars, regs);
    
    bool operandIsBoolean = operand.isBoolean();
    bool operandIsTrue    = operand.isTrue();
      
    operand.destroy();

    if (! operandIsBoolean) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_INVALID_LOGICAL_VALUE);
    }

    return AqlValue(new triagens::basics::Json(! operandIsTrue));
  }
  
  else if (node->type == NODE_TYPE_OPERATOR_BINARY_AND ||
           node->type == NODE_TYPE_OPERATOR_BINARY_OR) {
    TRI_document_collection_t const* leftCollection = nullptr;
    AqlValue left  = executeSimpleExpression(node->getMember(0), &leftCollection, trx, docColls, argv, startPos, vars, regs);
    TRI_document_collection_t const* rightCollection = nullptr;
    AqlValue right = executeSimpleExpression(node->getMember(1), &rightCollection, trx, docColls, argv, startPos, vars, regs);
    
    bool leftIsBoolean  = left.isBoolean();
    bool rightIsBoolean = right.isBoolean();
    bool leftIsTrue     = left.isTrue();
    bool rightIsTrue    = right.isTrue();
    left.destroy();
    right.destroy();

    if (! leftIsBoolean) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_INVALID_LOGICAL_VALUE);
    }

    // left is a boolean

    if (node->type == NODE_TYPE_OPERATOR_BINARY_AND) {
      // AND
      if (leftIsTrue) {
        if (! rightIsBoolean) {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_INVALID_LOGICAL_VALUE);
        }
          
        return AqlValue(new triagens::basics::Json(rightIsTrue));
      }
      
      return AqlValue(new triagens::basics::Json(false));
    }
    else {
      // OR
      if (! leftIsTrue) {
        if (! rightIsBoolean) {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_INVALID_LOGICAL_VALUE);
        }
        
        return AqlValue(new triagens::basics::Json(rightIsTrue));
      }
      // fall-through intentional
    
      return AqlValue(new triagens::basics::Json(true));
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
        // right operand must be a list
        left.destroy();
        right.destroy();
        THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_LIST_EXPECTED);
      }
    
      size_t const n = right.listSize();

      for (size_t i = 0; i < n; ++i) {
        auto listItem = right.extractListMember(trx, rightCollection, i);
        AqlValue listItemValue(&listItem);

        int compareResult = AqlValue::Compare(trx, left, leftCollection, listItemValue, nullptr);

        if (compareResult == 0) {
          // item found in the list
          left.destroy();
          right.destroy();
      
          // found
          return AqlValue(new triagens::basics::Json(node->type == NODE_TYPE_OPERATOR_BINARY_IN));
        }
      }
       
      left.destroy();
      right.destroy();
    
      // not found
      return AqlValue(new triagens::basics::Json(node->type != NODE_TYPE_OPERATOR_BINARY_IN));
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
  _node->append(buffer);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

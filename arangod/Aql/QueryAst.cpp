////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, query AST
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

#include "Aql/QueryAst.h"
#include "Aql/Parser.h"
#include "BasicsC/tri-strings.h"
#include "Utils/Exception.h"
#include "VocBase/collection.h"

using namespace triagens::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the AST
////////////////////////////////////////////////////////////////////////////////

QueryAst::QueryAst (Parser* parser) 
  : _parser(parser),
    _nodes(),
    _strings(),
    _scopes(),
    _bindParameters(),
    _collectionNames(),
    _root(nullptr),
    _writeCollection(nullptr),
    _writeOptions(nullptr),
    _nopNode() {

  _nodes.reserve(32);
  _root = createNode(NODE_TYPE_ROOT);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the AST
////////////////////////////////////////////////////////////////////////////////

QueryAst::~QueryAst () {
  // free strings
  for (auto it = _strings.begin(); it != _strings.end(); ++it) {
    TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, const_cast<char*>(*it));
  }

  // free nodes
  for (auto it = _nodes.begin(); it != _nodes.end(); ++it) {
    delete (*it);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief convert the AST into JSON
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* QueryAst::toJson (TRI_memory_zone_t* zone) {
  TRI_json_t* json = TRI_CreateListJson(zone);

  try {
    _root->toJson(json, zone);
  }
  catch (...) {
    TRI_FreeJson(zone, json);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the AST
////////////////////////////////////////////////////////////////////////////////

void QueryAst::addOperation (AstNode* node) {
  TRI_ASSERT(_root != nullptr);

  _root->addMember(node);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register a string
////////////////////////////////////////////////////////////////////////////////

char* QueryAst::registerString (char const* p, 
                                size_t length,
                                bool mustUnescape) {

  if (p == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  if (length == 0) {
    static char const* empty = "";
    // optimisation for the empty string
    return const_cast<char*>(empty);
  }

  char* copy = nullptr;
  if (mustUnescape) {
    size_t outLength;
    copy = TRI_UnescapeUtf8StringZ(TRI_UNKNOWN_MEM_ZONE, p, length, &outLength);
  }
  else {
    copy = TRI_DuplicateString2Z(TRI_UNKNOWN_MEM_ZONE, p, length);
  }

  if (copy == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  try {
    _strings.push_back(copy);
  }
  catch (...) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  return copy;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST for node
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::createNodeFor (char const* variableName,
                                  AstNode const* expression) {
  AstNode* node = createNode(NODE_TYPE_FOR);

  AstNode* variable = createNodeVariable(variableName, true);
  node->addMember(variable);
  node->addMember(expression);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST let node
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::createNodeLet (char const* variableName,
                                  AstNode const* expression) {
  AstNode* node = createNode(NODE_TYPE_LET);

  AstNode* variable = createNodeVariable(variableName, true);
  node->addMember(variable);
  node->addMember(expression);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST filter node
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::createNodeFilter (AstNode const* expression) {
  AstNode* node = createNode(NODE_TYPE_FILTER);
  node->addMember(expression);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST return node
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::createNodeReturn (AstNode const* expression) {
  AstNode* node = createNode(NODE_TYPE_RETURN);
  node->addMember(expression);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST remove node
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::createNodeRemove (AstNode const* expression,
                                     AstNode const* collection,
                                     AstNode* options) {
  AstNode* node = createNode(NODE_TYPE_REMOVE);
  node->addMember(expression);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST insert node
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::createNodeInsert (AstNode const* expression,
                                     AstNode const* collection,
                                     AstNode* options) {
  AstNode* node = createNode(NODE_TYPE_INSERT);
  node->addMember(expression);
  
  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST update node
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::createNodeUpdate (AstNode const* keyExpression,
                                     AstNode const* docExpression,
                                     AstNode const* collection,
                                     AstNode* options) {
  AstNode* node = createNode(NODE_TYPE_UPDATE);
  node->addMember(docExpression);

  if (keyExpression != nullptr) {
    node->addMember(keyExpression);
  }

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST replace node
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::createNodeReplace (AstNode const* keyExpression,
                                      AstNode const* docExpression,
                                      AstNode const* collection,
                                      AstNode* options) {
  AstNode* node = createNode(NODE_TYPE_REPLACE);
  node->addMember(docExpression);

  if (keyExpression != nullptr) {
    node->addMember(keyExpression);
  }

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST collect node
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::createNodeCollect (AstNode const* list,
                                      char const* name) {
  AstNode* node = createNode(NODE_TYPE_COLLECT);
  node->addMember(list);

  if (name != nullptr) {
    AstNode* variable = createNodeVariable(name, true);
    node->addMember(variable);
  }

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST sort node
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::createNodeSort (AstNode const* list) {
  AstNode* node = createNode(NODE_TYPE_SORT);
  node->addMember(list);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST sort element node
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::createNodeSortElement (AstNode const* expression,
                                          bool ascending) {
  AstNode* node = createNode(NODE_TYPE_SORT_ELEMENT);
  node->addMember(expression);
  node->setBoolValue(ascending);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST limit node
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::createNodeLimit (AstNode const* offset,
                                    AstNode const* count) {
  AstNode* node = createNode(NODE_TYPE_LIMIT);
  node->addMember(offset);
  node->addMember(count);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST assign node, used in COLLECT statements
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::createNodeAssign (char const* name,
                                     AstNode const* expression) {
  AstNode* node = createNode(NODE_TYPE_ASSIGN);
  AstNode* variable = createNodeVariable(name, true);
  node->addMember(variable);
  node->addMember(expression);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST variable node
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::createNodeVariable (char const* name, 
                                       bool isUserDefined) {
  if (name == nullptr || *name == '\0') {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  if (isUserDefined && *name == '_') {
    _parser->registerError(TRI_ERROR_QUERY_VARIABLE_NAME_INVALID);
    return nullptr;
  }

  if (_scopes.existsVariable(name)) {
    _parser->registerError(TRI_ERROR_QUERY_VARIABLE_REDECLARED, name);
    return nullptr;
  }
  
  AstNode* node = createNode(NODE_TYPE_VARIABLE);
  node->setStringValue(name);

  auto variable = _scopes.addVariable(name, isUserDefined);
  node->setData(static_cast<void*>(variable));

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST collection node
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::createNodeCollection (char const* name) {
  if (name == nullptr || *name == '\0') {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  if (! TRI_IsAllowedNameCollection(true, name)) {
    _parser->registerError(TRI_ERROR_ARANGO_ILLEGAL_NAME, name);
    return nullptr;
  }

  AstNode* node = createNode(NODE_TYPE_COLLECTION);
  node->setStringValue(name);

  _collectionNames.insert(std::string(name));

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST reference node
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::createNodeReference (char const* name) {
  AstNode* node = createNode(NODE_TYPE_REFERENCE);
  node->setStringValue(name);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST parameter node
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::createNodeParameter (char const* name) {
  AstNode* node = createNode(NODE_TYPE_PARAMETER);
 
  node->setStringValue(name);

  // insert bind parameter name into list of found parameters
  _bindParameters.insert(std::string(name));

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST unary operator node
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::createNodeUnaryOperator (AstNodeType type,
                                           AstNode const* operand) {
  AstNode* node = createNode(type);
  node->addMember(operand);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST binary operator node
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::createNodeBinaryOperator (AstNodeType type,
                                             AstNode const* lhs,
                                             AstNode const* rhs) {
  AstNode* node = createNode(type);
  node->addMember(lhs);
  node->addMember(rhs);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST ternary operator node
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::createNodeTernaryOperator (AstNode const* condition,
                                              AstNode const* truePart,
                                              AstNode const* falsePart) {
  AstNode* node = createNode(NODE_TYPE_OPERATOR_TERNARY);
  node->addMember(condition);
  node->addMember(truePart);
  node->addMember(falsePart);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST subquery node
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::createNodeSubquery (char const* tempName) {
  AstNode* node = createNode(NODE_TYPE_SUBQUERY);
  AstNode* variable = createNodeVariable(tempName, false);
  node->addMember(variable);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST attribute access node
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::createNodeAttributeAccess (AstNode const* accessed,
                                              char const* attributeName) {
  AstNode* node = createNode(NODE_TYPE_ATTRIBUTE_ACCESS);
  node->addMember(accessed);
  node->setStringValue(attributeName);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST attribute access node w/ bind parameter
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::createNodeBoundAttributeAccess (AstNode const* accessed,
                                                   AstNode const* parameter) {
  AstNode* node = createNode(NODE_TYPE_BOUND_ATTRIBUTE_ACCESS);
  node->addMember(accessed);
  node->addMember(parameter);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST indexed access node
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::createNodeIndexedAccess (AstNode const* accessed,
                                            AstNode const* indexValue) {
  AstNode* node = createNode(NODE_TYPE_INDEXED_ACCESS);
  node->addMember(accessed);
  node->addMember(indexValue);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST expand node
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::createNodeExpand (char const* variableName,
                                     char const* tempName,
                                     AstNode const* expanded,
                                     AstNode const* expansion) {
  AstNode* node = createNode(NODE_TYPE_EXPAND);
  AstNode* variable = createNodeVariable(variableName, false);
  AstNode* temp = createNodeVariable(tempName, false);
  node->addMember(variable);
  node->addMember(temp);
  node->addMember(expanded);
  node->addMember(expansion);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST null value node
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::createNodeValueNull () {
  AstNode* node = createNode(NODE_TYPE_VALUE);
  node->setValueType(VALUE_TYPE_NULL); 

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST bool value node
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::createNodeValueBool (bool value) {
  AstNode* node = createNode(NODE_TYPE_VALUE);
  node->setValueType(VALUE_TYPE_BOOL);
  node->setBoolValue(value);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST int value node
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::createNodeValueInt (int64_t value) {
  AstNode* node = createNode(NODE_TYPE_VALUE);
  node->setValueType(VALUE_TYPE_INT);
  node->setIntValue(value);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST double value node
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::createNodeValueDouble (double value) {
  AstNode* node = createNode(NODE_TYPE_VALUE);
  node->setValueType(VALUE_TYPE_DOUBLE);
  node->setDoubleValue(value);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST string value node
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::createNodeValueString (char const* value) {
  AstNode* node = createNode(NODE_TYPE_VALUE);
  node->setValueType(VALUE_TYPE_STRING);
  node->setStringValue(value);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST list node
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::createNodeList () {
  AstNode* node = createNode(NODE_TYPE_LIST);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST array node
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::createNodeArray () {
  AstNode* node = createNode(NODE_TYPE_ARRAY);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST array element node
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::createNodeArrayElement (char const* attributeName,
                                           AstNode const* expression) {
  AstNode* node = createNode(NODE_TYPE_ARRAY_ELEMENT);
  node->setStringValue(attributeName);
  node->addMember(expression);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST function call node
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::createNodeFunctionCall (char const* functionName,
                                           AstNode const* parameters) {

  std::string const normalizedName = normalizeFunctionName(functionName);
  char* fname = registerString(normalizedName.c_str(), normalizedName.size(), false);

  AstNode* node;

  if (normalizedName[0] == '_') {
    // built-in function
    node = createNode(NODE_TYPE_FCALL);
  }
  else {
    // user-defined function
    node = createNode(NODE_TYPE_FCALL_USER);
  }

  node->setStringValue(fname);
  node->addMember(parameters);
  
  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST range node
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::createNodeRange (AstNode const* start,
                                    AstNode const* end) {
  AstNode* node = createNode(NODE_TYPE_RANGE);
  node->addMember(start);
  node->addMember(end);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a no-op node
/// note: the same no-op node can be returned multiple times
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::createNodeNop () {
  if (_nopNode == nullptr) {
    _nopNode = createNode(NODE_TYPE_NOP);
  }

  return _nopNode;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief injects bind parameters into the AST
////////////////////////////////////////////////////////////////////////////////

void QueryAst::injectBindParameters (BindParameters& parameters) {
  auto p = parameters();

  if (p.empty()) {
    return;
  }

  auto func = [&](AstNode* node, void* data) -> AstNode* {
    if (node->type == NODE_TYPE_PARAMETER) {
      char const* param = node->getStringValue();

      if (param == nullptr) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
      }
        
      auto it = p.find(std::string(param));

      if (it == p.end()) {
        _parser->registerError(TRI_ERROR_QUERY_BIND_PARAMETER_MISSING, param);
        return nullptr;
      }

      auto value = (*it).second;

      if (*param == '@') {
        // collection parameter
        TRI_ASSERT(TRI_IsStringJson(value));

        bool isWriteCollection = false;
        if (_writeCollection != nullptr && 
            _writeCollection->type == NODE_TYPE_PARAMETER &&
            strcmp(param, _writeCollection->getStringValue()) == 0) {
          isWriteCollection = true;
        }

        // turn node into a collection node
        char const* name = registerString(value->_value._string.data, value->_value._string.length - 1, false);

        node = createNodeCollection(name);

        if (isWriteCollection) {
          // this was the bind parameter that contained the collection to update 
          _writeCollection = node;
        }
      }
      else {
        node = nodeFromJson((*it).second);
      }
    }

    return node;
  };

  _root = traverse(_root, func, &p); 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimizes the AST
////////////////////////////////////////////////////////////////////////////////

void QueryAst::optimize () {
  auto func = [&](AstNode* node, void* data) -> AstNode* {
    if (node == nullptr) {
      return nullptr;
    }

    // FILTER
    if (node->type == NODE_TYPE_FILTER) {
      return optimizeFilter(node);
    }

    // unary operators
    if (node->type == NODE_TYPE_OPERATOR_UNARY_PLUS ||
        node->type == NODE_TYPE_OPERATOR_UNARY_MINUS) {
      return optimizeUnaryOperatorArithmetic(node);
    }

    if (node->type == NODE_TYPE_OPERATOR_UNARY_NOT) {
      return optimizeUnaryOperatorLogical(node);
    }

    // binary operators
    if (node->type == NODE_TYPE_OPERATOR_BINARY_AND ||
        node->type == NODE_TYPE_OPERATOR_BINARY_OR) {
      return optimizeBinaryOperatorLogical(node);
    }
    
    if (node->type == NODE_TYPE_OPERATOR_BINARY_EQ ||
        node->type == NODE_TYPE_OPERATOR_BINARY_NE ||
        node->type == NODE_TYPE_OPERATOR_BINARY_LT ||
        node->type == NODE_TYPE_OPERATOR_BINARY_LE ||
        node->type == NODE_TYPE_OPERATOR_BINARY_GT ||
        node->type == NODE_TYPE_OPERATOR_BINARY_GE ||
        node->type == NODE_TYPE_OPERATOR_BINARY_IN) {
      return optimizeBinaryOperatorRelational(node);
    }
    
    if (node->type == NODE_TYPE_OPERATOR_BINARY_PLUS ||
        node->type == NODE_TYPE_OPERATOR_BINARY_MINUS ||
        node->type == NODE_TYPE_OPERATOR_BINARY_TIMES ||
        node->type == NODE_TYPE_OPERATOR_BINARY_DIV ||
        node->type == NODE_TYPE_OPERATOR_BINARY_MOD) {
      return optimizeBinaryOperatorArithmetic(node);
    }
      
    // ternary operator
    if (node->type == NODE_TYPE_OPERATOR_TERNARY) {
      return optimizeTernaryOperator(node);
    }

    return node;
  };

  _root = traverse(_root, func, nullptr); 
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief optimizes a FILTER node
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::optimizeFilter (AstNode* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->type == NODE_TYPE_FILTER);
  TRI_ASSERT(node->numMembers() == 1);
  
  AstNode* operand = node->getMember(0);
  if (! operand->isConstant()) {
    // unable to optimize non-constant expression
    return node;
  }

  if (! operand->isBoolValue()) {
    _parser->registerError(TRI_ERROR_QUERY_INVALID_LOGICAL_VALUE);
    return node;
  }

  if (operand->getBoolValue()) {
    // FILTER is always true, optimise it away
    return createNodeNop();
  }

  return node;
}  

////////////////////////////////////////////////////////////////////////////////
/// @brief optimizes the unary operators + and -
/// the unary plus will be converted into a simple value node if the operand of
/// the operation is a constant number
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::optimizeUnaryOperatorArithmetic (AstNode* node) {
  TRI_ASSERT(node != nullptr);

  TRI_ASSERT(node->type == NODE_TYPE_OPERATOR_UNARY_PLUS ||
             node->type == NODE_TYPE_OPERATOR_UNARY_MINUS);
  TRI_ASSERT(node->numMembers() == 1);

  AstNode* operand = node->getMember(0);
  if (! operand->isConstant()) {
    // operand is dynamic, cannot statically optimize it
    return node;
  }

  if (! operand->isNumericValue()) {
    _parser->registerError(TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
    return node;
  }

  TRI_ASSERT(operand->value.type == VALUE_TYPE_INT ||
             operand->value.type == VALUE_TYPE_DOUBLE);

  if (node->type == NODE_TYPE_OPERATOR_UNARY_PLUS) {
    // + number => number
    return operand;
  }
  else {
    // - number
    if (operand->value.type == VALUE_TYPE_INT) {
      // int64
      return createNodeValueInt(- operand->getIntValue());
    }
    else {
      // double
      double const value = - operand->getDoubleValue();
      
      if (value != value || 
          value == HUGE_VAL || 
          value == - HUGE_VAL) {
        // IEEE754 NaN values have an interesting property that we can exploit...
        // if the architecture does not use IEEE754 values then this shouldn't do
        // any harm either
        _parser->registerError(TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE);
        return node;
      }

      return createNodeValueDouble(value);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimizes the unary operator NOT
/// the unary NOT operation will be replaced with the result of the operation
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::optimizeUnaryOperatorLogical (AstNode* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->type == NODE_TYPE_OPERATOR_UNARY_NOT);
  TRI_ASSERT(node->numMembers() == 1);

  AstNode* operand = node->getMember(0);
  if (! operand->isConstant()) {
    // operand is dynamic, cannot statically optimize it
    return node;
  }

  if (! operand->isBoolValue()) {
    _parser->registerError(TRI_ERROR_QUERY_INVALID_LOGICAL_VALUE);
    return node;
  }

  // replace unary negation operation with result of negation
  return createNodeValueBool(! operand->getBoolValue());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimizes the binary logical operators && and ||
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::optimizeBinaryOperatorLogical (AstNode* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->type == NODE_TYPE_OPERATOR_BINARY_AND ||
             node->type == NODE_TYPE_OPERATOR_BINARY_OR);
  TRI_ASSERT(node->numMembers() == 2);

  auto lhs = node->getMember(0);
  auto rhs = node->getMember(1);

  if (lhs == nullptr || rhs == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  bool const lhsIsConst = lhs->isConstant();
  bool const rhsIsConst = rhs->isConstant();

  if (lhsIsConst && ! lhs->isBoolValue()) {
    // left operand is a constant value, but no boolean
    _parser->registerError(TRI_ERROR_QUERY_INVALID_LOGICAL_VALUE);
    return node;
  }

  if (rhsIsConst && ! rhs->isBoolValue()) {
    // right operand is a constant value, but no boolean
    _parser->registerError(TRI_ERROR_QUERY_INVALID_LOGICAL_VALUE);
    return node;
  }

  if (! lhsIsConst || ! rhsIsConst) {
    return node;
  }

  if (node->type == NODE_TYPE_OPERATOR_BINARY_AND) {
    // logical and

    if (lhs->getBoolValue()) {
      // (true && rhs) => rhs
      return rhs;
    }

    // (false && rhs) => false
    return lhs;
  }
  else {
    // logical or

    if (lhs->getBoolValue()) {
      // (true || rhs) => true
      return lhs;
    }

    // (false || rhs) => rhs
    return rhs;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimizes the binary relational operators <, <=, >, >=, ==, != and IN
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::optimizeBinaryOperatorRelational (AstNode* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 2);

  AstNode* lhs = node->getMember(0);
  AstNode* rhs = node->getMember(1);

  if (lhs == nullptr || rhs == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
 /* 
  bool const lhsIsConst = lhs->isConstant(); 
  bool const rhsIsConst = rhs->isConstant(); 

  // TODO
*/
  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimizes the binary arithmetic operators +, -, *, / and %
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::optimizeBinaryOperatorArithmetic (AstNode* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 2);

  AstNode* lhs = node->getMember(0);
  AstNode* rhs = node->getMember(1);

  if (lhs == nullptr || rhs == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  bool const lhsIsConst = lhs->isConstant(); 
  bool const rhsIsConst = rhs->isConstant(); 

  if (lhsIsConst && ! lhs->isNumericValue()) {
    // lhs is not a number
    _parser->registerError(TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
    return node;
  }
  
  if (rhsIsConst && ! rhs->isNumericValue()) {
    // rhs is not a number
    _parser->registerError(TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
    return node;
  }

  if (! lhsIsConst || ! rhsIsConst) {
    return node;
  }

  // now calculate the expression result

  double value;
  double const l = lhs->getDoubleValue();
  double const r = rhs->getDoubleValue();

  if (node->type == NODE_TYPE_OPERATOR_BINARY_PLUS) {
    value = l + r;
  }
  else if (node->type == NODE_TYPE_OPERATOR_BINARY_MINUS) {
    value = l - r;
  }
  else if (node->type == NODE_TYPE_OPERATOR_BINARY_TIMES) {
    value = l * r;
  }
  else if (node->type == NODE_TYPE_OPERATOR_BINARY_DIV) {
    if (r == 0.0) {
      _parser->registerError(TRI_ERROR_QUERY_DIVISION_BY_ZERO);
      return node;
    }

    value = l / r;
  }
  else if (node->type == NODE_TYPE_OPERATOR_BINARY_MOD) {
    if (r == 0.0) {
      _parser->registerError(TRI_ERROR_QUERY_DIVISION_BY_ZERO);
      return node;
    }

    value = fmod(l, r);
  }
  else {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }
      
  if (value != value || 
      value == HUGE_VAL || 
      value == - HUGE_VAL) {
    // IEEE754 NaN values have an interesting property that we can exploit...
    // if the architecture does not use IEEE754 values then this shouldn't do
    // any harm either
    _parser->registerError(TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE);
    return node;
  }

  return createNodeValueDouble(value);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimizes the ternary operator
/// if the condition is constant, the operator will be replaced with either the
/// true part or the false part
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::optimizeTernaryOperator (AstNode* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->type == NODE_TYPE_OPERATOR_TERNARY);
  TRI_ASSERT(node->numMembers() == 3);

  AstNode* condition = node->getMember(0);
  AstNode* truePart  = node->getMember(1);
  AstNode* falsePart = node->getMember(2);

  if (condition == nullptr || 
      truePart == nullptr || 
      falsePart == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  if (! condition->isConstant()) {
    return node;
  }

  if (! condition->isBoolValue()) {
    _parser->registerError(TRI_ERROR_QUERY_INVALID_LOGICAL_VALUE);
    return node;
  }

  if (condition->getBoolValue()) {
    // condition is always true, replace ternary operation with true part
    return truePart;
  }

  // condition is always false, replace ternary operation with false part
  return falsePart;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST node from JSON
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::nodeFromJson (TRI_json_t const* json) {
  TRI_ASSERT(json != nullptr);

  if (json->_type == TRI_JSON_BOOLEAN) {
    return createNodeValueBool(json->_value._boolean);
  }

  if (json->_type == TRI_JSON_NUMBER) {
    return createNodeValueDouble(json->_value._number);
  }

  if (json->_type == TRI_JSON_STRING) {
    char const* value = registerString(json->_value._string.data, json->_value._string.length - 1, false);
    return createNodeValueString(value);
  }

  if (json->_type == TRI_JSON_LIST) {
    auto node = createNodeList();
    size_t const n = json->_value._objects._length;

    for (size_t i = 0; i < n; ++i) {
      node->addMember(nodeFromJson(static_cast<TRI_json_t const*>(TRI_AtVector(&json->_value._objects, i)))); 
    }

    return node;
  }

  if (json->_type == TRI_JSON_ARRAY) {
    auto node = createNodeArray();
    size_t const n = json->_value._objects._length;

    for (size_t i = 0; i < n; i += 2) {
      TRI_json_t const* key = static_cast<TRI_json_t const*>(TRI_AtVector(&json->_value._objects, i));
      TRI_json_t const* value = static_cast<TRI_json_t const*>(TRI_AtVector(&json->_value._objects, i + 1));

      if (! TRI_IsStringJson(key) || value == nullptr) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
      }

      char const* attributeName = registerString(key->_value._string.data, key->_value._string.length - 1, false);
      
      node->addMember(createNodeArrayElement(attributeName, nodeFromJson(value)));
    }

    return node;
  }
  
  return createNodeValueNull();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief traverse the AST
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::traverse (AstNode* node, 
                             std::function<AstNode*(AstNode*, void*)> func,
                             void* data) {
  if (node == nullptr) {
    return nullptr;
  }
  
  // dump sub-nodes 
  size_t const n = node->numMembers();

  for (size_t i = 0; i < n; ++i) {
    auto member = node->getMember(i);

    if (member != nullptr) {
      AstNode* result = traverse(member, func, data);

      if (result != node) {
        node->changeMember(i, result);
      }
    }
  }

  return func(node, data);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief normalize a function name
////////////////////////////////////////////////////////////////////////////////

std::string QueryAst::normalizeFunctionName (char const* name) {
  if (name == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  char* upperName = TRI_UpperAsciiStringZ(TRI_UNKNOWN_MEM_ZONE, name);
  if (upperName == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  std::string functionName(upperName);

  TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, upperName);

  if (functionName.find(':') == std::string::npos) {
    // prepend default namespace for internal functions
    functionName = "_AQL:" + functionName;
  }

  return functionName;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a node of the specified type
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::createNode (AstNodeType type) {
  auto node = new AstNode(type);

  try {
    _nodes.push_back(node);
  }
  catch (...) {
    delete node;
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  return node;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

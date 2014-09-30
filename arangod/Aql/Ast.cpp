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

#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/Executor.h"
#include "Basics/tri-strings.h"
#include "Utils/Exception.h"
#include "VocBase/collection.h"

using namespace triagens::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                             static initialisation
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise a singleton NOP node instance
////////////////////////////////////////////////////////////////////////////////

AstNode const Ast::NopNode = { NODE_TYPE_NOP }; 

////////////////////////////////////////////////////////////////////////////////
/// @brief inverse comparison operators
////////////////////////////////////////////////////////////////////////////////

std::unordered_map<int, AstNodeType> const Ast::ReverseOperators{ 
  { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_EQ), NODE_TYPE_OPERATOR_BINARY_NE },
  { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_NE), NODE_TYPE_OPERATOR_BINARY_EQ },
  { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_GT), NODE_TYPE_OPERATOR_BINARY_LE },
  { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_GE), NODE_TYPE_OPERATOR_BINARY_LT },
  { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_LT), NODE_TYPE_OPERATOR_BINARY_GE },
  { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_LE), NODE_TYPE_OPERATOR_BINARY_GT },
  { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_IN), NODE_TYPE_OPERATOR_BINARY_NIN },
  { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_NIN), NODE_TYPE_OPERATOR_BINARY_IN }
};

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the AST
////////////////////////////////////////////////////////////////////////////////

Ast::Ast (Query* query)
  : _query(query),
    _scopes(),
    _variables(),
    _bindParameters(),
    _root(nullptr),
    _queries(),
    _writeCollection(nullptr) {

  TRI_ASSERT(_query != nullptr);

  startSubQuery();

  TRI_ASSERT(_root != nullptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the AST
////////////////////////////////////////////////////////////////////////////////

Ast::~Ast () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief convert the AST into JSON
/// the caller is responsible for freeing the JSON later
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* Ast::toJson (TRI_memory_zone_t* zone,
                         bool verbose) const {
  TRI_json_t* json = TRI_CreateListJson(zone);

  try {
    _root->toJson(json, zone, verbose);
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

void Ast::addOperation (AstNode* node) {
  TRI_ASSERT(_root != nullptr);

  _root->addMember(node);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST for node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeFor (char const* variableName,
                             AstNode const* expression) {
  if (variableName == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  AstNode* node = createNode(NODE_TYPE_FOR);

  AstNode* variable = createNodeVariable(variableName, true);
  node->addMember(variable);
  node->addMember(expression);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST let node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeLet (char const* variableName,
                             AstNode const* expression,
                             bool isUserDefinedVariable) {
  if (variableName == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  AstNode* node = createNode(NODE_TYPE_LET);

  AstNode* variable = createNodeVariable(variableName, isUserDefinedVariable);
  node->addMember(variable);
  node->addMember(expression);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST filter node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeFilter (AstNode const* expression) {
  AstNode* node = createNode(NODE_TYPE_FILTER);
  node->addMember(expression);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST return node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeReturn (AstNode const* expression) {
  AstNode* node = createNode(NODE_TYPE_RETURN);
  node->addMember(expression);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST remove node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeRemove (AstNode const* expression,
                                AstNode const* collection,
                                AstNode const* options) {
  AstNode* node = createNode(NODE_TYPE_REMOVE);

  if (options == nullptr) {
    // no options given. now use default options
    options = &NopNode;
  }

  node->addMember(options);
  node->addMember(collection);
  node->addMember(expression);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST insert node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeInsert (AstNode const* expression,
                                AstNode const* collection,
                                AstNode const* options) {
  
  AstNode* node = createNode(NODE_TYPE_INSERT);
  
  if (options == nullptr) {
    // no options given. now use default options
    options = &NopNode;
  }

  node->addMember(options);
  node->addMember(collection);
  node->addMember(expression);
  
  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST update node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeUpdate (AstNode const* keyExpression,
                                AstNode const* docExpression,
                                AstNode const* collection,
                                AstNode const* options) {
  AstNode* node = createNode(NODE_TYPE_UPDATE);
  
  if (options == nullptr) {
    // no options given. now use default options
    options = &NopNode;
  }

  node->addMember(options);
  node->addMember(collection);
  node->addMember(docExpression);

  if (keyExpression != nullptr) {
    node->addMember(keyExpression);
  }

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST replace node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeReplace (AstNode const* keyExpression,
                                 AstNode const* docExpression,
                                 AstNode const* collection,
                                 AstNode const* options) {
  AstNode* node = createNode(NODE_TYPE_REPLACE);
  
  if (options == nullptr) {
    // no options given. now use default options
    options = &NopNode;
  }

  node->addMember(options);
  node->addMember(collection);
  node->addMember(docExpression);

  if (keyExpression != nullptr) {
    node->addMember(keyExpression);
  }

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST collect node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeCollect (AstNode const* list,
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

AstNode* Ast::createNodeSort (AstNode const* list) {
  AstNode* node = createNode(NODE_TYPE_SORT);
  node->addMember(list);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST sort element node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeSortElement (AstNode const* expression,
                                     bool ascending) {
  AstNode* node = createNode(NODE_TYPE_SORT_ELEMENT);
  node->addMember(expression);
  node->setBoolValue(ascending);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST limit node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeLimit (AstNode const* offset,
                               AstNode const* count) {
  AstNode* node = createNode(NODE_TYPE_LIMIT);
  node->addMember(offset);
  node->addMember(count);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST assign node, used in COLLECT statements
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeAssign (char const* variableName,
                                AstNode const* expression) {
  if (variableName == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  AstNode* node = createNode(NODE_TYPE_ASSIGN);
  AstNode* variable = createNodeVariable(variableName, true);
  node->addMember(variable);
  node->addMember(expression);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST variable node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeVariable (char const* name, 
                                  bool isUserDefined) {
  if (name == nullptr || *name == '\0') {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  if (isUserDefined && *name == '_') {
    _query->registerError(TRI_ERROR_QUERY_VARIABLE_NAME_INVALID);
    return nullptr;
  }

  if (_scopes.existsVariable(name)) {
    _query->registerError(TRI_ERROR_QUERY_VARIABLE_REDECLARED, name);
    return nullptr;
  }
  
  auto variable = _variables.createVariable(name, isUserDefined);
  _scopes.addVariable(variable);

  AstNode* node = createNode(NODE_TYPE_VARIABLE);
  node->setData(static_cast<void*>(variable));

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST collection node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeCollection (char const* name) {
  if (name == nullptr || *name == '\0') {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  if (! TRI_IsAllowedNameCollection(true, name)) {
    _query->registerError(TRI_ERROR_ARANGO_ILLEGAL_NAME, name);
    return nullptr;
  }

  AstNode* node = createNode(NODE_TYPE_COLLECTION);
  node->setStringValue(name);
 
  _query->collections()->add(name, TRI_TRANSACTION_READ);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST reference node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeReference (char const* variableName) {
  if (variableName == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  AstNode* node = createNode(NODE_TYPE_REFERENCE);

  auto variable = _scopes.getVariable(variableName);

  if (variable == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "variable not found in reference AstNode");
  }

  node->setData(variable);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST parameter node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeParameter (char const* name) {
  if (name == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  AstNode* node = createNode(NODE_TYPE_PARAMETER);
 
  node->setStringValue(name);

  // insert bind parameter name into list of found parameters
  _bindParameters.emplace(name);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST unary operator node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeUnaryOperator (AstNodeType type,
                                       AstNode const* operand) {
  AstNode* node = createNode(type);
  node->addMember(operand);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST binary operator node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeBinaryOperator (AstNodeType type,
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

AstNode* Ast::createNodeTernaryOperator (AstNode const* condition,
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

AstNode* Ast::createNodeSubquery (char const* variableName,
                                  AstNode const* subQuery) {
  if (variableName == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  AstNode* node = createNode(NODE_TYPE_SUBQUERY);
  AstNode* variable = createNodeVariable(variableName, false);
  node->addMember(variable);
  node->addMember(subQuery);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST attribute access node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeAttributeAccess (AstNode const* accessed,
                                         char const* attributeName) {
  if (attributeName == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  AstNode* node = createNode(NODE_TYPE_ATTRIBUTE_ACCESS);
  node->addMember(accessed);
  node->setStringValue(attributeName);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST attribute access node w/ bind parameter
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeBoundAttributeAccess (AstNode const* accessed,
                                              AstNode const* parameter) {
  AstNode* node = createNode(NODE_TYPE_BOUND_ATTRIBUTE_ACCESS);
  node->setStringValue(parameter->getStringValue());
  node->addMember(accessed);
  node->addMember(parameter);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST indexed access node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeIndexedAccess (AstNode const* accessed,
                                       AstNode const* indexValue) {
  AstNode* node = createNode(NODE_TYPE_INDEXED_ACCESS);
  node->addMember(accessed);
  node->addMember(indexValue);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST expand node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeExpand (AstNode const* iterator,
                                AstNode const* expansion) {
  AstNode* node = createNode(NODE_TYPE_EXPAND);

  node->addMember(iterator);
  node->addMember(expansion);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST iterator node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeIterator (char const* variableName,
                                  AstNode const* expanded) {
  if (variableName == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  AstNode* node = createNode(NODE_TYPE_ITERATOR);

  AstNode* variable = createNodeVariable(variableName, false);
  node->addMember(variable);
  node->addMember(expanded);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST null value node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeValueNull () {
  AstNode* node = createNode(NODE_TYPE_VALUE);
  node->setValueType(VALUE_TYPE_NULL); 

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST bool value node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeValueBool (bool value) {
  AstNode* node = createNode(NODE_TYPE_VALUE);
  node->setValueType(VALUE_TYPE_BOOL);
  node->setBoolValue(value);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST int value node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeValueInt (int64_t value) {
  AstNode* node = createNode(NODE_TYPE_VALUE);
  node->setValueType(VALUE_TYPE_INT);
  node->setIntValue(value);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST double value node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeValueDouble (double value) {
  AstNode* node = createNode(NODE_TYPE_VALUE);
  node->setValueType(VALUE_TYPE_DOUBLE);
  node->setDoubleValue(value);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST string value node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeValueString (char const* value) {
  if (value == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  AstNode* node = createNode(NODE_TYPE_VALUE);
  node->setValueType(VALUE_TYPE_STRING);
  node->setStringValue(value);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST list node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeList () {
  AstNode* node = createNode(NODE_TYPE_LIST);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST array node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeArray () {
  AstNode* node = createNode(NODE_TYPE_ARRAY);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST array element node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeArrayElement (char const* attributeName,
                                      AstNode const* expression) {
  if (attributeName == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  AstNode* node = createNode(NODE_TYPE_ARRAY_ELEMENT);
  node->setStringValue(attributeName);
  node->addMember(expression);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST function call node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeFunctionCall (char const* functionName,
                                      AstNode const* arguments) {
  if (functionName == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  auto normalized = normalizeFunctionName(functionName);

  AstNode* node;

  if (normalized.second) {
    // built-in function
    node = createNode(NODE_TYPE_FCALL);
    // register a pointer to the function
    auto func = _query->executor()->getFunctionByName(normalized.first);

    TRI_ASSERT(func != nullptr);
    node->setData(static_cast<void const*>(func));

    TRI_ASSERT(arguments != nullptr);
    TRI_ASSERT(arguments->type == NODE_TYPE_LIST);
  
    // validate number of function call arguments
    size_t const n = arguments->numMembers();
    
    auto numExpectedArguments = func->numArguments();
    if (n < numExpectedArguments.first || n > numExpectedArguments.second) {
      THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, 
                                    functionName,
                                    static_cast<int>(numExpectedArguments.first),
                                    static_cast<int>(numExpectedArguments.second));
    }

  }
  else {
    // user-defined function
    node = createNode(NODE_TYPE_FCALL_USER);
    // register the function name
    char* fname = _query->registerString(normalized.first.c_str(), normalized.first.size(), false);
    node->setStringValue(fname);
  }

  node->addMember(arguments);
  
  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST range node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeRange (AstNode const* start,
                               AstNode const* end) {
  AstNode* node = createNode(NODE_TYPE_RANGE);
  node->addMember(start);
  node->addMember(end);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST nop node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeNop () {
  return const_cast<AstNode*>(&NopNode);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief injects bind parameters into the AST
////////////////////////////////////////////////////////////////////////////////

void Ast::injectBindParameters (BindParameters& parameters) {
  auto p = parameters();

  auto func = [&](AstNode* node, void*) -> AstNode* {
    if (node->type == NODE_TYPE_PARAMETER) {
      // found a bind parameter in the query string
      char const* param = node->getStringValue();

      if (param == nullptr) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
      }

      auto it = p.find(std::string(param));

      if (it == p.end()) {
        // query uses a bind parameter that was not defined by the user
        _query->registerError(TRI_ERROR_QUERY_BIND_PARAMETER_MISSING, param);
        return nullptr;
      }

      // mark the bind parameter as being used
      (*it).second.second = true;

      auto value = (*it).second.first;

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
        char const* name = _query->registerString(value->_value._string.data, value->_value._string.length - 1, false);

        node = createNodeCollection(name);

        if (isWriteCollection) {
          // this was the bind parameter that contained the collection to update 
          _writeCollection = node;
        }
      }
      else {
        node = nodeFromJson(value);
      }
    }

    else if (node->type == NODE_TYPE_BOUND_ATTRIBUTE_ACCESS) {
      // look at second sub-node. this is the (replaced) bind parameter
      auto name = node->getMember(1);

      if (name->type != NODE_TYPE_VALUE ||
          name->value.type != VALUE_TYPE_STRING ||
          *name->value.value._string == '\0') {
        // if no string value was inserted for the parameter name, this is an error
        THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, node->getStringValue());
      }
    }

    return node;
  };

  _root = traverse(_root, func, &p); 
  
  if (_writeCollection != nullptr &&
      _writeCollection->type == NODE_TYPE_COLLECTION) {

    _query->collections()->add(_writeCollection->getStringValue(), TRI_TRANSACTION_WRITE);
  }

  for (auto it = p.begin(); it != p.end(); ++it) {
    if (! (*it).second.second) {
      THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_BIND_PARAMETER_UNDECLARED, (*it).first.c_str());
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replace variables
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::replaceVariables (AstNode* node,
                                std::unordered_map<VariableId, Variable const*> const& replacements) {
  auto func = [&](AstNode* node, void*) -> AstNode* {
    if (node == nullptr) {
      return nullptr;
    }

    // reference to a variable
    if (node->type == NODE_TYPE_REFERENCE) {
      auto variable = static_cast<Variable*>(node->getData());
      if (variable != nullptr) {
        auto it = replacements.find(variable->id);
        if (it != replacements.end()) {
          // overwrite the node in place
          node->setData((*it).second);
        }
      }
      // fall-through intentional
    }
    
    return node;
  };

  return traverse(node, func, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimizes the AST
////////////////////////////////////////////////////////////////////////////////

void Ast::optimize () {
  auto func = [&](AstNode* node, void*) -> AstNode* {
    if (node == nullptr) {
      return nullptr;
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
        node->type == NODE_TYPE_OPERATOR_BINARY_IN ||
        node->type == NODE_TYPE_OPERATOR_BINARY_NIN) {
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

    // call to built-in function
    if (node->type == NODE_TYPE_FCALL) {
      return optimizeFunctionCall(node);
    }
    
    // reference to a variable
    if (node->type == NODE_TYPE_REFERENCE) {
      return optimizeReference(node);
    }
    
    // range
    if (node->type == NODE_TYPE_RANGE) {
      return optimizeRange(node);
    }
    
    // LET
    if (node->type == NODE_TYPE_LET) {
      return optimizeLet(node);
    }

    return node;
  };

  // optimization
  _root = traverse(_root, func, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief determines the variables referenced in an expression
////////////////////////////////////////////////////////////////////////////////

std::unordered_set<Variable*> Ast::getReferencedVariables (AstNode const* node) {
  auto func = [&](AstNode const* node, void* data) -> void {
    if (node == nullptr) {
      return;
    }

    // reference to a variable
    if (node->type == NODE_TYPE_REFERENCE) {
      auto variable = static_cast<Variable*>(node->getData());

      if (variable == nullptr) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
      }

      if (variable->needsRegister()) {
        auto result = static_cast<std::unordered_set<Variable*>*>(data);
        result->insert(variable);
      }
    }
  };

  std::unordered_set<Variable*> result;  
  traverse(node, func, &result); 

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief recursively clone a node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::clone (AstNode const* node) {
  auto type = node->type;
  auto copy = createNode(type);

  // special handling for certain node types
  // copy payload...
  if (type == NODE_TYPE_COLLECTION ||
      type == NODE_TYPE_PARAMETER ||
      type == NODE_TYPE_ATTRIBUTE_ACCESS ||
      type == NODE_TYPE_ARRAY_ELEMENT ||
      type == NODE_TYPE_FCALL_USER) {
    copy->setStringValue(node->getStringValue());
  }
  else if (type == NODE_TYPE_VARIABLE ||
           type == NODE_TYPE_REFERENCE ||
           type == NODE_TYPE_FCALL) {
    copy->setData(node->getData());
  }
  else if (type == NODE_TYPE_SORT_ELEMENT) {
    copy->setBoolValue(node->getBoolValue());
  }
  else if (type == NODE_TYPE_VALUE) {
    switch (node->value.type) {
      case VALUE_TYPE_BOOL:
        copy->setBoolValue(node->getBoolValue());
        break;
      case VALUE_TYPE_INT:
        copy->setIntValue(node->getIntValue());
        break;
      case VALUE_TYPE_DOUBLE:
        copy->setDoubleValue(node->getDoubleValue());
        break;
      case VALUE_TYPE_STRING:
        copy->setStringValue(node->getStringValue());
        break;
      default: {
      }
    }
  }

  // recursively clone subnodes
  size_t const n = node->numMembers();
  for (size_t i = 0; i < n; ++i) {
    copy->addMember(clone(node->getMember(i)));
  }

  return copy;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an expression with constant parameters
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::executeConstExpression (AstNode const* node) {
  TRI_json_t* result = _query->executor()->executeExpression(node); 

  if (result == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  AstNode* value = nullptr;
  try {
    value = nodeFromJson(result);
  }
  catch (...) {
  }
  
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, result);

  if (value == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  return value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimizes the unary operators + and -
/// the unary plus will be converted into a simple value node if the operand of
/// the operation is a constant number
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::optimizeUnaryOperatorArithmetic (AstNode* node) {
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
    _query->registerError(TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
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
        _query->registerError(TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE);
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

AstNode* Ast::optimizeNotExpression (AstNode* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->type == NODE_TYPE_OPERATOR_UNARY_NOT);
  TRI_ASSERT(node->numMembers() == 1);

  AstNode* operand = node->getMember(0);

  if (operand->isComparisonOperator()) {
    // remove the NOT and reverse the operation, e.g. NOT (a == b) => (a != b)
    TRI_ASSERT(operand->numMembers() == 2);
    auto lhs = operand->getMember(0);
    auto rhs = operand->getMember(1);

    auto it = ReverseOperators.find(static_cast<int>(operand->type));
    TRI_ASSERT(it != ReverseOperators.end());

    return createNodeBinaryOperator((*it).second, lhs, rhs); 
  }

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimizes the unary operator NOT
/// the unary NOT operation will be replaced with the result of the operation
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::optimizeUnaryOperatorLogical (AstNode* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->type == NODE_TYPE_OPERATOR_UNARY_NOT);
  TRI_ASSERT(node->numMembers() == 1);

  AstNode* operand = node->getMember(0);
  if (! operand->isConstant()) {
    // operand is dynamic, cannot statically optimize it
    return optimizeNotExpression(node);
  }

  if (! operand->isBoolValue()) {
    _query->registerError(TRI_ERROR_QUERY_INVALID_LOGICAL_VALUE);
    return node;
  }

  // replace unary negation operation with result of negation
  return createNodeValueBool(! operand->getBoolValue());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimizes the binary logical operators && and ||
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::optimizeBinaryOperatorLogical (AstNode* node) {
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
    _query->registerError(TRI_ERROR_QUERY_INVALID_LOGICAL_VALUE);
    return node;
  }

  if (rhsIsConst && ! rhs->isBoolValue()) {
    // right operand is a constant value, but no boolean
    _query->registerError(TRI_ERROR_QUERY_INVALID_LOGICAL_VALUE);
    return node;
  }

  if (! lhsIsConst) {
    if (node->type == NODE_TYPE_OPERATOR_BINARY_OR) {
      if (rhsIsConst && ! rhs->getBoolValue()) {
        // (lhs || false) => lhs
        return lhs;
      }
    }

    // default: don't optimize
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

AstNode* Ast::optimizeBinaryOperatorRelational (AstNode* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 2);

  AstNode* lhs = node->getMember(0);
  AstNode* rhs = node->getMember(1);

  if (lhs == nullptr || rhs == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
  
  bool const lhsIsConst = lhs->isConstant(); 
  bool const rhsIsConst = rhs->isConstant(); 

  if (! lhs->canThrow() &&
      rhs->type == NODE_TYPE_LIST &&
      rhs->numMembers() <= 1 &&
      (node->type == NODE_TYPE_OPERATOR_BINARY_IN ||
       node->type == NODE_TYPE_OPERATOR_BINARY_NIN)) {
    // turn an IN or a NOT IN with few members into an equality comparison
    if (rhs->numMembers() == 0) {
      // IN with no members returns false
      // NOT IN with no members returns true
      return createNodeValueBool(node->type == NODE_TYPE_OPERATOR_BINARY_NIN);
    }
    else if (rhs->numMembers() == 1) {
      // IN with a single member becomes equality
      // NOT IN with a single members becomes unequality
      if (node->type == NODE_TYPE_OPERATOR_BINARY_IN) {
        node = createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_EQ, lhs, rhs->getMember(0));
      }
      else {
        node = createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_NE, lhs, rhs->getMember(0));
      }
      // and optimize ourselves...
      return optimizeBinaryOperatorRelational(node);
    }
    // fall-through intentional
  }

  if (! rhsIsConst) {
    return node;
  }
  
  if (rhs->type != NODE_TYPE_LIST &&
      (node->type == NODE_TYPE_OPERATOR_BINARY_IN ||
       node->type == NODE_TYPE_OPERATOR_BINARY_NIN)) {
    // right operand of IN or NOT IN must be a list
    _query->registerError(TRI_ERROR_QUERY_LIST_EXPECTED);
    return node;
  }

  if (! lhsIsConst) {
    return node;
  }

  return executeConstExpression(node);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimizes the binary arithmetic operators +, -, *, / and %
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::optimizeBinaryOperatorArithmetic (AstNode* node) {
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
    _query->registerError(TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
    return node;
  }
  
  if (rhsIsConst && ! rhs->isNumericValue()) {
    // rhs is not a number
    _query->registerError(TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
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
      _query->registerError(TRI_ERROR_QUERY_DIVISION_BY_ZERO);
      return node;
    }

    value = l / r;
  }
  else if (node->type == NODE_TYPE_OPERATOR_BINARY_MOD) {
    if (r == 0.0) {
      _query->registerError(TRI_ERROR_QUERY_DIVISION_BY_ZERO);
      return node;
    }

    value = fmod(l, r);
  }
  else {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid operator");
  }
      
  if (value != value || 
      value == HUGE_VAL || 
      value == - HUGE_VAL) {
    // IEEE754 NaN values have an interesting property that we can exploit...
    // if the architecture does not use IEEE754 values then this shouldn't do
    // any harm either
    _query->registerError(TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE);
    return node;
  }

  return createNodeValueDouble(value);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimizes the ternary operator
/// if the condition is constant, the operator will be replaced with either the
/// true part or the false part
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::optimizeTernaryOperator (AstNode* node) {
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
    _query->registerError(TRI_ERROR_QUERY_INVALID_LOGICAL_VALUE);
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
/// @brief optimizes a call to a built-in function
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::optimizeFunctionCall (AstNode* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->type == NODE_TYPE_FCALL);
  TRI_ASSERT(node->numMembers() == 1);

  auto func = static_cast<Function*>(node->getData());
  TRI_ASSERT(func != nullptr);

  if (! func->isDeterministic) {
    // non-deterministic function
    return node;
  }

  if (! node->getMember(0)->isConstant()) {
    // arguments to function call are not constant
    return node;
  }

  return executeConstExpression(node);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimizes a reference to a variable
/// references are replaced with constants if possible
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::optimizeReference (AstNode* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->type == NODE_TYPE_REFERENCE);
 
  auto variable = static_cast<Variable*>(node->getData());

  if (variable == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  // constant propagation
  if (variable->constValue() == nullptr) {
    return node;
  }

  return static_cast<AstNode*>(variable->constValue());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimizes the range operator
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::optimizeRange (AstNode* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->type == NODE_TYPE_RANGE);
  TRI_ASSERT(node->numMembers() == 2);

  AstNode* lhs = node->getMember(0);
  AstNode* rhs = node->getMember(1);

  if (lhs == nullptr || rhs == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  if (! lhs->isConstant() || ! rhs->isConstant()) {
    return node;
  }

  if (! lhs->isNumericValue() || ! rhs->isNumericValue()) {
    _query->registerError(TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
  }

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimizes the LET statement 
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::optimizeLet (AstNode* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->type == NODE_TYPE_LET);
  TRI_ASSERT(node->numMembers() == 2);

  AstNode* variable = node->getMember(0);
  AstNode* expression = node->getMember(1);
    
  auto v = static_cast<Variable*>(variable->getData());
  TRI_ASSERT(v != nullptr);

  if (expression->isConstant()) {
    // if the expression assigned to the LET variable is constant, we'll store
    // a pointer to the const value in the variable
    // further optimizations can then use this pointer and optimize further, e.g.
    // LET a = 1 LET b = a + 1, c = b + a can be optimized to LET a = 1 LET b = 2 LET c = 4
    v->constValue(static_cast<void*>(expression));
  }  

  return node; 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST node from JSON
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::nodeFromJson (TRI_json_t const* json) {
  TRI_ASSERT(json != nullptr);

  if (json->_type == TRI_JSON_BOOLEAN) {
    return createNodeValueBool(json->_value._boolean);
  }

  if (json->_type == TRI_JSON_NUMBER) {
    return createNodeValueDouble(json->_value._number);
  }

  if (json->_type == TRI_JSON_STRING) {
    char const* value = _query->registerString(json->_value._string.data, json->_value._string.length - 1, false);
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
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unexpected type found in array node");
      }

      char const* attributeName = _query->registerString(key->_value._string.data, key->_value._string.length - 1, false);
      
      node->addMember(createNodeArrayElement(attributeName, nodeFromJson(value)));
    }

    return node;
  }
  
  return createNodeValueNull();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief traverse the AST
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::traverse (AstNode* node, 
                        std::function<AstNode*(AstNode*, void*)> func,
                        void* data) {
  if (node == nullptr) {
    return nullptr;
  }
 
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
/// @brief traverse the AST, with const nodes
////////////////////////////////////////////////////////////////////////////////

void Ast::traverse (AstNode const* node, 
                    std::function<void(AstNode const*, void*)> func,
                    void* data) {
  if (node == nullptr) {
    return;
  }
  
  size_t const n = node->numMembers();

  for (size_t i = 0; i < n; ++i) {
    auto member = node->getMember(i);

    if (member != nullptr) {
      traverse(const_cast<AstNode const*>(member), func, data);
    }
  }

  func(node, data);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief normalize a function name
////////////////////////////////////////////////////////////////////////////////

std::pair<std::string, bool> Ast::normalizeFunctionName (char const* name) {
  TRI_ASSERT(name != nullptr);

  char* upperName = TRI_UpperAsciiStringZ(TRI_UNKNOWN_MEM_ZONE, name);

  if (upperName == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  std::string functionName(upperName);

  TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, upperName);

  if (functionName.find(':') == std::string::npos) {
    // prepend default namespace for internal functions
    return std::make_pair(functionName, true);
  }

  // user-defined function
  return std::make_pair(functionName, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a node of the specified type
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNode (AstNodeType type) {
  auto node = new AstNode(type);

  try {
    _query->addNode(node);
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

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
#include "Aql/Arithmetic.h"
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
/// @brief initialise a singleton no-op node instance
////////////////////////////////////////////////////////////////////////////////

AstNode const Ast::NopNode{ NODE_TYPE_NOP }; 

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise a singleton null node instance
////////////////////////////////////////////////////////////////////////////////

AstNode const Ast::NullNode{ NODE_TYPE_VALUE, VALUE_TYPE_NULL }; 

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise a singleton false node instance
////////////////////////////////////////////////////////////////////////////////

AstNode const Ast::FalseNode{ false, VALUE_TYPE_BOOL }; 

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise a singleton true node instance
////////////////////////////////////////////////////////////////////////////////

AstNode const Ast::TrueNode{ true, VALUE_TYPE_BOOL }; 

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise a singleton zero node instance
////////////////////////////////////////////////////////////////////////////////

AstNode const Ast::ZeroNode{ static_cast<int64_t>(0), VALUE_TYPE_INT }; 

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise a singleton empty string node instance
////////////////////////////////////////////////////////////////////////////////

AstNode const Ast::EmptyStringNode{ "", VALUE_TYPE_STRING }; 

////////////////////////////////////////////////////////////////////////////////
/// @brief inverse comparison operators
////////////////////////////////////////////////////////////////////////////////

std::unordered_map<int, AstNodeType> const Ast::NegatedOperators{ 
  { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_EQ), NODE_TYPE_OPERATOR_BINARY_NE },
  { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_NE), NODE_TYPE_OPERATOR_BINARY_EQ },
  { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_GT), NODE_TYPE_OPERATOR_BINARY_LE },
  { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_GE), NODE_TYPE_OPERATOR_BINARY_LT },
  { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_LT), NODE_TYPE_OPERATOR_BINARY_GE },
  { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_LE), NODE_TYPE_OPERATOR_BINARY_GT },
  { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_IN), NODE_TYPE_OPERATOR_BINARY_NIN },
  { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_NIN), NODE_TYPE_OPERATOR_BINARY_IN }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief reverse comparison operators
////////////////////////////////////////////////////////////////////////////////

std::unordered_map<int, AstNodeType> const Ast::ReversedOperators{ 
  { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_EQ), NODE_TYPE_OPERATOR_BINARY_EQ },
  { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_GT), NODE_TYPE_OPERATOR_BINARY_LT },
  { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_GE), NODE_TYPE_OPERATOR_BINARY_LE },
  { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_LT), NODE_TYPE_OPERATOR_BINARY_GT },
  { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_LE), NODE_TYPE_OPERATOR_BINARY_GE }
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
  TRI_json_t* json = TRI_CreateArrayJson(zone);

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
                                 char const* name,
                                 AstNode const* keepVariables) {
  AstNode* node = createNode(NODE_TYPE_COLLECT);
  node->addMember(list);

  // INTO
  if (name != nullptr) {
    AstNode* variable = createNodeVariable(name, true);
    node->addMember(variable);

    // KEEP
    if (keepVariables != nullptr) {
      node->addMember(keepVariables);
    }
  }
  else {
    TRI_ASSERT(keepVariables == nullptr);
  }

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST collect node, INTO var = expr
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeCollectExpression (AstNode const* list,
                                           char const* name,
                                           AstNode const* expression) {
  AstNode* node = createNode(NODE_TYPE_COLLECT_EXPRESSION);
  node->addMember(list);

  AstNode* variable = createNodeVariable(name, true);
  node->addMember(variable);

  node->addMember(expression);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST collect node, COUNT INTO 
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeCollectCount (AstNode const* list,
                                      char const* name) {
  AstNode* node = createNode(NODE_TYPE_COLLECT_COUNT);
  node->addMember(list);

  AstNode* variable = createNodeVariable(name, true);
  node->addMember(variable);

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
                                     AstNode const* ascending) {
  AstNode* node = createNode(NODE_TYPE_SORT_ELEMENT);
  node->addMember(expression);
  node->addMember(ascending);

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
  if (name == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  if (*name == '\0' || ! TRI_IsAllowedNameCollection(true, name)) {
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
/// @brief create an AST reference node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeReference (Variable const* variable) {
  AstNode* node = createNode(NODE_TYPE_REFERENCE);
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
  // performance optimization:
  // return a pointer to the singleton null node
  // note: this node is never registered nor freed
  return const_cast<AstNode*>(&NullNode);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST bool value node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeValueBool (bool value) {
  // performance optimization:
  // return a pointer to the singleton bool nodes
  // note: these nodes are never registered nor freed
  if (value) {
    return const_cast<AstNode*>(&TrueNode);
  }

  return const_cast<AstNode*>(&FalseNode);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST int value node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeValueInt (int64_t value) {
  if (value == 0) {
    // performance optimization:
    // return a pointer to the singleton zero node
    // note: these nodes are never registered nor freed
    return const_cast<AstNode*>(&ZeroNode);
  }

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

  if (*value == '\0') {
    // performance optimization:
    // return a pointer to the singleton empty string node
    // note: these nodes are never registered nor freed
    return const_cast<AstNode*>(&EmptyStringNode);
  }

  AstNode* node = createNode(NODE_TYPE_VALUE);
  node->setValueType(VALUE_TYPE_STRING);
  node->setStringValue(value);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST list node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeArray () {
  AstNode* node = createNode(NODE_TYPE_ARRAY);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST array node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeObject () {
  return createNode(NODE_TYPE_OBJECT);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST array element node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeObjectElement (char const* attributeName,
                                       AstNode const* expression) {
  if (attributeName == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  AstNode* node = createNode(NODE_TYPE_OBJECT_ELEMENT);
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
    TRI_ASSERT(arguments->type == NODE_TYPE_ARRAY);
  
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
  auto visitor = [&](AstNode* node, void*) -> AstNode* {
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

  return traverse(node, visitor, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimizes the AST
////////////////////////////////////////////////////////////////////////////////

void Ast::optimize () {
  struct TraversalContext {
    bool isInFilter  = false;
  };

  auto preVisitor = [&](AstNode const* node, void* data) -> void {
    if (node->type == NODE_TYPE_FILTER) {
      static_cast<TraversalContext*>(data)->isInFilter = true;
    }
  };

  auto postVisitor = [&](AstNode const* node, void* data) -> void {
    if (node->type == NODE_TYPE_FILTER) {
      static_cast<TraversalContext*>(data)->isInFilter = false;
    }
  };

  auto visitor = [&](AstNode* node, void* data) -> AstNode* {
    if (node == nullptr) {
      return nullptr;
    }

    // unary operators
    if (node->type == NODE_TYPE_OPERATOR_UNARY_PLUS ||
        node->type == NODE_TYPE_OPERATOR_UNARY_MINUS) {
      return this->optimizeUnaryOperatorArithmetic(node);
    }

    if (node->type == NODE_TYPE_OPERATOR_UNARY_NOT) {
      return this->optimizeUnaryOperatorLogical(node);
    }

    // binary operators
    if (node->type == NODE_TYPE_OPERATOR_BINARY_AND ||
        node->type == NODE_TYPE_OPERATOR_BINARY_OR) {
      return this->optimizeBinaryOperatorLogical(node, static_cast<TraversalContext*>(data)->isInFilter);
    }
    
    if (node->type == NODE_TYPE_OPERATOR_BINARY_EQ ||
        node->type == NODE_TYPE_OPERATOR_BINARY_NE ||
        node->type == NODE_TYPE_OPERATOR_BINARY_LT ||
        node->type == NODE_TYPE_OPERATOR_BINARY_LE ||
        node->type == NODE_TYPE_OPERATOR_BINARY_GT ||
        node->type == NODE_TYPE_OPERATOR_BINARY_GE ||
        node->type == NODE_TYPE_OPERATOR_BINARY_IN ||
        node->type == NODE_TYPE_OPERATOR_BINARY_NIN) {
      return this->optimizeBinaryOperatorRelational(node);
    }
    
    if (node->type == NODE_TYPE_OPERATOR_BINARY_PLUS ||
        node->type == NODE_TYPE_OPERATOR_BINARY_MINUS ||
        node->type == NODE_TYPE_OPERATOR_BINARY_TIMES ||
        node->type == NODE_TYPE_OPERATOR_BINARY_DIV ||
        node->type == NODE_TYPE_OPERATOR_BINARY_MOD) {
      return this->optimizeBinaryOperatorArithmetic(node);
    }
      
    // ternary operator
    if (node->type == NODE_TYPE_OPERATOR_TERNARY) {
      return this->optimizeTernaryOperator(node);
    }

    // call to built-in function
    if (node->type == NODE_TYPE_FCALL) {
      return this->optimizeFunctionCall(node);
    }
    
    // reference to a variable, may be able to insert the variable value directly
    if (node->type == NODE_TYPE_REFERENCE) {
      return this->optimizeReference(node);
    }
    
    // LET
    if (node->type == NODE_TYPE_LET) {
      return this->optimizeLet(node);
    }

    // FILTER
    if (node->type == NODE_TYPE_FILTER) {
      return this->optimizeFilter(node);
    }
 
    // FOR
    if (node->type == NODE_TYPE_FOR) {
      return this->optimizeFor(node);
    }

    return node;
  };

  // run the optimizations
  TraversalContext context;
  this->_root = traverse(this->_root, preVisitor, visitor, postVisitor, &context);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief determines the variables referenced in an expression
////////////////////////////////////////////////////////////////////////////////

std::unordered_set<Variable*> Ast::getReferencedVariables (AstNode const* node) {
  auto visitor = [&](AstNode const* node, void* data) -> void {
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
  traverse(node, visitor, &result); 

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
      type == NODE_TYPE_OBJECT_ELEMENT ||
      type == NODE_TYPE_FCALL_USER) {
    copy->setStringValue(node->getStringValue());
  }
  else if (type == NODE_TYPE_VARIABLE ||
           type == NODE_TYPE_REFERENCE ||
           type == NODE_TYPE_FCALL) {
    copy->setData(node->getData());
  }
  else if (type == NODE_TYPE_VALUE) {
    switch (node->value.type) {
      case VALUE_TYPE_NULL:
        copy->value.type = VALUE_TYPE_NULL;
        break;
      case VALUE_TYPE_BOOL:
        copy->value.type = VALUE_TYPE_BOOL;
        copy->setBoolValue(node->getBoolValue());
        break;
      case VALUE_TYPE_INT:
        copy->value.type = VALUE_TYPE_INT;
        copy->setIntValue(node->getIntValue());
        break;
      case VALUE_TYPE_DOUBLE:
        copy->value.type = VALUE_TYPE_DOUBLE;
        copy->setDoubleValue(node->getDoubleValue());
        break;
      case VALUE_TYPE_STRING:
        copy->value.type = VALUE_TYPE_STRING;
        copy->setStringValue(node->getStringValue());
        break;
    }
  }

  // recursively clone subnodes
  size_t const n = node->numMembers();
  for (size_t i = 0; i < n; ++i) {
    copy->addMember(clone(node->getMember(i)));
  }

  return copy;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the reversed operator for a comparison operator
////////////////////////////////////////////////////////////////////////////////

AstNodeType Ast::ReverseOperator (AstNodeType type) {
  auto it = ReversedOperators.find(static_cast<int>(type));
  if (it == ReversedOperators.end()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid node type for inversed operator");
  }
  
  return (*it).second;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create a number node for an arithmetic result, integer
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createArithmeticResultNode (int64_t value) {
  return createNodeValueInt(value);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a number node for an arithmetic result, double
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createArithmeticResultNode (double value) {
  if (value != value || // intentional! 
      value == HUGE_VAL || 
      value == - HUGE_VAL) {
    // IEEE754 NaN values have an interesting property that we can exploit...
    // if the architecture does not use IEEE754 values then this shouldn't do
    // any harm either
    _query->registerWarning(TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE);
    return createNodeValueNull();
  }

  return createNodeValueDouble(value);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an expression with constant parameters
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::executeConstExpression (AstNode const* node) {
  // must enter v8 before we can execute any expression
  _query->enterContext();
  ISOLATE;
  v8::HandleScope scope(isolate); // do not delete this!

  TRI_json_t* result = _query->executor()->executeExpression(_query, node); 

  // context is not left here, but later
  // this allows re-using the same context for multiple expressions

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

  // operand is a constant, now convert it into a number
  auto converted = operand->castToNumber(this);

  if (converted->isNullValue()) {
    return createNodeValueNull();
  }

  if (node->type == NODE_TYPE_OPERATOR_UNARY_PLUS) {
    // + number => number
    return converted;
  }
  else {
    // - number
    if (converted->value.type == VALUE_TYPE_INT) {
      // int64
      return createNodeValueInt(- converted->getIntValue());
    }
    else {
      // double
      double const value = - converted->getDoubleValue();
      
      if (value != value ||  // intentional
          value == HUGE_VAL || 
          value == - HUGE_VAL) {
        // IEEE754 NaN values have an interesting property that we can exploit...
        // if the architecture does not use IEEE754 values then this shouldn't do
        // any harm either
        return createNodeValueNull();
      }

      return createNodeValueDouble(value);
    }
  }

  TRI_ASSERT(false);
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

    auto it = NegatedOperators.find(static_cast<int>(operand->type));
    TRI_ASSERT(it != NegatedOperators.end());

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

  auto converted = operand->castToBool(this);

  // replace unary negation operation with result of negation
  return createNodeValueBool(! converted->getBoolValue());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimizes the binary logical operators && and ||
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::optimizeBinaryOperatorLogical (AstNode* node,
                                             bool canModifyResultType) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->type == NODE_TYPE_OPERATOR_BINARY_AND ||
             node->type == NODE_TYPE_OPERATOR_BINARY_OR);
  TRI_ASSERT(node->numMembers() == 2);

  auto lhs = node->getMember(0);
  auto rhs = node->getMember(1);

  if (lhs == nullptr || rhs == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  if (lhs->isConstant()) {
    // left operand is a constant value
    if (node->type == NODE_TYPE_OPERATOR_BINARY_AND) {
      if (lhs->isFalse()) {
        // return it if it is falsey
        return lhs;
      }

      // left-operand was trueish, now return right operand
      return rhs;
    }
    else if (node->type == NODE_TYPE_OPERATOR_BINARY_OR) {
      if (lhs->isTrue()) {
        // return it if it is trueish
        return lhs;
      }

      // left-operand was falsey, now return right operand
      return rhs;
    }
  }

  if (canModifyResultType) {
    if (rhs->isConstant() && ! lhs->canThrow()) {
      // right operand is a constant value
      if (node->type == NODE_TYPE_OPERATOR_BINARY_AND) {
        if (rhs->isFalse()) {
          return createNodeValueBool(false);
        }

        // right-operand was trueish, now return it
        return lhs;
      }
      else if (node->type == NODE_TYPE_OPERATOR_BINARY_OR) {
        if (rhs->isTrue()) {
          // return it if it is trueish
          return createNodeValueBool(true);
        }

        // right-operand was falsey, now return left operand
        return lhs;
      }
    }
  }

  // default case
  return node;
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
      rhs->type == NODE_TYPE_ARRAY &&
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
  
  if (rhs->type != NODE_TYPE_ARRAY &&
      (node->type == NODE_TYPE_OPERATOR_BINARY_IN ||
       node->type == NODE_TYPE_OPERATOR_BINARY_NIN)) {
    // right operand of IN or NOT IN must be a list, otherwise we return false
    return createNodeValueBool(false);
  }

  if (! lhsIsConst) {
    if (rhs->numMembers() >= 10 &&
        (node->type == NODE_TYPE_OPERATOR_BINARY_IN ||
         node->type == NODE_TYPE_OPERATOR_BINARY_NIN)) {
      // if the IN list contains a considerable amount of items, we will sort
      // it, so we can find elements quicker later using a binary search
      // note that sorting will also set a flag for the node
      rhs->sort();
    }

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

  if (lhs->isConstant() && rhs->isConstant()) {
    // now calculate the expression result
    if (node->type == NODE_TYPE_OPERATOR_BINARY_PLUS) {
      // arithmetic +
      auto left  = lhs->castToNumber(this);
      auto right = rhs->castToNumber(this);

      if (left->isNullValue() && ! lhs->isNullValue()) {
        // conversion of lhs failed
        return createNodeValueNull();
      }
      
      if (right->isNullValue() && ! rhs->isNullValue()) {
        // conversion of rhs failed
        return createNodeValueNull();
      }

      bool useDoublePrecision = (left->isDoubleValue() || right->isDoubleValue());

      if (! useDoublePrecision) {
        auto l = left->getIntValue();
        auto r = right->getIntValue();
        // check if the result would overflow
        useDoublePrecision = IsUnsafeAddition<int64_t>(l, r); 
      
        if (! useDoublePrecision) {
          // can calculate using integers
          return createArithmeticResultNode(l + r);
        }
      }
        
      // must use double precision
      return createArithmeticResultNode(left->getDoubleValue() + right->getDoubleValue());
    }
    else if (node->type == NODE_TYPE_OPERATOR_BINARY_MINUS) {
      auto left  = lhs->castToNumber(this);
      auto right = rhs->castToNumber(this);
      
      if (left->isNullValue() && ! lhs->isNullValue()) {
        // conversion of lhs failed
        return createNodeValueNull();
      }
      
      if (right->isNullValue() && ! rhs->isNullValue()) {
        // conversion of rhs failed
        return createNodeValueNull();
      }

      bool useDoublePrecision = (left->isDoubleValue() || right->isDoubleValue());

      if (! useDoublePrecision) {
        auto l = left->getIntValue();
        auto r = right->getIntValue();
        // check if the result would overflow
        useDoublePrecision = IsUnsafeSubtraction<int64_t>(l, r); 
      
        if (! useDoublePrecision) {
          // can calculate using integers
          return createArithmeticResultNode(l - r);
        }
      }
        
      // must use double precision
      return createArithmeticResultNode(left->getDoubleValue() - right->getDoubleValue());
    }
    else if (node->type == NODE_TYPE_OPERATOR_BINARY_TIMES) {
      auto left  = lhs->castToNumber(this);
      auto right = rhs->castToNumber(this);
      
      if (left->isNullValue() && ! lhs->isNullValue()) {
        // conversion of lhs failed
        return createNodeValueNull();
      }
      
      if (right->isNullValue() && ! rhs->isNullValue()) {
        // conversion of rhs failed
        return createNodeValueNull();
      }

      bool useDoublePrecision = (left->isDoubleValue() || right->isDoubleValue());
      
      if (! useDoublePrecision) {
        auto l = left->getIntValue();
        auto r = right->getIntValue();
        // check if the result would overflow
        useDoublePrecision = IsUnsafeMultiplication<int64_t>(l, r);

        if (! useDoublePrecision) {
          // can calculate using integers
          return createArithmeticResultNode(l * r);
        }
      }
        
      // must use double precision
      return createArithmeticResultNode(left->getDoubleValue() * right->getDoubleValue());
    }
    else if (node->type == NODE_TYPE_OPERATOR_BINARY_DIV) {
      auto left  = lhs->castToNumber(this);
      auto right = rhs->castToNumber(this);
      
      if (left->isNullValue() && ! lhs->isNullValue()) {
        // conversion of lhs failed
        return createNodeValueNull();
      }
      
      if (right->isNullValue() && ! rhs->isNullValue()) {
        // conversion of rhs failed
        return createNodeValueNull();
      }

      bool useDoublePrecision = (left->isDoubleValue() || right->isDoubleValue());
      if (! useDoublePrecision) {
        auto l = left->getIntValue();
        auto r = right->getIntValue();

        if (r == 0) {
          _query->registerWarning(TRI_ERROR_QUERY_DIVISION_BY_ZERO);
          return createNodeValueNull();
        }

        // check if the result would overflow
        useDoublePrecision = (IsUnsafeDivision<int64_t>(l, r) || r < -1 || r > 1);

        if (! useDoublePrecision) {
          // can calculate using integers
          return createArithmeticResultNode(l / r);
        }
      }

      if (right->getDoubleValue() == 0.0) {
        _query->registerWarning(TRI_ERROR_QUERY_DIVISION_BY_ZERO);
        return createNodeValueNull();
      }

      return createArithmeticResultNode(left->getDoubleValue() / right->getDoubleValue());
    }
    else if (node->type == NODE_TYPE_OPERATOR_BINARY_MOD) {
      auto left  = lhs->castToNumber(this);
      auto right = rhs->castToNumber(this);
      
      if (left->isNullValue() && ! lhs->isNullValue()) {
        // conversion of lhs failed
        return createNodeValueNull();
      }
      
      if (right->isNullValue() && ! rhs->isNullValue()) {
        // conversion of rhs failed
        return createNodeValueNull();
      }

      bool useDoublePrecision = (left->isDoubleValue() || right->isDoubleValue());
      if (! useDoublePrecision) {
        auto l = left->getIntValue();
        auto r = right->getIntValue();

        if (r == 0) {
          _query->registerWarning(TRI_ERROR_QUERY_DIVISION_BY_ZERO);
          return createNodeValueNull();
        }

        // check if the result would overflow
        useDoublePrecision = IsUnsafeDivision<int64_t>(l, r);

        if (! useDoublePrecision) {
          // can calculate using integers
          return createArithmeticResultNode(l % r);
        }
      }

      if (right->getDoubleValue() == 0.0) {
        _query->registerWarning(TRI_ERROR_QUERY_DIVISION_BY_ZERO);
        return createNodeValueNull();
      }

      return createArithmeticResultNode(fmod(left->getDoubleValue(), right->getDoubleValue()));
    }
      
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid operator");
  }

  return node;
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

  if (condition->isTrue()) {
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

  if (node->hasFlag(FLAG_KEEP_VARIABLENAME)) {
    // this is a reference to a variable name, not a reference to the result
    return node;
  }

  return static_cast<AstNode*>(variable->constValue());
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
/// @brief optimizes the FILTER statement 
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::optimizeFilter (AstNode* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->type == NODE_TYPE_FILTER);
  TRI_ASSERT(node->numMembers() == 1);

  AstNode* expression = node->getMember(0);

  if (expression == nullptr || ! expression->isDeterministic()) {
    return node;
  }

  if (expression->isTrue()) {
    // optimize away the filter if it is always true
    return createNodeFilter(createNodeValueBool(true));
  }

  if (expression->isFalse()) {
    // optimize away the filter if it is always false
    return createNodeFilter(createNodeValueBool(false));
  }

  return node; 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimizes the FOR statement 
/// no real optimizations are done here, but we do an early check if the
/// FOR loop operand is actually a list 
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::optimizeFor (AstNode* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->type == NODE_TYPE_FOR);
  TRI_ASSERT(node->numMembers() == 2);

  AstNode* expression = node->getMember(1);

  if (expression == nullptr) {
    return node;
  }

  if (expression->isConstant() && 
      expression->type != NODE_TYPE_ARRAY) {
    // right-hand operand to FOR statement is no array
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_ARRAY_EXPECTED,
                                   TRI_errno_string(TRI_ERROR_QUERY_ARRAY_EXPECTED) + std::string(" as operand to FOR loop"));
  }
  
  // no real optimizations will be done here  
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

  if (json->_type == TRI_JSON_ARRAY) {
    auto node = createNodeArray();
    size_t const n = json->_value._objects._length;

    for (size_t i = 0; i < n; ++i) {
      node->addMember(nodeFromJson(static_cast<TRI_json_t const*>(TRI_AtVector(&json->_value._objects, i)))); 
    }

    return node;
  }

  if (json->_type == TRI_JSON_OBJECT) {
    auto node = createNodeObject();
    size_t const n = json->_value._objects._length;

    for (size_t i = 0; i < n; i += 2) {
      TRI_json_t const* key = static_cast<TRI_json_t const*>(TRI_AtVector(&json->_value._objects, i));
      TRI_json_t const* value = static_cast<TRI_json_t const*>(TRI_AtVector(&json->_value._objects, i + 1));

      if (! TRI_IsStringJson(key) || value == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unexpected type found in array node");
      }

      char const* attributeName = _query->registerString(key->_value._string.data, key->_value._string.length - 1, false);
      
      node->addMember(createNodeObjectElement(attributeName, nodeFromJson(value)));
    }

    return node;
  }
  
  return createNodeValueNull();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief traverse the AST, using pre- and post-order visitors
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::traverse (AstNode* node, 
                        std::function<void(AstNode const*, void*)> preVisitor,
                        std::function<AstNode*(AstNode*, void*)> visitor,
                        std::function<void(AstNode const*, void*)> postVisitor,
                        void* data) {
  if (node == nullptr) {
    return nullptr;
  }
 
  preVisitor(node, data);
  size_t const n = node->numMembers();

  for (size_t i = 0; i < n; ++i) {
    auto member = node->getMember(i);

    if (member != nullptr) {
      AstNode* result = traverse(member, preVisitor, visitor, postVisitor, data);

      if (result != node) {
        node->changeMember(i, result);
      }
    }
  }

  auto result = visitor(node, data);
  postVisitor(node, data);
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief traverse the AST, using a visitor
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::traverse (AstNode* node, 
                        std::function<AstNode*(AstNode*, void*)> visitor,
                        void* data) {
  if (node == nullptr) {
    return nullptr;
  }
 
  size_t const n = node->numMembers();

  for (size_t i = 0; i < n; ++i) {
    auto member = node->getMember(i);

    if (member != nullptr) {
      AstNode* result = traverse(member, visitor, data);

      if (result != node) {
        node->changeMember(i, result);
      }
    }
  }

  return visitor(node, data);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief traverse the AST using a visitor, with const nodes
////////////////////////////////////////////////////////////////////////////////

void Ast::traverse (AstNode const* node, 
                    std::function<void(AstNode const*, void*)> visitor,
                    void* data) {
  if (node == nullptr) {
    return;
  }
  
  size_t const n = node->numMembers();

  for (size_t i = 0; i < n; ++i) {
    auto member = node->getMember(i);

    if (member != nullptr) {
      traverse(const_cast<AstNode const*>(member), visitor, data);
    }
  }

  visitor(node, data);
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
    // register the node so it gets freed automatically later
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

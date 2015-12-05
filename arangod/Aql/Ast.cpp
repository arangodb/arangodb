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
#include "Aql/Graphs.h"
#include "Aql/Query.h"
#include "Basics/Exceptions.h"
#include "Basics/json-utilities.h"
#include "Basics/tri-strings.h"
#include "VocBase/collection.h"

using namespace triagens::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                             static initialization
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize a singleton no-op node instance
////////////////////////////////////////////////////////////////////////////////

AstNode const Ast::NopNode{ NODE_TYPE_NOP }; 

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize a singleton null node instance
////////////////////////////////////////////////////////////////////////////////

AstNode const Ast::NullNode{ NODE_TYPE_VALUE, VALUE_TYPE_NULL }; 

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize a singleton false node instance
////////////////////////////////////////////////////////////////////////////////

AstNode const Ast::FalseNode{ false, VALUE_TYPE_BOOL }; 

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize a singleton true node instance
////////////////////////////////////////////////////////////////////////////////

AstNode const Ast::TrueNode{ true, VALUE_TYPE_BOOL }; 

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize a singleton zero node instance
////////////////////////////////////////////////////////////////////////////////

AstNode const Ast::ZeroNode{ static_cast<int64_t>(0), VALUE_TYPE_INT }; 

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize a singleton empty string node instance
////////////////////////////////////////////////////////////////////////////////

AstNode const Ast::EmptyStringNode{ "", 0, VALUE_TYPE_STRING }; 

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
    _writeCollections(),
    _functionsMayAccessDocuments(false) {

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
/// @brief find the bottom-most expansion subnodes (if any)
////////////////////////////////////////////////////////////////////////////////

AstNode const* Ast::findExpansionSubNode (AstNode const* current) const {
  while (true) {
    TRI_ASSERT(current->type == NODE_TYPE_EXPANSION);

    if (current->getMember(1)->type != NODE_TYPE_EXPANSION) {
      return current;
    }
    current = current->getMember(1);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST passhthru node
/// note: this type of node is only used during parsing and optimized away later
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodePassthru (AstNode const* what) {
  AstNode* node = createNode(NODE_TYPE_PASSTHRU);

  node->addMember(what);

  return node;
}
  
////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST example node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeExample (AstNode const* variable,
                                 AstNode const* example) {
  if (example == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  if (example->type != NODE_TYPE_OBJECT && example->type != NODE_TYPE_PARAMETER) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_PARSE, "expecting object literal or bind parameter for example");
  }

  AstNode* node = createNode(NODE_TYPE_EXAMPLE);

  node->setData(const_cast<AstNode*>(variable));
  node->addMember(example);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST for node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeFor (char const* variableName,
                             size_t nameLength,
                             AstNode const* expression,
                             bool isUserDefinedVariable) {
  if (variableName == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  AstNode* node = createNode(NODE_TYPE_FOR);

  AstNode* variable = createNodeVariable(variableName, nameLength, isUserDefinedVariable);
  node->addMember(variable);
  node->addMember(expression);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST let node, without an IF condition
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeLet (char const* variableName,
                             size_t nameLength,
                             AstNode const* expression,
                             bool isUserDefinedVariable) {
  if (variableName == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  AstNode* node = createNode(NODE_TYPE_LET);

  AstNode* variable = createNodeVariable(variableName, nameLength, isUserDefinedVariable);
  node->addMember(variable);
  node->addMember(expression);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST let node, without creating a variable
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeLet (AstNode const* variable,
                             AstNode const* expression) {
  if (variable == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  AstNode* node = createNode(NODE_TYPE_LET);
  node->addMember(variable);
  node->addMember(expression);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST let node, with an IF condition
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeLet (char const* variableName,
                             size_t nameLength,
                             AstNode const* expression,
                             AstNode const* condition) {
  if (variableName == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  AstNode* node = createNode(NODE_TYPE_LET);

  AstNode* variable = createNodeVariable(variableName, nameLength, true);
  node->addMember(variable);
  node->addMember(expression);
  node->addMember(condition);

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
/// @brief create an AST filter node for an UPSERT query
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeUpsertFilter (AstNode const* variable,
                                      AstNode const* object) {
  AstNode* node = createNode(NODE_TYPE_FILTER);
  AstNode* example = createNodeExample(variable, object);

  node->addMember(example);

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
  node->addMember(createNodeVariable(TRI_CHAR_LENGTH_PAIR(Variable::NAME_OLD), false)); 

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
  node->addMember(createNodeVariable(TRI_CHAR_LENGTH_PAIR(Variable::NAME_NEW), false)); 
  
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
  else {
    node->addMember(&NopNode);    
  }
  
  node->addMember(createNodeVariable(TRI_CHAR_LENGTH_PAIR(Variable::NAME_OLD), false)); 
  node->addMember(createNodeVariable(TRI_CHAR_LENGTH_PAIR(Variable::NAME_NEW), false)); 
  
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
  else {
    node->addMember(&NopNode);    
  }
  
  node->addMember(createNodeVariable(TRI_CHAR_LENGTH_PAIR(Variable::NAME_OLD), false)); 
  node->addMember(createNodeVariable(TRI_CHAR_LENGTH_PAIR(Variable::NAME_NEW), false)); 

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST upsert node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeUpsert (AstNodeType type,
                                AstNode const* docVariable,
                                AstNode const* insertExpression,
                                AstNode const* updateExpression,
                                AstNode const* collection,
                                AstNode const* options) {
  AstNode* node = createNode(NODE_TYPE_UPSERT);
  
  node->setIntValue(static_cast<int64_t>(type));

  if (options == nullptr) {
    // no options given. now use default options
    options = &NopNode;
  }

  node->addMember(options);
  node->addMember(collection);
  node->addMember(docVariable);
  node->addMember(insertExpression);
  node->addMember(updateExpression);
  
  node->addMember(createNodeReference(Variable::NAME_OLD)); 
  node->addMember(createNodeVariable(TRI_CHAR_LENGTH_PAIR(Variable::NAME_NEW), false)); 
  
  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST distinct node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeDistinct (AstNode const* value) {
  AstNode* node = createNode(NODE_TYPE_DISTINCT);

  node->addMember(value);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST collect node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeCollect (AstNode const* list,
                                 char const* name,
                                 size_t nameLength,
                                 AstNode const* keepVariables,
                                 AstNode const* options) {
  AstNode* node = createNode(NODE_TYPE_COLLECT);
  
  if (options == nullptr) {
    // no options given. now use default options
    options = &NopNode;
  }

  node->addMember(options);
  node->addMember(list);

  // INTO
  if (name != nullptr) {
    AstNode* variable = createNodeVariable(name, nameLength, true);
    node->addMember(variable);

    // KEEP
    if (keepVariables != nullptr) {
      node->addMember(keepVariables);
    }
  }
  else {
    TRI_ASSERT(keepVariables == nullptr);
    TRI_ASSERT(nameLength == 0);
  }

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST collect node, INTO var = expr
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeCollectExpression (AstNode const* list,
                                           char const* name,
                                           size_t nameLength,
                                           AstNode const* expression,
                                           AstNode const* options) {
  AstNode* node = createNode(NODE_TYPE_COLLECT_EXPRESSION);
  
  if (options == nullptr) {
    // no options given. now use default options
    options = &NopNode;
  }

  node->addMember(options);
  node->addMember(list);

  AstNode* variable = createNodeVariable(name, nameLength, true);
  node->addMember(variable);

  node->addMember(expression);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST collect node, COUNT INTO 
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeCollectCount (AstNode const* list,
                                      char const* name,
                                      size_t nameLength,
                                      AstNode const* options) {
  AstNode* node = createNode(NODE_TYPE_COLLECT_COUNT);
  
  if (options == nullptr) {
    // no options given. now use default options
    options = &NopNode;
  }

  node->addMember(options);
  node->addMember(list);

  AstNode* variable = createNodeVariable(name, nameLength, true);
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
                                size_t nameLength,
                                AstNode const* expression) {
  if (variableName == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  AstNode* node = createNode(NODE_TYPE_ASSIGN);
  AstNode* variable = createNodeVariable(variableName, nameLength, true);
  node->addMember(variable);
  node->addMember(expression);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST variable node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeVariable (char const* name, 
                                  size_t nameLength,
                                  bool isUserDefined) {
  if (name == nullptr || nameLength == 0) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  if (isUserDefined && *name == '_') {
    _query->registerError(TRI_ERROR_QUERY_VARIABLE_NAME_INVALID, name);
    return nullptr;
  }

  if (_scopes.existsVariable(name, nameLength)) {
    if (! isUserDefined &&
        (strcmp(name, Variable::NAME_OLD) == 0 ||
         strcmp(name, Variable::NAME_NEW) == 0)) {
      // special variable
      auto variable = _variables.createVariable(name, nameLength, isUserDefined);
      _scopes.replaceVariable(variable);

      AstNode* node = createNode(NODE_TYPE_VARIABLE);
      node->setData(static_cast<void*>(variable));

      return node;
    }

    _query->registerError(TRI_ERROR_QUERY_VARIABLE_REDECLARED, name);
    return nullptr;
  }
  
  auto variable = _variables.createVariable(name, nameLength, isUserDefined);
  _scopes.addVariable(variable);

  AstNode* node = createNode(NODE_TYPE_VARIABLE);
  node->setData(static_cast<void*>(variable));

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST collection node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeCollection (char const* name,
                                    TRI_transaction_type_e accessType) {
  if (name == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  if (*name == '\0' || ! TRI_IsAllowedNameCollection(true, name)) {
    _query->registerErrorCustom(TRI_ERROR_ARANGO_ILLEGAL_NAME, name);
    return nullptr;
  }

  AstNode* node = createNode(NODE_TYPE_COLLECTION);
  node->setStringValue(name, strlen(name));
 
  _query->collections()->add(name, accessType);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST reference node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeReference (char const* variableName,
                                   size_t nameLength) {
  if (variableName == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  AstNode* node = createNode(NODE_TYPE_REFERENCE);

  auto variable = _scopes.getVariable(std::string(variableName, nameLength));

  if (variable == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "variable not found in reference AstNode");
  }

  node->setData(variable);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST reference node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeReference (std::string const& variableName) {
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

AstNode* Ast::createNodeParameter (char const* name, 
                                   size_t length) {
  if (name == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  AstNode* node = createNode(NODE_TYPE_PARAMETER);
 
  node->setStringValue(name, length);

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
/// @brief create an AST attribute access node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeAttributeAccess (AstNode const* accessed,
                                         char const* attributeName,
                                         size_t nameLength) {
  if (attributeName == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  AstNode* node = createNode(NODE_TYPE_ATTRIBUTE_ACCESS);
  node->addMember(accessed);
  node->setStringValue(attributeName, nameLength);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST attribute access node w/ bind parameter
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeBoundAttributeAccess (AstNode const* accessed,
                                              AstNode const* parameter) {
  AstNode* node = createNode(NODE_TYPE_BOUND_ATTRIBUTE_ACCESS);
  node->setStringValue(parameter->getStringValue(), parameter->getStringLength());
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
/// @brief create an AST array limit node (offset, count)
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeArrayLimit (AstNode const* offset,
                                    AstNode const* count) {
  AstNode* node = createNode(NODE_TYPE_ARRAY_LIMIT);

  if (offset == nullptr) {
    offset = createNodeValueInt(0);
  }
  node->addMember(offset);
  node->addMember(count);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST expansion node, with or without a filter
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeExpansion (int64_t levels,
                                   AstNode const* iterator, 
                                   AstNode const* expanded,
                                   AstNode const* filter,
                                   AstNode const* limit,
                                   AstNode const* projection) {
  AstNode* node = createNode(NODE_TYPE_EXPANSION);
  node->setIntValue(levels);

  node->addMember(iterator);
  node->addMember(expanded);

  if (filter == nullptr) {
    node->addMember(createNodeNop());
  }
  else {
    node->addMember(filter);
  }

  if (limit == nullptr) {
    node->addMember(createNodeNop());
  }
  else {
    TRI_ASSERT(limit->type == NODE_TYPE_ARRAY_LIMIT);
    node->addMember(limit);
  }

  if (projection == nullptr) {
    node->addMember(createNodeNop());
  }
  else {
    node->addMember(projection);
  }

  TRI_ASSERT(node->numMembers() == 5);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST iterator node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeIterator (char const* variableName,
                                  size_t nameLength,
                                  AstNode const* expanded) {
  if (variableName == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  AstNode* node = createNode(NODE_TYPE_ITERATOR);

  AstNode* variable = createNodeVariable(variableName, nameLength, false);
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

AstNode* Ast::createNodeValueString (char const* value,
                                     size_t length) {
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
  node->setStringValue(value, length);

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
/// @brief create an AST unique array node, AND-merged from two other arrays
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeIntersectedArray (AstNode const* lhs, 
                                          AstNode const* rhs) {
  TRI_ASSERT(lhs->isArray() && lhs->isConstant());
  TRI_ASSERT(rhs->isArray() && rhs->isConstant());

  size_t const nl = lhs->numMembers();
  size_t const nr = rhs->numMembers();

  std::unordered_map<TRI_json_t*, AstNode const*, triagens::basics::JsonHash, triagens::basics::JsonEqual> cache(
    nl + nr,
    triagens::basics::JsonHash(), 
    triagens::basics::JsonEqual()
  );

  for (size_t i = 0; i < nl; ++i) {
    auto member = lhs->getMemberUnchecked(i);
    auto json = member->computeJson();
        
    cache.emplace(json, member);
  }
  
  auto node = createNodeArray();

  for (size_t i = 0; i < nr; ++i) {
    auto member = rhs->getMemberUnchecked(i);
    auto json = member->computeJson();
        
    auto it = cache.find(json);

    if (it != cache.end()) {
      // TODO: check if the node needs cloning or if we can get away without
      // node->addMember(clone((*it).second));
      node->addMember((*it).second); 
    }
  }

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST unique array node, OR-merged from two other arrays
/// TODO: optimize this function
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeUnionizedArray (AstNode const* lhs, 
                                        AstNode const* rhs) {
  TRI_ASSERT(lhs->isArray() && lhs->isConstant());
  TRI_ASSERT(rhs->isArray() && rhs->isConstant());

  size_t const nl = lhs->numMembers();
  size_t const nr = rhs->numMembers();

  std::unordered_map<TRI_json_t*, AstNode const*, triagens::basics::JsonHash, triagens::basics::JsonEqual> cache(
    nl + nr,
    triagens::basics::JsonHash(), 
    triagens::basics::JsonEqual()
  );

  for (size_t i = 0; i < nl + nr; ++i) {
    AstNode* member;
    if (i < nl) {
      member = lhs->getMemberUnchecked(i);
    }
    else {
      member = rhs->getMemberUnchecked(i - nl);
    }
    auto json = member->computeJson();
        
    cache.emplace(json, member);
  }
  
  auto node = createNodeArray();

  for (auto& it : cache) {
    node->addMember(it.second);
  }
  node->sort();

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST object node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeObject () {
  return createNode(NODE_TYPE_OBJECT);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST object element node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeObjectElement (char const* attributeName,
                                       size_t nameLength,
                                       AstNode const* expression) {
  if (attributeName == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  AstNode* node = createNode(NODE_TYPE_OBJECT_ELEMENT);
  node->setStringValue(attributeName, nameLength);
  node->addMember(expression);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST calculated object element node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeCalculatedObjectElement (AstNode const* attributeName,
                                                 AstNode const* expression) {
  AstNode* node = createNode(NODE_TYPE_CALCULATED_OBJECT_ELEMENT);
  node->addMember(attributeName);
  node->addMember(expression);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST collection list node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeCollectionList (AstNode const* edgeCollections) {

  AstNode* node = createNode(NODE_TYPE_COLLECTION_LIST);

  TRI_ASSERT(edgeCollections->type == NODE_TYPE_ARRAY);

  for (size_t i = 0; i < edgeCollections->numMembers(); ++i) {
    auto eC = edgeCollections->getMember(i);
    if (eC->isStringValue()) {
      _query->collections()->add(eC->getStringValue(), TRI_TRANSACTION_READ);
    } // else bindParameter use default for collection bindVar
    // We do not need to propagate these members
    node->addMember(eC);
  }
  
  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST direction node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeDirection (uint64_t direction,
                                   uint64_t steps) {
  AstNode* node = createNode(NODE_TYPE_DIRECTION);
  AstNode* dir = createNodeValueInt(direction);
  AstNode* step = createNodeValueInt(steps);
  node->addMember(dir);
  node->addMember(step);

  TRI_ASSERT(node->numMembers() == 2);
  return node;
}

AstNode* Ast::createNodeDirection (uint64_t direction,
                                   AstNode const* steps) {
  AstNode* node = createNode(NODE_TYPE_DIRECTION);
  AstNode* dir = createNodeValueInt(direction);

  node->addMember(dir);
  node->addMember(steps);

  TRI_ASSERT(node->numMembers() == 2);
  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST traversal node with only vertex variable
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeTraversal (char const* vertexVarName,
                                   size_t vertexVarLength,
                                   AstNode const* direction,
                                   AstNode const* start,
                                   AstNode const* graph) {

  if (vertexVarName == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
  AstNode* node = createNode(NODE_TYPE_TRAVERSAL);

  node->addMember(direction);
  node->addMember(start);
  node->addMember(graph);

  AstNode* vertexVar = createNodeVariable(vertexVarName, vertexVarLength, false);
  node->addMember(vertexVar);

  TRI_ASSERT(node->numMembers() == 4);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST traversal node with vertex and edge variable
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeTraversal (char const* vertexVarName,
                                   size_t vertexVarLength,
                                   char const* edgeVarName,
                                   size_t edgeVarLength,
                                   AstNode const* direction,
                                   AstNode const* start,
                                   AstNode const* graph) {
  if (edgeVarName == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
  AstNode* node = createNodeTraversal(vertexVarName, vertexVarLength, direction, start, graph);

  AstNode* edgeVar = createNodeVariable(edgeVarName, edgeVarLength, false);
  node->addMember(edgeVar);

  TRI_ASSERT(node->numMembers() == 5);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST traversal node with vertex, edge and path variable
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeTraversal (char const* vertexVarName,
                                   size_t vertexVarLength,
                                   char const* edgeVarName,
                                   size_t edgeVarLength,
                                   char const* pathVarName,
                                   size_t pathVarLength,
                                   AstNode const* direction,
                                   AstNode const* start,
                                   AstNode const* graph) {

  if (pathVarName == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
  AstNode* node = createNodeTraversal(vertexVarName, vertexVarLength, edgeVarName, edgeVarLength, direction, start, graph);

  AstNode* pathVar = createNodeVariable(pathVarName, pathVarLength, false);
  node->addMember(pathVar);

  TRI_ASSERT(node->numMembers() == 6);

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

    if (! func->canRunOnDBServer) {
      // this also qualifies a query for potentially reading or modifying documents via function calls!
      _functionsMayAccessDocuments = true;
    }
  }
  else {
    // user-defined function
    node = createNode(NODE_TYPE_FCALL_USER);
    // register the function name
    char* fname = _query->registerString(normalized.first);
    node->setStringValue(fname, normalized.first.size());
      
    _functionsMayAccessDocuments = true;
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
/// @brief get the AST nop node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::getNodeNop () {
  return const_cast<AstNode*>(&NopNode);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST n-ary operator node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeNaryOperator (AstNodeType type) {
  TRI_ASSERT(type == NODE_TYPE_OPERATOR_NARY_AND ||
             type == NODE_TYPE_OPERATOR_NARY_OR);

  return createNode(type);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST n-ary operator node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNodeNaryOperator (AstNodeType type,
                                      AstNode const* child) {
  TRI_ASSERT(type == NODE_TYPE_OPERATOR_NARY_AND ||
             type == NODE_TYPE_OPERATOR_NARY_OR);

  AstNode* node = createNode(type);
  node->addMember(child);

  return node;
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
      size_t const length = node->getStringLength();

      if (param == nullptr) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
      }

      auto it = p.find(std::string(param, length));

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

        // check if the collection was used in a data-modification query
        bool isWriteCollection = false;

        for (auto const& it : _writeCollections) {
          if (it->type == NODE_TYPE_PARAMETER &&
              strcmp(param, it->getStringValue()) == 0) {
            isWriteCollection = true;
            break;
          }
        }

        // turn node into a collection node
        size_t const length = value->_value._string.length - 1;
        char const* name = _query->registerString(value->_value._string.data, length);

        node = createNodeCollection(name, isWriteCollection ? TRI_TRANSACTION_WRITE : TRI_TRANSACTION_READ);

        if (isWriteCollection) {
          // must update AST info now for all nodes that contained this parameter
          for (size_t i = 0; i < _writeCollections.size(); ++i) {
            if (_writeCollections[i]->type == NODE_TYPE_PARAMETER &&
                strcmp(param, _writeCollections[i]->getStringValue()) == 0) {
              _writeCollections[i] = node;
              // no break here. replace all occurrences
            }
          }
        }
      }
      else {
        node = nodeFromJson(value, false);

        if (node != nullptr) {
          // already mark node as constant here
          node->setFlag(DETERMINED_CONSTANT, VALUE_CONSTANT);
          // mark node as simple
          node->setFlag(DETERMINED_SIMPLE, VALUE_SIMPLE);
          // mark node as executable on db-server
          node->setFlag(DETERMINED_RUNONDBSERVER, VALUE_RUNONDBSERVER);
          // mark node as non-throwing
          node->setFlag(DETERMINED_THROWS);
          // mark node as deterministic
          node->setFlag(DETERMINED_NONDETERMINISTIC);
          
          // finally note that the node was created from a bind parameter
          node->setFlag(FLAG_BIND_PARAMETER);
        }
      }
    }

    else if (node->type == NODE_TYPE_BOUND_ATTRIBUTE_ACCESS) {
      // look at second sub-node. this is the (replaced) bind parameter
      auto name = node->getMember(1);

      if (name->type != NODE_TYPE_VALUE ||
          name->value.type != VALUE_TYPE_STRING ||
          name->value.length == 0) {
        // if no string value was inserted for the parameter name, this is an error
        THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, node->getStringValue());
      }
      // convert into a regular attribute access node to simplify handling later
      return createNodeAttributeAccess(node->getMember(0), name->getStringValue(), name->getStringLength());
    }
    else if (node->type == NODE_TYPE_TRAVERSAL) {
      auto graphNode = node->getMember(2);
      if (graphNode->type == NODE_TYPE_VALUE) {
        TRI_ASSERT(graphNode->isStringValue());
        std::string graphName = graphNode->getStringValue();
        auto graph = _query->lookupGraphByName(graphName);
        auto vColls = graph->vertexCollections();
        for (const auto& n: vColls) {
          _query->collections()->add(n, TRI_TRANSACTION_READ);
        }
        auto eColls = graph->edgeCollections();
        for (const auto& n: eColls) {
          _query->collections()->add(n, TRI_TRANSACTION_READ);
        }
      }
    }

    return node;
  };

  _root = traverseAndModify(_root, func, &p); 
 
  // add all collections used in data-modification statements
  for (auto& it : _writeCollections) { 
    if (it->type == NODE_TYPE_COLLECTION) {
      _query->collections()->add(it->getStringValue(), TRI_TRANSACTION_WRITE);
    }
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

  return traverseAndModify(node, visitor, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replace a variable reference in the expression with another 
/// expression (e.g. inserting c = `a + b` into expression `c + 1` so the latter 
/// becomes `a + b + 1`
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::replaceVariableReference (AstNode* node,
                                        Variable const* variable,
                                        AstNode const* expressionNode) {
  auto visitor = [&](AstNode* node, void*) -> AstNode* {
    if (node == nullptr) {
      return nullptr;
    }

    // reference to a variable
    if (node->type == NODE_TYPE_REFERENCE &&
        static_cast<Variable const*>(node->getData()) == variable) {
      // found the target node. now insert the new node
      return const_cast<AstNode*>(expressionNode);
    }
    
    return node;
  };

  return traverseAndModify(node, visitor, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimizes the AST
/// this does not only optimize but also performs a few validations after
/// bind parameter injection. merging this pass with the regular AST 
/// optimizations saves one extra pass over the AST
////////////////////////////////////////////////////////////////////////////////

void Ast::validateAndOptimize () {
  struct TraversalContext {
    std::unordered_set<std::string> writeCollectionsSeen;
    int64_t stopOptimizationRequests    = 0;
    bool isInFilter                     = false;
    bool hasSeenAnyWriteNode            = false;
    bool hasSeenWriteNodeInCurrentScope = false;
  };

  auto preVisitor = [&](AstNode const* node, void* data) -> bool {
    if (node->type == NODE_TYPE_FILTER) {
      static_cast<TraversalContext*>(data)->isInFilter = true;
    }
    else if (node->type == NODE_TYPE_FCALL) {
      auto func = static_cast<Function*>(node->getData());
      TRI_ASSERT(func != nullptr);

      if (func->externalName == "NOOPT") {
        // NOOPT will turn all function optimizations off
        ++(static_cast<TraversalContext*>(data)->stopOptimizationRequests);
      }
    }
    else if (node->hasFlag(FLAG_BIND_PARAMETER)) {
      return false;
    }
    else if (node->type == NODE_TYPE_REMOVE ||
             node->type == NODE_TYPE_INSERT ||
             node->type == NODE_TYPE_UPDATE ||
             node->type == NODE_TYPE_REPLACE ||
             node->type == NODE_TYPE_UPSERT) {
      auto c = static_cast<TraversalContext*>(data);

      if (c->hasSeenWriteNodeInCurrentScope) {
        // no two data-modification nodes are allowed in the same scope
        THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_MULTI_MODIFY);
      }
      c->hasSeenWriteNodeInCurrentScope = true;
    }

    return true;
  };

  auto postVisitor = [&](AstNode const* node, void* data) -> void {
    if (node->type == NODE_TYPE_FILTER) {
      static_cast<TraversalContext*>(data)->isInFilter = false;
    }
    else if (node->type == NODE_TYPE_REMOVE ||
             node->type == NODE_TYPE_INSERT ||
             node->type == NODE_TYPE_UPDATE ||
             node->type == NODE_TYPE_REPLACE ||
             node->type == NODE_TYPE_UPSERT) {
      auto c = static_cast<TraversalContext*>(data);
      c->hasSeenAnyWriteNode = true;

      TRI_ASSERT(c->hasSeenWriteNodeInCurrentScope);
      c->hasSeenWriteNodeInCurrentScope = false;

      auto collection = node->getMember(1);
      auto name = collection->getStringValue();
      c->writeCollectionsSeen.emplace(name);
    }
    else if (node->type == NODE_TYPE_FCALL) {
      auto func = static_cast<Function*>(node->getData());
      TRI_ASSERT(func != nullptr);

      if (func->externalName == "NOOPT") {
        // NOOPT will turn all function optimizations off
        --(static_cast<TraversalContext*>(data)->stopOptimizationRequests);
      }
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

    // passthru node
    if (node->type == NODE_TYPE_PASSTHRU) {
      // optimize away passthru node. this type of node is only used during parsing
      return node->getMember(0);
    }

    // call to built-in function
    if (node->type == NODE_TYPE_FCALL) {
      auto func = static_cast<Function*>(node->getData());

      if (static_cast<TraversalContext*>(data)->hasSeenAnyWriteNode &&
          ! func->canRunOnDBServer) {
        // if canRunOnDBServer is true, then this is an indicator for a 
        // document-accessing function
        THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_ACCESS_AFTER_MODIFICATION);
      }

      if (static_cast<TraversalContext*>(data)->stopOptimizationRequests == 0) {
        // optimization allowed
        return this->optimizeFunctionCall(node);
      }
      // optimization not allowed
      return node;
    }
    
    // reference to a variable, may be able to insert the variable value directly
    if (node->type == NODE_TYPE_REFERENCE) {
      return this->optimizeReference(node);
    }
    
    // indexed access, e.g. a[0] or a['foo']
    if (node->type == NODE_TYPE_INDEXED_ACCESS) {
      return this->optimizeIndexedAccess(node);
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

    // collection
    if (node->type == NODE_TYPE_COLLECTION) {
      char const* name = node->getStringValue();

      auto c = static_cast<TraversalContext*>(data);

      if (c->writeCollectionsSeen.find(name) != c->writeCollectionsSeen.end()) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_ACCESS_AFTER_MODIFICATION);
      }

      return node;
    }
    
    // traversal
    if (node->type == NODE_TYPE_TRAVERSAL) {
      // traversals must not be used after a modification operation
      if (static_cast<TraversalContext*>(data)->hasSeenAnyWriteNode) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_ACCESS_AFTER_MODIFICATION);
      }

      return node;
    }
    
    // example
    if (node->type == NODE_TYPE_EXAMPLE) {
      return this->makeConditionFromExample(node);
    }

    return node;
  };

  // run the optimizations
  TraversalContext context;
  this->_root = traverseAndModify(this->_root, preVisitor, visitor, postVisitor, &context);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief determines the variables referenced in an expression
////////////////////////////////////////////////////////////////////////////////

void Ast::getReferencedVariables (AstNode const* node,
                                  std::unordered_set<Variable const*>& result) {
  auto visitor = [](AstNode const* node, void* data) -> void {
    if (node == nullptr) {
      return;
    }

    // reference to a variable
    if (node->type == NODE_TYPE_REFERENCE) {
      auto variable = static_cast<Variable const*>(node->getData());

      if (variable == nullptr) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
      }

      if (variable->needsRegister()) {
        auto result = static_cast<std::unordered_set<Variable const*>*>(data);
        result->emplace(variable);
      }
    }
  };

  traverseReadOnly(node, visitor, &result); 
}
    
////////////////////////////////////////////////////////////////////////////////
/// @brief determines the top-level attributes referenced in an expression,
/// grouped by variable name
////////////////////////////////////////////////////////////////////////////////

TopLevelAttributes Ast::getReferencedAttributes (AstNode const* node,
                                                 bool& isSafeForOptimization) {
  TopLevelAttributes result;

  auto doNothingVisitor = [](AstNode const* node, void* data) -> void { };
  
  // traversal state
  char const* attributeName = nullptr;
  size_t nameLength = 0;
  isSafeForOptimization = true;
 
  auto visitor = [&](AstNode const* node, void* data) -> void {
    if (node == nullptr) {
      return;
    }

    if (node->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
      attributeName = node->getStringValue();
      nameLength = node->getStringLength();
      return;
    }

    if (node->type == NODE_TYPE_REFERENCE) {
      // reference to a variable
      if (attributeName == nullptr) {
        // we haven't seen an attribute access directly before...
        // this may have been an access to an indexed property, e.g value[0] or a 
        // reference to the complete value, e.g. FUNC(value)
        // note that this is unsafe to optimize this away
        isSafeForOptimization = false;
        return;
      }

      TRI_ASSERT(attributeName != nullptr);
   
      auto variable = static_cast<Variable const*>(node->getData());

      if (variable == nullptr) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
      }

      auto it = result.find(variable);

      if (it == result.end()) { 
        // insert variable and attributeName
        result.emplace(variable, std::unordered_set<std::string>({ std::string(attributeName, nameLength) }));
      }
      else {
        // insert attributeName only
        (*it).second.emplace(std::string(attributeName, nameLength));
      }

      // fall-through
    }
      
    attributeName = nullptr;
    nameLength = 0;
  };

  traverseReadOnly(node, visitor, doNothingVisitor, doNothingVisitor, nullptr); 

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief recursively clone a node
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::clone (AstNode const* node) {
  auto type = node->type;
  if (type == NODE_TYPE_NOP) {
    // nop node is a singleton
    return const_cast<AstNode*>(node);
  }

  auto copy = createNode(type);

  // special handling for certain node types
  // copy payload...
  if (type == NODE_TYPE_COLLECTION ||
      type == NODE_TYPE_PARAMETER ||
      type == NODE_TYPE_ATTRIBUTE_ACCESS ||
      type == NODE_TYPE_OBJECT_ELEMENT ||
      type == NODE_TYPE_FCALL_USER) {
    copy->setStringValue(node->getStringValue(), node->getStringLength());
  }
  else if (type == NODE_TYPE_VARIABLE ||
           type == NODE_TYPE_REFERENCE ||
           type == NODE_TYPE_FCALL) {
    copy->setData(node->getData());
  }
  else if (type == NODE_TYPE_UPSERT ||
           type == NODE_TYPE_EXPANSION) {
    copy->setIntValue(node->getIntValue(true));
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
        copy->setStringValue(node->getStringValue(), node->getStringLength());
        break;
    }
  }

  // recursively clone subnodes
  size_t const n = node->numMembers();
  if (n > 0) {
    copy->members.reserve(n);

    for (size_t i = 0; i < n; ++i) {
      copy->addMember(clone(node->getMemberUnchecked(i)));
    }
  }

  return copy;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deduplicate an array
/// will return the original node if no modifications were made, and a new
/// node if the array contained modifications
////////////////////////////////////////////////////////////////////////////////

AstNode const* Ast::deduplicateArray (AstNode const* node) {
  TRI_ASSERT(node != nullptr);

  if (! node->isArray() || ! node->isConstant()) {
    return node;
  }

  // ok, now we're sure node is a constant array
  size_t const n = node->numMembers();

  if (n <= 1) {
    // nothing to do
    return node;
  }

  // TODO: sort values in place first and compare two adjacent members each

  std::unordered_map<TRI_json_t*, AstNode const*, triagens::basics::JsonHash, triagens::basics::JsonEqual> cache(
    n,
    triagens::basics::JsonHash(), 
    triagens::basics::JsonEqual()
  );

  for (size_t i = 0; i < n; ++i) {
    auto member = node->getMemberUnchecked(i);
    auto json = member->computeJson();
        
    if (cache.find(json) == cache.end()) {
      cache.emplace(json, member);
    }
  }

  // we may have got duplicates. now create a copy of the deduplicated values
  auto copy = createNodeArray();
  copy->members.reserve(cache.size());

  for (auto& it : cache) {
    // TODO: check if the node needs cloning or if we can get away without
    copy->addMember(it.second);
  }
  copy->sort();

  return copy;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check if an operator is reversible
////////////////////////////////////////////////////////////////////////////////
  
bool Ast::IsReversibleOperator (AstNodeType type) {
  return (ReversedOperators.find(static_cast<int>(type)) != ReversedOperators.end());
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

////////////////////////////////////////////////////////////////////////////////
/// @brief get the n-ary operator type equivalent for a binary operator type
////////////////////////////////////////////////////////////////////////////////

AstNodeType Ast::NaryOperatorType (AstNodeType old) {
  TRI_ASSERT(old == NODE_TYPE_OPERATOR_BINARY_AND ||
             old == NODE_TYPE_OPERATOR_BINARY_OR);

  if (old == NODE_TYPE_OPERATOR_BINARY_AND) {
    return NODE_TYPE_OPERATOR_NARY_AND;
  }
  if (old == NODE_TYPE_OPERATOR_BINARY_OR) {
    return NODE_TYPE_OPERATOR_NARY_OR;
  }

  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid node type for n-ary operator");
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief make condition from example
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::makeConditionFromExample (AstNode const* node) {
  TRI_ASSERT(node->numMembers() == 1);

  auto object = node->getMember(0);

  if (object->type != NODE_TYPE_OBJECT) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_PARSE, "expecting object literal for example");
  }

  auto variable = static_cast<AstNode*>(node->getData());

  if (variable == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "internal error in object literal handling");
  }


  AstNode* result = nullptr;
  std::vector<std::pair<char const*, size_t>> attributeParts{ }; 

  std::function<void(AstNode const*)> createCondition = [&] (AstNode const* object) -> void {
    TRI_ASSERT(object->type == NODE_TYPE_OBJECT);

    auto const n = object->numMembers();

    for (size_t i = 0; i < n; ++i) {
      auto member = object->getMember(i); 
   
      if (member->type != NODE_TYPE_OBJECT_ELEMENT) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_PARSE, "expecting object literal with literal attribute names in example");
      }

      attributeParts.emplace_back(std::make_pair(member->getStringValue(), member->getStringLength()));

      auto value = member->getMember(0);

      if (value->type == NODE_TYPE_OBJECT) {
        createCondition(value);
      }
      else {
        auto access = variable;
        for (auto const& it : attributeParts) {
          access = createNodeAttributeAccess(access, it.first, it.second);
        }

        auto condition = createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_EQ, access, value);

        if (result == nullptr) {
          result = condition;
        }
        else {
          // AND-combine with previous condition
          result = createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_AND, result, condition);
        }
      }

      attributeParts.pop_back();
    }
  };

  createCondition(object);

  if (result == nullptr) {
    result = createNodeValueBool(true);
  }

  return result;
}

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
    value = nodeFromJson(result, true);
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
    // right operand of IN or NOT IN must be an array, otherwise we return false
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
    // this can be happen for variables that are specified in the COLLECT...KEEP clause
    return node;
  }

  return static_cast<AstNode*>(variable->constValue());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimizes indexed access, e.g. a[0] or a['foo']
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::optimizeIndexedAccess (AstNode* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->type == NODE_TYPE_INDEXED_ACCESS);
  TRI_ASSERT(node->numMembers() == 2);

  auto index = node->getMember(1);

  if (index->isConstant() &&
      index->type == NODE_TYPE_VALUE &&
      index->value.type == VALUE_TYPE_STRING) {
    // found a string value (e.g. a['foo']). now turn this into
    // an attribute access (e.g. a.foo) in order to make the node qualify
    // for being turned into an index range later
    char const* indexValue = index->getStringValue();

    if (indexValue != nullptr && 
        (indexValue[0] < '0' || indexValue[0] > '9')) {
      // we have to be careful with numeric values here...
      // e.g. array['0'] is not the same as array.0 but must remain a['0'] or (a[0])
      return createNodeAttributeAccess(node->getMember(0), indexValue, index->getStringLength()); 
    }
  } 
 
  // can't optimize when we get here
  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimizes the LET statement 
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::optimizeLet (AstNode* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->type == NODE_TYPE_LET);
  TRI_ASSERT(node->numMembers() >= 2);

  AstNode* variable = node->getMember(0);
  AstNode* expression = node->getMember(1);

  bool const hasCondition = (node->numMembers() > 2);
    
  auto v = static_cast<Variable*>(variable->getData());
  TRI_ASSERT(v != nullptr);

  if (! hasCondition && expression->isConstant()) {
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
/// if copyStringValues is `true`, then string values will be copied and will
/// be freed with the query afterwards. when set to `false`, string values
/// will not be copied and not freed by the query. the caller needs to make
/// sure then that string values are valid through the query lifetime.
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::nodeFromJson (TRI_json_t const* json,
                            bool copyStringValues) {
  TRI_ASSERT(json != nullptr);

  if (json->_type == TRI_JSON_BOOLEAN) {
    return createNodeValueBool(json->_value._boolean);
  }

  if (json->_type == TRI_JSON_NUMBER) {
    return createNodeValueDouble(json->_value._number);
  }

  if (json->_type == TRI_JSON_STRING ||
      json->_type == TRI_JSON_STRING_REFERENCE) {
    size_t const length = json->_value._string.length - 1;

    if (copyStringValues) {
      // we must copy string values!
      char const* value = _query->registerString(json->_value._string.data, length);
      return createNodeValueString(value, length);
    }
    // we can get away without copying string values
    return createNodeValueString(json->_value._string.data, length);
  }

  if (json->_type == TRI_JSON_ARRAY) {
    auto node = createNodeArray();
    size_t const n = TRI_LengthArrayJson(json);
    node->members.reserve(n);

    for (size_t i = 0; i < n; ++i) {
      node->addMember(nodeFromJson(static_cast<TRI_json_t const*>(TRI_AddressVector(&json->_value._objects, i)), copyStringValues)); 
    }

    return node;
  }

  if (json->_type == TRI_JSON_OBJECT) {
    auto node = createNodeObject();
    size_t const n = TRI_LengthVector(&json->_value._objects);

    for (size_t i = 0; i < n; i += 2) {
      auto key = static_cast<TRI_json_t const*>(TRI_AddressVector(&json->_value._objects, i));
      auto value = static_cast<TRI_json_t const*>(TRI_AddressVector(&json->_value._objects, i + 1));

      if (! TRI_IsStringJson(key) || value == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unexpected type found in object node");
      }

      char const* attributeName = key->_value._string.data;
      size_t const nameLength = key->_value._string.length - 1;

      if (copyStringValues) {
        // create a copy of the string value
        attributeName = _query->registerString(key->_value._string.data, nameLength);
      }
      
      node->addMember(createNodeObjectElement(attributeName, nameLength, nodeFromJson(value, copyStringValues)));
    }

    return node;
  }
  
  return createNodeValueNull();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief traverse the AST, using pre- and post-order visitors
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::traverseAndModify (AstNode* node, 
                                 std::function<bool(AstNode const*, void*)> preVisitor,
                                 std::function<AstNode*(AstNode*, void*)> visitor,
                                 std::function<void(AstNode const*, void*)> postVisitor,
                                 void* data) {
  if (node == nullptr) {
    return nullptr;
  }
 
  if (! preVisitor(node, data)) {
    return node;
  }

  size_t const n = node->numMembers();

  for (size_t i = 0; i < n; ++i) {
    auto member = node->getMemberUnchecked(i);

    if (member != nullptr) {
      AstNode* result = traverseAndModify(member, preVisitor, visitor, postVisitor, data);

      if (result != node) {
        TRI_ASSERT_EXPENSIVE(node != nullptr);
        node->changeMember(i, result);
      }
    }
  }

  auto result = visitor(node, data);
  postVisitor(node, data);
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief traverse the AST, using a depth-first visitor
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::traverseAndModify (AstNode* node, 
                                 std::function<AstNode*(AstNode*, void*)> visitor,
                                 void* data) {
  if (node == nullptr) {
    return nullptr;
  }
 
  size_t const n = node->numMembers();

  for (size_t i = 0; i < n; ++i) {
    auto member = node->getMemberUnchecked(i);

    if (member != nullptr) {
      AstNode* result = traverseAndModify(member, visitor, data);

      if (result != node) {
        node->changeMember(i, result);
      }
    }
  }

  return visitor(node, data);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief traverse the AST, using pre- and post-order visitors
////////////////////////////////////////////////////////////////////////////////

void Ast::traverseReadOnly (AstNode const* node, 
                            std::function<void(AstNode const*, void*)> preVisitor,
                            std::function<void(AstNode const*, void*)> visitor,
                            std::function<void(AstNode const*, void*)> postVisitor,
                            void* data) {
  if (node == nullptr) {
    return;
  }
 
  preVisitor(node, data);
  size_t const n = node->numMembers();

  for (size_t i = 0; i < n; ++i) {
    auto member = node->getMemberUnchecked(i);

    if (member != nullptr) {
      traverseReadOnly(member, preVisitor, visitor, postVisitor, data);
    }
  }

  visitor(node, data);
  postVisitor(node, data);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief traverse the AST using a visitor depth-first, with const nodes
////////////////////////////////////////////////////////////////////////////////

void Ast::traverseReadOnly (AstNode const* node, 
                            std::function<void(AstNode const*, void*)> visitor,
                            void* data) {
  if (node == nullptr) {
    return;
  }
  
  size_t const n = node->numMembers();

  for (size_t i = 0; i < n; ++i) {
    auto member = node->getMemberUnchecked(i);

    if (member != nullptr) {
      traverseReadOnly(const_cast<AstNode const*>(member), visitor, data);
    }
  }

  visitor(node, data);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief normalize a function name
////////////////////////////////////////////////////////////////////////////////

std::pair<std::string, bool> Ast::normalizeFunctionName (char const* name) {
  TRI_ASSERT(name != nullptr);

  char* upperName = TRI_UpperAsciiString(TRI_UNKNOWN_MEM_ZONE, name);

  if (upperName == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  std::string functionName(upperName);


  if (functionName.find(':') == std::string::npos) {
    TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, upperName);
    // prepend default namespace for internal functions
    return std::make_pair(functionName, true);
  }
  TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, upperName);

  // user-defined function
  return std::make_pair(functionName, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a node of the specified type
////////////////////////////////////////////////////////////////////////////////

AstNode* Ast::createNode (AstNodeType type) {
  TRI_ASSERT(_query != nullptr);

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

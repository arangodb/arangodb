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
#include "Utils/Exception.h"

using namespace triagens::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the AST
////////////////////////////////////////////////////////////////////////////////

QueryAst::QueryAst () 
  : _nodes(),
    _scopes(),
    _root(nullptr) {

  _nodes.reserve(32);
  _root = createNode(NODE_TYPE_ROOT);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the AST
////////////////////////////////////////////////////////////////////////////////

QueryAst::~QueryAst () {
  for (auto it = _nodes.begin(); it != _nodes.end(); ++it) {
    delete (*it);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the AST
////////////////////////////////////////////////////////////////////////////////

void QueryAst::addOperation (AstNode* node) {
  TRI_ASSERT(_root != nullptr);

  TRI_PushBackVectorPointer(&_root->members, (void*) node);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST scope start node
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::createNodeScopeStart () {
  // TODO: re-add hint?? 
  return createNode(NODE_TYPE_SCOPE_START);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST scope end node
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::createNodeScopeEnd () {
  // TODO: re-add hint?? 
  return createNode(NODE_TYPE_SCOPE_END);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST for node
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::createNodeFor (char const* variableName,
                                  AstNode const* expression) {

// TODO:
/*
  if (! TRI_IsValidVariableNameAql(variableName)) {
    // TRI_SetErrorContextAql(__FILE__, __LINE__, context, TRI_ERROR_QUERY_VARIABLE_NAME_INVALID, name);
  }
*/
  AstNode* node = createNode(NODE_TYPE_FOR);

  AstNode* variable = createNodeVariable(variableName);
  node->addMember(variable);
  node->addMember(expression);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST let node
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::createNodeLet (char const* variableName,
                                  AstNode const* expression) {

// TODO:
/*
  if (! TRI_IsValidVariableNameAql(variableName)) {
    // TRI_SetErrorContextAql(__FILE__, __LINE__, context, TRI_ERROR_QUERY_VARIABLE_NAME_INVALID, name);
  }
*/
  AstNode* node = createNode(NODE_TYPE_LET);

  AstNode* variable = createNodeVariable(variableName);
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

/* TODO
  SetWriteOperation(context, collection, TRI_AQL_QUERY_REMOVE, options);
*/
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
/* TODO
  SetWriteOperation(context, collection, TRI_AQL_QUERY_INSERT, options);
*/
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

/* TODO
  SetWriteOperation(context, collection, TRI_AQL_QUERY_UPDATE, options);
*/
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

/* TODO
  SetWriteOperation(context, collection, TRI_AQL_QUERY_REPLACE, options);
*/
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
    AstNode* variable = createNodeVariable(name);
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
/// @brief create an AST assign node
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::createNodeAssign (char const* name,
                                     AstNode const* expression) {
  // TODO: look up what an assign node is!!!!!!!!!
  AstNode* node = createNode(NODE_TYPE_ASSIGN);
  AstNode* variable = createNodeVariable(name);
  node->addMember(variable);
  node->addMember(expression);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST variable node
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::createNodeVariable (char const* name) {
  // TODO: check for duplicate names here!!
  // TRI_SetErrorContextAql(__FILE__, __LINE__, context, TRI_ERROR_QUERY_VARIABLE_REDECLARED, name);
  AstNode* node = createNode(NODE_TYPE_VARIABLE);
  node->setStringValue(name);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AST collection node
////////////////////////////////////////////////////////////////////////////////

AstNode* QueryAst::createNodeCollection (char const* name) {
  if (name == nullptr || *name == '\0') {
    // TODO
    // TRI_SetErrorContextAql(__FILE__, __LINE__, context, TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND, name);
    return nullptr;
  }
/*
  if (! TRI_IsAllowedNameCollection(true, name)) {
    // TODO
    // TRI_SetErrorContextAql(__FILE__, __LINE__, context, TRI_ERROR_ARANGO_ILLEGAL_NAME, name);
    return nullptr;
  }
*/  
  AstNode* node = createNode(NODE_TYPE_COLLECTION);
  AstNode* nameNode = createNodeValueString(name);

  // TODO: check if we can store the name inline 
  // TODO: add the collection to the query
  node->addMember(nameNode);

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

  // TODO: insert bind parameter name into list of found parameters
  // (so we can check which ones are missing)
  node->setStringValue(name);

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

AstNode* QueryAst::createNodeSubquery () {
  AstNode* node = createNode(NODE_TYPE_SUBQUERY);
  // TODO: let the parser create a dynamic name here
  AstNode* variable = createNodeVariable("tempName");
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
                                     AstNode const* expanded,
                                     AstNode const* expansion) {
  AstNode* node = createNode(NODE_TYPE_EXPAND);
  AstNode* variable = createNodeVariable(variableName);
  // TODO: let the parser create a temporary variable name
  AstNode* temp = createNodeVariable("temp");
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

  // TODO: support function calls!
  TRI_ASSERT(false);
  return nullptr;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

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

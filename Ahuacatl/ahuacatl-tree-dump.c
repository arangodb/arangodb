////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, AST dump functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2012, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Ahuacatl/ahuacatl-tree-dump.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief get the "nice" name of an AST node
////////////////////////////////////////////////////////////////////////////////
      
static char* GetTypeName (const TRI_aql_node_type_e type) {
  switch (type) {
    case AQL_NODE_UNDEFINED:
      assert(false);
      return "undefined";
    case AQL_NODE_MAIN:
      return "main";
    case AQL_NODE_FOR:
      return "for";
    case AQL_NODE_LET:
      return "let";
    case AQL_NODE_FILTER:
      return "filter";
    case AQL_NODE_RETURN:
      return "return";
    case AQL_NODE_COLLECT:
      return "collect";
    case AQL_NODE_SORT:
      return "sort";
    case AQL_NODE_SORT_ELEMENT:
      return "sort element";
    case AQL_NODE_LIMIT:
      return "limit";
    case AQL_NODE_VARIABLE:
      return "variable";
    case AQL_NODE_COLLECTION:
      return "collection";
    case AQL_NODE_REFERENCE:
      return "reference";
    case AQL_NODE_ATTRIBUTE:
      return "attribute";
    case AQL_NODE_ASSIGN:
      return "assign";
    case AQL_NODE_OPERATOR_UNARY_PLUS:
      return "uplus";
    case AQL_NODE_OPERATOR_UNARY_MINUS:
      return "uminus";
    case AQL_NODE_OPERATOR_UNARY_NOT:
      return "unot";
    case AQL_NODE_OPERATOR_BINARY_AND:
      return "and";
    case AQL_NODE_OPERATOR_BINARY_OR:
      return "or";
    case AQL_NODE_OPERATOR_BINARY_PLUS:
      return "plus";
    case AQL_NODE_OPERATOR_BINARY_MINUS:
      return "minus";
    case AQL_NODE_OPERATOR_BINARY_TIMES:
      return "times";
    case AQL_NODE_OPERATOR_BINARY_DIV:
      return "div";
    case AQL_NODE_OPERATOR_BINARY_MOD:
      return "mod";
    case AQL_NODE_OPERATOR_BINARY_EQ:
      return "eq";
    case AQL_NODE_OPERATOR_BINARY_NE:
      return "ne";
    case AQL_NODE_OPERATOR_BINARY_LT:
      return "lt";
    case AQL_NODE_OPERATOR_BINARY_LE:
      return "le";
    case AQL_NODE_OPERATOR_BINARY_GT:
      return "gt";
    case AQL_NODE_OPERATOR_BINARY_GE:
      return "ge";
    case AQL_NODE_OPERATOR_BINARY_IN:
      return "in";
    case AQL_NODE_OPERATOR_TERNARY:
      return "ternary";
    case AQL_NODE_SUBQUERY:
      return "subquery";
    case AQL_NODE_ATTRIBUTE_ACCESS:
      return "attribute access";
    case AQL_NODE_INDEXED:
      return "indexed";
    case AQL_NODE_EXPAND:
      return "expand";
    case AQL_NODE_VALUE:
      return "value";
    case AQL_NODE_LIST:
      return "list";
    case AQL_NODE_ARRAY:
      return "array";
    case AQL_NODE_ARRAY_ELEMENT:
      return "array element";
    case AQL_NODE_PARAMETER:
      return "parameter";
    case AQL_NODE_FCALL:
      return "function call";
  }
 
  printf("unhandled type: %lu\n", (unsigned long) type); 
  assert(false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print some indentation
////////////////////////////////////////////////////////////////////////////////

static void PrintIndent (TRI_aql_dump_t* const state) {
  size_t i;
  
  for (i = 0; i < state->_indent; ++i) {
    printf("  ");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print the type name of an AST node
////////////////////////////////////////////////////////////////////////////////

static void PrintType (TRI_aql_dump_t* const state, const TRI_aql_node_type_e type) {
  PrintIndent(state);
  printf("%s\n", GetTypeName(type));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump a string value from a node
////////////////////////////////////////////////////////////////////////////////

static void DumpString (TRI_aql_dump_t* const state, const TRI_aql_node_t* const node) {
  PrintIndent(state);
  printf("name: %s\n", TRI_AQL_NODE_STRING(node));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump an AST value node's value
////////////////////////////////////////////////////////////////////////////////

static void DumpValue (TRI_aql_dump_t* const state, const TRI_aql_node_t* const node) {
  PrintIndent(state);

  switch (node->_value._type) {
    case AQL_TYPE_FAIL:
      printf("fail\n");
      break;
    case AQL_TYPE_NULL:
      printf("null\n");
      break;
    case AQL_TYPE_BOOL:
      printf("bool (%lu)\n", (unsigned long) TRI_AQL_NODE_BOOL(node));
      break;
    case AQL_TYPE_INT:
      printf("int (%ld)\n", (long) TRI_AQL_NODE_INT(node));
      break;
    case AQL_TYPE_DOUBLE:
      printf("double (%f)\n", TRI_AQL_NODE_DOUBLE(node));
      break;
    case AQL_TYPE_STRING:
      printf("string (%s)\n", TRI_AQL_NODE_STRING(node));
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief increase the indent level by one
////////////////////////////////////////////////////////////////////////////////

static void Indent (void* data) {
  TRI_aql_dump_t* state = (TRI_aql_dump_t*) data;

  ++state->_indent;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief decrease the indent level by one
////////////////////////////////////////////////////////////////////////////////

static void Outdent (void* data) {
  TRI_aql_dump_t* state = (TRI_aql_dump_t*) data;

  assert(state->_indent > 0);
  --state->_indent;
}
        

////////////////////////////////////////////////////////////////////////////////
/// @brief dump an AST node
////////////////////////////////////////////////////////////////////////////////

static void DumpNode (void* data, const TRI_aql_node_t* const node) {
  TRI_aql_dump_t* state = (TRI_aql_dump_t*) data;

  if (!node) {
    return;
  }

  PrintType(state, node->_type);
  Indent(state);

  switch (node->_type) {
    case AQL_NODE_VALUE: 
      DumpValue(state, node);
      break;

    case AQL_NODE_VARIABLE:
    case AQL_NODE_ATTRIBUTE:
    case AQL_NODE_COLLECTION:
    case AQL_NODE_REFERENCE:
    case AQL_NODE_PARAMETER:
    case AQL_NODE_ARRAY_ELEMENT:
    case AQL_NODE_ATTRIBUTE_ACCESS:
    case AQL_NODE_FCALL:
      DumpString(state, node);
      break;

    case AQL_NODE_SORT_ELEMENT:
      PrintIndent(state);
      printf("asc/desc: %lu\n", (unsigned long) TRI_AQL_NODE_BOOL(node));
      break;
    
    default: {
      // nada
    }
  }

  Outdent(state);
}

////////////////////////////////////////////////////////////////////////////////
/// @} 
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief dump AST nodes recursively
////////////////////////////////////////////////////////////////////////////////

void TRI_DumpTreeAql (const TRI_aql_node_t* const node) {
  TRI_aql_const_tree_walker_t* walker;
  TRI_aql_dump_t state;

  state._indent = 0;

  walker = TRI_CreateConstTreeWalkerAql((void*) &state, &DumpNode, NULL, &Indent, &Outdent); 
  if (!walker) {
    return;
  }

  TRI_ConstWalkTreeAql(walker, node); 

  TRI_FreeConstTreeWalkerAql(walker);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

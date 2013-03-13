////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, statement dump functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Ahuacatl/ahuacatl-statement-dump.h"

#include "Ahuacatl/ahuacatl-functions.h"
#include "Ahuacatl/ahuacatl-node.h"
#include "Ahuacatl/ahuacatl-statementlist.h"
#include "Ahuacatl/ahuacatl-statement-walker.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief print some indentation
////////////////////////////////////////////////////////////////////////////////

static void PrintIndent (TRI_aql_dump_t* const state) {
  int64_t i;

  for (i = 0; i < state->_indent; ++i) {
    printf("  ");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print the type name of an AST node
////////////////////////////////////////////////////////////////////////////////

static void PrintType (TRI_aql_dump_t* const state, const TRI_aql_node_type_e type) {
  PrintIndent(state);
  printf("%s\n", TRI_NodeNameAql(type));
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
    case TRI_AQL_TYPE_FAIL:
      printf("fail\n");
      break;
    case TRI_AQL_TYPE_NULL:
      printf("null\n");
      break;
    case TRI_AQL_TYPE_BOOL:
      printf("bool (%lu)\n", (unsigned long) TRI_AQL_NODE_BOOL(node));
      break;
    case TRI_AQL_TYPE_INT:
      printf("int (%ld)\n", (long) TRI_AQL_NODE_INT(node));
      break;
    case TRI_AQL_TYPE_DOUBLE:
      printf("double (%f)\n", TRI_AQL_NODE_DOUBLE(node));
      break;
    case TRI_AQL_TYPE_STRING:
      printf("string (%s)\n", TRI_AQL_NODE_STRING(node));
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief increase the indent level by one
////////////////////////////////////////////////////////////////////////////////

static inline void Indent (TRI_aql_dump_t* const state) {
  ++state->_indent;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief decrease the indent level by one
////////////////////////////////////////////////////////////////////////////////

static inline void Outdent (TRI_aql_dump_t* const state) {

  assert(state->_indent > 0);
  --state->_indent;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump an AST node
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* DumpNode (TRI_aql_statement_walker_t* const walker,
                                 TRI_aql_node_t* const node) {
  TRI_aql_dump_t* state = (TRI_aql_dump_t*) walker->_data;

  if (node == NULL) {
    return node;
  }

  assert(state);

  PrintType(state, node->_type);
  Indent(state);

  switch (node->_type) {
    case TRI_AQL_NODE_VALUE:
      DumpValue(state, node);
      break;

    case TRI_AQL_NODE_VARIABLE:
    case TRI_AQL_NODE_ATTRIBUTE:
    case TRI_AQL_NODE_REFERENCE:
    case TRI_AQL_NODE_PARAMETER:
    case TRI_AQL_NODE_ARRAY_ELEMENT:
    case TRI_AQL_NODE_ATTRIBUTE_ACCESS:
      DumpString(state, node);
      break;
    case TRI_AQL_NODE_FCALL:
      printf("name: %s\n", TRI_GetInternalNameFunctionAql((TRI_aql_function_t*) TRI_AQL_NODE_DATA(node)));
      break;

    case TRI_AQL_NODE_SORT_ELEMENT:
      PrintIndent(state);
      printf("asc/desc: %lu\n", (unsigned long) TRI_AQL_NODE_BOOL(node));
      break;

    default: {
      // nada
    }
  }

  Outdent(state);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump a statement
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* DumpStatementStart (TRI_aql_statement_walker_t* const walker,
                                           TRI_aql_node_t* const node) {
  if (node == NULL) {
    return node;
  }

  assert(walker);
  Indent((TRI_aql_dump_t*) walker->_data);

  return DumpNode(walker, node);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump an AST node
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* DumpStatementEnd (TRI_aql_statement_walker_t* const walker,
                                         TRI_aql_node_t* const node) {
  if (node == NULL) {
    return node;
  }

  assert(walker);
  Outdent((TRI_aql_dump_t*) walker->_data);

  return node;
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
/// @brief dump statement list
////////////////////////////////////////////////////////////////////////////////

void TRI_DumpStatementsAql (TRI_aql_statement_list_t* const statements) {
  TRI_aql_statement_walker_t* walker;
  TRI_aql_dump_t state;

  state._indent = 0;

  walker = TRI_CreateStatementWalkerAql((void*) &state,
                                        false,
                                        &DumpNode,
                                        &DumpStatementStart,
                                        &DumpStatementEnd);
  if (walker == NULL) {
    return;
  }

  TRI_WalkStatementsAql(walker, statements);

  TRI_FreeStatementWalkerAql(walker);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

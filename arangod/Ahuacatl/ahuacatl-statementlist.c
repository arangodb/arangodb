////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, statement list
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

#include "Ahuacatl/ahuacatl-statementlist.h"
#include "Ahuacatl/ahuacatl-scope.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                  private members
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief dummy no-operations node (re-used for ALL non-operations)}
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t DummyNopNode;

////////////////////////////////////////////////////////////////////////////////
/// @brief dummy return node (re-used for ALL remove operations)}
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t DummyReturnNode;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief get the statement at a certain position
////////////////////////////////////////////////////////////////////////////////

static inline TRI_aql_node_t* StatementAt (const TRI_aql_statement_list_t* const list,
                                           const size_t position) {
  return (TRI_aql_node_t*) TRI_AtVectorPointer(&list->_statements, position);
}

////////////////////////////////////////////////////////////////////////////////
/// @} 
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief create and initialize a statement list
////////////////////////////////////////////////////////////////////////////////

TRI_aql_statement_list_t* TRI_CreateStatementListAql (void) {
  TRI_aql_statement_list_t* list;

  list = (TRI_aql_statement_list_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_aql_statement_list_t), false);

  if (list == NULL) {
    return NULL;
  }

  TRI_InitVectorPointer(&list->_statements, TRI_UNKNOWN_MEM_ZONE);
  list->_currentLevel = 0;

  return list;
} 

////////////////////////////////////////////////////////////////////////////////
/// @brief free a statement list
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeStatementListAql (TRI_aql_statement_list_t* const list) {
  if (list == NULL) {
    return;
  }

  TRI_DestroyVectorPointer(&list->_statements);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, list);
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
/// @brief init the no-op node at program start
////////////////////////////////////////////////////////////////////////////////

void TRI_InitStatementListAql (void) {
  DummyNopNode._type = TRI_AQL_NODE_NOP;
  DummyReturnNode._type = TRI_AQL_NODE_RETURN_DUMMY;

  TRI_InitVectorPointer(&DummyNopNode._members, TRI_UNKNOWN_MEM_ZONE);
  TRI_InitVectorPointer(&DummyReturnNode._members, TRI_UNKNOWN_MEM_ZONE);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the address of the dummy non-op node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_GetDummyNopNodeAql (void) {
  return &DummyNopNode;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the address of the remove node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_GetDummyReturnNodeAql (void) {
  return &DummyReturnNode;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a statement to the statement list
////////////////////////////////////////////////////////////////////////////////

bool TRI_AddStatementListAql (TRI_aql_statement_list_t* const list,
                              TRI_aql_node_t* const node) {
  TRI_aql_node_type_e type;
  int result;

  assert(list);
  assert(node);

  type = node->_type;
  assert(TRI_IsTopLevelTypeAql(type));

  if (type == TRI_AQL_NODE_SCOPE_START) {
    ++list->_currentLevel;
  }
  else if (type == TRI_AQL_NODE_SCOPE_END) {
    assert(list->_currentLevel > 0);
    --list->_currentLevel;
  }

  result = TRI_PushBackVectorPointer(&list->_statements, node);

  return result == TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove all non-ops from the statement list
////////////////////////////////////////////////////////////////////////////////

void TRI_CompactStatementListAql (TRI_aql_statement_list_t* const list) {
  size_t i, j, n;

  assert(list);

  i = 0;
  j = 0;
  n = list->_statements._length;

  while (i < n) {
    TRI_aql_node_t* node = StatementAt(list, i);

    if (node->_type == TRI_AQL_NODE_NOP) {
      ++i;
      continue;
    }

    if (node->_type == TRI_AQL_NODE_RETURN_DUMMY) {
      i = TRI_InvalidateStatementListAql(list, i);
      j = i;
      continue; 
    }

    list->_statements._buffer[j++] = node;
    ++i;
  }

  list->_statements._length = j;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief invalidate all statements in current scope and subscopes
////////////////////////////////////////////////////////////////////////////////

size_t TRI_InvalidateStatementListAql (TRI_aql_statement_list_t* const list,
                                       const size_t position) {
  size_t i, n;
  size_t start;
  size_t scopes;

  assert(list);
  assert(position >= 0);
  n = list->_statements._length;

  // remove from position backwards to scope start
  scopes = 1;
  i = position;
  while (true) {
    TRI_aql_node_t* node = StatementAt(list, i);
    TRI_aql_node_type_e type = node->_type;

    list->_statements._buffer[i] = TRI_GetDummyNopNodeAql();
    start = i;

    if (type == TRI_AQL_NODE_SCOPE_START) {
      TRI_aql_scope_t* scope = (TRI_aql_scope_t*) TRI_AQL_NODE_DATA(node);

      if (scope->_type != TRI_AQL_SCOPE_FOR_NESTED) {
        break;
      }
      scopes++;
    }

    if (i-- == 0) {
      break;
    }
  }

  // remove from position forwards to scope end
  i = position;
  while (true) {
    TRI_aql_node_t* node = StatementAt(list, i);
    TRI_aql_node_type_e type = node->_type;
    
    list->_statements._buffer[i] = TRI_GetDummyNopNodeAql();

    if (type == TRI_AQL_NODE_SCOPE_START) {
      ++scopes;
    }
    else if (type == TRI_AQL_NODE_SCOPE_END) {
      assert(scopes > 0);
      if (--scopes == 0) {
        break;
      }
    }

    if (++i == n) {
      break;
    }
  }
  
  list->_statements._buffer[start] = TRI_GetDummyReturnNodeAql();

  return start + 1;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

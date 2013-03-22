////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, statement list
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

#include "Ahuacatl/ahuacatl-statementlist.h"

#include "BasicsC/logging.h"

#include "Ahuacatl/ahuacatl-ast-node.h"
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

static TRI_aql_node_t* DummyNopNode;

////////////////////////////////////////////////////////////////////////////////
/// @brief dummy empty return node (re-used for multiple operations)
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* DummyReturnEmptyNode;

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
/// @brief init the global nodes at program start
////////////////////////////////////////////////////////////////////////////////

void TRI_GlobalInitStatementListAql (void) {
  DummyNopNode = TRI_CreateNodeNopAql();
  DummyReturnEmptyNode = TRI_CreateNodeReturnEmptyAql();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free the global nodes at program end
////////////////////////////////////////////////////////////////////////////////

void TRI_GlobalFreeStatementListAql (void) {
  if (DummyNopNode != NULL) {
    TRI_DestroyVectorPointer(&DummyNopNode->_members);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, DummyNopNode);
  }

  if (DummyReturnEmptyNode != NULL) {
    TRI_aql_node_t* list = TRI_AQL_NODE_MEMBER(DummyReturnEmptyNode, 0);

    TRI_DestroyVectorPointer(&list->_members);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, list);

    TRI_DestroyVectorPointer(&DummyReturnEmptyNode->_members);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, DummyReturnEmptyNode);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the address of the dummy non-op node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_GetDummyNopNodeAql (void) {
  return DummyNopNode;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the address of the dummy return empty node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_GetDummyReturnEmptyNodeAql (void) {
  return DummyReturnEmptyNode;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief pull out uncorrelated subqueries from the middle of the statement
/// list to the beginning
////////////////////////////////////////////////////////////////////////////////

void TRI_PulloutStatementListAql (TRI_aql_statement_list_t* const list) {
  size_t i, n;
  size_t scopes = 0;
  size_t targetScope = 0;
  size_t moveStart = 0;
  bool watch = false;

  assert(list);

  i = 0;
  n = list->_statements._length;

  while (i < n) {
    TRI_aql_node_t* node = StatementAt(list, i);
    TRI_aql_node_type_e type = node->_type;

    if (type == TRI_AQL_NODE_SCOPE_START) {
      // node is a scope start
      TRI_aql_scope_t* scope = (TRI_aql_scope_t*) TRI_AQL_NODE_DATA(node);

      if (scope->_type == TRI_AQL_SCOPE_SUBQUERY && scope->_selfContained) {
        if (! watch && scopes > 0) {
          watch = true;
          targetScope = scopes;
          moveStart = i;
        }
      }

      ++scopes;
    }
    else if (type == TRI_AQL_NODE_SCOPE_END) {
      // node is a scope end
      --scopes;

      if (watch && scopes == targetScope) {
        watch = false;

        node = StatementAt(list, i + 1);

        // check if next statement is a subquery statement
        if (i + 1 < n && node->_type == TRI_AQL_NODE_SUBQUERY) {
          size_t j = moveStart;
          size_t inserted = 0;

          // moving statements from the middle to the beginning of the list will also
          // modify the positions we're moving from
          while (j < i + 2) {
            node = StatementAt(list, j + inserted);
            if (! TRI_InsertStatementListAql(list, node, inserted + 0)) {
              return;
            }

            // insert a dummy node in place of the moved node
            list->_statements._buffer[j + inserted + 1] = TRI_GetDummyNopNodeAql();

            // next
            ++j;
            ++inserted;
          }

          // moving statements from the middle to the beginning of the list will also
          // change the list length and the position we'll be continuing from
          n += inserted;
          i = j + inserted;
        }

      }
    }

    ++i;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a statement to the front of the statement list
////////////////////////////////////////////////////////////////////////////////

bool TRI_InsertStatementListAql (TRI_aql_statement_list_t* const list,
                                 TRI_aql_node_t* const node,
                                 const size_t position) {
  int result;

  assert(list);
  assert(node);

  result = TRI_InsertVectorPointer(&list->_statements, node, position);

  return result == TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a statement to the end of the statement list
////////////////////////////////////////////////////////////////////////////////

bool TRI_AppendStatementListAql (TRI_aql_statement_list_t* const list,
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

    if (node->_type == TRI_AQL_NODE_RETURN_EMPTY) {
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
  size_t ignoreScopes;

  assert(list);
  n = list->_statements._length;

  // walk the scope from the specified position backwards until we find the start of the scope
  scopes = 1;
  ignoreScopes = 0;
  i = position;
  while (true) {
    TRI_aql_node_t* node = StatementAt(list, i);
    TRI_aql_node_type_e type = node->_type;

    list->_statements._buffer[i] = TRI_GetDummyNopNodeAql();
    start = i;

    if (type == TRI_AQL_NODE_SCOPE_START) {
      // node is a scope start
      TRI_aql_scope_t* scope = (TRI_aql_scope_t*) TRI_AQL_NODE_DATA(node);

      if (ignoreScopes > 0) {
        // this is an inner, parallel scope that we can ignore
        --ignoreScopes;
      }
      else {
        if (scope->_type != TRI_AQL_SCOPE_FOR_NESTED) {
          // we have reached the scope and need to stop
          break;
        }
        scopes++;
      }
    }
    else if (type == TRI_AQL_NODE_SCOPE_END) {
      // we found the end of another scope
      // we must remember how many other scopes we passed
      ignoreScopes++;
    }

    if (i-- == 0) {
      break;
    }
  }

  assert(ignoreScopes == 0);

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

  list->_statements._buffer[start] = TRI_GetDummyReturnEmptyNodeAql();

  return start + 1;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

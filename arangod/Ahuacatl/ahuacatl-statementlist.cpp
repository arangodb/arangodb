////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, statement list
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

#include "Ahuacatl/ahuacatl-statementlist.h"

#include "Basics/logging.h"

#include "Ahuacatl/ahuacatl-ast-node.h"
#include "Ahuacatl/ahuacatl-scope.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                  private members
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief dummy no-operations node (re-used for ALL non-operations)}
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* DummyNopNode;

////////////////////////////////////////////////////////////////////////////////
/// @brief dummy empty return node (re-used for multiple operations)
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* DummyReturnEmptyNode;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief get the statement at a certain position
////////////////////////////////////////////////////////////////////////////////

static inline TRI_aql_node_t* StatementAt (const TRI_aql_statement_list_t* const list,
                                           const size_t position) {
  return list->_statements[position];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief invalidates all statements in a scope marked as "empty"
////////////////////////////////////////////////////////////////////////////////

static size_t InvalidateEmptyScope (TRI_aql_statement_list_t* const list,
                                    const size_t position) {
  size_t i, n;
  size_t scopes;

  TRI_ASSERT(list);
  n = list->_statements.size();
  i = position;
  scopes = 0;

  while (i < n) {
    TRI_aql_node_t* node = StatementAt(list, i);
    TRI_aql_node_type_e type = node->_type;

    if (i == position) {
      TRI_ASSERT(type == TRI_AQL_NODE_SCOPE_START);
      list->_statements[i] = DummyReturnEmptyNode;
    }
    else {
      list->_statements[i] = TRI_GetDummyNopNodeAql();
    }
    ++i;

    if (type == TRI_AQL_NODE_SCOPE_START) {
      ++scopes;
    }
    else if (type == TRI_AQL_NODE_SCOPE_END) {
      TRI_ASSERT(scopes > 0);
      --scopes;

      if (scopes == 0) {
        break;
      }
    }

  }

  return i;
}

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create and initialize a statement list
////////////////////////////////////////////////////////////////////////////////

TRI_aql_statement_list_t* TRI_CreateStatementListAql (void) {
  TRI_aql_statement_list_t* list;

  try {
    list = new TRI_aql_statement_list_t();
  }
  catch (std::exception&) {
    list = nullptr;
  }

  if (list == NULL) {
    return NULL;
  }

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

  delete list;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

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
    delete DummyNopNode;
  }

  if (DummyReturnEmptyNode != NULL) {
    TRI_aql_node_t* list = TRI_AQL_NODE_MEMBER(DummyReturnEmptyNode, 0);

    TRI_DestroyVectorPointer(&list->_members);
    delete list;

    TRI_DestroyVectorPointer(&DummyReturnEmptyNode->_members);
    delete DummyReturnEmptyNode;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the address of the dummy non-op node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_GetDummyNopNodeAql (void) {
  return DummyNopNode;
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

  TRI_ASSERT(list);

  i = 0;
  n = list->_statements.size();

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

          node->_type = TRI_AQL_NODE_SUBQUERY_CACHED;

          // moving statements from the middle to the beginning of the list will also
          // modify the positions we're moving from
          while (j < i + 2) {
            node = StatementAt(list, j + inserted);
            if (! TRI_InsertStatementListAql(list, node, inserted + 0)) {
              return;
            }

            // insert a dummy node in place of the moved node
            list->_statements[j + inserted + 1] = TRI_GetDummyNopNodeAql();

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
  TRI_ASSERT(list != NULL);
  TRI_ASSERT(node != NULL);

  try {
    list->_statements.insert(list->_statements.begin() + position, node);
  }
  catch (std::exception&) {
    return false;
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a statement to the end of the statement list
////////////////////////////////////////////////////////////////////////////////

bool TRI_AppendStatementListAql (TRI_aql_statement_list_t* const list,
                                 TRI_aql_node_t* const node) {
  TRI_aql_node_type_e type;

  TRI_ASSERT(list != NULL);
  TRI_ASSERT(node != NULL);

  type = node->_type;
  TRI_ASSERT(TRI_IsTopLevelTypeAql(type));

  if (type == TRI_AQL_NODE_SCOPE_START) {
    ++list->_currentLevel;
  }
  else if (type == TRI_AQL_NODE_SCOPE_END) {
    TRI_ASSERT(list->_currentLevel > 0);
    --list->_currentLevel;
  }

  try {
    list->_statements.push_back(node);
  }
  catch (std::exception&) {
    return false;
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove all non-ops from the statement list
////////////////////////////////////////////////////////////////////////////////

void TRI_CompactStatementListAql (TRI_aql_statement_list_t* const list) {
  size_t i, j, n;

  TRI_ASSERT(list);

  i = 0;
  j = 0;
  n = list->_statements.size();

  while (i < n) {
    TRI_aql_node_t* node = StatementAt(list, i);

    if (node->_type == TRI_AQL_NODE_NOP) {
      ++i;
      continue;
    }

    if (node->_type == TRI_AQL_NODE_SCOPE_START) {
      TRI_aql_scope_t* scope = (TRI_aql_scope_t*) TRI_AQL_NODE_DATA(node);

      if (scope->_empty) {
        i = InvalidateEmptyScope(list, i);
        j = i;
        continue;
      }
    }

    /* should not happen anymore
    if (node->_type == TRI_AQL_NODE_RETURN_EMPTY) {
      TRI_ASSERT(false);
    }
    */

    list->_statements[j++] = node;
    ++i;
  }

  list->_statements.resize(j);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

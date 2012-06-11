////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, statement list walking
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

#include "Ahuacatl/ahuacatl-statement-walker.h"

// -----------------------------------------------------------------------------
// --SECTION--                                             statement list walker
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief process a statement
////////////////////////////////////////////////////////////////////////////////

static void VisitStatement (TRI_aql_statement_walker_t* const walker,
                            const size_t position,
                            TRI_aql_visit_f func) {
  TRI_aql_node_t* node;
  TRI_aql_node_t* modified;

  node = (TRI_aql_node_t*) TRI_AtVectorPointer(&walker->_statements->_statements, position);
  assert(node);

  // handle scopes
  if (node->_type == TRI_AQL_NODE_SCOPE_START) {
    TRI_PushBackVectorPointer(&walker->_currentScopes, TRI_AQL_NODE_DATA(node));
  }
  else if (node->_type == TRI_AQL_NODE_SCOPE_END) {
    size_t n = walker->_currentScopes._length;

    assert(n > 0);
    TRI_RemoveVectorPointer(&walker->_currentScopes, n - 1);
  }

  modified = func(walker, node);
  if (walker->_canModify && modified != node) {
    if (modified == NULL) {
      modified = TRI_GetNopNodeAql();
    }

    walker->_statements->_statements._buffer[position] = modified;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process a node's members
////////////////////////////////////////////////////////////////////////////////

static void VisitMembers (TRI_aql_statement_walker_t* const walker,
                          TRI_aql_node_t* const node) {
  size_t i, n;

  assert(node);

  n = node->_members._length;
  for (i = 0; i < n; ++i) {
    TRI_aql_node_t* member;
    TRI_aql_node_t* modified;
   
    member = (TRI_aql_node_t*) TRI_AtVectorPointer(&node->_members, i);

    if (!member) {
      continue;
    }

    VisitMembers(walker, member);
  
    modified = walker->visitMember(walker, member);
    if (walker->_canModify && modified != member) {
      if (modified == NULL) {
        modified = TRI_GetNopNodeAql();
      }

      node->_members._buffer[i] = modified;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief actually walk the statements in the list 
////////////////////////////////////////////////////////////////////////////////

static void RunWalk (TRI_aql_statement_walker_t* const walker) {
  size_t i, n;

  assert(walker);
  n = walker->_statements->_statements._length;

  for (i = 0; i < n; ++i) {
    TRI_aql_node_t* node;
   
    node = (TRI_aql_node_t*) TRI_AtVectorPointer(&walker->_statements->_statements, i);

    if (!node) {
      continue;
    }

    if (walker->preVisitStatement != NULL) {
      // this might change the node ptr
      VisitStatement(walker, i, walker->preVisitStatement);
      node = walker->_statements->_statements._buffer[i];
    }

    // process node's members first
    if (walker->visitMember != NULL) {
      VisitMembers(walker, node);
    }

    if (walker->postVisitStatement != NULL) {
      VisitStatement(walker, i, walker->postVisitStatement);
    }
  }
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
/// @brief create a statement walker
////////////////////////////////////////////////////////////////////////////////

TRI_aql_statement_walker_t* TRI_CreateStatementWalkerAql (void* data,
                                                          const bool canModify, 
                                                          TRI_aql_visit_f visitMember,
                                                          TRI_aql_visit_f preVisitStatement,
                                                          TRI_aql_visit_f postVisitStatement) {
  TRI_aql_statement_walker_t* walker;

  walker = (TRI_aql_statement_walker_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_aql_statement_walker_t), false);
  if (walker == NULL) {
    return NULL;
  }

  walker->_data = data;
  walker->_canModify = canModify;

  walker->visitMember = visitMember;
  walker->preVisitStatement = preVisitStatement;
  walker->postVisitStatement = postVisitStatement;

  TRI_InitVectorPointer(&walker->_currentScopes, TRI_UNKNOWN_MEM_ZONE);

  return walker;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free a statement walker
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeStatementWalkerAql (TRI_aql_statement_walker_t* const walker) {
  assert(walker);

  TRI_DestroyVectorPointer(&walker->_currentScopes);

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, walker);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief run the statement list walk
////////////////////////////////////////////////////////////////////////////////

void TRI_WalkStatementsAql (TRI_aql_statement_walker_t* const walker, 
                            TRI_aql_statement_list_t* const list) {
  assert(walker);
  assert(list);

  walker->_statements = list; 

  RunWalk(walker);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

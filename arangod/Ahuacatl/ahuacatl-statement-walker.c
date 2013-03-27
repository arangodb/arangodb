////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, statement list walking
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

#include "Ahuacatl/ahuacatl-statement-walker.h"

#include "BasicsC/logging.h"

#include "Ahuacatl/ahuacatl-access-optimiser.h"
#include "Ahuacatl/ahuacatl-scope.h"
#include "Ahuacatl/ahuacatl-variable.h"

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

  modified = func(walker, node);
  if (walker->_canModify && modified != node) {
    if (modified == NULL) {
      modified = TRI_GetDummyNopNodeAql();
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

    if (! member) {
      continue;
    }

    VisitMembers(walker, member);

    modified = walker->visitMember(walker, member);
    if (walker->_canModify && modified != member) {
      if (modified == NULL) {
        modified = TRI_GetDummyNopNodeAql();
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
    TRI_aql_node_type_e nodeType;

    node = (TRI_aql_node_t*) TRI_AtVectorPointer(&walker->_statements->_statements, i);

    if (! node) {
      continue;
    }

    nodeType = node->_type;

    // handle scopes
    if (nodeType == TRI_AQL_NODE_SCOPE_START) {
      TRI_PushBackVectorPointer(&walker->_currentScopes, TRI_AQL_NODE_DATA(node));
    }

    // preprocess node
    if (walker->preVisitStatement != NULL) {
      // this might change the node ptr
      VisitStatement(walker, i, walker->preVisitStatement);
      node = walker->_statements->_statements._buffer[i];
    }


    // process node's members
    if (walker->visitMember != NULL) {
      VisitMembers(walker, node);
    }

    // post process node
    if (walker->postVisitStatement != NULL) {
      VisitStatement(walker, i, walker->postVisitStatement);
    }

    if (nodeType == TRI_AQL_NODE_SCOPE_END) {
      size_t len = walker->_currentScopes._length;

      assert(len > 0);
      TRI_RemoveVectorPointer(&walker->_currentScopes, len - 1);
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
/// @brief mark the scope so it ignores following offset/limit statement
/// optimisations.
/// this is necessary if we find a SORT statement, followed by a LIMIT
/// statement. we must not optimise that because we would modify results then.
////////////////////////////////////////////////////////////////////////////////

void TRI_IgnoreCurrentLimitStatementWalkerAql (TRI_aql_statement_walker_t* const walker) {
  TRI_aql_scope_t* scope = TRI_GetCurrentScopeStatementWalkerAql(walker);

  assert(scope);
  if (scope->_limit._status == TRI_AQL_LIMIT_UNDEFINED) {
    scope->_limit._status = TRI_AQL_LIMIT_IGNORE;
    LOG_TRACE("setting limit status to ignorable");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restrict a offset/limit combination for the top scope to not apply
/// a limit optimisation on collection access, but to apply it on for loop
/// traversal
////////////////////////////////////////////////////////////////////////////////

void TRI_RestrictCurrentLimitStatementWalkerAql (TRI_aql_statement_walker_t* const walker) {
  TRI_aql_scope_t* scope = TRI_GetCurrentScopeStatementWalkerAql(walker);

  assert(scope);
  if (scope->_limit._hasFilter) {
    scope->_limit._status = TRI_AQL_LIMIT_IGNORE;
    LOG_TRACE("limit given up because of additional filter");
  }
  else if (scope->_limit._status == TRI_AQL_LIMIT_UNDEFINED) {
    scope->_limit._hasFilter = true;
    LOG_TRACE("setting limit status to use-in-for-loop");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief note an offset/limit combination for the top scope
////////////////////////////////////////////////////////////////////////////////

void TRI_SetCurrentLimitStatementWalkerAql (TRI_aql_statement_walker_t* const walker,
                                            const int64_t offset,
                                            const int64_t limit) {
  TRI_aql_scope_t* scope = TRI_GetCurrentScopeStatementWalkerAql(walker);

  assert(scope);

  if (scope->_limit._status == TRI_AQL_LIMIT_UNDEFINED) {
    // never overwrite a previously picked up limit
    scope->_limit._limit   = limit;
    scope->_limit._offset  = offset;
    scope->_limit._status  = TRI_AQL_LIMIT_USE;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get current ranges in top scope of statement walker
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t* TRI_GetCurrentRangesStatementWalkerAql (TRI_aql_statement_walker_t* const walker) {
  TRI_aql_scope_t* scope = TRI_GetCurrentScopeStatementWalkerAql(walker);

  assert(scope);

  return scope->_ranges;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set current ranges in top scope of statement walker
////////////////////////////////////////////////////////////////////////////////

void TRI_SetCurrentRangesStatementWalkerAql (TRI_aql_statement_walker_t* const walker,
                                             TRI_vector_pointer_t* ranges) {
  TRI_aql_scope_t* scope = TRI_GetCurrentScopeStatementWalkerAql(walker);

  assert(scope);

  if (ranges != NULL) { // ranges may be NULL
    TRI_vector_pointer_t* oldRanges = scope->_ranges;

    if (oldRanges != NULL) {
      // free old value
      TRI_FreeAccessesAql(oldRanges);
    }

    // set to new value
    scope->_ranges = ranges;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get current top scope of statement walker
////////////////////////////////////////////////////////////////////////////////

TRI_aql_scope_t* TRI_GetCurrentScopeStatementWalkerAql (TRI_aql_statement_walker_t* const walker) {
  size_t n;

  assert(walker);
  n = walker->_currentScopes._length;

  assert(n > 0);

  return (TRI_aql_scope_t*) TRI_AtVectorPointer(&walker->_currentScopes, n - 1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a pointer to a variable, identified by its name
///
/// The variable will be searched in the current and the surrounding scopes.
/// If the variable is not found and the reference to it is coming from a
/// subquery scope, we'll mark the subquery as not self-contained. This prevents
/// moving of the subquery to some other position (which would break the query
/// in this case)
////////////////////////////////////////////////////////////////////////////////

TRI_aql_variable_t* TRI_GetVariableStatementWalkerAql (TRI_aql_statement_walker_t* const walker,
                                                       const char* const name,
                                                       size_t* scopeCount) {
  size_t n;

  // init scope counter to 0
  *scopeCount = 0;

  assert(name != NULL);

  n = walker->_currentScopes._length;
  while (n > 0) {
    TRI_aql_scope_t* scope;
    TRI_aql_variable_t* variable;

    scope = (TRI_aql_scope_t*) TRI_AtVectorPointer(&walker->_currentScopes, --n);
    assert(scope);

    variable = TRI_LookupByKeyAssociativePointer(&scope->_variables, (void*) name);
    if (variable != NULL) {
      return variable;
    }

    if (n == 0 || scope->_type == TRI_AQL_SCOPE_SUBQUERY) {
      // reached the outermost scope
      if (scope->_type == TRI_AQL_SCOPE_SUBQUERY) {
        // variable not found but we reached the end of the scope
        // we must mark the scope as not self-contained so it is not moved to
        // some other position

        scope->_selfContained = false;
      }
      return NULL;
    }

    // increase the scope counter
    (*scopeCount)++;
  }

  // variable not found
  return NULL;
}

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

  walker->_data              = data;
  walker->_canModify         = canModify;

  walker->visitMember        = visitMember;
  walker->preVisitStatement  = preVisitStatement;
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
                            TRI_aql_statement_list_t* list) {
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
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

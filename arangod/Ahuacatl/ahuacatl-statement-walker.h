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

#ifndef TRIAGENS_AHUACATL_AHUACATL_STATEMENT_WALKER_H
#define TRIAGENS_AHUACATL_AHUACATL_STATEMENT_WALKER_H 1

#include "BasicsC/common.h"
#include "BasicsC/vector.h"

#include "Ahuacatl/ahuacatl-ast-node.h"
#include "Ahuacatl/ahuacatl-statementlist.h"

#ifdef __cplusplus
extern "C" {
#endif

struct TRI_aql_scope_s;
struct TRI_aql_variable_s;

// -----------------------------------------------------------------------------
// --SECTION--                                  modifiying statement list walker
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief tree walker container
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_aql_statement_walker_s {
  void* _data;
  bool _canModify;
  TRI_aql_statement_list_t* _statements;
  TRI_vector_pointer_t _currentScopes;

  TRI_aql_node_t* (*visitMember)(struct TRI_aql_statement_walker_s* const, TRI_aql_node_t*);
  TRI_aql_node_t* (*preVisitStatement)(struct TRI_aql_statement_walker_s* const, TRI_aql_node_t*);
  TRI_aql_node_t* (*postVisitStatement)(struct TRI_aql_statement_walker_s* const, TRI_aql_node_t*);
}
TRI_aql_statement_walker_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief typedef for node visitation function
////////////////////////////////////////////////////////////////////////////////

typedef TRI_aql_node_t* (*TRI_aql_visit_f)(TRI_aql_statement_walker_t* const, TRI_aql_node_t*);

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

void TRI_IgnoreCurrentLimitStatementWalkerAql (TRI_aql_statement_walker_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief restrict a offset/limit combination for the top scope to not apply
/// a limit optimisation on collection access, but to apply it on for loop
/// traversal. this is applied when a FILTER statement is found in a scope
/// and that is later followed by a LIMIT statement
////////////////////////////////////////////////////////////////////////////////

void TRI_RestrictCurrentLimitStatementWalkerAql (TRI_aql_statement_walker_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief note an offset/limit combination for the top scope
////////////////////////////////////////////////////////////////////////////////

void TRI_SetCurrentLimitStatementWalkerAql (TRI_aql_statement_walker_t* const,
                                            const int64_t,
                                            const int64_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief get current ranges in top scope of statement walker
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t* TRI_GetCurrentRangesStatementWalkerAql (TRI_aql_statement_walker_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief set current ranges in top scope of statement walker
////////////////////////////////////////////////////////////////////////////////

void TRI_SetCurrentRangesStatementWalkerAql (TRI_aql_statement_walker_t* const,
                                             TRI_vector_pointer_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief get current top scope of statement walker
////////////////////////////////////////////////////////////////////////////////

struct TRI_aql_scope_s* TRI_GetCurrentScopeStatementWalkerAql (TRI_aql_statement_walker_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief return a pointer to a variable, identified by its name
////////////////////////////////////////////////////////////////////////////////

struct TRI_aql_variable_s* TRI_GetVariableStatementWalkerAql (TRI_aql_statement_walker_t* const,
                                                              const char* const,
                                                              size_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief create a statement walker
////////////////////////////////////////////////////////////////////////////////

TRI_aql_statement_walker_t* TRI_CreateStatementWalkerAql (void*,
                                                          const bool,
                                                          TRI_aql_visit_f,
                                                          TRI_aql_visit_f,
                                                          TRI_aql_visit_f);

////////////////////////////////////////////////////////////////////////////////
/// @brief free a statement walker
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeStatementWalkerAql (TRI_aql_statement_walker_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief run the statement list walk
////////////////////////////////////////////////////////////////////////////////

void TRI_WalkStatementsAql (TRI_aql_statement_walker_t* const,
                            TRI_aql_statement_list_t*);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

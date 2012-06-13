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

#ifndef TRIAGENS_DURHAM_AHUACATL_STATEMENTLIST_WALKER_H
#define TRIAGENS_DURHAM_AHUACATL_STATEMENTLIST_WALKER_H 1

#include <BasicsC/common.h>
#include <BasicsC/strings.h>
#include <BasicsC/vector.h>

#include "Ahuacatl/ahuacatl-access-optimiser.h"
#include "Ahuacatl/ahuacatl-ast-node.h"
#include "Ahuacatl/ahuacatl-context.h"
#include "Ahuacatl/ahuacatl-scope.h"
#include "Ahuacatl/ahuacatl-statementlist.h"
#include "Ahuacatl/ahuacatl-variable.h"

#ifdef __cplusplus
extern "C" {
#endif

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

TRI_aql_scope_t* TRI_GetCurrentScopeStatementWalkerAql (TRI_aql_statement_walker_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief get current scopes of statement walker
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t* TRI_GetScopesStatementWalkerAql (TRI_aql_statement_walker_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief return a pointer to a variable, identified by its name
////////////////////////////////////////////////////////////////////////////////

TRI_aql_variable_t* TRI_GetVariableStatementWalkerAql (TRI_aql_statement_walker_t* const,
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
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

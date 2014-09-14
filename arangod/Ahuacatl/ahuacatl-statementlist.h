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

#ifndef ARANGODB_AHUACATL_AHUACATL__STATEMENTLIST_H
#define ARANGODB_AHUACATL_AHUACATL__STATEMENTLIST_H 1

#include "Basics/Common.h"
#include "Basics/vector.h"

struct TRI_aql_node_t;

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

struct TRI_aql_statement_list_t {
  std::vector<struct TRI_aql_node_t*> _statements;
  size_t _currentLevel;
};

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief init the global nodes at program start
////////////////////////////////////////////////////////////////////////////////

void TRI_GlobalInitStatementListAql (void);

////////////////////////////////////////////////////////////////////////////////
/// @brief de-init the global nodes at program end
////////////////////////////////////////////////////////////////////////////////

void TRI_GlobalFreeStatementListAql (void);

////////////////////////////////////////////////////////////////////////////////
/// @brief create and initialize a statement list
////////////////////////////////////////////////////////////////////////////////

TRI_aql_statement_list_t* TRI_CreateStatementListAql (void);

////////////////////////////////////////////////////////////////////////////////
/// @brief free a statement list
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeStatementListAql (TRI_aql_statement_list_t* const);

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief get the address of the dummy non-op node
////////////////////////////////////////////////////////////////////////////////

struct TRI_aql_node_t* TRI_GetDummyNopNodeAql (void);

////////////////////////////////////////////////////////////////////////////////
/// @brief pull out subqueries in the statement list from the middle to the
/// beginning
////////////////////////////////////////////////////////////////////////////////

void TRI_PulloutStatementListAql (TRI_aql_statement_list_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief remove all non-ops from the statement list
///
/// this is achieved by skipping over all nop nodes in the statement list
/// the resulting statement list will contain the remaining nodes only
////////////////////////////////////////////////////////////////////////////////

void TRI_CompactStatementListAql (TRI_aql_statement_list_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief insert a statement into the statement list
////////////////////////////////////////////////////////////////////////////////

bool TRI_InsertStatementListAql (TRI_aql_statement_list_t* const,
                                 struct TRI_aql_node_t* const,
                                 const size_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief add a statement to the end of the statement list
////////////////////////////////////////////////////////////////////////////////

bool TRI_AppendStatementListAql (TRI_aql_statement_list_t* const,
                                 struct TRI_aql_node_t* const);

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

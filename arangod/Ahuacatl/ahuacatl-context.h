////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, query context
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

#ifndef TRIAGENS_AHUACATL_AHUACATL_CONTEXT_H
#define TRIAGENS_AHUACATL_AHUACATL_CONTEXT_H 1

#include "BasicsC/common.h"
#include "BasicsC/vector.h"
#include "BasicsC/associative.h"

#include "Ahuacatl/ahuacatl-error.h"
#include "Ahuacatl/ahuacatl-statementlist.h"

#ifdef __cplusplus
extern "C" {
#endif

struct TRI_aql_parser_s;
struct TRI_json_s;
struct TRI_vocbase_s;

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief the context for parsing a query
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_aql_context_s {
  struct TRI_vocbase_s* _vocbase;
  struct TRI_aql_parser_s* _parser;
  TRI_aql_statement_list_t* _statements;
  TRI_aql_error_t _error;
  TRI_vector_pointer_t _collections;
  TRI_associative_pointer_t _collectionNames;
  struct {
    TRI_vector_pointer_t _nodes;
    TRI_vector_pointer_t _strings;
    TRI_vector_pointer_t _scopes;
  }
  _memory;
  TRI_vector_pointer_t _currentScopes;
  struct {
    TRI_associative_pointer_t _values;
    TRI_associative_pointer_t _names;
  }
  _parameters;

  const char* _query;

  size_t _variableIndex;
  size_t _scopeIndex;
}
TRI_aql_context_t;

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
/// @brief create and initialize a context
////////////////////////////////////////////////////////////////////////////////

TRI_aql_context_t* TRI_CreateContextAql (struct TRI_vocbase_s*,
                                         const char* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief free a context
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeContextAql (TRI_aql_context_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief parse & validate the query string
////////////////////////////////////////////////////////////////////////////////

bool TRI_ValidateQueryContextAql (TRI_aql_context_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief add bind parameters to the query context
////////////////////////////////////////////////////////////////////////////////

bool TRI_BindQueryContextAql (TRI_aql_context_t* const,
                              const struct TRI_json_s* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief perform some AST optimisations
////////////////////////////////////////////////////////////////////////////////

bool TRI_OptimiseQueryContextAql (TRI_aql_context_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief set up all collections used in the query
////////////////////////////////////////////////////////////////////////////////

bool TRI_SetupCollectionsContextAql (TRI_aql_context_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief register a string
////////////////////////////////////////////////////////////////////////////////

char* TRI_RegisterStringAql (TRI_aql_context_t* const,
                             const char* const,
                             const size_t,
                             const bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief register a node
////////////////////////////////////////////////////////////////////////////////

bool TRI_RegisterNodeContextAql (TRI_aql_context_t* const,
                                 void* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief register an error
////////////////////////////////////////////////////////////////////////////////

void TRI_SetErrorContextAql (TRI_aql_context_t* const,
                             const int,
                             const char* const);

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

////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, variables
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

#ifndef ARANGODB_AHUACATL_AHUACATL__VARIABLE_H
#define ARANGODB_AHUACATL_AHUACATL__VARIABLE_H 1

#include "Basics/Common.h"
#include "Basics/tri-strings.h"
#include "Basics/hashes.h"
#include "Basics/vector.h"
#include "Basics/associative.h"

#include "Ahuacatl/ahuacatl-ast-node.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief a query variable
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_aql_variable_s {
  char* _name;
  TRI_aql_node_t* _definingNode;
  bool _isUpdated;
}
TRI_aql_variable_t;

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief register a new variable
////////////////////////////////////////////////////////////////////////////////

TRI_aql_variable_t* TRI_CreateVariableAql (char const*,
                                           TRI_aql_node_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief free an existing variable
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeVariableAql (TRI_aql_variable_t*);

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief hash variable
////////////////////////////////////////////////////////////////////////////////

uint64_t TRI_HashVariableAql (TRI_associative_pointer_t*, void const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief comparison function used to determine variable equality
////////////////////////////////////////////////////////////////////////////////

bool TRI_EqualVariableAql (TRI_associative_pointer_t*, void const*, void const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a variable name follows the required naming convention
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsValidVariableNameAql (char const*);

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

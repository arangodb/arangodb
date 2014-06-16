////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, variables
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

#include "Ahuacatl/ahuacatl-variable.h"

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief register a new variable
////////////////////////////////////////////////////////////////////////////////

TRI_aql_variable_t* TRI_CreateVariableAql (char const* name,
                                           TRI_aql_node_t* definingNode) {
  TRI_aql_variable_t* variable;

  variable = (TRI_aql_variable_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_aql_variable_t), false);

  if (variable == nullptr) {
    return nullptr;
  }

  variable->_name = TRI_DuplicateStringZ(TRI_UNKNOWN_MEM_ZONE, name);

  if (variable->_name == nullptr) {
    TRI_FreeVariableAql(variable);
    return nullptr;
  }

  variable->_definingNode = definingNode;
  variable->_isUpdated = false;

  TRI_ASSERT(definingNode);

  return variable;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free an existing variable
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeVariableAql (TRI_aql_variable_t* variable) {
  TRI_ASSERT(variable != nullptr);

  if (variable->_name != nullptr) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, variable->_name);
  }

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, variable);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief hash variable
////////////////////////////////////////////////////////////////////////////////

uint64_t TRI_HashVariableAql (TRI_associative_pointer_t* array,
                              void const* element) {
  TRI_aql_variable_t* variable = (TRI_aql_variable_t*) element;

  return TRI_FnvHashString(variable->_name);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief comparison function used to determine variable equality
////////////////////////////////////////////////////////////////////////////////

bool TRI_EqualVariableAql (TRI_associative_pointer_t* array,
                           void const* key,
                           void const* element) {
  TRI_aql_variable_t* variable = (TRI_aql_variable_t*) element;

  return TRI_EqualString(static_cast<char const*>(key), variable->_name);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a variable name follows the required naming convention
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsValidVariableNameAql (char const* name) {
  TRI_ASSERT(name != nullptr);

  if (strlen(name) == 0) {
    // name must be at least one char long
    return false;
  }

  if (*name == '_') {
    // name must not start with an underscore
    return false;
  }

  // everything else is allowed
  return true;
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

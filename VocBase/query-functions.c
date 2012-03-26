////////////////////////////////////////////////////////////////////////////////
/// @brief In-query functions
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

#include "VocBase/query-functions.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief Hash function used to hash function name
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashName (TRI_associative_pointer_t* array, 
                          void const* key) {
  return TRI_FnvHashString((char const*) key);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Hash function used to hash function struct
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashFunction (TRI_associative_pointer_t* array, 
                              void const* element) {
  TRI_query_function_t* function = (TRI_query_function_t*) element;

  return TRI_FnvHashString(function->_name);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Comparison function used to determine function name equality
////////////////////////////////////////////////////////////////////////////////

static bool EqualName (TRI_associative_pointer_t* array, 
                       void const* key, 
                       void const* element) {
  TRI_query_function_t* function = (TRI_query_function_t*) element;

  return TRI_EqualString(key, function->_name);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise the array with the function declarations 
////////////////////////////////////////////////////////////////////////////////

TRI_associative_pointer_t* TRI_InitialiseQueryFunctions (void) {
  TRI_associative_pointer_t* functions = 
    (TRI_associative_pointer_t*) TRI_Allocate(sizeof(TRI_associative_pointer_t)); 

  if (!functions) {
    return NULL;
  }

  TRI_InitAssociativePointer(functions, HashName, HashFunction, EqualName, 0); 
  TRI_RegisterQueryFunction(functions, "AQL_IS_UNDEFINED", 1, 1);

  return functions;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function name
////////////////////////////////////////////////////////////////////////////////

bool TRI_RegisterQueryFunction (TRI_associative_pointer_t* functions,
                                char* functionName, 
                                int minArgs, 
                                int maxArgs) {
  TRI_query_function_t* function = 
    (TRI_query_function_t*) TRI_Allocate(sizeof(TRI_query_function_t));

  if (!function) {
    return false;
  }

  function->_name = TRI_DuplicateString(functionName);
  if (!function->_name) {
    TRI_Free(function);
    return false;
  }
  function->_minArgs = minArgs; 
  function->_maxArgs = maxArgs; 

  if (TRI_InsertKeyAssociativePointer(functions, functionName, function, false)) {
    TRI_Free(function->_name);
    TRI_Free(function);
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free the array with the function declarations 
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeQueryFunctions (TRI_associative_pointer_t* functions) {
  TRI_query_function_t* function;
  size_t i;

  for (i = 0; i < functions->_nrAlloc; ++i) {
    function = (TRI_query_function_t*) functions->_table[i];
    if (!function) {
      continue;
    }

    if (function->_name) {
      TRI_Free(function->_name);
    }
    TRI_Free(function);
  }

  TRI_DestroyAssociativePointer(functions);
  TRI_Free(functions);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

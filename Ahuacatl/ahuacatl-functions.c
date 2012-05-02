////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, query language functions
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

#include "Ahuacatl/ahuacatl-functions.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                    private macros
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief shorthand to register a query function and process the result
////////////////////////////////////////////////////////////////////////////////

#define REGISTER_FUNCTION(internalName, externalName, deterministic, group, minArgs, maxArgs) \
  result &= TRI_RegisterFunctionAql (functions, internalName, "AHUACATL_" externalName, deterministic, group, minArgs, maxArgs);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief hash function used to hash function struct
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashFunction (TRI_associative_pointer_t* array, 
                              void const* element) {
  TRI_aql_function_t* function = (TRI_aql_function_t*) element;

  return TRI_FnvHashString(function->_externalName);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief comparison function used to determine function name equality
////////////////////////////////////////////////////////////////////////////////

static bool EqualName (TRI_associative_pointer_t* array, 
                       void const* key, 
                       void const* element) {
  TRI_aql_function_t* function = (TRI_aql_function_t*) element;

  return TRI_EqualString(key, function->_externalName);
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
/// @brief initialise the array with the function declarations 
////////////////////////////////////////////////////////////////////////////////

TRI_associative_pointer_t* TRI_InitialiseFunctionsAql (void) {
  TRI_associative_pointer_t* functions;
  bool result = true;
  
  functions = (TRI_associative_pointer_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_associative_pointer_t), false); 

  if (!functions) {
    return NULL;
  }

  TRI_InitAssociativePointer(functions, 
                             TRI_UNKNOWN_MEM_ZONE, 
                             TRI_HashStringKeyAssociativePointer, 
                             HashFunction, 
                             EqualName, 
                             NULL); 

  // cast functions
  REGISTER_FUNCTION("TONUMBER", "CAST_NUMBER", true, false, 1, 1);
  REGISTER_FUNCTION("TOSTRING", "CAST_STRING", true, false, 1, 1);
  REGISTER_FUNCTION("TOBOOL", "CAST_BOOL", true, false, 1, 1);
  REGISTER_FUNCTION("TONULL", "CAST_NULL", true, false, 1, 1);

  // string concat
  REGISTER_FUNCTION("CONCAT", "STRING_CONCAT", true, false, 2, 256); 

  // type check functions
  REGISTER_FUNCTION("ISNULL", "IS_NULL", true, false, 1, 1);
  REGISTER_FUNCTION("ISBOOL", "IS_BOOL", true, false, 1, 1);
  REGISTER_FUNCTION("ISNUMBER", "IS_NUMBER", true, false, 1, 1);
  REGISTER_FUNCTION("ISSTRING", "IS_STRING", true, false, 1, 1);
  REGISTER_FUNCTION("ISLIST", "IS_LIST", true, false, 1, 1);
  REGISTER_FUNCTION("ISDOCUMENT", "IS_DOCUMENT", true, false, 1, 1);

  // numeric functions 
  REGISTER_FUNCTION("FLOOR", "NUMBER_FLOOR", true, false, 1, 1);
  REGISTER_FUNCTION("CEIL", "NUMBER_CEIL", true, false, 1, 1);
  REGISTER_FUNCTION("ROUND", "NUMBER_ROUND", true, false, 1, 1);
  REGISTER_FUNCTION("ABS", "NUMBER_ABS", true, false, 1, 1);
  REGISTER_FUNCTION("RAND", "NUMBER_RAND", false, false, 0, 0);
  
  // string functions
  
  // misc functions
  REGISTER_FUNCTION("MERGE", "MERGE", true, false, 2, 256);
  REGISTER_FUNCTION("UNION", "UNION", true, false, 2, 256);
  REGISTER_FUNCTION("LENGTH", "LENGTH", true, true, 1, 1);
  REGISTER_FUNCTION("MIN", "MIN", true, true, 1, 256);
  REGISTER_FUNCTION("MAX", "MAX", true, true, 1, 256);

  if (!result) {
    TRI_FreeFunctionsAql(functions);
    return NULL;
  }

  return functions;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free the array with the function declarations 
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeFunctionsAql (TRI_associative_pointer_t* functions) {
  size_t i;

  for (i = 0; i < functions->_nrAlloc; ++i) {
    TRI_aql_function_t* function = (TRI_aql_function_t*) functions->_table[i];
    if (!function) {
      continue;
    }

    TRI_Free(TRI_UNKNOWN_MEM_ZONE, function->_externalName);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, function->_internalName);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, function);
  }

  TRI_DestroyAssociativePointer(functions);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, functions);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a pointer to a function by name
////////////////////////////////////////////////////////////////////////////////

TRI_aql_function_t* TRI_GetFunctionAql (TRI_associative_pointer_t* functions,
                                        const char* const internalName) {
  TRI_aql_function_t* function;
  
  function = (TRI_aql_function_t*) TRI_LookupByKeyAssociativePointer(functions, (void*) internalName);
  assert(function);

  return function;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a function name is valid
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsValidFunctionAql (TRI_associative_pointer_t* functions,
                             const char* const externalName) {
  if (TRI_LookupByKeyAssociativePointer(functions, externalName)) {
    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get internal function name for an external one
////////////////////////////////////////////////////////////////////////////////

const char* TRI_GetInternalNameFunctionAql (const TRI_aql_function_t* const function) {
  return function->_internalName;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function name
////////////////////////////////////////////////////////////////////////////////

bool TRI_RegisterFunctionAql (TRI_associative_pointer_t* functions,
                              const char* const externalName, 
                              const char* const internalName, 
                              const bool isDeterministic,
                              const bool isGroup,
                              const int minArgs, 
                              const int maxArgs) {
  TRI_aql_function_t* function;
  
  function = (TRI_aql_function_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_aql_function_t), false);

  if (!function) {
    return false;
  }

  function->_externalName = TRI_DuplicateString(externalName);
  if (!function->_externalName) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, function);
    return false;
  }

  // normalize name by upper-casing it
  function->_internalName = TRI_UpperAsciiString(internalName);
  if (!function->_internalName) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, function->_externalName);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, function);
    return false;
  }

  function->_isDeterministic = isDeterministic; 
  function->_isGroup = isGroup; 
  function->_minArgs = minArgs; 
  function->_maxArgs = maxArgs; 

  if (TRI_InsertKeyAssociativePointer(functions, externalName, function, false)) {
    // function already registered
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, function->_externalName);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, function->_internalName);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, function);
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

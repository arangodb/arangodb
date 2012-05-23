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

#define REGISTER_FUNCTION(internalName, externalName, deterministic, group, argPattern) \
  result &= TRI_RegisterFunctionAql(functions, internalName, "AHUACATL_" externalName, deterministic, group, argPattern);

////////////////////////////////////////////////////////////////////////////////
/// @brief shorthand to check an argument and return an error if it is invalid
////////////////////////////////////////////////////////////////////////////////

#define ARG_CHECK                                                                                                   \
  if (!CheckArgumentType(parameter, &allowed)) {                                                                    \
    TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, function->_externalName);      \
    return false;                                                                                                   \
  }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief parameter type holder
////////////////////////////////////////////////////////////////////////////////

typedef struct param_s {
  bool _null       : 1;
  bool _bool       : 1;
  bool _number     : 1;
  bool _string     : 1;
  bool _list       : 1;
  bool _array      : 1;
  bool _collection : 1;
} 
param_t;

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
/// @brief return a param_t structure with all bits set to 0
////////////////////////////////////////////////////////////////////////////////

static param_t InitParam (void) {
  param_t param;

  param._null = false;
  param._bool = false;
  param._number = false;
  param._string = false;
  param._list = false;
  param._array = false;
  param._collection = false;

  return param;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief check the type of an argument for a function call
////////////////////////////////////////////////////////////////////////////////

static bool CheckArgumentType (TRI_aql_node_t* parameter, 
                               const param_t* const allowed) {
  param_t found = InitParam(); 

  if (parameter->_type == AQL_NODE_PARAMETER) {
    // node is a bind parameter
    char* name = TRI_AQL_NODE_STRING(parameter);

    if (*name == '@') {
      // collection bind parameter. this is an error
      found._collection = true;
      found._list = true; // a collection is a list of documents
    }
    else {
      // regular bind parameter
      found._null = true;
      found._bool = true;
      found._number = true;
      found._string = true;
      found._list = true;
      found._array = true;
    }
  }
  else if (parameter->_type == AQL_NODE_VALUE) {
    switch (parameter->_value._type) {
      case AQL_TYPE_FAIL:
      case AQL_TYPE_NULL:
        found._null = true;
        break;
      case AQL_TYPE_BOOL:
        found._bool = true;
        break;
      case AQL_TYPE_INT:
      case AQL_TYPE_DOUBLE:
        found._number = true;
        break;
      case AQL_TYPE_STRING:
        found._string = true;
        break;
    }
  }
  else if (parameter->_type == AQL_NODE_LIST) {
    // actual parameter is a list
    found._list = true;
  }
  else if (parameter->_type == AQL_NODE_ARRAY) {
    // actual parameter is an array
    found._array = true;
  }
  else if (parameter->_type == AQL_NODE_COLLECTION) {
    // actual parameter is a collection
    found._collection = true;
    found._list = true; // a collection is a list of documents
  }
  else {
    // we cannot yet determine the type of the parameter
    // this is the case if the argument is an expression, a function call etc.

    if (!allowed->_collection) {
      // if we do require anything else but a collection, we don't know the
      // type and must exit here
      return true;
    }

    // if we require a collection, it must be passed in a form that we know
    // the collection name at parse time. otherwise, an error will be raised
  }


  if (allowed->_null && found._null) {
    // argument is a null value, and this is allowed
    return true;
  }

  if (allowed->_bool && found._bool) {
    // argument is a bool value, and this is allowed
    return true;
  }
  
  if (allowed->_number && found._number) {
    // argument is a numeric value, and this is allowed
    return true;
  }
  
  if (allowed->_string && found._string) {
    // argument is a string value, and this is allowed
    return true;
  }
  
  if (allowed->_list && found._list) {
    // argument is a list, and this is allowed
    return true;
  }

  if (allowed->_array && found._array) {
    // argument is an array, and this is allowed
    return true;
  }

  if (allowed->_collection && found._collection) {
    // argument is a collection, and this is allowed
    return true;
  }

  return false;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief determine minimum and maximum argument number for argument pattern
////////////////////////////////////////////////////////////////////////////////

static void SetArgumentCount (TRI_aql_function_t* const function) {
  const char* pattern;
  char c;
  size_t minArgs = 0;
  size_t maxArgs = 0;
  bool inOptional = false;
  bool foundArg = false;
  bool parse = true;

  assert(function);
 
  pattern = function->_argPattern;
  while (parse) {
    c = *pattern++;

    switch (c) {
      case '\0':
        if (foundArg) {
          if (!inOptional) {
            ++minArgs;
          }
          ++maxArgs;
        }
        parse = false;
        break;
      case '|':
        assert(!inOptional);
        if (foundArg) {
          ++minArgs;
          ++maxArgs;
        }
        inOptional = true;
        foundArg = false;
        break;
      case ',':
        assert(foundArg);
        if (!inOptional) {
          ++minArgs;
        }
        ++maxArgs;
        foundArg = false;
        break;
      case '+':
        assert(inOptional);
        maxArgs = 256;
        parse = false;
        break;
      default:
        foundArg = true;
    }
  }

  function->_minArgs = minArgs;
  function->_maxArgs = maxArgs;
}

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

  // . = argument of any type (except collection)
  // c = collection name, will be converted into list with documents
  // h = collection name, will be converted into string
  // z = null
  // b = bool
  // n = number
  // s = string
  // p = primitive
  // l = list
  // a = array

  // cast functions
  REGISTER_FUNCTION("TONUMBER", "CAST_NUMBER", true, false, ".");
  REGISTER_FUNCTION("TOSTRING", "CAST_STRING", true, false, ".");
  REGISTER_FUNCTION("TOBOOL", "CAST_BOOL", true, false, ".");
  REGISTER_FUNCTION("TONULL", "CAST_NULL", true, false, ".");

  // string functions
  REGISTER_FUNCTION("CONCAT", "STRING_CONCAT", true, false, "sz,sz|+"); 
  REGISTER_FUNCTION("CONCATSEPARATOR", "STRING_CONCAT_SEPARATOR", true, false, "s,sz,sz|+"); 
  REGISTER_FUNCTION("CHARLENGTH", "STRING_LENGTH", true, false, "s"); 
  REGISTER_FUNCTION("LOWER", "STRING_LOWER", true, false, "s"); 
  REGISTER_FUNCTION("UPPER", "STRING_UPPER", true, false, "s"); 
  REGISTER_FUNCTION("SUBSTRING", "STRING_SUBSTRING", true, false, "s,n|n");

  // type check functions
  REGISTER_FUNCTION("ISNULL", "IS_NULL", true, false, ".");
  REGISTER_FUNCTION("ISBOOL", "IS_BOOL", true, false, ".");
  REGISTER_FUNCTION("ISNUMBER", "IS_NUMBER", true, false, ".");
  REGISTER_FUNCTION("ISSTRING", "IS_STRING", true, false, ".");
  REGISTER_FUNCTION("ISLIST", "IS_LIST", true, false, ".");
  REGISTER_FUNCTION("ISDOCUMENT", "IS_DOCUMENT", true, false, ".");

  // numeric functions 
  REGISTER_FUNCTION("FLOOR", "NUMBER_FLOOR", true, false, "n");
  REGISTER_FUNCTION("CEIL", "NUMBER_CEIL", true, false, "n");
  REGISTER_FUNCTION("ROUND", "NUMBER_ROUND", true, false, "n");
  REGISTER_FUNCTION("ABS", "NUMBER_ABS", true, false, "n");
  REGISTER_FUNCTION("RAND", "NUMBER_RAND", false, false, "");

  // geo functions
  REGISTER_FUNCTION("NEAR", "GEO_NEAR", false, false, "h,n,n,n|s");
  REGISTER_FUNCTION("WITHIN", "GEO_WITHIN", false, false, "h,n,n,n|s");

  // graph functions
  REGISTER_FUNCTION("PATHS", "GRAPH_PATHS", false, false, "c,h|s,b");
  
  // misc functions
  REGISTER_FUNCTION("FAIL", "FAIL", false, false, "|s"); // FAIL is non-deterministic, otherwise query optimisation will fail!
  REGISTER_FUNCTION("PASSTHRU", "PASSTHRU", false, false, "."); // simple non-deterministic wrapper to avoid optimisations at parse time
  REGISTER_FUNCTION("COLLECTIONS", "COLLECTIONS", false, false, ""); 
  REGISTER_FUNCTION("HAS", "HAS", true, false, "az,s"); 

  REGISTER_FUNCTION("MERGE", "MERGE", true, false, "a,a|+");

  // list functions
  REGISTER_FUNCTION("UNION", "UNION", true, false, "l,l|+");
  REGISTER_FUNCTION("LENGTH", "LENGTH", true, true, "l");
  REGISTER_FUNCTION("MIN", "MIN", true, true, "l");
  REGISTER_FUNCTION("MAX", "MAX", true, true, "l");
  REGISTER_FUNCTION("SUM", "SUM", true, true, "l");
  REGISTER_FUNCTION("UNIQUE", "UNIQUE", true, false, "l");
  REGISTER_FUNCTION("REVERSE", "REVERSE", true, false, "l");
  REGISTER_FUNCTION("FIRST", "FIRST", true, false, "l");
  REGISTER_FUNCTION("LAST", "LAST", true, false, "l");

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
/// @brief return a function, looked up by its external name
////////////////////////////////////////////////////////////////////////////////

TRI_aql_function_t* TRI_GetByExternalNameFunctionAql (TRI_associative_pointer_t* functions,
                                                      const char* const externalName) {
  TRI_aql_function_t* function;
  char* upperName;

  assert(functions);
  assert(externalName);

  // normalize the name by upper-casing it
  upperName = TRI_UpperAsciiString(externalName);
  if (!upperName) {
    return NULL;
  }

  function = (TRI_aql_function_t*) TRI_LookupByKeyAssociativePointer(functions, (void*) upperName);
  TRI_Free(TRI_CORE_MEM_ZONE, upperName);

  return function;
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
                              const char* const argPattern) {
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

  if (TRI_InsertKeyAssociativePointer(functions, externalName, function, false)) {
    // function already registered
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, function->_externalName);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, function->_internalName);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, function);
    return false;
  }

  function->_isDeterministic = isDeterministic; 
  function->_isGroup = isGroup; 
  function->_argPattern = argPattern;

  // set minArgs and maxArgs
  SetArgumentCount(function);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether a function argument must be converted to another type
////////////////////////////////////////////////////////////////////////////////

bool TRI_ConvertParameterFunctionAql (const TRI_aql_function_t* const function,
                                      const size_t checkArg) {
  const char* pattern;
  char c;
  size_t i = 0;
  bool foundArg = false;

  assert(function);

  i = 0; 
  pattern = function->_argPattern;
  while ((c = *pattern++)) {
    switch (c) {
      case '|':
      case ',':
        if (foundArg) {
          if (++i > checkArg) {
            return false;
          }
        }
        foundArg = false;
        break;
      case 'h': 
        if (i == checkArg) {
          return true;
        }
        // break intentionally missing
      default:
        foundArg = true;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief validate the arguments passed to a function
////////////////////////////////////////////////////////////////////////////////

bool TRI_ValidateArgsFunctionAql (TRI_aql_context_t* const context,
                                  const TRI_aql_function_t* const function,
                                  const TRI_aql_node_t* const parameters) {
  param_t allowed;
  const char* pattern;
  size_t i, n;
  bool eof = false;
  bool repeat = false;

  assert(function);
  assert(parameters);
  assert(parameters->_type == AQL_NODE_LIST);

  n = parameters->_members._length;

  // validate number of arguments
  if (n < function->_minArgs || n > function->_maxArgs) {
    // invalid number of arguments
    TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, function->_externalName);
    return false;
  }

  pattern = function->_argPattern;

  // validate argument types
  for (i = 0; i < n; ++i) {
    TRI_aql_node_t* parameter = (TRI_aql_node_t*) TRI_AQL_NODE_MEMBER(parameters, i);
    bool parse = true;
    bool foundArg = false;

    if (repeat) {
      // last argument is repeated
      ARG_CHECK
    }
    else {
      // last argument is not repeated
      allowed = InitParam();

      foundArg = false;

      while (parse && !eof) {
        char c = *pattern++;
        
        switch (c) {
          case '\0':
            parse = false;
            eof = true;
            if (foundArg) {
              ARG_CHECK
            }
            break;
          case '|': // optional marker
            if (foundArg) {
              parse = false;
              ARG_CHECK
              if (*pattern == '+') {
                repeat = true;
                eof = true;
              }
            }
            break;
          case ',': // next argument
            assert(foundArg);
            parse = false;
            ARG_CHECK
            break;
          case '+': // repeat last argument
            repeat = true;
            parse = false;
            eof = true;
            ARG_CHECK
            break;
          case '.': // any type except collections
            allowed._list = true;
            allowed._array = true;
            // break intentionally missing!!
          case 'p': // primitive types
            allowed._null = true;
            allowed._bool = true;
            allowed._number = true;
            allowed._string = true;
            foundArg = true;
            break;
          case 'z': // null
            allowed._null = true;
            foundArg = true;
            break;
          case 'b': // bool
            allowed._bool = true;
            foundArg = true;
            break;
          case 'n': // number
            allowed._number = true;
            foundArg = true;
            break;
          case 's': // string
            allowed._string = true;
            foundArg = true;
            break;
          case 'l': // list
            allowed._list = true;
            foundArg = true;
            break;
          case 'a': // array
            allowed._array = true;
            foundArg = true;
            break;
          case 'c': // collection name => list
            allowed._collection = true;
            foundArg = true;
            break;
          case 'h': // collection name => string
            allowed._collection = true;
            foundArg = true;
            break;
        }
      }
    }

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

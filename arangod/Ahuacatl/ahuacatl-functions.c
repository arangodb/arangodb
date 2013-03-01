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

#include "BasicsC/associative.h"
#include "BasicsC/logging.h"
#include "BasicsC/strings.h"

#include "Ahuacatl/ahuacatl-access-optimiser.h"
#include "Ahuacatl/ahuacatl-collections.h"
#include "Ahuacatl/ahuacatl-context.h"

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

#define REGISTER_FUNCTION(internalName, externalName, deterministic, group, argPattern, optimiseCallback) \
  result &= TRI_RegisterFunctionAql(functions, internalName, externalName, deterministic, group, argPattern, optimiseCallback)

////////////////////////////////////////////////////////////////////////////////
/// @brief shorthand to check an argument and return an error if it is invalid
////////////////////////////////////////////////////////////////////////////////

#define ARG_CHECK                                                                                                   \
  if (! CheckArgumentType(parameter, &allowed)) {                                                                   \
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
  bool _regex      : 1;
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

  param._null       = false;
  param._bool       = false;
  param._number     = false;
  param._string     = false;
  param._list       = false;
  param._array      = false;
  param._collection = false;
  param._regex      = false;

  return param;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check the type of an argument for a function call
////////////////////////////////////////////////////////////////////////////////

static bool CheckArgumentType (TRI_aql_node_t* parameter, 
                               const param_t* const allowed) {
  param_t found = InitParam(); 

  if (parameter->_type == TRI_AQL_NODE_PARAMETER) {
    // node is a bind parameter
    char* name = TRI_AQL_NODE_STRING(parameter);

    if (*name == '@') {
      // collection bind parameter. this is an error
      found._collection = true;
      found._list       = true; // a collection is a list of documents
    }
    else {
      // regular bind parameter
      found._null   = true;
      found._bool   = true;
      found._number = true;
      found._string = true;
      found._list   = true;
      found._array  = true;
    }
  }
  else if (parameter->_type == TRI_AQL_NODE_VALUE) {
    switch (parameter->_value._type) {
      case TRI_AQL_TYPE_FAIL:
      case TRI_AQL_TYPE_NULL:
        found._null   = true;
        break;
      case TRI_AQL_TYPE_BOOL:
        found._bool   = true;
        break;
      case TRI_AQL_TYPE_INT:
      case TRI_AQL_TYPE_DOUBLE:
        found._number = true;
        break;
      case TRI_AQL_TYPE_STRING:
        found._string = true;
        break;
    }
  }
  else if (parameter->_type == TRI_AQL_NODE_LIST) {
    // actual parameter is a list
    found._list = true;
  }
  else if (parameter->_type == TRI_AQL_NODE_ARRAY) {
    // actual parameter is an array
    found._array = true;
  }
  else if (parameter->_type == TRI_AQL_NODE_COLLECTION) {
    // actual parameter is a collection
    found._collection = true;
    found._list       = true; // a collection is a list of documents
  }
  else {
    // we cannot yet determine the type of the parameter
    // this is the case if the argument is an expression, a function call etc.

    if (allowed->_regex) {
      return false;
    }

    if (! allowed->_collection) {
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
  
  if ((allowed->_string || allowed->_regex) && found._string) {
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
// --SECTION--                                               optimiser callbacks
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief check if we have a matching restriction we can use to optimise
/// a PATHS query
////////////////////////////////////////////////////////////////////////////////

static bool CheckPathRestriction (TRI_aql_field_access_t* fieldAccess, 
                                  TRI_aql_context_t* const context,
                                  TRI_aql_node_t* vertexCollection,
                                  const char* lookFor, 
                                  char* name,
                                  const size_t n) {
  size_t len;

  assert(fieldAccess);
  assert(lookFor);
  
  len = strlen(lookFor);
  if (len == 0) {
    return false;
  }

  if (n > fieldAccess->_variableNameLength + len && 
      memcmp((void*) lookFor, (void*) name, len) == 0) {
    // we'll now patch the collection hint
    TRI_aql_collection_hint_t* hint;

    // field name is collection.source.XXX, e.g. users.source._id
    LOG_DEBUG("optimising PATHS() field access %s", fieldAccess->_fullName);
 
    // we can now modify this fieldaccess in place to collection.XXX, e.g. users._id
    // copy trailing \0 byte as well
    memmove(name, name + len - 1, n - fieldAccess->_variableNameLength - len + 2);
    
    // attach the modified fieldaccess to the collection
    hint = (TRI_aql_collection_hint_t*) (TRI_AQL_NODE_DATA(vertexCollection));
    hint->_ranges = TRI_AddAccessAql(context, hint->_ranges, fieldAccess);

    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise callback function for PATHS() AQL function
////////////////////////////////////////////////////////////////////////////////

static void OptimisePaths (const TRI_aql_node_t* const fcallNode,
                           TRI_aql_context_t* const context,
                           TRI_aql_field_access_t* fieldAccess) {
  TRI_aql_node_t* args;
  TRI_aql_node_t* vertexCollection;
  TRI_aql_node_t* edgeCollection;
  TRI_aql_node_t* direction;
  char* directionValue;
  char* name;
  size_t n;
  
  args = TRI_AQL_NODE_MEMBER(fcallNode, 0);

  if (args == NULL) {
    return;
  }

  vertexCollection = TRI_AQL_NODE_MEMBER(args, 0);
  edgeCollection = TRI_AQL_NODE_MEMBER(args, 1);
  direction = TRI_AQL_NODE_MEMBER(args, 2);

  assert(vertexCollection);
  assert(edgeCollection);
  assert(direction);
  assert(fieldAccess);

  n = strlen(fieldAccess->_fullName);
  name = fieldAccess->_fullName + fieldAccess->_variableNameLength;

  directionValue = TRI_AQL_NODE_STRING(direction);
  // try to optimise the vertex collection access
  if (TRI_EqualString(directionValue, "outbound")) {
    CheckPathRestriction(fieldAccess, context, vertexCollection, ".source.", name, n);
  }
  else if (TRI_EqualString(directionValue, "inbound")) {
    CheckPathRestriction(fieldAccess, context, vertexCollection, ".source.", name, n);
  }
  else if (TRI_EqualString(directionValue, "any")) {
    // "any" cannot be optimised sanely becuase the conditions would be AND-combined
    // (but for "any", we'd need them OR-combined)

    // CheckPathRestriction(fieldAccess, context, vertexCollection, ".source.", name, n);
    // CheckPathRestriction(fieldAccess, context, vertexCollection, ".destination.", name, n);
  }

  // check if we have a filter on LENGTH(edges)
  if (args->_members._length <= 4 && 
      TRI_EqualString(name, ".edges.LENGTH()")) {
    // length restriction, can only be applied if length parameters are not already set
    TRI_json_t* value;
    double minValue = 0.0;
    double maxValue = 0.0;
    bool useMin = false;
    bool useMax = false;

    if (fieldAccess->_type == TRI_AQL_ACCESS_EXACT) {
      value = fieldAccess->_value._value;

      if (value != NULL && value->_type == TRI_JSON_NUMBER) {
        // LENGTH(p.edges) == const
        minValue = maxValue = value->_value._number;
        useMin = useMax = true;
      }
    }
    else if (fieldAccess->_type == TRI_AQL_ACCESS_RANGE_SINGLE) {
      value = fieldAccess->_value._singleRange._value;

      if (value != NULL && value->_type == TRI_JSON_NUMBER) {
        // LENGTH(p.edges) operator const
        if (fieldAccess->_value._singleRange._type == TRI_AQL_RANGE_LOWER_INCLUDED) {
          minValue = value->_value._number;
          useMin = true;
        }
        else if (fieldAccess->_value._singleRange._type == TRI_AQL_RANGE_UPPER_INCLUDED) {
          maxValue = value->_value._number;
          useMax = true;
        }
        else if (fieldAccess->_value._singleRange._type == TRI_AQL_RANGE_LOWER_EXCLUDED) {
          if ((double) ((int) value->_value._number) == value->_value._number) {
            minValue = value->_value._number + 1.0;
            useMin = true;
          }
        }
        else if (fieldAccess->_value._singleRange._type == TRI_AQL_RANGE_UPPER_EXCLUDED) {
          if ((double) ((int) value->_value._number) == value->_value._number) {
            maxValue = value->_value._number - 1.0;
            useMax = true;
          }
        }
      }
    }
    else if (fieldAccess->_type == TRI_AQL_ACCESS_RANGE_DOUBLE) {
      // LENGTH(p.edges) > const && LENGTH(p.edges) < const
      value = fieldAccess->_value._between._lower._value;

      if (value != NULL && value->_type == TRI_JSON_NUMBER) {
        if (fieldAccess->_value._between._lower._type == TRI_AQL_RANGE_LOWER_INCLUDED) {
          minValue = value->_value._number;
          useMin = true;
        }
        else if (fieldAccess->_value._between._lower._type == TRI_AQL_RANGE_LOWER_EXCLUDED) {
          if ((double) ((int) value->_value._number) == value->_value._number) {
            minValue = value->_value._number + 1.0;
            useMin = true;
          }
        }
      }

      value = fieldAccess->_value._between._upper._value;

      if (value != NULL && value->_type == TRI_JSON_NUMBER) {
        if (fieldAccess->_value._between._upper._type == TRI_AQL_RANGE_UPPER_INCLUDED) {
          maxValue = value->_value._number;
          useMax = true;
        }
        else if (fieldAccess->_value._between._upper._type == TRI_AQL_RANGE_UPPER_EXCLUDED) {
          if ((double) ((int) value->_value._number) == value->_value._number) {
            maxValue = value->_value._number - 1.0;
            useMax = true;
          }
        }
      }
    }

    if (useMin || useMax) {
      TRI_aql_node_t* argNode;

      // minLength and maxLength are parameters 5 & 6
      // add as many null value nodes as are missing
      while (args->_members._length < 4) {
        argNode = TRI_CreateNodeValueNullAql(context);
        if (argNode) {
          TRI_PushBackVectorPointer(&args->_members, (void*) argNode);
        }
      }

      // add min and max values to the function call argument list
      argNode = TRI_CreateNodeValueIntAql(context, useMin ? (int64_t) minValue : (int64_t) 0);
      if (argNode) {
        // min value node
        TRI_PushBackVectorPointer(&args->_members, (void*) argNode);

        argNode = TRI_CreateNodeValueIntAql(context, useMax ? (int64_t) maxValue : (int64_t) (1024 * 1024));
        if (argNode) {
          // max value node
          TRI_PushBackVectorPointer(&args->_members, (void*) argNode);
        }
      }
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
/// @brief initialise the array with the function declarations 
////////////////////////////////////////////////////////////////////////////////

TRI_associative_pointer_t* TRI_InitialiseFunctionsAql (void) {
  TRI_associative_pointer_t* functions;
  bool result = true;
  
  functions = (TRI_associative_pointer_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_associative_pointer_t), false); 

  if (functions == NULL) {
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
  // a = (hash) array/document
  // r = regex (a string with a special format). note: the regex type is mutually exclusive with all other types

  // type check functions
  REGISTER_FUNCTION("IS_NULL", "IS_NULL", true, false, ".", NULL);
  REGISTER_FUNCTION("IS_BOOL", "IS_BOOL", true, false, ".", NULL);
  REGISTER_FUNCTION("IS_NUMBER", "IS_NUMBER", true, false, ".", NULL);
  REGISTER_FUNCTION("IS_STRING", "IS_STRING", true, false, ".", NULL);
  REGISTER_FUNCTION("IS_LIST", "IS_LIST", true, false, ".", NULL);
  REGISTER_FUNCTION("IS_DOCUMENT", "IS_DOCUMENT", true, false, ".", NULL);

  // cast functions
  REGISTER_FUNCTION("TO_NUMBER", "CAST_NUMBER", true, false, ".", NULL);
  REGISTER_FUNCTION("TO_STRING", "CAST_STRING", true, false, ".", NULL);
  REGISTER_FUNCTION("TO_BOOL", "CAST_BOOL", true, false, ".", NULL);
  REGISTER_FUNCTION("TO_LIST", "CAST_LIST", true, false, ".", NULL);

  // string functions
  REGISTER_FUNCTION("CONCAT", "STRING_CONCAT", true, false, "sz,sz|+", NULL); 
  REGISTER_FUNCTION("CONCAT_SEPARATOR", "STRING_CONCAT_SEPARATOR", true, false, "s,sz,sz|+", NULL); 
  REGISTER_FUNCTION("CHAR_LENGTH", "CHAR_LENGTH", true, false, "s", NULL); 
  REGISTER_FUNCTION("LOWER", "STRING_LOWER", true, false, "s", NULL); 
  REGISTER_FUNCTION("UPPER", "STRING_UPPER", true, false, "s", NULL); 
  REGISTER_FUNCTION("SUBSTRING", "STRING_SUBSTRING", true, false, "s,n|n", NULL);
  REGISTER_FUNCTION("CONTAINS", "STRING_CONTAINS", true, false, "s,s|b", NULL);
  REGISTER_FUNCTION("LIKE", "STRING_LIKE", true, false, "s,r|b", NULL);

  // numeric functions 
  REGISTER_FUNCTION("FLOOR", "NUMBER_FLOOR", true, false, "n", NULL);
  REGISTER_FUNCTION("CEIL", "NUMBER_CEIL", true, false, "n", NULL);
  REGISTER_FUNCTION("ROUND", "NUMBER_ROUND", true, false, "n", NULL);
  REGISTER_FUNCTION("ABS", "NUMBER_ABS", true, false, "n", NULL);
  REGISTER_FUNCTION("RAND", "NUMBER_RAND", false, false, "", NULL);

  // list functions
  REGISTER_FUNCTION("UNION", "UNION", true, false, "l,l|+", NULL);
  REGISTER_FUNCTION("LENGTH", "LENGTH", true, true, "la", NULL);
  REGISTER_FUNCTION("MIN", "MIN", true, true, "l", NULL);
  REGISTER_FUNCTION("MAX", "MAX", true, true, "l", NULL);
  REGISTER_FUNCTION("SUM", "SUM", true, true, "l", NULL);
  REGISTER_FUNCTION("UNIQUE", "UNIQUE", true, false, "l", NULL);
  REGISTER_FUNCTION("REVERSE", "REVERSE", true, false, "l", NULL);
  REGISTER_FUNCTION("FIRST", "FIRST", true, false, "l", NULL);
  REGISTER_FUNCTION("LAST", "LAST", true, false, "l", NULL);
  
  // document functions
  REGISTER_FUNCTION("HAS", "HAS", true, false, "az,s", NULL); 
  REGISTER_FUNCTION("ATTRIBUTES", "ATTRIBUTES", true, false, "a|b,b", NULL); 
  REGISTER_FUNCTION("MERGE", "MERGE", true, false, "a,a|+", NULL);
  REGISTER_FUNCTION("MERGE_RECURSIVE", "MERGE_RECURSIVE", true, false, "a,a|+", NULL);
  REGISTER_FUNCTION("DOCUMENT", "DOCUMENT", false, false, "h,sl", NULL);
  REGISTER_FUNCTION("MATCHES", "MATCHES", true, false, ".,l|b", NULL);
  REGISTER_FUNCTION("UNSET", "UNSET", true, false, "a,sl|+", NULL);
  REGISTER_FUNCTION("KEEP", "KEEP", true, false, "a,sl|+", NULL);

  // geo functions
  REGISTER_FUNCTION("NEAR", "GEO_NEAR", false, false, "h,n,n,n|s", NULL);
  REGISTER_FUNCTION("WITHIN", "GEO_WITHIN", false, false, "h,n,n,n|s", NULL);

  // fulltext functions
  REGISTER_FUNCTION("FULLTEXT", "FULLTEXT", false, false, "h,s,s", NULL);

  // graph functions
  REGISTER_FUNCTION("PATHS", "GRAPH_PATHS", false, false, "c,h|s,b", &OptimisePaths);
  REGISTER_FUNCTION("TRAVERSAL", "GRAPH_TRAVERSAL", false, false, "h,h,s,s,a", NULL);
  REGISTER_FUNCTION("TRAVERSAL_TREE", "GRAPH_TRAVERSAL_TREE", false, false, "h,h,s,s,s,a", NULL);
  REGISTER_FUNCTION("EDGES", "GRAPH_EDGES", false, false, "h,s,s", NULL);
  
  // misc functions
  REGISTER_FUNCTION("FAIL", "FAIL", false, false, "|s", NULL); // FAIL is non-deterministic, otherwise query optimisation will fail!
  REGISTER_FUNCTION("PASSTHRU", "PASSTHRU", false, false, ".", NULL); // simple non-deterministic wrapper to avoid optimisations at parse time
  REGISTER_FUNCTION("COLLECTIONS", "COLLECTIONS", false, false, "", NULL); 
  REGISTER_FUNCTION("NOT_NULL", "NOT_NULL", true, false, ".|+", NULL);
  REGISTER_FUNCTION("FIRST_LIST", "FIRST_LIST", true, false, ".|+", NULL);
  REGISTER_FUNCTION("FIRST_DOCUMENT", "FIRST_DOCUMENT", true, false, ".|+", NULL);

  if (! result) {
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
/// @brief return a function, looked up by its external name
////////////////////////////////////////////////////////////////////////////////

TRI_aql_function_t* TRI_GetByExternalNameFunctionAql (TRI_associative_pointer_t* functions,
                                                      const char* const externalName) {
  TRI_aql_function_t* function;
  char* upperName;

  assert(functions);
  assert(externalName);

  // normalize the name by upper-casing it
  upperName = TRI_UpperAsciiStringZ(TRI_UNKNOWN_MEM_ZONE, externalName);
  if (upperName == NULL) {
    return NULL;
  }

  function = (TRI_aql_function_t*) TRI_LookupByKeyAssociativePointer(functions, (void*) upperName);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, upperName);

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
                              const char* const argPattern,
                              void (*optimise)(const TRI_aql_node_t* const, TRI_aql_context_t* const, TRI_aql_field_access_t*)) {
  TRI_aql_function_t* function;
  
  function = (TRI_aql_function_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_aql_function_t), false);

  if (function == NULL) {
    return false;
  }

  function->_externalName = TRI_DuplicateStringZ(TRI_UNKNOWN_MEM_ZONE, externalName);
  if (function->_externalName == NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, function);
    return false;
  }

  // normalize name by upper-casing it
  function->_internalName = TRI_UpperAsciiStringZ(TRI_UNKNOWN_MEM_ZONE, internalName);
  if (function->_internalName == NULL) {
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
  function->optimise = optimise;

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
  size_t i;
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
  assert(parameters->_type == TRI_AQL_NODE_LIST);

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
    
    if (repeat) {
      // last argument is repeated
      ARG_CHECK
    }
    else {
      // last argument is not repeated
      bool parse = true;
      bool foundArg;

      allowed = InitParam();

      foundArg = false;

      while (parse && ! eof) {
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
          case 'r': // regex
            allowed._regex = true;
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

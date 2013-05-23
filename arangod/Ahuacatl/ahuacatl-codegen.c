////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, AST dump functions
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

#include "BasicsC/logging.h"

#include "Ahuacatl/ahuacatl-access-optimiser.h"
#include "Ahuacatl/ahuacatl-codegen.h"
#include "Ahuacatl/ahuacatl-collections.h"
#include "Ahuacatl/ahuacatl-index.h"
#include "Ahuacatl/ahuacatl-functions.h"
#include "Ahuacatl/ahuacatl-node.h"
#include "Ahuacatl/ahuacatl-scope.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                    private macros
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief generate verbose code
////////////////////////////////////////////////////////////////////////////////

#undef AQL_VERBOSE

////////////////////////////////////////////////////////////////////////////////
/// @brief prefix for register
////////////////////////////////////////////////////////////////////////////////

#define REGISTER_PREFIX "r"

////////////////////////////////////////////////////////////////////////////////
/// @brief prefix for functions
////////////////////////////////////////////////////////////////////////////////

#define FUNCTION_PREFIX "f"

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                          forwards
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

static void ProcessAttributeAccess (TRI_aql_codegen_js_t* const,
                                    const TRI_aql_node_t* const);

static void ProcessNode (TRI_aql_codegen_js_t* const,
                         const TRI_aql_node_t* const);

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
/// @brief hash a variable
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashVariable (TRI_associative_pointer_t* array,
                              void const* element) {
  TRI_aql_codegen_variable_t* variable = (TRI_aql_codegen_variable_t*) element;

  return TRI_FnvHashString(variable->_name);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief comparison function used to determine variable equality
////////////////////////////////////////////////////////////////////////////////

static bool EqualVariable (TRI_associative_pointer_t* array,
                           void const* key,
                           void const* element) {
  TRI_aql_codegen_variable_t* variable = (TRI_aql_codegen_variable_t*) element;

  return TRI_EqualString(key, variable->_name);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the next register number
////////////////////////////////////////////////////////////////////////////////

static inline TRI_aql_codegen_register_t IncRegister (TRI_aql_codegen_js_t* const generator) {
  return ++generator->_registerIndex;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the next function number
////////////////////////////////////////////////////////////////////////////////

static inline TRI_aql_codegen_register_t IncFunction (TRI_aql_codegen_js_t* const generator) {
  return ++generator->_functionIndex;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the current scope
////////////////////////////////////////////////////////////////////////////////

static inline TRI_aql_codegen_scope_t* CurrentScope (TRI_aql_codegen_js_t* const generator) {
  size_t n = generator->_scopes._length;

  assert(n > 0);
  return (TRI_aql_codegen_scope_t*) TRI_AtVectorPointer(&generator->_scopes, n - 1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief append a string to the buffer
////////////////////////////////////////////////////////////////////////////////

static inline bool OutputString (TRI_string_buffer_t* const buffer,
                                 const char* const value) {
  if (buffer == NULL) {
    return true;
  }

  if (value == NULL) {
    return false;
  }

  return (TRI_AppendStringStringBuffer(buffer, value) == TRI_ERROR_NO_ERROR);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief append a string to the buffer
////////////////////////////////////////////////////////////////////////////////

static inline bool OutputString2 (TRI_string_buffer_t* const buffer,
                                  const char* const value,
                                  size_t length) {
  return (TRI_AppendString2StringBuffer(buffer, value, length) == TRI_ERROR_NO_ERROR);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief append a single character to the buffer
////////////////////////////////////////////////////////////////////////////////

static inline bool OutputChar (TRI_string_buffer_t* const buffer,
                               const char value) {
  if (buffer == NULL) {
    return true;
  }

  return (TRI_AppendCharStringBuffer(buffer, value) == TRI_ERROR_NO_ERROR);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief append an unsigned integer value to the buffer
////////////////////////////////////////////////////////////////////////////////

static inline bool OutputUInt (TRI_string_buffer_t* const buffer,
                               const uint64_t value) {
  if (buffer == NULL) {
    return true;
  }

  return (TRI_AppendUInt64StringBuffer(buffer, value) == TRI_ERROR_NO_ERROR);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief append an integer value to the buffer
////////////////////////////////////////////////////////////////////////////////

static inline bool OutputInt (TRI_string_buffer_t* const buffer,
                              const int64_t value) {
  if (buffer == NULL) {
    return true;
  }

  return (TRI_AppendInt64StringBuffer(buffer, value) == TRI_ERROR_NO_ERROR);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print a string in the current scope
////////////////////////////////////////////////////////////////////////////////

static inline void ScopeOutput (TRI_aql_codegen_js_t* const generator,
                                const char* const value) {
  TRI_aql_codegen_scope_t* scope = CurrentScope(generator);

  if (! OutputString(scope->_buffer, value)) {
    generator->_errorCode = TRI_ERROR_OUT_OF_MEMORY;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print an unsgined integer in the current scope
////////////////////////////////////////////////////////////////////////////////

static inline void ScopeOutputUInt (TRI_aql_codegen_js_t* const generator,
                                    const uint64_t value) {
  TRI_aql_codegen_scope_t* scope = CurrentScope(generator);

  if (! OutputUInt(scope->_buffer, value)) {
    generator->_errorCode = TRI_ERROR_OUT_OF_MEMORY;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print an escaped string in the current scope, enclosed by '
////////////////////////////////////////////////////////////////////////////////

static inline void ScopeOutputQuoted (TRI_aql_codegen_js_t* const generator,
                                      const char* const value) {
  TRI_aql_codegen_scope_t* scope = CurrentScope(generator);

  if (! OutputChar(scope->_buffer, '\'')) {
    generator->_errorCode = TRI_ERROR_OUT_OF_MEMORY;
  }
  if (! OutputString(scope->_buffer, value)) {
    generator->_errorCode = TRI_ERROR_OUT_OF_MEMORY;
  }
  if (! OutputChar(scope->_buffer, '\'')) {
    generator->_errorCode = TRI_ERROR_OUT_OF_MEMORY;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print an escaped string in the current scope, enclosed by "
////////////////////////////////////////////////////////////////////////////////

static inline void ScopeOutputQuoted2 (TRI_aql_codegen_js_t* const generator,
                                       const char* const value) {
  TRI_aql_codegen_scope_t* scope = CurrentScope(generator);
  char* escaped;
  size_t outLength;

  if (! OutputChar(scope->_buffer, '"')) {
    generator->_errorCode = TRI_ERROR_OUT_OF_MEMORY;
  }

  escaped = TRI_EscapeUtf8StringZ(TRI_UNKNOWN_MEM_ZONE, value, strlen(value), false, &outLength, false);

  if (escaped != NULL) {
    if (! OutputString2(scope->_buffer, escaped, outLength)) {
      generator->_errorCode = TRI_ERROR_OUT_OF_MEMORY;
    }

    TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, escaped);
  }
  else {
    generator->_errorCode = TRI_ERROR_OUT_OF_MEMORY;
  }

  if (! OutputChar(scope->_buffer, '"')) {
    generator->_errorCode = TRI_ERROR_OUT_OF_MEMORY;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print a JSON value in the current scope
////////////////////////////////////////////////////////////////////////////////

static inline void ScopeOutputJson (TRI_aql_codegen_js_t* const generator,
                                    const TRI_json_t* const json) {
  TRI_aql_codegen_scope_t* scope = CurrentScope(generator);

  if (! scope->_buffer) {
    return;
  }

  if (TRI_StringifyJson(scope->_buffer, json) != TRI_ERROR_NO_ERROR) {
    generator->_errorCode = TRI_ERROR_OUT_OF_MEMORY;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print an index id in the current scope
////////////////////////////////////////////////////////////////////////////////

static inline void ScopeOutputIndexId (TRI_aql_codegen_js_t* const generator,
                                       const char* collectionName,
                                       const TRI_aql_index_t* const idx) {
  ScopeOutput(generator, "\"");
  ScopeOutput(generator, collectionName);
  ScopeOutput(generator, "/");
  ScopeOutputUInt(generator, idx->_idx->_iid);
  ScopeOutput(generator, "\"");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print a function name in the current scope
////////////////////////////////////////////////////////////////////////////////

static inline void ScopeOutputFunction (TRI_aql_codegen_js_t* const generator,
                                        const TRI_aql_codegen_register_t functionIndex) {
  TRI_aql_codegen_scope_t* scope = CurrentScope(generator);

  if (! OutputString(scope->_buffer, FUNCTION_PREFIX)) {
    generator->_errorCode = TRI_ERROR_OUT_OF_MEMORY;
  }
  if (! OutputInt(scope->_buffer, (int64_t) functionIndex)) {
    generator->_errorCode = TRI_ERROR_OUT_OF_MEMORY;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print a register name in the current scope
////////////////////////////////////////////////////////////////////////////////

static inline void ScopeOutputRegister (TRI_aql_codegen_js_t* const generator,
                                        const TRI_aql_codegen_register_t registerIndex) {
  TRI_aql_codegen_scope_t* scope = CurrentScope(generator);

  if (! OutputString(scope->_buffer, REGISTER_PREFIX)) {
    generator->_errorCode = TRI_ERROR_OUT_OF_MEMORY;
  }
  if (! OutputInt(scope->_buffer, (int64_t) registerIndex)) {
    generator->_errorCode = TRI_ERROR_OUT_OF_MEMORY;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for indexed register access
////////////////////////////////////////////////////////////////////////////////

static void ScopeOutputIndexedName (TRI_aql_codegen_js_t* const generator,
                                    const TRI_aql_codegen_register_t sourceRegister,
                                    const char* const indexName) {
  ScopeOutputRegister(generator, sourceRegister);
  ScopeOutput(generator, "[");
  ScopeOutputQuoted(generator, indexName);
  ScopeOutput(generator, "]");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free a scope variable
////////////////////////////////////////////////////////////////////////////////

static void FreeVariable (TRI_aql_codegen_variable_t* const variable) {
  assert(variable);
  assert(variable->_name);

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, variable->_name);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, variable);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a scope variable
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_codegen_variable_t* CreateVariable (const char* const name,
                                                   const TRI_aql_codegen_register_t registerIndex) {
  TRI_aql_codegen_variable_t* variable;

  variable = (TRI_aql_codegen_variable_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_aql_codegen_variable_t), false);
  if (variable == NULL) {
    return NULL;
  }

  variable->_register = registerIndex;
  variable->_name = TRI_DuplicateStringZ(TRI_UNKNOWN_MEM_ZONE, name);

  if (variable->_name == NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, variable);

    return NULL;
  }

  return variable;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the type for the next scope
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_scope_e NextScopeType (TRI_aql_codegen_js_t* const generator,
                                      const TRI_aql_scope_e requestedType) {
  TRI_aql_codegen_scope_t* scope;

  // we're only interested in TRI_AQL_SCOPE_FOR
  if (requestedType != TRI_AQL_SCOPE_FOR) {
    return requestedType;
  }

  scope = CurrentScope(generator);

  // if we are in a TRI_AQL_SCOPE_FOR scope, we'll return TRI_AQL_SCOPE_FOR_NESTED
  if (scope->_type == TRI_AQL_SCOPE_FOR || scope->_type == TRI_AQL_SCOPE_FOR_NESTED) {
    return TRI_AQL_SCOPE_FOR_NESTED;
  }

  return requestedType;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start a new scope and push it onto the stack
////////////////////////////////////////////////////////////////////////////////

static void StartScope (TRI_aql_codegen_js_t* const generator,
                        TRI_string_buffer_t* const buffer,
                        const TRI_aql_scope_e type,
                        const TRI_aql_codegen_register_t listRegister,
                        const TRI_aql_codegen_register_t keyRegister,
                        const TRI_aql_codegen_register_t ownRegister,
                        const TRI_aql_codegen_register_t resultRegister,
                        TRI_aql_for_hint_t* const hint) {
  TRI_aql_codegen_scope_t* scope;
  TRI_aql_codegen_register_t offsetRegister;
  TRI_aql_codegen_register_t limitRegister;

  scope = (TRI_aql_codegen_scope_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_aql_codegen_scope_t), false);
  if (scope == NULL) {
    generator->_errorCode = TRI_ERROR_OUT_OF_MEMORY;
    return;
  }

  // use optimisations for offset and limit?
  offsetRegister = 0;
  limitRegister  = 0;
  if (hint != NULL && hint->_limit._status == TRI_AQL_LIMIT_USE) {
    if (hint->_limit._hasFilter) {
      // delegate limit handling to following filter condition
      if (hint->_limit._offset > 0) {
        // only use offset if > 0
        offsetRegister = IncRegister(generator);
      }
      // always use limit
      limitRegister = IncRegister(generator);
      LOG_TRACE("limit optimisation will be handled by filter condition");
    }
    else {
      // do the limit handling within the for-loop itself
      LOG_TRACE("limit optimisation will be handled by for-loop");
    }
  }

  scope->_buffer         = buffer;
  scope->_type           = NextScopeType(generator, type);
  scope->_listRegister   = listRegister;
  scope->_keyRegister    = keyRegister;
  scope->_ownRegister    = ownRegister;
  scope->_offsetRegister = offsetRegister;
  scope->_limitRegister  = limitRegister;
  scope->_resultRegister = resultRegister;
  scope->_prefix         = NULL;
  scope->_hint           = hint; // for-loop hint

  // init symbol table
  TRI_InitAssociativePointer(&scope->_variables,
                             TRI_UNKNOWN_MEM_ZONE,
                             &TRI_HashStringKeyAssociativePointer,
                             &HashVariable,
                             &EqualVariable,
                             NULL);

  // push the scope on the stack
  TRI_PushBackVectorPointer(&generator->_scopes, (void*) scope);

#if AQL_VERBOSE
  ScopeOutput(generator, "\n/* scope start (");
  ScopeOutput(generator, TRI_TypeNameScopeAql(scope->_type));
  ScopeOutput(generator, ", resultRegister: ");
  ScopeOutputRegister(generator, scope->_resultRegister);
  ScopeOutput(generator, ", limitRegister: ");
  ScopeOutputRegister(generator, scope->_limitRegister);
  ScopeOutput(generator, ") */\n");
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief end the current scope and pop it from the stack
////////////////////////////////////////////////////////////////////////////////

static void EndScope (TRI_aql_codegen_js_t* const generator) {
  TRI_aql_codegen_scope_t* scope;
  TRI_aql_codegen_register_t i, n;

#if AQL_VERBOSE
  scope = CurrentScope(generator);
  ScopeOutput(generator, "\n/* scope end (");
  ScopeOutput(generator, TRI_TypeNameScopeAql(scope->_type));
  ScopeOutput(generator, ") */\n");
#endif

  n = generator->_scopes._length;
  assert(n > 0);
  scope = (TRI_aql_codegen_scope_t*) TRI_RemoveVectorPointer(&generator->_scopes, n - 1);
  assert(scope);

  // free all variables in scope
  n = scope->_variables._nrAlloc;
  for (i = 0; i < n; ++i) {
    TRI_aql_codegen_variable_t* variable = (TRI_aql_codegen_variable_t*) scope->_variables._table[i];
    if (variable) {
      FreeVariable(variable);
    }
  }
  // free symbol table
  TRI_DestroyAssociativePointer(&scope->_variables);

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, scope);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for a sort function
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_codegen_register_t CreateSortFunction (TRI_aql_codegen_js_t* const generator,
                                                      const TRI_aql_node_t* const node,
                                                      const size_t elementIndex) {
  TRI_aql_node_t* list;
  TRI_aql_codegen_scope_t* scope;
  TRI_aql_codegen_register_t functionIndex = IncFunction(generator);
  size_t i;
  size_t n;

  // start a new scope first
  StartScope(generator, &generator->_functionBuffer, TRI_AQL_SCOPE_FUNCTION, 0, 0, 0, 0, NULL);
  scope = CurrentScope(generator);

  ScopeOutput(generator, "function ");
  ScopeOutputFunction(generator, functionIndex);
  ScopeOutput(generator, " (l, r) {\n");
  ScopeOutput(generator, "var res, lhs, rhs;\n");

  list = TRI_AQL_NODE_MEMBER(node, 0);

  // loop over sort criteria
  n = list->_members._length;
  for (i = 0; i < n; ++i) {
    TRI_aql_node_t* element = TRI_AQL_NODE_MEMBER(list, i);

    scope->_prefix = "l";
    ScopeOutput(generator, "lhs = ");
    ProcessNode(generator, TRI_AQL_NODE_MEMBER(element, elementIndex));
    ScopeOutput(generator, ";\n");

    scope->_prefix = "r";
    ScopeOutput(generator, "rhs = ");
    ProcessNode(generator, TRI_AQL_NODE_MEMBER(element, elementIndex));
    ScopeOutput(generator, ";\n");

    scope->_prefix = NULL;
    ScopeOutput(generator, "res = aql.RELATIONAL_CMP(lhs, rhs);\n");
    ScopeOutput(generator, "if (res) {\n");
    if (elementIndex || TRI_AQL_NODE_BOOL(element)) {
      // ascending
      ScopeOutput(generator, "return res;\n");
    }
    else {
      // descending
      ScopeOutput(generator, "return -res;\n");
    }
    ScopeOutput(generator, "}\n");
  }

  // return 0 if all elements are equal
  ScopeOutput(generator, "return 0;\n");
  ScopeOutput(generator, "}\n");

  // finish scope
  EndScope(generator);

  return functionIndex;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for a group function
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_codegen_register_t CreateGroupFunction (TRI_aql_codegen_js_t* const generator,
                                                       const TRI_aql_node_t* const node) {
  TRI_aql_node_t* list;
  TRI_aql_codegen_scope_t* scope;
  TRI_aql_codegen_register_t functionIndex = IncFunction(generator);
  size_t i;
  size_t n;

  // start a new scope first
  StartScope(generator, &generator->_functionBuffer, TRI_AQL_SCOPE_FUNCTION, 0, 0, 0, 0, NULL);
  scope = CurrentScope(generator);
  scope->_prefix = "g";

  ScopeOutput(generator, "function ");
  ScopeOutputFunction(generator, functionIndex);
  ScopeOutput(generator, " (g) {\n");
  ScopeOutput(generator, "return { ");

  // loop over group criteria
  list = TRI_AQL_NODE_MEMBER(node, 0);
  n = list->_members._length;
  for (i = 0; i < n; ++i) {
    TRI_aql_node_t* element = TRI_AQL_NODE_MEMBER(list, i);
    TRI_aql_node_t* varNode = TRI_AQL_NODE_MEMBER(element, 0);
    if (i > 0) {
      ScopeOutput(generator, ", ");
    }
    ScopeOutputQuoted(generator, TRI_AQL_NODE_STRING(varNode));
    ScopeOutput(generator, " : ");
    ProcessNode(generator, TRI_AQL_NODE_MEMBER(element, 1));
  }
  ScopeOutput(generator, " };\n");
  ScopeOutput(generator, "}\n");

  // finish scope
  EndScope(generator);

  return functionIndex;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create code for an empty list (var x = [ ];)
////////////////////////////////////////////////////////////////////////////////

static void InitList (TRI_aql_codegen_js_t* const generator,
                      const TRI_aql_codegen_register_t resultRegister) {
  ScopeOutput(generator, "var ");
  ScopeOutputRegister(generator, resultRegister);
  ScopeOutput(generator, " = [ ];\n");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create code for an empty array (var x = { };)
////////////////////////////////////////////////////////////////////////////////

static void InitArray (TRI_aql_codegen_js_t* const generator,
                       const TRI_aql_codegen_register_t resultRegister) {
  ScopeOutput(generator, "var ");
  ScopeOutputRegister(generator, resultRegister);
  ScopeOutput(generator, " = { };\n");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief enter a variable into the symbol table
////////////////////////////////////////////////////////////////////////////////

static void EnterSymbol (TRI_aql_codegen_js_t* const generator,
                         const char* const name,
                         const TRI_aql_codegen_register_t registerIndex) {
  TRI_aql_codegen_scope_t* scope; 
  TRI_aql_codegen_variable_t* variable = CreateVariable(name, registerIndex);

  if (variable == NULL) {
    generator->_errorCode = TRI_ERROR_OUT_OF_MEMORY;
    return;
  }

  // if not a temporary variable
  if (*name != '_') {
    scope = CurrentScope(generator);
  } 
  else {
    assert(generator->_scopes._length > 0);
  
    // get scope at level 0
    scope = (TRI_aql_codegen_scope_t*) TRI_AtVectorPointer(&generator->_scopes, 0);
  }

  if (TRI_InsertKeyAssociativePointer(&scope->_variables, name, (void*) variable, false)) {
    // variable already exists in symbol table. this should never happen
    LOG_TRACE("variable already registered: %s", name);
    generator->_errorCode = TRI_ERROR_QUERY_VARIABLE_REDECLARED;
    generator->_errorValue = (char*) name;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief look up a variable in the symbol table
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_codegen_register_t LookupSymbol (TRI_aql_codegen_js_t* const generator,
                                                const char* const name) {
  size_t i = generator->_scopes._length;

  assert(name);

  // iterate from current scope to the top level scope
  while (i-- > 0) {
    TRI_aql_codegen_scope_t* scope = (TRI_aql_codegen_scope_t*) generator->_scopes._buffer[i];
    TRI_aql_codegen_variable_t* variable;

    variable = (TRI_aql_codegen_variable_t*) TRI_LookupByKeyAssociativePointer(&scope->_variables, name);
    // variable found in scope
    if (variable) {
      // return the variable register
      return variable->_register;
    }
  }

  // variable not found. this should never happen
  LOG_TRACE("variable not found: %s", name);
  generator->_errorCode = TRI_ERROR_QUERY_VARIABLE_NAME_UNKNOWN;
  generator->_errorValue = (char*) name;

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start a for loop
////////////////////////////////////////////////////////////////////////////////

static void StartFor (TRI_aql_codegen_js_t* const generator,
                      TRI_string_buffer_t* const buffer,
                      const TRI_aql_codegen_register_t sourceRegister,
                      const bool sourceIsList,
                      const char* const variableName,
                      TRI_aql_for_hint_t* const hint) {
  TRI_aql_codegen_register_t listRegister  = IncRegister(generator);
  TRI_aql_codegen_register_t keyRegister   = IncRegister(generator);
  TRI_aql_codegen_register_t ownRegister   = IncRegister(generator);
  TRI_aql_codegen_scope_t* scope           = CurrentScope(generator);
  TRI_aql_codegen_scope_t* forScope;

  // always start a new scope
  StartScope(generator,
             buffer,
             TRI_AQL_SCOPE_FOR,
             listRegister,
             keyRegister,
             ownRegister,
             scope->_resultRegister,
             hint);

  if (variableName) {
    EnterSymbol(generator, variableName, ownRegister);
  }

  ScopeOutput(generator, "var ");
  ScopeOutputRegister(generator, listRegister);

  if (sourceIsList) {
    // the source register we're using definitely is a list
    // we can therefore get rid of the function call to LIST()
    ScopeOutput(generator, " = ");
    ScopeOutputRegister(generator, sourceRegister);
    ScopeOutput(generator, ";\n");
  }
  else {
    // we're not sure whether the source is a list, so we need to force it to be a list
    ScopeOutput(generator, " = aql.LIST(");
    ScopeOutputRegister(generator, sourceRegister);
    ScopeOutput(generator, ");\n");
  }

  forScope = CurrentScope(generator);

  // limit: offset and limit registers
  if (forScope->_limitRegister != 0) {
    if (forScope->_offsetRegister != 0) {
      // offset register
      ScopeOutput(generator, "var ");
      ScopeOutputRegister(generator, forScope->_offsetRegister);
#if AQL_VERBOSE
      ScopeOutput(generator, " = 0; /* offset */\n");
#else
      ScopeOutput(generator, " = 0;\n");
#endif
    }

    // limit register
    ScopeOutput(generator, "var ");
    ScopeOutputRegister(generator, forScope->_limitRegister);
#if AQL_VERBOSE
    ScopeOutput(generator, " = 0; /* limit */\n");
#else
    ScopeOutput(generator, " = 0;\n");
#endif
  }


  // for (var keyx in listx)
  if (forScope->_hint != NULL &&
      forScope->_hint->_limit._status == TRI_AQL_LIMIT_USE &&
      ! forScope->_hint->_limit._hasFilter) {
    // we have a limit the we need to handle in the for loop
    TRI_aql_codegen_register_t maxRegister = IncRegister(generator);

    ScopeOutput(generator, "var ");
    ScopeOutputRegister(generator, maxRegister);
    ScopeOutput(generator, " = ");
    ScopeOutputUInt(generator, (uint64_t) (forScope->_hint->_limit._offset + forScope->_hint->_limit._limit));
    ScopeOutput(generator, ";\n");
    ScopeOutput(generator, "if (");
    ScopeOutputRegister(generator, listRegister);
    ScopeOutput(generator, ".length < ");
    ScopeOutputRegister(generator, maxRegister);
    ScopeOutput(generator, ") {\n");
    ScopeOutputRegister(generator, maxRegister);
    ScopeOutput(generator, " = ");
    ScopeOutputRegister(generator, listRegister);
    ScopeOutput(generator, ".length;\n");
    ScopeOutput(generator, "}\n");

    ScopeOutput(generator, "for (var ");
    ScopeOutputRegister(generator, keyRegister);
    ScopeOutput(generator, " = ");
    ScopeOutputUInt(generator, forScope->_hint->_limit._offset);
    ScopeOutput(generator, "; ");
    ScopeOutputRegister(generator, keyRegister);
    ScopeOutput(generator, " < ");
    ScopeOutputRegister(generator, maxRegister);
    ScopeOutput(generator, "; ++");
    ScopeOutputRegister(generator, keyRegister);
    ScopeOutput(generator, ") {\n");
  }
  else {
    // regular for loop
    ScopeOutput(generator, "for (var ");
    ScopeOutputRegister(generator, keyRegister);
    ScopeOutput(generator, " = 0; ");
    ScopeOutputRegister(generator, keyRegister);
    ScopeOutput(generator, " < ");
    ScopeOutputRegister(generator, listRegister);
    ScopeOutput(generator, ".length; ++");
    ScopeOutputRegister(generator, keyRegister);
    ScopeOutput(generator, ") {\n");
  }

  // var rx = listx[keyx];
  ScopeOutput(generator, "var ");
  ScopeOutputRegister(generator, forScope->_ownRegister);
  ScopeOutput(generator, " = ");
  ScopeOutputRegister(generator, forScope->_listRegister);
  ScopeOutput(generator, "[");
  ScopeOutputRegister(generator, forScope->_keyRegister);
  ScopeOutput(generator, "];\n");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief close for loops til the parent scope is reached
///
/// all for loops will be closed until another, non-for scope is reached
////////////////////////////////////////////////////////////////////////////////

static void CloseLoops (TRI_aql_codegen_js_t* const generator) {
  TRI_aql_codegen_scope_t* scope = CurrentScope(generator);

  if (scope->_type == TRI_AQL_SCOPE_MAIN || scope->_type == TRI_AQL_SCOPE_SUBQUERY) {
    // these scopes are closed by other means
    return;
  }

  // we are closing at least one scope
  while (true) {
    TRI_aql_scope_e type = scope->_type;
    ScopeOutput(generator, "}\n");
    EndScope(generator);

    // break if we reached the top level for loop
    if (type != TRI_AQL_SCOPE_FOR_NESTED) {
      break;
    }

    // next iteration
    scope = CurrentScope(generator);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief store scope symbols in a result variable
///
/// fetches all symbols from the current scope and all potentially available
/// parent for loops, pushes their values into a result register, and note the
/// symbol names
/// this function is called before a scope is closed and allows remembering
/// which variables were active in a scope. it is necessary to call this
/// function for language constructs like SORT, COLLECT etc. which close the
/// current scope and open a new scope afterwards with the same variables as
/// before
////////////////////////////////////////////////////////////////////////////////

static TRI_vector_string_t StoreSymbols (TRI_aql_codegen_js_t* const generator,
                                         const TRI_aql_codegen_register_t rowRegister) {
  TRI_vector_string_t variableNames;
  size_t i = generator->_scopes._length;

  TRI_InitVectorString(&variableNames, TRI_UNKNOWN_MEM_ZONE);

  // start at current scope
  while (i-- > 0) {
    TRI_aql_codegen_scope_t* peek = (TRI_aql_codegen_scope_t*) generator->_scopes._buffer[i];
    size_t j, n;

    // iterate over all variables in the scope
    n = peek->_variables._nrAlloc;
    for (j = 0; j < n; ++j) {
      TRI_aql_codegen_variable_t* variable = (TRI_aql_codegen_variable_t*) peek->_variables._table[j];
      char* copy;

      if (! variable) {
        continue;
      }

      // push all variable values into the rowRegister
      // row[name] = ...;
      ScopeOutputIndexedName(generator, rowRegister, variable->_name);
      ScopeOutput(generator, " = ");
      ScopeOutputRegister(generator, variable->_register);
      ScopeOutput(generator, ";\n");

      copy = TRI_DuplicateStringZ(TRI_UNKNOWN_MEM_ZONE, variable->_name);
      if (copy == NULL) {
        generator->_errorCode = TRI_ERROR_OUT_OF_MEMORY;
      }
      else {
        TRI_PushBackVectorString(&variableNames, copy);
      }
    }

    // break if we reached the top level for loop
    if (peek->_type != TRI_AQL_SCOPE_FOR_NESTED) {
      break;
    }

    // next scope
  }

  return variableNames;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief re-enter variables into the symbol table
///
/// re-enters all passed variables into the symbol table of the current scope
/// and pop them the current scope's iterator register
/// this function is after a scope is closed and StoreSymbols has been called.
/// it allows using the same variables as in the previous scope.
/// it is necessary to call this function for language constructs like SORT etc.
/// which close a scope and open a new scope afterwards with the same variables
/// as before
////////////////////////////////////////////////////////////////////////////////

static void RestoreSymbols (TRI_aql_codegen_js_t* const generator,
                            TRI_vector_string_t* const variableNames) {
  TRI_aql_codegen_scope_t* scope = CurrentScope(generator);
  size_t i, n;

  // iterate over all variables passed
  n = variableNames->_length;
  for (i = 0; i < n; ++i) {
    // create a new register for each variable
    TRI_aql_codegen_register_t registerIndex = IncRegister(generator);
    char* variableName = (char*) variableNames->_buffer[i];

    // re-enter symbols
    if (variableName) {
      EnterSymbol(generator, variableName, registerIndex);
    }

    // unpack variables from current scope's iterator register
    ScopeOutput(generator, "var ");
    ScopeOutputRegister(generator, registerIndex);
    ScopeOutput(generator, " = ");
    ScopeOutputIndexedName(generator, scope->_ownRegister, variableName);
    ScopeOutput(generator, ";\n");
  }

  TRI_DestroyVectorString(variableNames);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for primary index access
////////////////////////////////////////////////////////////////////////////////

static void GeneratePrimaryAccess (TRI_aql_codegen_js_t* const generator,
                                   const TRI_aql_index_t* const idx,
                                   const TRI_aql_collection_t* const collection,
                                   const char* const collectionName) {
  TRI_aql_field_access_t* fieldAccess;
  size_t n;

  n = idx->_fieldAccesses->_length;
  assert(n == 1);

  fieldAccess = (TRI_aql_field_access_t*) TRI_AtVectorPointer(idx->_fieldAccesses, 0);

  assert(fieldAccess->_type == TRI_AQL_ACCESS_EXACT ||
         fieldAccess->_type == TRI_AQL_ACCESS_LIST ||
         (fieldAccess->_type == TRI_AQL_ACCESS_REFERENCE &&
          fieldAccess->_value._reference._operator == TRI_AQL_NODE_OPERATOR_BINARY_EQ));

  if (fieldAccess->_type == TRI_AQL_ACCESS_LIST) {
    ScopeOutput(generator, "aql.GET_DOCUMENTS_PRIMARY_LIST('");
  }
  else {
    ScopeOutput(generator, "aql.GET_DOCUMENTS_PRIMARY('");
  }

  ScopeOutput(generator, collectionName);
  ScopeOutput(generator, "', ");
  ScopeOutputIndexId(generator, collectionName, idx);
  ScopeOutput(generator, ", ");
  if (fieldAccess->_type == TRI_AQL_ACCESS_REFERENCE) {
    assert(fieldAccess->_value._reference._operator == TRI_AQL_NODE_OPERATOR_BINARY_EQ);

    if (fieldAccess->_value._reference._type == TRI_AQL_REFERENCE_VARIABLE) {
      ScopeOutputRegister(generator, LookupSymbol(generator, fieldAccess->_value._reference._ref._name));
    }
    else {
      ProcessAttributeAccess(generator, fieldAccess->_value._reference._ref._node);
    }
  }
  else {
    ScopeOutputJson(generator, fieldAccess->_value._value);
  }
  ScopeOutput(generator, ")");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for hash index access
////////////////////////////////////////////////////////////////////////////////

static void GenerateHashAccess (TRI_aql_codegen_js_t* const generator,
                                const TRI_aql_index_t* const idx,
                                const TRI_aql_collection_t* const collection,
                                const char* const collectionName) {
  size_t i, n;

  n = idx->_fieldAccesses->_length;
  assert(n >= 1);

  if (n == 1) {
    // peek at first element and check if it is a list access
    TRI_aql_field_access_t* fieldAccess = (TRI_aql_field_access_t*) TRI_AtVectorPointer(idx->_fieldAccesses, 0);

    if (fieldAccess->_type == TRI_AQL_ACCESS_LIST) {
      ScopeOutput(generator, "aql.GET_DOCUMENTS_HASH_LIST('");
      ScopeOutput(generator, collectionName);
      ScopeOutput(generator, "', ");
      ScopeOutputIndexId(generator, collectionName, idx);
      ScopeOutput(generator, ", ");
      ScopeOutputQuoted2(generator, fieldAccess->_fullName + fieldAccess->_variableNameLength + 1);
      ScopeOutput(generator, ", ");
      ScopeOutputJson(generator, fieldAccess->_value._value);
      ScopeOutput(generator, ")");
      return;
    }
    // fall through to exact access
  }

  ScopeOutput(generator, "aql.GET_DOCUMENTS_HASH('");
  ScopeOutput(generator, collectionName);
  ScopeOutput(generator, "', ");
  ScopeOutputIndexId(generator, collectionName, idx);
  ScopeOutput(generator, ", { ");

  // write the example document
  for (i = 0; i < n; ++i) {
    TRI_aql_field_access_t* fieldAccess = (TRI_aql_field_access_t*) TRI_AtVectorPointer(idx->_fieldAccesses, i);

    assert(fieldAccess->_type == TRI_AQL_ACCESS_EXACT ||
           (fieldAccess->_type == TRI_AQL_ACCESS_REFERENCE &&
            fieldAccess->_value._reference._operator == TRI_AQL_NODE_OPERATOR_BINARY_EQ));

    if (i > 0) {
      ScopeOutput(generator, ", ");
    }

    ScopeOutputQuoted2(generator, fieldAccess->_fullName + fieldAccess->_variableNameLength + 1);
    ScopeOutput(generator, " : ");
    if (fieldAccess->_type == TRI_AQL_ACCESS_REFERENCE) {
      assert(fieldAccess->_value._reference._operator == TRI_AQL_NODE_OPERATOR_BINARY_EQ);

      if (fieldAccess->_value._reference._type == TRI_AQL_REFERENCE_VARIABLE) {
        ScopeOutputRegister(generator, LookupSymbol(generator, fieldAccess->_value._reference._ref._name));
      }
      else {
        ProcessAttributeAccess(generator, fieldAccess->_value._reference._ref._node);
      }
    }
    else {
      ScopeOutputJson(generator, fieldAccess->_value._value);
    }
  }

  ScopeOutput(generator, " })");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for edge index access
////////////////////////////////////////////////////////////////////////////////

static void GenerateEdgeAccess (TRI_aql_codegen_js_t* const generator,
                                const TRI_aql_index_t* const idx,
                                const TRI_aql_collection_t* const collection,
                                const char* const collectionName) {
  TRI_aql_field_access_t* fieldAccess;
  size_t n;

  n = idx->_fieldAccesses->_length;
  assert(n > 0);

  fieldAccess = (TRI_aql_field_access_t*) TRI_AtVectorPointer(idx->_fieldAccesses, 0);

  assert(fieldAccess->_type == TRI_AQL_ACCESS_EXACT ||
         fieldAccess->_type == TRI_AQL_ACCESS_LIST ||
         (fieldAccess->_type == TRI_AQL_ACCESS_REFERENCE &&
          fieldAccess->_value._reference._operator == TRI_AQL_NODE_OPERATOR_BINARY_EQ));

  if (fieldAccess->_type == TRI_AQL_ACCESS_LIST) {
    ScopeOutput(generator, "aql.GET_DOCUMENTS_EDGE_LIST('");
  }
  else {
    ScopeOutput(generator, "aql.GET_DOCUMENTS_EDGE('");
  }

  ScopeOutput(generator, collectionName);
  ScopeOutput(generator, "', ");
  ScopeOutputQuoted2(generator, fieldAccess->_fullName + fieldAccess->_variableNameLength + 1);
  ScopeOutput(generator, ", ");
  if (fieldAccess->_type == TRI_AQL_ACCESS_REFERENCE) {
    assert(fieldAccess->_value._reference._operator == TRI_AQL_NODE_OPERATOR_BINARY_EQ);

    if (fieldAccess->_value._reference._type == TRI_AQL_REFERENCE_VARIABLE) {
      ScopeOutputRegister(generator, LookupSymbol(generator, fieldAccess->_value._reference._ref._name));
    }
    else {
      ProcessAttributeAccess(generator, fieldAccess->_value._reference._ref._node);
    }
  }
  else {
    ScopeOutputJson(generator, fieldAccess->_value._value);
  }
  ScopeOutput(generator, ")");
}


////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for skiplist access
////////////////////////////////////////////////////////////////////////////////

static void GenerateSkiplistAccess (TRI_aql_codegen_js_t* const generator,
                                    const TRI_aql_index_t* const idx,
                                    const TRI_aql_collection_t* const collection,
                                    const char* const collectionName) {
  size_t i, n;

  n = idx->_fieldAccesses->_length;
  assert(n >= 1);

  if (n == 1) {
    // peek at first element and check if it is a list access
    TRI_aql_field_access_t* fieldAccess = (TRI_aql_field_access_t*) TRI_AtVectorPointer(idx->_fieldAccesses, 0);

    if (fieldAccess->_type == TRI_AQL_ACCESS_LIST) {
      ScopeOutput(generator, "aql.GET_DOCUMENTS_SKIPLIST_LIST('");
      ScopeOutput(generator, collectionName);
      ScopeOutput(generator, "', ");
      ScopeOutputIndexId(generator, collectionName, idx);
      ScopeOutput(generator, ", ");
      ScopeOutputQuoted2(generator, fieldAccess->_fullName + fieldAccess->_variableNameLength + 1);
      ScopeOutput(generator, ", ");
      ScopeOutputJson(generator, fieldAccess->_value._value);
      ScopeOutput(generator, ")");
      return;
    }
    // fall through to other access types
  }

  ScopeOutput(generator, "aql.GET_DOCUMENTS_SKIPLIST('");
  ScopeOutput(generator, collectionName);
  ScopeOutput(generator, "', ");
  ScopeOutputIndexId(generator, collectionName, idx);
  ScopeOutput(generator, ", { ");

  // write the example document
  for (i = 0; i < n; ++i) {
    TRI_aql_field_access_t* fieldAccess = (TRI_aql_field_access_t*) TRI_AtVectorPointer(idx->_fieldAccesses, i);

    assert(fieldAccess->_type == TRI_AQL_ACCESS_EXACT ||
           fieldAccess->_type == TRI_AQL_ACCESS_RANGE_SINGLE ||
           fieldAccess->_type == TRI_AQL_ACCESS_RANGE_DOUBLE ||
           fieldAccess->_type == TRI_AQL_ACCESS_REFERENCE);

    if (i > 0) {
      ScopeOutput(generator, ", ");
    }

    ScopeOutputQuoted2(generator, fieldAccess->_fullName + fieldAccess->_variableNameLength + 1); // offset);
    ScopeOutput(generator, " : [ ");

    if (fieldAccess->_type == TRI_AQL_ACCESS_EXACT) {
      ScopeOutput(generator, " [ \"==\", ");
      ScopeOutputJson(generator, fieldAccess->_value._value);
      ScopeOutput(generator, " ] ");
    }
    else if (fieldAccess->_type == TRI_AQL_ACCESS_RANGE_SINGLE) {
      ScopeOutput(generator, " [ \"");
      ScopeOutput(generator, TRI_RangeOperatorAql(fieldAccess->_value._singleRange._type));
      ScopeOutput(generator, "\", ");
      ScopeOutputJson(generator, fieldAccess->_value._singleRange._value);
      ScopeOutput(generator, " ] ");
    }
    else if (fieldAccess->_type == TRI_AQL_ACCESS_RANGE_DOUBLE) {
      // lower bound
      ScopeOutput(generator, " [ \"");
      ScopeOutput(generator, TRI_RangeOperatorAql(fieldAccess->_value._between._lower._type));
      ScopeOutput(generator, "\", ");
      ScopeOutputJson(generator, fieldAccess->_value._between._lower._value);
      ScopeOutput(generator, " ], [ \"");
      // upper bound
      ScopeOutput(generator, TRI_RangeOperatorAql(fieldAccess->_value._between._upper._type));
      ScopeOutput(generator, "\", ");
      ScopeOutputJson(generator, fieldAccess->_value._between._upper._value);
      ScopeOutput(generator, " ] ");
    }
    else if (fieldAccess->_type == TRI_AQL_ACCESS_REFERENCE) {
      ScopeOutput(generator, " [ \"");
      ScopeOutput(generator, TRI_ComparisonOperatorAql(fieldAccess->_value._reference._operator));
      ScopeOutput(generator, "\", ");

      if (fieldAccess->_value._reference._type == TRI_AQL_REFERENCE_VARIABLE) {
        ScopeOutputRegister(generator, LookupSymbol(generator, fieldAccess->_value._reference._ref._name));
      }
      else {
        ProcessAttributeAccess(generator, fieldAccess->_value._reference._ref._node);
      }
      ScopeOutput(generator, " ] ");
    }

    ScopeOutput(generator, " ] ");
  }

  ScopeOutput(generator, " })");
}


////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for bitarray access
////////////////////////////////////////////////////////////////////////////////

static void GenerateBitarrayAccess (TRI_aql_codegen_js_t* const generator,
                                    const TRI_aql_index_t* const idx,
                                    const TRI_aql_collection_t* const collection,
                                    const char* const collectionName) {
  size_t i, n;

  n = idx->_fieldAccesses->_length;
  assert(n >= 1);


  if (n == 1) {
    // peek at first element and check if it is a list access
    TRI_aql_field_access_t* fieldAccess = (TRI_aql_field_access_t*) TRI_AtVectorPointer(idx->_fieldAccesses, 0);

    if (fieldAccess->_type == TRI_AQL_ACCESS_LIST) {
      assert(false);
      ScopeOutput(generator, "aql.GET_DOCUMENTS_BITARRAY_LIST('");
      ScopeOutput(generator, collectionName);
      ScopeOutput(generator, "', ");
      ScopeOutputIndexId(generator, collectionName, idx);
      ScopeOutput(generator, ", ");
      ScopeOutputQuoted2(generator, fieldAccess->_fullName + fieldAccess->_variableNameLength + 1);
      ScopeOutput(generator, ", ");
      ScopeOutputJson(generator, fieldAccess->_value._value);
      ScopeOutput(generator, ")");
      return;
    }
    // fall through to other access types
  }

  ScopeOutput(generator, "aql.GET_DOCUMENTS_BITARRAY('");
  ScopeOutput(generator, collectionName);
  ScopeOutput(generator, "', ");
  ScopeOutputIndexId(generator, collectionName, idx);
  ScopeOutput(generator, ", { \"==\" : {"); // only support the equality operator for now


  // ..................................................................................
  // Construct the javascript object which will eventually be used to generate
  // the index operator. The object is in the form: {"==": {"x":0}}
  // ..................................................................................

  for (i = 0; i < n; ++i) {
    TRI_aql_field_access_t* fieldAccess = (TRI_aql_field_access_t*) TRI_AtVectorPointer(idx->_fieldAccesses, i);

    // ................................................................................
    // Only implement equality operator for now
    // ................................................................................

    ScopeOutputQuoted2(generator, fieldAccess->_fullName + fieldAccess->_variableNameLength + 1);
    ScopeOutput(generator, " : ");

    switch (fieldAccess->_type) {

      case TRI_AQL_ACCESS_EXACT: {
        ScopeOutputJson(generator, fieldAccess->_value._value);
        break;
      }

      case TRI_AQL_ACCESS_REFERENCE: {
        ProcessAttributeAccess(generator, fieldAccess->_value._reference._ref._node);
      }

      default: {
        assert(false);
      }
    }

    if (i < (n - 1)) {
      ScopeOutput(generator, ", ");
    }

  }

  ScopeOutput(generator, "} })");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for a reference (the name of a variable)
////////////////////////////////////////////////////////////////////////////////

static void ProcessReference (TRI_aql_codegen_js_t* const generator,
                              const TRI_aql_node_t* const node) {
  TRI_aql_codegen_scope_t* scope = CurrentScope(generator);
  char* name = TRI_AQL_NODE_STRING(node);

  if (scope->_prefix) {
    // sort function, group function etc. - we are using a variable
    ScopeOutput(generator, scope->_prefix);
    ScopeOutput(generator, "[");
    ScopeOutputQuoted(generator, name);
    ScopeOutput(generator, "]");
  }
  else {
    // regular scope, we are using a register
    TRI_aql_codegen_register_t registerIndex = LookupSymbol(generator, name);
    ScopeOutputRegister(generator, registerIndex);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for a value literal
////////////////////////////////////////////////////////////////////////////////

static void ProcessValue (TRI_aql_codegen_js_t* const generator,
                          const TRI_aql_node_t* const node) {
  TRI_aql_codegen_scope_t* scope = CurrentScope(generator);

  if (! scope->_buffer) {
    return;
  }

  if (! TRI_ValueJavascriptAql(scope->_buffer, &node->_value, node->_value._type)) {
    generator->_errorCode = TRI_ERROR_OUT_OF_MEMORY;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for a list ([ ])
////////////////////////////////////////////////////////////////////////////////

static void ProcessList (TRI_aql_codegen_js_t* const generator,
                         const TRI_aql_node_t* const node) {
  size_t i, n;

  ScopeOutput(generator, "[ ");
  n = node->_members._length;
  for (i = 0; i < n; ++i) {
    if (i > 0) {
      ScopeOutput(generator, ", ");
    }
    ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, i));
  }
  ScopeOutput(generator, " ]");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for an array ({ })
////////////////////////////////////////////////////////////////////////////////

static void ProcessArray (TRI_aql_codegen_js_t* const generator,
                          const TRI_aql_node_t* const node) {
  size_t i, n;

  ScopeOutput(generator, "{ ");
  n = node->_members._length;
  for (i = 0; i < n; ++i) {
    if (i > 0) {
      ScopeOutput(generator, ", ");
    }
    ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, i));
  }
  ScopeOutput(generator, " }");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for a single named array element
////////////////////////////////////////////////////////////////////////////////

static void ProcessArrayElement (TRI_aql_codegen_js_t* const generator,
                                const TRI_aql_node_t* const node) {
  TRI_aql_codegen_scope_t* scope = CurrentScope(generator);

  if (! scope->_buffer) {
    return;
  }

  TRI_ValueJavascriptAql(scope->_buffer, &node->_value, TRI_AQL_TYPE_STRING);
  ScopeOutput(generator, " : ");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for a function call argument list
////////////////////////////////////////////////////////////////////////////////

static void ProcessArgList (TRI_aql_codegen_js_t* const generator,
                            const TRI_aql_function_t* const function,
                            const TRI_aql_node_t* const node) {
  size_t i, n;

  n = node->_members._length;
  for (i = 0; i < n; ++i) {
    TRI_aql_node_t* parameter = TRI_AQL_NODE_MEMBER(node, i);

    if (i > 0) {
      ScopeOutput(generator, ", ");
    }

    if (function != NULL && 
        parameter->_type == TRI_AQL_NODE_COLLECTION &&
        TRI_ConvertParameterFunctionAql(function, i)) {
      // collection arguments will be created as string argument => e.g. "users"
      TRI_aql_node_t* nameNode = TRI_AQL_NODE_MEMBER(parameter, 0);

      ScopeOutputQuoted(generator, TRI_AQL_NODE_STRING(nameNode));
    }
    else {
      // anything else will be created as is
      ProcessNode(generator, parameter);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for an attribute name in exapnd (e.g. users[*].name)
////////////////////////////////////////////////////////////////////////////////

static void ProcessAttribute (TRI_aql_codegen_js_t* const generator,
                              const TRI_aql_node_t* const node) {
  TRI_aql_codegen_scope_t* scope = CurrentScope(generator);

  ScopeOutput(generator, "aql.DOCUMENT_MEMBER(");
  ScopeOutputRegister(generator, scope->_ownRegister);
  ScopeOutput(generator, ", ");
  ScopeOutputQuoted(generator, TRI_AQL_NODE_STRING(node));
  ScopeOutput(generator, ")");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for an attribute name (e.g. users.name)
////////////////////////////////////////////////////////////////////////////////

static void ProcessAttributeAccess (TRI_aql_codegen_js_t* const generator,
                                    const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "aql.DOCUMENT_MEMBER(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ", ");
  ScopeOutputQuoted(generator, TRI_AQL_NODE_STRING(node));
  ScopeOutput(generator, ")");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for indexed access (e.g. users[0])
////////////////////////////////////////////////////////////////////////////////

static void ProcessIndexed (TRI_aql_codegen_js_t* const generator,
                            const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "aql.GET_INDEX(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ", ");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  ScopeOutput(generator, ")");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for collection access (full table scan)
/// offset & limit values might be applied here if they were collected by the
/// optimiser during the optimisation phase
////////////////////////////////////////////////////////////////////////////////

static void ProcessCollectionFull (TRI_aql_codegen_js_t* const generator,
                                   const TRI_aql_node_t* const node) {
  TRI_aql_node_t* nameNode = TRI_AQL_NODE_MEMBER(node, 0);
  TRI_aql_collection_hint_t* hint = (TRI_aql_collection_hint_t*) TRI_AQL_NODE_DATA(node);

  ScopeOutput(generator, "aql.GET_DOCUMENTS(");
  ProcessNode(generator, nameNode);

  if (hint != NULL && hint->_limit._status == TRI_AQL_LIMIT_USE) {
    // use limit values collected during optimisation
    ScopeOutput(generator, ", ");

    // offset
    ScopeOutputUInt(generator, (uint64_t) hint->_limit._offset);

    ScopeOutput(generator, ", ");
    // now the limit value
    ScopeOutputUInt(generator, (uint64_t) hint->_limit._limit);
  }

  ScopeOutput(generator, ")");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for collection access (hinted access)
////////////////////////////////////////////////////////////////////////////////

static void ProcessCollectionHinted (TRI_aql_codegen_js_t* const generator,
                                     const TRI_aql_node_t* const node) {
  TRI_aql_node_t* nameNode = TRI_AQL_NODE_MEMBER(node, 0);
  TRI_aql_collection_hint_t* hint = (TRI_aql_collection_hint_t*) TRI_AQL_NODE_DATA(node);
  char* collectionName;

  collectionName = TRI_AQL_NODE_STRING(nameNode);

  switch (hint->_index->_idx->_type) {
    case TRI_IDX_TYPE_GEO1_INDEX:
    case TRI_IDX_TYPE_GEO2_INDEX:
    case TRI_IDX_TYPE_PRIORITY_QUEUE_INDEX:
    case TRI_IDX_TYPE_CAP_CONSTRAINT:
    case TRI_IDX_TYPE_FULLTEXT_INDEX:
      // these index types are not yet supported
      generator->_errorCode = TRI_ERROR_INTERNAL;
      break;

    case TRI_IDX_TYPE_PRIMARY_INDEX:
      GeneratePrimaryAccess(generator, hint->_index, hint->_collection, collectionName);
      break;

    case TRI_IDX_TYPE_HASH_INDEX:
      GenerateHashAccess(generator, hint->_index, hint->_collection, collectionName);
      break;

    case TRI_IDX_TYPE_EDGE_INDEX:
      GenerateEdgeAccess(generator, hint->_index, hint->_collection, collectionName);
      break;

    case TRI_IDX_TYPE_SKIPLIST_INDEX:
      GenerateSkiplistAccess(generator, hint->_index, hint->_collection, collectionName);
      break;

    case TRI_IDX_TYPE_BITARRAY_INDEX: {
      GenerateBitarrayAccess(generator, hint->_index, hint->_collection, collectionName);
      break;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for collection access
////////////////////////////////////////////////////////////////////////////////

static void ProcessCollection (TRI_aql_codegen_js_t* const generator,
                               const TRI_aql_node_t* const node) {
  TRI_aql_collection_hint_t* hint = (TRI_aql_collection_hint_t*) (TRI_AQL_NODE_DATA(node));

  assert(hint);

  if (hint->_index == NULL) {
    ProcessCollectionFull(generator, node);
  }
  else {
    ProcessCollectionHinted(generator, node);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for expand (e.g. users[*]....)
////////////////////////////////////////////////////////////////////////////////

static void ProcessExpand (TRI_aql_codegen_js_t* const generator,
                           const TRI_aql_node_t* const node) {
  TRI_aql_codegen_scope_t* scope = CurrentScope(generator);
  TRI_aql_node_t* nameNode1 = TRI_AQL_NODE_MEMBER(node, 0);
  TRI_aql_node_t* nameNode2 = TRI_AQL_NODE_MEMBER(node, 1);
  TRI_aql_node_t* expressionNode = TRI_AQL_NODE_MEMBER(node, 2);
  TRI_aql_codegen_register_t sourceRegister = IncRegister(generator);
  TRI_aql_codegen_register_t resultRegister = IncRegister(generator);
  bool isList = TRI_IsListNodeAql(expressionNode);

  // init source
  ScopeOutput(generator, "var ");
  ScopeOutputRegister(generator, sourceRegister);
  ScopeOutput(generator, " = ");
  ProcessNode(generator, expressionNode);
  ScopeOutput(generator, ";\n");

  // var result = [ ];
  InitList(generator, resultRegister);

  // expand scope
  StartScope(generator, scope->_buffer, TRI_AQL_SCOPE_EXPAND, 0, 0, 0, resultRegister, NULL);

  // for
  StartFor(generator, scope->_buffer, sourceRegister, isList, NULL, NULL);
  EnterSymbol(generator, TRI_AQL_NODE_STRING(nameNode1), CurrentScope(generator)->_ownRegister);

  ScopeOutputRegister(generator, resultRegister);
  ScopeOutput(generator, ".push(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 3));
  ScopeOutput(generator, ");\n");

  // }
  CloseLoops(generator);

  // expand scope
  EndScope(generator);

  // register the variable for the result
  EnterSymbol(generator, TRI_AQL_NODE_STRING(nameNode2), resultRegister);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for unary not (!value)
////////////////////////////////////////////////////////////////////////////////

static void ProcessUnaryNot (TRI_aql_codegen_js_t* const generator,
                             const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "aql.LOGICAL_NOT(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ")");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for unary minus (-value)
////////////////////////////////////////////////////////////////////////////////

static void ProcessUnaryMinus (TRI_aql_codegen_js_t* const generator,
                               const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "aql.UNARY_MINUS(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ")");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for unary plus (+value)
////////////////////////////////////////////////////////////////////////////////

static void ProcessUnaryPlus (TRI_aql_codegen_js_t* const generator,
                              const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "aql.UNARY_PLUS(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ")");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for binary and (a && b)
////////////////////////////////////////////////////////////////////////////////

static void ProcessBinaryAnd (TRI_aql_codegen_js_t* const generator,
                              const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "aql.LOGICAL_AND(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ", ");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  ScopeOutput(generator, ")");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for binary or (a || b)
////////////////////////////////////////////////////////////////////////////////

static void ProcessBinaryOr (TRI_aql_codegen_js_t* const generator,
                             const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "aql.LOGICAL_OR(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ", ");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  ScopeOutput(generator, ")");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for binary plus (a + b)
////////////////////////////////////////////////////////////////////////////////

static void ProcessBinaryPlus (TRI_aql_codegen_js_t* const generator,
                               const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "aql.ARITHMETIC_PLUS(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ", ");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  ScopeOutput(generator, ")");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for binary minus (a - b)
////////////////////////////////////////////////////////////////////////////////

static void ProcessBinaryMinus (TRI_aql_codegen_js_t* const generator,
                                const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "aql.ARITHMETIC_MINUS(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ", ");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  ScopeOutput(generator, ")");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for binary times (a * b)
////////////////////////////////////////////////////////////////////////////////

static void ProcessBinaryTimes (TRI_aql_codegen_js_t* const generator,
                                const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "aql.ARITHMETIC_TIMES(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ", ");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  ScopeOutput(generator, ")");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for binary divide (a / b)
////////////////////////////////////////////////////////////////////////////////

static void ProcessBinaryDivide (TRI_aql_codegen_js_t* const generator,
                                 const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "aql.ARITHMETIC_DIVIDE(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ", ");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  ScopeOutput(generator, ")");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for binary modulus (a % b)
////////////////////////////////////////////////////////////////////////////////

static void ProcessBinaryModulus(TRI_aql_codegen_js_t* const generator,
                                 const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "aql.ARITHMETIC_MODULUS(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ", ");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  ScopeOutput(generator, ")");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for binary equality (a == b)
////////////////////////////////////////////////////////////////////////////////

static void ProcessBinaryEqual (TRI_aql_codegen_js_t* const generator,
                                const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "aql.RELATIONAL_EQUAL(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ", ");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  ScopeOutput(generator, ")");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for binary inequality (a != b)
////////////////////////////////////////////////////////////////////////////////

static void ProcessBinaryUnequal (TRI_aql_codegen_js_t* const generator,
                                  const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "aql.RELATIONAL_UNEQUAL(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ", ");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  ScopeOutput(generator, ")");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for binary less equal (a <= b)
////////////////////////////////////////////////////////////////////////////////

static void ProcessBinaryLessEqual (TRI_aql_codegen_js_t* const generator,
                                    const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "aql.RELATIONAL_LESSEQUAL(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ", ");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  ScopeOutput(generator, ")");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for binary less (a < b)
////////////////////////////////////////////////////////////////////////////////

static void ProcessBinaryLess (TRI_aql_codegen_js_t* const generator,
                               const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "aql.RELATIONAL_LESS(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ", ");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  ScopeOutput(generator, ")");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for binary greater equal (a >= b)
////////////////////////////////////////////////////////////////////////////////

static void ProcessBinaryGreaterEqual (TRI_aql_codegen_js_t* const generator,
                                       const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "aql.RELATIONAL_GREATEREQUAL(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ", ");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  ScopeOutput(generator, ")");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for binary greater (a > b)
////////////////////////////////////////////////////////////////////////////////

static void ProcessBinaryGreater (TRI_aql_codegen_js_t* const generator,
                                 const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "aql.RELATIONAL_GREATER(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ", ");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  ScopeOutput(generator, ")");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for binary in (a in b)
////////////////////////////////////////////////////////////////////////////////

static void ProcessBinaryIn (TRI_aql_codegen_js_t* const generator,
                             const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "aql.RELATIONAL_IN(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ", ");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  ScopeOutput(generator, ")");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for ternary
////////////////////////////////////////////////////////////////////////////////

static void ProcessTernary (TRI_aql_codegen_js_t* const generator,
                            const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "aql.TERNARY_OPERATOR(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ", ");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  ScopeOutput(generator, ", ");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 2));
  ScopeOutput(generator, ")");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for subqueries (nested fors etc.)
////////////////////////////////////////////////////////////////////////////////

static void ProcessSubquery (TRI_aql_codegen_js_t* const generator,
                             const TRI_aql_node_t* const node) {
  TRI_aql_codegen_register_t subQueryRegister;
  TRI_aql_codegen_register_t resultRegister = IncRegister(generator);
  TRI_aql_node_t* nameNode = TRI_AQL_NODE_MEMBER(node, 0);

  // get the subquery scope's result register
  subQueryRegister = generator->_lastResultRegister;

  ScopeOutput(generator, "var ");
  ScopeOutputRegister(generator, resultRegister);
  ScopeOutput(generator, " = ");
  ScopeOutputRegister(generator, subQueryRegister);
#if AQL_VERBOSE
  ScopeOutput(generator, "; /* subquery */\n");
#else
  ScopeOutput(generator, ";\n");
#endif

  EnterSymbol(generator, nameNode->_value._value._string, resultRegister);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for function calls
////////////////////////////////////////////////////////////////////////////////

static void ProcessFcallInternal (TRI_aql_codegen_js_t* const generator,
                                  const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "aql.");
  ScopeOutput(generator, TRI_GetInternalNameFunctionAql((TRI_aql_function_t*) TRI_AQL_NODE_DATA(node)));
  ScopeOutput(generator, "(");
  ProcessArgList(generator, (TRI_aql_function_t*) TRI_AQL_NODE_DATA(node), TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ")");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for function calls
////////////////////////////////////////////////////////////////////////////////

static void ProcessFcallUser (TRI_aql_codegen_js_t* const generator,
                                  const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "aql.FCALL_USER(");
  ScopeOutputQuoted(generator, TRI_AQL_NODE_STRING(node));
  ScopeOutput(generator, ", [");
  ProcessArgList(generator, NULL, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, "])");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for a scope start
////////////////////////////////////////////////////////////////////////////////

static void ProcessScopeStart (TRI_aql_codegen_js_t* const generator,
                               const TRI_aql_node_t* const node) {
  TRI_aql_codegen_register_t resultRegister;
  TRI_aql_codegen_register_t sourceRegister;
  TRI_aql_scope_t* scope = (TRI_aql_scope_t*) TRI_AQL_NODE_DATA(node);

  if (scope->_type != TRI_AQL_SCOPE_SUBQUERY) {
    return;
  }

  resultRegister = IncRegister(generator);
  InitList(generator, resultRegister);
  StartScope(generator, &generator->_buffer, TRI_AQL_SCOPE_SUBQUERY, 0, 0, 0, resultRegister, NULL);

  // start a dummy for loop for subqueries
  // TODO: validate if this is necessary
  sourceRegister = IncRegister(generator);
  ScopeOutput(generator, "var ");
  ScopeOutputRegister(generator, sourceRegister);
  ScopeOutput(generator, " = [ 1 ];\n"); // just one iteration
  StartFor(generator, &generator->_buffer, sourceRegister, true, NULL, NULL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for a scope end
////////////////////////////////////////////////////////////////////////////////

static void ProcessScopeEnd (TRI_aql_codegen_js_t* const generator,
                             const TRI_aql_node_t* const node) {
  TRI_aql_scope_t* scope = (TRI_aql_scope_t*) TRI_AQL_NODE_DATA(node);

  if (scope->_type != TRI_AQL_SCOPE_SUBQUERY) {
    return;
  }

  EndScope(generator);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for for keyword
////////////////////////////////////////////////////////////////////////////////

static void ProcessFor (TRI_aql_codegen_js_t* const generator,
                        const TRI_aql_node_t* const node) {
  TRI_aql_codegen_scope_t* scope = CurrentScope(generator);
  TRI_aql_node_t* nameNode = TRI_AQL_NODE_MEMBER(node, 0);
  TRI_aql_node_t* expressionNode = TRI_AQL_NODE_MEMBER(node, 1);
  TRI_aql_codegen_register_t sourceRegister = IncRegister(generator);
  TRI_aql_for_hint_t* hint = TRI_AQL_NODE_DATA(node);
  TRI_string_buffer_t* buffer;
  bool isList;

  isList = (TRI_IsListNodeAql(expressionNode) ||
            expressionNode->_type == TRI_AQL_NODE_COLLECTION);

  buffer = scope->_buffer; // inherit buffer from current scope

  ScopeOutput(generator, "var ");
  ScopeOutputRegister(generator, sourceRegister);
  ScopeOutput(generator, " = ");
  ProcessNode(generator, expressionNode);
  ScopeOutput(generator, ";\n");

  StartFor(generator, buffer, sourceRegister, isList, TRI_AQL_NODE_STRING(nameNode), hint);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for sort keyword
////////////////////////////////////////////////////////////////////////////////

static void ProcessSort (TRI_aql_codegen_js_t* const generator,
                         const TRI_aql_node_t* const node) {
  TRI_aql_codegen_scope_t* scope = CurrentScope(generator);
  TRI_aql_codegen_register_t rowRegister = IncRegister(generator);
  TRI_aql_codegen_register_t sourceRegister = scope->_resultRegister;
  TRI_aql_codegen_register_t resultRegister;
  TRI_aql_codegen_register_t functionIndex;
  TRI_vector_string_t variableNames;

  // var row = { };
  InitArray(generator, rowRegister);

  // save symbols
  variableNames = StoreSymbols(generator, rowRegister);

  // result.push(row)
  ScopeOutputRegister(generator, scope->_resultRegister);
  ScopeOutput(generator, ".push(");
  ScopeOutputRegister(generator, rowRegister);
  ScopeOutput(generator, ");\n");

  // }
  CloseLoops(generator);

  functionIndex = CreateSortFunction(generator, node, 0);

  // now apply actual sorting
  ScopeOutput(generator, "aql.SORT(");
  ScopeOutputRegister(generator, sourceRegister);
  ScopeOutput(generator, ", ");
  ScopeOutputFunction(generator, functionIndex);
  ScopeOutput(generator, ");\n");

  // start new for loop
  resultRegister = IncRegister(generator);
  scope = CurrentScope(generator);
  scope->_resultRegister = resultRegister;

  InitList(generator, resultRegister);
  StartFor(generator, scope->_buffer, sourceRegister, true, NULL, NULL);

  // restore symbols
  RestoreSymbols(generator, &variableNames);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for collect keyword
////////////////////////////////////////////////////////////////////////////////

static void ProcessCollect (TRI_aql_codegen_js_t* const generator,
                            const TRI_aql_node_t* const node) {
  TRI_aql_codegen_scope_t* scope = CurrentScope(generator);
  TRI_aql_codegen_register_t rowRegister = IncRegister(generator);
  TRI_aql_codegen_register_t sourceRegister = scope->_resultRegister;
  TRI_aql_codegen_register_t groupRegister = IncRegister(generator);
  TRI_aql_codegen_register_t resultRegister;
  TRI_aql_codegen_register_t sortFunctionIndex;
  TRI_aql_codegen_register_t groupFunctionIndex;
  TRI_vector_string_t variableNames;
  TRI_aql_node_t* list;
  size_t i, n;

  // var row = { };
  InitArray(generator, rowRegister);

  // we are not interested in the variable names here, but
  // StoreSymbols also generates code to save the current state in
  // a rowRegister, and we need that
  variableNames = StoreSymbols(generator, rowRegister);
  TRI_DestroyVectorString(&variableNames);

  // result.push(row)
  ScopeOutputRegister(generator, scope->_resultRegister);
  ScopeOutput(generator, ".push(");
  ScopeOutputRegister(generator, rowRegister);
  ScopeOutput(generator, ");\n");

  // }
  CloseLoops(generator);

  // sort function
  sortFunctionIndex = CreateSortFunction(generator, node, 1);

  // group function
  groupFunctionIndex = CreateGroupFunction(generator, node);

  // now apply actual grouping
  ScopeOutput(generator, "var ");
  ScopeOutputRegister(generator, groupRegister);
  ScopeOutput(generator, " = ");
  ScopeOutput(generator, "aql.GROUP(");
  ScopeOutputRegister(generator, sourceRegister);
  ScopeOutput(generator, ", ");
  ScopeOutputFunction(generator, sortFunctionIndex);
  ScopeOutput(generator, ", ");
  ScopeOutputFunction(generator, groupFunctionIndex);

  // into
  if (node->_members._length > 1) {
    TRI_aql_node_t* nameNode = TRI_AQL_NODE_MEMBER(node, 1);
    ScopeOutput(generator, ", ");
    ScopeOutputQuoted(generator, TRI_AQL_NODE_STRING(nameNode));
  }
  ScopeOutput(generator, ");\n");

  // start new for loop
  resultRegister = IncRegister(generator);
  scope = CurrentScope(generator);
  scope->_resultRegister = resultRegister;
  InitList(generator, resultRegister);
  StartFor(generator, scope->_buffer, groupRegister, true, NULL, NULL);
  scope = CurrentScope(generator);

  // re-enter symbols for collect variables
  list = TRI_AQL_NODE_MEMBER(node, 0);
  n = list->_members._length;
  for (i = 0; i < n; ++i) {
    TRI_aql_node_t* element = TRI_AQL_NODE_MEMBER(list, i);
    TRI_aql_node_t* varNode = TRI_AQL_NODE_MEMBER(element, 0);
    TRI_aql_codegen_register_t varRegister = IncRegister(generator);

    // var collect = temp['collect'];
    ScopeOutput(generator, "var ");
    ScopeOutputRegister(generator, varRegister);
    ScopeOutput(generator, " = ");
    ScopeOutputIndexedName(generator, scope->_ownRegister, TRI_AQL_NODE_STRING(varNode));
    ScopeOutput(generator, ";\n");

    EnterSymbol(generator, TRI_AQL_NODE_STRING(varNode), varRegister);
  }

  // re-enter symbol for into
  if (node->_members._length > 1) {
    TRI_aql_node_t* nameNode = TRI_AQL_NODE_MEMBER(node, 1);
    TRI_aql_codegen_register_t intoRegister = IncRegister(generator);

    // var into = temp['into'];
    ScopeOutput(generator, "var ");
    ScopeOutputRegister(generator, intoRegister);
    ScopeOutput(generator, " = ");
    ScopeOutputIndexedName(generator, scope->_ownRegister, TRI_AQL_NODE_STRING(nameNode));
    ScopeOutput(generator, ";\n");

    EnterSymbol(generator, TRI_AQL_NODE_STRING(nameNode), intoRegister);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for limit keyword
////////////////////////////////////////////////////////////////////////////////

static void ProcessLimit (TRI_aql_codegen_js_t* const generator,
                          const TRI_aql_node_t* const node) {
  TRI_aql_codegen_scope_t* scope = CurrentScope(generator);
  TRI_aql_codegen_register_t rowRegister = IncRegister(generator);
  TRI_aql_codegen_register_t sourceRegister = scope->_resultRegister;
  TRI_aql_codegen_register_t resultRegister;
  TRI_aql_codegen_register_t limitRegister;
  TRI_vector_string_t variableNames;

  // var row = { };
  InitArray(generator, rowRegister);

  // save symbols
  variableNames = StoreSymbols(generator, rowRegister);

  // result.push(row)
  ScopeOutputRegister(generator, scope->_resultRegister);
  ScopeOutput(generator, ".push(");
  ScopeOutputRegister(generator, rowRegister);
  ScopeOutput(generator, ");\n");

  // }
  CloseLoops(generator);

  // now apply actual limit
  limitRegister = IncRegister(generator);
  ScopeOutput(generator, "var ");
  ScopeOutputRegister(generator, limitRegister);
  ScopeOutput(generator, " = aql.LIMIT(");
  ScopeOutputRegister(generator, sourceRegister);
  ScopeOutput(generator, ", ");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ", ");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  ScopeOutput(generator, ");\n");

  // start new for loop
  resultRegister = IncRegister(generator);
  scope = CurrentScope(generator);
  scope->_resultRegister = resultRegister;
  InitList(generator, resultRegister);
  StartFor(generator, scope->_buffer, limitRegister, true, NULL, NULL);

  // restore symbols
  RestoreSymbols(generator, &variableNames);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for return keyword
////////////////////////////////////////////////////////////////////////////////

static void ProcessReturn (TRI_aql_codegen_js_t* const generator,
                           const TRI_aql_node_t* const node) {
  TRI_aql_codegen_scope_t* scope = CurrentScope(generator);
  TRI_aql_codegen_register_t rowRegister = IncRegister(generator);

  // var row = ...;
  ScopeOutput(generator, "var ");
  ScopeOutputRegister(generator, rowRegister);
  ScopeOutput(generator, " = ");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ";\n");

  // result.push(row);
  ScopeOutputRegister(generator, scope->_resultRegister);
  ScopeOutput(generator, ".push(");
  ScopeOutputRegister(generator, rowRegister);
  ScopeOutput(generator, ");\n");

  generator->_lastResultRegister = scope->_resultRegister;

  // }
  CloseLoops(generator);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for empty return
////////////////////////////////////////////////////////////////////////////////

static void ProcessReturnEmpty (TRI_aql_codegen_js_t* const generator,
                                const TRI_aql_node_t* const node) {
  TRI_aql_codegen_register_t resultRegister = IncRegister(generator);

  ScopeOutput(generator, "var ");
  // var row = ...;
  ScopeOutputRegister(generator, resultRegister);
#if AQL_VERBOSE
  ScopeOutput(generator, " = [ ]; /* return empty */\n");
#else
  ScopeOutput(generator, " = [ ];\n");
#endif

  generator->_lastResultRegister = resultRegister;

  // }
  CloseLoops(generator);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for let keyword
////////////////////////////////////////////////////////////////////////////////

static void ProcessLet (TRI_aql_codegen_js_t* const generator,
                        const TRI_aql_node_t* const node) {
  TRI_aql_node_t* nameNode = TRI_AQL_NODE_MEMBER(node, 0);
  TRI_aql_codegen_register_t resultRegister = IncRegister(generator);

  // TODO: if variable not refcounted, remove let statement!
  ScopeOutput(generator, "var ");
  ScopeOutputRegister(generator, resultRegister);
  ScopeOutput(generator, " = ");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  ScopeOutput(generator, ";\n");

  // enter the new variable in the symbol table
  EnterSymbol(generator, nameNode->_value._value._string, resultRegister);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for an assignment (used in COLLECT)
////////////////////////////////////////////////////////////////////////////////

static void ProcessAssign (TRI_aql_codegen_js_t* const generator,
                           const TRI_aql_node_t* const node) {
  TRI_aql_node_t* nameNode = TRI_AQL_NODE_MEMBER(node, 0);
  TRI_aql_codegen_register_t resultRegister = IncRegister(generator);
  TRI_aql_codegen_register_t lastResultRegister;
  TRI_aql_codegen_scope_t* scope = CurrentScope(generator);

  InitList(generator, resultRegister);

  StartScope(generator, scope->_buffer, TRI_AQL_SCOPE_SUBQUERY, 0, 0, 0, resultRegister, NULL);
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 1));

  lastResultRegister = CurrentScope(generator)->_resultRegister;
  EndScope(generator);

  if (lastResultRegister > 0) {
    // result register was modified inside the scope, e.g. due to a SORT
    resultRegister = lastResultRegister;
  }

  // enter the new variable in the symbol table
  EnterSymbol(generator, nameNode->_value._value._string, resultRegister);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for filter keyword
////////////////////////////////////////////////////////////////////////////////

static void ProcessFilter (TRI_aql_codegen_js_t* const generator,
                           const TRI_aql_node_t* const node) {
  TRI_aql_codegen_scope_t* scope = CurrentScope(generator);

  ScopeOutput(generator, "if (! (");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ")) {\n");

  if (scope->_type == TRI_AQL_SCOPE_MAIN || scope->_type == TRI_AQL_SCOPE_SUBQUERY) {
    // in these scopes, we must not generate a continue statement. this would be illegal
    ScopeOutput(generator, "return [ ];\n");
  }
  else {
    ScopeOutput(generator, "continue;\n");
  }

  ScopeOutput(generator, "}\n");

#if AQL_VERBOSE
    ScopeOutput(generator, "/* found a match */\n");
#endif

  if (scope->_limitRegister != 0) {
    assert(scope->_hint != NULL);

    if (scope->_offsetRegister != 0) {
      ScopeOutput(generator, "if (++");
      ScopeOutputRegister(generator, scope->_offsetRegister);
      ScopeOutput(generator, " <= ");
      ScopeOutputUInt(generator, (uint64_t) scope->_hint->_limit._offset);
      ScopeOutput(generator, ") {\ncontinue;\n}\n");
    }

    ScopeOutput(generator, "if (");
    ScopeOutputRegister(generator, scope->_limitRegister);
    ScopeOutput(generator, "++ >= ");
    ScopeOutputUInt(generator, (uint64_t) scope->_hint->_limit._limit);
    ScopeOutput(generator, ") {\nbreak;\n}\n");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for a node
///
/// this is a dispatcher function only
////////////////////////////////////////////////////////////////////////////////

static void ProcessNode (TRI_aql_codegen_js_t* const generator, const TRI_aql_node_t* const node) {
  if (node == NULL) {
    return;
  }

  switch (node->_type) {
    case TRI_AQL_NODE_VALUE:
      ProcessValue(generator, node);
      break;
    case TRI_AQL_NODE_LIST:
      ProcessList(generator, node);
      break;
    case TRI_AQL_NODE_ARRAY:
      ProcessArray(generator, node);
      break;
    case TRI_AQL_NODE_ARRAY_ELEMENT:
      ProcessArrayElement(generator, node);
      break;
    case TRI_AQL_NODE_COLLECTION:
      ProcessCollection(generator, node);
      break;
    case TRI_AQL_NODE_REFERENCE:
      ProcessReference(generator, node);
      break;
    case TRI_AQL_NODE_ATTRIBUTE:
      ProcessAttribute(generator, node);
      break;
    case TRI_AQL_NODE_ATTRIBUTE_ACCESS:
      ProcessAttributeAccess(generator, node);
      break;
    case TRI_AQL_NODE_INDEXED:
      ProcessIndexed(generator, node);
      break;
    case TRI_AQL_NODE_EXPAND:
      ProcessExpand(generator, node);
      break;
    case TRI_AQL_NODE_OPERATOR_UNARY_NOT:
      ProcessUnaryNot(generator, node);
      break;
    case TRI_AQL_NODE_OPERATOR_UNARY_PLUS:
      ProcessUnaryPlus(generator, node);
      break;
    case TRI_AQL_NODE_OPERATOR_UNARY_MINUS:
      ProcessUnaryMinus(generator, node);
      break;
    case TRI_AQL_NODE_OPERATOR_BINARY_AND:
      ProcessBinaryAnd(generator, node);
      break;
    case TRI_AQL_NODE_OPERATOR_BINARY_OR:
      ProcessBinaryOr(generator, node);
      break;
    case TRI_AQL_NODE_OPERATOR_BINARY_PLUS:
      ProcessBinaryPlus(generator, node);
      break;
    case TRI_AQL_NODE_OPERATOR_BINARY_MINUS:
      ProcessBinaryMinus(generator, node);
      break;
    case TRI_AQL_NODE_OPERATOR_BINARY_TIMES:
      ProcessBinaryTimes(generator, node);
      break;
    case TRI_AQL_NODE_OPERATOR_BINARY_DIV:
      ProcessBinaryDivide(generator, node);
      break;
    case TRI_AQL_NODE_OPERATOR_BINARY_MOD:
      ProcessBinaryModulus(generator, node);
      break;
    case TRI_AQL_NODE_OPERATOR_BINARY_EQ:
      ProcessBinaryEqual(generator, node);
      break;
    case TRI_AQL_NODE_OPERATOR_BINARY_NE:
      ProcessBinaryUnequal(generator, node);
      break;
    case TRI_AQL_NODE_OPERATOR_BINARY_LT:
      ProcessBinaryLess(generator, node);
      break;
    case TRI_AQL_NODE_OPERATOR_BINARY_LE:
      ProcessBinaryLessEqual(generator, node);
      break;
    case TRI_AQL_NODE_OPERATOR_BINARY_GT:
      ProcessBinaryGreater(generator, node);
      break;
    case TRI_AQL_NODE_OPERATOR_BINARY_GE:
      ProcessBinaryGreaterEqual(generator, node);
      break;
    case TRI_AQL_NODE_OPERATOR_BINARY_IN:
      ProcessBinaryIn(generator, node);
      break;
    case TRI_AQL_NODE_OPERATOR_TERNARY:
      ProcessTernary(generator, node);
      break;
    case TRI_AQL_NODE_FCALL:
      ProcessFcallInternal(generator, node);
      break;
    case TRI_AQL_NODE_FCALL_USER:
      ProcessFcallUser(generator, node);
      break;
    case TRI_AQL_NODE_FOR:
      ProcessFor(generator, node);
      break;
    case TRI_AQL_NODE_LET:
      ProcessLet(generator, node);
      break;
    case TRI_AQL_NODE_COLLECT:
      ProcessCollect(generator, node);
      break;
    case TRI_AQL_NODE_ASSIGN:
      ProcessAssign(generator, node);
      break;
    case TRI_AQL_NODE_FILTER:
      ProcessFilter(generator, node);
      break;
    case TRI_AQL_NODE_LIMIT:
      ProcessLimit(generator, node);
      break;
    case TRI_AQL_NODE_SORT:
      ProcessSort(generator, node);
      break;
    case TRI_AQL_NODE_RETURN:
      ProcessReturn(generator, node);
      break;
    case TRI_AQL_NODE_RETURN_EMPTY:
      ProcessReturnEmpty(generator, node);
      break;
    case TRI_AQL_NODE_SUBQUERY:
      ProcessSubquery(generator, node);
      break;
    case TRI_AQL_NODE_SCOPE_START:
      ProcessScopeStart(generator, node);
      break;
    case TRI_AQL_NODE_SCOPE_END:
      ProcessScopeEnd(generator, node);
      break;
    default: {
    }
  }

}

////////////////////////////////////////////////////////////////////////////////
/// @brief create code from the statements
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_codegen_register_t CreateCode (TRI_aql_codegen_js_t* generator) {
  TRI_aql_codegen_register_t startRegister = IncRegister(generator);
  TRI_aql_codegen_register_t resultRegister;
  TRI_vector_pointer_t* statements;
  size_t i, n;

  StartScope(generator, &generator->_buffer, TRI_AQL_SCOPE_MAIN, 0, 0, 0, startRegister, NULL);
  InitList(generator, startRegister);

  statements = &generator->_context->_statements->_statements;
  n = statements->_length;
  for (i = 0; i < n; ++i) {
    TRI_aql_node_t* node;

    node = TRI_AtVectorPointer(statements, i);
    ProcessNode(generator, node);
  }

  resultRegister = CurrentScope(generator)->_resultRegister;

  EndScope(generator);

  return resultRegister;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free memory associated with the code generator
////////////////////////////////////////////////////////////////////////////////

static void FreeGenerator (TRI_aql_codegen_js_t* const generator) {
  TRI_DestroyVectorPointer(&generator->_scopes);

  TRI_DestroyStringBuffer(&generator->_buffer);
  TRI_DestroyStringBuffer(&generator->_functionBuffer);

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, generator);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a code generator
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_codegen_js_t* CreateGenerator (TRI_aql_context_t* const context) {
  TRI_aql_codegen_js_t* generator;

  assert(context);

  generator = (TRI_aql_codegen_js_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_aql_codegen_js_t), false);
  if (generator == NULL) {
    return NULL;
  }

  generator->_context = context;

  TRI_InitStringBuffer(&generator->_buffer, TRI_UNKNOWN_MEM_ZONE);
  TRI_InitStringBuffer(&generator->_functionBuffer, TRI_UNKNOWN_MEM_ZONE);

  TRI_InitVectorPointer(&generator->_scopes, TRI_UNKNOWN_MEM_ZONE);
  generator->_errorCode = TRI_ERROR_NO_ERROR;
  generator->_errorValue = NULL;
  generator->_registerIndex = 0;
  generator->_functionIndex = 0;

  return generator;
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
/// @brief generate Javascript code for the AST nodes recursively
////////////////////////////////////////////////////////////////////////////////

char* TRI_GenerateCodeAql (TRI_aql_context_t* const context) {
  TRI_aql_codegen_js_t* generator;
  TRI_aql_codegen_register_t resultRegister;
  char* code;

  assert(context);

  generator = CreateGenerator(context);
  if (generator == NULL) {
    return NULL;
  }

  OutputString(&generator->_functionBuffer, "(function () {\nvar aql = require(\"org/arangodb/ahuacatl\");\n");

  resultRegister = CreateCode(generator);

  // append result
  OutputString(&generator->_buffer, "return ");
  OutputString(&generator->_buffer, REGISTER_PREFIX);
  OutputInt(&generator->_buffer, (int64_t) resultRegister);
  OutputString(&generator->_buffer, ";\n");

  OutputString(&generator->_buffer, "})();");

  code = NULL;

  if (generator->_errorCode == TRI_ERROR_NO_ERROR) {
    // put everything together
    code = TRI_Concatenate2StringZ(TRI_UNKNOWN_MEM_ZONE, generator->_functionBuffer._buffer, generator->_buffer._buffer);
    if (code) {
      LOG_TRACE("generated code: %s", code);
    }
    else {
      generator->_errorCode = TRI_ERROR_OUT_OF_MEMORY;
    }
  }

  // errorCode might have changed, no else!
  if (generator->_errorCode != TRI_ERROR_NO_ERROR) {
    // register the error
    TRI_SetErrorContextAql(context, generator->_errorCode, generator->_errorValue);
  }

  assert(generator);
  FreeGenerator(generator);

  return code;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

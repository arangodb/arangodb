////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, AST dump functions
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

#include <BasicsC/logging.h>

#include "Ahuacatl/ahuacatl-codegen-js.h"
#include "Ahuacatl/ahuacatl-functions.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                    private macros
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

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

static void ProcessNode (TRI_aql_codegen_js_t* const generator, const TRI_aql_node_t* const node);

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
  if (!value) {
    return false;
  }

  return (TRI_AppendStringStringBuffer(buffer, value) == TRI_ERROR_NO_ERROR);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief append a single character to the buffer
////////////////////////////////////////////////////////////////////////////////

static inline bool OutputChar (TRI_string_buffer_t* const buffer, 
                               const char value) {
  return (TRI_AppendCharStringBuffer(buffer, value) == TRI_ERROR_NO_ERROR);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief append an integer value to the buffer
////////////////////////////////////////////////////////////////////////////////

static inline bool OutputInt (TRI_string_buffer_t* const buffer, 
                              const int64_t value) {
  return (TRI_AppendInt64StringBuffer(buffer, value) == TRI_ERROR_NO_ERROR);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print a string in the current scope
////////////////////////////////////////////////////////////////////////////////

static inline void ScopeOutput (TRI_aql_codegen_js_t* const generator, 
                                const char* const value) {
  TRI_aql_codegen_scope_t* scope = CurrentScope(generator);

  if (!OutputString(scope->_buffer, value)) {
    generator->_error = true;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print an integer in the current scope
////////////////////////////////////////////////////////////////////////////////

static inline void ScopeOutputInt (TRI_aql_codegen_js_t* const generator, 
                                   const int64_t value) {
  TRI_aql_codegen_scope_t* scope = CurrentScope(generator);

  if (!OutputInt(scope->_buffer, value)) {
    generator->_error = true;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print an escaped string in the current scope, enclosed by '
////////////////////////////////////////////////////////////////////////////////

static inline void ScopeOutputQuoted (TRI_aql_codegen_js_t* const generator, 
                                      const char* const value) {
  TRI_aql_codegen_scope_t* scope = CurrentScope(generator);

  if (!OutputChar(scope->_buffer, '\'')) {
    generator->_error = true;
  }
  if (!OutputString(scope->_buffer, value)) {
    generator->_error = true;
  }
  if (!OutputChar(scope->_buffer, '\'')) {
    generator->_error = true;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print a function name in the current scope
////////////////////////////////////////////////////////////////////////////////

static inline void ScopeOutputFunction (TRI_aql_codegen_js_t* const generator, 
                                        const TRI_aql_codegen_register_t functionIndex) {
  TRI_aql_codegen_scope_t* scope = CurrentScope(generator);

  if (!OutputString(scope->_buffer, FUNCTION_PREFIX)) {
    generator->_error = true;
  }
  if (!OutputInt(scope->_buffer, (int64_t) functionIndex)) {
    generator->_error = true;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print a register name in the current scope
////////////////////////////////////////////////////////////////////////////////

static inline void ScopeOutputRegister (TRI_aql_codegen_js_t* const generator, 
                                        const TRI_aql_codegen_register_t registerIndex) {
  TRI_aql_codegen_scope_t* scope = CurrentScope(generator);

  if (!OutputString(scope->_buffer, REGISTER_PREFIX)) {
    generator->_error = true;
  }
  if (!OutputInt(scope->_buffer, (int64_t) registerIndex)) {
    generator->_error = true;
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
  if (!variable) {
    return NULL;
  }

  variable->_register = registerIndex;
  variable->_name = TRI_DuplicateString(name);

  if (!variable->_name) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, variable);
    return NULL;
  }

  return variable;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the type for the next scope
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_codegen_scope_e NextScopeType (TRI_aql_codegen_js_t* const generator, 
                                              const TRI_aql_codegen_scope_e requestedType) {
  TRI_aql_codegen_scope_t* scope;
 
  // we're only interested in TRI_AQL_SCOPE_FOR 
  if (requestedType != TRI_AQL_SCOPE_FOR) {
    return requestedType;
  }
  
  scope = CurrentScope(generator);

  // if we are in a TRI_AQL_SCOPE_FOR scope, we'll return TRI_AQL_SCOPE_FOR_NESTED 
  if (scope->_type == TRI_AQL_SCOPE_FOR) {
    return TRI_AQL_SCOPE_FOR_NESTED;
  }

  return requestedType;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start a new scope and push it onto the stack
////////////////////////////////////////////////////////////////////////////////

static void StartScope (TRI_aql_codegen_js_t* const generator, 
                        TRI_string_buffer_t* const buffer,
                        const TRI_aql_codegen_scope_e const type,
                        const TRI_aql_codegen_register_t listRegister, 
                        const TRI_aql_codegen_register_t keyRegister, 
                        const TRI_aql_codegen_register_t ownRegister, 
                        const TRI_aql_codegen_register_t resultRegister, 
                        const char* const variableName,
                        const char* const name) {
  TRI_aql_codegen_scope_t* scope;
  
  scope = (TRI_aql_codegen_scope_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_aql_codegen_scope_t), false);
  if (!scope) {
    generator->_error = true;
    return;
  }

  scope->_buffer = buffer;
  scope->_type = NextScopeType(generator, type);
  scope->_listRegister = listRegister;
  scope->_keyRegister = keyRegister;
  scope->_ownRegister = ownRegister;
  scope->_resultRegister = resultRegister;
  scope->_variableName = variableName;
  scope->_prefix = NULL;
  scope->_name = name;

  // init symbol table
  TRI_InitAssociativePointer(&scope->_variables, 
                             TRI_UNKNOWN_MEM_ZONE, 
                             &TRI_HashStringKeyAssociativePointer, 
                             &HashVariable,
                             &EqualVariable,
                             NULL);
  
  // push the scope on the stack
  TRI_PushBackVectorPointer(&generator->_scopes, (void*) scope);

  ScopeOutput(generator, "\n/* scope start (");
  ScopeOutput(generator, scope->_name);
  ScopeOutput(generator, ") */\n");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief end the current scope and pop it from the stack
////////////////////////////////////////////////////////////////////////////////

static void EndScope (TRI_aql_codegen_js_t* const generator) {
  TRI_aql_codegen_scope_t* scope = CurrentScope(generator);
  TRI_aql_codegen_register_t i, n;
  
  ScopeOutput(generator, "\n/* scope end (");
  ScopeOutput(generator, scope->_name);
  ScopeOutput(generator, ") */\n");

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
  StartScope(generator, &generator->_functionBuffer, TRI_AQL_SCOPE_FUNCTION, 0, 0, 0, 0, NULL, "sort");
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
    ScopeOutput(generator, "res = AHUACATL_RELATIONAL_CMP(lhs, rhs);\n");
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
  StartScope(generator, &generator->_functionBuffer, TRI_AQL_SCOPE_FUNCTION, 0, 0, 0, 0, NULL, "group");
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
  TRI_aql_codegen_scope_t* scope = CurrentScope(generator);
  TRI_aql_codegen_variable_t* variable = CreateVariable(name, registerIndex);

  if (!variable) {
    generator->_error = true;
    return;
  }

  if (TRI_InsertKeyAssociativePointer(&scope->_variables, name, (void*) variable, false)) {
    // variable already exists in symbol table. this should never happen
    generator->_error = true;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief look up a variable in the symbol table
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_codegen_register_t LookupSymbol (TRI_aql_codegen_js_t* const generator, 
                                                const char* const name) {
  size_t i = generator->_scopes._length;

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

  // variable not found. this should not happen
  generator->_error = true;
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add code for the property check (if (!var.hasOwnProperty()) continue)
////////////////////////////////////////////////////////////////////////////////

static void ScopePropertyCheck (TRI_aql_codegen_js_t* const generator, 
                                const TRI_aql_codegen_register_t listRegister,
                                const TRI_aql_codegen_register_t keyRegister) {
  ScopeOutput(generator, "if (!");
  ScopeOutputRegister(generator, listRegister);
  ScopeOutput(generator, ".hasOwnProperty(");
  ScopeOutputRegister(generator, keyRegister); 
  ScopeOutput(generator, ")) {\n");
  ScopeOutput(generator, "continue;\n");
  ScopeOutput(generator, "}\n");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start a for loop
////////////////////////////////////////////////////////////////////////////////

static void StartFor (TRI_aql_codegen_js_t* const generator,
                      const TRI_aql_codegen_register_t sourceRegister,
                      const char* const variableName) {
  TRI_aql_codegen_register_t listRegister = IncRegister(generator);
  TRI_aql_codegen_register_t keyRegister = IncRegister(generator);
  TRI_aql_codegen_register_t ownRegister = IncRegister(generator);
  TRI_aql_codegen_scope_t* scope = CurrentScope(generator);

  // always start a new scope
  StartScope(generator, 
             scope->_buffer,  // inherit buffer from previous scope
             TRI_AQL_SCOPE_FOR, 
             listRegister, 
             keyRegister, 
             ownRegister, 
             scope->_resultRegister, 
             variableName, 
             "for");

  if (variableName) {
    EnterSymbol(generator, variableName, ownRegister);
  }

  ScopeOutput(generator, "var ");
  ScopeOutputRegister(generator, listRegister);
  ScopeOutput(generator, " = AHUACATL_LIST(");
  ScopeOutputRegister(generator, sourceRegister);
  ScopeOutput(generator, ");\n");
       
  // for (var keyx in listx) 
  ScopeOutput(generator, "for (var "); 
  ScopeOutputRegister(generator, keyRegister);
  ScopeOutput(generator, " in ");
  ScopeOutputRegister(generator, listRegister);
  ScopeOutput(generator, ") {\n");

  ScopePropertyCheck(generator, listRegister, keyRegister);
    
  // var rx = listx[keyx];
  scope = CurrentScope(generator);
  ScopeOutput(generator, "var ");
  ScopeOutputRegister(generator, scope->_ownRegister);
  ScopeOutput(generator, " = ");
  ScopeOutputRegister(generator, scope->_listRegister);
  ScopeOutput(generator, "[");
  ScopeOutputRegister(generator, scope->_keyRegister); 
  ScopeOutput(generator, "];\n");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief close for loops til the parent scope is reached
///
/// all for loops will be closed until another, non-for scope is reached
////////////////////////////////////////////////////////////////////////////////

static void CloseLoops (TRI_aql_codegen_js_t* const generator) {
  TRI_aql_codegen_scope_t* scope = CurrentScope(generator);

  // we are closing at least one scope
  while (true) {
    TRI_aql_codegen_scope_e type = scope->_type;
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

      if (!variable) {
        continue;
      }

      // push all variable values into the rowRegister
      // row[name] = ...;
      ScopeOutputIndexedName(generator, rowRegister, variable->_name);
      ScopeOutput(generator, " = ");
      ScopeOutputRegister(generator, variable->_register);
      ScopeOutput(generator, ";\n");
  
      TRI_PushBackVectorString(&variableNames, TRI_DuplicateString(variable->_name));
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
  if (!TRI_ValueJavascriptAql(CurrentScope(generator)->_buffer, &node->_value, node->_value._type)) {
    generator->_error = true;
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
  TRI_ValueJavascriptAql(CurrentScope(generator)->_buffer, &node->_value, AQL_TYPE_STRING);
  ScopeOutput(generator, " : ");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for an attribute name in exapnd (e.g. users[*].name)
////////////////////////////////////////////////////////////////////////////////

static void ProcessAttribute (TRI_aql_codegen_js_t* const generator,
                              const TRI_aql_node_t* const node) {
  TRI_aql_codegen_scope_t* scope = CurrentScope(generator);

  ScopeOutput(generator, "AHUACATL_DOCUMENT_MEMBER(");
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
  ScopeOutput(generator, "AHUACATL_DOCUMENT_MEMBER(");
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
  ScopeOutput(generator, "AHUACATL_GET_INDEX(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ", ");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  ScopeOutput(generator, ")");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for collection access
////////////////////////////////////////////////////////////////////////////////

static void ProcessCollection (TRI_aql_codegen_js_t* const generator,
                               const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "AHUACATL_GET_DOCUMENTS('");
  ScopeOutput(generator, TRI_AQL_NODE_STRING(node));
  ScopeOutput(generator, "')");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for expand (e.g. users[*]....)
////////////////////////////////////////////////////////////////////////////////

static void ProcessExpand (TRI_aql_codegen_js_t* const generator,
                           const TRI_aql_node_t* const node) {
  TRI_aql_codegen_register_t functionIndex = IncFunction(generator);
  TRI_aql_codegen_register_t inputRegister = IncRegister(generator);
  TRI_aql_codegen_register_t resultRegister = IncRegister(generator);

  // expand scope
  StartScope(generator, &generator->_functionBuffer, TRI_AQL_SCOPE_EXPAND, 0, 0, 0, resultRegister, NULL, "expand");

  ScopeOutput(generator, "function ");
  ScopeOutputFunction(generator, functionIndex);
  ScopeOutput(generator, " (");
  ScopeOutputRegister(generator, inputRegister);
  ScopeOutput(generator, ") {\n");

  // var result = [ ];
  InitList(generator, resultRegister);
 
  // for
  StartFor(generator, inputRegister, NULL);
  ScopeOutputRegister(generator, resultRegister);
  ScopeOutput(generator, ".push(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  ScopeOutput(generator, ");\n");

  // }
  CloseLoops(generator);
  ScopeOutput(generator, "return ");
  ScopeOutputRegister(generator, resultRegister);
  ScopeOutput(generator, ";\n");
  ScopeOutput(generator, "}\n");
  
  // expand scope
  EndScope(generator);

  ScopeOutputFunction(generator, functionIndex);
  ScopeOutput(generator, "(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ")");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for unary not (!value)
////////////////////////////////////////////////////////////////////////////////

static void ProcessUnaryNot (TRI_aql_codegen_js_t* const generator,
                             const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "AHUACATL_LOGICAL_NOT(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ")");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for unary minus (-value)
////////////////////////////////////////////////////////////////////////////////

static void ProcessUnaryMinus (TRI_aql_codegen_js_t* const generator,
                               const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "AHUACATL_UNARY_MINUS(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ")");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for unary plus (+value)
////////////////////////////////////////////////////////////////////////////////

static void ProcessUnaryPlus (TRI_aql_codegen_js_t* const generator,
                              const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "AHUACATL_UNARY_PLUS(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ")");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for binary and (a && b)
////////////////////////////////////////////////////////////////////////////////

static void ProcessBinaryAnd (TRI_aql_codegen_js_t* const generator,
                              const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "AHUACATL_LOGICAL_AND(");
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
  ScopeOutput(generator, "AHUACATL_LOGICAL_OR(");
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
  ScopeOutput(generator, "AHUACATL_ARITHMETIC_PLUS(");
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
  ScopeOutput(generator, "AHUACATL_ARITHMETIC_MINUS(");
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
  ScopeOutput(generator, "AHUACATL_ARITHMETIC_TIMES(");
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
  ScopeOutput(generator, "AHUACATL_ARITHMETIC_DIVIDE(");
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
  ScopeOutput(generator, "AHUACATL_ARITHMETIC_MODULUS(");
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
  ScopeOutput(generator, "AHUACATL_RELATIONAL_EQUAL(");
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
  ScopeOutput(generator, "AHUACATL_RELATIONAL_UNEQUAL(");
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
  ScopeOutput(generator, "AHUACATL_RELATIONAL_LESSEQUAL(");
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
  ScopeOutput(generator, "AHUACATL_RELATIONAL_LESS(");
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
  ScopeOutput(generator, "AHUACATL_RELATIONAL_GREATEREQUAL(");
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
  ScopeOutput(generator, "AHUACATL_RELATIONAL_GREATER(");
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
  ScopeOutput(generator, "AHUACATL_RELATIONAL_IN(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ", ");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  ScopeOutput(generator, ")");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for subqueries (nested fors etc.)
////////////////////////////////////////////////////////////////////////////////

static void ProcessSubquery (TRI_aql_codegen_js_t* const generator, 
                             const TRI_aql_node_t* const node) {
  TRI_aql_codegen_register_t scopeRegister = IncRegister(generator);
  TRI_aql_codegen_register_t resultRegister = IncRegister(generator);
  TRI_aql_codegen_register_t subFunction = IncFunction(generator);

  StartScope(generator, &generator->_functionBuffer, TRI_AQL_SCOPE_FUNCTION, 0, 0, 0, resultRegister, NULL, "subquery");
  ScopeOutput(generator, "function ");
  ScopeOutputFunction(generator, subFunction);
  ScopeOutput(generator, " () {\n");
  InitList(generator, resultRegister);
  
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));

  // register might have changed
  resultRegister = CurrentScope(generator)->_resultRegister;

  ScopeOutput(generator, "return ");
  ScopeOutputRegister(generator, resultRegister);
  ScopeOutput(generator, ";\n");
  ScopeOutput(generator, "}\n");

  EndScope(generator);

  ScopeOutput(generator, "var ");
  ScopeOutputRegister(generator, scopeRegister);
  ScopeOutput(generator, " = ");
  ScopeOutputFunction(generator, subFunction);
  ScopeOutput(generator, "();\n");
  
  CurrentScope(generator)->_resultRegister = scopeRegister;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for function calls
////////////////////////////////////////////////////////////////////////////////

static void ProcessFcall (TRI_aql_codegen_js_t* const generator,
                          const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "AHUACATL_FCALL(");
  ScopeOutput(generator, TRI_GetInternalNameFunctionAql((TRI_aql_function_t*) TRI_AQL_NODE_DATA(node)));
  ScopeOutput(generator, ", ");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ")");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for for keyword
////////////////////////////////////////////////////////////////////////////////

static void ProcessFor (TRI_aql_codegen_js_t* const generator, 
                        const TRI_aql_node_t* const node) {
  TRI_aql_node_t* nameNode;
  
  nameNode = TRI_AQL_NODE_MEMBER(node, 0);

  if (((TRI_aql_node_t*) TRI_AQL_NODE_MEMBER(node, 1))->_type != AQL_NODE_SUBQUERY) {
    TRI_aql_codegen_register_t sourceRegister = IncRegister(generator);

    ScopeOutput(generator, "var ");
    ScopeOutputRegister(generator, sourceRegister);
    ScopeOutput(generator, " = ");
    ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
    ScopeOutput(generator, ";\n");
  
    StartFor(generator, sourceRegister, nameNode->_value._value._string);
  }
  else {
    TRI_aql_codegen_register_t resultRegister = IncRegister(generator);
    TRI_aql_codegen_register_t sourceRegister;

    InitList(generator, resultRegister); 
    
    ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
    sourceRegister = CurrentScope(generator)->_resultRegister;
    CurrentScope(generator)->_resultRegister = resultRegister;
    StartFor(generator, sourceRegister, nameNode->_value._value._string);
  }
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
  ScopeOutput(generator, "AHUACATL_SORT(");
  ScopeOutputRegister(generator, sourceRegister);
  ScopeOutput(generator, ", ");
  ScopeOutputFunction(generator, functionIndex);
  ScopeOutput(generator, ");\n");

  // start new for loop
  resultRegister = IncRegister(generator);
  scope = CurrentScope(generator);
  scope->_resultRegister = resultRegister;
  
  InitList(generator, resultRegister);
  StartFor(generator, sourceRegister, NULL);

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
  ScopeOutputRegister(generator, groupRegister);
  ScopeOutput(generator, " = ");
  ScopeOutput(generator, "AHUACATL_GROUP(");
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
  StartFor(generator, groupRegister, NULL);
  scope = CurrentScope(generator);

  // re-enter symbols for collect variables
  list = TRI_AQL_NODE_MEMBER(node, 0);
  n = list->_members._length;
  for (i = 0; i < n; ++i) {
    TRI_aql_node_t* element = TRI_AQL_NODE_MEMBER(list, i);
    TRI_aql_node_t* varNode = TRI_AQL_NODE_MEMBER(element, 0);
    TRI_aql_codegen_register_t varRegister = IncRegister(generator);

    // var collect = temp['collect'];
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
  ScopeOutput(generator, " = AHUACATL_LIMIT(");
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
  StartFor(generator, limitRegister, NULL);

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

  // }
  CloseLoops(generator);
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

  StartScope(generator, scope->_buffer, TRI_AQL_SCOPE_LET, 0, 0, 0, resultRegister, NULL, "let");
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
  ScopeOutput(generator, "if (!(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ")) {\n");
  ScopeOutput(generator, "continue;\n");
  ScopeOutput(generator, "}\n");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate code for a node
///
/// this is a dispatcher function only
////////////////////////////////////////////////////////////////////////////////

static void ProcessNode (TRI_aql_codegen_js_t* generator, const TRI_aql_node_t* const data) {
  TRI_aql_node_t* node = (TRI_aql_node_t*) data;

  if (!node) {
    return;
  }

  while (node) {
    switch (node->_type) {
      case AQL_NODE_VALUE:
        ProcessValue(generator, node);
        break;
      case AQL_NODE_LIST: 
        ProcessList(generator, node);
        break;
      case AQL_NODE_ARRAY: 
        ProcessArray(generator, node);
        break;
      case AQL_NODE_ARRAY_ELEMENT: 
        ProcessArrayElement(generator, node);
        break;
      case AQL_NODE_COLLECTION:
        ProcessCollection(generator, node);
        break;
      case AQL_NODE_REFERENCE: 
        ProcessReference(generator, node);
        break;
      case AQL_NODE_ATTRIBUTE:
        ProcessAttribute(generator, node);
        break;
      case AQL_NODE_ATTRIBUTE_ACCESS:
        ProcessAttributeAccess(generator, node);
        break;
      case AQL_NODE_INDEXED:
        ProcessIndexed(generator, node);
        break;
      case AQL_NODE_EXPAND:
        ProcessExpand(generator, node);
        break; 
      case AQL_NODE_OPERATOR_UNARY_NOT:
        ProcessUnaryNot(generator, node);
        break;
      case AQL_NODE_OPERATOR_UNARY_PLUS:
        ProcessUnaryPlus(generator, node);
        break;
      case AQL_NODE_OPERATOR_UNARY_MINUS:
        ProcessUnaryMinus(generator, node);
        break;
      case AQL_NODE_OPERATOR_BINARY_AND:
        ProcessBinaryAnd(generator, node);
        break;
      case AQL_NODE_OPERATOR_BINARY_OR:
        ProcessBinaryOr(generator, node);
        break;
      case AQL_NODE_OPERATOR_BINARY_PLUS:
        ProcessBinaryPlus(generator, node);
        break;
      case AQL_NODE_OPERATOR_BINARY_MINUS:
        ProcessBinaryMinus(generator, node);
        break;
      case AQL_NODE_OPERATOR_BINARY_TIMES:
        ProcessBinaryTimes(generator, node);
        break;
      case AQL_NODE_OPERATOR_BINARY_DIV:
        ProcessBinaryDivide(generator, node);
        break;
      case AQL_NODE_OPERATOR_BINARY_MOD:
        ProcessBinaryModulus(generator, node);
        break;
      case AQL_NODE_OPERATOR_BINARY_EQ:
        ProcessBinaryEqual(generator, node);
        break;
      case AQL_NODE_OPERATOR_BINARY_NE:
        ProcessBinaryUnequal(generator, node);
        break;
      case AQL_NODE_OPERATOR_BINARY_LT:
        ProcessBinaryLess(generator, node);
        break;
      case AQL_NODE_OPERATOR_BINARY_LE:
        ProcessBinaryLessEqual(generator, node);
        break;
      case AQL_NODE_OPERATOR_BINARY_GT:
        ProcessBinaryGreater(generator, node);
        break;
      case AQL_NODE_OPERATOR_BINARY_GE:
        ProcessBinaryGreaterEqual(generator, node);
        break;
      case AQL_NODE_OPERATOR_BINARY_IN:
        ProcessBinaryIn(generator, node);
        break;
      case AQL_NODE_FCALL:
        ProcessFcall(generator, node);
        break;
      case AQL_NODE_FOR:
        ProcessFor(generator, node);
        break;
      case AQL_NODE_COLLECT: 
        ProcessCollect(generator, node);
        break;
      case AQL_NODE_ASSIGN:
        ProcessAssign(generator, node);
        break;
      case AQL_NODE_FILTER:
        ProcessFilter(generator, node);
        break;
      case AQL_NODE_LIMIT:
        ProcessLimit(generator, node);
        break;
      case AQL_NODE_SORT:
        ProcessSort(generator, node);
        break;
      case AQL_NODE_RETURN:
        ProcessReturn(generator, node);
        break;
      case AQL_NODE_SUBQUERY:
        ProcessSubquery(generator, node);
        break;
      default: {
      }
    }

    // now handle subnodes
    node = node->_next;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create code for the AST starting at the specified node
////////////////////////////////////////////////////////////////////////////////

TRI_aql_codegen_register_t CreateCode (TRI_aql_codegen_js_t* generator, 
                                       const TRI_aql_node_t* const node) {
  TRI_aql_codegen_register_t startRegister = IncRegister(generator);
  TRI_aql_codegen_register_t resultRegister;

  assert(node);

  StartScope(generator, &generator->_buffer, TRI_AQL_SCOPE_MAIN, 0, 0, 0, startRegister, NULL, "main");
  InitList(generator, startRegister);
  
  ProcessNode(generator, node);
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

static TRI_aql_codegen_js_t* CreateGenerator (void) {
  TRI_aql_codegen_js_t* generator;

  generator = (TRI_aql_codegen_js_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_aql_codegen_js_t), false);
  if (!generator) {
    return NULL;
  }

  TRI_InitStringBuffer(&generator->_buffer, TRI_UNKNOWN_MEM_ZONE);
  TRI_InitStringBuffer(&generator->_functionBuffer, TRI_UNKNOWN_MEM_ZONE);

  TRI_InitVectorPointer(&generator->_scopes, TRI_UNKNOWN_MEM_ZONE);
  generator->_error = false;
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

char* TRI_GenerateCodeAql (const void* const data) {
  TRI_aql_codegen_js_t* generator;
  TRI_aql_codegen_register_t resultRegister;
  char* code;

  generator = CreateGenerator();
  if (!generator) {
    return NULL;
  }

  OutputString(&generator->_functionBuffer, "(function () {\n");

  resultRegister = CreateCode(generator, data);

  // append result
  OutputString(&generator->_buffer, "return ");
  OutputString(&generator->_buffer, REGISTER_PREFIX);
  OutputInt(&generator->_buffer, (int64_t) resultRegister);
  OutputString(&generator->_buffer, ";\n");

  OutputString(&generator->_buffer, "})()");

  if (generator->_error) {
    FreeGenerator(generator);
    return NULL;
  }

  // put everything together
  code = TRI_Concatenate2String(generator->_functionBuffer._buffer, generator->_buffer._buffer);

  FreeGenerator(generator);

  if (code) {
    LOG_TRACE("generated code: %s", code);
    // printf("generated code: %s", code);
  }

  return code;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

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
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

#define REGISTER_PREFIX "r"
#define FUNCTION_PREFIX "f"

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief hash variable
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

static void ProcessNode (TRI_aql_codegen_js_t* const generator, const TRI_aql_node_t* const node);

static inline TRI_aql_codegen_register_t IncRegister (TRI_aql_codegen_js_t* const generator) {
  return ++generator->_registerIndex;
}

static inline TRI_aql_codegen_register_t IncFunction (TRI_aql_codegen_js_t* const generator) {
  return ++generator->_functionIndex;
}

static inline bool OutputString (TRI_string_buffer_t* const buffer, const char* const value) {
  if (!value) {
    return false;
  }

  return (TRI_AppendStringStringBuffer(buffer, value) == TRI_ERROR_NO_ERROR);
}

static inline bool OutputChar (TRI_string_buffer_t* const buffer, const char value) {
  return (TRI_AppendCharStringBuffer(buffer, value) == TRI_ERROR_NO_ERROR);
}

static inline bool OutputInt (TRI_string_buffer_t* const buffer, const int64_t value) {
  return (TRI_AppendInt64StringBuffer(buffer, value) == TRI_ERROR_NO_ERROR);
}

static inline TRI_aql_codegen_scope_t* CurrentScope (TRI_aql_codegen_js_t* const generator) {
  size_t n = generator->_scopes._length;

  assert(n > 0);
  return (TRI_aql_codegen_scope_t*) TRI_AtVectorPointer(&generator->_scopes, n - 1);
}

static inline void ScopeOutput (TRI_aql_codegen_js_t* const generator, const char* const value) {
  TRI_aql_codegen_scope_t* scope = CurrentScope(generator);

  if (!OutputString(scope->_buffer, value)) {
    generator->_error = true;
  }
}

static inline void ScopeOutputInt (TRI_aql_codegen_js_t* const generator, const int64_t value) {
  TRI_aql_codegen_scope_t* scope = CurrentScope(generator);

  if (!OutputInt(scope->_buffer, value)) {
    generator->_error = true;
  }
}

static inline void ScopeOutputQuoted (TRI_aql_codegen_js_t* const generator, const char* const value) {
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

static inline void ScopeOutputFunction (TRI_aql_codegen_js_t* const generator, const TRI_aql_codegen_register_t functionIndex) {
  TRI_aql_codegen_scope_t* scope = CurrentScope(generator);

  if (!OutputString(scope->_buffer, FUNCTION_PREFIX)) {
    generator->_error = true;
  }
  if (!OutputInt(scope->_buffer, (int64_t) functionIndex)) {
    generator->_error = true;
  }
}

static inline void ScopeOutputRegister (TRI_aql_codegen_js_t* const generator, const TRI_aql_codegen_register_t registerIndex) {
  TRI_aql_codegen_scope_t* scope = CurrentScope(generator);

  if (!OutputString(scope->_buffer, REGISTER_PREFIX)) {
    generator->_error = true;
  }
  if (!OutputInt(scope->_buffer, (int64_t) registerIndex)) {
    generator->_error = true;
  }
}

static void FreeVariable (TRI_aql_codegen_variable_t* const variable) {
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, variable->_name);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, variable);
}

static TRI_aql_codegen_variable_t* CreateVariable (const char* const name, const TRI_aql_codegen_register_t registerIndex) {
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

static TRI_aql_codegen_scope_e NextScopeType (TRI_aql_codegen_js_t* const generator, const TRI_aql_codegen_scope_e type) {
  TRI_aql_codegen_scope_t* scope;
  
  if (type == TRI_AQL_SCOPE_MAIN || type == TRI_AQL_SCOPE_LET) {
    return type;
  }
  
  scope = CurrentScope(generator);

  if (scope->_type == TRI_AQL_SCOPE_FOR || scope->_type == TRI_AQL_SCOPE_FOR_NESTED) {
    return TRI_AQL_SCOPE_FOR_NESTED;
  }

  return type;
}

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

  TRI_InitAssociativePointer(&scope->_variables, 
                             TRI_UNKNOWN_MEM_ZONE, 
                             &TRI_HashStringKeyAssociativePointer, 
                             &HashVariable,
                             &EqualVariable,
                             NULL);
  
  TRI_PushBackVectorPointer(&generator->_scopes, (void*) scope);

  ScopeOutput(generator, "\n/* scope start (");
  ScopeOutput(generator, scope->_name);
  ScopeOutput(generator, ") */\n");
}

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
  
  n = scope->_variables._nrAlloc;
  for (i = 0; i < n; ++i) {
    TRI_aql_codegen_variable_t* variable = (TRI_aql_codegen_variable_t*) scope->_variables._table[i];
    if (variable) {
      FreeVariable(variable);
    }
  }
  TRI_DestroyAssociativePointer(&scope->_variables); 

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, scope);         
}

static TRI_aql_codegen_register_t CreateSortFunction (TRI_aql_codegen_js_t* const generator, 
                                                      const TRI_aql_node_t* const node) {
  TRI_aql_node_t* list;
  TRI_aql_codegen_scope_t* scope = CurrentScope(generator);
  TRI_aql_codegen_register_t functionIndex = IncFunction(generator);
  size_t i;
  size_t n;

  ScopeOutput(generator, "function ");
  ScopeOutputFunction(generator, functionIndex);
  ScopeOutput(generator, " (l, r) {\n");
  ScopeOutput(generator, "var lhs, rhs;\n");

  list = TRI_AQL_NODE_MEMBER(node, 0);
  n = list->_members._length;
  for (i = 0; i < n; ++i) {
    TRI_aql_node_t* element = TRI_AQL_NODE_MEMBER(list, i);

    scope->_prefix = "l";
    ScopeOutput(generator, "lhs = ");
    ProcessNode(generator, TRI_AQL_NODE_MEMBER(element, 0));
    ScopeOutput(generator, ";\n");
          
    scope->_prefix = "r";
    ScopeOutput(generator, "rhs = ");
    ProcessNode(generator, TRI_AQL_NODE_MEMBER(element, 0));
    ScopeOutput(generator, ";\n");

    scope->_prefix = NULL;
    ScopeOutput(generator, "if (AHUACATL_RELATIONAL_LESS(lhs, rhs)) {\n");
    ScopeOutput(generator, TRI_AQL_NODE_BOOL(element) ? "return -1;\n" : "return 1;\n");
    ScopeOutput(generator, "}\n");
    ScopeOutput(generator, "if (AHUACATL_RELATIONAL_GREATER(lhs, rhs)) {\n");
    ScopeOutput(generator, TRI_AQL_NODE_BOOL(element) ? "return 1;\n" : "return -1;\n");
    ScopeOutput(generator, "}\n");
  }
  ScopeOutput(generator, "return 0;\n");
  ScopeOutput(generator, "}\n");

  return functionIndex;
}

static void InitList (TRI_aql_codegen_js_t* const generator, const TRI_aql_codegen_register_t resultRegister) {
  ScopeOutput(generator, "var ");
  ScopeOutputRegister(generator, resultRegister);
  ScopeOutput(generator, " = [ ];\n");
}

static void InitArray (TRI_aql_codegen_js_t* const generator, const TRI_aql_codegen_register_t resultRegister) {
  ScopeOutput(generator, "var ");
  ScopeOutputRegister(generator, resultRegister);
  ScopeOutput(generator, " = { };\n");
}

static void EnterSymbol (TRI_aql_codegen_js_t* const generator, const char* const name, const TRI_aql_codegen_register_t registerIndex) {
  TRI_aql_codegen_scope_t* scope = CurrentScope(generator);
  TRI_aql_codegen_variable_t* variable = CreateVariable(name, registerIndex);

  if (!variable) {
    generator->_error = true;
    return;
  }

  if (TRI_InsertKeyAssociativePointer(&scope->_variables, name, (void*) variable, false)) {
    generator->_error = true;
  }
}

static TRI_aql_codegen_register_t LookupSymbol (TRI_aql_codegen_js_t* const generator, const char* const name) {
  size_t i = generator->_scopes._length;

  while (i-- > 0) {
    TRI_aql_codegen_scope_t* scope = (TRI_aql_codegen_scope_t*) generator->_scopes._buffer[i];
    TRI_aql_codegen_variable_t* variable;

    variable = (TRI_aql_codegen_variable_t*) TRI_LookupByKeyAssociativePointer(&scope->_variables, name);
    if (variable) {
      return variable->_register;
    }
  }

  generator->_error = true;
  return 0;
}

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

static void StartFor (TRI_aql_codegen_js_t* const generator,
                      const TRI_aql_codegen_register_t sourceRegister,
                      const char* const variableName) {
  TRI_aql_codegen_register_t listRegister = IncRegister(generator);
  TRI_aql_codegen_register_t keyRegister = IncRegister(generator);
  TRI_aql_codegen_register_t ownRegister = IncRegister(generator);
  TRI_aql_codegen_scope_t* scope = CurrentScope(generator);

  StartScope(generator, &generator->_buffer, TRI_AQL_SCOPE_FOR, listRegister, keyRegister, ownRegister, scope->_resultRegister, variableName, "for");
  if (variableName) {
    EnterSymbol(generator, variableName, ownRegister);
  }

  ScopeOutput(generator, "var ");
  ScopeOutputRegister(generator, listRegister);
  ScopeOutput(generator, " = AHUACATL_LIST(");
  ScopeOutputRegister(generator, sourceRegister);
  ScopeOutput(generator, ");\n");
        
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

static void CloseLoops (TRI_aql_codegen_js_t* const generator) {
  TRI_aql_codegen_scope_t* scope = CurrentScope(generator);

  while (true) {
    TRI_aql_codegen_scope_e type = scope->_type;
    ScopeOutput(generator, "}\n");
    EndScope(generator);

    if (type != TRI_AQL_SCOPE_FOR_NESTED) {
      break;
    }

    scope = CurrentScope(generator);
  }
}

static TRI_vector_string_t StoreSymbols (TRI_aql_codegen_js_t* const generator, 
                                         const TRI_aql_codegen_register_t rowRegister) {
  TRI_vector_string_t variableNames;
  size_t i = generator->_scopes._length;

  TRI_InitVectorString(&variableNames, TRI_UNKNOWN_MEM_ZONE);

  while (i-- > 0) {
    TRI_aql_codegen_scope_t* peek = (TRI_aql_codegen_scope_t*) generator->_scopes._buffer[i];
    size_t j, n;

    n = peek->_variables._nrAlloc;
    for (j = 0; j < n; ++j) {
      TRI_aql_codegen_variable_t* variable = (TRI_aql_codegen_variable_t*) peek->_variables._table[j];

      if (!variable) {
        continue;
      }

      // row[name] = ...;
      ScopeOutputRegister(generator, rowRegister);
      ScopeOutput(generator, "[");
      ScopeOutputQuoted(generator, variable->_name);
      ScopeOutput(generator, "] = ");
      ScopeOutputRegister(generator, variable->_register);
      ScopeOutput(generator, ";\n");
  
      TRI_PushBackVectorString(&variableNames, TRI_DuplicateString(variable->_name));
    }

    if (peek->_type != TRI_AQL_SCOPE_FOR_NESTED) {
      break;
    }
  }

  return variableNames;
}

static void RestoreSymbols (TRI_aql_codegen_js_t* const generator, TRI_vector_string_t* const variableNames) {
  TRI_aql_codegen_scope_t* scope = CurrentScope(generator);
  size_t i, n;

  n = variableNames->_length;
  for (i = 0; i < n; ++i) {
    TRI_aql_codegen_register_t registerIndex = IncRegister(generator);
    char* variableName = (char*) variableNames->_buffer[i];

    // re-enter symbols
    if (variableName) {
      EnterSymbol(generator, variableName, registerIndex);
    }
  
    // unpack variables
    ScopeOutput(generator, "var ");
    ScopeOutputRegister(generator, registerIndex);
    ScopeOutput(generator, " = ");
    ScopeOutputRegister(generator, scope->_ownRegister);
    ScopeOutput(generator, "[");
    ScopeOutputQuoted(generator, variableName);
    ScopeOutput(generator, "];\n");
  }
  
  TRI_DestroyVectorString(variableNames);
}

static void ProcessReference (TRI_aql_codegen_js_t* const generator,
                              const TRI_aql_node_t* const node) { 
  TRI_aql_codegen_scope_t* scope = CurrentScope(generator);
  char* name = TRI_AQL_NODE_STRING(node);

  if (scope->_prefix) {
    // sort function
    ScopeOutput(generator, scope->_prefix);
    ScopeOutput(generator, "[");
    ScopeOutputQuoted(generator, name);
    ScopeOutput(generator, "]");
  }
  else {
    // regular scope
    TRI_aql_codegen_register_t registerIndex = LookupSymbol(generator, name);
    ScopeOutputRegister(generator, registerIndex);
  }
}

static void ProcessValue (TRI_aql_codegen_js_t* const generator, 
                          const TRI_aql_node_t* const node) {
  if (!TRI_ValueJavascriptAql(CurrentScope(generator)->_buffer, &node->_value, node->_value._type)) {
    generator->_error = true;
  }
}

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

static void ProcessArrayElement (TRI_aql_codegen_js_t* const generator, 
                                const TRI_aql_node_t* const node) {
  TRI_ValueJavascriptAql(CurrentScope(generator)->_buffer, &node->_value, AQL_TYPE_STRING);
  ScopeOutput(generator, " : ");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
}

static void ProcessAttributeAccess (TRI_aql_codegen_js_t* const generator,
                                    const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "AHUACATL_DOCUMENT_MEMBER(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ", ");
  ScopeOutputQuoted(generator, TRI_AQL_NODE_STRING(node));
  ScopeOutput(generator, ")");
}

static void ProcessIndexed (TRI_aql_codegen_js_t* const generator,
                            const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "AHUACATL_GET_INDEX(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ", ");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  ScopeOutput(generator, ")");
}

static void ProcessCollection (TRI_aql_codegen_js_t* const generator,
                               const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "AHUACATL_GET_DOCUMENTS('");
  ScopeOutput(generator, TRI_AQL_NODE_STRING(node));
  ScopeOutput(generator, "')");
}

static void ProcessUnaryNot (TRI_aql_codegen_js_t* const generator,
                             const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "AHUACATL_LOGICAL_NOT(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ")");
}

static void ProcessUnaryMinus (TRI_aql_codegen_js_t* const generator,
                               const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "AHUACATL_UNARY_MINUS(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ")");
}

static void ProcessUnaryPlus (TRI_aql_codegen_js_t* const generator,
                              const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "AHUACATL_UNARY_PLUS(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ")");
}

static void ProcessBinaryAnd (TRI_aql_codegen_js_t* const generator,
                              const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "AHUACATL_LOGICAL_AND(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ", ");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  ScopeOutput(generator, ")");
}

static void ProcessBinaryOr (TRI_aql_codegen_js_t* const generator,
                             const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "AHUACATL_LOGICAL_OR(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ", ");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  ScopeOutput(generator, ")");
}

static void ProcessBinaryPlus (TRI_aql_codegen_js_t* const generator,
                               const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "AHUACATL_ARITHMETIC_PLUS(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ", ");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  ScopeOutput(generator, ")");
}

static void ProcessBinaryMinus (TRI_aql_codegen_js_t* const generator,
                                const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "AHUACATL_ARITHMETIC_MINUS(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ", ");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  ScopeOutput(generator, ")");
}

static void ProcessBinaryTimes (TRI_aql_codegen_js_t* const generator,
                                const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "AHUACATL_ARITHMETIC_TIMES(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ", ");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  ScopeOutput(generator, ")");
}

static void ProcessBinaryDivide (TRI_aql_codegen_js_t* const generator,
                                 const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "AHUACATL_ARITHMETIC_DIVIDE(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ", ");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  ScopeOutput(generator, ")");
}

static void ProcessBinaryModulus(TRI_aql_codegen_js_t* const generator,
                                 const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "AHUACATL_ARITHMETIC_MODULUS(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ", ");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  ScopeOutput(generator, ")");
}

static void ProcessBinaryEqual (TRI_aql_codegen_js_t* const generator,
                                const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "AHUACATL_RELATIONAL_EQUAL(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ", ");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  ScopeOutput(generator, ")");
}

static void ProcessBinaryUnequal (TRI_aql_codegen_js_t* const generator,
                                  const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "AHUACATL_RELATIONAL_UNEQUAL(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ", ");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  ScopeOutput(generator, ")");
}

static void ProcessBinaryLessEqual (TRI_aql_codegen_js_t* const generator,
                                    const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "AHUACATL_RELATIONAL_LESSEQUAL(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ", ");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  ScopeOutput(generator, ")");
}

static void ProcessBinaryLess (TRI_aql_codegen_js_t* const generator,
                               const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "AHUACATL_RELATIONAL_LESS(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ", ");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  ScopeOutput(generator, ")");
}

static void ProcessBinaryGreaterEqual (TRI_aql_codegen_js_t* const generator,
                                       const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "AHUACATL_RELATIONAL_GREATEREQUAL(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ", ");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  ScopeOutput(generator, ")");
}

static void ProcessBinaryGreater (TRI_aql_codegen_js_t* const generator,
                                 const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "AHUACATL_RELATIONAL_GREATER(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ", ");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  ScopeOutput(generator, ")");
}

static void ProcessBinaryIn (TRI_aql_codegen_js_t* const generator,
                             const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "AHUACATL_RELATIONAL_IN(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ", ");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  ScopeOutput(generator, ")");
}

static void ProcessSubquery (TRI_aql_codegen_js_t* const generator, 
                             const TRI_aql_node_t* const node) {
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
}

static void ProcessFcall (TRI_aql_codegen_js_t* const generator,
                          const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "AHUACATL_FCALL(");
  ScopeOutput(generator, TRI_GetInternalNameFunctionAql((TRI_aql_function_t*) TRI_AQL_NODE_DATA(node)));
  ScopeOutput(generator, ", ");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ")");
}

static void ProcessFor (TRI_aql_codegen_js_t* const generator, 
                        const TRI_aql_node_t* const node) {
  TRI_aql_codegen_register_t sourceRegister = IncRegister(generator);
  TRI_aql_node_t* nameNode;

  ScopeOutput(generator, "var ");
  ScopeOutputRegister(generator, sourceRegister);
  ScopeOutput(generator, " = ");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  ScopeOutput(generator, ";\n");

  nameNode = TRI_AQL_NODE_MEMBER(node, 0);
  StartFor(generator, sourceRegister, nameNode->_value._value._string);
}

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
  
  variableNames = StoreSymbols(generator, rowRegister);

  // result.push(row)
  ScopeOutputRegister(generator, scope->_resultRegister);
  ScopeOutput(generator, ".push(");
  ScopeOutputRegister(generator, rowRegister);
  ScopeOutput(generator, ");\n");

  // }
  CloseLoops(generator);

  functionIndex = CreateSortFunction(generator, node);

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

  RestoreSymbols(generator, &variableNames);
}

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

  RestoreSymbols(generator, &variableNames);
}

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

static void ProcessAssign (TRI_aql_codegen_js_t* const generator, 
                           const TRI_aql_node_t* const node) {
  TRI_aql_node_t* nameNode = TRI_AQL_NODE_MEMBER(node, 0);
  TRI_aql_codegen_register_t resultRegister = IncRegister(generator);
  
  InitList(generator, resultRegister);

  StartScope(generator, &generator->_buffer, TRI_AQL_SCOPE_LET, 0, 0, 0, resultRegister, NULL, "let");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  EndScope(generator);
  
  EnterSymbol(generator, nameNode->_value._value._string, resultRegister);
}

static void ProcessFilter (TRI_aql_codegen_js_t* const generator, 
                           const TRI_aql_node_t* const node) {
  ScopeOutput(generator, "if (!(");
  ProcessNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  ScopeOutput(generator, ")) {\n");
  ScopeOutput(generator, "continue;\n");
  ScopeOutput(generator, "}\n");
}

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
//        ProcessAttribute(generator, node);
        break;
      case AQL_NODE_ATTRIBUTE_ACCESS:
        ProcessAttributeAccess(generator, node);
        break;
      case AQL_NODE_INDEXED:
        ProcessIndexed(generator, node);
        break;
      case AQL_NODE_EXPAND:
//        ProcessExpand(generator, node);
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
//        ProcessCollect(generator, node);
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

    node = node->_next;
  }
}


void fux (TRI_aql_codegen_js_t* generator, const void* const data) {
  TRI_aql_codegen_register_t resultRegister;

  resultRegister = IncRegister(generator);
  StartScope(generator, &generator->_buffer, TRI_AQL_SCOPE_MAIN, 0, 0, 0, resultRegister, NULL, "main");
  InitList(generator, resultRegister);
  
  ProcessNode(generator, (TRI_aql_node_t*) data);

  EndScope(generator);
}






////////////////////////////////////////////////////////////////////////////////
/// @brief append the value of a value node to the current scope's output buffer
////////////////////////////////////////////////////////////////////////////////

static void DumpNode (TRI_aql_codegen_js_t* const generator, const TRI_aql_node_t* const node);

static TRI_string_buffer_t* GetBuffer (TRI_aql_codegen_js_t* const generator) {
  TRI_aql_codegen_functionx_t* function;
  size_t n;
  
  n = generator->_functions._length;
  assert(n > 0);

  function = (TRI_aql_codegen_functionx_t*) generator->_functions._buffer[n - 1];
  assert(function);

  return &function->_buffer;
}

static size_t NextRegister (TRI_aql_codegen_js_t* const generator) {
  return ++generator->_registerIndex;
}

static size_t NextFunction (TRI_aql_codegen_js_t* const generator) {
  return ++generator->_functionIndex;
}

static TRI_aql_codegen_functionx_t* CreateFunction (TRI_aql_codegen_js_t* const generator) {   
  TRI_aql_codegen_functionx_t* function;

  function = (TRI_aql_codegen_functionx_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_aql_codegen_functionx_t), false);
  if (!function) {
    generator->_error = true;
    return NULL;
  }

  function->_index = NextFunction(generator);
  function->_forCount = 0;
  function->_prefix = NULL;

  TRI_InitStringBuffer(&function->_buffer, TRI_UNKNOWN_MEM_ZONE);

  return function;
}

static void FreeFunction (TRI_aql_codegen_functionx_t* const function) {
  assert(function);

  TRI_DestroyStringBuffer(&function->_buffer);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, function);
}

static void AppendString (TRI_aql_codegen_js_t* const generator, 
                          const char* const value) {
  if (!value) {
    generator->_error = true;
    return;
  }

  TRI_AppendStringStringBuffer(GetBuffer(generator), value);
}

static void AppendInt (TRI_aql_codegen_js_t* const generator, 
                       const int64_t value) {
  TRI_AppendInt64StringBuffer(GetBuffer(generator), value);
}

static void AppendQuoted (TRI_aql_codegen_js_t* const generator, 
                          const char* name) {
  if (!name) {
    generator->_error = true;
    return;
  }

  TRI_AppendCharStringBuffer(GetBuffer(generator), '\'');
  AppendString(generator, name);
  TRI_AppendCharStringBuffer(GetBuffer(generator), '\'');
}

static void AppendResultVariable (TRI_aql_codegen_js_t* const generator,
                                  const TRI_aql_node_t* const nameNode) {
  if (!nameNode) {
    generator->_error = true;
    return;
  }

  TRI_AppendStringStringBuffer(GetBuffer(generator), "$[");
  AppendQuoted(generator, nameNode->_value._value._string);
  TRI_AppendStringStringBuffer(GetBuffer(generator), "]");
}

static void AppendFuncName (TRI_aql_codegen_js_t* const generator, 
                            const size_t funcIndex) {
  AppendString(generator, FUNCTION_PREFIX);
  AppendInt(generator, (int64_t) funcIndex);
}

static void AppendRegisterName (TRI_aql_codegen_js_t* const generator, 
                                const size_t registerIndex) {
  AppendString(generator, REGISTER_PREFIX);
  AppendInt(generator, (int64_t) registerIndex);
}

static void AppendOwnPropertyCheck (TRI_aql_codegen_js_t* const generator, 
                                    const size_t listRegister,
                                    const size_t keyRegister) {
  AppendString(generator, "if (!");
  AppendRegisterName(generator, listRegister);
  AppendString(generator, ".hasOwnProperty(");
  AppendRegisterName(generator, keyRegister); 
  AppendString(generator, ")) {\n");
  AppendString(generator, "continue;\n");
  AppendString(generator, "}\n");
}

static void StartFunction (TRI_aql_codegen_js_t* const generator,
                           const TRI_aql_codegen_function_type_e type) {
  TRI_aql_codegen_functionx_t* function = CreateFunction(generator);

  if (!function) {
    generator->_error = true;
    return;
  }
  
  TRI_PushBackVectorPointer(&generator->_functions, function);
  
  AppendString(generator, "function ");
  AppendFuncName(generator, function->_index);
  if (type == AQL_FUNCTION_COMPARE) {
    AppendString(generator, "(l, r) {\n");
  } 
  else {
    AppendString(generator, "($) {\n");
  }
}

static size_t EndFunction (TRI_aql_codegen_js_t* const generator) {
  TRI_aql_codegen_functionx_t* function;
  size_t n;
  size_t funcIndex;

  AppendString(generator, "}\n");

  n = generator->_functions._length;
  assert(n > 0);
  function = TRI_RemoveVectorPointer(&generator->_functions, n -1);
  assert(function);

  TRI_AppendStringStringBuffer(&generator->_buffer, function->_buffer._buffer);
  funcIndex = function->_index;

  FreeFunction(function);

  return funcIndex;
}

static TRI_aql_codegen_functionx_t* CurrentFunction (TRI_aql_codegen_js_t* const generator) {
  TRI_aql_codegen_functionx_t* function;
  size_t n;

  n = generator->_functions._length;
  assert(n > 0);

  function = (TRI_aql_codegen_functionx_t*) generator->_functions._buffer[n - 1];
  assert(function);

  return function;
}

static void IncreaseForCount (TRI_aql_codegen_js_t* const generator) {
  TRI_aql_codegen_functionx_t* function = CurrentFunction(generator);

  if (!function) {
    return;
  }

  ++function->_forCount;
}

static void CloseForLoops (TRI_aql_codegen_js_t* const generator) {
  TRI_aql_codegen_functionx_t* function = CurrentFunction(generator);
  size_t i;

  if (!function) {
    return;
  }

  for (i = 0; i < function->_forCount; ++i) {
    AppendString(generator, "}\n");
  }

  function->_forCount = 0;
}

static void AppendForStart (TRI_aql_codegen_js_t* const generator, 
                            const size_t listRegister,
                            const size_t keyRegister) {
  AppendString(generator, "for (var "); 
  AppendRegisterName(generator, keyRegister);
  AppendString(generator, " in ");
  AppendRegisterName(generator, listRegister);
  AppendString(generator, ") {\n");
  IncreaseForCount(generator);
  AppendOwnPropertyCheck(generator, listRegister, keyRegister);
}

static size_t HandleSortNode (TRI_aql_codegen_js_t* const generator, 
                              const TRI_aql_node_t* const node,
                              const size_t idx) {
  TRI_aql_node_t* list;
  TRI_aql_codegen_functionx_t* function;
  size_t funcIndex;
  size_t i;
  size_t n;

  StartFunction(generator, AQL_FUNCTION_COMPARE);
  AppendString(generator, "var lhs, rhs;\n");

  function = CurrentFunction(generator);

  list = TRI_AQL_NODE_MEMBER(node, 0);
  n = list->_members._length;
  for (i = 0; i < n; ++i) {
    TRI_aql_node_t* element = TRI_AQL_NODE_MEMBER(list, i);

    AppendString(generator, "lhs = ");
    function->_prefix = "l";
    DumpNode(generator, TRI_AQL_NODE_MEMBER(element, idx));
    AppendString(generator, ";\n");
          
    AppendString(generator, "rhs = ");
    function->_prefix = "r";
    DumpNode(generator, TRI_AQL_NODE_MEMBER(element, idx));
    AppendString(generator, ";\n");
    function->_prefix = NULL;

    AppendString(generator, "if (AHUACATL_RELATIONAL_LESS(lhs, rhs)) {\n");
    AppendString(generator, (idx || TRI_AQL_NODE_BOOL(element)) ? "return -1;\n" : "return 1;\n");
    AppendString(generator, "}\n");
    AppendString(generator, "if (AHUACATL_RELATIONAL_GREATER(lhs, rhs)) {\n");
    AppendString(generator, (idx || TRI_AQL_NODE_BOOL(element)) ? "return 1;\n" : "return -1;\n");
    AppendString(generator, "}\n");
  }
  AppendString(generator, "return 0;\n");

  funcIndex = EndFunction(generator);

  return funcIndex;
}

static void HandleValue (TRI_aql_codegen_js_t* const generator, 
                         const TRI_aql_node_t* const node) {
  if (!TRI_ValueJavascriptAql(GetBuffer(generator), &node->_value, node->_value._type)) {
    generator->_error = true;
  }
}

static void HandleList (TRI_aql_codegen_js_t* const generator, 
                        const TRI_aql_node_t* const node) {
  size_t i, n;

  AppendString(generator, "[ ");
  n = node->_members._length;
  for (i = 0; i < n; ++i) {
    if (i > 0) {
      AppendString(generator, ", ");
    }
    DumpNode(generator, TRI_AQL_NODE_MEMBER(node, i));
  }
  AppendString(generator, " ]");
}

static void HandleArray (TRI_aql_codegen_js_t* const generator, 
                         const TRI_aql_node_t* const node) {
  size_t i, n;

  AppendString(generator, "{ ");
  n = node->_members._length;
  for (i = 0; i < n; ++i) {
    if (i > 0) {
      AppendString(generator, ", ");
    }
    DumpNode(generator, TRI_AQL_NODE_MEMBER(node, i));
  }
  AppendString(generator, " }");
} 

static void HandleArrayElement (TRI_aql_codegen_js_t* const generator, 
                                const TRI_aql_node_t* const node) {
  TRI_ValueJavascriptAql(GetBuffer(generator), &node->_value, AQL_TYPE_STRING);
  AppendString(generator, " : ");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
}

static void HandleCollection (TRI_aql_codegen_js_t* const generator,
                              const TRI_aql_node_t* const node) {
  AppendString(generator, "AHUACATL_GET_DOCUMENTS('");
  AppendString(generator, TRI_AQL_NODE_STRING(node));
  AppendString(generator, "')");
}

static void HandleReference (TRI_aql_codegen_js_t* const generator,
                             const TRI_aql_node_t* const node) { 
  TRI_aql_codegen_functionx_t* function;
  
  function = CurrentFunction(generator);
  if (function && function->_prefix) {
    AppendString(generator, function->_prefix);
    AppendString(generator, "['");
    AppendString(generator, TRI_AQL_NODE_STRING(node));
    AppendString(generator, "']");
    return;
  }

  AppendString(generator, "$[");
  AppendQuoted(generator, TRI_AQL_NODE_STRING(node));
  AppendString(generator, "]");
}

static void HandleAttribute (TRI_aql_codegen_js_t* const generator,
                             const TRI_aql_node_t* const node) {
  AppendString(generator, "AHUACATL_DOCUMENT_MEMBER(");
  AppendRegisterName(generator, generator->_last);
  AppendString(generator, "[");
  AppendRegisterName(generator, generator->_last + 1);
  AppendString(generator, "], ");
  AppendQuoted(generator, TRI_AQL_NODE_STRING(node));
  AppendString(generator, ")");
}

static void HandleAttributeAccess (TRI_aql_codegen_js_t* const generator,
                                   const TRI_aql_node_t* const node) {
  AppendString(generator, "AHUACATL_DOCUMENT_MEMBER(");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, ", ");
  AppendQuoted(generator, TRI_AQL_NODE_STRING(node));
  AppendString(generator, ")");
}

static void HandleIndexed (TRI_aql_codegen_js_t* const generator,
                           const TRI_aql_node_t* const node) {
  AppendString(generator, "AHUACATL_GET_INDEX(");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, ", ");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  AppendString(generator, ")");
}

static void HandleExpand (TRI_aql_codegen_js_t* const generator,
                          const TRI_aql_node_t* const node) {
  size_t r1 = NextRegister(generator);
  size_t r2 = NextRegister(generator);
  size_t funcIndex;

  generator->_last = r1;
  StartFunction(generator, AQL_FUNCTION_STANDALONE);
  AppendString(generator, "var result = [];\n");
  AppendString(generator, "var "); 
  AppendRegisterName(generator, r1);
  AppendString(generator, " = AHUACATL_LIST($);\n");
  AppendString(generator, "for (var "); 
  AppendRegisterName(generator, r2);
  AppendString(generator, " in "); 
  AppendRegisterName(generator, r1);
  AppendString(generator, ") {\n"); 
  AppendOwnPropertyCheck(generator, r1, r2);
  AppendString(generator, "result.push(");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  AppendString(generator, ");\n");
  AppendString(generator, "}\n");
  AppendString(generator, "return result;\n");

  funcIndex = EndFunction(generator);
  AppendFuncName(generator, funcIndex);
  AppendString(generator, "(");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, ")");
}

static void HandleUnaryNot (TRI_aql_codegen_js_t* const generator,
                            const TRI_aql_node_t* const node) {
  AppendString(generator, "AHUACATL_LOGICAL_NOT(");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, ")");
}

static void HandleUnaryMinus (TRI_aql_codegen_js_t* const generator,
                            const TRI_aql_node_t* const node) {
  AppendString(generator, "AHUACATL_UNARY_MINUS(");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, ")");
}

static void HandleUnaryPlus (TRI_aql_codegen_js_t* const generator,
                             const TRI_aql_node_t* const node) {
  AppendString(generator, "AHUACATL_UNARY_PLUS(");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, ")");
}

static void HandleBinaryAnd (TRI_aql_codegen_js_t* const generator,
                             const TRI_aql_node_t* const node) {
  AppendString(generator, "AHUACATL_LOGICAL_AND(");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, ", ");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  AppendString(generator, ")");
}

static void HandleBinaryOr (TRI_aql_codegen_js_t* const generator,
                            const TRI_aql_node_t* const node) {
  AppendString(generator, "AHUACATL_LOGICAL_OR(");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, ", ");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  AppendString(generator, ")");
}

static void HandleBinaryPlus (TRI_aql_codegen_js_t* const generator,
                              const TRI_aql_node_t* const node) {
  AppendString(generator, "AHUACATL_ARITHMETIC_PLUS(");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, ", ");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  AppendString(generator, ")");
}

static void HandleBinaryMinus (TRI_aql_codegen_js_t* const generator,
                               const TRI_aql_node_t* const node) {
  AppendString(generator, "AHUACATL_ARITHMETIC_MINUS(");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, ", ");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  AppendString(generator, ")");
}

static void HandleBinaryTimes (TRI_aql_codegen_js_t* const generator,
                               const TRI_aql_node_t* const node) {
  AppendString(generator, "AHUACATL_ARITHMETIC_TIMES(");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, ", ");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  AppendString(generator, ")");
}

static void HandleBinaryDivide (TRI_aql_codegen_js_t* const generator,
                                const TRI_aql_node_t* const node) {
  AppendString(generator, "AHUACATL_ARITHMETIC_DIVIDE(");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, ", ");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  AppendString(generator, ")");
}

static void HandleBinaryModulus(TRI_aql_codegen_js_t* const generator,
                                const TRI_aql_node_t* const node) {
  AppendString(generator, "AHUACATL_ARITHMETIC_MODULUS(");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, ", ");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  AppendString(generator, ")");
}

static void HandleBinaryEqual (TRI_aql_codegen_js_t* const generator,
                               const TRI_aql_node_t* const node) {
  AppendString(generator, "AHUACATL_RELATIONAL_EQUAL(");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, ", ");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  AppendString(generator, ")");
}

static void HandleBinaryUnequal (TRI_aql_codegen_js_t* const generator,
                                 const TRI_aql_node_t* const node) {
  AppendString(generator, "AHUACATL_RELATIONAL_UNEQUAL(");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, ", ");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  AppendString(generator, ")");
}

static void HandleBinaryLessEqual (TRI_aql_codegen_js_t* const generator,
                                   const TRI_aql_node_t* const node) {
  AppendString(generator, "AHUACATL_RELATIONAL_LESSEQUAL(");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, ", ");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  AppendString(generator, ")");
}

static void HandleBinaryLess (TRI_aql_codegen_js_t* const generator,
                              const TRI_aql_node_t* const node) {
  AppendString(generator, "AHUACATL_RELATIONAL_LESS(");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, ", ");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  AppendString(generator, ")");
}

static void HandleBinaryGreaterEqual (TRI_aql_codegen_js_t* const generator,
                                      const TRI_aql_node_t* const node) {
  AppendString(generator, "AHUACATL_RELATIONAL_GREATEREQUAL(");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, ", ");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  AppendString(generator, ")");
}

static void HandleBinaryGreater (TRI_aql_codegen_js_t* const generator,
                                 const TRI_aql_node_t* const node) {
  AppendString(generator, "AHUACATL_RELATIONAL_GREATER(");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, ", ");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  AppendString(generator, ")");
}

static void HandleBinaryIn (TRI_aql_codegen_js_t* const generator,
                            const TRI_aql_node_t* const node) {
  AppendString(generator, "AHUACATL_RELATIONAL_IN(");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, ", ");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  AppendString(generator, ")");
}

static void HandleFcall (TRI_aql_codegen_js_t* const generator,
                         const TRI_aql_node_t* const node) {
  AppendString(generator, "AHUACATL_FCALL(");
  AppendString(generator, TRI_GetInternalNameFunctionAql((TRI_aql_function_t*) TRI_AQL_NODE_DATA(node)));
  AppendString(generator, ", ");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, ")");
}

static void HandleFor (TRI_aql_codegen_js_t* const generator, 
                       const TRI_aql_node_t* const node) {
  size_t r1 = NextRegister(generator);
  size_t r2 = NextRegister(generator);

  AppendString(generator, "var ");
  AppendRegisterName(generator, r1); 
  AppendString(generator, " = AHUACATL_LIST(");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  AppendString(generator, ");\n");
        
  AppendForStart(generator, r1, r2);
  AppendResultVariable(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, " = ");
  AppendRegisterName(generator, r1);
  AppendString(generator, "[");
  AppendRegisterName(generator, r2); 
  AppendString(generator, "];\n");
}

static void HandleReturn (TRI_aql_codegen_js_t* const generator, 
                          const TRI_aql_node_t* const node) {
  AppendString(generator, "result.push(");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, ");\n");

  CloseForLoops(generator);
}

static void HandleFilter (TRI_aql_codegen_js_t* const generator, 
                          const TRI_aql_node_t* const node) {
  AppendString(generator, "if (!(");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, ")) {\n");
  AppendString(generator, "continue;\n");
  AppendString(generator, "}\n");
}

static void HandleSort (TRI_aql_codegen_js_t* const generator, 
                        const TRI_aql_node_t* const node) {
  size_t r1 = NextRegister(generator);
  size_t r2 = NextRegister(generator);
  size_t funcIndex;

  funcIndex = HandleSortNode(generator, node, 0);
        
  AppendString(generator, "result.push(AHUACATL_CLONE($));\n");
  CloseForLoops(generator);
  AppendString(generator, "AHUACATL_SORT(result, ");
  AppendFuncName(generator, funcIndex);
  AppendString(generator, ");\n");
  AppendString(generator, "return result;\n");
  funcIndex = EndFunction(generator);

  StartFunction(generator, AQL_FUNCTION_STANDALONE);
  AppendString(generator, "var result = [ ];\n");
  AppendString(generator, "var ");
  AppendRegisterName(generator, r1);
  AppendString(generator, " = ");
  AppendFuncName(generator, funcIndex);
  AppendString(generator, "($);\n");

  AppendForStart(generator, r1, r2);
  AppendString(generator, "var $ = ");
  AppendRegisterName(generator, r1);
  AppendString(generator, "[");
  AppendRegisterName(generator, r2);
  AppendString(generator, "];\n");
}

static void HandleLimit (TRI_aql_codegen_js_t* const generator, 
                         const TRI_aql_node_t* const node) {
  size_t r1 = NextRegister(generator);
  size_t r2 = NextRegister(generator);
  size_t funcIndex;

  AppendString(generator, "result.push(AHUACATL_CLONE($));\n");
  CloseForLoops(generator);
  AppendString(generator, "result = AHUACATL_LIMIT(result, ");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, ", ");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  AppendString(generator, ");\n");
  AppendString(generator, "return result;\n");

  funcIndex = EndFunction(generator);

  StartFunction(generator, AQL_FUNCTION_STANDALONE);
  AppendString(generator, "var result = [];\n");
  AppendString(generator, "var ");
  AppendRegisterName(generator, r1);
  AppendString(generator, " = ");
  AppendFuncName(generator, funcIndex);
  AppendString(generator, "($);\n");
  AppendString(generator, "for (var ");
  AppendRegisterName(generator, r2);
  AppendString(generator, " in ");
  AppendRegisterName(generator, r1);
  AppendString(generator, ") {\n");
  IncreaseForCount(generator);
  AppendOwnPropertyCheck(generator, r1, r2);
  AppendString(generator, "var $ = ");
  AppendRegisterName(generator, r1);
  AppendString(generator, "[");
  AppendRegisterName(generator, r2);
  AppendString(generator, "];\n");
}

static void HandleAssign (TRI_aql_codegen_js_t* const generator, 
                          const TRI_aql_node_t* const node) {
  AppendResultVariable(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, " = ");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  AppendString(generator, ";\n");
}

static void HandleCollect (TRI_aql_codegen_js_t* const generator, 
                           const TRI_aql_node_t* const node) {
  TRI_aql_node_t* list;
  size_t r1 = NextRegister(generator);
  size_t r2 = NextRegister(generator);
  size_t sortFuncIndex;
  size_t groupFuncIndex;
  size_t funcIndex;
  size_t i;
  size_t n;

  sortFuncIndex = HandleSortNode(generator, node, 1);

  StartFunction(generator, AQL_FUNCTION_STANDALONE);
  AppendString(generator, "return { ");
  list = TRI_AQL_NODE_MEMBER(node, 0);
  n = list->_members._length;
  for (i = 0; i < n; ++i) {
    TRI_aql_node_t* element = TRI_AQL_NODE_MEMBER(list, i);
    TRI_aql_node_t* varNode = TRI_AQL_NODE_MEMBER(element, 0);
    if (i > 0) {
      AppendString(generator, ", ");
    }
    AppendQuoted(generator, TRI_AQL_NODE_STRING(varNode));
    AppendString(generator, " : ");
    DumpNode(generator, TRI_AQL_NODE_MEMBER(element, 1));
  }
  AppendString(generator, " };\n");
  groupFuncIndex = EndFunction(generator);
       
  AppendString(generator, "result.push(AHUACATL_CLONE($));\n");
  CloseForLoops(generator);
  AppendString(generator, "result = AHUACATL_GROUP(result, ");
  AppendFuncName(generator, sortFuncIndex);
  AppendString(generator, ", ");
  AppendFuncName(generator, groupFuncIndex);

  if (node->_members._length > 1) {
    TRI_aql_node_t* nameNode = TRI_AQL_NODE_MEMBER(node, 1);
    AppendString(generator, ", ");
    AppendQuoted(generator, TRI_AQL_NODE_STRING(nameNode));
  }
  AppendString(generator, ");\n");
  AppendString(generator, "return result;\n");
  funcIndex = EndFunction(generator);
 
  StartFunction(generator, AQL_FUNCTION_STANDALONE); 
  AppendString(generator, "var result = [ ];\n");
  AppendString(generator, "var ");
  AppendRegisterName(generator, r1);
  AppendString(generator, " = ");
  AppendFuncName(generator, funcIndex);
  AppendString(generator, "($);\n");
  AppendForStart(generator, r1, r2);
  AppendString(generator, "var $ = ");
  AppendRegisterName(generator, r1);
  AppendString(generator, "[\n");
  AppendRegisterName(generator, r2);
  AppendString(generator, "];\n");
}

static void HandleSubquery (TRI_aql_codegen_js_t* const generator, 
                            const TRI_aql_node_t* const node) {
  size_t funcIndex;

  StartFunction(generator, AQL_FUNCTION_STANDALONE);
  AppendString(generator, "var result = [ ];\n");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, "return result;\n");
  funcIndex = EndFunction(generator);

  AppendFuncName(generator, funcIndex);
  AppendString(generator, "($)");
}

static void DumpNode (TRI_aql_codegen_js_t* generator, const TRI_aql_node_t* const data) {
  TRI_aql_node_t* node = (TRI_aql_node_t*) data;

  assert(generator);

  if (!node) {
    return;
  }

  while (node) {
    switch (node->_type) {
      case AQL_NODE_VALUE:
        HandleValue(generator, node);
        break;
      case AQL_NODE_LIST: 
        HandleList(generator, node);
        break;
      case AQL_NODE_ARRAY: 
        HandleArray(generator, node);
        break;
      case AQL_NODE_ARRAY_ELEMENT: 
        HandleArrayElement(generator, node);
        break;
      case AQL_NODE_COLLECTION:
        HandleCollection(generator, node);
        break;
      case AQL_NODE_REFERENCE: 
        HandleReference(generator, node);
        break;
      case AQL_NODE_ATTRIBUTE:
        HandleAttribute(generator, node);
        break;
      case AQL_NODE_ATTRIBUTE_ACCESS:
        HandleAttributeAccess(generator, node);
        break;
      case AQL_NODE_INDEXED:
        HandleIndexed(generator, node);
        break;
      case AQL_NODE_EXPAND:
        HandleExpand(generator, node);
        break; 
      case AQL_NODE_OPERATOR_UNARY_NOT:
        HandleUnaryNot(generator, node);
        break;
      case AQL_NODE_OPERATOR_UNARY_PLUS:
        HandleUnaryPlus(generator, node);
        break;
      case AQL_NODE_OPERATOR_UNARY_MINUS:
        HandleUnaryMinus(generator, node);
        break;
      case AQL_NODE_OPERATOR_BINARY_AND:
        HandleBinaryAnd(generator, node);
        break;
      case AQL_NODE_OPERATOR_BINARY_OR:
        HandleBinaryOr(generator, node);
        break;
      case AQL_NODE_OPERATOR_BINARY_PLUS:
        HandleBinaryPlus(generator, node);
        break;
      case AQL_NODE_OPERATOR_BINARY_MINUS:
        HandleBinaryMinus(generator, node);
        break;
      case AQL_NODE_OPERATOR_BINARY_TIMES:
        HandleBinaryTimes(generator, node);
        break;
      case AQL_NODE_OPERATOR_BINARY_DIV:
        HandleBinaryDivide(generator, node);
        break;
      case AQL_NODE_OPERATOR_BINARY_MOD:
        HandleBinaryModulus(generator, node);
        break;
      case AQL_NODE_OPERATOR_BINARY_EQ:
        HandleBinaryEqual(generator, node);
        break;
      case AQL_NODE_OPERATOR_BINARY_NE:
        HandleBinaryUnequal(generator, node);
        break;
      case AQL_NODE_OPERATOR_BINARY_LT:
        HandleBinaryLess(generator, node);
        break;
      case AQL_NODE_OPERATOR_BINARY_LE:
        HandleBinaryLessEqual(generator, node);
        break;
      case AQL_NODE_OPERATOR_BINARY_GT:
        HandleBinaryGreater(generator, node);
        break;
      case AQL_NODE_OPERATOR_BINARY_GE:
        HandleBinaryGreaterEqual(generator, node);
        break;
      case AQL_NODE_OPERATOR_BINARY_IN:
        HandleBinaryIn(generator, node);
        break;
      case AQL_NODE_FCALL:
        HandleFcall(generator, node);
        break;
      case AQL_NODE_FOR:
        HandleFor(generator, node);
        break;
      case AQL_NODE_COLLECT: 
        HandleCollect(generator, node);
        break;
      case AQL_NODE_ASSIGN:
        HandleAssign(generator, node);
        break;
      case AQL_NODE_FILTER:
        HandleFilter(generator, node);
        break;
      case AQL_NODE_LIMIT:
        HandleLimit(generator, node);
        break;
      case AQL_NODE_SORT:
        HandleSort(generator, node);
        break;
      case AQL_NODE_RETURN:
        HandleReturn(generator, node);
        break;
      case AQL_NODE_SUBQUERY:
        HandleSubquery(generator, node);
        break;
      default: {
      }
    }

    node = node->_next;
  }

}

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
/// @brief free memory associated with a code generator
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeGeneratorAql (TRI_aql_codegen_js_t* const generator) {
  TRI_DestroyVectorPointer(&generator->_functions);
  TRI_DestroyVectorPointer(&generator->_scopes);
  TRI_DestroyStringBuffer(&generator->_buffer);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, generator);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a code generator
////////////////////////////////////////////////////////////////////////////////

TRI_aql_codegen_js_t* TRI_CreateGeneratorAql (void) {
  TRI_aql_codegen_js_t* generator;

  generator = (TRI_aql_codegen_js_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_aql_codegen_js_t), false);
  if (!generator) {
    return NULL;
  }

  TRI_InitStringBuffer(&generator->_buffer, TRI_UNKNOWN_MEM_ZONE);
  TRI_InitVectorPointer(&generator->_functions, TRI_UNKNOWN_MEM_ZONE);
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

TRI_aql_codegen_js_t* TRI_GenerateCodeAql (const void* const data) {
  TRI_aql_codegen_js_t* generator;
  size_t funcIndex;

  generator = TRI_CreateGeneratorAql();
  if (!generator) {
    return NULL;
  }

//  fux(generator, data);

  StartFunction(generator, AQL_FUNCTION_STANDALONE);
  AppendString(generator, "var result = [];\n");
  DumpNode(generator, (TRI_aql_node_t*) data);
  AppendString(generator, "return result;\n");
  funcIndex = EndFunction(generator);

  TRI_AppendStringStringBuffer(&generator->_buffer, FUNCTION_PREFIX);
  TRI_AppendInt64StringBuffer(&generator->_buffer, (int64_t) funcIndex);
  TRI_AppendStringStringBuffer(&generator->_buffer, "({ });");

  LOG_DEBUG("generated code: %s", generator->_buffer._buffer);
//  printf("generated code: %s", generator->_buffer._buffer);
  return generator;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

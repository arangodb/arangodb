////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, parser types and helper functionality
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

#include "Ahuacatl/ahuacatl-ast-node.h"
#include "Ahuacatl/ahuacatl-bind-parameter.h"
#include "Ahuacatl/ahuacatl-collections.h"
#include "Ahuacatl/ahuacatl-constant-folder.h"
#include "Ahuacatl/ahuacatl-context.h"
#include "Ahuacatl/ahuacatl-tree-dump.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                    private macros
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief shortcut macro for signalling out of memory
////////////////////////////////////////////////////////////////////////////////

#define ABORT_OOM \
  TRI_SetErrorAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL); \
  return NULL;

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
/// @brief hash variable 
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashVariable (TRI_associative_pointer_t* array, 
                              void const* element) {
  TRI_aql_variable_t* variable = (TRI_aql_variable_t*) element;

  return TRI_FnvHashString(variable->_name);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief comparison function used to determine variable equality
////////////////////////////////////////////////////////////////////////////////

static bool EqualVariable (TRI_associative_pointer_t* array, 
                           void const* key, 
                           void const* element) {
  TRI_aql_variable_t* variable = (TRI_aql_variable_t*) element;

  return TRI_EqualString(key, variable->_name);
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
/// @brief create and initialize a parse context
////////////////////////////////////////////////////////////////////////////////

TRI_aql_context_t* TRI_CreateContextAql (TRI_vocbase_t* vocbase,
                                                    const char* const query) {
  TRI_aql_context_t* context;

  context = (TRI_aql_context_t*) TRI_Allocate(sizeof(TRI_aql_context_t));
  if (!context) {
    return NULL;
  }

  context->_vocbase = vocbase;
  
  // actual bind parameter values
  TRI_InitAssociativePointer(&context->_parameterValues,
                             &TRI_HashStringKeyAssociativePointer,
                             &TRI_HashBindParameterAql,
                             &TRI_EqualBindParameterAql,
                             0);

  // bind parameter names used in the query
  TRI_InitAssociativePointer(&context->_parameterNames,
                             &TRI_HashStringKeyAssociativePointer,
                             &TRI_HashStringKeyAssociativePointer,
                             &TRI_EqualStringKeyAssociativePointer,
                             0);
  
  // collections
  TRI_InitAssociativePointer(&context->_collectionNames,
                             &TRI_HashStringKeyAssociativePointer,
                             &TRI_HashStringKeyAssociativePointer,
                             &TRI_EqualStringKeyAssociativePointer,
                             0);
  
  TRI_InitVectorPointer(&context->_stack);
  TRI_InitVectorPointer(&context->_nodes);
  TRI_InitVectorPointer(&context->_strings);
  TRI_InitVectorPointer(&context->_scopes);
  TRI_InitVectorPointer(&context->_collections);

  TRI_InitErrorAql(&context->_error);

  context->_query = NULL;
  context->_parser = NULL;
  context->_first = NULL;

  context->_query = TRI_DuplicateString(query);
  if (!context->_query) {
    TRI_FreeContextAql(context);
    return NULL;
  }

  context->_parser = (TRI_aql_parser_t*) TRI_Allocate(sizeof(TRI_aql_parser_t));
  if (!context->_parser) {
    TRI_FreeContextAql(context);
    return NULL;
  }

  context->_parser->_buffer = context->_query;
  context->_parser->_length = strlen(context->_query);

  Ahuacatllex_init(&context->_parser->_scanner);
  Ahuacatlset_extra(context, context->_parser->_scanner);

  return context;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free a parse context
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeContextAql (TRI_aql_context_t* const context) {
  size_t i;

  assert(context);

  // remove barriers for all collections used
  TRI_RemoveBarrierCollectionsAql(context);

  // read-unlock all collections used
  TRI_ReadUnlockCollectionsAql(context);

  // release all collections used
  TRI_UnlockCollectionsAql(context);

  // free all remaining scopes
  while (context->_scopes._length) {
    TRI_EndScopeContextAql(context);
  }
  TRI_DestroyVectorPointer(&context->_scopes);

  // free all strings registered
  i = context->_strings._length;
  while (i--) {
    void* string = context->_strings._buffer[i];

    if (string) {
      TRI_Free(context->_strings._buffer[i]);
    }
  }
  TRI_DestroyVectorPointer(&context->_strings);
 
  // free all nodes registered
  i = context->_nodes._length;
  while (i--) {
    TRI_aql_node_t* node = (TRI_aql_node_t*) context->_nodes._buffer[i];
    if (node) {
      TRI_DestroyVectorPointer(&node->_subNodes);
      // free node itself
      TRI_Free(node);
    }
  }
  TRI_DestroyVectorPointer(&context->_nodes);

  // free the stack
  TRI_DestroyVectorPointer(&context->_stack);

  // free parameter names hash
  TRI_DestroyAssociativePointer(&context->_parameterNames);
  
  // free collection names
  TRI_DestroyAssociativePointer(&context->_collectionNames);
  
  // free collections
  i = context->_collections._length;
  while (i--) {
    TRI_aql_collection_t* collection = (TRI_aql_collection_t*) context->_collections._buffer[i];
    if (collection) {
      TRI_Free(collection);
    }
  }
  TRI_DestroyVectorPointer(&context->_collections);
  
  // free parameter values
  TRI_FreeBindParametersAql(context);
  TRI_DestroyAssociativePointer(&context->_parameterValues);

  // free query string
  if (context->_query) {
    TRI_Free(context->_query);
  }

  // free lexer
  if (context->_parser) {
    Ahuacatllex_destroy(context->_parser->_scanner);
    TRI_Free(context->_parser);
  }
  
  // free error struct
  TRI_DestroyErrorAql(&context->_error);

  TRI_Free(context);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parse & validate the query string
////////////////////////////////////////////////////////////////////////////////
  
bool TRI_ValidateQueryContextAql (TRI_aql_context_t* const context) {
  // parse the query
  if (Ahuacatlparse(context)) {
    // lexing/parsing failed
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add bind parameters to the query context
////////////////////////////////////////////////////////////////////////////////
 
bool TRI_BindQueryContextAql (TRI_aql_context_t* const context,
                              const TRI_json_t* const parameters) {

  // add the bind parameters
  if (!TRI_AddParameterValuesAql(context, parameters)) {
    // adding parameters failed
    return false;
  }
  
  // validate the bind parameters used/passed
  if (!TRI_ValidateBindParametersAql(context)) {
    // invalid bind parameters
    return false;
  }

  // inject the bind parameter values into the query AST
  if (!TRI_InjectBindParametersAql(context, (TRI_aql_node_t*) context->_first)) {
    // bind parameter injection failed
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform some AST optimisations
////////////////////////////////////////////////////////////////////////////////

bool TRI_OptimiseQueryContextAql (TRI_aql_context_t* const context) {
  // do some basic optimisations in the AST
  if (!TRI_FoldConstantsAql(context, (TRI_aql_node_t*) context->_first)) {
    // constant folding failed
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief acquire all locks necessary for the query
////////////////////////////////////////////////////////////////////////////////

bool TRI_LockQueryContextAql (TRI_aql_context_t* const context) {
  // mark all used collections as being used
  if (!TRI_LockCollectionsAql(context)) {
    return false;
  }

  // acquire read locks on all collections used
  if (!TRI_ReadLockCollectionsAql(context)) {
    return false;
  }
  
  // add barriers for all collections used
  if (!TRI_AddBarrierCollectionsAql(context)) {
    return false;
  }

  // TRI_DumpTreeAql((TRI_aql_node_t*) context->_first);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new variable scope
////////////////////////////////////////////////////////////////////////////////

TRI_aql_scope_t* TRI_CreateScopeAql (void) {
  TRI_aql_scope_t* scope;

  scope = (TRI_aql_scope_t*) TRI_Allocate(sizeof(TRI_aql_scope_t));
  if (!scope) {
    return NULL;
  }

  TRI_InitAssociativePointer(&scope->_variables, 
                             TRI_HashStringKeyAssociativePointer,
                             HashVariable,
                             EqualVariable, 
                             0);
  
  scope->_first = NULL;
  scope->_last = NULL;

  return scope;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free a variable scope
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeScopeAql (TRI_aql_scope_t* const scope) {
  size_t i, length;

  assert(scope);
 
  // free variables lookup hash
  length = scope->_variables._nrAlloc;
  for (i = 0; i < length; ++i) {
    TRI_aql_variable_t* variable = scope->_variables._table[i];

    if (variable) {
      TRI_FreeVariableAql(variable);
    }
  }

  TRI_DestroyAssociativePointer(&scope->_variables);

  TRI_Free(scope);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register a node
////////////////////////////////////////////////////////////////////////////////

bool TRI_RegisterNodeContextAql (TRI_aql_context_t* const context,
                                      void* const node) {
  assert(context);
  assert(node);

  TRI_PushBackVectorPointer(&context->_nodes, node);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register an error
////////////////////////////////////////////////////////////////////////////////

void TRI_SetErrorAql (TRI_aql_context_t* const context, 
                      const int code,
                      const char* const data) {

  assert(context);
  assert(code > 0);

  if (context->_error._code == 0) {
    // do not overwrite previous error
    TRI_set_errno(code);
    context->_error._code = code;
    context->_error._message = (char*) TRI_last_error();
    if (data) {
      context->_error._data = TRI_DuplicateString(data);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register a parse error
////////////////////////////////////////////////////////////////////////////////

void TRI_SetParseErrorAql (TRI_aql_context_t* const context,
                           const char* const message,
                           const int line,
                           const int column) {
  char buffer[1024];

  snprintf(buffer, 
           sizeof(buffer), 
           "%d:%d %s near '%s'", 
           line,
           column,
           message, 
           TRI_GetContextErrorAql(context->_query, line, column));

  TRI_SetErrorAql(context, TRI_ERROR_QUERY_PARSE, buffer); 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief push something on the stack
////////////////////////////////////////////////////////////////////////////////

bool TRI_PushStackAql (TRI_aql_context_t* const context, 
                       const void* const value) {
  assert(context);

  if (!value) {
    ABORT_OOM
  }

  TRI_PushBackVectorPointer(&context->_stack, (void*) value);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief pop something from the stack
////////////////////////////////////////////////////////////////////////////////

void* TRI_PopStackAql (TRI_aql_context_t* const context) {
  assert(context);
  assert(context->_stack._length > 0);

  return TRI_RemoveVectorPointer(&context->_stack, context->_stack._length - 1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief peek at the end of the stack
////////////////////////////////////////////////////////////////////////////////

void* TRI_PeekStackAql (TRI_aql_context_t* const context) {
  assert(context);
  assert(context->_stack._length > 0);

  return context->_stack._buffer[context->_stack._length - 1];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get first statement in the current scope
////////////////////////////////////////////////////////////////////////////////

void* TRI_GetFirstStatementAql (TRI_aql_context_t* const context) {
  TRI_aql_scope_t* scope;
  size_t length;

  assert(context);

  length = context->_scopes._length;

  assert(length > 0);

  scope = (TRI_aql_scope_t*) context->_scopes._buffer[length - 1];
  assert(scope);

  return scope->_first;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a statement to the current scope
////////////////////////////////////////////////////////////////////////////////

bool TRI_AddStatementAql (TRI_aql_context_t* const context, 
                          const void* const statement) {
  TRI_aql_scope_t* scope;
  size_t length;

  assert(context);
  assert(statement);

  length = context->_scopes._length;

  assert(length > 0);

  scope = (TRI_aql_scope_t*) context->_scopes._buffer[length - 1];
  if (!scope->_first) {
    if (length == 1 && !context->_first) {
      // first ever statement on outermost scope
      context->_first = (void*) statement;
    }

    scope->_first = (void*) statement;
    scope->_last = (void*) statement;
  }
  else {
    TRI_aql_node_t* node = (TRI_aql_node_t*) scope->_last;

    node->_next = (void*) statement;
    scope->_last = (void*) statement;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new variable scope and stack it in the parser context
////////////////////////////////////////////////////////////////////////////////

TRI_aql_scope_t* TRI_StartScopeContextAql (TRI_aql_context_t* const context) {
  TRI_aql_scope_t* scope;

  assert(context);

  scope = TRI_CreateScopeAql();
  if (!scope) {
    ABORT_OOM
  }

  TRI_PushBackVectorPointer(&context->_scopes, scope);
  return scope;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove a variable scope from parser context scopes stack
////////////////////////////////////////////////////////////////////////////////

void TRI_EndScopeContextAql (TRI_aql_context_t* const context) {
  TRI_aql_scope_t* scope;
  size_t length;
//size_t i;

  assert(context);

  length = context->_scopes._length;

  assert(length > 0);

  scope = (TRI_aql_scope_t*) TRI_RemoveVectorPointer(&context->_scopes, length - 1);
  
  assert(scope);
/*
for (i = 0; i < scope->_variables._nrAlloc; i++) {
  if (scope->_variables._table[i]) {
    printf("END VARIABLE IN SCOPE: %s\n", ((TRI_aql_variable_t*) scope->_variables._table[i])->_name);
  }
}
*/
  TRI_FreeScopeAql(scope);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief move the contents of the outermost variable scope into the previous 
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExchangeScopeContextAql (TRI_aql_context_t* const context) {
  TRI_aql_scope_t* scope;
  size_t length;

  assert(context);
  
  length = context->_scopes._length;

  assert(length > 1);
  
  scope = (TRI_aql_scope_t*) TRI_RemoveVectorPointer(&context->_scopes, length - 2);
printf("EXCHANGING SCOPE\n");
  if (!scope) {
    // signal OOM
    return false;
  }

  TRI_FreeScopeAql(scope);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief push a variable into the current scope context
////////////////////////////////////////////////////////////////////////////////

bool TRI_AddVariableContextAql (TRI_aql_context_t* const context, const char* name) {
  TRI_aql_variable_t* variable;
  TRI_aql_scope_t* scope;
  size_t current;
  
  assert(context);
  assert(name);

  if (TRI_VariableExistsAql(context, name)) {
    return false;
  }

  variable = TRI_CreateVariableAql(name);
  if (!variable) {
    return false;
  }

  // use outermost scope for the assignment
  current = context->_scopes._length - 1;
  scope = (TRI_aql_scope_t*) context->_scopes._buffer[current];
  assert(scope);
  assert(!TRI_InsertKeyAssociativePointer(&scope->_variables, variable->_name, (void*) variable, false));

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register a new variable
////////////////////////////////////////////////////////////////////////////////

TRI_aql_variable_t* TRI_CreateVariableAql (const char* const name) {
  TRI_aql_variable_t* variable;

  variable = (TRI_aql_variable_t*) TRI_Allocate(sizeof(TRI_aql_variable_t));
  if (!variable) {
    return NULL;
  }

  variable->_name = TRI_DuplicateString(name);
  if (!variable->_name) {
    TRI_FreeVariableAql(variable);
    return NULL;
  }

  return variable;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free an existing variable
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeVariableAql (TRI_aql_variable_t* const variable) {
  assert(variable);

  if (variable->_name) {
    TRI_Free(variable->_name);
  }

  TRI_Free(variable);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register a string
////////////////////////////////////////////////////////////////////////////////

char* TRI_RegisterStringAql (TRI_aql_context_t* const context, 
                             const char* const value, 
                             const size_t length) {
  char* copy;
  size_t outLength;

  if (!value) {
    ABORT_OOM
  }

  copy = TRI_UnescapeUtf8String(value, length, &outLength);
  if (!copy) {
    ABORT_OOM
  }

  TRI_PushBackVectorPointer(&context->_strings, copy);

  return copy;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a variable is defined in the current scope or above
////////////////////////////////////////////////////////////////////////////////

bool TRI_VariableExistsAql (TRI_aql_context_t* const context, 
                            const char* const name) {
  size_t current = context->_scopes._length;

  if (!name) {
    TRI_SetErrorAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return false;
  }

  while (current > 0) {
    TRI_aql_scope_t* scope = (TRI_aql_scope_t*) context->_scopes._buffer[--current];
    assert(scope);

    if (TRI_LookupByKeyAssociativePointer(&scope->_variables, (void*) name)) {
      // duplicate variable
      return true;
    }

    if (current == 0) {
      // reached the outermost scope
      break;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a variable name follows the required naming convention 
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsValidVariableNameAql (const char* const name) {
  assert(name);

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

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

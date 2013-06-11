////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, scopes
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

#include "Ahuacatl/ahuacatl-scope.h"
#include "Ahuacatl/ahuacatl-access-optimiser.h"
#include "Ahuacatl/ahuacatl-variable.h"

#include "BasicsC/logging.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief get the current scope
////////////////////////////////////////////////////////////////////////////////

static inline TRI_aql_scope_t* CurrentScope (TRI_aql_context_t* const context) {
  TRI_aql_scope_t* scope;
  size_t n;

  assert(context);

  n = context->_currentScopes._length;
  assert(n > 0);

  scope = (TRI_aql_scope_t*) TRI_AtVectorPointer(&context->_currentScopes, n - 1);
  assert(scope);

  return scope;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the current type of a scope
////////////////////////////////////////////////////////////////////////////////

static inline TRI_aql_scope_e CurrentType (TRI_aql_context_t* const context) {
  return CurrentScope(context)->_type;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the type for the next scope
///
/// this returns TRI_AQL_SCOPE_FOR_NESTED for a nested for scope, and the
/// originally requested type in all other cases
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_scope_e NextType (TRI_aql_context_t* const context,
                                 const TRI_aql_scope_e requestedType) {
  TRI_aql_scope_e current;

  // we're only interested in TRI_AQL_SCOPE_FOR
  if (requestedType != TRI_AQL_SCOPE_FOR) {
    return requestedType;
  }

  current = CurrentType(context);

  // if we are in a TRI_AQL_SCOPE_FOR scope, we'll return TRI_AQL_SCOPE_FOR_NESTED
  if (current == TRI_AQL_SCOPE_FOR || current == TRI_AQL_SCOPE_FOR_NESTED) {
    return TRI_AQL_SCOPE_FOR_NESTED;
  }

  return requestedType;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a scope
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_scope_t* CreateScope (TRI_aql_context_t* const context,
                                     const TRI_aql_scope_e type) {
  TRI_aql_scope_t* scope;

  assert(context);

  scope = (TRI_aql_scope_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_aql_scope_t), false);

  if (scope == NULL) {
    return NULL;
  }

  scope->_type             = NextType(context, type);
  scope->_ranges           = NULL;
  scope->_selfContained    = true;
  scope->_level            = 0; // init with a dummy value

  scope->_limit._offset    = 0;
  scope->_limit._limit     = INT64_MAX;
  scope->_limit._status    = TRI_AQL_LIMIT_UNDEFINED;
  scope->_limit._hasFilter = false;
  scope->_limit._found     = 0;

  TRI_InitVectorPointer(&scope->_sorts, TRI_UNKNOWN_MEM_ZONE);

  TRI_InitAssociativePointer(&scope->_variables,
                             TRI_UNKNOWN_MEM_ZONE,
                             &TRI_HashStringKeyAssociativePointer,
                             &TRI_HashVariableAql,
                             &TRI_EqualVariableAql,
                             0);

  return scope;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free a scope
////////////////////////////////////////////////////////////////////////////////

static void FreeScope (TRI_aql_scope_t* const scope) {
  size_t i, n;

  // free variables lookup hash
  n = scope->_variables._nrAlloc;
  for (i = 0; i < n; ++i) {
    TRI_aql_variable_t* variable = scope->_variables._table[i];

    if (variable) {
      TRI_FreeVariableAql(variable);
    }
  }

  TRI_DestroyAssociativePointer(&scope->_variables);

  if (scope->_ranges) {
    // free ranges if set
    TRI_FreeAccessesAql(scope->_ranges);
  }

  for (i = 0; i < scope->_sorts._length; ++i) {
    char* criterion = (char*) TRI_AtVectorPointer(&scope->_sorts, i);

    TRI_Free(TRI_UNKNOWN_MEM_ZONE, criterion);
  }

  TRI_DestroyVectorPointer(&scope->_sorts);

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, scope);
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
/// @brief create a hint for an AQL for
////////////////////////////////////////////////////////////////////////////////

TRI_aql_for_hint_t* TRI_CreateForHintScopeAql (TRI_aql_context_t* const context) {
  TRI_aql_for_hint_t* hint;

  assert(context);

  hint = (TRI_aql_for_hint_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_aql_for_hint_t), false);
  if (hint == NULL) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return NULL;
  }

  // initialise
  hint->_limit._offset = 0;
  hint->_limit._limit  = 0;
  hint->_limit._status = TRI_AQL_LIMIT_UNDEFINED;
  hint->_isIncremental = false;

  return hint;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free a hint for an AQL for
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeForHintScopeAql (TRI_aql_for_hint_t* const hint) {
  assert(hint);

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, hint);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the name of a scope type
////////////////////////////////////////////////////////////////////////////////

const char* TRI_TypeNameScopeAql (const TRI_aql_scope_e type) {
  switch (type) {
    case TRI_AQL_SCOPE_FOR:
      return "for";
    case TRI_AQL_SCOPE_FOR_NESTED:
      return "for nested";
    case TRI_AQL_SCOPE_SUBQUERY:
      return "subquery";
    case TRI_AQL_SCOPE_EXPAND:
      return "expand";
    case TRI_AQL_SCOPE_MAIN:
      return "main";
    case TRI_AQL_SCOPE_FUNCTION:
      return "function";
  }

  return "unknown";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief init scopes
////////////////////////////////////////////////////////////////////////////////

bool TRI_InitScopesAql (TRI_aql_context_t* const context) {
  assert(context);

  TRI_InitVectorPointer(&context->_memory._scopes, TRI_UNKNOWN_MEM_ZONE);
  TRI_InitVectorPointer(&context->_currentScopes, TRI_UNKNOWN_MEM_ZONE);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free all scopes
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeScopesAql (TRI_aql_context_t* const context) {
  size_t i, n;

  assert(context);

  n = context->_memory._scopes._length;
  for (i = 0; i < n; ++i) {
    TRI_aql_scope_t* scope = (TRI_aql_scope_t*) TRI_AtVectorPointer(&context->_memory._scopes, i);

    FreeScope(scope);
  }

  TRI_DestroyVectorPointer(&context->_memory._scopes);
  TRI_DestroyVectorPointer(&context->_currentScopes);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a new scope
////////////////////////////////////////////////////////////////////////////////

bool TRI_StartScopeAql (TRI_aql_context_t* const context, const TRI_aql_scope_e type) {
  TRI_aql_scope_t* scope;
  TRI_aql_node_t* node;

  assert(context);

  scope = CreateScope(context, type);

  if (scope == NULL) {
    return false;
  }

  // set real scope level
  scope->_level = (size_t) context->_memory._scopes._length;

  LOG_TRACE("starting scope of type %s", TRI_TypeNameScopeAql(scope->_type));
  TRI_PushBackVectorPointer(&context->_memory._scopes, (void*) scope);
  TRI_PushBackVectorPointer(&context->_currentScopes, (void*) scope);

  node = TRI_CreateNodeScopeStartAql(context, scope);
  if (node == NULL) {
    return false;
  }

  if (! TRI_AppendStatementListAql(context->_statements, node)) {
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief end the current scope, only 1
////////////////////////////////////////////////////////////////////////////////

bool TRI_EndScopeAql (TRI_aql_context_t* const context) {
  TRI_aql_scope_t* scope;
  TRI_aql_node_t* node;
  size_t n;

  assert(context);

  n = context->_currentScopes._length;
  assert(n > 0);

  scope = TRI_RemoveVectorPointer(&context->_currentScopes, --n);
  LOG_TRACE("closing scope of type %s", TRI_TypeNameScopeAql(scope->_type));

  node = TRI_CreateNodeScopeEndAql(context, scope);
  if (node == NULL) {
    return false;
  }

  if (! TRI_AppendStatementListAql(context->_statements, node)) {
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief end the current scopes, 0 .. n
////////////////////////////////////////////////////////////////////////////////

bool TRI_EndScopeByReturnAql (TRI_aql_context_t* const context) {
  TRI_aql_scope_e type;
  TRI_aql_node_t* node;
  size_t n;

  assert(context);
  type = CurrentType(context);

  if (type == TRI_AQL_SCOPE_MAIN || type == TRI_AQL_SCOPE_SUBQUERY) {
    return true;
  }

  n = context->_currentScopes._length;
  assert(n > 0);

  while (n > 0) {
    TRI_aql_scope_t* scope;

    scope = (TRI_aql_scope_t*) TRI_RemoveVectorPointer(&context->_currentScopes, --n);
    assert(scope);

    LOG_TRACE("closing scope of type %s", TRI_TypeNameScopeAql(scope->_type));

    node = TRI_CreateNodeScopeEndAql(context, scope);
    if (node == NULL) {
      return false;
    }

    if (! TRI_AppendStatementListAql(context->_statements, node)) {
      return false;
    }

    if (scope->_type != TRI_AQL_SCOPE_FOR_NESTED) {
      // removed enough scopes
      break;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a variable is defined in the current scope or above
////////////////////////////////////////////////////////////////////////////////

bool TRI_VariableExistsScopeAql (TRI_aql_context_t* const context,
                                 const char* const name) {
  size_t n;

  if (name == NULL) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return false;
  }

  n = context->_currentScopes._length;
  assert(n > 0);

  while (n > 0) {
    TRI_aql_scope_t* scope = (TRI_aql_scope_t*) TRI_AtVectorPointer(&context->_currentScopes, --n);
    assert(scope);

    if (TRI_LookupByKeyAssociativePointer(&scope->_variables, (void*) name)) {
      // duplicate variable
      return true;
    }

    if (n == 0) {
      // reached the outermost scope
      break;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief push a variable into the current scope's symbol table
////////////////////////////////////////////////////////////////////////////////

bool TRI_AddVariableScopeAql (TRI_aql_context_t* const context,
                              const char* name,
                              TRI_aql_node_t* const definingNode) {
  TRI_aql_variable_t* variable;
  TRI_aql_scope_t* scope;
  void* result;

  assert(context);
  assert(name);

  if (TRI_VariableExistsScopeAql(context, name)) {
    return false;
  }

  variable = TRI_CreateVariableAql(name, definingNode);
  if (variable == NULL) {
    return false;
  }

  scope = CurrentScope(context);

  result = TRI_InsertKeyAssociativePointer(&scope->_variables, variable->_name, (void*) variable, false);
  assert(result == NULL);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

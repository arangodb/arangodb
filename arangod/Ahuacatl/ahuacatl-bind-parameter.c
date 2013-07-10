////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, bind parameters
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

#include "Ahuacatl/ahuacatl-bind-parameter.h"

#include "BasicsC/json.h"

#include "Ahuacatl/ahuacatl-context.h"
#include "Ahuacatl/ahuacatl-conversions.h"
#include "Ahuacatl/ahuacatl-statement-walker.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/// @brief check if a node is a bind parameter and convert it into a value node
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* ModifyNode (TRI_aql_statement_walker_t* const walker,
                                   TRI_aql_node_t* node) {
  TRI_aql_bind_parameter_t* bind;
  TRI_associative_pointer_t* bindValues;
  TRI_aql_context_t* context;
  char* name;

  if (node == NULL || 
      node->_type != TRI_AQL_NODE_PARAMETER) {
    return node;
  }

  // we found a parameter node
  context = (TRI_aql_context_t*) walker->_data;
  assert(context);

  bindValues = (TRI_associative_pointer_t*) &context->_parameters._values;
  assert(bindValues);

  name = TRI_AQL_NODE_STRING(node);
  assert(name);

  bind = (TRI_aql_bind_parameter_t*) TRI_LookupByKeyAssociativePointer(bindValues, name);

  if (bind) {
    if (*name == '@') {
      // a collection name bind parameter
      if (TRI_IsStringJson(bind->_value)) {
        char* collectionName = TRI_RegisterStringAql(context,
                                                     bind->_value->_value._string.data,
                                                     bind->_value->_value._string.length - 1,
                                                     false);

        node = TRI_CreateNodeCollectionAql(context, collectionName);
      }
      else {
        TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, name);
        node = NULL;
      }
    }
    else {
      node = TRI_JsonNodeAql(context, bind->_value);
    }

    if (node == NULL) {
      TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_BIND_PARAMETERS_INVALID, NULL);
    }
  }

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a bind parameter
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_bind_parameter_t* CreateParameter (const char* const name,
                                                  const size_t nameLength,
                                                  const TRI_json_t* const value) {
  TRI_aql_bind_parameter_t* parameter;

  assert(name);
  assert(value);

  parameter = (TRI_aql_bind_parameter_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_aql_bind_parameter_t), false);

  if (parameter == NULL) {
    return NULL;
  }

  parameter->_name = TRI_DuplicateString2Z(TRI_UNKNOWN_MEM_ZONE, name, nameLength);

  if (parameter->_name == NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, parameter);

    return NULL;
  }

  parameter->_value = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, (TRI_json_t*) value);

  if (parameter->_value == NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, parameter->_name);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, parameter);

    return NULL;
  }

  return parameter;
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
/// @brief hash bind parameter
////////////////////////////////////////////////////////////////////////////////

uint64_t TRI_HashBindParameterAql (TRI_associative_pointer_t* array,
                                   void const* element) {
  TRI_aql_bind_parameter_t* parameter = (TRI_aql_bind_parameter_t*) element;

  return TRI_FnvHashString(parameter->_name);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief comparison function used to determine bind parameter equality
////////////////////////////////////////////////////////////////////////////////

bool TRI_EqualBindParameterAql (TRI_associative_pointer_t* array,
                                void const* key,
                                void const* element) {
  TRI_aql_bind_parameter_t* parameter = (TRI_aql_bind_parameter_t*) element;

  return TRI_EqualString(key, parameter->_name);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free bind parameters
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeBindParametersAql (TRI_aql_context_t* const context) {
  size_t i;
  size_t n;

  // iterate thru all parameters allocated
  n = context->_parameters._values._nrAlloc;
  for (i = 0; i < n; ++i) {
    TRI_aql_bind_parameter_t* parameter;

    parameter = (TRI_aql_bind_parameter_t*) context->_parameters._values._table[i];

    if (!parameter) {
      continue;
    }

    assert(parameter->_name);
    assert(parameter->_value);

    TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, parameter->_name);
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, parameter->_value);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, parameter);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add bind parameters
////////////////////////////////////////////////////////////////////////////////

bool TRI_AddParameterValuesAql (TRI_aql_context_t* const context,
                                const TRI_json_t* const parameters) {
  size_t i;
  size_t n;

  assert(context);

  if (parameters == NULL) {
    // no bind parameters, direclty return
    return true;
  }

  if (parameters->_type != TRI_JSON_ARRAY) {
    // parameters must be a list
    TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_BIND_PARAMETERS_INVALID, NULL);
    return false;
  }

  n = parameters->_value._objects._length;
  if (n == 0) {
    // empty list, this is ok
    return true;
  }

  for (i = 0; i < n; i += 2) {
    TRI_json_t* name = TRI_AtVector(&parameters->_value._objects, i);
    TRI_json_t* value = TRI_AtVector(&parameters->_value._objects, i + 1);
    TRI_aql_bind_parameter_t* parameter;

    assert(TRI_IsStringJson(name));
    assert(value);

    parameter = CreateParameter(name->_value._string.data, 
                                name->_value._string.length - 1,
                                value);

    if (parameter == NULL) {
      TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);

      return false;
    }

    TRI_InsertKeyAssociativePointer(&context->_parameters._values, parameter->_name, parameter, false);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief validate bind parameters passed
////////////////////////////////////////////////////////////////////////////////

bool TRI_ValidateBindParametersAql (TRI_aql_context_t* const context) {
  size_t i;
  size_t n;

  // iterate thru all parameter names used in the query
  n = context->_parameters._names._nrAlloc;
  for (i = 0; i < n; ++i) {
    char* name = (char*) context->_parameters._names._table[i];

    if (!name) {
      continue;
    }

    if (!TRI_LookupByKeyAssociativePointer(&context->_parameters._values, name)) {
      TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_BIND_PARAMETER_MISSING, name);
      return false;
    }
  }

  // iterate thru all parameters that we have values for
  n = context->_parameters._values._nrAlloc;
  for (i = 0; i < n; ++i) {
    TRI_aql_bind_parameter_t* parameter;

    parameter = (TRI_aql_bind_parameter_t*) context->_parameters._values._table[i];

    if (!parameter) {
      continue;
    }

    assert(parameter->_name);

    if (!TRI_LookupByKeyAssociativePointer(&context->_parameters._names, parameter->_name)) {
      TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_BIND_PARAMETER_UNDECLARED, parameter->_name);
      return false;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inject values of bind parameters into query
////////////////////////////////////////////////////////////////////////////////

bool TRI_InjectBindParametersAql (TRI_aql_context_t* const context) {
  TRI_aql_statement_walker_t* walker;

  assert(context);

  if (TRI_GetLengthAssociativePointer(&context->_parameters._names) == 0) {
    // no bind parameters used in query, instantly return
    return true;
  }

  walker = TRI_CreateStatementWalkerAql(context, true, &ModifyNode, NULL, NULL);
  if (walker == NULL) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);

    return false;
  }


  TRI_WalkStatementsAql(walker, context->_statements);

  TRI_FreeStatementWalkerAql(walker);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, bind parameters
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

#include "Ahuacatl/ahuacatl-bind-parameter.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief create a value node from a bind parameter
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* CreateNodeFromJson (TRI_aql_parse_context_t* const context,
                                           TRI_json_t* json) {
  TRI_aql_node_t* node = NULL;

  switch (json->_type) {
    case TRI_JSON_UNUSED:
      break;

    case TRI_JSON_NULL:
      node = TRI_CreateNodeValueNullAql(context);
      break;
    
    case TRI_JSON_BOOLEAN:
      node = TRI_CreateNodeValueBoolAql(context, json->_value._boolean); 
      break;
    
    case TRI_JSON_NUMBER:
      node = TRI_CreateNodeValueDoubleAql(context, json->_value._number); 
      break;
    
    case TRI_JSON_STRING:
      node = TRI_CreateNodeValueStringAql(context, json->_value._string.data);
      break;
    
    case TRI_JSON_LIST: {
      size_t i;
      size_t n;

      node = TRI_CreateNodeListAql(context);
      n = json->_value._objects._length;

      for (i = 0; i < n; ++i) {
        TRI_json_t* subJson;
        TRI_aql_node_t* subNode;

        subJson = (TRI_json_t*) TRI_AtVector(&json->_value._objects, i);
        assert(subJson);

        subNode = CreateNodeFromJson(context, subJson);
        if (subNode) {
          TRI_PushBackVectorPointer(&node->_subNodes, (void*) subNode); 
        }
        else {
          TRI_SetErrorAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
          return NULL;
        }
      }
      break;
    }
    case TRI_JSON_ARRAY: {
      size_t i;
      size_t n;

      node = TRI_CreateNodeArrayAql(context);
      n = json->_value._objects._length;

      for (i = 0; i < n; i += 2) {
        TRI_json_t* nameJson;
        TRI_json_t* valueJson;
        TRI_aql_node_t* subNode;
        TRI_aql_node_t* valueNode;
        char* name;

        // json_t containing the array element name
        nameJson = (TRI_json_t*) TRI_AtVector(&json->_value._objects, i);
        assert(nameJson);

        name = nameJson->_value._string.data;
        assert(name);

        // json_t containing the array element value
        valueJson = (TRI_json_t*) TRI_AtVector(&json->_value._objects, i + 1);
        assert(valueJson);

        valueNode = CreateNodeFromJson(context, valueJson);
        if (!valueNode) {
          TRI_SetErrorAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
          return NULL;
        }

        subNode = TRI_CreateNodeArrayElementAql(context, name, valueNode);
        if (subNode) {
          TRI_PushBackVectorPointer(&node->_subNodes, (void*) subNode); 
        }
        else {
          TRI_SetErrorAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
          return NULL;
        }
      }
      break;
    }
  }

  if (!node) {
    TRI_SetErrorAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
  }

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a node is a bind parameter and convert it into a value node 
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* ModifyNode (void* data,
                                   TRI_aql_node_t* node) {
  TRI_aql_bind_parameter_t* bind;
  TRI_associative_pointer_t* bindValues;
  TRI_aql_parse_context_t* context;
  char* name;

  if (!node || node->_type != AQL_NODE_PARAMETER) {
    return node;
  }
  
  // we found a parameter node
  context = (TRI_aql_parse_context_t*) data;
  assert(context);

  bindValues = (TRI_associative_pointer_t*) &context->_parameterValues;
  assert(bindValues);
  
  name = TRI_AQL_NODE_STRING(node);
  assert(name);
     
  bind = (TRI_aql_bind_parameter_t*) TRI_LookupByKeyAssociativePointer(bindValues, name);
  if (bind) {
    node = CreateNodeFromJson(context, bind->_value);
  }
  
  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a bind parameter
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_bind_parameter_t* CreateParameter (const char* const name,
                                                  const TRI_json_t* const value) {
  TRI_aql_bind_parameter_t* parameter;

  assert(name);
  assert(value);

  parameter = (TRI_aql_bind_parameter_t*) TRI_Allocate(sizeof(TRI_aql_bind_parameter_t));
  if (!parameter) {
    return NULL;
  }

  parameter->_name = TRI_DuplicateString(name);
  if (!parameter->_name) {
    TRI_Free(parameter);
    return NULL;
  }

  parameter->_value = TRI_CopyJson((TRI_json_t*) value);
  if (!parameter->_value) {
    TRI_Free(parameter->_name);
    TRI_Free(parameter);
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

void TRI_FreeBindParametersAql (TRI_aql_parse_context_t* const context) {
  size_t i;
  size_t n;

  // iterate thru all parameters allocated
  n = context->_parameterValues._nrAlloc;
  for (i = 0; i < n; ++i) {
    TRI_aql_bind_parameter_t* parameter;

    parameter = (TRI_aql_bind_parameter_t*) context->_parameterValues._table[i];

    if (!parameter) {
      continue;
    }

    assert(parameter->_name);
    assert(parameter->_value);

    TRI_FreeString(parameter->_name);
    TRI_FreeJson(parameter->_value);
    TRI_Free(parameter);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add bind parameters
////////////////////////////////////////////////////////////////////////////////

bool TRI_AddParameterValuesAql (TRI_aql_parse_context_t* const context,
                                const TRI_json_t* const parameters) {
  size_t i;
  size_t n;

  assert(context);

  if (parameters == NULL) {
    // no bind parameters
    return true;
  }

  if (parameters->_type != TRI_JSON_ARRAY) {
    // parameters must be a list
    TRI_SetErrorAql(context, TRI_ERROR_QUERY_BIND_PARAMETERS_INVALID, NULL);
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

    assert(name);
    assert(name->_type == TRI_JSON_STRING); 
    assert(value);

    parameter = CreateParameter(name->_value._string.data, value);
    if (!parameter) {
      TRI_SetErrorAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
      return false;
    }
 
    TRI_InsertKeyAssociativePointer(&context->_parameterValues, parameter->_name, parameter, false);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief validate bind parameters passed
////////////////////////////////////////////////////////////////////////////////

bool TRI_ValidateBindParametersAql (TRI_aql_parse_context_t* const context) {
  size_t i;
  size_t n;

  // iterate thru all parameter names used in the query
  n = context->_parameterNames._nrAlloc;
  for (i = 0; i < n; ++i) {
    char* name = (char*) context->_parameterNames._table[i];

    if (!name) {
      continue;
    }

    if (!TRI_LookupByKeyAssociativePointer(&context->_parameterValues, name)) {
      TRI_SetErrorAql(context, TRI_ERROR_QUERY_BIND_PARAMETER_MISSING, name);
      return false;
    }
  }

  // iterate thru all parameters that we have values for
  n = context->_parameterValues._nrAlloc;
  for (i = 0; i < n; ++i) {
    TRI_aql_bind_parameter_t* parameter;

    parameter = (TRI_aql_bind_parameter_t*) context->_parameterValues._table[i];

    if (!parameter) {
      continue;
    }

    assert(parameter->_name);

    if (!TRI_LookupByKeyAssociativePointer(&context->_parameterNames, parameter->_name)) {
      TRI_SetErrorAql(context, TRI_ERROR_QUERY_BIND_PARAMETER_UNDECLARED, parameter->_name);
      return false;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inject values of bind parameters into query
////////////////////////////////////////////////////////////////////////////////

bool TRI_InjectBindParametersAql (TRI_aql_parse_context_t* const context, 
                                  TRI_aql_node_t* node) {
  TRI_aql_modify_tree_walker_t* walker;

  assert(context);
  assert(context->_first);

  if (TRI_GetLengthAssociativePointer(&context->_parameterNames) == 0) {
    // no bind parameters used in query, instantly return
    return true;
  }
  
  walker = TRI_CreateModifyTreeWalkerAql(context, &ModifyNode);
  if (!walker) {
    TRI_SetErrorAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return false;
  }

  context->_first = TRI_ModifyWalkTreeAql(walker, node);

  TRI_FreeModifyTreeWalkerAql(walker);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

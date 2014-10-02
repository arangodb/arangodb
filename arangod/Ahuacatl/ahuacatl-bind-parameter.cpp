////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, bind parameters
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Ahuacatl/ahuacatl-bind-parameter.h"

#include "Basics/json.h"

#include "Ahuacatl/ahuacatl-context.h"
#include "Ahuacatl/ahuacatl-conversions.h"
#include "Ahuacatl/ahuacatl-statement-walker.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a node is a bound attribute access and convert it into a
/// "normal" attribute access
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* FixAttributeAccess (TRI_aql_statement_walker_t* const walker,
                                           TRI_aql_node_t* node) {
  TRI_aql_node_t* valueNode;
  char* stringValue;

  if (node == nullptr ||
      node->_type != TRI_AQL_NODE_BOUND_ATTRIBUTE_ACCESS) {
    return node;
  }

  // we found a bound attribute access
  TRI_aql_context_t* context = static_cast<TRI_aql_context_t*>(walker->_data);
  TRI_ASSERT(context);

  TRI_ASSERT(node->_members._length == 2);
  valueNode = TRI_AQL_NODE_MEMBER(node, 1);

  if (valueNode == nullptr) {
    TRI_SetErrorContextAql(__FILE__, __LINE__, context, TRI_ERROR_OUT_OF_MEMORY, nullptr);
    return node;
  }

  if (valueNode->_type != TRI_AQL_NODE_VALUE || valueNode->_value._type != TRI_AQL_TYPE_STRING) {
    TRI_SetErrorContextAql(__FILE__, __LINE__, context, TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, "(unknown)");
    return node;
  }

  stringValue = TRI_AQL_NODE_STRING(valueNode);

  if (stringValue == nullptr || strlen(stringValue) == 0) {
    TRI_SetErrorContextAql(__FILE__, __LINE__, context, TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, "(unknown)");
    return node;
  }

  // remove second node
  TRI_RemoveVectorPointer(&node->_members, 1);

  // set attribute name
  TRI_AQL_NODE_STRING(node) = stringValue;

  // convert node type
  node->_type = TRI_AQL_NODE_ATTRIBUTE_ACCESS;

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a node is a bind parameter and convert it into a value node
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* InjectParameter (TRI_aql_statement_walker_t* const walker,
                                        TRI_aql_node_t* node) {
  TRI_aql_bind_parameter_t* bind;
  TRI_associative_pointer_t* bindValues;
  char* name;

  if (node == nullptr ||
      node->_type != TRI_AQL_NODE_PARAMETER) {
    return node;
  }

  // we found a parameter node
  TRI_aql_context_t* context = static_cast<TRI_aql_context_t*>(walker->_data);
  TRI_ASSERT(context != nullptr);

  bindValues = (TRI_associative_pointer_t*) &context->_parameters._values;
  TRI_ASSERT(bindValues != nullptr);

  name = TRI_AQL_NODE_STRING(node);
  TRI_ASSERT(name);

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
        TRI_SetErrorContextAql(__FILE__, __LINE__, context, TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, name);
        node = nullptr;
      }
    }
    else {
      node = TRI_JsonNodeAql(context, bind->_value);
    }

    if (node == nullptr) {
      TRI_SetErrorContextAql(__FILE__, __LINE__, context, TRI_ERROR_QUERY_BIND_PARAMETERS_INVALID, nullptr);
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
  TRI_ASSERT(name);
  TRI_ASSERT(value);

  TRI_aql_bind_parameter_t* parameter = (TRI_aql_bind_parameter_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_aql_bind_parameter_t), false);

  if (parameter == nullptr) {
    return nullptr;
  }

  parameter->_name = TRI_DuplicateString2Z(TRI_UNKNOWN_MEM_ZONE, name, nameLength);

  if (parameter->_name == nullptr) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, parameter);

    return nullptr;
  }

  parameter->_value = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, (TRI_json_t*) value);

  if (parameter->_value == nullptr) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, parameter->_name);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, parameter);

    return nullptr;
  }

  return parameter;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

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

  return TRI_EqualString(static_cast<char const*>(key), parameter->_name);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free bind parameters
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeBindParametersAql (TRI_aql_context_t* const context) {
  // iterate thru all parameters allocated
  size_t const n = context->_parameters._values._nrAlloc;

  for (size_t i = 0; i < n; ++i) {
    TRI_aql_bind_parameter_t* parameter = static_cast<TRI_aql_bind_parameter_t*>(context->_parameters._values._table[i]);

    if (parameter == nullptr) {
      continue;
    }

    TRI_ASSERT(parameter->_name);
    TRI_ASSERT(parameter->_value);

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
  TRI_ASSERT(context);

  if (parameters == nullptr) {
    // no bind parameters, direclty return
    return true;
  }

  if (parameters->_type != TRI_JSON_ARRAY) {
    // parameters must be a list
    TRI_SetErrorContextAql(__FILE__, __LINE__, context, TRI_ERROR_QUERY_BIND_PARAMETERS_INVALID, nullptr);
    return false;
  }

  size_t const n = parameters->_value._objects._length;

  if (n == 0) {
    // empty list, this is ok
    return true;
  }

  for (size_t i = 0; i < n; i += 2) {
    TRI_json_t* name = static_cast<TRI_json_t*>(TRI_AtVector(&parameters->_value._objects, i));
    TRI_json_t* value = static_cast<TRI_json_t*>(TRI_AtVector(&parameters->_value._objects, i + 1));

    TRI_ASSERT(TRI_IsStringJson(name));
    TRI_ASSERT(value);

    TRI_aql_bind_parameter_t* parameter = CreateParameter(name->_value._string.data,
                                                          name->_value._string.length - 1,
                                                          value);

    if (parameter == nullptr) {
      TRI_SetErrorContextAql(__FILE__, __LINE__, context, TRI_ERROR_OUT_OF_MEMORY, nullptr);
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
  // iterate thru all parameter names used in the query
  size_t n = context->_parameters._names._nrAlloc;

  for (size_t i = 0; i < n; ++i) {
    char* name = (char*) context->_parameters._names._table[i];

    if (name == nullptr) {
      continue;
    }

    if (! TRI_LookupByKeyAssociativePointer(&context->_parameters._values, name)) {
      TRI_SetErrorContextAql(__FILE__, __LINE__, context, TRI_ERROR_QUERY_BIND_PARAMETER_MISSING, name);
      return false;
    }
  }

  // iterate thru all parameters that we have values for
  n = context->_parameters._values._nrAlloc;
  for (size_t i = 0; i < n; ++i) {
    TRI_aql_bind_parameter_t* parameter = static_cast<TRI_aql_bind_parameter_t*>(context->_parameters._values._table[i]);

    if (parameter == nullptr) {
      continue;
    }

    TRI_ASSERT(parameter->_name);

    if (! TRI_LookupByKeyAssociativePointer(&context->_parameters._names, parameter->_name)) {
      TRI_SetErrorContextAql(__FILE__, __LINE__, context, TRI_ERROR_QUERY_BIND_PARAMETER_UNDECLARED, parameter->_name);
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

  TRI_ASSERT(context);

  if (TRI_GetLengthAssociativePointer(&context->_parameters._names) == 0) {
    // no bind parameters used in query, instantly return
    return true;
  }

  if (context->_writeCollection != nullptr &&
      context->_writeCollection[0] == '@') {
    // replace bind parameter in write-collection name
    void* found = TRI_LookupByKeyAssociativePointer(&context->_parameters._values, context->_writeCollection);

    if (found == nullptr) {
      TRI_SetErrorContextAql(__FILE__, __LINE__, context, TRI_ERROR_QUERY_BIND_PARAMETER_MISSING, nullptr);
      return false;
    }

    TRI_json_t* json = static_cast<TRI_aql_bind_parameter_t*>(found)->_value;
    if (! TRI_IsStringJson(json)) {
      TRI_SetErrorContextAql(__FILE__, __LINE__, context, TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, nullptr);
      return false;
    }

    context->_writeCollection = json->_value._string.data;
  }

  walker = TRI_CreateStatementWalkerAql(context, true, &InjectParameter, nullptr, nullptr);

  if (walker == nullptr) {
    TRI_SetErrorContextAql(__FILE__, __LINE__, context, TRI_ERROR_OUT_OF_MEMORY, nullptr);
    return false;
  }

  TRI_WalkStatementsAql(walker, context->_statements);
  TRI_FreeStatementWalkerAql(walker);


  walker = TRI_CreateStatementWalkerAql(context, true, &FixAttributeAccess, nullptr, nullptr);

  if (walker == nullptr) {
    TRI_SetErrorContextAql(__FILE__, __LINE__, context, TRI_ERROR_OUT_OF_MEMORY, nullptr);

    return false;
  }

  TRI_WalkStatementsAql(walker, context->_statements);
  TRI_FreeStatementWalkerAql(walker);

  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

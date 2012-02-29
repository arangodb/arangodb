////////////////////////////////////////////////////////////////////////////////
/// @brief Basic query data structures
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

#include "VocBase/query-base.h"
#include "VocBase/query-parse.h"

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Hash function used to hash bind parameter names
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashBindParameterName (TRI_associative_pointer_t* array, 
                                       void const* key) {
  return TRI_FnvHashString((char const*) key);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Hash function used to hash bind parameters
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashBindParameter (TRI_associative_pointer_t* array, 
                                   void const* element) {
  TRI_bind_parameter_t* parameter = (TRI_bind_parameter_t*) element;

  return TRI_FnvHashString(parameter->_name);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Comparison function used to determine bind parameter equality
////////////////////////////////////////////////////////////////////////////////

static bool EqualBindParameter (TRI_associative_pointer_t* array, 
                                void const* key, 
                                void const* element) {
  TRI_bind_parameter_t* parameter = (TRI_bind_parameter_t*) element;

  return TRI_EqualString(key, parameter->_name);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Free all bind parameters in associative array
////////////////////////////////////////////////////////////////////////////////

static void FreeBindParameters (TRI_associative_pointer_t* array) {
  TRI_bind_parameter_t* parameter;
  size_t i;

  for (i = 0; i < array->_nrAlloc; i++) {
    parameter = (TRI_bind_parameter_t*) array->_table[i];
    if (parameter) {
      TRI_FreeBindParameter(parameter);
    }
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   bind parameters
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Free a single bind parameter
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeBindParameter (TRI_bind_parameter_t* const parameter) {
  assert(parameter);

  if (parameter->_name) {
    TRI_Free(parameter->_name);
  }

  if (parameter->_data) {
    TRI_FreeJson(parameter->_data);
  }

  TRI_Free(parameter);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Create a bind parameter
////////////////////////////////////////////////////////////////////////////////

TRI_bind_parameter_t* TRI_CreateBindParameter (const char* name, 
                                               const TRI_json_t* data) {
  TRI_bind_parameter_t* parameter;

  assert(name);
  assert(data);

  parameter = (TRI_bind_parameter_t*) TRI_Allocate(sizeof(TRI_bind_parameter_t));
  if (!parameter) {
    return NULL;
  }

  parameter->_name = TRI_DuplicateString(name);
  if (!parameter->_name) {
    TRI_Free(parameter);
    return NULL;
  }

  parameter->_data = TRI_CopyJson((TRI_json_t*) data);
  if (!parameter->_data) {
    TRI_Free(parameter->_name);
    TRI_Free(parameter);
    return NULL;
  }

  return parameter;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    query template
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Add a bind parameter to a query template
////////////////////////////////////////////////////////////////////////////////

bool TRI_AddBindParameterQueryTemplate (TRI_query_template_t* const template_,
                                        const TRI_bind_parameter_t* const parameter) {
  assert(template_);
  
  if (TRI_LookupByKeyAssociativePointer(&template_->_bindParameters, 
                                        parameter->_name)) {
    return false;
  }

  TRI_InsertKeyAssociativePointer(&template_->_bindParameters, 
                                  parameter->_name,
                                  (void*) parameter, 
                                  true);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Create a query template
////////////////////////////////////////////////////////////////////////////////

TRI_query_template_t* TRI_CreateQueryTemplate (const char* query, 
                                               const TRI_vocbase_t* const vocbase) {
  TRI_query_template_t* template_;
  
  assert(query);
  assert(vocbase);

  template_ = (TRI_query_template_t*) TRI_Allocate(sizeof(TRI_query_template_t));
  if (!template_) {
    return NULL;
  }
  
  template_->_queryString = TRI_DuplicateString(query);
  if (!template_->_queryString) {
    TRI_Free(template_);
    return NULL;
  }
  
  template_->_query = (QL_ast_query_t*) TRI_Allocate(sizeof(QL_ast_query_t));
  if (!template_->_query) {
    TRI_Free(template_->_queryString);
    TRI_Free(template_);
    return NULL;
  }
  QLAstQueryInit(template_->_vocbase, template_->_query);

  template_->_vocbase = (TRI_vocbase_t*) vocbase;
  TRI_InitQueryError(&template_->_error);

  TRI_InitAssociativePointer(&template_->_bindParameters,
                             HashBindParameterName,
                             HashBindParameter,
                             EqualBindParameter,
                             0);
  
  // init vectors needed for book-keeping memory
  TRI_InitVectorPointer(&template_->_memory._nodes);
  TRI_InitVectorPointer(&template_->_memory._strings);
  TRI_InitVectorPointer(&template_->_memory._listHeads);
  TRI_InitVectorPointer(&template_->_memory._listTails);

  return template_;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Free a query template
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeQueryTemplate (TRI_query_template_t* const template_) {
  void* nodePtr;
  char* stringPtr;
  size_t i;

  assert(template_);
  assert(template_->_queryString);
  assert(template_->_query);
    
  QLAstQueryFree(template_->_query);
  TRI_Free(template_->_query);
  
  // nodes
  i = template_->_memory._nodes._length;
  // free all nodes in vector, starting at the end (prevents copying the remaining elements in vector)
  while (i > 0) {
    i--;
    nodePtr = TRI_RemoveVectorPointer(&template_->_memory._nodes, i);
    TRI_ParseQueryFreeNode(nodePtr);
    if (i == 0) {
      break;
    }
  }

  // strings
  i = template_->_memory._strings._length;
  // free all strings in vector, starting at the end (prevents copying the remaining elements in vector)
  while (i > 0) {
    i--;
    stringPtr = TRI_RemoveVectorPointer(&template_->_memory._strings, i);
    TRI_ParseQueryFreeString(stringPtr);
    if (i == 0) {
      break;
    }
  }
  
  // list elements in _listHeads and _listTails must not be freed separately as they are AstNodes handled
  // by _nodes already
  
  // free vectors themselves
  TRI_DestroyVectorPointer(&template_->_memory._strings);
  TRI_DestroyVectorPointer(&template_->_memory._nodes);
  TRI_DestroyVectorPointer(&template_->_memory._listHeads);
  TRI_DestroyVectorPointer(&template_->_memory._listTails);

  TRI_Free(template_->_queryString);
 
  TRI_FreeQueryError(&template_->_error); 
  
  FreeBindParameters(&template_->_bindParameters);
  TRI_DestroyAssociativePointer(&template_->_bindParameters);

  TRI_Free(template_);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    query instance
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Set the value of a bind parameter
////////////////////////////////////////////////////////////////////////////////

bool TRI_AddBindParameterQueryInstance (TRI_query_instance_t* const instance,
                                        const TRI_bind_parameter_t* const parameter) {
  assert(instance);
  assert(parameter);
  assert(parameter->_name);
  assert(parameter->_data);

  // check if template has bind parameter defined
  if (!TRI_LookupByKeyAssociativePointer(&instance->_template->_bindParameters, 
                                         parameter->_name)) {
    // invalid bind parameter
    TRI_RegisterErrorQueryInstance(instance, 
                                   TRI_ERROR_QUERY_BIND_PARAMETER_UNDECLARED, 
                                   parameter->_name);
    return false;
  }
  
  // check if bind parameter is already defined for instance
  if (!TRI_LookupByKeyAssociativePointer(&instance->_bindParameters, 
                                         parameter->_name)) {
    // bind parameter defined => duplicate definition
    TRI_RegisterErrorQueryInstance(instance, 
                                   TRI_ERROR_QUERY_BIND_PARAMETER_REDECLARED, 
                                   parameter->_name);
    return false;
  }

  TRI_InsertKeyAssociativePointer(&instance->_bindParameters, 
                                  parameter->_name,
                                  (void*) parameter, 
                                  true);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Free a query instance
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeQueryInstance (TRI_query_instance_t* const instance) {
  assert(instance);
  assert(instance->_template);

  TRI_FreeQueryError(&instance->_error); 

  FreeBindParameters(&instance->_bindParameters);
  TRI_DestroyAssociativePointer(&instance->_bindParameters);

  TRI_Free(instance);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Create a query instance
////////////////////////////////////////////////////////////////////////////////

TRI_query_instance_t* TRI_CreateQueryInstance (const TRI_query_template_t* const template_) {
  TRI_query_instance_t* instance;

  assert(template_);

  instance = (TRI_query_instance_t*) TRI_Allocate(sizeof(TRI_query_instance_t));
  if (!instance) {
    return NULL;
  }

  instance->_template      = (TRI_query_template_t*) template_;

  // init values
  instance->_wasKilled     = false;
  instance->_doAbort       = false;
  
  TRI_InitQueryError(&instance->_error);

  TRI_InitAssociativePointer(&instance->_bindParameters,
                             HashBindParameterName,
                             HashBindParameter,
                             EqualBindParameter,
                             0);

  return instance;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Register an error during query execution
////////////////////////////////////////////////////////////////////////////////

bool TRI_RegisterErrorQueryInstance (TRI_query_instance_t* const instance, 
                                     const int code,
                                     const char* data) {

  assert(instance);
  assert(code > 0);

  TRI_SetQueryError(&instance->_error, code, data);

  // set abort flag
  instance->_doAbort = true;

  return false;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                            errors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Register an error
////////////////////////////////////////////////////////////////////////////////

void TRI_SetQueryError (TRI_query_error_t* const error, 
                        const int code, 
                        const char* data) {
  assert(code > 0);

  if (TRI_GetCodeQueryError(error) == 0) {
    // do not overwrite previous error
    TRI_set_errno(code);
    error->_code = code;
    error->_message = (char*) TRI_last_error();
    if (data) {
      error->_data = TRI_DuplicateString(data);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Get the error code registered last
////////////////////////////////////////////////////////////////////////////////

int TRI_GetCodeQueryError (const TRI_query_error_t* const error) {
  assert(error);
  
  return error->_code;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Get the error string registered last
////////////////////////////////////////////////////////////////////////////////

char* TRI_GetStringQueryError (const TRI_query_error_t* const error) {
  char* message = NULL;
  char buffer[1024];
  int code;

  assert(error);
  code = TRI_GetCodeQueryError(error);
  if (!code) {
    return NULL;
  }

  message = error->_message;
  if (!message) {
    return NULL;
  }

  if (error->_data && (NULL != strstr(message, "%s"))) {
    snprintf(buffer, sizeof(buffer), message, error->_data);
    return TRI_DuplicateString((const char*) &buffer);
  }

  return TRI_DuplicateString(message);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize an error structure
////////////////////////////////////////////////////////////////////////////////
  
void TRI_InitQueryError (TRI_query_error_t* const error) {
  assert(error);

  error->_code = 0;
  error->_data = NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free an error structure
////////////////////////////////////////////////////////////////////////////////
  
void TRI_FreeQueryError (TRI_query_error_t* const error) {
  assert(error);
  
  if (error->_data) {
    TRI_Free(error->_data);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

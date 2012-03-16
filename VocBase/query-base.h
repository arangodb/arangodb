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

#ifndef TRIAGENS_DURHAM_VOC_BASE_QUERY_BASE_H
#define TRIAGENS_DURHAM_VOC_BASE_QUERY_BASE_H 1

#include <BasicsC/common.h>
#include <BasicsC/vector.h>
#include <BasicsC/associative.h>
#include <BasicsC/strings.h>
#include <BasicsC/hashes.h>
#include <BasicsC/json.h>

#include "VocBase/vocbase.h"
#include "VocBase/query-node.h"
#include "VocBase/query-error.h"

#include "QL/parser.h"
#include "QL/ast-query.h"

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                      query errors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief query error structure
///
/// This struct is used to hold information about errors that happen during
/// query execution. The data will be passed to the end user.
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_query_error_s {
  int    _code;
  char*  _message;
  char*  _data;
} 
TRI_query_error_t;

// -----------------------------------------------------------------------------
// --SECTION--                                                             locks
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Collection lock
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_query_instance_lock_s {
  char*                 _collectionName;
  TRI_doc_collection_t* _collection;
}
TRI_query_instance_lock_t;

// -----------------------------------------------------------------------------
// --SECTION--                                                   bind parameters
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Query bind parameter
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_bind_parameter_s {
  char*        _name;
  TRI_json_t*  _data;
} 
TRI_bind_parameter_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief Get the names of all bind parameters
////////////////////////////////////////////////////////////////////////////////

TRI_vector_string_t TRI_GetNamesBindParameter (TRI_associative_pointer_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief Free a bind parameter
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeBindParameter (TRI_bind_parameter_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief Create a bind parameter
////////////////////////////////////////////////////////////////////////////////

TRI_bind_parameter_t* TRI_CreateBindParameter (const char*, const TRI_json_t*);

// -----------------------------------------------------------------------------
// --SECTION--                                                    query template
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief lexer state
///
/// This struct contains the current lexer / scanner state. It contains 
/// positioning information (pointer into query string, remaining length)
/// because flex processes strings in chunks of 8k by default.
///
/// Each lexer instance will have its own struct instance to support reentrancy.
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_query_parser_s {
  void*  _scanner;
  char*  _buffer;
  int    _length;
}
TRI_query_parser_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief query template structure
///
/// A query template is a blueprint for a query execution. It is abstract in the
/// sense that it does not contain any bind parameter values and no result set.
/// A query instance (@ref TRI_query_instance_t) can be created from a query
/// template.
///
/// The template contains vectors that contain locations of allocated memory. 
/// This is especially important because in case of parse errors, bison / flex 
/// will not automatically free any allocated memory. 
/// This has to be done manually, and that is what the vectors are used for. 
/// There is a vector for AST nodes and a vector for strings
/// (used to keep track of string literals used in the query). There are also
/// two vectors to keep track of list elements (arrays / objects) in queries.
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_query_template_s {
  TRI_vocbase_t*             _vocbase;
  char*                      _queryString;
  QL_ast_query_t*            _query;
  TRI_query_parser_t*        _parser;
  TRI_mutex_t                _lock;
  bool                       _deleted;
  struct {
    TRI_vector_pointer_t     _nodes;     // memory locations of allocated AST nodes
    TRI_vector_pointer_t     _strings;   // memory locations of allocated strings
    TRI_vector_pointer_t     _listHeads; // start of lists
    TRI_vector_pointer_t     _listTails; // end of lists
  }                          _memory;
  TRI_query_error_t          _error;
  TRI_associative_pointer_t  _bindParameters;
}
TRI_query_template_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief Initialize the structs contained in a query template and perform
/// some basic optimizations and type detections
////////////////////////////////////////////////////////////////////////////////

bool TRI_InitQueryTemplate (TRI_query_template_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief Add a bind parameter to a query template
////////////////////////////////////////////////////////////////////////////////

bool TRI_AddBindParameterQueryTemplate (TRI_query_template_t* const, 
                                        const TRI_bind_parameter_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief Create a query template
////////////////////////////////////////////////////////////////////////////////

TRI_query_template_t* TRI_CreateQueryTemplate (const char*, 
                                               const TRI_vocbase_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief Free a query template
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeQueryTemplate (TRI_query_template_t*);

// -----------------------------------------------------------------------------
// --SECTION--                                                    query instance
// -----------------------------------------------------------------------------
  
////////////////////////////////////////////////////////////////////////////////
/// @brief query instance structure
///
/// A query instance is a concrete query that has specific bind parameter values
/// and a specific result set
/// This struct is used to hold information about errors that happen during
/// query execution. The data will be passed to the end user.
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_query_instance_s {
  TRI_query_template_t*      _template;
  bool                       _wasKilled;
  bool                       _doAbort;
  TRI_query_error_t          _error;
  TRI_associative_pointer_t  _bindParameters;
  QL_ast_query_t             _query;
  TRI_vector_pointer_t       _join;
  TRI_vector_pointer_t       _locks;
  struct {
    TRI_vector_pointer_t     _nodes;     // memory locations of allocated AST nodes
    TRI_vector_pointer_t     _strings;   // memory locations of allocated strings
  }                          _memory;
}
TRI_query_instance_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief Set the value of a bind parameter
////////////////////////////////////////////////////////////////////////////////

bool TRI_AddBindParameterQueryInstance (TRI_query_instance_t* const,
                                        const TRI_bind_parameter_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief Free a query instance
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeQueryInstance (TRI_query_instance_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief Create a query instance with bind parameters (may be empty)
////////////////////////////////////////////////////////////////////////////////

TRI_query_instance_t* TRI_CreateQueryInstance (const TRI_query_template_t* const,
                                               const TRI_json_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief Kill a query instance
///
/// This will set a killed flag and register an error. It will not free the
/// query instance. This will be done by the query executor
////////////////////////////////////////////////////////////////////////////////

void TRI_KillQueryInstance (TRI_query_instance_t* const instance);

////////////////////////////////////////////////////////////////////////////////
/// @brief Register an error during query execution
////////////////////////////////////////////////////////////////////////////////

bool TRI_RegisterErrorQueryInstance (TRI_query_instance_t* const,
                                     const int,
                                     const char*);  

////////////////////////////////////////////////////////////////////////////////
/// @brief Copy a part of the query's AST and insert bind parameter values on
/// the fly
////////////////////////////////////////////////////////////////////////////////

TRI_query_node_t* TRI_CopyQueryPartQueryInstance (TRI_query_instance_t* const,
                                                  const TRI_query_node_t* const);

// -----------------------------------------------------------------------------
// --SECTION--                                                            errors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Register an error
////////////////////////////////////////////////////////////////////////////////

void TRI_SetQueryError (TRI_query_error_t* const, const int, const char*);

////////////////////////////////////////////////////////////////////////////////
/// @brief Get the error code registered last
////////////////////////////////////////////////////////////////////////////////

int TRI_GetCodeQueryError (const TRI_query_error_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief Get the error string registered last
////////////////////////////////////////////////////////////////////////////////

char* TRI_GetStringQueryError (const TRI_query_error_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize an error structure
////////////////////////////////////////////////////////////////////////////////

void TRI_InitQueryError (TRI_query_error_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief free an error structure
////////////////////////////////////////////////////////////////////////////////
  
void TRI_FreeQueryError (TRI_query_error_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

////////////////////////////////////////////////////////////////////////////////
/// @brief index-operator (operators used for skiplists and other indexes)
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
/// @author Dr. Oreste Costa-Panaia
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_INDEX_OPERATORS_INDEX_OPERATOR_H
#define TRIAGENS_INDEX_OPERATORS_INDEX_OPERATOR_H 1

#include "BasicsC/json.h"
#include "ShapedJson/json-shaper.h"
#include "ShapedJson/shaped-json.h"
#include "VocBase/vocbase.h"

#ifdef __cplusplus
extern "C" {
#endif


// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief select clause type
////////////////////////////////////////////////////////////////////////////////

typedef enum {

  TRI_EQ_INDEX_OPERATOR,
  TRI_NE_INDEX_OPERATOR,
  TRI_LE_INDEX_OPERATOR,
  TRI_LT_INDEX_OPERATOR,
  TRI_GE_INDEX_OPERATOR,
  TRI_GT_INDEX_OPERATOR,
  TRI_IN_INDEX_OPERATOR,

  TRI_AND_INDEX_OPERATOR,
  TRI_NOT_INDEX_OPERATOR,
  TRI_OR_INDEX_OPERATOR
}
TRI_index_operator_type_e;



////////////////////////////////////////////////////////////////////////////////
/// @brief operands and operators
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_index_operator_s {
  TRI_index_operator_type_e _type;
  TRI_shaper_t* _shaper;
}
TRI_index_operator_t;


//................................................................................
// Storage for NOT / AND / OR logical operators used in skip list indexes and others
// It is a binary or unary operator
//................................................................................

typedef struct TRI_logical_index_operator_s {
  TRI_index_operator_t _base;
  TRI_index_operator_t* _left;  // could be a relation or another logical operator
  TRI_index_operator_t* _right; // could be a relation or another logical operator or null
}
TRI_logical_index_operator_t;


//................................................................................
// Storage for relation operator, e.g. <, <=, >, >=, ==, in
//................................................................................

typedef struct TRI_relation_index_operator_s {
  TRI_index_operator_t _base;
  TRI_json_t* _parameters;    // parameters with which this relation was called with
  TRI_shaped_json_t* _fields; // actual data from the parameters converted from
                              // a json array to a shaped json array
  size_t _numFields;          // number of fields in the array above
  void* _collection;          // required for the computation of the order operation
}
TRI_relation_index_operator_t;


////////////////////////////////////////////////////////////////////////////////
/// @brief create a new index operator of the specified type
///
/// note that the index which uses these operators  will take ownership of the
/// json parameters passed to it
////////////////////////////////////////////////////////////////////////////////

TRI_index_operator_t* TRI_CreateIndexOperator (TRI_index_operator_type_e,
                                               TRI_index_operator_t*,
                                               TRI_index_operator_t*,
                                               TRI_json_t*,
                                               TRI_shaper_t*,
                                               TRI_shaped_json_t*,
                                               size_t,
                                               void*);


////////////////////////////////////////////////////////////////////////////////
/// @brief copy an index  operator recursively (deep copy)
////////////////////////////////////////////////////////////////////////////////

TRI_index_operator_t* TRI_CopyIndexOperator (TRI_index_operator_t*);



////////////////////////////////////////////////////////////////////////////////
/// @brief free an index operator recursively
///
/// note that this will also free all the bound json parameters
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeIndexOperator (TRI_index_operator_t*);




////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////



// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

// .............................................................................
// A structure which (eventually) will allow a query to make an informed 'guess'
// as to the likelihood of an index being efficient if used for document
// retrieval. Added here in this file for convenience. Eventually this structrue
// will be expanded. (Essentially a placeholder for now.)
// .............................................................................

typedef struct TRI_index_challenge_s {
  double _response; // 0 == NO, 1 == YES
} TRI_index_challenge_t;

// .............................................................................
// An enumeration of possible methods which require assignment from a 'generic'
// index structure to a concrete index structure. Added here in this file
// for convenience.
// .............................................................................

typedef enum {
  TRI_INDEX_METHOD_ASSIGNMENT_FREE,
  TRI_INDEX_METHOD_ASSIGNMENT_QUERY,
  TRI_INDEX_METHOD_ASSIGNMENT_RESULT
}
TRI_index_method_assignment_type_e;

struct TRI_index_iterator_s;

typedef int (*TRI_index_query_method_call_t) (void*, TRI_index_operator_t*, TRI_index_challenge_t*, void*);
typedef struct TRI_index_iterator_s* (*TRI_index_query_result_method_call_t) (void*, TRI_index_operator_t*, void*, bool (*filter) (struct TRI_index_iterator_s*));
typedef int (*TRI_index_query_free_method_call_t) (void*, void*);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

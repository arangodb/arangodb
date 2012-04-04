////////////////////////////////////////////////////////////////////////////////
/// @brief sl-operator (operators used for skiplists and others)
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Dr. O
/// @author Copyright 2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_DURHAM_SKIPLISTS_SL_OPERATOR_H
#define TRIAGENS_DURHAM_SKIPLISTS_SL_OPERATOR_H 1

#include "VocBase/vocbase.h"
#include "BasicsC/json.h"
#include "ShapedJson/shaped-json.h"

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

  TRI_SL_EQ_OPERATOR,
  TRI_SL_NE_OPERATOR,
  TRI_SL_LE_OPERATOR,
  TRI_SL_LT_OPERATOR,
  TRI_SL_GE_OPERATOR,
  TRI_SL_GT_OPERATOR,
  
  TRI_SL_AND_OPERATOR,
  TRI_SL_NOT_OPERATOR,
  TRI_SL_OR_OPERATOR
}
TRI_sl_operator_type_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief operands and operators
////////////////////////////////////////////////////////////////////////////////


typedef struct TRI_sl_operator_s {
  TRI_sl_operator_type_e _type;
}
TRI_sl_operator_t;
//................................................................................
// Storage for NOT / AND / OR logical operators used in skip list indexes and others
// It is a binary or unary operator
//................................................................................
typedef struct TRI_sl_logical_operator_s {
  TRI_sl_operator_t _base;
  TRI_sl_operator_t* _left;  // could be a relation or another logical operator
  TRI_sl_operator_t* _right; // could be a relation or another logical operator
}
TRI_sl_logical_operator_t;


typedef struct TRI_sl_relation_operator_s {
  TRI_sl_operator_t _base;
  TRI_json_t* _parameters;    // parameters with which this relation was called with 
  TRI_shaped_json_t* _fields; // actual data from the parameters converted from
                              // a json array to a shaped json array  
  size_t _numFields;          // number of fields in the array above
  void* _collection;          // required for the computation of the order operation  
}
TRI_sl_relation_operator_t;


TRI_sl_operator_t* CreateSLOperator (TRI_sl_operator_type_e,
                                    TRI_sl_operator_t*,TRI_sl_operator_t*,
                                    TRI_json_t*, TRI_shaped_json_t*,
                                    size_t, void*);                                    
TRI_sl_operator_t* CopySLOperator (TRI_sl_operator_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief a skiplist operator with all its linked sub information
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeSLOperator(TRI_sl_operator_t*);

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

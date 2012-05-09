////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, access optimiser
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

#ifndef TRIAGENS_DURHAM_AHUACATL_ACCESS_OPTIMISER_H
#define TRIAGENS_DURHAM_AHUACATL_ACCESS_OPTIMISER_H 1

#include <BasicsC/common.h>
#include <BasicsC/associative.h>
#include <BasicsC/hashes.h>
#include <BasicsC/json-utilities.h>
#include <BasicsC/logging.h>
#include <BasicsC/strings.h>
#include <BasicsC/string-buffer.h>
#include <BasicsC/vector.h>

#include "Ahuacatl/ahuacatl-ast-node.h"
#include "Ahuacatl/ahuacatl-conversions.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief logical operator types
////////////////////////////////////////////////////////////////////////////////
                              
typedef enum {
  TRI_AQL_LOGICAL_AND,
  TRI_AQL_LOGICAL_OR
}
TRI_aql_logical_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief access types, sorted from best (most efficient) to last
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_AQL_ACCESS_IMPOSSIBLE,   // no values must be accessed
  TRI_AQL_ACCESS_EXACT,        // one value must be accessed
  TRI_AQL_ACCESS_LIST,         // a list of values must be accessed
  TRI_AQL_ACCESS_RANGE_SINGLE, // a range with one bound must be accessed
  TRI_AQL_ACCESS_RANGE_DOUBLE, // a two bounded range must be accessed
  TRI_AQL_ACCESS_ALL           // all values must be accessed
}
TRI_aql_access_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief range access types
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_AQL_RANGE_LOWER_EXCLUDED, // x| ... inf
  TRI_AQL_RANGE_LOWER_INCLUDED, // |x ... inf 
  TRI_AQL_RANGE_UPPER_EXCLUDED, // -inf ... |x
  TRI_AQL_RANGE_UPPER_INCLUDED  // -inf ... x|
}
TRI_aql_range_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief range type (consisting of range type & range bound value)
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_aql_range_s {
  TRI_json_t* _value;
  TRI_aql_range_e _type;
}
TRI_aql_range_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief attribute access container used during optimisation
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_aql_field_access_s {
  char* _fieldName;
  TRI_aql_access_e _type;

  union {
    TRI_json_t* _value;           // used for TRI_AQL_ACCESS_EXACT, TRI_AQL_ACCESS_LIST
    TRI_aql_range_t _singleRange; // used for TRI_AQL_ACCESS_RANGE_SINGLE
    struct {
      TRI_aql_range_t _lower;     // lower bound
      TRI_aql_range_t _upper;     // upper bound
    }
    _between;                     // used for TRI_AQL_ACCESS_RANGE_DOUBLE
  } 
  _value;
}
TRI_aql_field_access_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief attribute name container
////////////////////////////////////////////////////////////////////////////////
  
typedef struct TRI_aql_attribute_name_s {
  const char* _variable;     // variable name/alias used
  TRI_string_buffer_t _name; // complete attribute name (including variable and '.'s)
}
TRI_aql_attribute_name_t;

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
/// @brief clone a vector of ranges
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t* TRI_CloneRangesAql (TRI_aql_context_t* const, 
                                          const TRI_vector_pointer_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief free access structure with its members and the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeAccessAql (TRI_aql_field_access_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief clone an attribute access structure by deep-copying it
////////////////////////////////////////////////////////////////////////////////

TRI_aql_field_access_t* TRI_CloneAccessAql (TRI_aql_context_t* const,
                                            TRI_aql_field_access_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the preferred (i.e. better) access type for a loop
////////////////////////////////////////////////////////////////////////////////

int TRI_PickAccessAql (const TRI_aql_field_access_t* const,
                       const TRI_aql_field_access_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief free a range vector
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeRangesAql (TRI_vector_pointer_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief dump a single range for debugging purposes
////////////////////////////////////////////////////////////////////////////////

void TRI_DumpRangeAql (const TRI_aql_field_access_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief dump ranges found for debugging purposes
////////////////////////////////////////////////////////////////////////////////

void TRI_DumpRangesAql (const TRI_vector_pointer_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief track and optimise attribute accesses for a given node and subnodes
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t* TRI_OptimiseRangesAql (TRI_aql_context_t* const,  
                                             TRI_aql_node_t*,
                                             bool*,
                                             const TRI_vector_pointer_t* const);

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

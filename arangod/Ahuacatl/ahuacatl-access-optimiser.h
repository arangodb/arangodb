////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, access optimiser
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

#ifndef TRIAGENS_AHUACATL_AHUACATL_ACCESS_OPTIMISER_H
#define TRIAGENS_AHUACATL_AHUACATL_ACCESS_OPTIMISER_H 1

#include "BasicsC/common.h"
#include "BasicsC/string-buffer.h"

#include "Ahuacatl/ahuacatl-ast-node.h"

#ifdef __cplusplus
extern "C" {
#endif

struct TRI_aql_context_s;
struct TRI_json_s;
struct TRI_vector_pointer_s;

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
  TRI_AQL_ACCESS_IMPOSSIBLE,       // no value needs to be accessed (impossible range)
  TRI_AQL_ACCESS_EXACT,            // one value is accessed
  TRI_AQL_ACCESS_LIST,             // a list of values is accessed
  TRI_AQL_ACCESS_RANGE_SINGLE,     // a range with one boundary is accessed
  TRI_AQL_ACCESS_RANGE_DOUBLE,     // a two bounded range is accessed
  TRI_AQL_ACCESS_REFERENCE,        // a reference can be used for eq access (a.x == b.x)
                                   // or range access (a.x > b.x)
  TRI_AQL_ACCESS_ALL               // all values must be accessed (full scan)
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
/// @brief access types, sorted from best (most efficient) to last
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_AQL_REFERENCE_VARIABLE,        // reference to a variable
  TRI_AQL_REFERENCE_ATTRIBUTE_ACCESS // reference to an attribute access
}
TRI_aql_reference_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief range type (consisting of range type & range bound value)
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_aql_range_s {
  struct TRI_json_s* _value;
  TRI_aql_range_e    _type;
}
TRI_aql_range_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief attribute access container used during optimisation
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_aql_field_access_s {
  char* _fullName;
  size_t _variableNameLength;     // length of variable name part (upto '.') in _fullName
  TRI_aql_access_e _type;

  union {
    struct TRI_json_s* _value;    // used for TRI_AQL_ACCESS_EXACT, TRI_AQL_ACCESS_LIST

    TRI_aql_range_t _singleRange; // used for TRI_AQL_ACCESS_RANGE_SINGLE

    struct {
      TRI_aql_range_t _lower;     // lower bound
      TRI_aql_range_t _upper;     // upper bound
    }
    _between;                     // used for TRI_AQL_ACCESS_RANGE_DOUBLE

    struct {
      union {
        char* _name;
        TRI_aql_node_t* _node;
      }
      _ref;
      TRI_aql_reference_e _type;
      TRI_aql_node_type_e _operator;
    }
    _reference;                   // used for TRI_AQL_ACCESS_REFERENCE
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
/// @brief dump an AQL access type
////////////////////////////////////////////////////////////////////////////////

void TRI_DumpAccessAql (const TRI_aql_field_access_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an access structure of type impossible
////////////////////////////////////////////////////////////////////////////////

TRI_aql_field_access_t* TRI_CreateImpossibleAccessAql (struct TRI_aql_context_s* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if an attribute access structure vector contains the
/// impossible range
////////////////////////////////////////////////////////////////////////////////

bool TRI_ContainsImpossibleAql (const struct TRI_vector_pointer_s* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief clone a vector of accesses
////////////////////////////////////////////////////////////////////////////////

struct TRI_vector_pointer_s* TRI_CloneAccessesAql (struct TRI_aql_context_s* const,
                                                   const struct TRI_vector_pointer_s* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief free access structure with its members and the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeAccessAql (TRI_aql_field_access_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief clone an attribute access structure by deep-copying it
////////////////////////////////////////////////////////////////////////////////

TRI_aql_field_access_t* TRI_CloneAccessAql (struct TRI_aql_context_s* const,
                                            TRI_aql_field_access_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the preferred (i.e. better) access type for a loop
////////////////////////////////////////////////////////////////////////////////

int TRI_PickAccessAql (const TRI_aql_field_access_t* const,
                       const TRI_aql_field_access_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief free all field access structs in a vector
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeAccessesAql (struct TRI_vector_pointer_s* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief add a field access type to an existing field access vector
////////////////////////////////////////////////////////////////////////////////

struct TRI_vector_pointer_s* TRI_AddAccessAql (struct TRI_aql_context_s* const,
                                               struct TRI_vector_pointer_s* const,
                                               TRI_aql_field_access_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the range operator string for a range operator
////////////////////////////////////////////////////////////////////////////////

const char* TRI_RangeOperatorAql (const TRI_aql_range_e);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the range operator string for a comparison operator
////////////////////////////////////////////////////////////////////////////////

const char* TRI_ComparisonOperatorAql (const TRI_aql_node_type_e);

////////////////////////////////////////////////////////////////////////////////
/// @brief track and optimise attribute accesses for a given node and subnodes
////////////////////////////////////////////////////////////////////////////////

struct TRI_vector_pointer_s* TRI_OptimiseRangesAql (struct TRI_aql_context_s* const,
                                                    TRI_aql_node_t*,
                                                    bool*,
                                                    const struct TRI_vector_pointer_s* const);

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

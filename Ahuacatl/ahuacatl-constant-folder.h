////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, constant folding
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

#ifndef TRIAGENS_DURHAM_AHUACATL_CONSTANT_FOLDER_H
#define TRIAGENS_DURHAM_AHUACATL_CONSTANT_FOLDER_H 1

#include <BasicsC/common.h>
#include <BasicsC/strings.h>
#include <BasicsC/string-buffer.h>
#include <BasicsC/hashes.h>
#include <BasicsC/logging.h>
#include <BasicsC/vector.h>
#include <BasicsC/associative.h>

#include "Ahuacatl/ahuacatl-ast-node.h"
#include "Ahuacatl/ahuacatl-tree-walker.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief access type
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_AQL_RANGE_LOWER_EXCLUDED,
  TRI_AQL_RANGE_LOWER_INCLUDED,
  TRI_AQL_RANGE_UPPER_EXCLUDED,
  TRI_AQL_RANGE_UPPER_INCLUDED
}
TRI_aql_range_e;

typedef enum {
  TRI_AQL_ACCESS_ALL = 0,
  TRI_AQL_ACCESS_IMPOSSIBLE,
  TRI_AQL_ACCESS_EXACT,
  TRI_AQL_ACCESS_LIST,
  TRI_AQL_ACCESS_SINGLE_RANGE,
  TRI_AQL_ACCESS_DOUBLE_RANGE,
}
TRI_aql_access_e;

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
    TRI_json_t* _exactValue;
    TRI_json_t* _list;
    TRI_aql_range_t _singleRange;
    struct {
      TRI_aql_range_t _lower;
      TRI_aql_range_t _upper;
    }
    _between;
  } 
  _value;
}
TRI_aql_field_access_t;
  


typedef struct TRI_aql_field_name_s {
  const char* _variable;
  TRI_string_buffer_t _name;
}
TRI_aql_field_name_t;
  


////////////////////////////////////////////////////////////////////////////////
/// @brief fold constants recursively
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_FoldConstantsAql (TRI_aql_context_t* const, 
                                      TRI_aql_node_t*);

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

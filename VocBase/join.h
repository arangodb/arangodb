////////////////////////////////////////////////////////////////////////////////
/// @brief %BRIEF%
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

#ifndef TRIAGENS_DURHAM_VOC_BASE_JOIN_H
#define TRIAGENS_DURHAM_VOC_BASE_JOIN_H 1

#include <BasicsC/common.h>
#include <BasicsC/vector.h>
#include <BasicsC/strings.h>

#include "VocBase/query.h"

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief possible join types
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  JOIN_TYPE_PRIMARY,
  JOIN_TYPE_INNER,
  JOIN_TYPE_OUTER,
  JOIN_TYPE_LIST
}
TRI_join_type_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief join data structure
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_join_part_s {
  TRI_join_type_e _type;
  TRI_qry_where_t* _condition;
  TRI_doc_collection_t* _collection;
  char* _collectionName;
  char* _alias;
//  TRI_js_exec_context_t _context;

  void (*free) (struct TRI_join_part_s*);
} 
TRI_join_part_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief join container data structure for select queries
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_select_join_s {
  TRI_vector_pointer_t _parts;
  
  void (*free) (struct TRI_select_join_s*);
}
TRI_select_join_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief Add a part to a select join
////////////////////////////////////////////////////////////////////////////////
      
bool TRI_AddPartSelectJoin(TRI_select_join_t*, 
                           const TRI_join_type_e, 
                           TRI_qry_where_t*, 
                           char*, 
                           char*);

////////////////////////////////////////////////////////////////////////////////
/// @brief Create a new join 
////////////////////////////////////////////////////////////////////////////////

TRI_select_join_t* TRI_CreateSelectJoin (void);

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



////////////////////////////////////////////////////////////////////////////////
/// @brief joins
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

#include "VocBase/join.h"

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief get size of extra data in standard join parts - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static size_t GetExtraDataSizeVoid (TRI_join_part_t* part) {
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get size of extra data in geo join parts - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static size_t GetExtraDataSizeGeo (TRI_join_part_t* part) {
  return sizeof(double);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Free join part memory - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static void FreePart (TRI_join_part_t* part) {
  if (part->_alias) {
    TRI_Free(part->_alias);
  }

  if (part->_ranges) {
    // TODO: free ranges
  }

  if (part->_collectionName) {
    TRI_Free(part->_collectionName);
  }
  
  if (part->_condition) {
    part->_condition->free(part->_condition);
  }

  if (part->_feeder) {
    part->_feeder->free(part->_feeder);
  }

  if (part->_context) {
    TRI_FreeExecutionContext(part->_context);
  }

  if (part->_listDocuments._buffer) {  
    TRI_DestroyVectorPointer(&part->_listDocuments);
  }

  TRI_Free(part);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Free join memory - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static void FreeSelectJoin (TRI_select_join_t* join) {
  TRI_join_part_t* part;
  size_t i;

  for (i = 0; i < join->_parts._length; i++) {
    part = (TRI_join_part_t*) join->_parts._buffer[i];
    part->free(part);
  }

  TRI_DestroyVectorPointer(&join->_parts);
  TRI_Free(join);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Add a part to a select join - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

bool TRI_AddPartSelectJoinX (TRI_select_join_t* join, 
                            const TRI_join_type_e type, 
                            TRI_qry_where_t* condition,
                            TRI_vector_pointer_t* ranges, 
                            char* collectionName, 
                            char* alias,
                            QL_ast_query_geo_restriction_t* geoRestriction) {
  
  TRI_join_part_t* part;
  TRI_qry_where_general_t* conditionGeneral;
  
  assert(join != NULL);
  part = (TRI_join_part_t*) TRI_Allocate(sizeof(TRI_join_part_t));

  if (!part) {
    return false;
  }

  // initialize vector for list joins
  TRI_InitVectorPointer(&part->_listDocuments);
  part->_context = NULL;
  part->_singleDocument = NULL;
  part->_feeder = NULL; 
  part->_type = type;
  part->_condition = condition;
  part->_ranges = ranges;
  part->_collection = NULL;
  part->_collectionName = TRI_DuplicateString(collectionName);
  part->_alias = TRI_DuplicateString(alias);
  part->_geoRestriction = geoRestriction;
  
  if (part->_condition != NULL && part->_condition->_type == TRI_QRY_WHERE_GENERAL) {
    conditionGeneral = (TRI_qry_where_general_t*) part->_condition;
    part->_context = TRI_CreateExecutionContext(conditionGeneral->_code);
  }

  // determine size of extra data to store  
  if (part->_geoRestriction) {
    part->_extraData._size = GetExtraDataSizeGeo(part);
    part->_extraData._alias = part->_geoRestriction->_alias;
  }
  else {
    part->_extraData._size = GetExtraDataSizeVoid(part);
    part->_extraData._alias = NULL;
  }
  part->free = FreePart; 

  TRI_PushBackVectorPointer(&join->_parts, part);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Create a new join - DEPRECATED 
////////////////////////////////////////////////////////////////////////////////

TRI_select_join_t* TRI_CreateSelectJoin (void) {
  TRI_select_join_t* join;

  join = (TRI_select_join_t*) TRI_Allocate(sizeof(TRI_select_join_t));
  if (!join) {
    return NULL;
  }

  TRI_InitVectorPointer(&join->_parts);
  join->free = FreeSelectJoin;

  return join;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


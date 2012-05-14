////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, index access
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

#include "Ahuacatl/ahuacatl-index.h"
#include "Ahuacatl/ahuacatl-access-optimiser.h"
#include "Ahuacatl/ahuacatl-context.h" 

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief log information about the used index
////////////////////////////////////////////////////////////////////////////////

static void LogIndexString (TRI_index_t const* idx,
                            char const* collectionName) {
  TRI_string_buffer_t* buffer = TRI_CreateStringBuffer(TRI_UNKNOWN_MEM_ZONE);
  size_t i;

  if (buffer == NULL) {
    return;
  }
  
  for (i = 0; i < idx->_fields._length; i++) {
    if (i > 0) {
      TRI_AppendStringStringBuffer(buffer, ", "); 
    }

    TRI_AppendStringStringBuffer(buffer, idx->_fields._buffer[i]);
  }

  LOG_TRACE("using %s index (%s) for '%s'", 
            TRI_TypeNameIndex(idx), 
            buffer->_buffer, 
            collectionName);

  TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief pick or replace an index
////////////////////////////////////////////////////////////////////////////////
      
static TRI_aql_index_t* PickIndex (TRI_aql_context_t* const context,
                                   TRI_aql_index_t* pickedIndex, 
                                   const TRI_index_t* const idx,
                                   TRI_vector_pointer_t* fieldAccesses) {
  bool isBetter;

  assert(idx);
  assert(fieldAccesses);

  if (pickedIndex == NULL) {
    pickedIndex = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_aql_index_t), false);
    pickedIndex->_idx = NULL;
    pickedIndex->_fieldAccesses = NULL;
  }

  if (pickedIndex == NULL) {
    // OOM
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return NULL;
  }

  if (pickedIndex->_idx == NULL) {
    isBetter = true;
  }
  else {
    isBetter = idx->_type == TRI_IDX_TYPE_PRIMARY_INDEX ||
               fieldAccesses->_length < pickedIndex->_fieldAccesses->_length ||
               (fieldAccesses->_length == pickedIndex->_fieldAccesses->_length && idx->_unique && !pickedIndex->_idx->_unique);
  }

  if (isBetter) { 
    if (pickedIndex->_fieldAccesses != NULL) {
      TRI_FreeVectorPointer(TRI_UNKNOWN_MEM_ZONE, pickedIndex->_fieldAccesses);
    }

    pickedIndex->_idx = (TRI_index_t*) idx;
    pickedIndex->_fieldAccesses = TRI_CopyVectorPointer(TRI_UNKNOWN_MEM_ZONE, fieldAccesses);
  }

  return pickedIndex;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief determine which index to use for a specific for loop
////////////////////////////////////////////////////////////////////////////////

TRI_aql_index_t* TRI_DetermineIndexAql (TRI_aql_context_t* const context,
                                        const TRI_vector_pointer_t* const availableIndexes,
                                        const char* const collectionName,
                                        const TRI_vector_pointer_t* const candidates) {
  TRI_aql_index_t* picked = NULL;
  TRI_vector_pointer_t matches;
  size_t i, j, k, n;

  TRI_InitVectorPointer(&matches, TRI_UNKNOWN_MEM_ZONE);

  assert(context);
  assert(collectionName);
  assert(candidates);

  n = availableIndexes->_length;
  for (i = 0; i < n; ++i) {
    TRI_index_t* idx = (TRI_index_t*) availableIndexes->_buffer[i];
    TRI_aql_access_e lastType;
    size_t numIndexFields = idx->_fields._length;

    if (idx->_type == TRI_IDX_TYPE_GEO1_INDEX ||
        idx->_type == TRI_IDX_TYPE_GEO2_INDEX) {
      // ignore all geo indexes for now
      continue;
    }

    if (numIndexFields == 0) {
      // index should contain at least one field
      continue;
    }
    
    TRI_ClearVectorPointer(&matches);

    lastType = TRI_AQL_ACCESS_EXACT;

    // now loop over all index fields
    for (j = 0; j < numIndexFields; ++j) {
      char* indexedFieldName = idx->_fields._buffer[j];

      if (!indexedFieldName) {
        continue;
      }

      // now loop over all candidates
      for (k = 0; k < candidates->_length; ++k) {
        TRI_aql_field_access_t* candidate = (TRI_aql_field_access_t*) TRI_AtVectorPointer(candidates, k);

        if (!TRI_EqualString(indexedFieldName, candidate->_fieldName)) {
          // different field
          continue;
        }

        if (candidate->_type == TRI_AQL_ACCESS_IMPOSSIBLE || 
            candidate->_type == TRI_AQL_ACCESS_ALL) {
          // wrong index type, doesn't help us at all
          continue;
        }

        if (idx->_type == TRI_IDX_TYPE_PRIMARY_INDEX) {
          if (candidate->_type != TRI_AQL_ACCESS_EXACT) {
            // wrong access type for primary index
            continue;
          }
          
          TRI_PushBackVectorPointer(&matches, candidate);
        }
        else if (idx->_type == TRI_IDX_TYPE_HASH_INDEX) {
          if (candidate->_type != TRI_AQL_ACCESS_EXACT) {
            // wrong access type for hash index
            continue;
          }
          
          TRI_PushBackVectorPointer(&matches, candidate);
        }
        else if (idx->_type == TRI_IDX_TYPE_SKIPLIST_INDEX) {
          if (candidate->_type != TRI_AQL_ACCESS_EXACT && 
              candidate->_type != TRI_AQL_ACCESS_RANGE_SINGLE && 
              candidate->_type != TRI_AQL_ACCESS_RANGE_DOUBLE) {
            // wrong access type for skiplists
            continue;
          }

          if ((candidate->_type == TRI_AQL_ACCESS_EXACT && lastType != TRI_AQL_ACCESS_EXACT) ||
              (candidate->_type != TRI_AQL_ACCESS_EXACT && lastType != TRI_AQL_ACCESS_EXACT)) {
            // if we already had a range query, we cannot check for equality after that
            // if we already had a range query, we cannot check another range after that
            continue;
          }

          TRI_PushBackVectorPointer(&matches, candidate);
        }
      }
    }

    // we now do or don't have an index candidate in the matches vector
    if (matches._length < numIndexFields && 
        (idx->_type == TRI_IDX_TYPE_PRIMARY_INDEX || idx->_type == TRI_IDX_TYPE_HASH_INDEX)) {
      // the matches vector does not fully cover the indexed fields, but the index requires it
      continue;
    }

    // if we can use the primary index, we'll use it
    if (idx->_type == TRI_IDX_TYPE_PRIMARY_INDEX) {
      picked = PickIndex(context, picked, idx, &matches);
    }
  }

  TRI_DestroyVectorPointer(&matches);

  if (picked) {
    LogIndexString(picked->_idx, collectionName);
  }

  return picked;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, index access
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

#include "Ahuacatl/ahuacatl-index.h"

#include "BasicsC/logging.h"
#include "BasicsC/tri-strings.h"
#include "BasicsC/string-buffer.h"

#include "Ahuacatl/ahuacatl-access-optimiser.h"
#include "Ahuacatl/ahuacatl-context.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief log information about the used index
////////////////////////////////////////////////////////////////////////////////

static void LogIndexString (const char* const what,
                            TRI_index_t const* idx,
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

  LOG_TRACE("%s %s index (%s) for '%s'",
            what,
            idx->typeName(idx),
            buffer->_buffer,
            collectionName);

  TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether a field access candidate is an exact access
////////////////////////////////////////////////////////////////////////////////

static bool IsExactCandidate (const TRI_aql_field_access_t* const candidate) {
  if (candidate->_type == TRI_AQL_ACCESS_EXACT) {
    // ==
    return true;
  }

  if (candidate->_type == TRI_AQL_ACCESS_LIST) {
    // in (...)
    return true;
  }

  if (candidate->_type == TRI_AQL_ACCESS_REFERENCE &&
      candidate->_value._reference._operator == TRI_AQL_NODE_OPERATOR_BINARY_EQ) {
    // == ref
    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief pick or replace an index
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_index_t* PickIndex (TRI_aql_context_t* const context,
                                   TRI_aql_index_t* pickedIndex,
                                   const TRI_index_t* const idx,
                                   TRI_vector_pointer_t* fieldAccesses) {
  bool isBetter = false;

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


  // ...........................................................................
  // If we do not have an index yet, then this index will do. As has been said
  // before 'any index is better than none'
  // ...........................................................................

  if (pickedIndex->_idx == NULL) {
    pickedIndex->_idx = (TRI_index_t*) idx;
    pickedIndex->_fieldAccesses = TRI_CopyVectorPointer(TRI_UNKNOWN_MEM_ZONE, fieldAccesses);
    return pickedIndex;
  }


  // ...........................................................................
  // We have previously selected an index, if it happens to be the primary then
  // we stick with it.
  // ...........................................................................

  if (pickedIndex->_idx->_type == TRI_IDX_TYPE_PRIMARY_INDEX) {
    return pickedIndex;
  }


  // ...........................................................................
  // Now go through the various possibilities if we have not located something
  // better.
  // ...........................................................................

  if ( (isBetter == false) && (idx->_type == TRI_IDX_TYPE_PRIMARY_INDEX) ) {
    // .........................................................................
    // If we can used the primary index, then this is better than any other
    // index so use it.
    // .........................................................................
    isBetter = true;
  }


  if ( (isBetter == false) && (idx->_type == TRI_IDX_TYPE_HASH_INDEX) ) {
    // .........................................................................
    // If the index type is a hash index, use this -- but only if we have NOT
    // located something better BEFORE.
    // .........................................................................
    isBetter = true;
  }


  if ( (isBetter == false) && (idx->_type == TRI_IDX_TYPE_SKIPLIST_INDEX) &&
       (pickedIndex->_idx->_type != TRI_IDX_TYPE_HASH_INDEX) ) {
    // .........................................................................
    // If the index type is a skiplist index, use this -- but only if we have NOT
    // located something better BEFORE.
    // .........................................................................
    isBetter = true;
  }


  if ( (isBetter == false) && (idx->_type == TRI_IDX_TYPE_BITARRAY_INDEX) &&
       (pickedIndex->_idx->_type != TRI_IDX_TYPE_HASH_INDEX)              &&
       (pickedIndex->_idx->_type != TRI_IDX_TYPE_SKIPLIST_INDEX) ) {
    // .........................................................................
    // If the index type is a bitarray index, use this -- but only if we have NOT
    // located something better BEFORE.
    // .........................................................................
    isBetter = true;
  }


  if ( (isBetter == false) && (idx->_unique == true) && (pickedIndex->_idx->_unique == false) ) {
    // .........................................................................
    // If the index is a unique one and the picked index is non-unique, then
    // replace it with the unique overriding the preferences above. E.g. if
    // we have a non-unique hash index (which we have chosen) and now we are
    // testing a unique skiplist, replace it with the skiplist.
    // .........................................................................
    isBetter = true;
  }


  if ( (isBetter == false) &&
       (fieldAccesses->_length < pickedIndex->_fieldAccesses->_length ) &&
       (idx->_unique == true) ) {
    isBetter = true;
  }


  if ( (isBetter == false) &&
       (fieldAccesses->_length >  pickedIndex->_fieldAccesses->_length ) &&
       (idx->_unique == false) ) {
    isBetter = true;
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
/// @brief check eligibility of an index for further inspection
////////////////////////////////////////////////////////////////////////////////

static bool CanUseIndex (const TRI_index_t* const idx) {
  if (idx->_fields._length == 0) {
    // index should contain at least one field
    return false;
  }

  // we'll use a switch here so the compiler warns if new index types are added elsewhere but not here
  switch (idx->_type) {
    case TRI_IDX_TYPE_GEO1_INDEX:
    case TRI_IDX_TYPE_GEO2_INDEX:
    case TRI_IDX_TYPE_PRIORITY_QUEUE_INDEX:
    case TRI_IDX_TYPE_CAP_CONSTRAINT:
    case TRI_IDX_TYPE_FULLTEXT_INDEX:
      // ignore all these index types for now
      return false;

    case TRI_IDX_TYPE_PRIMARY_INDEX:
    case TRI_IDX_TYPE_HASH_INDEX:
    case TRI_IDX_TYPE_EDGE_INDEX:
    case TRI_IDX_TYPE_SKIPLIST_INDEX:
    case TRI_IDX_TYPE_BITARRAY_INDEX:
      // these indexes are valid candidates
      break;
  }

  return true;
}

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
/// @brief free an index structure
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeIndexAql (TRI_aql_index_t* const idx) {
  assert(idx);

  TRI_FreeVectorPointer(TRI_UNKNOWN_MEM_ZONE, idx->_fieldAccesses);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, idx);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief determine which index to use for a specific for loop
////////////////////////////////////////////////////////////////////////////////

TRI_aql_index_t* TRI_DetermineIndexAql (TRI_aql_context_t* const context,
                                        const TRI_vector_pointer_t* const availableIndexes,
                                        const char* const collectionName,
                                        const TRI_vector_pointer_t* candidates) {
  TRI_aql_index_t* picked = NULL;
  TRI_vector_pointer_t matches;
  size_t i, n;

  TRI_InitVectorPointer(&matches, TRI_UNKNOWN_MEM_ZONE);

  assert(context);
  assert(collectionName);
  assert(candidates);

  n = availableIndexes->_length;
  for (i = 0; i < n; ++i) {
    TRI_index_t* idx = (TRI_index_t*) availableIndexes->_buffer[i];
    size_t numIndexFields;
    bool lastTypeWasExact;
    size_t j;

    if (! CanUseIndex(idx)) {
      continue;
    }

    LogIndexString("checking", idx, collectionName);

    TRI_ClearVectorPointer(&matches);

    lastTypeWasExact = true;
    numIndexFields = idx->_fields._length;

    // now loop over all index fields, from left to right
    // index field order is important because skiplists can be used with leftmost prefixes as well,
    // but not with rightmost prefixes
    for (j = 0; j < numIndexFields; ++j) {
      char* indexedFieldName;
      char* fieldName;
      size_t k;

      indexedFieldName = idx->_fields._buffer[j];
      if (indexedFieldName == NULL) {
        continue;
      }

      // now loop over all candidates
      for (k = 0; k < candidates->_length; ++k) {
        TRI_aql_field_access_t* candidate = (TRI_aql_field_access_t*) TRI_AtVectorPointer(candidates, k);

        if (candidate->_type == TRI_AQL_ACCESS_IMPOSSIBLE ||
            candidate->_type == TRI_AQL_ACCESS_ALL) {
          // wrong index type, doesn't help us at all
          continue;
        }

        fieldName = candidate->_fullName + candidate->_variableNameLength + 1;

        if (idx->_type == TRI_IDX_TYPE_PRIMARY_INDEX) {
          // primary index key names must be treated differently. _id and _key are the same
          if (! TRI_EqualString("_id", fieldName) && ! TRI_EqualString(TRI_VOC_ATTRIBUTE_KEY, fieldName)) {
            continue;
          }
        }
        else if (idx->_type == TRI_IDX_TYPE_EDGE_INDEX) {
          // edge index key names must be treated differently. _from and _to can be used independently
          if (! TRI_EqualString(TRI_VOC_ATTRIBUTE_FROM, fieldName) &&
              ! TRI_EqualString(TRI_VOC_ATTRIBUTE_TO, fieldName)) {
            continue;
          }
        }
        else if (! TRI_EqualString(indexedFieldName, fieldName)) {
          // different attribute, doesn't help
          continue;
        }

        // attribute is used in index

        if (idx->_type == TRI_IDX_TYPE_PRIMARY_INDEX || idx->_type == TRI_IDX_TYPE_EDGE_INDEX) {
          if (! IsExactCandidate(candidate)) {
            // wrong access type for primary index
            continue;
          }

          TRI_PushBackVectorPointer(&matches, candidate);
        }

        else if (idx->_type == TRI_IDX_TYPE_HASH_INDEX) {
          if (! IsExactCandidate(candidate)) {
            // wrong access type for hash index
            continue;
          }

          if (candidate->_type == TRI_AQL_ACCESS_LIST && numIndexFields != 1) {
            // we found a list, but the index covers multiple attributes. that means we cannot use list access
            continue;
          }

          TRI_PushBackVectorPointer(&matches, candidate);
        }

        else if (idx->_type == TRI_IDX_TYPE_BITARRAY_INDEX) {
          if (! IsExactCandidate(candidate)) {
            // wrong access type for hash index
            continue;
          }

          if (candidate->_type == TRI_AQL_ACCESS_LIST) {
            // we found a list, but the index covers multiple attributes. that means we cannot use list access
            continue;
          }

          TRI_PushBackVectorPointer(&matches, candidate);
        }

        else if (idx->_type == TRI_IDX_TYPE_SKIPLIST_INDEX) {
          bool candidateIsExact;

          if (candidate->_type != TRI_AQL_ACCESS_EXACT &&
              candidate->_type != TRI_AQL_ACCESS_LIST &&
              candidate->_type != TRI_AQL_ACCESS_RANGE_SINGLE &&
              candidate->_type != TRI_AQL_ACCESS_RANGE_DOUBLE &&
              candidate->_type != TRI_AQL_ACCESS_REFERENCE) {
            // wrong access type for skiplists
            continue;
          }

          if (candidate->_type == TRI_AQL_ACCESS_LIST && numIndexFields != 1) {
            // we found a list, but the index covers multiple attributes. that means we cannot use list access
            continue;
          }

          candidateIsExact = IsExactCandidate(candidate);

          if ((candidateIsExact && ! lastTypeWasExact) ||
              (! candidateIsExact && ! lastTypeWasExact)) {
            // if we already had a range query, we cannot check for equality after that
            // if we already had a range query, we cannot check another range after that
            continue;
          }

          lastTypeWasExact = candidateIsExact;

          TRI_PushBackVectorPointer(&matches, candidate);
        }
      }

      // finished iterating over all candidates

      if (matches._length != j + 1) {
        // we already have picked less candidate fields than we should
        break;
      }
    }

    if (matches._length < 1) {
      // nothing found
      continue;
    }

    // we now do or don't have an index candidate in the matches vector
    if (matches._length < numIndexFields && idx->_needsFullCoverage) {
      // the matches vector does not fully cover the indexed fields, but the index requires it
      continue;
    }

    // if we can use the primary index, we'll use it
    picked = PickIndex(context, picked, idx, &matches);
  }

  TRI_DestroyVectorPointer(&matches);

  if (picked) {
    LogIndexString("using", picked->_idx, collectionName);
  }

  return picked;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

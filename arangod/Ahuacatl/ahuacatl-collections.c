////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, collection
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

#include "Ahuacatl/ahuacatl-collections.h"

#include "BasicsC/logging.h"
#include "BasicsC/tri-strings.h"
#include "VocBase/index.h"
#include "VocBase/primary-collection.h"

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
/// @brief get collection/index id as a string
///
/// The caller must free the result string
////////////////////////////////////////////////////////////////////////////////

static char* GetIndexIdString (TRI_aql_collection_hint_t* const hint) {
  TRI_string_buffer_t buffer;
  char* result;

  TRI_InitStringBuffer(&buffer, TRI_UNKNOWN_MEM_ZONE);
  TRI_AppendUInt64StringBuffer(&buffer, hint->_collection->_collection->_cid);
  TRI_AppendCharStringBuffer(&buffer, '/');
  TRI_AppendUInt64StringBuffer(&buffer, hint->_index->_idx->_iid);

  result = TRI_DuplicateStringZ(TRI_UNKNOWN_MEM_ZONE, buffer._buffer);
  TRI_DestroyStringBuffer(&buffer);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief comparator function to sort collections by name
////////////////////////////////////////////////////////////////////////////////

static int CollectionNameComparator (const void* l, const void* r) {
  TRI_aql_collection_t* left = *((TRI_aql_collection_t**) l);
  TRI_aql_collection_t* right = *((TRI_aql_collection_t**) r);

  return strcmp(left->_name, right->_name);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a collection container
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_collection_t* CreateCollectionContainer (const char* const name) {
  TRI_aql_collection_t* collection;

  assert(name);

  collection = (TRI_aql_collection_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_aql_collection_t), false);
  if (collection == NULL) {
    return NULL;
  }

  collection->_name = (char*) name;
  collection->_collection = NULL;
  collection->_barrier = NULL;

  return collection;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set up the collection names vector and order it
////////////////////////////////////////////////////////////////////////////////

bool SetupCollections (TRI_aql_context_t* const context) {
  size_t i;
  size_t n;
  bool result = true;

  // each collection used is contained once in the assoc. array
  // so we do not have to care about duplicate names here
  n = context->_collectionNames._nrAlloc;
  for (i = 0; i < n; ++i) {
    char* name = context->_collectionNames._table[i];
    TRI_aql_collection_t* collection;

    if (! name) {
      continue;
    }

    collection = CreateCollectionContainer(name);
    if (! collection) {
      result = false;
      TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
      break;
    }

    TRI_PushBackVectorPointer(&context->_collections, (void*) collection);
  }

  if (result && n > 0) {
    qsort(context->_collections._buffer,
          context->_collections._length,
          sizeof(void*),
          &CollectionNameComparator);
  }

  // now collections contains the sorted list of collections

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief open all collections used
////////////////////////////////////////////////////////////////////////////////

bool OpenCollections (TRI_aql_context_t* const context) {
  size_t i;
  size_t n;

  n = context->_collections._length;
  for (i = 0; i < n; ++i) {
    TRI_aql_collection_t* collection = context->_collections._buffer[i];

    assert(collection);
    assert(collection->_name);
    assert(! collection->_collection);
    assert(! collection->_barrier);

    LOG_TRACE("locking collection '%s'", collection->_name);
    collection->_collection = TRI_UseCollectionByNameVocBase(context->_vocbase, collection->_name);
    if (collection->_collection == NULL) {
      TRI_SetErrorContextAql(context, TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND, collection->_name);

      return false;
    }
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
/// @brief get the JSON representation of a collection hint
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_GetJsonCollectionHintAql (TRI_aql_collection_hint_t* const hint) {
  TRI_json_t* result;

  if (hint == NULL) {
    return NULL;
  }

  result = TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE);

  if (result == NULL) {
    return NULL;
  }

  if (hint->_index == NULL) {
    // full table scan
    TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE,
                         result,
                         "accessType",
                         TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, "all"));
  }
  else {
    // index usage
    TRI_index_t* idx = hint->_index->_idx;
    TRI_json_t* indexDescription;

    TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE,
                         result,
                         "accessType",
                         TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, "index"));

    indexDescription = TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE);
    if (indexDescription != NULL) {
      TRI_string_buffer_t* buffer;
      char* idString = GetIndexIdString(hint);

      // index id
      if (idString != NULL) {
        TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE,
                             indexDescription,
                             "id",
                             TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, idString));

        TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, idString);
      }

      // index type
      TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE,
                           indexDescription,
                           "type",
                           TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, TRI_TypeNameIndex(idx)));

      // index attributes
      buffer = TRI_CreateStringBuffer(TRI_UNKNOWN_MEM_ZONE);
      if (buffer != NULL) {
        size_t i;

        for (i = 0; i < idx->_fields._length; i++) {
          if (i > 0) {
            TRI_AppendStringStringBuffer(buffer, ", ");
          }
          TRI_AppendStringStringBuffer(buffer, idx->_fields._buffer[i]);
        }

        TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE,
                             indexDescription,
                             "attributes",
                             TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, buffer->_buffer));

        TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);
      }

    }

    TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE,
                         result,
                         "index",
                         indexDescription);
  }

  if (hint->_limit._status == TRI_AQL_LIMIT_USE) {
    TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE,
                         result,
                         "limit",
                         TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, (double) hint->_limit._offset + (double) hint->_limit._limit));
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a collection hint
////////////////////////////////////////////////////////////////////////////////

TRI_aql_collection_hint_t* TRI_CreateCollectionHintAql (void) {
  TRI_aql_collection_hint_t* hint;

  hint = (TRI_aql_collection_hint_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_aql_collection_hint_t), false);

  if (hint == NULL) {
    return NULL;
  }

  hint->_ranges        = NULL;
  hint->_index         = NULL;
  hint->_collection    = NULL;
  hint->_variableName  = NULL;

  // init limit
  hint->_limit._offset = 0;
  hint->_limit._limit  = INT64_MAX;
  hint->_limit._status = TRI_AQL_LIMIT_UNDEFINED;

  return hint;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free a collection hint
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeCollectionHintAql (TRI_aql_collection_hint_t* const hint) {
  assert(hint);

  if (hint->_ranges != NULL) {
    TRI_FreeAccessesAql(hint->_ranges);
  }

  if (hint->_index != NULL) {
    TRI_FreeIndexAql(hint->_index);
  }

  if (hint->_variableName != NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, hint->_variableName);
  }

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, hint);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lookup a collection in the internal vector
////////////////////////////////////////////////////////////////////////////////

TRI_aql_collection_t* TRI_GetCollectionAql (const TRI_aql_context_t* const context,
                                            const char* const collectionName) {
  size_t i, n;

  assert(context);

  n = context->_collections._length;
  for (i = 0; i < n; ++i) {
    TRI_aql_collection_t* col = (TRI_aql_collection_t*) TRI_AtVectorPointer(&context->_collections, i);

    if (TRI_EqualString(col->_name, collectionName)) {
      return col;
    }
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief init all collections
////////////////////////////////////////////////////////////////////////////////

bool TRI_SetupCollectionsAql (TRI_aql_context_t* const context) {
  if (! SetupCollections(context)) {
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a gc marker for all collections used in a query
////////////////////////////////////////////////////////////////////////////////

bool TRI_AddBarrierCollectionsAql (TRI_aql_context_t* const context) {
  size_t i;
  size_t n;
  bool result = true;

  // iterate in forward order
  n = context->_collections._length;
  for (i = 0; i < n; ++i) {
    TRI_barrier_t* ce;

    TRI_aql_collection_t* collection = (TRI_aql_collection_t*) context->_collections._buffer[i];
    TRI_primary_collection_t* primaryCollection;

    assert(collection);
    assert(collection->_name);
    assert(collection->_collection);
    assert(collection->_collection->_collection);
    assert(! collection->_barrier);

    primaryCollection = (TRI_primary_collection_t*) collection->_collection->_collection;

    LOG_TRACE("adding barrier for collection '%s'", collection->_name);

    ce = TRI_CreateBarrierElement(&primaryCollection->_barrierList);
    if (!ce) {
      // couldn't create the barrier
      result = false;
      TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
      break;
    }
    else {
      collection->_barrier = ce;
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes the gc markers for all collections used in a query
////////////////////////////////////////////////////////////////////////////////

void TRI_RemoveBarrierCollectionsAql (TRI_aql_context_t* const context) {
  size_t i;

  // iterate in reverse order
  i = context->_collections._length;
  while (i--) {
    TRI_aql_collection_t* collection = (TRI_aql_collection_t*) context->_collections._buffer[i];

    assert(collection);
    assert(collection->_name);

    if (! collection->_collection || ! collection->_barrier) {
      // don't process collections we weren't able to lock at all
      continue;
    }

    assert(collection->_barrier);
    assert(collection->_collection->_collection);

    LOG_TRACE("removing barrier for collection '%s'", collection->_name);

    TRI_FreeBarrier(collection->_barrier);
    collection->_barrier = NULL;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection name to the list of collections used
////////////////////////////////////////////////////////////////////////////////

bool TRI_AddCollectionAql (TRI_aql_context_t* const context, const char* const name) {
  assert(context);
  assert(name);

  // duplicates are not a problem here, we simply ignore them
  TRI_InsertKeyAssociativePointer(&context->_collectionNames, name, (void*) name, false);

  if (context->_collectionNames._nrUsed > AQL_MAX_COLLECTIONS) {
    TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_TOO_MANY_COLLECTIONS, NULL);

    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

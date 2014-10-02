////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, collection
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Ahuacatl/ahuacatl-collections.h"

#include "Basics/logging.h"
#include "Basics/tri-strings.h"
#include "Basics/vector.h"
#include "VocBase/index.h"
#include "VocBase/document-collection.h"

#include "Ahuacatl/ahuacatl-access-optimiser.h"
#include "Ahuacatl/ahuacatl-context.h"

struct TRI_vector_pointer_s;
struct TRI_vector_pointer_s* TRI_GetCoordinatorIndexes (char const*, char const*);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief free indexes
////////////////////////////////////////////////////////////////////////////////

static void FreeIndexes (TRI_aql_collection_t* collection) {
  if (collection->_availableIndexes == nullptr) {
    return;
  }

  for (size_t i = 0; i < collection->_availableIndexes->_length; ++i) {
    TRI_index_t* idx = (TRI_index_t*) TRI_AtVectorPointer(collection->_availableIndexes, i);

    if (idx != nullptr) {
      TRI_DestroyVectorString(&idx->_fields);
      TRI_Free(TRI_CORE_MEM_ZONE, idx);
    }
  }

  TRI_FreeVectorPointer(TRI_UNKNOWN_MEM_ZONE, collection->_availableIndexes);
  collection->_availableIndexes = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get collection/index id as a string
///
/// The caller must free the result string
////////////////////////////////////////////////////////////////////////////////

static char* GetIndexIdString (TRI_aql_collection_hint_t* const hint) {
  TRI_string_buffer_t buffer;
  char* result;

  TRI_InitStringBuffer(&buffer, TRI_UNKNOWN_MEM_ZONE);
//  TRI_AppendUInt64StringBuffer(&buffer, hint->_collection->_name); // cid);
  TRI_AppendStringStringBuffer(&buffer, hint->_collection->_name);
  TRI_AppendCharStringBuffer(&buffer, '/');
  TRI_AppendUInt64StringBuffer(&buffer, hint->_index->_idx->_iid);

  result = TRI_StealStringBuffer(&buffer);
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

static TRI_aql_collection_t* CreateCollectionContainer (const char* name) {
  TRI_ASSERT(name != nullptr);

  TRI_aql_collection_t* collection = static_cast<TRI_aql_collection_t*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_aql_collection_t), false));

  if (collection == nullptr) {
    return nullptr;
  }

  collection->_name             = (char*) name;
  collection->_collection       = nullptr;
  collection->_availableIndexes = nullptr;

  return collection;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set up the collection names vector and order it
////////////////////////////////////////////////////////////////////////////////

static bool SetupCollections (TRI_aql_context_t* context) {
  if (context->_writeCollection != nullptr) {
    TRI_aql_collection_t* collection = CreateCollectionContainer(context->_writeCollection);

    if (collection == nullptr) {
      TRI_SetErrorContextAql(__FILE__, __LINE__, context, TRI_ERROR_OUT_OF_MEMORY, NULL);
      return false;
    }

    TRI_PushBackVectorPointer(&context->_collections, (void*) collection);
  }

  bool result = true;

  // each collection used is contained once in the assoc. array
  // so we do not have to care about duplicate names here
  size_t const n = context->_collectionNames._nrAlloc;

  for (size_t i = 0; i < n; ++i) {
    char* name = static_cast<char*>(context->_collectionNames._table[i]);

    if (name == nullptr ||
        (context->_writeCollection != nullptr && TRI_EqualString(name, context->_writeCollection))) {
      continue;
    }

    TRI_aql_collection_t* collection = CreateCollectionContainer(name);

    if (collection == nullptr) {
      result = false;
      TRI_SetErrorContextAql(__FILE__, __LINE__, context, TRI_ERROR_OUT_OF_MEMORY, NULL);
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

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

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
                           TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, TRI_TypeNameIndex(idx->_type)));

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
  TRI_ASSERT(hint);

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
/// @brief free a collection
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeCollectionAql (TRI_aql_collection_t* collection) {
  FreeIndexes(collection);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, collection);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lookup a collection in the internal vector
////////////////////////////////////////////////////////////////////////////////

TRI_aql_collection_t* TRI_GetCollectionAql (const TRI_aql_context_t* context,
                                            const char* collectionName) {
  size_t i, n;

  TRI_ASSERT(context != NULL);

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

bool TRI_SetupCollectionsAql (TRI_aql_context_t* context) {
  if (! SetupCollections(context)) {
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection name to the list of collections used
////////////////////////////////////////////////////////////////////////////////

bool TRI_AddCollectionAql (TRI_aql_context_t* context,
                           const char* name) {
  TRI_ASSERT(context != NULL);
  TRI_ASSERT(name != NULL);

  // duplicates are not a problem here, we simply ignore them
  TRI_InsertKeyAssociativePointer(&context->_collectionNames, name, (void*) name, false);

  if (context->_collectionNames._nrUsed > AQL_MAX_COLLECTIONS) {
    TRI_SetErrorContextAql(__FILE__, __LINE__, context, TRI_ERROR_QUERY_TOO_MANY_COLLECTIONS, NULL);

    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get available indexes of a collection
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t* TRI_GetIndexesCollectionAql (TRI_aql_context_t* context,
                                                   TRI_aql_collection_t* collection) {
  if (context->_isCoordinator) {
    if (collection->_availableIndexes == nullptr) {
      collection->_availableIndexes = TRI_GetCoordinatorIndexes(context->_vocbase->_name, collection->_name);
    }

    return collection->_availableIndexes;
  }
  else {
    if (collection->_collection == nullptr) {
      return nullptr;
    }

    TRI_document_collection_t* document = collection->_collection->_collection;
    return &document->_allIndexes;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

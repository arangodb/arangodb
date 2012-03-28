////////////////////////////////////////////////////////////////////////////////
/// @brief Collection locking in queries
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

#include "VocBase/query-locks.h"
#include "VocBase/query-join.h"

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief end usage of a collection
////////////////////////////////////////////////////////////////////////////////

static inline void EndCollectionUsage (TRI_vocbase_t* const vocbase,
                                       TRI_vocbase_col_t* const collection, 
                                       const char* name) {
  
  if (collection != NULL) { 
    LOG_DEBUG("ending usage of collection '%s'", name);

    TRI_ReleaseCollectionVocBase(vocbase, collection);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free the locks for the query
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeLocksQueryInstance (TRI_vocbase_t* const vocbase,
                                 TRI_vector_pointer_t* const locks) {
  size_t i;
  
  for (i = 0; i < locks->_length; i++) {
    TRI_query_instance_lock_t* lock = (TRI_query_instance_lock_t*) locks->_buffer[i];

    assert(lock);
    assert(lock->_collectionName);

    EndCollectionUsage(vocbase, lock->_collection, lock->_collectionName);

    TRI_Free(lock->_collectionName);
    TRI_Free(lock);
  }

  TRI_DestroyVectorPointer(locks);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unlock all collections we already marked as being used
////////////////////////////////////////////////////////////////////////////////

void TRI_UnlockCollectionsQueryInstance (TRI_vocbase_t* const vocbase, 
                                         TRI_vector_pointer_t* const locks) {
  size_t i;
  size_t n = locks->_length;

  for (i = 0; i < n; ++i) {
    TRI_query_instance_lock_t* lock = (TRI_query_instance_lock_t*) locks->_buffer[i];
    if (!lock) {
      continue;
    }

    assert(lock->_collection);
    
    EndCollectionUsage(vocbase, lock->_collection, lock->_collectionName);

    TRI_ReleaseCollectionVocBase(vocbase, lock->_collection);
    lock->_collection = NULL;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set up the collection locks for the query
////////////////////////////////////////////////////////////////////////////////

bool TRI_LockCollectionsQueryInstance (TRI_vocbase_t* const vocbase,
                                       TRI_query_instance_t* const instance,
                                       TRI_vector_pointer_t* const locks) {
  size_t i;

  assert(vocbase);

  for (i = 0; i < instance->_join._length; i++) {
    // iterate over all collections used in query
    TRI_join_part_t* part = (TRI_join_part_t*) instance->_join._buffer[i];
    TRI_query_instance_lock_t* lock;
    bool insert;
    size_t j;

    assert(part);
    assert(part->_collectionName);
    assert(!part->_collection);

    insert = true;
    for (j = 0; j < locks->_length; j++) {
      // iterate over all collections we have already found & locked
      int compareResult;

      lock = (TRI_query_instance_lock_t*) locks->_buffer[j];
      assert(lock);
      assert(lock->_collection);
      assert(lock->_collectionName);

      compareResult = strcmp(part->_collectionName, lock->_collectionName);

      if (compareResult == 0) {
        // we have found a duplicate. no need to insert & lock the collection again
        insert = false;
   
        // still we need to give the join part the collection pointer 
        part->_collection = lock->_collection;
        break;
      }
    }

    if (!insert) {
      continue;
    }

    // allocate a new lock struct
    lock = (TRI_query_instance_lock_t*) TRI_Allocate(sizeof(TRI_query_instance_lock_t));
    if (!lock) {
      TRI_RegisterErrorQueryInstance(instance, TRI_ERROR_OUT_OF_MEMORY, NULL);

      TRI_UnlockCollectionsQueryInstance(vocbase, locks);
      return false;
    }

    lock->_collection     = NULL;
    lock->_collectionName = TRI_DuplicateString(part->_collectionName);
    if (!lock->_collectionName) {
      TRI_RegisterErrorQueryInstance(instance, TRI_ERROR_OUT_OF_MEMORY, NULL);
        
      TRI_UnlockCollectionsQueryInstance(vocbase, locks);
      TRI_Free(lock);
      return false;
    }

    LOG_DEBUG("starting usage of collection '%s'", part->_collectionName);

    // if successful, this will mark the collection as currently being read from
    // (so no one else can delete it while we use it)
    lock->_collection = TRI_UseCollectionByNameVocBase(vocbase, part->_collectionName);
    if (!lock->_collection) {
      // collection does not exist or cannot be opened
      TRI_RegisterErrorQueryInstance(instance, 
                                     TRI_ERROR_QUERY_COLLECTION_NOT_FOUND, 
                                     part->_collectionName);

      TRI_UnlockCollectionsQueryInstance(vocbase, locks);
      TRI_Free(lock);
      return false;
    }

    TRI_InsertVectorPointer(locks, lock, j);
      
    part->_collection = lock->_collection;
  }
  
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read locks all collections of a query
////////////////////////////////////////////////////////////////////////////////

void TRI_ReadLockCollectionsQueryInstance (TRI_query_instance_t* const instance) {
  size_t i;

  for (i = 0; i < instance->_locks->_length; ++i) {
    TRI_query_instance_lock_t* lock = instance->_locks->_buffer[i];
    TRI_doc_collection_t* collection = (TRI_doc_collection_t*) lock->_collection->_collection;

    assert(lock);
    assert(lock->_collection);

    LOG_DEBUG("locking collection '%s'", lock->_collectionName);

    collection->beginRead(collection);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read-unlocks all collections of a query
////////////////////////////////////////////////////////////////////////////////

void TRI_ReadUnlockCollectionsQueryInstance (TRI_query_instance_t* const instance) {
  size_t i;
  
  i = instance->_locks->_length;
  while (i > 0) {
    TRI_query_instance_lock_t* lock;
    TRI_doc_collection_t* collection;
    
    lock = instance->_locks->_buffer[--i];
    assert(lock);
    assert(lock->_collection);
    collection = (TRI_doc_collection_t*) lock->_collection->_collection;

    LOG_DEBUG("unlocking collection '%s'", lock->_collectionName);
    collection->endRead(collection);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a gc marker for all collections
////////////////////////////////////////////////////////////////////////////////

bool TRI_AddCollectionsBarrierQueryInstance (TRI_query_instance_t* const instance,
                                             TRI_query_cursor_t* const cursor) {
  size_t i;

  // note: the same collection might be added multiple times here
  for (i = 0; i < instance->_locks->_length; ++i) {
    TRI_query_instance_lock_t* lock = instance->_locks->_buffer[i];
    TRI_doc_collection_t* collection;
    TRI_barrier_t* ce;

    assert(lock);
    assert(lock->_collection);
    collection = (TRI_doc_collection_t*) lock->_collection->_collection;
    ce = TRI_CreateBarrierElement(&collection->_barrierList);
    if (!ce) {
      TRI_RegisterErrorQueryInstance(instance, TRI_ERROR_OUT_OF_MEMORY, NULL);
      return false;
    }
    TRI_PushBackVectorPointer(&cursor->_containers, ce);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a vector for holding collection locks
////////////////////////////////////////////////////////////////////////////////
  
TRI_vector_pointer_t* TRI_InitLocksQueryInstance (void) {
  TRI_vector_pointer_t* locks = (TRI_vector_pointer_t*) TRI_Allocate(sizeof(TRI_vector_pointer_t));

  if (!locks) {
    return NULL;
  }

  TRI_InitVectorPointer(locks);

  return locks;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

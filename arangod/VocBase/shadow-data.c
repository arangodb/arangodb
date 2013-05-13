////////////////////////////////////////////////////////////////////////////////
/// @brief shadow data storage
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
/// @author Dr. Frank Celler
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "BasicsC/logging.h"

#include "VocBase/shadow-data.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                       SHADOW DATA
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief set the timestamp of a shadow to the current date & time
////////////////////////////////////////////////////////////////////////////////

static inline void UpdateTimestampShadow (TRI_shadow_t* const shadow) {
  shadow->_timestamp = TRI_microtime();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief init a shadow data structure
////////////////////////////////////////////////////////////////////////////////

static TRI_shadow_t* CreateShadow (const void* const data) {
  TRI_shadow_t* shadow = (TRI_shadow_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_shadow_t), false);

  if (shadow == NULL) {
    return NULL;
  }

  shadow->_rc        = 1;
  shadow->_data      = (void*) data;
  shadow->_id        = TRI_NewTickVocBase();
  shadow->_deleted   = false;
  shadow->_type      = SHADOW_TRANSIENT;

  UpdateTimestampShadow(shadow);

  LOG_TRACE("created shadow %p with data ptr %p and id %llu",
            shadow,
            data,
            (unsigned long long) shadow->_id);

  return shadow;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief decrease the refcount for a shadow
////////////////////////////////////////////////////////////////////////////////

static void DecreaseRefCount (TRI_shadow_store_t* const store, TRI_shadow_t* const shadow) {
  LOG_TRACE("decreasing refcount for shadow %p with data ptr %p and id %llu to %d",
            shadow,
            shadow->_data,
            (unsigned long long) shadow->_id,
            (int) (shadow->_rc - 1));

  if (--shadow->_rc <= 0 && shadow->_type == SHADOW_TRANSIENT) {
    LOG_TRACE("deleting transient shadow %p", shadow);

    TRI_RemoveKeyAssociativePointer(&store->_ids, &shadow->_id);
    TRI_RemoveKeyAssociativePointer(&store->_pointers, shadow->_data);
    store->destroyShadow(shadow->_data);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, shadow);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief increase the refcount for a shadow
////////////////////////////////////////////////////////////////////////////////

static void IncreaseRefCount (TRI_shadow_store_t* const store, TRI_shadow_t* const shadow) {
  LOG_TRACE("increasing refcount for shadow %p with data ptr %p and id %llu to %d",
            shadow,
            shadow->_data,
            (unsigned long long) shadow->_id,
            (int) (shadow->_rc + 1));

  if (++shadow->_rc <= 0) {
    // should not be less or equal to 0 now
    shadow->_rc = 1;
  }
  UpdateTimestampShadow(shadow);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the persistence flag for a shadow
////////////////////////////////////////////////////////////////////////////////

static void PersistShadow (TRI_shadow_t* const shadow) {
  LOG_TRACE("persisting shadow %p with data ptr %p and id %llu",
            shadow,
            shadow->_data,
            (unsigned long long) shadow->_id);

  shadow->_type = SHADOW_PERSISTENT;
  UpdateTimestampShadow(shadow);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the deleted flag for a shadow
////////////////////////////////////////////////////////////////////////////////

static void DeleteShadow (TRI_shadow_store_t* const store,
                          TRI_shadow_t* const shadow,
                          const bool decreaseRefCount) {
  LOG_TRACE("setting deleted flag for shadow %p with data ptr %p and id %llu",
            shadow,
            shadow->_data,
            (unsigned long long) shadow->_id);

  shadow->_deleted = true;
  if (decreaseRefCount) {
    DecreaseRefCount(store, shadow);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes an element in the ids index
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashKeyId (TRI_associative_pointer_t* array, void const* k) {
  TRI_shadow_id key = *((TRI_shadow_id*) k);

  return (uint64_t) key;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes an element in the ids index
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementId (TRI_associative_pointer_t* array, void const* e) {
  TRI_shadow_t const* element = e;

  return (uint64_t) element->_id;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tests if two elements are equal
////////////////////////////////////////////////////////////////////////////////

static bool EqualKeyId (TRI_associative_pointer_t* array, void const* k, void const* e) {
  TRI_shadow_t const* element = e;
  TRI_shadow_id key = *((TRI_shadow_id*) k);

  return (key == element->_id);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes an element in the pointers index
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashKeyData (TRI_associative_pointer_t* array, void const* k) {
  uint64_t key;

  key = (uint64_t) (uintptr_t) k;
  return key;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes an element in the pointers index
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementData (TRI_associative_pointer_t* array, void const* e) {
  TRI_shadow_t const* element = e;

  return (uint64_t) (uintptr_t) element->_data;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tests if two elements are equal
////////////////////////////////////////////////////////////////////////////////

static bool EqualKeyData (TRI_associative_pointer_t* array, void const* k, void const* e) {
  TRI_shadow_t const* element = e;

  return ((uint64_t) (uintptr_t) k == (uint64_t) (uintptr_t) element->_data);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a shadow data storage
////////////////////////////////////////////////////////////////////////////////

TRI_shadow_store_t* TRI_CreateShadowStore (void (*destroy) (void*)) {
  TRI_shadow_store_t* store;

  store = (TRI_shadow_store_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_shadow_store_t), false);

  if (store != NULL) {
    TRI_InitAssociativePointer(&store->_ids,
                               TRI_UNKNOWN_MEM_ZONE,
                               HashKeyId,
                               HashElementId,
                               EqualKeyId,
                               NULL);

    TRI_InitAssociativePointer(&store->_pointers,
                               TRI_UNKNOWN_MEM_ZONE,
                               HashKeyData,
                               HashElementData,
                               EqualKeyData,
                               NULL);

    store->destroyShadow = destroy;

    TRI_InitMutex(&store->_lock);
  }

  return store;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a shadow data storage
///
/// Note: all remaining shadows will be destroyed
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeShadowStore (TRI_shadow_store_t* const store) {
  assert(store);

  // force deletion of all remaining shadows
  TRI_CleanupShadowData(store, 0.0, true);

  TRI_DestroyMutex(&store->_lock);
  TRI_DestroyAssociativePointer(&store->_ids);
  TRI_DestroyAssociativePointer(&store->_pointers);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, store);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief look up a shadow in the index using its data pointer and return
/// its id
////////////////////////////////////////////////////////////////////////////////

TRI_shadow_id TRI_GetIdDataShadowData (TRI_shadow_store_t* const store,
                                       const void* const data) {
  TRI_shadow_t* shadow;
  TRI_shadow_id id = 0;

  assert(store);

  if (data) {
    TRI_LockMutex(&store->_lock);
    shadow = (TRI_shadow_t*) TRI_LookupByKeyAssociativePointer(&store->_pointers, data);

    if (shadow && ! shadow->_deleted) {
      id = shadow->_id;
      UpdateTimestampShadow(shadow);
    }

    TRI_UnlockMutex(&store->_lock);
  }

  return id;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief look up a shadow in the index using its data pointer
///
/// If the shadow is found, this will return the data pointer, NULL otherwise.
/// When the shadow is found, its refcount will also be increased by one
////////////////////////////////////////////////////////////////////////////////

void* TRI_BeginUsageDataShadowData (TRI_shadow_store_t* const store,
                                    const void* const data) {
  TRI_shadow_t* shadow;

  assert(store);

  if (! data) {
    return NULL;
  }

  TRI_LockMutex(&store->_lock);
  shadow = (TRI_shadow_t*) TRI_LookupByKeyAssociativePointer(&store->_pointers, data);

  if (shadow && ! shadow->_deleted) {
    IncreaseRefCount(store, shadow);
    TRI_UnlockMutex(&store->_lock);
    return shadow->_data;
  }

  TRI_UnlockMutex(&store->_lock);
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief look up a shadow in the index using its id
///
/// If the shadow is found, this will return the data pointer, NULL otherwise.
/// When the shadow is found, its refcount will also be increased by one
////////////////////////////////////////////////////////////////////////////////

void* TRI_BeginUsageIdShadowData (TRI_shadow_store_t* const store,
                                  const TRI_shadow_id id) {
  TRI_shadow_t* shadow;

  assert(store);

  TRI_LockMutex(&store->_lock);
  shadow = (TRI_shadow_t*) TRI_LookupByKeyAssociativePointer(&store->_ids, (void const*) &id);

  if (shadow && ! shadow->_deleted) {
    IncreaseRefCount(store, shadow);
    TRI_UnlockMutex(&store->_lock);
    return shadow->_data;
  }

  TRI_UnlockMutex(&store->_lock);
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief look up a shadow in the index using its data pointer
///
/// If the shadow is found, its refcount will be decreased by one.
/// If the refcount is 0 and the shadow is of type SHADOW_TRANSIENT, the shadow
/// object will be destroyed.
////////////////////////////////////////////////////////////////////////////////

void TRI_EndUsageDataShadowData (TRI_shadow_store_t* const store,
                                 const void* const data) {
  TRI_shadow_t* shadow;

  assert(store);

  TRI_LockMutex(&store->_lock);
  shadow = (TRI_shadow_t*) TRI_LookupByKeyAssociativePointer(&store->_pointers, data);

  if (shadow && ! shadow->_deleted) {
    DecreaseRefCount(store, shadow); // this might delete the shadow
  }

  TRI_UnlockMutex(&store->_lock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the persistence flag for a shadow using its data pointer
////////////////////////////////////////////////////////////////////////////////

bool TRI_PersistDataShadowData (TRI_shadow_store_t* const store,
                                const void* const data) {
  TRI_shadow_t* shadow;
  bool result = false;

  assert(store);

  TRI_LockMutex(&store->_lock);
  shadow = (TRI_shadow_t*) TRI_LookupByKeyAssociativePointer(&store->_pointers, data);

  if (shadow && ! shadow->_deleted) {
    PersistShadow(shadow);
    result = true;
  }

  TRI_UnlockMutex(&store->_lock);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the deleted flag for a shadow using its data pointer
////////////////////////////////////////////////////////////////////////////////

bool TRI_DeleteDataShadowData (TRI_shadow_store_t* const store,
                               const void* const data) {
  bool found = false;

  assert(store);

  if (data) {
    TRI_shadow_t* shadow;

    TRI_LockMutex(&store->_lock);
    shadow = (TRI_shadow_t*) TRI_LookupByKeyAssociativePointer(&store->_pointers, data);

    if (shadow && ! shadow->_deleted) {
      DeleteShadow(store, shadow, true);
      found = true;
    }

    TRI_UnlockMutex(&store->_lock);
  }

  return found;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the deleted flag for a shadow using its id
////////////////////////////////////////////////////////////////////////////////

bool TRI_DeleteIdShadowData (TRI_shadow_store_t* const store,
                             const TRI_shadow_id id) {
  TRI_shadow_t* shadow;
  bool found = false;

  assert(store);

  TRI_LockMutex(&store->_lock);
  shadow = (TRI_shadow_t*) TRI_LookupByKeyAssociativePointer(&store->_ids, &id);

  if (shadow && ! shadow->_deleted) {
    DeleteShadow(store, shadow, false);
    found = true;
  }

  TRI_UnlockMutex(&store->_lock);

  return found;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief enumerate all shadows and remove them if
/// - their refcount is 0 and they are transient
/// - their refcount is 0 and they are expired
/// - the force flag is set
///
/// The max age must be specified in seconds. The max age is ignored if the
/// force flag is set. In this case all remaining shadows will be deleted
////////////////////////////////////////////////////////////////////////////////

void TRI_CleanupShadowData (TRI_shadow_store_t* const store,
                            const double maxAge,
                            const bool force) {
  double compareStamp = TRI_microtime() - maxAge; // age must be specified in secs
  size_t deleteCount = 0;

  // we need an exclusive lock on the index
  TRI_LockMutex(&store->_lock);

  if (store->_ids._nrUsed == 0) {
    // store is empty, nothing to do!
    TRI_UnlockMutex(&store->_lock);
    return;
  }

  LOG_TRACE("cleaning shadows. in store: %ld", (unsigned long) store->_ids._nrUsed);

  // loop until there's nothing to delete or
  // we have deleted SHADOW_MAX_DELETE elements
  while (deleteCount++ < SHADOW_MAX_DELETE || force) {
    bool deleted = false;
    size_t i;

    for (i = 0; i < store->_ids._nrAlloc; i++) {
      // enum all shadows
      TRI_shadow_t* shadow = (TRI_shadow_t*) store->_ids._table[i];

      if (shadow == NULL) {
        continue;
      }
          
      // check if shadow is unused and expired
      if (shadow->_rc < 1 || force) {
        if (shadow->_type == SHADOW_TRANSIENT ||
            shadow->_timestamp < compareStamp ||
            shadow->_deleted ||
            force) {
          LOG_TRACE("cleaning shadow %p, id: %llu, rc: %d, expired: %d, deleted: %d",
                    shadow,
                    (unsigned long long) shadow->_id,
                    (int) shadow->_rc,
                    (int) (shadow->_timestamp < compareStamp),
                    (int) shadow->_deleted);

          TRI_RemoveKeyAssociativePointer(&store->_ids, &shadow->_id);
          TRI_RemoveKeyAssociativePointer(&store->_pointers, shadow->_data);
          store->destroyShadow(shadow->_data);
          TRI_Free(TRI_UNKNOWN_MEM_ZONE, shadow);

          deleted = true;
          // the remove might reposition elements in the container.
          // therefore break here and start iteration anew
          break;
        }
      }
    }

    if (! deleted) {
      // we did not find anything to delete, so give up
      break;
    }
  }

  // release lock
  TRI_UnlockMutex(&store->_lock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief store a new shadow in the store
////////////////////////////////////////////////////////////////////////////////

TRI_shadow_t* TRI_StoreShadowData (TRI_shadow_store_t* const store,
                                   const void* const data) {
  TRI_shadow_t* shadow;

  assert(store);

  shadow = CreateShadow(data);
  if (shadow) {
    LOG_TRACE("storing shadow %p with data ptr %p and id %llu",
            shadow,
            shadow->_data,
            (unsigned long long) shadow->_id);

    TRI_LockMutex(&store->_lock);
    if (TRI_InsertKeyAssociativePointer(&store->_ids, &shadow->_id, shadow, false)) {
      // duplicate entry
      LOG_WARNING("storing shadow failed");

      TRI_UnlockMutex(&store->_lock);
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, shadow);
      return NULL;
    }

    TRI_InsertKeyAssociativePointer(&store->_pointers, data, shadow, false);

    TRI_UnlockMutex(&store->_lock);
  }

  // might be NULL in case of OOM
  return shadow;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

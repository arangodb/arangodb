////////////////////////////////////////////////////////////////////////////////
/// @brief associative multi array implementation
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
/// @author Martin Schoenert
/// @author Copyright 2006-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "associative-multi.h"
#include "BasicsC/prime-numbers.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 ASSOCIATIVE ARRAY
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                   private defines
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Collections
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initial number of elements of a container
////////////////////////////////////////////////////////////////////////////////

#define INITIAL_SIZE (64)

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Collections
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new element
///
/// Note: no out-of-memory possible.
////////////////////////////////////////////////////////////////////////////////

static void AddNewElement (TRI_multi_array_t* array, void* element) {
  uint64_t hash;
  uint64_t i;

  // compute the hash
  hash = array->hashElement(array, element);

  // search the table
  i = hash % array->_nrAlloc;

  while (! array->isEmptyElement(array, array->_table + i * array->_elementSize)) {
    i = TRI_IncModU64(i, array->_nrAlloc);
#ifdef TRI_INTERNAL_STATS
    array->_nrProbesR++;
#endif
  }

  // add a new element to the associative array
  memcpy(array->_table + i * array->_elementSize, element, array->_elementSize);
  array->_nrUsed++;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief resizes the array
////////////////////////////////////////////////////////////////////////////////

static void ResizeMultiArray (TRI_multi_array_t* array) {
  char* oldTable;
  uint64_t oldAlloc;
  uint64_t j;

  oldTable = array->_table;
  oldAlloc = array->_nrAlloc;

  array->_nrAlloc = 2 * array->_nrAlloc + 1;

  array->_table = TRI_Allocate(array->_memoryZone, array->_nrAlloc * array->_elementSize, true);

  if (array->_table == NULL) {
    array->_nrAlloc = oldAlloc;
    array->_table = oldTable;

    return;
  }

  array->_nrUsed = 0;
#ifdef TRI_INTERNAL_STATS
  array->_nrResizes++;
#endif

  for (j = 0; j < oldAlloc; j++) {
    if (! array->isEmptyElement(array, oldTable + j * array->_elementSize)) {
      AddNewElement(array, oldTable + j * array->_elementSize);
    }
  }

  TRI_Free(array->_memoryZone, oldTable);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Collections
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises an array
////////////////////////////////////////////////////////////////////////////////

int TRI_InitMultiArray(TRI_multi_array_t* array,
                       TRI_memory_zone_t* zone,
                       size_t elementSize,
                       uint64_t (*hashKey) (TRI_multi_array_t*, void*),
                       uint64_t (*hashElement) (TRI_multi_array_t*, void*),
                       void (*clearElement) (TRI_multi_array_t*, void*),
                       bool (*isEmptyElement) (TRI_multi_array_t*, void*),
                       bool (*isEqualKeyElement) (TRI_multi_array_t*, void*, void*),
                       bool (*isEqualElementElement) (TRI_multi_array_t*, void*, void*)) {

  array->hashKey = hashKey;
  array->hashElement = hashElement;
  array->clearElement = clearElement;
  array->isEmptyElement = isEmptyElement;
  array->isEqualKeyElement = isEqualKeyElement;
  array->isEqualElementElement = isEqualElementElement;

  array->_memoryZone = zone;
  array->_elementSize = elementSize;
  array->_nrAlloc = 0;
  array->_nrUsed = 0;

  if (NULL == (array->_table = TRI_Allocate(array->_memoryZone, array->_elementSize * INITIAL_SIZE, true))) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  array->_nrAlloc = INITIAL_SIZE;

#ifdef TRI_INTERNAL_STATS
  array->_nrFinds = 0;
  array->_nrAdds = 0;
  array->_nrRems = 0;
  array->_nrResizes = 0;
  array->_nrProbesF = 0;
  array->_nrProbesA = 0;
  array->_nrProbesD = 0;
  array->_nrProbesR = 0;
#endif

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an array, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyMultiArray (TRI_multi_array_t* array) {
  if (array->_table != NULL) {
    TRI_Free(array->_memoryZone, array->_table);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an array and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeMultiArray (TRI_memory_zone_t* zone, TRI_multi_array_t* array) {
  TRI_DestroyMultiArray(array);
  TRI_Free(zone, array);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Collections
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given a key
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t TRI_LookupByKeyMultiArray (TRI_memory_zone_t* zone,
                                                TRI_multi_array_t* array,
                                                void* key) {
  TRI_vector_pointer_t result;
  uint64_t hash;
  uint64_t i;

  // initialise the vector which will hold the result if any
  TRI_InitVectorPointer(&result, zone);

  // compute the hash
  hash = array->hashKey(array, key);
  i = hash % array->_nrAlloc;

#ifdef TRI_INTERNAL_STATS
  // update statistics
  array->_nrFinds++;
#endif

  // search the table
  while (! array->isEmptyElement(array, array->_table + i * array->_elementSize)) {

    if (array->isEqualKeyElement(array, key, array->_table + i * array->_elementSize)) {
      TRI_PushBackVectorPointer(&result, array->_table + i * array->_elementSize);
    }
#ifdef TRI_INTERNAL_STATS
    else {
      array->_nrProbesF++;
    }
#endif

    i = TRI_IncModU64(i, array->_nrAlloc);
  }

  // ...........................................................................
  // return whatever we found -- which could be an empty vector list if nothing
  // matches. If an out-of-memory occurred than the zone will have a suitable
  // marker.
  // ...........................................................................

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given an element
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t TRI_LookupByElementMultiArray (TRI_memory_zone_t* zone,
                                                    TRI_multi_array_t* array,
                                                    void* element) {
  TRI_vector_pointer_t result;
  uint64_t hash;
  uint64_t i;

  // initialise the vector which will hold the result if any
  TRI_InitVectorPointer(&result, zone);

  // compute the hash
  hash = array->hashElement(array, element);
  i = hash % array->_nrAlloc;

#ifdef TRI_INTERNAL_STATS
  // update statistics
  array->_nrFinds++;
#endif

  // search the table
  while (! array->isEmptyElement(array, array->_table + i * array->_elementSize)) {
    if (array->isEqualElementElement(array, element, array->_table + i * array->_elementSize)) {
      TRI_PushBackVectorPointer(&result, array->_table + i * array->_elementSize);
    }
#ifdef TRI_INTERNAL_STATS
    else {
      array->_nrProbesF++;
    }
#endif

    i = TRI_IncModU64(i, array->_nrAlloc);
  }

  // ...........................................................................
  // return whatever we found -- which could be an empty vector list if nothing
  // matches. Note that we allow multiple elements (compare with pointer
  // impl). If an out-of-memory occurred than the zone will have a suitable
  // marker.
  // ...........................................................................

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element to the array
////////////////////////////////////////////////////////////////////////////////

bool TRI_InsertElementMultiArray (TRI_multi_array_t* array, void* element, bool overwrite) {
  uint64_t hash;
  uint64_t i;

  // check for out-of-memory
  if (array->_nrAlloc == array->_nrUsed) {
    return false;
  }

  // compute the hash
  hash = array->hashElement(array, element);
  i = hash % array->_nrAlloc;

#ifdef TRI_INTERNAL_STATS
  // update statistics
  array->_nrAdds++;
#endif

  // search the table
  while (! array->isEmptyElement(array, array->_table + i * array->_elementSize)
         && ! array->isEqualElementElement(array, element, array->_table + i * array->_elementSize)) {
    i = TRI_IncModU64(i, array->_nrAlloc);
#ifdef TRI_INTERNAL_STATS
    array->_nrProbesA++;
#endif
  }

  // ...........................................................................
  // If we found an element, return. While we allow duplicate entries in the hash
  // table, we do not allow duplicate elements. Elements would refer to the
  // (for example) an actual row in memory. This is different from the
  // TRI_InsertElementMultiArray function below where we only have keys to
  // differentiate between elements.
  // ...........................................................................

  if (! array->isEmptyElement(array, array->_table + i * array->_elementSize)) {
    if (overwrite) {
      memcpy(array->_table + i * array->_elementSize, element, array->_elementSize);
    }

    return false;
  }

  // add a new element to the associative array
  memcpy(array->_table + i * array->_elementSize, element, array->_elementSize);
  array->_nrUsed++;

  // if we were adding and the table is more than half full, extend it
  if (array->_nrAlloc < 2 * array->_nrUsed) {
    ResizeMultiArray(array);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an key/element to the array
////////////////////////////////////////////////////////////////////////////////

bool TRI_InsertKeyMultiArray (TRI_multi_array_t* array, void* key, void* element, bool overwrite) {
  uint64_t hash;
  uint64_t i;

  // check for out-of-memory
  if (array->_nrAlloc == array->_nrUsed) {
    return false;
  }

  // compute the hash
  hash = array->hashKey(array, key);
  i = hash % array->_nrAlloc;

#ifdef TRI_INTERNAL_STATS
  // update statistics
  array->_nrAdds++;
#endif

  // search the table
  while (! array->isEmptyElement(array, array->_table + i * array->_elementSize)) {
    i = TRI_IncModU64(i, array->_nrAlloc);
#ifdef TRI_INTERNAL_STATS
    array->_nrProbesA++;
#endif
  }

  // ...........................................................................
  // We do not look for an element (as opposed to the function above). So whether
  // or not there exists a duplicate we do not care.
  // ...........................................................................

  // add a new element to the associative array
  memcpy(array->_table + i * array->_elementSize, element, array->_elementSize);
  array->_nrUsed++;

  // if we were adding and the table is more than half full, extend it
  if (array->_nrAlloc < 2 * array->_nrUsed) {
    ResizeMultiArray(array);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from the array
////////////////////////////////////////////////////////////////////////////////

bool TRI_RemoveElementMultiArray (TRI_multi_array_t* array, void* element, void* old) {
  uint64_t hash;
  uint64_t i;
  uint64_t k;

  // Obtain the hash
  hash = array->hashElement(array, element);
  i = hash % array->_nrAlloc;

#ifdef TRI_INTERNAL_STATS
  // update statistics
  array->_nrRems++;
#endif

  // search the table
  while (! array->isEmptyElement(array, array->_table + i * array->_elementSize)
         && ! array->isEqualElementElement(array, element, array->_table + i * array->_elementSize)) {
    i = TRI_IncModU64(i, array->_nrAlloc);
#ifdef TRI_INTERNAL_STATS
    array->_nrProbesD++;
#endif
  }

  // if we did not find such an item return false
  if (array->isEmptyElement(array, array->_table + i * array->_elementSize)) {
    if (old != NULL) {
      memset(old, 0, array->_elementSize);
    }

    return false;
  }

  // remove item
  if (old != NULL) {
    memcpy(old, array->_table + i * array->_elementSize, array->_elementSize);
  }

  array->clearElement(array, array->_table + i * array->_elementSize);
  array->_nrUsed--;

  // and now check the following places for items to move here
  k = TRI_IncModU64(i, array->_nrAlloc);

  while (! array->isEmptyElement(array, array->_table + k * array->_elementSize)) {
    uint64_t j = array->hashElement(array, array->_table + k * array->_elementSize) % array->_nrAlloc;

    if ((i < k && !(i < j && j <= k)) || (k < i && !(i < j || j <= k))) {
      memcpy(array->_table + i * array->_elementSize, array->_table + k * array->_elementSize, array->_elementSize);
      array->clearElement(array, array->_table + k * array->_elementSize);
      i = k;
    }

    k = TRI_IncModU64(k, array->_nrAlloc);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an key/element to the array
////////////////////////////////////////////////////////////////////////////////

bool TRI_RemoveKeyMultiArray (TRI_multi_array_t* array, void* key, void* old) {
  uint64_t hash;
  uint64_t i;
  uint64_t k;

  // generate hash using key
  hash = array->hashKey(array, key);
  i = hash % array->_nrAlloc;

#ifdef TRI_INTERNAL_STATS
  // update statistics
  array->_nrRems++;
#endif

  // search the table -- we can only match keys
  while (! array->isEmptyElement(array, array->_table + i * array->_elementSize)
         && ! array->isEqualKeyElement(array, key, array->_table + i * array->_elementSize)) {
    i = TRI_IncModU64(i, array->_nrAlloc);
#ifdef TRI_INTERNAL_STATS
    array->_nrProbesD++;
#endif
  }

  // if we did not find such an item return false
  if (array->isEmptyElement(array, array->_table + i * array->_elementSize)) {
    if (old != NULL) {
      memset(old, 0, array->_elementSize);
    }

    return false;
  }

  // remove item
  if (old != NULL) {
    memcpy(old, array->_table + i * array->_elementSize, array->_elementSize);
  }

  array->clearElement(array, array->_table + i * array->_elementSize);
  array->_nrUsed--;

  // and now check the following places for items to move here
  k = TRI_IncModU64(i, array->_nrAlloc);

  while (! array->isEmptyElement(array, array->_table + k * array->_elementSize)) {
    uint64_t j = array->hashElement(array, array->_table + k * array->_elementSize) % array->_nrAlloc;

    if ((i < k && !(i < j && j <= k)) || (k < i && !(i < j || j <= k))) {
      memcpy(array->_table + i * array->_elementSize, array->_table + k * array->_elementSize, array->_elementSize);
      array->clearElement(array, array->_table + k * array->_elementSize);
      i = k;
    }

    k = TRI_IncModU64(k, array->_nrAlloc);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                              ASSOCIATIVE POINTERS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Collections
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises an array
////////////////////////////////////////////////////////////////////////////////

int TRI_InitMultiPointer (TRI_multi_pointer_t* array,
                          TRI_memory_zone_t* zone,
                          uint64_t (*hashKey) (TRI_multi_pointer_t*, 
                                               void const*),
                          uint64_t (*hashElement) (TRI_multi_pointer_t*, 
                                                   void const*, bool),
                          bool (*isEqualKeyElement) (TRI_multi_pointer_t*, 
                                                     void const*, void const*),
                          bool (*isEqualElementElement) (TRI_multi_pointer_t*, 
                                                         void const*, 
                                                         void const*, bool)) {
  array->hashKey = hashKey;
  array->hashElement = hashElement;
  array->isEqualKeyElement = isEqualKeyElement;
  array->isEqualElementElement = isEqualElementElement;

  array->_memoryZone = zone;
  array->_nrUsed  = 0;
  array->_nrAlloc = 0;

  if (NULL == (array->_table = TRI_Allocate(zone, 
                 sizeof(TRI_multi_pointer_entry_t) * INITIAL_SIZE, true))) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  array->_nrAlloc = INITIAL_SIZE;

#ifdef TRI_INTERNAL_STATS
  array->_nrFinds = 0;
  array->_nrAdds = 0;
  array->_nrRems = 0;
  array->_nrResizes = 0;
  array->_nrProbesF = 0;
  array->_nrProbesA = 0;
  array->_nrProbesD = 0;
  array->_nrProbesR = 0;
#endif

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an array, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyMultiPointer (TRI_multi_pointer_t* array) {
  if (array->_table != NULL) {
    TRI_Free(array->_memoryZone, array->_table);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an array and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeMultiPointer (TRI_memory_zone_t* zone, TRI_multi_pointer_t* array) {
  TRI_DestroyMultiPointer(array);
  TRI_Free(zone, array);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

uint64_t ALL_HASH_ADDS = 0;
uint64_t ALL_HASH_COLLS = 0;

static inline uint64_t FindElementPlace (TRI_multi_pointer_t* array,
                                         void* element,
                                         bool checkEquality) {
  // This either finds a place to store element or an entry in the table
  // that is equal to element. If checkEquality is set to false, the caller
  // guarantees that there is no entry that compares equal to element
  // in the table, which saves a lot of element comparisons. This function
  // always returns a pointer into the table, which is either empty or 
  // points to an entry that compares equal to element.

  uint64_t hash;
  uint64_t i;

  hash = array->hashElement(array, element, false);
  i = hash % array->_nrAlloc;

  while (array->_table[i].ptr != NULL && 
         (! checkEquality || 
          ! array->isEqualElementElement(array, element, 
                                                array->_table[i].ptr, false))) {
    i = TRI_IncModU64(i, array->_nrAlloc);
    ALL_HASH_COLLS++;
#ifdef TRI_INTERNAL_STATS
    array->_nrProbesA++;
#endif
  }
  return i;
}


// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Collections
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a key/element to the array
////////////////////////////////////////////////////////////////////////////////

void* TRI_InsertElementMultiPointer (TRI_multi_pointer_t* array,
                                     void* element,
                                     const bool overwrite,
                                     const bool checkEquality) {

  // if the checkEquality flag is not set, we do not check for element
  // equality we use this flag to speed up initial insertion into the
  // index, i.e. when the index is built for a collection and we know
  // for sure no duplicate elements will be inserted

  uint64_t hash;
  uint64_t i, j, k;
  void* old;

  ALL_HASH_ADDS++;

#ifdef TRI_INTERNAL_STATS
  // update statistics
  array->_nrAdds++;
#endif

  // compute the hash by the key only first
  hash = array->hashElement(array, element, true);
  i = hash % array->_nrAlloc;

  // If this slot is free, just use it:
  if (NULL == array->_table[i].ptr) {
    array->_table[i].ptr = element;
    array->_table[i].next = NULL;
    array->_table[i].prev = NULL;
    return NULL;
  }

  // Now find the first slot with an entry with the same key that is the
  // start of a linked list, or a free slot:
  while (array->_table[i].ptr != NULL &&
         (! array->isEqualElementElement(array, element, 
                                                array->_table[i].ptr,true) ||
          array->_table[i].prev != NULL)) {
    i = TRI_IncModU64(i, array->_nrAlloc);
    ALL_HASH_COLLS++;
  }

  // If this is free, we are the first with this key:
  if (NULL == array->_table[i].ptr) {
    array->_table[i].ptr = element;
    array->_table[i].next = NULL;
    array->_table[i].prev = NULL;
    return NULL;
  }

  // Otherwise, entry i points to the beginning of the linked list of which 
  // we want to make element a member. Perhaps an equal element is right here:
  if (checkEquality && array->isEqualElementElement(array, element,
                                                    array->_table[i].ptr,
                                                    false)) {
    old = array->_table[i].ptr;
    if (overwrite) {
      array->_table[i].ptr = element;
    }
    return old;
  }

  // Now find a new home for element in this linked list:
  j = FindElementPlace(array, element, checkEquality);

  old = array->_table[j].ptr;

  // if we found an element, return
  if (old != NULL) {
    if (overwrite) {
      array->_table[j].ptr = element;
    }
    return old;
  }

  // add a new element to the associative array and linked list (in pos 2):
  array->_table[j].ptr = element;
  array->_table[j].next = array->_table[i].next;
  array->_table[j].prev = array->_table[i].ptr;
  array->_table[i].next = element;
  // Finally, we need to find the successor to patch it up:
  if (array->_table[j].next != NULL) {
    k = FindElementPlace(array, array->_table[j].next, true);
    array->_table[k].prev = element;
  }
  array->_nrUsed++;

  // if we were adding and the table is more than half full, extend it
  if (array->_nrAlloc < 2 * array->_nrUsed) {
    TRI_ResizeMultiPointer(array, 2 * array->_nrAlloc + 1);
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given a key
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t TRI_LookupByKeyMultiPointer (TRI_memory_zone_t* zone,
                                                  TRI_multi_pointer_t* array,
                                                  void const* key) {
  TRI_vector_pointer_t result;
#if 0
  uint64_t hash;
  uint64_t i;

  // initialises the result vector
  TRI_InitVectorPointer(&result, zone);

  // compute the hash
  hash = array->hashKey(array, key);
  i = hash % array->_nrAlloc;

#ifdef TRI_INTERNAL_STATS
  // update statistics
  array->_nrFinds++;
#endif

  // search the table
  while (array->_table[i].ptr != NULL) {
    if (array->isEqualKeyElement(array, key, array->_table[i])) {
      TRI_PushBackVectorPointer(&result, array->_table[i]);
    }
#ifdef TRI_INTERNAL_STATS
    else {
      array->_nrProbesF++;
    }
#endif
    i = TRI_IncModU64(i, array->_nrAlloc);
  }

#endif
  // return whatever we found
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given an element
////////////////////////////////////////////////////////////////////////////////

void* TRI_LookupByElementMultiPointer (TRI_multi_pointer_t* array, void const* element) {
#if 0
  uint64_t hash;
  uint64_t i;

  // compute the hash
  hash = array->hashElement(array, element);
  i = hash % array->_nrAlloc;

#ifdef TRI_INTERNAL_STATS
  // update statistics
  array->_nrFinds++;
#endif

  // search the table
  while (array->_table[i] != NULL && ! array->isEqualElementElement(array, element, array->_table[i])) {
    i = TRI_IncModU64(i, array->_nrAlloc);
#ifdef TRI_INTERNAL_STATS
    array->_nrProbesF++;
#endif
  }
  // return whatever we found
  return array->_table[i];
#endif
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from the array
////////////////////////////////////////////////////////////////////////////////

void* TRI_RemoveElementMultiPointer (TRI_multi_pointer_t* array, void const* element) {
#if 0
  uint64_t hash;
  uint64_t i;
  uint64_t k;
  void* old;

  hash = array->hashElement(array, element);
  i = hash % array->_nrAlloc;

#ifdef TRI_INTERNAL_STATS
  // update statistics
  array->_nrRems++;
#endif

  // search the table
  while (array->_table[i] != NULL && ! array->isEqualElementElement(array, element, array->_table[i])) {
    i = TRI_IncModU64(i, array->_nrAlloc);
#ifdef TRI_INTERNAL_STATS
    array->_nrProbesD++;
#endif
  }

  // if we did not find such an item return 0
  if (array->_table[i] == NULL) {
    return NULL;
  }

  // remove item
  old = array->_table[i];
  array->_table[i] = NULL;
  array->_nrUsed--;

  // and now check the following places for items to move here
  k = TRI_IncModU64(i, array->_nrAlloc);

  while (array->_table[k] != NULL) {
    uint64_t j = array->hashElement(array, array->_table[k]) % array->_nrAlloc;

    if ((i < k && !(i < j && j <= k)) || (k < i && !(i < j || j <= k))) {
      array->_table[i] = array->_table[k];
      array->_table[k] = NULL;
      i = k;
    }

    k = TRI_IncModU64(k, array->_nrAlloc);
  }

  // return success
  return old;
#endif
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief resize the array
////////////////////////////////////////////////////////////////////////////////

int TRI_ResizeMultiPointer (TRI_multi_pointer_t* array, size_t size) {
  TRI_multi_pointer_entry_t* oldTable;
  uint64_t oldAlloc;
  uint64_t j;

  if (size < 2*array->_nrUsed) {
    return TRI_ERROR_BAD_PARAMETER;
  }

  oldTable = array->_table;
  oldAlloc = array->_nrAlloc;

  array->_nrAlloc = TRI_NextPrime((uint64_t) size*2);
  array->_table = TRI_Allocate(array->_memoryZone, 
                 array->_nrAlloc * sizeof(TRI_multi_pointer_t), true);

  if (array->_table == NULL) {
    array->_nrAlloc = oldAlloc;
    array->_table = oldTable;

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  array->_nrUsed = 0;
#ifdef TRI_INTERNAL_STATS
  array->_nrResizes++;
#endif

  // table is already clear by allocate, copy old data
  for (j = 0; j < oldAlloc; j++) {
    if (oldTable[j].ptr != NULL) {
      TRI_InsertElementMultiPointer(array, oldTable[j].ptr, true, false);
    }
  }

  TRI_Free(array->_memoryZone, oldTable);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

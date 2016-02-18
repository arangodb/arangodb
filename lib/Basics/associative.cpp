////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
/// @author Martin Schoenert
////////////////////////////////////////////////////////////////////////////////

#include "associative.h"
#include "Basics/hashes.h"
#include "Basics/tri-strings.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief initial number of elements in the array
////////////////////////////////////////////////////////////////////////////////

#define INITIAL_SIZE (11)

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new element
////////////////////////////////////////////////////////////////////////////////

static void AddNewElementPointer(TRI_associative_pointer_t* array,
                                 void* element) {
  uint64_t hash;
  uint64_t i;

  // compute the hash
  hash = array->hashElement(array, element);

  // search the table
  i = hash % array->_nrAlloc;

  while (array->_table[i] != nullptr) {
    i = TRI_IncModU64(i, array->_nrAlloc);
#ifdef TRI_INTERNAL_STATS
    array->_nrProbesR++;
#endif
  }

  // add a new element to the associative array
  array->_table[i] = element;
  array->_nrUsed++;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief resizes the array
////////////////////////////////////////////////////////////////////////////////

static bool ResizeAssociativePointer(TRI_associative_pointer_t* array,
                                     uint32_t targetSize) {
  void** oldTable = array->_table;
  uint32_t oldAlloc = array->_nrAlloc;

  array->_nrAlloc = targetSize;
#ifdef TRI_INTERNAL_STATS
  array->_nrResizes++;
#endif

  array->_table = static_cast<void**>(TRI_Allocate(
      array->_memoryZone, (size_t)(array->_nrAlloc * sizeof(void*)), true));

  if (array->_table == nullptr) {
    array->_nrAlloc = oldAlloc;
    array->_table = oldTable;

    return false;
  }

  array->_nrUsed = 0;

  if (oldTable != nullptr) {
    // table is already cleared by allocate, copy old data
    for (uint32_t j = 0; j < oldAlloc; j++) {
      if (oldTable[j] != nullptr) {
        AddNewElementPointer(array, oldTable[j]);
      }
    }

    TRI_Free(array->_memoryZone, oldTable);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes an array
////////////////////////////////////////////////////////////////////////////////

int TRI_InitAssociativePointer(
    TRI_associative_pointer_t* array, TRI_memory_zone_t* zone,
    uint64_t (*hashKey)(TRI_associative_pointer_t*, void const*),
    uint64_t (*hashElement)(TRI_associative_pointer_t*, void const*),
    bool (*isEqualKeyElement)(TRI_associative_pointer_t*, void const*,
                              void const*),
    bool (*isEqualElementElement)(TRI_associative_pointer_t*, void const*,
                                  void const*)) {
  array->hashKey = hashKey;
  array->hashElement = hashElement;
  array->isEqualKeyElement = isEqualKeyElement;
  array->isEqualElementElement = isEqualElementElement;

  array->_memoryZone = zone;
  array->_nrAlloc = 0;
  array->_nrUsed = 0;
  array->_table = nullptr;

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

  if (nullptr == (array->_table = static_cast<void**>(TRI_Allocate(
                      zone, sizeof(void*) * INITIAL_SIZE, true)))) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  array->_nrAlloc = INITIAL_SIZE;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an array, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyAssociativePointer(TRI_associative_pointer_t* array) {
  if (array->_table != nullptr) {
    TRI_Free(array->_memoryZone, array->_table);
    array->_table = nullptr;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief General hash function that can be used to hash a key
////////////////////////////////////////////////////////////////////////////////

uint64_t TRI_HashStringKeyAssociativePointer(TRI_associative_pointer_t* array,
                                             void const* key) {
  return TRI_FnvHashString((char const*)key);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief General function to determine equality of two string values
////////////////////////////////////////////////////////////////////////////////

bool TRI_EqualStringKeyAssociativePointer(TRI_associative_pointer_t* array,
                                          void const* key,
                                          void const* element) {
  return TRI_EqualString((char*)key, (char*)element);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reserves space in the array for extra elements
////////////////////////////////////////////////////////////////////////////////

bool TRI_ReserveAssociativePointer(TRI_associative_pointer_t* array,
                                   uint32_t nrElements) {
  uint32_t targetSize = array->_nrUsed + nrElements;

  if (array->_nrAlloc >= 2 * targetSize) {
    // no need to resize
    return true;
  }

  // we must resize

  // make sure we grow the array by a huge amount so we have only few resizes
  if (targetSize < (2 * array->_nrAlloc) + 1) {
    targetSize = (2 * array->_nrAlloc) + 1;
  }

  return ResizeAssociativePointer(array, targetSize);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an element given a key
////////////////////////////////////////////////////////////////////////////////

void* TRI_LookupByKeyAssociativePointer(TRI_associative_pointer_t* array,
                                        void const* key) {
  if (array->_nrUsed == 0) {
    return nullptr;
  }

  TRI_ASSERT(array->_nrAlloc > 0);

  // compute the hash
  uint64_t const hash = array->hashKey(array, key);
  uint64_t const n = array->_nrAlloc;
  uint64_t i = hash % n;

#ifdef TRI_INTERNAL_STATS
  // update statistics
  array->_nrFinds++;
#endif

  // search the table
  while (array->_table[i] != nullptr &&
         !array->isEqualKeyElement(array, key, array->_table[i])) {
    i = TRI_IncModU64(i, n);
#ifdef TRI_INTERNAL_STATS
    array->_nrProbesF++;
#endif
  }

  // return whatever we found
  return array->_table[i];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an element given an element
////////////////////////////////////////////////////////////////////////////////

void* TRI_LookupByElementAssociativePointer(TRI_associative_pointer_t* array,
                                            void const* element) {
  if (array->_nrUsed == 0) {
    return nullptr;
  }

  // compute the hash
  uint64_t const hash = array->hashElement(array, element);
  uint64_t const n = array->_nrAlloc;

  TRI_ASSERT(n > 0);

  uint64_t i, k;
  i = k = hash % n;

#ifdef TRI_INTERNAL_STATS
  // update statistics
  array->_nrFinds++;
#endif

  // search the table
  for (; i < n && array->_table[i] != nullptr &&
             !array->isEqualElementElement(array, element, array->_table[i]);
       ++i)
    ;

  if (i == n) {
    for (i = 0;
         i < k && array->_table[i] != nullptr &&
             !array->isEqualElementElement(array, element, array->_table[i]);
         ++i)
      ;
  }

  // return whatever we found
  return array->_table[i];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element to the array
////////////////////////////////////////////////////////////////////////////////

void* TRI_InsertElementAssociativePointer(TRI_associative_pointer_t* array,
                                          void* element, bool overwrite) {
  // check for out-of-memory
  if (array->_nrAlloc == array->_nrUsed || array->_nrAlloc == 0) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  // compute the hash
  uint64_t hash = array->hashElement(array, element);
  uint64_t i = hash % array->_nrAlloc;

#ifdef TRI_INTERNAL_STATS
  // update statistics
  array->_nrAdds++;
#endif

  // search the table
  while (array->_table[i] != nullptr &&
         !array->isEqualElementElement(array, element, array->_table[i])) {
    i = TRI_IncModU64(i, array->_nrAlloc);
#ifdef TRI_INTERNAL_STATS
    array->_nrProbesA++;
#endif
  }

  void* old = array->_table[i];

  // if we found an element, return
  if (old != nullptr) {
    if (overwrite) {
      array->_table[i] = element;
    }

    return old;
  }

  // add a new element to the associative array
  array->_table[i] = element;
  array->_nrUsed++;

  // if we were adding and the table is more than half full, extend it
  if (array->_nrAlloc < 2 * array->_nrUsed) {
    ResizeAssociativePointer(array, (uint32_t)(2 * array->_nrAlloc) + 1);
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an key/element to the array
////////////////////////////////////////////////////////////////////////////////

void* TRI_InsertKeyAssociativePointer(TRI_associative_pointer_t* array,
                                      void const* key, void* element,
                                      bool overwrite) {
  // check for out-of-memory
  if (array->_nrAlloc == array->_nrUsed || array->_nrAlloc == 0) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  // compute the hash
  uint64_t hash = array->hashKey(array, key);
  uint64_t i = hash % array->_nrAlloc;

#ifdef TRI_INTERNAL_STATS
  // update statistics
  array->_nrAdds++;
#endif

  // search the table
  while (array->_table[i] != nullptr &&
         !array->isEqualKeyElement(array, key, array->_table[i])) {
    i = TRI_IncModU64(i, array->_nrAlloc);
#ifdef TRI_INTERNAL_STATS
    array->_nrProbesA++;
#endif
  }

  void* old = array->_table[i];

  // if we found an element, return
  if (old != nullptr) {
    if (overwrite) {
      array->_table[i] = element;
    }

    return old;
  }

  // add a new element to the associative array
  array->_table[i] = element;
  array->_nrUsed++;

  // if we were adding and the table is more than half full, extend it
  if (array->_nrAlloc < 2 * array->_nrUsed) {
    ResizeAssociativePointer(array, (uint32_t)(2 * array->_nrAlloc) + 1);
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an key/element to the array
/// returns a status code, and *found will contain the found element (if any)
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertKeyAssociativePointer2(TRI_associative_pointer_t* array,
                                     void const* key, void* element,
                                     void const** found) {
  if (found != nullptr) {
    *found = nullptr;
  }

  // check for out-of-memory
  if (array->_nrAlloc == array->_nrUsed || array->_nrAlloc == 0) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // compute the hash
  uint64_t hash = array->hashKey(array, key);
  uint64_t i = hash % array->_nrAlloc;

#ifdef TRI_INTERNAL_STATS
  // update statistics
  array->_nrAdds++;
#endif

  // search the table
  while (array->_table[i] != nullptr &&
         !array->isEqualKeyElement(array, key, array->_table[i])) {
    i = TRI_IncModU64(i, array->_nrAlloc);
#ifdef TRI_INTERNAL_STATS
    array->_nrProbesA++;
#endif
  }

  void* old = array->_table[i];

  // if we found an element, return
  if (old != nullptr) {
    if (found != nullptr) {
      *found = old;
    }

    return TRI_ERROR_NO_ERROR;
  }

  // if we were adding and the table is more than half full, extend it
  if (array->_nrAlloc < 2 * array->_nrUsed) {
    if (!ResizeAssociativePointer(array, (uint32_t)(2 * array->_nrAlloc) + 1)) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    // now we need to recalc the position
    i = hash % array->_nrAlloc;
    // search the table
    while (array->_table[i] != nullptr &&
           !array->isEqualKeyElement(array, key, array->_table[i])) {
      i = TRI_IncModU64(i, array->_nrAlloc);
#ifdef TRI_INTERNAL_STATS
      array->_nrProbesA++;
#endif
    }
  }

  // add a new element to the associative array
  array->_table[i] = element;
  array->_nrUsed++;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an key/element to the array
////////////////////////////////////////////////////////////////////////////////

void* TRI_RemoveKeyAssociativePointer(TRI_associative_pointer_t* array,
                                      void const* key) {
  if (array->_nrUsed == 0) {
    return nullptr;
  }

  uint64_t hash = array->hashKey(array, key);
  uint64_t i = hash % array->_nrAlloc;

#ifdef TRI_INTERNAL_STATS
  // update statistics
  array->_nrRems++;
#endif

  // search the table
  while (array->_table[i] != nullptr &&
         !array->isEqualKeyElement(array, key, array->_table[i])) {
    i = TRI_IncModU64(i, array->_nrAlloc);
#ifdef TRI_INTERNAL_STATS
    array->_nrProbesD++;
#endif
  }

  // if we did not find such an item return false
  if (array->_table[i] == nullptr) {
    return nullptr;
  }

  // remove item
  void* old = array->_table[i];
  array->_table[i] = nullptr;
  array->_nrUsed--;

  // and now check the following places for items to move here
  uint64_t k = TRI_IncModU64(i, array->_nrAlloc);

  while (array->_table[k] != nullptr) {
    uint64_t j = array->hashElement(array, array->_table[k]) % array->_nrAlloc;

    if ((i < k && !(i < j && j <= k)) || (k < i && !(i < j || j <= k))) {
      array->_table[i] = array->_table[k];
      array->_table[k] = nullptr;
      i = k;
    }

    k = TRI_IncModU64(k, array->_nrAlloc);
  }

  // return success
  return old;
}

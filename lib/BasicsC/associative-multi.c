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
// --SECTION--                                              ASSOCIATIVE POINTERS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                   private defines
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initial number of elements of a container
////////////////////////////////////////////////////////////////////////////////

#define INITIAL_SIZE (64)

////////////////////////////////////////////////////////////////////////////////
/// @brief forward declaration
////////////////////////////////////////////////////////////////////////////////

static int ResizeMultiPointer (TRI_multi_pointer_t* array, size_t size); 

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

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

  if (NULL == (array->_table_alloc = TRI_Allocate(zone, 
                 sizeof(TRI_multi_pointer_entry_t) * INITIAL_SIZE+64, true))) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  array->_table = (TRI_multi_pointer_entry_t*) 
                  (((uint64_t) array->_table_alloc + 63) & ~((uint64_t)63));

  array->_nrAlloc = INITIAL_SIZE;

#ifdef TRI_INTERNAL_STATS
  array->_nrFinds = 0;
  array->_nrAdds = 0;
  array->_nrRems = 0;
  array->_nrResizes = 0;
  array->_nrProbes = 0;
  array->_nrProbesF = 0;
  array->_nrProbesD = 0;
#endif

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an array, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyMultiPointer (TRI_multi_pointer_t* array) {
  if (array->_table != NULL) {
    TRI_Free(array->_memoryZone, array->_table_alloc);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an array and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeMultiPointer (TRI_memory_zone_t* zone, TRI_multi_pointer_t* array) {
  TRI_DestroyMultiPointer(array);
  TRI_Free(zone, array);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief find an element or its place using the element hash function
////////////////////////////////////////////////////////////////////////////////

static inline uint64_t FindElementPlace (TRI_multi_pointer_t* array,
                                         void const* element,
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
#ifdef TRI_INTERNAL_STATS
    array->_nrProbes++;
#endif
  }
  return i;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find an element or its place by key or element identity
////////////////////////////////////////////////////////////////////////////////

static uint64_t LookupByElement (TRI_multi_pointer_t* array,
                                 void const* element) {
  // This performs a complete lookup for an element. It returns a slot
  // number. This slot is either empty or contains an element that
  // compares equal to element.
  uint64_t hash;
  uint64_t i, j;

  // compute the hash
  hash = array->hashElement(array, element, true);
  i = hash % array->_nrAlloc;

  // Now find the first slot with an entry with the same key that is the
  // start of a linked list, or a free slot:
  while (array->_table[i].ptr != NULL &&
         (array->_table[i].prev != TRI_MULTI_POINTER_INVALID_INDEX ||
          ! array->isEqualElementElement(array, element, 
                                                array->_table[i].ptr, true))) {
    i = TRI_IncModU64(i, array->_nrAlloc);
#ifdef TRI_INTERNAL_STATS
    array->_nrProbes++;
#endif
  }

  if (array->_table[i].ptr != NULL) {
    // It might be right here!
    if (array->isEqualElementElement(array, element,
                                            array->_table[i].ptr, false)) {
      return i;
    }

    // Now we have to look for it in its hash position:
    j = FindElementPlace(array, element, true);

    // We have either found an equal element or nothing:
    return j;
  }

  // If we get here, no element with the same key is in the array, so
  // we will not be able to find it anywhere!
  return i;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief helper to decide whether something is between to places
////////////////////////////////////////////////////////////////////////////////

static inline bool IsBetween(uint64_t from, uint64_t x, uint64_t to) {
  // returns whether or not x is behind from and before or equal to
  // to in the cyclic order. If x is equal to from, then the result is
  // always false. If from is equal to to, then the result is always
  // true.
  return (from < to) ? (from < x && x <= to)
                     : (x > from || x <= to);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief helper to invalidate a slot
////////////////////////////////////////////////////////////////////////////////

static inline void InvalidateEntry (TRI_multi_pointer_t* array, uint64_t i) {
  array->_table[i].ptr = NULL;
  array->_table[i].next = TRI_MULTI_POINTER_INVALID_INDEX;
  array->_table[i].prev = TRI_MULTI_POINTER_INVALID_INDEX;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief helper to move an entry from one slot to another
////////////////////////////////////////////////////////////////////////////////

static inline void MoveEntry (TRI_multi_pointer_t* array, 
                              uint64_t from, uint64_t to) {
  // Moves an entry, adjusts the linked lists, but does not take care
  // for the hole. to must be unused. from can be any element in a
  // linked list.
  array->_table[to] = array->_table[from];
  if (array->_table[to].prev != TRI_MULTI_POINTER_INVALID_INDEX) {
    array->_table[array->_table[to].prev].next = to;
  }
  if (array->_table[to].next != TRI_MULTI_POINTER_INVALID_INDEX) {
    array->_table[array->_table[to].next].prev = to;
  }
  InvalidateEntry(array, from);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief helper to heal a hole where we deleted something
////////////////////////////////////////////////////////////////////////////////

static void HealHole (TRI_multi_pointer_t* array, uint64_t i) {
  uint64_t j, k;
  uint64_t hash;

  j = TRI_IncModU64(i, array->_nrAlloc);
  while (array->_table[j].ptr != NULL) {
    // Find out where this element ought to be:
    // If it is the start of one of the linked lists, we need to hash
    // by key, otherwise, we hash by the full identity of the element:
    hash = array->hashElement(array, array->_table[j].ptr, 
                     array->_table[j].prev == TRI_MULTI_POINTER_INVALID_INDEX);
    k = hash % array->_nrAlloc;
    if (! IsBetween(i, k, j)) {
      // we have to move j to i:
      MoveEntry(array, j, i);
      i = j;  // Now heal this hole at j, j will be incremented right away
    }
    j = TRI_IncModU64(j, array->_nrAlloc);
#ifdef TRI_INTERNAL_STATS
    array->_nrProbesD++;
#endif
  }
}


// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a key/element to the array
////////////////////////////////////////////////////////////////////////////////

void* TRI_InsertElementMultiPointer (TRI_multi_pointer_t* array,
                                     void* element,
                                     bool const overwrite,
                                     bool const checkEquality) {

  // if the checkEquality flag is not set, we do not check for element
  // equality we use this flag to speed up initial insertion into the
  // index, i.e. when the index is built for a collection and we know
  // for sure no duplicate elements will be inserted

  uint64_t hash;
  uint64_t i, j;
  void* old;

  // if we were adding and the table is more than half full, extend it
  if (array->_nrAlloc < 2 * array->_nrUsed) {
    ResizeMultiPointer(array, 2 * array->_nrAlloc + 1);
  }

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
    array->_table[i].next = TRI_MULTI_POINTER_INVALID_INDEX;
    array->_table[i].prev = TRI_MULTI_POINTER_INVALID_INDEX;
    array->_nrUsed++;
    return NULL;
  }

  // Now find the first slot with an entry with the same key that is the
  // start of a linked list, or a free slot:
  while (array->_table[i].ptr != NULL &&
         (! array->isEqualElementElement(array, element, 
                                                array->_table[i].ptr,true) ||
          array->_table[i].prev != TRI_MULTI_POINTER_INVALID_INDEX)) {
    i = TRI_IncModU64(i, array->_nrAlloc);
#ifdef TRI_INTERNAL_STATS
  // update statistics
    array->_ProbesA++;
#endif

  }

  // If this is free, we are the first with this key:
  if (NULL == array->_table[i].ptr) {
    array->_table[i].ptr = element;
    array->_table[i].next = TRI_MULTI_POINTER_INVALID_INDEX;
    array->_table[i].prev = TRI_MULTI_POINTER_INVALID_INDEX;
    array->_nrUsed++;
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
  array->_table[j].prev = i;
  array->_table[i].next = j;
  // Finally, we need to find the successor to patch it up:
  if (array->_table[j].next != TRI_MULTI_POINTER_INVALID_INDEX) {
    array->_table[array->_table[j].next].prev = i;
  }
  array->_nrUsed++;

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given a key
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t TRI_LookupByKeyMultiPointer (TRI_memory_zone_t* zone,
                                                  TRI_multi_pointer_t* array,
                                                  void const* key) {
  TRI_vector_pointer_t result;
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
  while (array->_table[i].ptr != NULL &&
         (! array->isEqualKeyElement(array, key, array->_table[i].ptr) ||
          array->_table[i].prev != TRI_MULTI_POINTER_INVALID_INDEX)) {
    i = TRI_IncModU64(i, array->_nrAlloc);
#ifdef TRI_INTERNAL_STATS
    array->_nrProbesF++;
#endif
  }

  if (array->_table[i].ptr != NULL) {
    // We found the beginning of the linked list:
    do {
      TRI_PushBackVectorPointer(&result, array->_table[i].ptr);
      i = array->_table[i].next;
    } while (i != TRI_MULTI_POINTER_INVALID_INDEX);
  }

  // return whatever we found
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given an element
////////////////////////////////////////////////////////////////////////////////

void* TRI_LookupByElementMultiPointer (TRI_multi_pointer_t* array, 
                                       void const* element) {
  uint64_t i;

#ifdef TRI_INTERNAL_STATS
  // update statistics
  array->_nrFinds++;
#endif

  i = LookupByElement(array, element);
  return array->_table[i].ptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from the array
////////////////////////////////////////////////////////////////////////////////

static bool CHECK (TRI_multi_pointer_t* array) {
  uint64_t i,ii,j;

  for (i = 0;i < array->_nrAlloc;i++) {
    if (array->_table[i].ptr != NULL && 
        array->_table[i].prev == TRI_MULTI_POINTER_INVALID_INDEX) {
      ii = i;
      j = array->_table[ii].next;
      while (j != TRI_MULTI_POINTER_INVALID_INDEX) {
        if (array->_table[j].prev != ii) {
          printf("Alarm: %llu %llu %llu\n",(unsigned long long) i,
                                           (unsigned long long) ii,
                                           (unsigned long long) j);
          return true;
        }
        ii = j;
        j = array->_table[ii].next;
      }
    }
  }
  return false;
}

void* TRI_RemoveElementMultiPointer (TRI_multi_pointer_t* array, void const* element) {
  uint64_t i, j;
  void* old;

#ifdef TRI_INTERNAL_STATS
  // update statistics
  array->_nrRems++;
#endif

  if (CHECK(array)) {
    printf("Alarm 1\n");
  }
  i = LookupByElement(array, element);
  if (array->_table[i].ptr == NULL) {
    return NULL;
  }
  printf("Removing %llu with links %llu %llu\n",
         (unsigned long long) i,
         (unsigned long long) array->_table[i].prev,
         (unsigned long long) array->_table[i].next);
  old = array->_table[i].ptr;
  // We have to delete entry i
  if (array->_table[i].prev == TRI_MULTI_POINTER_INVALID_INDEX) {
    // This is the first in its linked list.
    j = array->_table[i].next;
    if (j == TRI_MULTI_POINTER_INVALID_INDEX) {
      // The only one in its linked list, simply remove it and heal
      // the hole:
      InvalidateEntry(array, i);
  if (CHECK(array)) {
    printf("Alarm 2\n");
  }
      HealHole(array, i);
  if (CHECK(array)) {
    printf("Alarm 3\n");
  }
    }
    else {
      // There is at least one successor in position j.
      array->_table[j].prev = TRI_MULTI_POINTER_INVALID_INDEX;
      MoveEntry(array, j, i);
  if (CHECK(array)) {
    printf("Alarm 4\n");
  }
      HealHole(array, j);
  if (CHECK(array)) {
    printf("Alarm 5\n");
  }
    }
  }
  else {
    // This one is not the first in its linked list
    j = array->_table[i].prev;
    array->_table[j].next = array->_table[i].next;
    j = array->_table[i].next;
    if (j != TRI_MULTI_POINTER_INVALID_INDEX) {
      // We are not the last in the linked list.
      array->_table[j].prev = array->_table[i].prev;
    }
    printf("prev=%llu next=%llu\n",(unsigned long long) array->_table[i].prev,
                                   (unsigned long long) array->_table[i].next);
    InvalidateEntry(array, i);
  if (CHECK(array)) {
    printf("Alarm 6\n");
  }
    HealHole(array, i);
  if (CHECK(array)) {
    printf("Alarm 7\n");
  }
  }
  array->_nrUsed--;
  
  // return success
  return old;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief resize the array, internal version taking the size as given
////////////////////////////////////////////////////////////////////////////////

static int ResizeMultiPointer (TRI_multi_pointer_t* array, size_t size) {
  TRI_multi_pointer_entry_t* oldTable_alloc;
  TRI_multi_pointer_entry_t* oldTable;
  uint64_t oldAlloc;
  uint64_t j;

  oldTable_alloc = array->_table_alloc;
  oldTable = array->_table;
  oldAlloc = array->_nrAlloc;

  array->_nrAlloc = TRI_NearPrime((uint64_t) size);
  array->_table_alloc = TRI_Allocate(array->_memoryZone, 
                 array->_nrAlloc * sizeof(TRI_multi_pointer_entry_t) + 64,true);
  array->_table = (TRI_multi_pointer_entry_t*) 
                  (((uint64_t) array->_table_alloc + 63) & ~((uint64_t)63));

  if (array->_table == NULL) {
    array->_nrAlloc = oldAlloc;
    array->_table = oldTable;
    array->_table_alloc = oldTable_alloc;

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

  TRI_Free(array->_memoryZone, oldTable_alloc);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief resize the array, adds a reserve of a factor of 2
////////////////////////////////////////////////////////////////////////////////

int TRI_ResizeMultiPointer (TRI_multi_pointer_t* array, size_t size) {
  if (2*size+1 < array->_nrUsed) {
    return TRI_ERROR_BAD_PARAMETER;
  }

  return ResizeMultiPointer(array, 2*size+1);
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

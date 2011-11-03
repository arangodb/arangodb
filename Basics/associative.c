////////////////////////////////////////////////////////////////////////////////
/// @brief associative array implementation
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2006-2011
, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "associative.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 ASSOCIATIVE ARRAY
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup CollectionsPrivate Collections, List, Queues, Vectors (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new element
////////////////////////////////////////////////////////////////////////////////

static void AddNewElement (TRI_associative_array_t* array, void* element) {
  uint64_t hash;
  uint64_t i;

  // compute the hash
  hash = array->hashElement(array, element);

  // search the table
  i = hash % array->_nrAlloc;

  while (! array->isEmptyElement(array, array->_table + i * array->_elementSize)) {
    i = (i + 1) % array->_nrAlloc;
    array->_nrProbesR++;
  }

  // add a new element to the associative array
  memcpy(array->_table + i * array->_elementSize, element, array->_elementSize);
  array->_nrUsed++;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief resizes the array
////////////////////////////////////////////////////////////////////////////////

static void ResizeAssociativeArray (TRI_associative_array_t* array) {
  char * oldTable;
  uint64_t oldAlloc;
  uint64_t j;

  oldTable = array->_table;
  oldAlloc = array->_nrAlloc;

  array->_nrAlloc = 2 * array->_nrAlloc + 1;
  array->_nrUsed = 0;
  array->_nrResizes++;

  array->_table = TRI_Allocate(array->_nrAlloc * array->_elementSize);

  for (j = 0; j < array->_nrAlloc; j++) {
    array->clearElement(array, array->_table + j * array->_elementSize);
  }

  for (j = 0; j < oldAlloc; j++) {
    if (! array->isEmptyElement(array, oldTable + j * array->_elementSize)) {
      AddNewElement(array, oldTable + j * array->_elementSize);
    }
  }

  TRI_Free(oldTable);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Collections Collections, List, Queues, Vectors
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises an array
////////////////////////////////////////////////////////////////////////////////

void TRI_InitAssociativeArray (TRI_associative_array_t* array,
                               size_t elementSize,
                               uint64_t (*hashKey) (TRI_associative_array_t*, void*),
                               uint64_t (*hashElement) (TRI_associative_array_t*, void*),
                               void (*clearElement) (TRI_associative_array_t*, void*),
                               bool (*isEmptyElement) (TRI_associative_array_t*, void*),
                               bool (*isEqualKeyElement) (TRI_associative_array_t*, void*, void*),
                               bool (*isEqualElementElement) (TRI_associative_array_t*, void*, void*)) {
  char* p;
  char* e;

  array->hashKey = hashKey;
  array->hashElement = hashElement;
  array->clearElement = clearElement;
  array->isEmptyElement = isEmptyElement;
  array->isEqualKeyElement = isEqualKeyElement;
  array->isEqualElementElement = isEqualElementElement;

  array->_elementSize = elementSize;
  array->_nrAlloc = 10;

  array->_table = TRI_Allocate(array->_elementSize * array->_nrAlloc);

  p = array->_table;
  e = p + array->_elementSize * array->_nrAlloc;

  for (;  p < e;  p += array->_elementSize) {
    array->clearElement(array, p);
  }

  array->_nrUsed = 0;
  array->_nrFinds = 0;
  array->_nrAdds = 0;
  array->_nrRems = 0;
  array->_nrResizes = 0;
  array->_nrProbesF = 0;
  array->_nrProbesA = 0;
  array->_nrProbesD = 0;
  array->_nrProbesR = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an array, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyAssociativeArray (TRI_associative_array_t* array) {
  TRI_Free(array->_table);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an array and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeAssociativeArray (TRI_associative_array_t* array) {
  TRI_DestroyAssociativeArray(array);
  TRI_Free(array);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Collections Collections, List, Queues, Vectors
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given a key
////////////////////////////////////////////////////////////////////////////////

void* TRI_LookupByKeyAssociativeArray (TRI_associative_array_t* array, void* key) {
  uint64_t hash;
  uint64_t i;

  // compute the hash
  hash = array->hashKey(array, key);
  i = hash % array->_nrAlloc;

  // update statistics
  array->_nrFinds++;

  // search the table
  while (! array->isEmptyElement(array, array->_table + i * array->_elementSize)
         && ! array->isEqualKeyElement(array, key, array->_table + i * array->_elementSize)) {
    i = (i + 1) % array->_nrAlloc;
    array->_nrProbesF++;
  }

  // return whatever we found
  return array->_table + i * array->_elementSize;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds an element given a key, return NULL if not found
////////////////////////////////////////////////////////////////////////////////

void* TRI_FindByKeyAssociativeArray (TRI_associative_array_t* array, void* key) {
  void* element;

  element = TRI_LookupByKeyAssociativeArray(array, key);

  if (! array->isEmptyElement(array, element) && array->isEqualKeyElement(array, key, element)) {
    return element;
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given an element
////////////////////////////////////////////////////////////////////////////////

void* TRI_LookupByElementAssociativeArray (TRI_associative_array_t* array, void* element) {
  uint64_t hash;
  uint64_t i;

  // compute the hash
  hash = array->hashElement(array, element);
  i = hash % array->_nrAlloc;

  // update statistics
  array->_nrFinds++;

  // search the table
  while (! array->isEmptyElement(array, array->_table + i * array->_elementSize)
         && ! array->isEqualElementElement(array, element, array->_table + i * array->_elementSize)) {
    i = (i + 1) % array->_nrAlloc;
    array->_nrProbesF++;
  }

  // return whatever we found
  return array->_table + i * array->_elementSize;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds an element given an element, returns NULL if not found
////////////////////////////////////////////////////////////////////////////////

void* TRI_FindByElementAssociativeArray (TRI_associative_array_t* array, void* element) {
  void* element2;

  element2 = TRI_LookupByElementAssociativeArray(array, element);

  if (! array->isEmptyElement(array, element2) && array->isEqualElementElement(array, element2, element)) {
    return element2;
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element to the array
////////////////////////////////////////////////////////////////////////////////

bool TRI_InsertElementAssociativeArray (TRI_associative_array_t* array, void* element, bool overwrite) {
  uint64_t hash;
  uint64_t i;

  // compute the hash
  hash = array->hashElement(array, element);
  i = hash % array->_nrAlloc;

  // update statistics
  array->_nrAdds++;

  // search the table
  while (! array->isEmptyElement(array, array->_table + i * array->_elementSize)
         && ! array->isEqualElementElement(array, element, array->_table + i * array->_elementSize)) {
    i = (i + 1) % array->_nrAlloc;
    array->_nrProbesA++;
  }

  // if we found an element, return
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
    ResizeAssociativeArray(array);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an key/element to the array
////////////////////////////////////////////////////////////////////////////////

bool TRI_InsertKeyAssociativeArray (TRI_associative_array_t* array, void* key, void* element, bool overwrite) {
  uint64_t hash;
  uint64_t i;

  // compute the hash
  hash = array->hashKey(array, key);
  i = hash % array->_nrAlloc;

  // update statistics
  array->_nrAdds++;

  // search the table
  while (! array->isEmptyElement(array, array->_table + i * array->_elementSize)
         && ! array->isEqualKeyElement(array, key, array->_table + i * array->_elementSize)) {
    i = (i + 1) % array->_nrAlloc;
    array->_nrProbesA++;
  }

  // if we found an element, return
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
    ResizeAssociativeArray(array);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from the array
////////////////////////////////////////////////////////////////////////////////

bool TRI_RemoveElementAssociativeArray (TRI_associative_array_t* array, void* element, void* old) {
  uint64_t hash;
  uint64_t i;
  uint64_t k;

  hash = array->hashElement(array, element);
  i = hash % array->_nrAlloc;

  // update statistics
  array->_nrRems++;

  // search the table
  while (! array->isEmptyElement(array, array->_table + i * array->_elementSize)
         && ! array->isEqualElementElement(array, element, array->_table + i * array->_elementSize)) {
    i = (i + 1) % array->_nrAlloc;
    array->_nrProbesD++;
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
  k = (i + 1) % array->_nrAlloc;

  while (! array->isEmptyElement(array, array->_table + k * array->_elementSize)) {
    uint64_t j = array->hashElement(array, array->_table + k * array->_elementSize) % array->_nrAlloc;

    if ((i < k && !(i < j && j <= k)) || (k < i && !(i < j || j <= k))) {
      memcpy(array->_table + i * array->_elementSize, array->_table + k * array->_elementSize, array->_elementSize);
      array->clearElement(array, array->_table + k * array->_elementSize);
      i = k;
    }

    k = (k + 1) % array->_nrAlloc;
  }

  // return success
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an key/element to the array
////////////////////////////////////////////////////////////////////////////////

bool TRI_RemoveKeyAssociativeArray (TRI_associative_array_t* array, void* key, void* old) {
  uint64_t hash;
  uint64_t i;
  uint64_t k;

  hash = array->hashKey(array, key);
  i = hash % array->_nrAlloc;

  // update statistics
  array->_nrRems++;

  // search the table
  while (! array->isEmptyElement(array, array->_table + i * array->_elementSize)
         && ! array->isEqualKeyElement(array, key, array->_table + i * array->_elementSize)) {
    i = (i + 1) % array->_nrAlloc;
    array->_nrProbesD++;
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
  k = (i + 1) % array->_nrAlloc;

  while (! array->isEmptyElement(array, array->_table + k * array->_elementSize)) {
    uint64_t j = array->hashElement(array, array->_table + k * array->_elementSize) % array->_nrAlloc;

    if ((i < k && !(i < j && j <= k)) || (k < i && !(i < j || j <= k))) {
      memcpy(array->_table + i * array->_elementSize, array->_table + k * array->_elementSize, array->_elementSize);
      array->clearElement(array, array->_table + k * array->_elementSize);
      i = k;
    }

    k = (k + 1) % array->_nrAlloc;
  }

  // return success
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                              ASSOCIATIVE POINTERS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup CollectionsPrivate Collections, List, Queues, Vectors (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new element
////////////////////////////////////////////////////////////////////////////////

static void AddNewElementPointer (TRI_associative_pointer_t* array, void* element) {
  uint64_t hash;
  uint64_t i;

  // compute the hash
  hash = array->hashElement(array, element);

  // search the table
  i = hash % array->_nrAlloc;

  while (array->_table[i] != NULL) {
    i = (i + 1) % array->_nrAlloc;
    array->_nrProbesR++;
  }

  // add a new element to the associative array
  array->_table[i] = element;
  array->_nrUsed++;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief resizes the array
////////////////////////////////////////////////////////////////////////////////

static void ResizeAssociativePointer (TRI_associative_pointer_t* array) {
  void** oldTable;
  uint64_t oldAlloc;
  uint64_t j;

  oldTable = array->_table;
  oldAlloc = array->_nrAlloc;

  array->_nrAlloc = 2 * array->_nrAlloc + 1;
  array->_nrUsed = 0;
  array->_nrResizes++;

  array->_table = TRI_Allocate(array->_nrAlloc * sizeof(void*));

  for (j = 0; j < array->_nrAlloc; j++) {
    array->_table[j] = 0;
  }

  for (j = 0; j < oldAlloc; j++) {
    if (oldTable[j] != NULL) {
      AddNewElementPointer(array, oldTable[j]);
    }
  }

  TRI_Free(oldTable);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Collections Collections, List, Queues, Vectors
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises an array
////////////////////////////////////////////////////////////////////////////////

void TRI_InitAssociativePointer (TRI_associative_pointer_t* array,
                                 uint64_t (*hashKey) (TRI_associative_pointer_t*, void const*),
                                 uint64_t (*hashElement) (TRI_associative_pointer_t*, void const*),
                                 bool (*isEqualKeyElement) (TRI_associative_pointer_t*, void const*, void const*),
                                 bool (*isEqualElementElement) (TRI_associative_pointer_t*, void const*, void const*)) {
  void** p;
  void** e;

  array->hashKey = hashKey;
  array->hashElement = hashElement;
  array->isEqualKeyElement = isEqualKeyElement;
  array->isEqualElementElement = isEqualElementElement;

  array->_nrAlloc = 10;

  array->_table = TRI_Allocate(sizeof(void*) * array->_nrAlloc);

  p = array->_table;
  e = p + array->_nrAlloc;

  for (;  p < e;  ++p) {
    *p = 0;
  }

  array->_nrUsed = 0;
  array->_nrFinds = 0;
  array->_nrAdds = 0;
  array->_nrRems = 0;
  array->_nrResizes = 0;
  array->_nrProbesF = 0;
  array->_nrProbesA = 0;
  array->_nrProbesD = 0;
  array->_nrProbesR = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an array, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyAssociativePointer (TRI_associative_pointer_t* array) {
  TRI_Free(array->_table);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an array and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeAssociativePointer (TRI_associative_pointer_t* array) {
  TRI_DestroyAssociativePointer(array);
  TRI_Free(array);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Collections Collections, List, Queues, Vectors
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given a key
////////////////////////////////////////////////////////////////////////////////

void const* TRI_LookupByKeyAssociativePointer (TRI_associative_pointer_t* array, void const* key) {
  uint64_t hash;
  uint64_t i;

  // compute the hash
  hash = array->hashKey(array, key);
  i = hash % array->_nrAlloc;

  // update statistics
  array->_nrFinds++;

  // search the table
  while (array->_table[i] != 0 && ! array->isEqualKeyElement(array, key, array->_table[i])) {
    i = (i + 1) % array->_nrAlloc;
    array->_nrProbesF++;
  }

  // return whatever we found
  return array->_table[i];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds an element given a key, return NULL if not found
////////////////////////////////////////////////////////////////////////////////

void const* TRI_FindByKeyAssociativePointer (TRI_associative_pointer_t* array, void const* key) {
  void const* element;

  element = TRI_LookupByKeyAssociativePointer(array, key);

  if (element != 0 && array->isEqualKeyElement(array, key, element)) {
    return element;
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given an element
////////////////////////////////////////////////////////////////////////////////

void const* TRI_LookupByElementAssociativePointer (TRI_associative_pointer_t* array, void const* element) {
  uint64_t hash;
  uint64_t i;

  // compute the hash
  hash = array->hashElement(array, element);
  i = hash % array->_nrAlloc;

  // update statistics
  array->_nrFinds++;

  // search the table
  while (array->_table[i] != 0 && ! array->isEqualElementElement(array, element, array->_table[i])) {
    i = (i + 1) % array->_nrAlloc;
    array->_nrProbesF++;
  }

  // return whatever we found
  return array->_table[i];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds an element given an element, returns NULL if not found
////////////////////////////////////////////////////////////////////////////////

void const* TRI_FindByElementAssociativePointer (TRI_associative_pointer_t* array, void const* element) {
  void const* element2;

  element2 = TRI_LookupByElementAssociativePointer(array, element);

  if (element2 != 0 && array->isEqualElementElement(array, element2, element)) {
    return element2;
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element to the array
////////////////////////////////////////////////////////////////////////////////

void* TRI_InsertElementAssociativePointer (TRI_associative_pointer_t* array, void* element, bool overwrite) {
  uint64_t hash;
  uint64_t i;
  void* old;

  // compute the hash
  hash = array->hashElement(array, element);
  i = hash % array->_nrAlloc;

  // update statistics
  array->_nrAdds++;

  // search the table
  while (array->_table[i] != 0 && ! array->isEqualElementElement(array, element, array->_table[i])) {
    i = (i + 1) % array->_nrAlloc;
    array->_nrProbesA++;
  }

  old = array->_table[i];

  // if we found an element, return
  if (old != NULL) {
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
    ResizeAssociativePointer(array);
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an key/element to the array
////////////////////////////////////////////////////////////////////////////////

void* TRI_InsertKeyAssociativePointer (TRI_associative_pointer_t* array, void const* key, void* element, bool overwrite) {
  uint64_t hash;
  uint64_t i;
  void* old;

  // compute the hash
  hash = array->hashKey(array, key);
  i = hash % array->_nrAlloc;

  // update statistics
  array->_nrAdds++;

  // search the table
  while (array->_table[i] != 0 && ! array->isEqualKeyElement(array, key, array->_table[i])) {
    i = (i + 1) % array->_nrAlloc;
    array->_nrProbesA++;
  }

  old = array->_table[i];

  // if we found an element, return
  if (old != NULL) {
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
    ResizeAssociativePointer(array);
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from the array
////////////////////////////////////////////////////////////////////////////////

void* TRI_RemoveElementAssociativePointer (TRI_associative_pointer_t* array, void const* element) {
  uint64_t hash;
  uint64_t i;
  uint64_t k;
  void* old;

  hash = array->hashElement(array, element);
  i = hash % array->_nrAlloc;

  // update statistics
  array->_nrRems++;

  // search the table
  while (array->_table[i] != 0 && ! array->isEqualElementElement(array, element, array->_table[i])) {
    i = (i + 1) % array->_nrAlloc;
    array->_nrProbesD++;
  }

  // if we did not find such an item return 0
  if (array->_table[i] == NULL) {
    return NULL;
  }

  // remove item
  old = array->_table[i];
  array->_table[i] = 0;
  array->_nrUsed--;

  // and now check the following places for items to move here
  k = (i + 1) % array->_nrAlloc;

  while (array->_table[k] != NULL) {
    uint64_t j = array->hashElement(array, array->_table[k]) % array->_nrAlloc;

    if ((i < k && !(i < j && j <= k)) || (k < i && !(i < j || j <= k))) {
      array->_table[i] = array->_table[k];
      array->_table[k] = 0;
      i = k;
    }

    k = (k + 1) % array->_nrAlloc;
  }

  // return success
  return old;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an key/element to the array
////////////////////////////////////////////////////////////////////////////////

void* TRI_RemoveKeyAssociativePointer (TRI_associative_pointer_t* array, void const* key) {
  uint64_t hash;
  uint64_t i;
  uint64_t k;
  void* old;

  hash = array->hashKey(array, key);
  i = hash % array->_nrAlloc;

  // update statistics
  array->_nrRems++;

  // search the table
  while (array->_table[i] != 0 && ! array->isEqualKeyElement(array, key, array->_table[i])) {
    i = (i + 1) % array->_nrAlloc;
    array->_nrProbesD++;
  }

  // if we did not find such an item return false
  if (array->_table[i] == NULL) {
    return NULL;
  }

  // remove item
  old = array->_table[i];
  array->_table[i] = 0;
  array->_nrUsed--;

  // and now check the following places for items to move here
  k = (i + 1) % array->_nrAlloc;

  while (array->_table[k] != NULL) {
    uint64_t j = array->hashElement(array, array->_table[k]) % array->_nrAlloc;

    if ((i < k && !(i < j && j <= k)) || (k < i && !(i < j || j <= k))) {
      array->_table[i] = array->_table[k];
      array->_table[k] = 0;
      i = k;
    }

    k = (k + 1) % array->_nrAlloc;
  }

  // return success
  return old;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                ASSOCIATIVE SYNCED
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup CollectionsPrivate Collections, List, Queues, Vectors (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new element
////////////////////////////////////////////////////////////////////////////////

static void AddNewElementSynced (TRI_associative_synced_t* array, void* element) {
  uint64_t hash;
  uint64_t i;

  // compute the hash
  hash = array->hashElement(array, element);

  // search the table
  i = hash % array->_nrAlloc;

  while (array->_table[i] != NULL) {
    i = (i + 1) % array->_nrAlloc;
    array->_nrProbesR++;
  }

  // add a new element to the associative array
  array->_table[i] = element;
  array->_nrUsed++;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief resizes the array
////////////////////////////////////////////////////////////////////////////////

static void ResizeAssociativeSynced (TRI_associative_synced_t* array) {
  void** oldTable;
  uint64_t oldAlloc;
  uint64_t j;

  oldTable = array->_table;
  oldAlloc = array->_nrAlloc;

  array->_nrAlloc = 2 * array->_nrAlloc + 1;
  array->_nrUsed = 0;
  array->_nrResizes++;

  array->_table = TRI_Allocate(array->_nrAlloc * sizeof(void*));

  for (j = 0; j < array->_nrAlloc; j++) {
    array->_table[j] = 0;
  }

  for (j = 0; j < oldAlloc; j++) {
    if (oldTable[j] != NULL) {
      AddNewElementSynced(array, oldTable[j]);
    }
  }

  TRI_Free(oldTable);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Collections Collections, List, Queues, Vectors
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises an array
////////////////////////////////////////////////////////////////////////////////

void TRI_InitAssociativeSynced (TRI_associative_synced_t* array,
                                 uint64_t (*hashKey) (TRI_associative_synced_t*, void const*),
                                 uint64_t (*hashElement) (TRI_associative_synced_t*, void const*),
                                 bool (*isEqualKeyElement) (TRI_associative_synced_t*, void const*, void const*),
                                 bool (*isEqualElementElement) (TRI_associative_synced_t*, void const*, void const*)) {
  void** p;
  void** e;

  array->hashKey = hashKey;
  array->hashElement = hashElement;
  array->isEqualKeyElement = isEqualKeyElement;
  array->isEqualElementElement = isEqualElementElement;

  array->_nrAlloc = 10;

  array->_table = TRI_Allocate(sizeof(void*) * array->_nrAlloc);

  p = array->_table;
  e = p + array->_nrAlloc;

  for (;  p < e;  ++p) {
    *p = 0;
  }

  array->_nrUsed = 0;
  array->_nrFinds = 0;
  array->_nrAdds = 0;
  array->_nrRems = 0;
  array->_nrResizes = 0;
  array->_nrProbesF = 0;
  array->_nrProbesA = 0;
  array->_nrProbesD = 0;
  array->_nrProbesR = 0;

  TRI_InitReadWriteLock(&array->_lock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an array, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyAssociativeSynced (TRI_associative_synced_t* array) {
  TRI_Free(array->_table);
  TRI_DestroyReadWriteLock(&array->_lock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an array and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeAssociativeSynced (TRI_associative_synced_t* array) {
  TRI_DestroyAssociativeSynced(array);
  TRI_Free(array);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Collections Collections, List, Queues, Vectors
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given a key
////////////////////////////////////////////////////////////////////////////////

void const* TRI_LookupByKeyAssociativeSynced (TRI_associative_synced_t* array, void const* key) {
  uint64_t hash;
  uint64_t i;
  void const* result;

  // compute the hash
  hash = array->hashKey(array, key);
  i = hash % array->_nrAlloc;

  // update statistics
  array->_nrFinds++;

  // search the table
  TRI_ReadLockReadWriteLock(&array->_lock);

  while (array->_table[i] != 0 && ! array->isEqualKeyElement(array, key, array->_table[i])) {
    i = (i + 1) % array->_nrAlloc;
    array->_nrProbesF++;
  }

  result = array->_table[i];

  TRI_ReadUnlockReadWriteLock(&array->_lock);

  // return whatever we found
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds an element given a key, return NULL if not found
////////////////////////////////////////////////////////////////////////////////

void const* TRI_FindByKeyAssociativeSynced (TRI_associative_synced_t* array, void const* key) {
  void const* element;

  element = TRI_LookupByKeyAssociativeSynced(array, key);

  if (element != 0 && array->isEqualKeyElement(array, key, element)) {
    return element;
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given an element
////////////////////////////////////////////////////////////////////////////////

void const* TRI_LookupByElementAssociativeSynced (TRI_associative_synced_t* array, void const* element) {
  uint64_t hash;
  uint64_t i;
  void const* result;

  // compute the hash
  hash = array->hashElement(array, element);
  i = hash % array->_nrAlloc;

  // update statistics
  array->_nrFinds++;

  // search the table
  TRI_ReadLockReadWriteLock(&array->_lock);

  while (array->_table[i] != 0 && ! array->isEqualElementElement(array, element, array->_table[i])) {
    i = (i + 1) % array->_nrAlloc;
    array->_nrProbesF++;
  }

  result = array->_table[i];

  TRI_ReadUnlockReadWriteLock(&array->_lock);

  // return whatever we found
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds an element given an element, returns NULL if not found
////////////////////////////////////////////////////////////////////////////////

void const* TRI_FindByElementAssociativeSynced (TRI_associative_synced_t* array, void const* element) {
  void const* element2;

  element2 = TRI_LookupByElementAssociativeSynced(array, element);

  if (element2 != 0 && array->isEqualElementElement(array, element2, element)) {
    return element2;
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element to the array
////////////////////////////////////////////////////////////////////////////////

void* TRI_InsertElementAssociativeSynced (TRI_associative_synced_t* array, void* element) {
  uint64_t hash;
  uint64_t i;
  union { void const* c; void* v; } old;

  // compute the hash
  hash = array->hashElement(array, element);
  i = hash % array->_nrAlloc;

  // update statistics
  array->_nrAdds++;

  // search the table, TODO optimise the locks
  TRI_WriteLockReadWriteLock(&array->_lock);

  while (array->_table[i] != 0 && ! array->isEqualElementElement(array, element, array->_table[i])) {
    i = (i + 1) % array->_nrAlloc;
    array->_nrProbesA++;
  }

  old.c = array->_table[i];

  // if we found an element, return
  if (old.c != NULL) {
    TRI_WriteUnlockReadWriteLock(&array->_lock);
    return old.v;
  }

  // add a new element to the associative array
  array->_table[i] = element;
  array->_nrUsed++;

  // if we were adding and the table is more than half full, extend it
  if (array->_nrAlloc < 2 * array->_nrUsed) {
    ResizeAssociativeSynced(array);
  }

  TRI_WriteUnlockReadWriteLock(&array->_lock);
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an key/element to the array
////////////////////////////////////////////////////////////////////////////////

void* TRI_InsertKeyAssociativeSynced (TRI_associative_synced_t* array, void const* key, void* element) {
  uint64_t hash;
  uint64_t i;
  union { void const* c; void* v; } old;

  // compute the hash
  hash = array->hashKey(array, key);
  i = hash % array->_nrAlloc;

  // update statistics
  array->_nrAdds++;

  // search the table
  TRI_WriteLockReadWriteLock(&array->_lock);

  while (array->_table[i] != 0 && ! array->isEqualKeyElement(array, key, array->_table[i])) {
    i = (i + 1) % array->_nrAlloc;
    array->_nrProbesA++;
  }

  old.c = array->_table[i];

  // if we found an element, return
  if (old.c != NULL) {
    TRI_WriteUnlockReadWriteLock(&array->_lock);
    return old.v;
  }

  // add a new element to the associative array
  array->_table[i] = element;
  array->_nrUsed++;

  // if we were adding and the table is more than half full, extend it
  if (array->_nrAlloc < 2 * array->_nrUsed) {
    ResizeAssociativeSynced(array);
  }

  TRI_WriteUnlockReadWriteLock(&array->_lock);
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from the array
////////////////////////////////////////////////////////////////////////////////

void* TRI_RemoveElementAssociativeSynced (TRI_associative_synced_t* array, void const* element) {
  uint64_t hash;
  uint64_t i;
  uint64_t k;
  union { void const* c; void* v; } old;

  hash = array->hashElement(array, element);
  i = hash % array->_nrAlloc;

  // update statistics
  array->_nrRems++;

  // search the table
  TRI_WriteLockReadWriteLock(&array->_lock);

  while (array->_table[i] != 0 && ! array->isEqualElementElement(array, element, array->_table[i])) {
    i = (i + 1) % array->_nrAlloc;
    array->_nrProbesD++;
  }

  // if we did not find such an item return 0
  if (array->_table[i] == NULL) {
    TRI_WriteUnlockReadWriteLock(&array->_lock);
    return NULL;
  }

  // remove item
  old.c = array->_table[i];
  array->_table[i] = 0;
  array->_nrUsed--;

  // and now check the following places for items to move here
  k = (i + 1) % array->_nrAlloc;

  while (array->_table[k] != NULL) {
    uint64_t j = array->hashElement(array, array->_table[k]) % array->_nrAlloc;

    if ((i < k && !(i < j && j <= k)) || (k < i && !(i < j || j <= k))) {
      array->_table[i] = array->_table[k];
      array->_table[k] = 0;
      i = k;
    }

    k = (k + 1) % array->_nrAlloc;
  }

  // return success
  TRI_WriteUnlockReadWriteLock(&array->_lock);
  return old.v;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an key/element to the array
////////////////////////////////////////////////////////////////////////////////

void* TRI_RemoveKeyAssociativeSynced (TRI_associative_synced_t* array, void const* key) {
  uint64_t hash;
  uint64_t i;
  uint64_t k;
  union { void const* c; void* v; } old;

  hash = array->hashKey(array, key);
  i = hash % array->_nrAlloc;

  // update statistics
  array->_nrRems++;

  // search the table
  TRI_WriteLockReadWriteLock(&array->_lock);

  while (array->_table[i] != 0 && ! array->isEqualKeyElement(array, key, array->_table[i])) {
    i = (i + 1) % array->_nrAlloc;
    array->_nrProbesD++;
  }

  // if we did not find such an item return false
  if (array->_table[i] == NULL) {
    TRI_WriteUnlockReadWriteLock(&array->_lock);
    return NULL;
  }

  // remove item
  old.c = array->_table[i];
  array->_table[i] = 0;
  array->_nrUsed--;

  // and now check the following places for items to move here
  k = (i + 1) % array->_nrAlloc;

  while (array->_table[k] != NULL) {
    uint64_t j = array->hashElement(array, array->_table[k]) % array->_nrAlloc;

    if ((i < k && !(i < j && j <= k)) || (k < i && !(i < j || j <= k))) {
      array->_table[i] = array->_table[k];
      array->_table[k] = 0;
      i = k;
    }

    k = (k + 1) % array->_nrAlloc;
  }

  // return success
  TRI_WriteUnlockReadWriteLock(&array->_lock);
  return old.v;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\)"
// End:

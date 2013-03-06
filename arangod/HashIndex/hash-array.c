////////////////////////////////////////////////////////////////////////////////
/// @brief hash array implementation
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
/// @author Dr. Oreste Costa-Panaia
/// @author Martin Schoenert
/// @author Copyright 2004-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "hash-array.h"

#include "BasicsC/hashes.h"
#include "HashIndex/hash-index.h"
#include "VocBase/document-collection.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                        COMPARISON
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup HashArray
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns true if an element is 'empty'
////////////////////////////////////////////////////////////////////////////////

static bool IsEmptyElement (TRI_hash_array_t* array,
                            TRI_hash_index_element_t* element) {
  if (element != NULL) {
    if (element->_document == NULL) {
      return true;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief marks an element in the hash array as being 'cleared' or 'empty'
////////////////////////////////////////////////////////////////////////////////

static void ClearElement (TRI_hash_array_t* array,
                          TRI_hash_index_element_t* element) {
  if (element != NULL) {
    element->_document = NULL;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an element, removing any allocated memory
////////////////////////////////////////////////////////////////////////////////

static void DestroyElement (TRI_hash_array_t* array,
                            TRI_hash_index_element_t* element) {
  if (element != NULL) {
    if (element->_document != NULL) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, element->_subObjects);
    }
  }

  ClearElement(array, element);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief determines if two elements are equal
///
/// Two elements are 'equal' if the document pointer is the same.
////////////////////////////////////////////////////////////////////////////////

static bool IsEqualElementElement (TRI_hash_array_t* array,
                                   TRI_hash_index_element_t* left,
                                   TRI_hash_index_element_t* right) {
  if (left->_document == NULL || right->_document == NULL) {
    return false;
  }

  return left->_document == right->_document;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief determines if a key corresponds to an element
////////////////////////////////////////////////////////////////////////////////

static bool IsEqualKeyElement (TRI_hash_array_t* array,
                               TRI_index_search_value_t* left,
                               TRI_hash_index_element_t* right) {
  size_t j;

  if (right->_document == NULL) {
    return false;
  }

  for (j = 0;  j < array->_numFields;  ++j) {
    TRI_shaped_json_t* leftJson = &left->_values[j];
    TRI_shaped_sub_t* rightSub = &right->_subObjects[j];
    size_t length;

    if (leftJson->_sid != rightSub->_sid) {
      return false;
    }

    length = leftJson->_data.length;

    if (length != rightSub->_length) {
      return false;
    }

    if (0 < length) {
      char* ptr = ((char*) right->_document->_data) + rightSub->_offset;

      if (memcmp(leftJson->_data.data, ptr, length) != 0) {
        return false;
      }
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief given a key generates a hash integer
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashKey (TRI_hash_array_t* array,
                         TRI_index_search_value_t* key) {
  uint64_t hash = TRI_FnvHashBlockInitial();
  size_t j;

  for (j = 0;  j < array->_numFields;  ++j) {

    // ignore the sid for hashing
    hash = TRI_FnvHashBlock(hash, key->_values[j]._data.data, key->_values[j]._data.length);
  }
  return hash;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief given an element generates a hash integer
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElement (TRI_hash_array_t* array,
                             TRI_hash_index_element_t* element) {
  uint64_t hash = TRI_FnvHashBlockInitial();
  char* ptr;
  size_t j;

  for (j = 0;  j < array->_numFields;  j++) {

    // ignore the sid for hashing
    ptr = ((char*) element->_document->_data) + element->_subObjects[j]._offset;

    // only hash the data block
    hash = TRI_FnvHashBlock(hash, ptr, element->_subObjects[j]._length);
  }

  return hash;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                        HASH ARRAY
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                   private defines
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup HashArray
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief size of a cache line, in bytes
/// the memory acquired for the hash table is aligned to a multiple of this
/// value
////////////////////////////////////////////////////////////////////////////////

#define CACHE_LINE_SIZE (64)

////////////////////////////////////////////////////////////////////////////////
/// @brief initial preallocation size of the hash table when the table is
/// first created
/// setting this to a high value will waste memory but reduce the number of
/// reallocations/repositionings necessary when the table grows
////////////////////////////////////////////////////////////////////////////////

#define INITIAL_SIZE    (256)

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup HashArray
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new element
////////////////////////////////////////////////////////////////////////////////

static void AddNewElement (TRI_hash_array_t* array,
                           TRI_hash_index_element_t* element) {
  uint64_t hash;
  uint64_t i;

  // ...........................................................................
  // compute the hash
  // ...........................................................................

  hash = HashElement(array, element);

  // ...........................................................................
  // search the table
  // ...........................................................................

  i = hash % array->_nrAlloc;

  while (! IsEmptyElement(array, &array->_table[i])) {
    i = (i + 1) % array->_nrAlloc;

#ifdef TRI_INTERNAL_STATS
    array->_nrProbesR++;
#endif
  }

  // ...........................................................................
  // add a new element to the associative array
  // memcpy ok here since are simply moving array items internally
  // ...........................................................................

  memcpy(&array->_table[i], element, sizeof(TRI_hash_index_element_t));
  array->_nrUsed++;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief allocate memory for the hash table
///
/// the hash table memory will be aligned on a cache line boundary
////////////////////////////////////////////////////////////////////////////////

static int AllocateTable (TRI_hash_array_t* array, size_t numElements) {
  TRI_hash_index_element_t* table;

  table = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE,
                       CACHE_LINE_SIZE + (sizeof(TRI_hash_index_element_t) * numElements),
                       true);

  if (table == NULL) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  array->_table = table;
  array->_nrAlloc = numElements;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief resizes the array
////////////////////////////////////////////////////////////////////////////////

static int ResizeHashArray (TRI_hash_array_t* array) {
  TRI_hash_index_element_t* oldTable;
  uint64_t oldAlloc;
  uint64_t j;
  int res;

  oldTable = array->_table;
  oldAlloc = array->_nrAlloc;

  res = AllocateTable(array, 2 * array->_nrAlloc + 1);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  array->_nrUsed = 0;

#ifdef TRI_INTERNAL_STATS
  array->_nrResizes++;
#endif

  for (j = 0; j < oldAlloc; j++) {
    if (! IsEmptyElement(array, &oldTable[j])) {
      AddNewElement(array, &oldTable[j]);
    }
  }

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, oldTable);
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup HashArray
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises an array
////////////////////////////////////////////////////////////////////////////////

int TRI_InitHashArray (TRI_hash_array_t* array,
                       size_t initialDocumentCount,
                       size_t numFields) {
  size_t initialSize;
  int res;

  // ...........................................................................
  // Assign the callback functions
  // ...........................................................................

  assert(numFields > 0);

  array->_numFields = numFields;
  array->_table = NULL;

  if (initialDocumentCount > 0) {
    // use initial document count provided as initial size
    initialSize = (size_t) (2.5 * initialDocumentCount);
  }
  else {
    initialSize = INITIAL_SIZE;
  }

  res = AllocateTable(array, initialSize);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  // ...........................................................................
  // allocate storage for the hash array
  // ...........................................................................

  array->_nrUsed = 0;

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

void TRI_DestroyHashArray (TRI_hash_array_t* array) {
  if (array == NULL) {
    return;
  }

  // ...........................................................................
  // Go through each item in the array and remove any internal allocated memory
  // ...........................................................................

  // array->_table might be NULL if array initialisation fails
  if (array->_table != NULL) {
    TRI_hash_index_element_t* p;
    TRI_hash_index_element_t* e;

    p = array->_table;
    e = p + array->_nrAlloc;

    for (;  p < e;  ++p) {
      DestroyElement(array, p);
    }

    TRI_Free(TRI_UNKNOWN_MEM_ZONE, array->_table);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an array and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeHashArray (TRI_hash_array_t* array) {
  if (array != NULL) {
    TRI_DestroyHashArray(array);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, array);
  }
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

TRI_hash_index_element_t* TRI_LookupByKeyHashArray (TRI_hash_array_t* array,
                                                    TRI_index_search_value_t* key) {
  uint64_t hash;
  uint64_t i;

  // ...........................................................................
  // compute the hash
  // ...........................................................................

  hash = HashKey(array, key);
  i = hash % array->_nrAlloc;

  // ...........................................................................
  // update statistics
  // ...........................................................................

#ifdef TRI_INTERNAL_STATS
  array->_nrFinds++;
#endif

  // ...........................................................................
  // search the table
  // ...........................................................................

  while (! IsEmptyElement(array, &array->_table[i])
      && ! IsEqualKeyElement(array, key, &array->_table[i])) {
    i = (i + 1) % array->_nrAlloc;

#ifdef TRI_INTERNAL_STATS
    array->_nrProbesF++;
#endif
  }

  // ...........................................................................
  // return whatever we found
  // ...........................................................................

  return &array->_table[i];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds an element given a key, return NULL if not found
////////////////////////////////////////////////////////////////////////////////

TRI_hash_index_element_t* TRI_FindByKeyHashArray (TRI_hash_array_t* array,
                                                  TRI_index_search_value_t* key) {
  TRI_hash_index_element_t* element;

  element = TRI_LookupByKeyHashArray(array, key);

  if (! IsEmptyElement(array, element) && IsEqualKeyElement(array, key, element)) {
    return element;
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given an element
////////////////////////////////////////////////////////////////////////////////

TRI_hash_index_element_t* TRI_LookupByElementHashArray (TRI_hash_array_t* array,
                                                        TRI_hash_index_element_t* element) {
  uint64_t hash;
  uint64_t i;

  // ...........................................................................
  // compute the hash
  // ...........................................................................

  hash = HashElement(array, element);
  i = hash % array->_nrAlloc;

  // ...........................................................................
  // update statistics
  // ...........................................................................

#ifdef TRI_INTERNAL_STATS
  array->_nrFinds++;
#endif

  // ...........................................................................
  // search the table
  // ...........................................................................

  while (! IsEmptyElement(array, &array->_table[i])
      && ! IsEqualElementElement(array, element, &array->_table[i])) {
    i = (i + 1) % array->_nrAlloc;

#ifdef TRI_INTERNAL_STATS
    array->_nrProbesF++;
#endif
  }

  // ...........................................................................
  // return whatever we found
  // ...........................................................................

  return &array->_table[i];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds an element given an element, returns NULL if not found
////////////////////////////////////////////////////////////////////////////////

TRI_hash_index_element_t* TRI_FindByElementHashArray (TRI_hash_array_t* array,
                                                      TRI_hash_index_element_t* element) {
  TRI_hash_index_element_t* element2;

  element2 = TRI_LookupByElementHashArray(array, element);

  if (! IsEmptyElement(array, element2) && IsEqualElementElement(array, element2, element)) {
    return element2;
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element to the array
///
/// This function claims the owenship of the sub-objects in the inserted
/// element.
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertElementHashArray (TRI_hash_array_t* array,
                                TRI_hash_index_element_t* element,
                                bool overwrite) {
  uint64_t hash;
  uint64_t i;
  bool found;
  int res;
  TRI_hash_index_element_t* arrayElement;

  // ...........................................................................
  // compute the hash
  // ...........................................................................

  hash = HashElement(array, element);
  i = hash % array->_nrAlloc;

  // ...........................................................................
  // update statistics
  // ...........................................................................

#ifdef TRI_INTERNAL_STATS
  array->_nrAdds++;
#endif

  // ...........................................................................
  // search the table
  // ...........................................................................

  while (! IsEmptyElement(array, &array->_table[i])
      && ! IsEqualElementElement(array, element, &array->_table[i])) {
    i = (i + 1) % array->_nrAlloc;

#ifdef TRI_INTERNAL_STATS
    array->_nrProbesA++;
#endif
  }

  arrayElement = &array->_table[i];

  // ...........................................................................
  // if we found an element, return
  // ...........................................................................

  found = ! IsEmptyElement(array, arrayElement);

  if (found) {
    if (overwrite) {
      // destroy the underlying element since we are going to stomp on top if it
      DestroyElement(array, arrayElement);
      *arrayElement = *element;
    }
    else {
      DestroyElement(array, element);
    }

    return TRI_RESULT_ELEMENT_EXISTS;
  }

  // ...........................................................................
  // add a new element to the hash array (existing item is empty so no need to
  // destroy it)
  // ...........................................................................

  *arrayElement = *element;
  array->_nrUsed++;

  // ...........................................................................
  // we are adding and the table is more than half full, extend it
  // ...........................................................................

  if (array->_nrAlloc < 2 * array->_nrUsed) {
    res = ResizeHashArray(array);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an key/element to the array
///
/// This function claims the owenship of the sub-objects in the inserted
/// element.
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertKeyHashArray (TRI_hash_array_t* array,
                            TRI_index_search_value_t* key,
                            TRI_hash_index_element_t* element,
                            bool overwrite) {
  uint64_t hash;
  uint64_t i;
  bool found;
  int res;
  TRI_hash_index_element_t* arrayElement;

  // ...........................................................................
  // compute the hash
  // ...........................................................................

  hash = HashKey(array, key);
  i = hash % array->_nrAlloc;

  // ...........................................................................
  // update statistics
  // ...........................................................................

#ifdef TRI_INTERNAL_STATS
  array->_nrAdds++;
#endif

  // ...........................................................................
  // search the table
  // ...........................................................................

  while (! IsEmptyElement(array, &array->_table[i])
      && ! IsEqualKeyElement(array, key, &array->_table[i])) {
    i = (i + 1) % array->_nrAlloc;

#ifdef TRI_INTERNAL_STATS
    array->_nrProbesA++;
#endif
  }

  arrayElement = &array->_table[i];

  // ...........................................................................
  // if we found an element, return
  // ...........................................................................

  found = ! IsEmptyElement(array, arrayElement);

  if (found) {
    if (overwrite) {
      // destroy the underlying element since we are going to stomp on top if it
      DestroyElement(array, arrayElement);
      *arrayElement = *element;
    }
    else {
      DestroyElement(array, element);
    }

    return TRI_RESULT_KEY_EXISTS;
  }

  // ...........................................................................
  // add a new element to the associative array
  // ...........................................................................

  *arrayElement = *element;
  array->_nrUsed++;

  // ...........................................................................
  // we are adding and the table is more than half full, extend it
  // ...........................................................................

  if (array->_nrAlloc < 2 * array->_nrUsed) {
    res = ResizeHashArray(array);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from the array
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveElementHashArray (TRI_hash_array_t* array,
                                TRI_hash_index_element_t* element) {
  uint64_t hash;
  uint64_t i;
  uint64_t k;
  bool found;
  void* arrayElement;

  // ...........................................................................
  // compute the hash
  // ...........................................................................

  hash = HashElement(array, element);
  i = hash % array->_nrAlloc;

  // ...........................................................................
  // update statistics
  // ...........................................................................

#ifdef TRI_INTERNAL_STATS
  array->_nrRems++;
#endif

  // ...........................................................................
  // search the table
  // ...........................................................................

  while (! IsEmptyElement(array, &array->_table[i])
      && ! IsEqualElementElement(array, element, &array->_table[i])) {
    i = (i + 1) % array->_nrAlloc;

#ifdef TRI_INTERNAL_STATS
    array->_nrProbesD++;
#endif
  }

  arrayElement = &array->_table[i];

  // ...........................................................................
  // if we did not find such an item return false
  // ...........................................................................

  found = ! IsEmptyElement(array, arrayElement);

  if (! found) {
    return TRI_RESULT_ELEMENT_NOT_FOUND;
  }

  // ...........................................................................
  // remove item - destroy any internal memory associated with the element structure
  // ...........................................................................

  DestroyElement(array, arrayElement);
  array->_nrUsed--;

  // ...........................................................................
  // and now check the following places for items to move closer together
  // so that there are no gaps in the array
  // ...........................................................................

  k = (i + 1) % array->_nrAlloc;

  while (! IsEmptyElement(array, &array->_table[k])) {
    uint64_t j = HashElement(array, &array->_table[k]) % array->_nrAlloc;

    if ((i < k && !(i < j && j <= k)) || (k < i && !(i < j || j <= k))) {
      array->_table[i] = array->_table[k];
      ClearElement(array, &array->_table[k]);
      i = k;
    }

    k = (k + 1) % array->_nrAlloc;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an key from the array
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveKeyHashArray (TRI_hash_array_t* array,
                            TRI_index_search_value_t* key) {
  uint64_t hash;
  uint64_t i;
  uint64_t k;
  bool found;
  void* arrayElement;

  // ...........................................................................
  // compute the hash
  // ...........................................................................

  hash = HashKey(array, key);
  i = hash % array->_nrAlloc;

  // ...........................................................................
  // update statistics
  // ...........................................................................

#ifdef TRI_INTERNAL_STATS
  array->_nrRems++;
#endif

  // ...........................................................................
  // search the table
  // ...........................................................................

  while (! IsEmptyElement(array, &array->_table[i])
      && ! IsEqualKeyElement(array, key, &array->_table[i])) {
    i = (i + 1) % array->_nrAlloc;

#ifdef TRI_INTERNAL_STATS
    array->_nrProbesD++;
#endif
  }

  arrayElement = &array->_table[i];

  // ...........................................................................
  // if we did not find such an item return false
  // ...........................................................................

  found = ! IsEmptyElement(array, arrayElement);

  if (! found) {
    return TRI_RESULT_KEY_NOT_FOUND;
  }

  // ...........................................................................
  // remove item
  // ...........................................................................

  DestroyElement(array, arrayElement);
  array->_nrUsed--;

  // ...........................................................................
  // and now check the following places for items to move here
  // ...........................................................................

  k = (i + 1) % array->_nrAlloc;

  while (! IsEmptyElement(array, &array->_table[k])) {
    uint64_t j = HashElement(array, &array->_table[k]) % array->_nrAlloc;

    if ((i < k && !(i < j && j <= k)) || (k < i && !(i < j || j <= k))) {
      array->_table[i] = array->_table[k];
      ClearElement(array, &array->_table[k]);
      i = k;
    }

    k = (k + 1) % array->_nrAlloc;
  }

  // ...........................................................................
  // return success
  // ...........................................................................

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 HASH ARRAY MULTI
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup HashArray
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given a key
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t TRI_LookupByKeyHashArrayMulti (TRI_hash_array_t* array,
                                                    TRI_index_search_value_t* key) {
  TRI_vector_pointer_t result;
  uint64_t hash;
  uint64_t i;

  // ...........................................................................
  // initialise the vector which will hold the result if any
  // ...........................................................................

  TRI_InitVectorPointer(&result, TRI_UNKNOWN_MEM_ZONE);

  // ...........................................................................
  // compute the hash
  // ...........................................................................

  hash = HashKey(array, key);
  i = hash % array->_nrAlloc;

  // ...........................................................................
  // update statistics
  // ...........................................................................

#ifdef TRI_INTERNAL_STATS
  array->_nrFinds++;
#endif

  // ...........................................................................
  // search the table
  // ...........................................................................

  while (! IsEmptyElement(array, &array->_table[i])) {
    if (IsEqualKeyElement(array, key, &array->_table[i])) {
      TRI_PushBackVectorPointer(&result, &array->_table[i]);
    }

#ifdef TRI_INTERNAL_STATS
    else {
      array->_nrProbesF++;
    }
#endif

    i = (i + 1) % array->_nrAlloc;
  }

  // ...........................................................................
  // return whatever we found -- which could be an empty vector list if nothing
  // matches.
  // ...........................................................................

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given an element
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t TRI_LookupByElementHashArrayMulti (TRI_hash_array_t* array,
                                                        TRI_hash_index_element_t* element) {
  TRI_vector_pointer_t result;
  uint64_t hash;
  uint64_t i;

  // ...........................................................................
  // initialise the vector which will hold the result if any
  // ...........................................................................

  TRI_InitVectorPointer(&result, TRI_UNKNOWN_MEM_ZONE);

  // ...........................................................................
  // compute the hash
  // ...........................................................................

  hash = HashElement(array, element);
  i = hash % array->_nrAlloc;

  // ...........................................................................
  // update statistics
  // ...........................................................................

#ifdef TRI_INTERNAL_STATS
  array->_nrFinds++;
#endif

  // ...........................................................................
  // search the table
  // ...........................................................................

  while (! IsEmptyElement(array, &array->_table[i])) {
    if (IsEqualElementElement(array, element, &array->_table[i])) {
      TRI_PushBackVectorPointer(&result, &array->_table[i]);
    }

#ifdef TRI_INTERNAL_STATS
    else {
      array->_nrProbesF++;
    }
#endif

    i = (i + 1) % array->_nrAlloc;
  }

  // ...........................................................................
  // return whatever we found -- which could be an empty vector list if nothing
  // matches. Note that we allow multiple elements (compare with pointer impl).
  // ...........................................................................

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element to the array
///
/// This function claims the owenship of the sub-objects in the inserted
/// element.
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertElementHashArrayMulti (TRI_hash_array_t* array,
                                     TRI_hash_index_element_t* element,
                                     bool overwrite) {

  uint64_t hash;
  uint64_t i;
  bool found;
  int res;
  TRI_hash_index_element_t* arrayElement;

  // ...........................................................................
  // compute the hash
  // ...........................................................................

  hash = HashElement(array, element);
  i = hash % array->_nrAlloc;

  // ...........................................................................
  // update statistics
  // ...........................................................................

#ifdef TRI_INTERNAL_STATS
  array->_nrAdds++;
#endif

  // ...........................................................................
  // search the table
  // ...........................................................................

  while (! IsEmptyElement(array, &array->_table[i])
      && ! IsEqualElementElement(array, element, &array->_table[i])) {
    i = (i + 1) % array->_nrAlloc;

#ifdef TRI_INTERNAL_STATS
    array->_nrProbesA++;
#endif
  }

  arrayElement = &array->_table[i];

  // ...........................................................................
  // If we found an element, return. While we allow duplicate entries in the
  // hash table, we do not allow duplicate elements. Elements would refer to the
  // (for example) an actual row in memory. This is different from the
  // TRI_InsertElementMultiArray function below where we only have keys to
  // differentiate between elements.
  // ...........................................................................

  found = ! IsEmptyElement(array, arrayElement);

  if (found) {
    if (overwrite) {
      // destroy the underlying element since we are going to stomp on top if it
      DestroyElement(array, arrayElement);
      *arrayElement = *element;
    }
    else {
      DestroyElement(array, element);
    }

    return TRI_RESULT_ELEMENT_EXISTS;
  }

  // ...........................................................................
  // add a new element to the associative array
  // ...........................................................................

  *arrayElement = *element;
  array->_nrUsed++;

  // ...........................................................................
  // if we were adding and the table is more than half full, extend it
  // ...........................................................................

  if (array->_nrAlloc < 2 * array->_nrUsed) {
    res = ResizeHashArray(array);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a key/element to the array
///
/// This function claims the owenship of the sub-objects in the inserted
/// element.
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertKeyHashArrayMulti (TRI_hash_array_t* array,
                                 TRI_index_search_value_t* key,
                                 TRI_hash_index_element_t* element,
                                 bool overwrite) {
  uint64_t hash;
  uint64_t i;
  int res;
  TRI_hash_index_element_t* arrayElement;

  // ...........................................................................
  // compute the hash
  // ...........................................................................

  hash = HashKey(array, key);
  i = hash % array->_nrAlloc;

  // ...........................................................................
  // update statistics
  // ...........................................................................

#ifdef TRI_INTERNAL_STATS
  array->_nrAdds++;
#endif

  // ...........................................................................
  // search the table
  // ...........................................................................

  while (! IsEmptyElement(array, &array->_table[i])) {
    i = (i + 1) % array->_nrAlloc;

#ifdef TRI_INTERNAL_STATS
    array->_nrProbesA++;
#endif
  }

  arrayElement = &array->_table[i];

  // ...........................................................................
  // We do not look for an element (as opposed to the function above). So whether
  // or not there exists a duplicate we do not care.
  // ...........................................................................

  // ...........................................................................
  // add a new element to the associative array
  // ...........................................................................

  *arrayElement = * element;
  array->_nrUsed++;

  // ...........................................................................
  // if we were adding and the table is more than half full, extend it
  // ...........................................................................

  if (array->_nrAlloc < 2 * array->_nrUsed) {
    res = ResizeHashArray(array);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from the array
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveElementHashArrayMulti (TRI_hash_array_t* array,
                                     TRI_hash_index_element_t* element) {
  uint64_t hash;
  uint64_t i;
  uint64_t k;
  bool found;
  TRI_hash_index_element_t* arrayElement;

  // ...........................................................................
  // Obtain the hash
  // ...........................................................................

  hash = HashElement(array, element);
  i = hash % array->_nrAlloc;

  // ...........................................................................
  // update statistics
  // ...........................................................................

#ifdef TRI_INTERNAL_STATS
  array->_nrRems++;
#endif

  // ...........................................................................
  // search the table
  // ...........................................................................

  while (! IsEmptyElement(array, &array->_table[i])
      && ! IsEqualElementElement(array, element, &array->_table[i])) {
    i = (i + 1) % array->_nrAlloc;

#ifdef TRI_INTERNAL_STATS
    array->_nrProbesD++;
#endif
  }

  arrayElement = &array->_table[i];

  // ...........................................................................
  // if we did not find such an item return false
  // ...........................................................................

  found = ! IsEmptyElement(array, arrayElement);

  if (! found) {
    return TRI_RESULT_ELEMENT_NOT_FOUND;
  }

  // ...........................................................................
  // remove item
  // ...........................................................................

  DestroyElement(array, arrayElement);
  array->_nrUsed--;

  // ...........................................................................
  // and now check the following places for items to move here
  // ...........................................................................

  k = (i + 1) % array->_nrAlloc;

  while (! IsEmptyElement(array, &array->_table[k])) {
    uint64_t j = HashElement(array, &array->_table[k]) % array->_nrAlloc;

    if ((i < k && !(i < j && j <= k)) || (k < i && !(i < j || j <= k))) {
      array->_table[i] = array->_table[k];
      ClearElement(array, &array->_table[k]);
      i = k;
    }

    k = (k + 1) % array->_nrAlloc;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an key/element to the array
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveKeyHashArrayMulti (TRI_hash_array_t* array,
                                 TRI_index_search_value_t* key) {
  uint64_t hash;
  uint64_t i;
  uint64_t k;
  bool found;
  TRI_hash_index_element_t* arrayElement;

  // ...........................................................................
  // generate hash using key
  // ...........................................................................

  hash = HashKey(array, key);
  i = hash % array->_nrAlloc;

  // ...........................................................................
  // update statistics
  // ...........................................................................

#ifdef TRI_INTERNAL_STATS
  array->_nrRems++;
#endif

  // ...........................................................................
  // search the table -- we can only match keys
  // ...........................................................................

  while (! IsEmptyElement(array, &array->_table[i])
      && ! IsEqualKeyElement(array, key, &array->_table[i])) {
    i = (i + 1) % array->_nrAlloc;

#ifdef TRI_INTERNAL_STATS
    array->_nrProbesD++;
#endif
  }

  arrayElement = &array->_table[i];

  // ...........................................................................
  // if we did not find such an item return false
  // ...........................................................................

  found = ! IsEmptyElement(array, arrayElement);

  if (! found) {
    return TRI_RESULT_KEY_NOT_FOUND;
  }

  // ...........................................................................
  // remove item
  // ...........................................................................

  DestroyElement(array, arrayElement);
  array->_nrUsed--;

  // ...........................................................................
  // and now check the following places for items to move here
  // ...........................................................................

  k = (i + 1) % array->_nrAlloc;

  while (! IsEmptyElement(array, &array->_table[k])) {
    uint64_t j = HashElement(array, &array->_table[k]) % array->_nrAlloc;

    if ((i < k && !(i < j && j <= k)) || (k < i && !(i < j || j <= k))) {
      array->_table[i] = array->_table[k];
      ClearElement(array, &array->_table[k]);
      i = k;
    }

    k = (k + 1) % array->_nrAlloc;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

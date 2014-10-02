////////////////////////////////////////////////////////////////////////////////
/// @brief hash array implementation
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
/// @author Dr. Frank Celler
/// @author Dr. Oreste Costa-Panaia
/// @author Martin Schoenert
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2004-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "hash-array.h"

#include "Basics/fasthash.h"
#include "HashIndex/hash-index.h"
#include "VocBase/document-collection.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                        COMPARISON
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an element, removing any allocated memory
////////////////////////////////////////////////////////////////////////////////

static void DestroyElement (TRI_hash_array_t* array,
                            TRI_hash_index_element_t* element) {
  TRI_ASSERT_EXPENSIVE(element != nullptr);
  TRI_ASSERT_EXPENSIVE(element->_document != nullptr);

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, element->_subObjects);
  element->_document = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief determines if a key corresponds to an element
////////////////////////////////////////////////////////////////////////////////

static bool IsEqualKeyElement (TRI_hash_array_t* array,
                               TRI_index_search_value_t* left,
                               TRI_hash_index_element_t* right) {
  TRI_ASSERT_EXPENSIVE(right->_document != nullptr);

  for (size_t j = 0;  j < array->_numFields;  ++j) {
    TRI_shaped_json_t* leftJson = &left->_values[j];
    TRI_shaped_sub_t* rightSub = &right->_subObjects[j];

    if (leftJson->_sid != rightSub->_sid) {
      return false;
    }

    auto length = leftJson->_data.length;

    if (length != rightSub->_length) {
      return false;
    }

    if (0 < length) {
      char const* ptr = right->_document->getShapedJsonPtr() + rightSub->_offset;  // ONLY IN INDEX

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
  uint64_t hash = 0x0123456789abcdef;

  for (size_t j = 0;  j < array->_numFields;  ++j) {

    // ignore the sid for hashing
    hash = fasthash64(key->_values[j]._data.data, key->_values[j]._data.length, hash);
  }

  return hash;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief given an element generates a hash integer
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElement (TRI_hash_array_t* array,
                             TRI_hash_index_element_t* element) {
  uint64_t hash = 0x0123456789abcdef;

  for (size_t j = 0;  j < array->_numFields;  j++) {
    // ignore the sid for hashing
    char const* ptr = element->_document->getShapedJsonPtr() + element->_subObjects[j]._offset;  // ONLY IN INDEX

    // only hash the data block
    hash = fasthash64(ptr, element->_subObjects[j]._length, hash);
  }

  return hash;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                        HASH ARRAY
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                   private defines
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initial preallocation size of the hash table when the table is
/// first created
/// setting this to a high value will waste memory but reduce the number of
/// reallocations/repositionings necessary when the table grows
////////////////////////////////////////////////////////////////////////////////

static inline uint64_t InitialSize () {
  return 251;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return the size of a single entry
////////////////////////////////////////////////////////////////////////////////

static inline size_t TableEntrySize () {
  return sizeof(TRI_hash_index_element_t);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief allocate memory for the hash table
///
/// the hash table memory will be aligned on a cache line boundary
////////////////////////////////////////////////////////////////////////////////

static int AllocateTable (TRI_hash_array_t* array,
                          uint64_t numElements) {
  size_t const size = (size_t) (TableEntrySize() * numElements + 64);

  TRI_hash_index_element_t* table = static_cast<TRI_hash_index_element_t*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, size, true));

  if (table == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  array->_tablePtr = table;
  array->_table    = static_cast<TRI_hash_index_element_t*>(TRI_Align64(table));
  array->_nrAlloc  = numElements;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief resizes the array
////////////////////////////////////////////////////////////////////////////////

static int ResizeHashArray (TRI_hash_array_t* array,
                            uint64_t targetSize,
                            bool allowShrink) {
  if (array->_nrAlloc >= targetSize && ! allowShrink) {
    return TRI_ERROR_NO_ERROR;
  }

  TRI_hash_index_element_t* oldTable    = array->_table;
  TRI_hash_index_element_t* oldTablePtr = array->_tablePtr;
  uint64_t oldAlloc = array->_nrAlloc;

  TRI_ASSERT(targetSize > 0);

  int res = AllocateTable(array, targetSize);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  if (array->_nrUsed > 0) {
    uint64_t const n = array->_nrAlloc;

    for (uint64_t j = 0; j < oldAlloc; j++) {
      TRI_hash_index_element_t* element = &oldTable[j];

      if (element->_document != nullptr) {
        uint64_t i, k;
        i = k = HashElement(array, element) % n;

        for (; i < n && array->_table[i]._document != nullptr; ++i);
        if (i == n) {
          for (i = 0; i < k && array->_table[i]._document != nullptr; ++i);
        }

        TRI_ASSERT_EXPENSIVE(i < n);

        // ...........................................................................
        // add a new element to the associative array
        // memcpy ok here since are simply moving array items internally
        // ...........................................................................

        memcpy(&array->_table[i], element, TableEntrySize());
      }
    }
  }

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, oldTablePtr);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief triggers a resize if necessary
////////////////////////////////////////////////////////////////////////////////

static bool CheckResize (TRI_hash_array_t* array) {
  if (array->_nrAlloc < 2 * array->_nrUsed) {
    int res = ResizeHashArray(array, 2 * array->_nrAlloc + 1, false);

    if (res != TRI_ERROR_NO_ERROR) {
      return false;
    }
  }

  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises an array
////////////////////////////////////////////////////////////////////////////////

int TRI_InitHashArray (TRI_hash_array_t* array,
                       size_t numFields) {

  TRI_ASSERT(numFields > 0);

  array->_numFields = numFields;
  array->_tablePtr  = nullptr;
  array->_table     = nullptr;
  array->_nrUsed    = 0;
  array->_nrAlloc   = 0;

  return AllocateTable(array, InitialSize());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an array, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyHashArray (TRI_hash_array_t* array) {
  if (array == nullptr) {
    return;
  }

  // ...........................................................................
  // Go through each item in the array and remove any internal allocated memory
  // ...........................................................................

  // array->_table might be NULL if array initialisation fails
  if (array->_table != nullptr) {
    TRI_hash_index_element_t* p;
    TRI_hash_index_element_t* e;

    p = array->_table;
    e = p + array->_nrAlloc;

    for (;  p < e;  ++p) {
      if (p->_document != nullptr) {
        DestroyElement(array, p);
      }
    }

    TRI_Free(TRI_UNKNOWN_MEM_ZONE, array->_tablePtr);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an array and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeHashArray (TRI_hash_array_t* array) {
  if (array != nullptr) {
    TRI_DestroyHashArray(array);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, array);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief get the hash array's memory usage
////////////////////////////////////////////////////////////////////////////////

size_t TRI_MemoryUsageHashArray (TRI_hash_array_t const* array) {
  if (array == nullptr) {
    return 0;
  }

  size_t tableSize  = (size_t) (array->_nrAlloc * TableEntrySize() + 64);
  size_t memberSize = (size_t) (array->_nrUsed * array->_numFields * sizeof(TRI_shaped_sub_t));

  return (size_t) (tableSize + memberSize);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief resizes the hash table
////////////////////////////////////////////////////////////////////////////////

int TRI_ResizeHashArray (TRI_hash_array_t* array,
                         size_t size) {
  return ResizeHashArray(array, (uint64_t) (2 * size + 1), false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given a key
////////////////////////////////////////////////////////////////////////////////

TRI_hash_index_element_t* TRI_LookupByKeyHashArray (TRI_hash_array_t* array,
                                                    TRI_index_search_value_t* key) {
  uint64_t const n = array->_nrAlloc;
  uint64_t i, k;

  i = k = HashKey(array, key) % n;

  for (; i < n && array->_table[i]._document != nullptr && ! IsEqualKeyElement(array, key, &array->_table[i]); ++i);
  if (i == n) {
    for (i = 0; i < k && array->_table[i]._document != nullptr && ! IsEqualKeyElement(array, key, &array->_table[i]); ++i);
  }

  TRI_ASSERT_EXPENSIVE(i < n);

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
  TRI_hash_index_element_t* element = TRI_LookupByKeyHashArray(array, key);

  if (element != nullptr && element->_document != nullptr && IsEqualKeyElement(array, key, element)) {
    return element;
  }

  return nullptr;
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

  // ...........................................................................
  // we are adding and the table is more than half full, extend it
  // ...........................................................................

  if (! CheckResize(array)) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  const uint64_t n = array->_nrAlloc;
  uint64_t i, k;

  i = k = HashKey(array, key) % n;

  for (; i < n && array->_table[i]._document != nullptr && ! IsEqualKeyElement(array, key, &array->_table[i]); ++i);
  if (i == n) {
    for (i = 0; i < k && array->_table[i]._document != nullptr && ! IsEqualKeyElement(array, key, &array->_table[i]); ++i);
  }

  TRI_ASSERT_EXPENSIVE(i < n);

  TRI_hash_index_element_t* arrayElement = &array->_table[i];

  // ...........................................................................
  // if we found an element, return
  // ...........................................................................

  bool found = (arrayElement->_document != nullptr);

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

  *arrayElement = *element;
  array->_nrUsed++;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from the array
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveElementHashArray (TRI_hash_array_t* array,
                                TRI_hash_index_element_t* element) {
  uint64_t const n = array->_nrAlloc;
  uint64_t i, k;

  i = k = HashElement(array, element) % n;

  for (; i < n && array->_table[i]._document != nullptr && element->_document != array->_table[i]._document; ++i);
  if (i == n) {
    for (i = 0; i < k && array->_table[i]._document != nullptr && element->_document != array->_table[i]._document; ++i);
  }

  TRI_ASSERT_EXPENSIVE(i < n);

  TRI_hash_index_element_t* arrayElement = static_cast<TRI_hash_index_element_t*>(&array->_table[i]);

  // ...........................................................................
  // if we did not find such an item return false
  // ...........................................................................

  bool found = (arrayElement->_document != nullptr);

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

  k = TRI_IncModU64(i, n);

  while (array->_table[k]._document != nullptr) {
    uint64_t j = HashElement(array, &array->_table[k]) % n;

    if ((i < k && ! (i < j && j <= k)) || (k < i && ! (i < j || j <= k))) {
      array->_table[i] = array->_table[k];
      array->_table[k]._document = nullptr;
      i = k;
    }

    k = TRI_IncModU64(k, n);
  }

  if (array->_nrUsed == 0) {
    ResizeHashArray(array, InitialSize(), true);
  }

  return TRI_ERROR_NO_ERROR;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 HASH ARRAY MULTI
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given a key
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t TRI_LookupByKeyHashArrayMulti (TRI_hash_array_t* array,
                                                    TRI_index_search_value_t* key) {
  TRI_ASSERT_EXPENSIVE(array->_nrUsed < array->_nrAlloc);

  // ...........................................................................
  // initialise the vector which will hold the result if any
  // ...........................................................................

  TRI_vector_pointer_t result;
  TRI_InitVectorPointer(&result, TRI_UNKNOWN_MEM_ZONE);

  uint64_t const n = array->_nrAlloc;
  uint64_t i, k;

  i = k = HashKey(array, key) % n;

  for (; i < n && array->_table[i]._document != nullptr; ++i) {
    if (IsEqualKeyElement(array, key, &array->_table[i])) {
      TRI_PushBackVectorPointer(&result, &array->_table[i]);
    }
  }

  if (i == n) {
    for (i = 0; i < k && array->_table[i]._document != nullptr; ++i) {
      if (IsEqualKeyElement(array, key, &array->_table[i])) {
        TRI_PushBackVectorPointer(&result, &array->_table[i]);
      }
    }
  }

  TRI_ASSERT_EXPENSIVE(i < n);

  // ...........................................................................
  // return whatever we found -- which could be an empty vector list if nothing
  // matches.
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
  if (! CheckResize(array)) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  uint64_t const n = array->_nrAlloc;
  uint64_t i, k;

  i = k = HashElement(array, element) % n;

  for (; i < n && array->_table[i]._document != nullptr && element->_document != array->_table[i]._document; ++i);
  if (i == n) {
    for (i = 0; i < k && array->_table[i]._document != nullptr && element->_document != array->_table[i]._document; ++i);
  }

  TRI_ASSERT_EXPENSIVE(i < n);

  TRI_hash_index_element_t* arrayElement = &array->_table[i];

  // ...........................................................................
  // If we found an element, return. While we allow duplicate entries in the
  // hash table, we do not allow duplicate elements. Elements would refer to the
  // (for example) an actual row in memory. This is different from the
  // TRI_InsertElementMultiArray function below where we only have keys to
  // differentiate between elements.
  // ...........................................................................

  bool found = (arrayElement->_document != nullptr);

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

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from the array
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveElementHashArrayMulti (TRI_hash_array_t* array,
                                     TRI_hash_index_element_t* element) {
  uint64_t const n = array->_nrAlloc;
  uint64_t i, k;

  i = k = HashElement(array, element) % n;

  for (; i < n && array->_table[i]._document != nullptr && element->_document != array->_table[i]._document; ++i);
  if (i == n) {
    for (i = 0; i < k && array->_table[i]._document != nullptr && element->_document != array->_table[i]._document; ++i);
  }

  TRI_ASSERT_EXPENSIVE(i < n);

  TRI_hash_index_element_t* arrayElement = &array->_table[i];

  // ...........................................................................
  // if we did not find such an item return false
  // ...........................................................................

  bool found = (arrayElement->_document != nullptr);

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

  k = TRI_IncModU64(i, n);

  while (array->_table[k]._document != nullptr) {
    uint64_t j = HashElement(array, &array->_table[k]) % n;

    if ((i < k && ! (i < j && j <= k)) || (k < i && ! (i < j || j <= k))) {
      array->_table[i] = array->_table[k];
      array->_table[k]._document = nullptr;
      i = k;
    }

    k = TRI_IncModU64(k, n);
  }

  if (array->_nrUsed == 0) {
    ResizeHashArray(array, InitialSize(), true);
  }

  return TRI_ERROR_NO_ERROR;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

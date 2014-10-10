////////////////////////////////////////////////////////////////////////////////
/// @brief multi-hash array implementation, using a linked-list for collisions
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
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2004-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "hash-array-multi.h"

#include "Basics/fasthash.h"
#include "HashIndex/hash-index.h"
#include "VocBase/document-collection.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                        COMPARISON
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an element, removing any allocated memory
////////////////////////////////////////////////////////////////////////////////

static void DestroyElement (TRI_hash_array_multi_t* array,
                            TRI_hash_index_element_multi_t* element) {
  TRI_ASSERT_EXPENSIVE(element != nullptr);
  TRI_ASSERT_EXPENSIVE(element->_document != nullptr);

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, element->_subObjects);
  element->_document = nullptr;
  element->_next = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief determines if a key corresponds to an element
////////////////////////////////////////////////////////////////////////////////

static bool IsEqualKeyElement (TRI_hash_array_multi_t const* array,
                               TRI_index_search_value_t const* left,
                               TRI_hash_index_element_multi_t const* right) {
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

static uint64_t HashKey (TRI_hash_array_multi_t const* array,
                         TRI_index_search_value_t const* key) {
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

static uint64_t HashElement (TRI_hash_array_multi_t const* array,
                             TRI_hash_index_element_multi_t const* element) {
  uint64_t hash = 0x0123456789abcdef;
  char const* ptr = element->_document->getShapedJsonPtr(); // ONLY IN INDEX

  for (size_t j = 0;  j < array->_numFields;  j++) {
    // ignore the sid for hashing
    // only hash the data block
    hash = fasthash64(ptr + element->_subObjects[j]._offset, element->_subObjects[j]._length, hash);
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
  return sizeof(TRI_hash_index_element_multi_t);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief allocate memory for the hash table
///
/// the hash table memory will be aligned on a cache line boundary
////////////////////////////////////////////////////////////////////////////////

static int AllocateTable (TRI_hash_array_multi_t* array,
                          uint64_t numElements) {
  size_t const size = (size_t) (TableEntrySize() * numElements + 64);

  TRI_hash_index_element_multi_t* table = static_cast<TRI_hash_index_element_multi_t*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, size, true));

  if (table == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  array->_tablePtr = table;
  array->_table    = static_cast<TRI_hash_index_element_multi_t*>(TRI_Align64(table));
  array->_nrAlloc  = numElements;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief resizes the array
////////////////////////////////////////////////////////////////////////////////

static int ResizeHashArray (TRI_hash_array_multi_t* array,
                            uint64_t targetSize,
                            bool allowShrink) {
  if (array->_nrAlloc >= targetSize && ! allowShrink) {
    return TRI_ERROR_NO_ERROR;
  }

  TRI_hash_index_element_multi_t* oldTable    = array->_table;
  TRI_hash_index_element_multi_t* oldTablePtr = array->_tablePtr;
  uint64_t oldAlloc = array->_nrAlloc;

  TRI_ASSERT(targetSize > 0);

  int res = AllocateTable(array, targetSize);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  if (array->_nrUsed > 0) {
    uint64_t const n = array->_nrAlloc;

    for (uint64_t j = 0; j < oldAlloc; j++) {
      TRI_hash_index_element_multi_t* element = &oldTable[j];

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

static bool CheckResize (TRI_hash_array_multi_t* array) {
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

int TRI_InitHashArrayMulti (TRI_hash_array_multi_t* array,
                            size_t numFields) {

  TRI_ASSERT(numFields > 0);

  array->_numFields  = numFields;
  array->_tablePtr   = nullptr;
  array->_table      = nullptr;
  array->_nrUsed     = 0;
  array->_nrAlloc    = 0;
  array->_nrOverflow = 0;

  return AllocateTable(array, InitialSize());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an array, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyHashArrayMulti (TRI_hash_array_multi_t* array) {
  if (array == nullptr) {
    return;
  }

  // ...........................................................................
  // Go through each item in the array and remove any internal allocated memory
  // ...........................................................................

  // array->_table might be NULL if array initialisation fails
  if (array->_table != nullptr) {
    TRI_hash_index_element_multi_t* p;
    TRI_hash_index_element_multi_t* e;

    p = array->_table;
    e = p + array->_nrAlloc;

    for (;  p < e;  ++p) {
      auto current = p;

      while (current != nullptr && current->_document != nullptr) {
        auto ptr = current->_next;
        DestroyElement(array, current);
        if (current != p) {
          TRI_Free(TRI_UNKNOWN_MEM_ZONE, current);
        }
        current = ptr;
      }
    }

    TRI_Free(TRI_UNKNOWN_MEM_ZONE, array->_tablePtr);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an array and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeHashArrayMulti (TRI_hash_array_multi_t* array) {
  if (array != nullptr) {
    TRI_DestroyHashArrayMulti(array);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, array);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief get the hash array's memory usage
////////////////////////////////////////////////////////////////////////////////

size_t TRI_MemoryUsageHashArrayMulti (TRI_hash_array_multi_t const* array) {
  if (array == nullptr) {
    return 0;
  }

  size_t tableSize    = (size_t) (array->_nrAlloc * TableEntrySize() + 64);
  size_t memberSize   = (size_t) (array->_nrUsed * array->_numFields * sizeof(TRI_shaped_sub_t));
  size_t overflowSize = (size_t) (array->_nrOverflow * array->_numFields * sizeof(TRI_shaped_sub_t));

  return (size_t) (tableSize + memberSize + overflowSize);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief resizes the hash table
////////////////////////////////////////////////////////////////////////////////

int TRI_ResizeHashArrayMulti (TRI_hash_array_multi_t* array,
                              size_t size) {
  return ResizeHashArray(array, (uint64_t) (2 * size + 1), false);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given a key
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t TRI_LookupByKeyHashArrayMulti (TRI_hash_array_multi_t const* array,
                                                    TRI_index_search_value_t const* key) {
  TRI_ASSERT_EXPENSIVE(array->_nrUsed < array->_nrAlloc);

  // ...........................................................................
  // initialise the vector which will hold the result if any
  // ...........................................................................

  TRI_vector_pointer_t result;
  TRI_InitVectorPointer(&result, TRI_UNKNOWN_MEM_ZONE);

  uint64_t const n = array->_nrAlloc;
  uint64_t i, k;

  i = k = HashKey(array, key) % n;
  
  for (; i < n && array->_table[i]._document != nullptr && ! IsEqualKeyElement(array, key, &array->_table[i]); ++i);
  if (i == n) {
    for (i = 0; i < k && array->_table[i]._document != nullptr && ! IsEqualKeyElement(array, key, &array->_table[i]); ++i);
  }

  TRI_ASSERT_EXPENSIVE(i < n);

  if (array->_table[i]._document != nullptr) {
    auto current = &array->_table[i];
    while (current != nullptr) {
      if (IsEqualKeyElement(array, key, current)) {
        TRI_PushBackVectorPointer(&result, current);
      }
      current = current->_next;
    }
  }

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

int TRI_InsertElementHashArrayMulti (TRI_hash_array_multi_t* array,
                                     TRI_index_search_value_t const* key,
                                     TRI_hash_index_element_multi_t* element,
                                     bool isRollback) {
  if (! CheckResize(array)) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  element->_next = nullptr;

  uint64_t const n = array->_nrAlloc;
  uint64_t i, k;

  i = k = HashKey(array, key) % n;

  for (; i < n && array->_table[i]._document != nullptr && ! IsEqualKeyElement(array, key, &array->_table[i]); ++i);
  if (i == n) {
    for (i = 0; i < k && array->_table[i]._document != nullptr && ! IsEqualKeyElement(array, key, &array->_table[i]); ++i);
  }

  TRI_ASSERT_EXPENSIVE(i < n);

  TRI_hash_index_element_multi_t* arrayElement = &array->_table[i];

  // ...........................................................................
  // If we found an element, return. While we allow duplicate entries in the
  // hash table, we do not allow duplicate elements. Elements would refer to the
  // (for example) an actual row in memory. This is different from the
  // TRI_InsertElementMultiArray function below where we only have keys to
  // differentiate between elements.
  // ...........................................................................

  bool found = (arrayElement->_document != nullptr);

  if (found) {
    if (isRollback) {
      auto current = arrayElement;
      while (current != nullptr) {
        if (current->_document == element->_document) {
          DestroyElement(array, element);

          return TRI_RESULT_ELEMENT_EXISTS;
        }
        current = current->_next;
      }
    }

    auto ptr = static_cast<TRI_hash_index_element_multi_t*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_hash_index_element_multi_t), true));
    if (ptr == nullptr) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    element->_next = arrayElement->_next;
    *ptr = *element;
    arrayElement->_next = ptr;
    array->_nrOverflow++;

    return TRI_ERROR_NO_ERROR;
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

int TRI_RemoveElementHashArrayMulti (TRI_hash_array_multi_t* array,
                                     TRI_index_search_value_t const* key,
                                     TRI_hash_index_element_multi_t* element) {
  uint64_t const n = array->_nrAlloc;
  uint64_t i, k;

  i = k = HashKey(array, key) % n;

  for (; i < n && array->_table[i]._document != nullptr && ! IsEqualKeyElement(array, key, &array->_table[i]); ++i);
  if (i == n) {
    for (i = 0; i < k && array->_table[i]._document != nullptr && ! IsEqualKeyElement(array, key, &array->_table[i]); ++i);
  }

  TRI_ASSERT_EXPENSIVE(i < n);

  TRI_hash_index_element_multi_t* arrayElement = &array->_table[i];

  // ...........................................................................
  // if we did not find such an item return false
  // ...........................................................................

  bool found = (arrayElement->_document != nullptr);

  if (! found) {
    return TRI_RESULT_ELEMENT_NOT_FOUND;
  }

  if (arrayElement->_document != element->_document) {
    auto current = arrayElement;
    while (current->_next != nullptr) {
      if (current->_next->_document == element->_document) {
        auto ptr = current->_next->_next;
        DestroyElement(array, current->_next);
        TRI_Free(TRI_UNKNOWN_MEM_ZONE, current->_next);

        current->_next = ptr;
        array->_nrOverflow--;
      
        return TRI_ERROR_NO_ERROR;
      }
      current = current->_next;
    }
  }

  if (arrayElement->_next != nullptr) {
    auto ptr = arrayElement->_next;
    DestroyElement(array, arrayElement);

    *arrayElement = *ptr;
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, ptr);

    array->_nrOverflow--;

    return TRI_ERROR_NO_ERROR;
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

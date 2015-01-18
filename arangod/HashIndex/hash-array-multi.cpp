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
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief get a block size for hash elements batch allocation
////////////////////////////////////////////////////////////////////////////////

static inline size_t GetBlockSize (size_t blockNumber) {
  static size_t const BLOCK_SIZE_UNIT = 128;

  if (blockNumber >= 7) {
    blockNumber = 7;
  }
  return (size_t) (BLOCK_SIZE_UNIT << blockNumber);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the size of a single entry
////////////////////////////////////////////////////////////////////////////////

static inline size_t TableEntrySize () {
  return sizeof(TRI_hash_index_element_multi_t);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the size of a single overflow entry
////////////////////////////////////////////////////////////////////////////////

static inline size_t OverflowEntrySize () {
  return sizeof(TRI_hash_index_element_multi_t);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a storage location from the freelist
////////////////////////////////////////////////////////////////////////////////

static TRI_hash_index_element_multi_t* GetFromFreelist (TRI_hash_array_multi_t* array) {
  if (array->_freelist == nullptr) {
    size_t blockSize = GetBlockSize(array->_blocks._length);
    TRI_ASSERT(blockSize > 0);

    auto begin = static_cast<TRI_hash_index_element_multi_t*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, blockSize * OverflowEntrySize(), true));

    if (begin == nullptr) {
      return nullptr;
    }

    auto ptr = begin;
    auto end = begin + (blockSize - 1);

    while (ptr < end) {
      ptr->_next = (ptr + 1);
      ++ptr;
    }

    array->_freelist = begin;
    TRI_PushBackVectorPointer(&array->_blocks, begin);

    array->_nrOverflowAlloc += blockSize;
  }

  auto next = array->_freelist;
  TRI_ASSERT(next != nullptr);
  array->_freelist = next->_next;
  array->_nrOverflowUsed++;
   
  return next;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return an element to the freelist
////////////////////////////////////////////////////////////////////////////////

static void ReturnToFreelist (TRI_hash_array_multi_t* array,
                              TRI_hash_index_element_multi_t* element) {
  TRI_ASSERT(element->_subObjects == nullptr);

  element->_document   = nullptr;
  element->_subObjects = nullptr;
  element->_next       = array->_freelist;
  array->_freelist     = element;
  array->_nrOverflowUsed--;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an element, removing any allocated memory
////////////////////////////////////////////////////////////////////////////////

static void DestroyElement (TRI_hash_array_multi_t* array,
                            TRI_hash_index_element_multi_t* element) {
  TRI_ASSERT_EXPENSIVE(element != nullptr);
  TRI_ASSERT_EXPENSIVE(element->_document != nullptr);

  if (element->_subObjects != nullptr) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, element->_subObjects);
  }
  element->_document = nullptr;
  element->_subObjects = nullptr;
  element->_next = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initial preallocation size of the hash table when the table is
/// first created
/// setting this to a high value will waste memory but reduce the number of
/// reallocations/repositionings necessary when the table grows
////////////////////////////////////////////////////////////////////////////////

static inline uint64_t InitialSize () {
  return 251;
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

  array->_numFields       = numFields;
  array->_tablePtr        = nullptr;
  array->_table           = nullptr;
  array->_nrUsed          = 0;
  array->_nrAlloc         = 0;
  array->_nrOverflowUsed  = 0;
  array->_nrOverflowAlloc = 0;
  array->_freelist        = nullptr;

  TRI_InitVectorPointer2(&array->_blocks, TRI_UNKNOWN_MEM_ZONE, 16);

  return AllocateTable(array, InitialSize());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an array, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyHashArrayMulti (TRI_hash_array_multi_t* array) {
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
      if (p->_document != nullptr) {
        // destroy overflow elements
        auto current = p->_next;
        while (current != nullptr) {
          auto ptr = current->_next;
          DestroyElement(array, current);
          current = ptr;
        }

        // destroy the element itself
        DestroyElement(array, p);
      }
    }

    TRI_Free(TRI_UNKNOWN_MEM_ZONE, array->_tablePtr);
  }

  // free overflow elements
  for (size_t i = 0;  i < array->_blocks._length;  ++i) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, array->_blocks._buffer[i]);
  }

  TRI_DestroyVectorPointer(&array->_blocks);
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

  size_t tableSize     = (size_t) (array->_nrAlloc * TableEntrySize() + 64);
  size_t memberSize    = (size_t) (array->_nrUsed * array->_numFields * sizeof(TRI_shaped_sub_t));
  size_t overflowAlloc = (size_t) (array->_nrOverflowAlloc * OverflowEntrySize());

  return (size_t) (tableSize + memberSize + overflowAlloc);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief resizes the hash table
////////////////////////////////////////////////////////////////////////////////

int TRI_ResizeHashArrayMulti (TRI_hash_array_multi_t* array,
                              size_t size) {
  // use less than 1 element per number of documents
  // we does this because expect duplicate values, which are stored in the overflow
  // items (which are allocated separately)
  size_t targetSize = static_cast<size_t>(0.75 * size);
  if ((targetSize & 1) == 0) {
    // make odd
    targetSize++;
  }
  return ResizeHashArray(array, (uint64_t) targetSize, false);
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
    // add the element itself
    TRI_PushBackVectorPointer(&result, array->_table[i]._document);

    // add the overflow elements
    auto current = array->_table[i]._next;
    while (current != nullptr) {
      TRI_PushBackVectorPointer(&result, current->_document);
      current = current->_next;
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given a key
////////////////////////////////////////////////////////////////////////////////

int TRI_LookupByKeyHashArrayMulti (TRI_hash_array_multi_t const* array,
                                   TRI_index_search_value_t const* key,
                                   std::vector<TRI_doc_mptr_copy_t>& result) {
  TRI_ASSERT_EXPENSIVE(array->_nrUsed < array->_nrAlloc);

  uint64_t const n = array->_nrAlloc;
  uint64_t i, k;

  i = k = HashKey(array, key) % n;
  
  for (; i < n && array->_table[i]._document != nullptr && ! IsEqualKeyElement(array, key, &array->_table[i]); ++i);
  if (i == n) {
    for (i = 0; i < k && array->_table[i]._document != nullptr && ! IsEqualKeyElement(array, key, &array->_table[i]); ++i);
  }

  TRI_ASSERT_EXPENSIVE(i < n);

  if (array->_table[i]._document != nullptr) {
    // add the element itself
    result.emplace_back(*(array->_table[i]._document));

    // add the overflow elements
    auto current = array->_table[i]._next;
    while (current != nullptr) {
      result.emplace_back(*(current->_document));
      current = current->_next;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given a key and a state
////////////////////////////////////////////////////////////////////////////////

int TRI_LookupByKeyHashArrayMulti (TRI_hash_array_multi_t const* array,
                                   TRI_index_search_value_t const* key,
                                   std::vector<TRI_doc_mptr_copy_t>& result,
                                   TRI_hash_index_element_multi_t*& next,
                                   size_t batchSize) {
  size_t const initialSize = result.size();
  TRI_ASSERT_EXPENSIVE(array->_nrUsed < array->_nrAlloc);
  TRI_ASSERT(batchSize > 0);

  if (next == nullptr) {
    // no previous state. start at the beginning
    uint64_t const n = array->_nrAlloc;
    uint64_t i, k;

    i = k = HashKey(array, key) % n;
  
    for (; i < n && array->_table[i]._document != nullptr && ! IsEqualKeyElement(array, key, &array->_table[i]); ++i);
    if (i == n) {
      for (i = 0; i < k && array->_table[i]._document != nullptr && ! IsEqualKeyElement(array, key, &array->_table[i]); ++i);
    }

    TRI_ASSERT_EXPENSIVE(i < n);

    if (array->_table[i]._document != nullptr) {
      result.emplace_back(*(array->_table[i]._document));
    }
    next = array->_table[i]._next;
  }
  
  if (next != nullptr) {
    // we already had a state
    size_t total = result.size() - initialSize;

    while (next != nullptr && total < batchSize) {
      result.emplace_back(*(next->_document));
      next = next->_next;
      ++total;
    }
  }
    
  return TRI_ERROR_NO_ERROR;
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
      if (arrayElement->_document == element->_document) {
        DestroyElement(array, element);

        return TRI_RESULT_ELEMENT_EXISTS;
      }

      auto current = arrayElement->_next;
      while (current != nullptr) {
        if (current->_document == element->_document) {
          DestroyElement(array, element);

          return TRI_RESULT_ELEMENT_EXISTS;
        }
        current = current->_next;
      }
    }

    auto ptr = GetFromFreelist(array);

    if (ptr == nullptr) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    // link our element at the list head
    ptr->_document   = element->_document;
    ptr->_subObjects = element->_subObjects;
    ptr->_next       = arrayElement->_next;
    arrayElement->_next = ptr;
          
    // it is ok to destroy the element here, because we have copied its internal before!
    element->_subObjects = nullptr;
    DestroyElement(array, element);

    return TRI_ERROR_NO_ERROR;
  }
  
  TRI_ASSERT(arrayElement->_next == nullptr);

  // not found in list, now insert
  element->_next = nullptr;
  *arrayElement  = *element;
  array->_nrUsed++;
  
  TRI_ASSERT(arrayElement->_next == nullptr);

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

  bool found = (arrayElement->_document != nullptr);

  if (! found) {
    return TRI_RESULT_ELEMENT_NOT_FOUND;
  }
    
  if (arrayElement->_document != element->_document) {
    // look in the overflow list for the sought document
    auto next = &(arrayElement->_next);
    while (*next != nullptr) {
      if ((*next)->_document == element->_document) {
        auto ptr = (*next)->_next;
        DestroyElement(array, *next);
        ReturnToFreelist(array, *next);
        *next = ptr;

        return TRI_ERROR_NO_ERROR;
      }
      next = &((*next)->_next);
    }

    return TRI_RESULT_ELEMENT_NOT_FOUND;
  }

  // the element itself is the document to remove
  TRI_ASSERT(arrayElement->_document == element->_document);
  
  if (arrayElement->_next != nullptr) {
    auto next = arrayElement->_next;

    // destroy our own data first, otherwise we'll leak
    TRI_ASSERT(arrayElement->_subObjects != nullptr);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, arrayElement->_subObjects);

    // copy data from first overflow element into ourselves
    arrayElement->_document   = next->_document;
    arrayElement->_subObjects = next->_subObjects;
    arrayElement->_next       = next->_next;
 
    // and remove the first overflow element
    next->_subObjects = nullptr;
    DestroyElement(array, next);
    ReturnToFreelist(array, next);
        
    return TRI_ERROR_NO_ERROR;
  }

  TRI_ASSERT(arrayElement->_next == nullptr);

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
      array->_table[k]._document   = nullptr;
      array->_table[k]._next       = nullptr;
      array->_table[k]._subObjects = nullptr;
      i = k;
    }

    k = TRI_IncModU64(k, n);
  }

  if (array->_nrUsed == 0) {
    TRI_ASSERT(array->_nrOverflowUsed == 0);
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

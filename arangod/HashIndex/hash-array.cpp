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
#include "Basics/logging.h"
#include "HashIndex/hash-index-common.h"
#include "Indexes/HashIndex.h"
#include "Indexes/Index.h"
#include "VocBase/document-collection.h"
#include "VocBase/VocShaper.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                        COMPARISON
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief determines if a key corresponds to an element
////////////////////////////////////////////////////////////////////////////////

static bool IsEqualKeyElement (TRI_hash_array_t const* array,
                               TRI_index_search_value_t const* left,
                               TRI_hash_index_element_t const* right) {
  TRI_ASSERT_EXPENSIVE(right->_document != nullptr);

  for (size_t j = 0;  j < array->_numFields;  ++j) {
    TRI_shaped_json_t* leftJson = &left->_values[j];
    TRI_shaped_sub_t* rightSub = &right->_subObjects[j];

    if (leftJson->_sid != rightSub->_sid) {
      return false;
    }

    auto length = leftJson->_data.length;

    char const* rightData;
    size_t rightLength;
    TRI_InspectShapedSub(rightSub, right->_document, rightData, rightLength);

    if (length != rightLength) {
      return false;
    }

    if (length > 0 && memcmp(leftJson->_data.data, rightData, length) != 0) {
      return false;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief given a key generates a hash integer
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashKey (TRI_hash_array_t const* array,
                         TRI_index_search_value_t const* key) {
  uint64_t hash = 0x0123456789abcdef;

  for (size_t j = 0;  j < array->_numFields;  ++j) {
    // ignore the sid for hashing
    hash = fasthash64(key->_values[j]._data.data, key->_values[j]._data.length,
                      hash);
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
    char const* data;
    size_t length;
    TRI_InspectShapedSub(&element->_subObjects[j], element->_document, data, length);

    // ignore the sid for hashing
    // only hash the data block
    hash = fasthash64(data, length, hash);
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
/// @brief allocate memory for the hash table
////////////////////////////////////////////////////////////////////////////////

static int AllocateTable (TRI_hash_array_t* array,
                          uint64_t numElements) {
  size_t const size = static_cast<size_t>
                            (sizeof(TRI_hash_index_element_t*) * numElements);

  auto table = static_cast<TRI_hash_index_element_t**>
                     (TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, size, true));

  if (table == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  array->_table    = table;
  array->_nrAlloc  = numElements;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief resizes the array
////////////////////////////////////////////////////////////////////////////////

static int ResizeHashArray (triagens::arango::HashIndex* hashIndex,
                            TRI_hash_array_t* array,
                            uint64_t targetSize,
                            bool allowShrink) {
  if (array->_nrAlloc >= targetSize && ! allowShrink) {
    return TRI_ERROR_NO_ERROR;
  }
  
  // only log performance infos for indexes with more than this number of entries
  static uint64_t const NotificationSizeThreshold = 131072; 

  double start = TRI_microtime();
  if (targetSize > NotificationSizeThreshold) {
    LOG_ACTION("index-resize %s, target size: %llu", 
               hashIndex->context().c_str(),
               (unsigned long long) targetSize);
  }

  TRI_hash_index_element_t** oldTable    = array->_table;
  uint64_t oldAlloc = array->_nrAlloc;

  TRI_ASSERT(targetSize > 0);

  int res = AllocateTable(array, targetSize);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  if (array->_nrUsed > 0) {
    uint64_t const n = array->_nrAlloc;

    for (uint64_t j = 0; j < oldAlloc; j++) {
      TRI_hash_index_element_t* element = oldTable[j];

      if (element != nullptr) {
        uint64_t i, k;
        i = k = HashElement(array, element) % n;

        for (; i < n && array->_table[i] != nullptr; ++i);
        if (i == n) {
          for (i = 0; i < k && array->_table[i] != nullptr; ++i);
        }

        TRI_ASSERT_EXPENSIVE(i < n);

        // .....................................................................
        // add a new element to the associative array
        // memcpy ok here since are simply moving array items internally
        // .....................................................................

        array->_table[i] = element;
      }
    }
  }

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, oldTable);
  
  LOG_TIMER((TRI_microtime() - start),
            "index-resize %s, target size: %llu", 
            hashIndex->context().c_str(),
            (unsigned long long) targetSize);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief triggers a resize if necessary
////////////////////////////////////////////////////////////////////////////////

static bool CheckResize (triagens::arango::HashIndex* hashIndex,
                         TRI_hash_array_t* array) {
  if (array->_nrAlloc < 2 * array->_nrUsed) {
    int res = ResizeHashArray(hashIndex, array, 2 * array->_nrAlloc + 1, false);

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
    TRI_hash_index_element_t** p;
    TRI_hash_index_element_t** e;

    p = array->_table;
    e = p + array->_nrAlloc;

    for (;  p < e;  ++p) {
      if (*p != nullptr) {
        DestroyHashIndexElement(*p);
      }
    }

    TRI_Free(TRI_UNKNOWN_MEM_ZONE, array->_table);
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

  size_t tableSize  = (size_t) (array->_nrAlloc * 
                                sizeof(TRI_hash_index_element_t*));
  size_t memberSize = (size_t) (array->_nrUsed * 
     (sizeof(TRI_hash_index_element_t) + 
      array->_numFields * sizeof(TRI_shaped_sub_t)));

  return (size_t) (tableSize + memberSize);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief resizes the hash table
////////////////////////////////////////////////////////////////////////////////

int TRI_ResizeHashArray (triagens::arango::HashIndex* hashIndex,
                         TRI_hash_array_t* array,
                         size_t size) {
  return ResizeHashArray(hashIndex, array, (uint64_t) (2 * size + 1), false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds an element given a key, return NULL if not found
////////////////////////////////////////////////////////////////////////////////

TRI_hash_index_element_t* TRI_FindByKeyHashArray (TRI_hash_array_t const* array,
                                                  TRI_index_search_value_t* key) {
  uint64_t const n = array->_nrAlloc;
  uint64_t i, k;

  i = k = HashKey(array, key) % n;

  for (; i < n && array->_table[i] != nullptr && 
         ! IsEqualKeyElement(array, key, array->_table[i]); ++i);
  if (i == n) {
    for (i = 0; i < k && array->_table[i] != nullptr && 
                ! IsEqualKeyElement(array, key, array->_table[i]); ++i);
  }

  TRI_ASSERT_EXPENSIVE(i < n);

  // ...........................................................................
  // return whatever we found, this is nullptr if the thing was not found
  // and otherwise a valid pointer
  // ...........................................................................

  return array->_table[i];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an key/element to the array
///
/// This function claims the owenship of the sub-objects in the inserted
/// element.
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertKeyHashArray (triagens::arango::HashIndex* hashIndex,
                            TRI_hash_array_t* array,
                            TRI_index_search_value_t const* key,
                            TRI_hash_index_element_t* element,
                            bool isRollback) {

  // ...........................................................................
  // we are adding and the table is more than half full, extend it
  // ...........................................................................

  if (! CheckResize(hashIndex, array)) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  const uint64_t n = array->_nrAlloc;
  uint64_t i, k;

  i = k = HashKey(array, key) % n;

  for (; i < n && array->_table[i] != nullptr && 
         ! IsEqualKeyElement(array, key, array->_table[i]); ++i);
  if (i == n) {
    for (i = 0; i < k && array->_table[i] != nullptr && 
                ! IsEqualKeyElement(array, key, array->_table[i]); ++i);
  }

  TRI_ASSERT_EXPENSIVE(i < n);

  if (array->_table[i] != nullptr) {
    return TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED;
  }

  array->_table[i] = element;
  array->_nrUsed++;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from the array
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveElementHashArray (triagens::arango::HashIndex* hashIndex,
                                TRI_hash_array_t* array,
                                TRI_hash_index_element_t* element) {
  uint64_t const n = array->_nrAlloc;
  uint64_t i, k;

  i = k = HashElement(array, element) % n;

  for (; i < n && array->_table[i] != nullptr && 
         element->_document != array->_table[i]->_document; ++i);
  if (i == n) {
    for (i = 0; i < k && array->_table[i] != nullptr && 
                element->_document != array->_table[i]->_document; ++i);
  }

  TRI_ASSERT_EXPENSIVE(i < n);

  // ...........................................................................
  // if we did not find such an item return error code
  // ...........................................................................
 
  if (array->_table[i] == nullptr) {
    return TRI_RESULT_ELEMENT_NOT_FOUND;
  }

  // ...........................................................................
  // remove item - destroy any internal memory associated with the 
  // element structure
  // ...........................................................................

  DestroyHashIndexElement(array->_table[i]);
  array->_table[i] = nullptr;
  array->_nrUsed--;

  // ...........................................................................
  // and now check the following places for items to move closer together
  // so that there are no gaps in the array
  // ...........................................................................

  k = TRI_IncModU64(i, n);

  while (array->_table[k] != nullptr) {
    uint64_t j = HashElement(array, array->_table[k]) % n;

    if ((i < k && ! (i < j && j <= k)) || (k < i && ! (i < j || j <= k))) {
      array->_table[i] = array->_table[k];
      array->_table[k] = nullptr;
      i = k;
    }

    k = TRI_IncModU64(k, n);
  }

  if (array->_nrUsed == 0) {
    ResizeHashArray(hashIndex, array, InitialSize(), true);
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

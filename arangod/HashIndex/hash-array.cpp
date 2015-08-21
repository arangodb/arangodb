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
#include "Indexes/HashIndex.h"
#include "VocBase/document-collection.h"
#include "VocBase/VocShaper.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                        COMPARISON
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief determines if a key corresponds to an element
////////////////////////////////////////////////////////////////////////////////

bool TRI_hash_array_t::isEqualKeyElement (TRI_index_search_value_t const* left,
                                          TRI_index_element_t const* right) const {
  TRI_ASSERT_EXPENSIVE(right->document() != nullptr);

  for (size_t j = 0;  j < _numFields;  ++j) {
    TRI_shaped_json_t* leftJson = &left->_values[j];
    TRI_shaped_sub_t* rightSub = &right->subObjects()[j];

    if (leftJson->_sid != rightSub->_sid) {
      return false;
    }

    auto length = leftJson->_data.length;

    char const* rightData;
    size_t rightLength;
    TRI_InspectShapedSub(rightSub, right->document(), rightData, rightLength);

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

uint64_t TRI_hash_array_t::hashKey (TRI_index_search_value_t const* key) const {
  uint64_t hash = 0x0123456789abcdef;

  for (size_t j = 0;  j < _numFields;  ++j) {
    // ignore the sid for hashing
    hash = fasthash64(key->_values[j]._data.data, key->_values[j]._data.length,
                      hash);
  }

  return hash;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief given an element generates a hash integer
////////////////////////////////////////////////////////////////////////////////

uint64_t TRI_hash_array_t::hashElement (TRI_index_element_t const* element) const {
  uint64_t hash = 0x0123456789abcdef;

  for (size_t j = 0;  j < _numFields;  j++) {
    char const* data;
    size_t length;
    TRI_InspectShapedSub(&element->subObjects()[j], element->document(), data, length);

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
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief resizes the array
////////////////////////////////////////////////////////////////////////////////

int TRI_hash_array_t::resizeInternal (triagens::arango::HashIndex* hashIndex,
                                      Bucket& b,
                                      uint64_t targetSize,
                                      bool allowShrink) {

  if (b._nrAlloc >= targetSize && ! allowShrink) {
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

  TRI_index_element_t** oldTable    = b._table;
  uint64_t oldAlloc = b._nrAlloc;

  TRI_ASSERT(targetSize > 0);

  try {
    b._table = new TRI_index_element_t* [targetSize];
  }
  catch (...) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  b._nrAlloc = targetSize;

  if (b._nrUsed > 0) {
    uint64_t const n = b._nrAlloc;

    for (uint64_t j = 0; j < oldAlloc; j++) {
      TRI_index_element_t* element = oldTable[j];

      if (element != nullptr) {
        uint64_t i, k;
        i = k = hashElement(element) % n;

        for (; i < n && b._table[i] != nullptr; ++i);
        if (i == n) {
          for (i = 0; i < k && b._table[i] != nullptr; ++i);
        }

        // .....................................................................
        // add a new element to the associative array
        // memcpy ok here since are simply moving array items internally
        // .....................................................................

        b._table[i] = element;
      }
    }
  }

  delete [] oldTable;
  
  LOG_TIMER((TRI_microtime() - start),
            "index-resize %s, target size: %llu", 
            hashIndex->context().c_str(),
            (unsigned long long) targetSize);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief triggers a resize if necessary
////////////////////////////////////////////////////////////////////////////////

bool TRI_hash_array_t::checkResize (triagens::arango::HashIndex* hashIndex,
                                    Bucket& b) {
  if (2 * b._nrAlloc < 3 * b._nrUsed) {
    int res = resizeInternal(hashIndex, b, 2 * b._nrAlloc + 1, false);

    if (res != TRI_ERROR_NO_ERROR) {
      return false;
    }
  }

  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief get the hash array's memory usage
////////////////////////////////////////////////////////////////////////////////

size_t TRI_hash_array_t::memoryUsage () {
  size_t sum = 0;
  for (auto& b : _buckets) {
    sum += (size_t) (b._nrAlloc * sizeof(TRI_index_element_t*));
  }
  return sum;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief resizes the hash table
////////////////////////////////////////////////////////////////////////////////

int TRI_hash_array_t::resize (triagens::arango::HashIndex* hashIndex,
                              size_t size) {
  int res = TRI_ERROR_NO_ERROR;
  for (auto& b : _buckets) {
    res = resizeInternal(hashIndex, b,
                         (uint64_t) (3 * size / 2 + 1) / _buckets.size(), 
                         false);
    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds an element given a key, return NULL if not found
////////////////////////////////////////////////////////////////////////////////

TRI_index_element_t* TRI_hash_array_t::findByKey (TRI_index_search_value_t* key) const {
  uint64_t i = hashKey(key);
  Bucket const& b = _buckets[i & _bucketsMask];

  uint64_t const n = b._nrAlloc;
  i = i % n;
  uint64_t k = i;

  for (; i < n && b._table[i] != nullptr && 
         ! isEqualKeyElement(key, b._table[i]); ++i);
  if (i == n) {
    for (i = 0; i < k && b._table[i] != nullptr && 
                ! isEqualKeyElement(key, b._table[i]); ++i);
  }

  // ...........................................................................
  // return whatever we found, this is nullptr if the thing was not found
  // and otherwise a valid pointer
  // ...........................................................................

  return b._table[i];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an key/element to the array
///
/// This function claims the owenship of the sub-objects in the inserted
/// element.
////////////////////////////////////////////////////////////////////////////////

int TRI_hash_array_t::insert (triagens::arango::HashIndex* hashIndex,
                              TRI_index_search_value_t const* key,
                              TRI_index_element_t const* element,
                              bool isRollback) {
  // ...........................................................................
  // we are adding and the table is more than half full, extend it
  // ...........................................................................

  uint64_t i = hashKey(key);
  Bucket& b = _buckets[i & _bucketsMask];

  if (! checkResize(hashIndex, b)) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  uint64_t const n = b._nrAlloc;
  i = i % n;
  uint64_t k = i;

  for (; i < n && b._table[i] != nullptr && 
         ! isEqualKeyElement(key, b._table[i]); ++i);
  if (i == n) {
    for (i = 0; i < k && b._table[i] != nullptr && 
                ! isEqualKeyElement(key, b._table[i]); ++i);
  }

  TRI_index_element_t* arrayElement = b._table[i];

  // ...........................................................................
  // if we found an element, return
  // ...........................................................................

  if (arrayElement != nullptr) {
    return TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED;
  }

  b._table[i] = const_cast<TRI_index_element_t*>(element);
  TRI_ASSERT(b._table[i] != nullptr && b._table[i]->document() != nullptr);
  b._nrUsed++;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from the array
////////////////////////////////////////////////////////////////////////////////

int TRI_hash_array_t::remove (triagens::arango::HashIndex* hashIndex,
                              TRI_index_element_t* element) {
  uint64_t i = hashElement(element);
  Bucket& b = _buckets[i & _bucketsMask];

  uint64_t const n = b._nrAlloc;
  i = i % n;
  uint64_t k = i;

  for (; i < n && b._table[i] != nullptr && 
         element->document() != b._table[i]->document(); ++i);
  if (i == n) {
    for (i = 0; i < k && b._table[i] != nullptr && 
                element->document() != b._table[i]->document(); ++i);
  }

  TRI_index_element_t* arrayElement = b._table[i];

  // ...........................................................................
  // if we did not find such an item return error code
  // ...........................................................................

  if (arrayElement == nullptr) {
    return TRI_RESULT_ELEMENT_NOT_FOUND;
  }

  // ...........................................................................
  // remove item - destroy any internal memory associated with the 
  // element structure
  // ...........................................................................

  TRI_index_element_t::free(arrayElement);
  b._table[i] = nullptr;
  b._nrUsed--;

  // ...........................................................................
  // and now check the following places for items to move closer together
  // so that there are no gaps in the array
  // ...........................................................................

  k = TRI_IncModU64(i, n);

  while (b._table[k] != nullptr) {
    uint64_t j = hashElement(b._table[k]) % n;

    if ((i < k && ! (i < j && j <= k)) || (k < i && ! (i < j || j <= k))) {
      b._table[i] = b._table[k];
      b._table[k] = nullptr;
      i = k;
    }

    k = TRI_IncModU64(k, n);
  }

  if (b._nrUsed == 0) {
    resizeInternal (hashIndex, b, initialSize(), true);
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

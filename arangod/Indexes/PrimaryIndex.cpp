////////////////////////////////////////////////////////////////////////////////
/// @brief primary index
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "PrimaryIndex.h"
#include "Basics/Exceptions.h"
#include "Basics/hashes.h"
#include "Basics/logging.h"
#include "VocBase/document-collection.h"
#include "VocBase/transaction.h"

using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief comparison function, compares a master pointer to another
////////////////////////////////////////////////////////////////////////////////

static inline bool IsDifferentKeyElement (TRI_doc_mptr_t const* header,
                                          void const* element) {
  TRI_doc_mptr_t const* e = static_cast<TRI_doc_mptr_t const*>(element);

  // only after that compare actual keys
  return (header->_hash != e->_hash || strcmp(TRI_EXTRACT_MARKER_KEY(header), TRI_EXTRACT_MARKER_KEY(e)) != 0);  // ONLY IN INDEX, PROTECTED by RUNTIME
}

////////////////////////////////////////////////////////////////////////////////
/// @brief comparison function, compares a hash/key to a master pointer
////////////////////////////////////////////////////////////////////////////////

static inline bool IsDifferentHashElement (char const* key, uint64_t hash, void const* element) {
  TRI_doc_mptr_t const* e = static_cast<TRI_doc_mptr_t const*>(element);

  return (hash != e->_hash || strcmp(key, TRI_EXTRACT_MARKER_KEY(e)) != 0);  // ONLY IN INDEX, PROTECTED by RUNTIME
}

// -----------------------------------------------------------------------------
// --SECTION--                                                class PrimaryIndex
// -----------------------------------------------------------------------------
        
uint64_t const PrimaryIndex::InitialSize = 251;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

PrimaryIndex::PrimaryIndex (TRI_document_collection_t* collection) 
  : Index(0, std::vector<std::string>( { TRI_VOC_ATTRIBUTE_KEY } )),
    _collection(collection) {

  _primaryIndex._nrAlloc = 0;
  _primaryIndex._nrUsed  = 0;
  _primaryIndex._table   = static_cast<void**>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, static_cast<size_t>(InitialSize * sizeof(void*)), true));

  if (_primaryIndex._table == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  _primaryIndex._nrAlloc = InitialSize;
}

PrimaryIndex::~PrimaryIndex () {
  if (_primaryIndex._table != nullptr) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, _primaryIndex._table);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------
        
size_t PrimaryIndex::memory () const {
  return static_cast<size_t>(_primaryIndex._nrAlloc * sizeof(void*));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a JSON representation of the index
////////////////////////////////////////////////////////////////////////////////

triagens::basics::Json PrimaryIndex::toJson (TRI_memory_zone_t* zone) const {
  auto json = Index::toJson(zone);

  // hard-coded
  json("unique", triagens::basics::Json(true))
      ("sparse", triagens::basics::Json(false));

  return json;
}

int PrimaryIndex::insert (TRI_doc_mptr_t const*, 
                          bool) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
         
int PrimaryIndex::remove (TRI_doc_mptr_t const*, 
                          bool) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an element given a key
////////////////////////////////////////////////////////////////////////////////

void* PrimaryIndex::lookupKey (char const* key) const {
  if (_primaryIndex._nrUsed == 0) {
    return nullptr;
  }

  // compute the hash
  uint64_t const hash = calculateHash(key);
  uint64_t const n = _primaryIndex._nrAlloc;
  uint64_t i, k;

  i = k = hash % n;

  TRI_ASSERT_EXPENSIVE(n > 0);

  // search the table
  for (; i < n && _primaryIndex._table[i] != nullptr && IsDifferentHashElement(key, hash, _primaryIndex._table[i]); ++i);
  if (i == n) {
    for (i = 0; i < k && _primaryIndex._table[i] != nullptr && IsDifferentHashElement(key, hash, _primaryIndex._table[i]); ++i);
  }

  TRI_ASSERT_EXPENSIVE(i < n);

  // return whatever we found
  return _primaryIndex._table[i];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a key/element to the index
/// returns a status code, and *found will contain a found element (if any)
////////////////////////////////////////////////////////////////////////////////

int PrimaryIndex::insertKey (TRI_doc_mptr_t const* header,
                             void const** found) {
  *found = nullptr;

  if (shouldResize()) {
    // check for out-of-memory
    if (! resize(static_cast<uint64_t>(2 * _primaryIndex._nrAlloc + 1), false)) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }
  }

  uint64_t const n = _primaryIndex._nrAlloc;
  uint64_t i, k;

  TRI_ASSERT_EXPENSIVE(n > 0);

  i = k = header->_hash % n;

  for (; i < n && _primaryIndex._table[i] != nullptr && IsDifferentKeyElement(header, _primaryIndex._table[i]); ++i);
  if (i == n) {
    for (i = 0; i < k && _primaryIndex._table[i] != nullptr && IsDifferentKeyElement(header, _primaryIndex._table[i]); ++i);
  }

  TRI_ASSERT_EXPENSIVE(i < n);

  void* old = _primaryIndex._table[i];

  // if we found an element, return
  if (old != nullptr) {
    *found = old;

    return TRI_ERROR_NO_ERROR;
  }

  // add a new element to the associative idx
  _primaryIndex._table[i] = (void*) header;
  ++_primaryIndex._nrUsed;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a key/element to the index
/// this is a special, optimized (read: reduced) variant of the above insert
/// function 
////////////////////////////////////////////////////////////////////////////////

void PrimaryIndex::insertKey (TRI_doc_mptr_t const* header) {
  uint64_t const n = _primaryIndex._nrAlloc;
  uint64_t i, k;

  i = k = header->_hash % n;

  for (; i < n && _primaryIndex._table[i] != nullptr && IsDifferentKeyElement(header, _primaryIndex._table[i]); ++i);
  if (i == n) {
    for (i = 0; i < k && _primaryIndex._table[i] != nullptr && IsDifferentKeyElement(header, _primaryIndex._table[i]); ++i);
  }

  TRI_ASSERT_EXPENSIVE(i < n);

  TRI_ASSERT_EXPENSIVE(_primaryIndex._table[i] == nullptr);

  _primaryIndex._table[i] = const_cast<void*>(static_cast<void const*>(header));
  ++_primaryIndex._nrUsed;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an key/element from the index
////////////////////////////////////////////////////////////////////////////////

void* PrimaryIndex::removeKey (char const* key) {
  uint64_t const hash = calculateHash(key);
  uint64_t const n = _primaryIndex._nrAlloc;
  uint64_t i, k;

  i = k = hash % n;

  // search the table
  for (; i < n && _primaryIndex._table[i] != nullptr && IsDifferentHashElement(key, hash, _primaryIndex._table[i]); ++i);
  if (i == n) {
    for (i = 0; i < k && _primaryIndex._table[i] != nullptr && IsDifferentHashElement(key, hash, _primaryIndex._table[i]); ++i);
  }

  TRI_ASSERT_EXPENSIVE(i < n);

  // if we did not find such an item return false
  if (_primaryIndex._table[i] == nullptr) {
    return nullptr;
  }

  // remove item
  void* old = _primaryIndex._table[i];
  _primaryIndex._table[i] = nullptr;
  _primaryIndex._nrUsed--;

  // and now check the following places for items to move here
  k = TRI_IncModU64(i, n);

  while (_primaryIndex._table[k] != nullptr) {
    uint64_t j = (static_cast<TRI_doc_mptr_t const*>(_primaryIndex._table[k])->_hash) % n;

    if ((i < k && ! (i < j && j <= k)) || (k < i && ! (i < j || j <= k))) {
      _primaryIndex._table[i] = _primaryIndex._table[k];
      _primaryIndex._table[k] = nullptr;
      i = k;
    }

    k = TRI_IncModU64(k, n);
  }

  if (_primaryIndex._nrUsed == 0) {
    resize(InitialSize, true);
  }

  // return success
  return old;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief resizes the index
////////////////////////////////////////////////////////////////////////////////

int PrimaryIndex::resize (size_t targetSize) {
  if (! resize(static_cast<uint64_t>(2 * targetSize + 1), false)) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief resize the index to a good size if too small
////////////////////////////////////////////////////////////////////////////////

int PrimaryIndex::resize () {
  if (shouldResize() &&
      ! resize(static_cast<uint64_t>(2 * _primaryIndex._nrAlloc + 1), false)) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  return TRI_ERROR_NO_ERROR;
}
  
uint64_t PrimaryIndex::calculateHash (char const* key) {
  return TRI_FnvHashString(key);
}

uint64_t PrimaryIndex::calculateHash (char const* key,
                                      size_t length) {
  return TRI_FnvHashPointer(static_cast<void const*>(key), length);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the index must be resized
////////////////////////////////////////////////////////////////////////////////

bool PrimaryIndex::shouldResize () const {
  return _primaryIndex._nrAlloc < _primaryIndex._nrUsed + _primaryIndex._nrUsed;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief resizes the index
////////////////////////////////////////////////////////////////////////////////

bool PrimaryIndex::resize (uint64_t targetSize,
                           bool allowShrink) {
  TRI_ASSERT(targetSize > 0);

  if (_primaryIndex._nrAlloc >= targetSize && ! allowShrink) {
    return true;
  }

  void** oldTable = _primaryIndex._table;
  
  // only log performance infos for indexes with more than this number of entries
  static uint64_t const NotificationSizeThreshold = 131072; 

  double start = TRI_microtime();
  if (targetSize > NotificationSizeThreshold) {
    LOG_ACTION("primary-index-resize, target size: %llu", 
               (unsigned long long) targetSize);
  }

  _primaryIndex._table = static_cast<void**>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, (size_t) (targetSize * sizeof(void*)), true));

  if (_primaryIndex._table == nullptr) {
    _primaryIndex._table = oldTable;

    return false;
  }

  if (_primaryIndex._nrUsed > 0) {
    uint64_t const oldAlloc = _primaryIndex._nrAlloc;

    // table is already cleared by allocate, now copy old data
    for (uint64_t j = 0; j < oldAlloc; j++) {
      TRI_doc_mptr_t const* element = static_cast<TRI_doc_mptr_t const*>(oldTable[j]);

      if (element != nullptr) {
        uint64_t const hash = element->_hash;
        uint64_t i, k;

        i = k = hash % targetSize;

        for (; i < targetSize && _primaryIndex._table[i] != nullptr; ++i);
        if (i == targetSize) {
          for (i = 0; i < k && _primaryIndex._table[i] != nullptr; ++i);
        }

        TRI_ASSERT_EXPENSIVE(i < targetSize);

        _primaryIndex._table[i] = (void*) element;
      }
    }
  }

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, oldTable);
  _primaryIndex._nrAlloc = targetSize;

  LOG_TIMER((TRI_microtime() - start),
            "primary-index-resize, target size: %llu", 
            (unsigned long long) targetSize);

  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

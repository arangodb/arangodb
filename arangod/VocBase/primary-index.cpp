////////////////////////////////////////////////////////////////////////////////
/// @brief primary hash index functionality
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
/// @author Martin Schoenert
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2006-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "primary-index.h"

#include "Basics/hashes.h"
#include "VocBase/document-collection.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initial number of elements in the index
////////////////////////////////////////////////////////////////////////////////

static inline uint64_t InitialSize () {
  return 251;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief resizes the index
////////////////////////////////////////////////////////////////////////////////

static bool ResizePrimaryIndex (TRI_primary_index_t* idx,
                                uint64_t targetSize,
                                bool allowShrink) {
  TRI_ASSERT(targetSize > 0);

  if (idx->_nrAlloc >= targetSize && ! allowShrink) {
    return true;
  }

  void** oldTable = idx->_table;

  idx->_table = static_cast<void**>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, (size_t) (targetSize * sizeof(void*)), true));

  if (idx->_table == nullptr) {
    idx->_table = oldTable;

    return false;
  }

  if (idx->_nrUsed > 0) {
    uint64_t const oldAlloc = idx->_nrAlloc;

    // table is already cleared by allocate, now copy old data
    for (uint64_t j = 0; j < oldAlloc; j++) {
      TRI_doc_mptr_t const* element = static_cast<TRI_doc_mptr_t const*>(oldTable[j]);

      if (element != nullptr) {
        uint64_t const hash = element->_hash;
        uint64_t i, k;

        i = k = hash % targetSize;

        for (; i < targetSize && idx->_table[i] != nullptr; ++i);
        if (i == targetSize) {
          for (i = 0; i < k && idx->_table[i] != nullptr; ++i);
        }

        TRI_ASSERT_EXPENSIVE(i < targetSize);

        idx->_table[i] = (void*) element;
      }
    }
  }

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, oldTable);
  idx->_nrAlloc = targetSize;

  return true;
}

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
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises the index
////////////////////////////////////////////////////////////////////////////////

int TRI_InitPrimaryIndex (TRI_primary_index_t* idx) {
  idx->_nrAlloc = 0;
  idx->_nrUsed  = 0;

  idx->_table = static_cast<void**>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, (size_t) (InitialSize() * sizeof(void*)), true));

  if (idx->_table == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  idx->_nrAlloc = InitialSize();

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an index, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyPrimaryIndex (TRI_primary_index_t* idx) {
  if (idx->_table != nullptr) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, idx->_table);
    idx->_table = nullptr;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an element given a key
////////////////////////////////////////////////////////////////////////////////

void* TRI_LookupByKeyPrimaryIndex (TRI_primary_index_t* idx,
                                   char const* key) {
  if (idx->_nrUsed == 0) {
    return nullptr;
  }

  // compute the hash
  uint64_t const hash = TRI_HashKeyPrimaryIndex(key);
  uint64_t const n = idx->_nrAlloc;
  uint64_t i, k;

  i = k = hash % n;

  TRI_ASSERT_EXPENSIVE(n > 0);

  // search the table
  for (; i < n && idx->_table[i] != nullptr && IsDifferentHashElement(key, hash, idx->_table[i]); ++i);
  if (i == n) {
    for (i = 0; i < k && idx->_table[i] != nullptr && IsDifferentHashElement(key, hash, idx->_table[i]); ++i);
  }

  TRI_ASSERT_EXPENSIVE(i < n);

  // return whatever we found
  return idx->_table[i];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a key/element to the index
/// returns a status code, and *found will contain a found element (if any)
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertKeyPrimaryIndex (TRI_primary_index_t* idx,
                               TRI_doc_mptr_t const* header,
                               void const** found) {
  *found = nullptr;

  if (idx->_nrAlloc < 2 * idx->_nrUsed) {
    // check for out-of-memory
    if (! ResizePrimaryIndex(idx, (uint64_t) (2 * idx->_nrAlloc + 1), false)) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }
  }

  uint64_t const n = idx->_nrAlloc;
  uint64_t i, k;

  TRI_ASSERT_EXPENSIVE(n > 0);

  i = k = header->_hash % n;

  for (; i < n && idx->_table[i] != nullptr && IsDifferentKeyElement(header, idx->_table[i]); ++i);
  if (i == n) {
    for (i = 0; i < k && idx->_table[i] != nullptr && IsDifferentKeyElement(header, idx->_table[i]); ++i);
  }

  TRI_ASSERT_EXPENSIVE(i < n);

  void* old = idx->_table[i];

  // if we found an element, return
  if (old != nullptr) {
    *found = old;

    return TRI_ERROR_NO_ERROR;
  }

  // add a new element to the associative idx
  idx->_table[i] = (void*) header;
  ++idx->_nrUsed;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an key/element from the index
////////////////////////////////////////////////////////////////////////////////

void* TRI_RemoveKeyPrimaryIndex (TRI_primary_index_t* idx,
                                 char const* key) {
  uint64_t const hash = TRI_HashKeyPrimaryIndex(key);
  uint64_t const n = idx->_nrAlloc;
  uint64_t i, k;

  i = k = hash % n;

  // search the table
  for (; i < n && idx->_table[i] != nullptr && IsDifferentHashElement(key, hash, idx->_table[i]); ++i);
  if (i == n) {
    for (i = 0; i < k && idx->_table[i] != nullptr && IsDifferentHashElement(key, hash, idx->_table[i]); ++i);
  }

  TRI_ASSERT_EXPENSIVE(i < n);

  // if we did not find such an item return false
  if (idx->_table[i] == nullptr) {
    return nullptr;
  }

  // remove item
  void* old = idx->_table[i];
  idx->_table[i] = nullptr;
  idx->_nrUsed--;

  // and now check the following places for items to move here
  k = TRI_IncModU64(i, n);

  while (idx->_table[k] != nullptr) {
    uint64_t j = (static_cast<TRI_doc_mptr_t const*>(idx->_table[k])->_hash) % n;

    if ((i < k && ! (i < j && j <= k)) || (k < i && ! (i < j || j <= k))) {
      idx->_table[i] = idx->_table[k];
      idx->_table[k] = nullptr;
      i = k;
    }

    k = TRI_IncModU64(k, n);
  }

  if (idx->_nrUsed == 0) {
    ResizePrimaryIndex(idx, InitialSize(), true);
  }

  // return success
  return old;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

////////////////////////////////////////////////////////////////////////////////
/// @brief primary hash index functionality
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

#include "primary-index.h"

#include "BasicsC/hashes.h"
#include "VocBase/document-collection.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                   private defines
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initial number of elements in the index
////////////////////////////////////////////////////////////////////////////////

#define INITIAL_SIZE (11)

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new element
////////////////////////////////////////////////////////////////////////////////

static void AddNewElement (TRI_primary_index_t* idx, void* element) {
  uint64_t hash = ((TRI_doc_mptr_t const*) element)->_hash;

  // search the table
  uint64_t i = hash % idx->_nrAlloc;

  while (idx->_table[i] != nullptr) {
    i = TRI_IncModU64(i, idx->_nrAlloc);
  }

  // add a new element to the associative idx
  idx->_table[i] = element;
  idx->_nrUsed++;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief resizes the index
////////////////////////////////////////////////////////////////////////////////

static bool ResizePrimaryIndex (TRI_primary_index_t* idx,
                                uint32_t targetSize) {
  void** oldTable;
  uint32_t oldAlloc;
  uint32_t j;

  oldTable = idx->_table;
  oldAlloc = idx->_nrAlloc;

  idx->_nrAlloc = targetSize; 
#ifdef TRI_INTERNAL_STATS
  idx->_nrResizes++;
#endif

  idx->_table = (void**) TRI_Allocate(idx->_memoryZone, (size_t) (idx->_nrAlloc * sizeof(void*)), true);

  if (idx->_table == nullptr) {
    idx->_nrAlloc = oldAlloc;
    idx->_table = oldTable;

    return false;
  }

  idx->_nrUsed = 0;

  // table is already cleared by allocate, copy old data
  for (j = 0; j < oldAlloc; j++) {
    if (oldTable[j] != nullptr) {
      AddNewElement(idx, oldTable[j]);
    }
  }

  TRI_Free(idx->_memoryZone, oldTable);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief comparison function, compares a master pointer to another 
////////////////////////////////////////////////////////////////////////////////

static inline bool IsEqualKeyElement (TRI_doc_mptr_t const* header, 
                                      void const* element) {
  TRI_doc_mptr_t const* e = static_cast<TRI_doc_mptr_t const*>(element);

  if (header->_hash != e->_hash) {
    // first compare hash values
    return false;
  }

  // only after that compare actual keys
  return (strcmp(TRI_EXTRACT_MARKER_KEY(header), TRI_EXTRACT_MARKER_KEY(e)) == 0);  // ONLY IN INDEX
}

////////////////////////////////////////////////////////////////////////////////
/// @brief comparison function, compares a hash/key to a master pointer
////////////////////////////////////////////////////////////////////////////////

static inline bool IsEqualHashElement (char const* key, uint64_t hash, void const* element) {
  TRI_doc_mptr_t const* e = static_cast<TRI_doc_mptr_t const*>(element);

  if (hash != e->_hash) {
    return false;
  }
  return (strcmp(key, TRI_EXTRACT_MARKER_KEY(e)) == 0);  // ONLY IN INDEX
}

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises the index
////////////////////////////////////////////////////////////////////////////////

int TRI_InitPrimaryIndex (TRI_primary_index_t* idx,
                          TRI_memory_zone_t* zone) {
  idx->_memoryZone = zone;
  idx->_nrAlloc = 0;
  idx->_nrUsed  = 0;

  if (nullptr == (idx->_table = (void**) TRI_Allocate(zone, sizeof(void*) * INITIAL_SIZE, true))) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  idx->_nrAlloc = INITIAL_SIZE;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an index, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyPrimaryIndex (TRI_primary_index_t* idx) {
  if (idx->_table != nullptr) {
    TRI_Free(idx->_memoryZone, idx->_table);
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
                                   void const* key) {
  if (idx->_nrUsed == 0) {
    return nullptr;
  }

  // compute the hash
  uint64_t hash = TRI_FnvHashString((char const*) key);
  uint64_t i = hash % idx->_nrAlloc;

  // search the table
  while (idx->_table[i] != nullptr && ! IsEqualHashElement((char const*) key, hash, idx->_table[i])) {
    i = TRI_IncModU64(i, idx->_nrAlloc);
  }

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

  // check for out-of-memory
  if (idx->_nrAlloc == idx->_nrUsed) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // compute the hash
  uint64_t hash = header->_hash;
  uint64_t i = hash % idx->_nrAlloc;

  // search the table
  while (idx->_table[i] != nullptr && ! IsEqualKeyElement(header, idx->_table[i])) {
    i = TRI_IncModU64(i, idx->_nrAlloc);
  }

  void* old = idx->_table[i];

  // if we found an element, return
  if (old != nullptr) {
    *found = old;

    return TRI_ERROR_NO_ERROR;
  }

  // if we were adding and the table is more than half full, extend it
  if (idx->_nrAlloc < 2 * idx->_nrUsed) {
    if (! ResizePrimaryIndex(idx, (uint32_t) (2 * idx->_nrAlloc) + 1)) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    // now we need to recalc the position
    i = hash % idx->_nrAlloc;
    // search the table
    while (idx->_table[i] != nullptr && ! IsEqualKeyElement(header, idx->_table[i])) {
      i = TRI_IncModU64(i, idx->_nrAlloc);
    }
  }
  
  // add a new element to the associative idx
  idx->_table[i] = (void*) header;
  idx->_nrUsed++;

  return TRI_ERROR_NO_ERROR; 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an key/element from the index
////////////////////////////////////////////////////////////////////////////////

void* TRI_RemoveKeyPrimaryIndex (TRI_primary_index_t* idx,
                                 void const* key) {
  uint64_t hash = TRI_FnvHashString((char const*) key);
  uint64_t i = hash % idx->_nrAlloc;

  // search the table
  while (idx->_table[i] != nullptr && ! IsEqualHashElement((char const*) key, hash, idx->_table[i])) {
    i = TRI_IncModU64(i, idx->_nrAlloc);
  }

  // if we did not find such an item return false
  if (idx->_table[i] == nullptr) {
    return nullptr;
  }

  // remove item
  void*old = idx->_table[i];
  idx->_table[i] = nullptr;
  idx->_nrUsed--;

  // and now check the following places for items to move here
  uint64_t k = TRI_IncModU64(i, idx->_nrAlloc);

  while (idx->_table[k] != nullptr) {
    uint64_t j = (((TRI_doc_mptr_t const*) idx->_table[k])->_hash) % idx->_nrAlloc;

    if ((i < k && !(i < j && j <= k)) || (k < i && !(i < j || j <= k))) {
      idx->_table[i] = idx->_table[k];
      idx->_table[k] = nullptr;
      i = k;
    }

    k = TRI_IncModU64(k, idx->_nrAlloc);
  }

  // return success
  return old;
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

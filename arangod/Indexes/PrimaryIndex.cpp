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

static uint64_t HashKey (char const* key) {
  return TRI_FnvHashString(key);
}

static uint64_t HashElement (TRI_doc_mptr_t const* element) {
  return element->_hash;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief determines if a key corresponds to an element
////////////////////////////////////////////////////////////////////////////////

static bool IsEqualKeyElement (char const* key,
                               TRI_doc_mptr_t const element) {

  // Performance?
  // uint64_t hash = HashKey(key);
  // return (hash == element->_hash &&
  return strcmp(key, TRI_EXTRACT_MARKER_KEY(element)) == 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief determines if two elements are equal
////////////////////////////////////////////////////////////////////////////////

static bool IsEqualElementElement (TRI_doc_mptr_t const* left,
                                   TRI_doc_mptr_t const* right) {
  return left->_hash == right->_hash
         && strcmp(TRI_EXTRACT_MARKER_KEY(left), TRI_EXTRACT_MARKER_KEY(right)) == 0;
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
// --SECTION--                                                class PrimaryIndex
// -----------------------------------------------------------------------------
        
uint64_t const PrimaryIndex::InitialSize = 251;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

PrimaryIndex::PrimaryIndex (TRI_document_collection_t* collection) 
  : Index(0, collection, std::vector<std::vector<triagens::basics::AttributeName>>( { { { TRI_VOC_ATTRIBUTE_KEY, false } } } )) {
  uint32_t indexBuckets = 1;
  if (collection != nullptr) {
    // document is a nullptr in the coordinator case
    indexBuckets = collection->_info._indexBuckets;
  }
  _primaryIndex = new TRI_PrimaryIndex_t(HashKey,
                                         HashElement,
                                         IsEqualKeyElement,
                                         IsEqualElementElement,
                                         indexBuckets,
                                         [] () -> std::string { return "primary"; }
  );
}

PrimaryIndex::~PrimaryIndex () {
  if (_primaryIndex != nullptr) {
    _primaryIndex->invokeOnAllElements(FreeElement);
  }
  delete _primaryIndex;
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

triagens::basics::Json PrimaryIndex::toJson (TRI_memory_zone_t* zone,
                                             bool withFigures) const {
  auto json = Index::toJson(zone, withFigures);

  // hard-coded
  json("unique", triagens::basics::Json(true))
      ("sparse", triagens::basics::Json(false));

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a JSON representation of the index figures
////////////////////////////////////////////////////////////////////////////////

triagens::basics::Json PrimaryIndex::toJsonFigures (TRI_memory_zone_t* zone) const {
  triagens::basics::Json json(zone, triagens::basics::Json::Object);
  
  json("memory", triagens::basics::Json(static_cast<double>(memory())));
  json("nrAlloc", triagens::basics::Json(static_cast<double>(_primaryIndex._nrAlloc)));
  json("nrUsed", triagens::basics::Json(static_cast<double>(_primaryIndex._nrUsed)));

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

TRI_doc_mptr_t* PrimaryIndex::lookupKey (char const* key) const {
  return _primaryIndex->findByKey(key);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an element given a key
/// returns the index position into which a key would belong in the second
/// parameter. sets position to UINT64_MAX if the position cannot be determined
////////////////////////////////////////////////////////////////////////////////

TRI_doc_mptr_t* PrimaryIndex::lookupKey (char const* key,
                               uint64_t& position) const {
  if (_primaryIndex._nrUsed == 0) {
    position = UINT64_MAX;
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
  position = i;

  // return whatever we found
  return _primaryIndex._table[i];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief a method to iterate over all elements in the index in
///        a random order. 
///        Returns nullptr if all documents have been returned.
///        Convention: step === 0 indicates a new start.
////////////////////////////////////////////////////////////////////////////////

TRI_doc_mptr_t* PrimaryIndex::lookupRandom(uint64_t& initialPosition,
                                           uint64_t& position,
                                           uint64_t* step,
                                           uint64_t* total) {
  return _primaryIndex->findRandom(initialPosition, position, step, total);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief a method to iterate over all elements in the index in
///        a sequential order. 
///        Returns nullptr if all documents have been returned.
///        Convention: position === 0 indicates a new start.
////////////////////////////////////////////////////////////////////////////////

TRI_doc_mptr_t* PrimaryIndex::findSequential(uint64_t& position,
                                             uint64_t* total) {
  return _primaryIndex->findSequential(position, total);
}


////////////////////////////////////////////////////////////////////////////////
/// @brief adds a key/element to the index
/// returns a status code, and *found will contain a found element (if any)
////////////////////////////////////////////////////////////////////////////////

int PrimaryIndex::insertKey (TRI_doc_mptr_t const* header,
                             void const** found) {
  *found = nullptr;
  int res = _primaryIndex->insert(TRI_EXTRACT_MARKER_KEY(header), header, false);
  if (res === TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED) {
    *found = _primaryIndex->find(header);
  }
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a key/element to the index
/// this is a special, optimized (read: reduced) variant of the above insert
/// function 
////////////////////////////////////////////////////////////////////////////////

void PrimaryIndex::insertKey (TRI_doc_mptr_t const* header) {
  _primaryIndex->insert(TRI_EXTRACT_MARKER_KEY(header), header, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a key/element to the index
/// this is a special, optimized version that receives the target slot index
/// from a previous lookupKey call
////////////////////////////////////////////////////////////////////////////////

void PrimaryIndex::insertKey (TRI_doc_mptr_t const* header,
                              uint64_t slot) {
  _primaryIndex->insert(TRI_EXTRACT_MARKER_KEY(header), header, false);
  // TODO slot is hint where to insert the element. It is not yet used
  //
  // if (slot != UINT64_MAX) {
  //   i = k = slot;
  // }
  // else {
  //   i = k = header->_hash % n;
  // }
  //
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an key/element from the index
////////////////////////////////////////////////////////////////////////////////

void* PrimaryIndex::removeKey (char const* key) {
  return _primaryIndex->remove(key);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief resizes the index
////////////////////////////////////////////////////////////////////////////////

int PrimaryIndex::resize (size_t targetSize) {
  return _primaryIndex->resize(targetSize);
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
  return HashKey(key);
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
    LOG_ACTION("index-resize %s, target size: %llu", 
               context().c_str(),
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
            "index-resize, %s, target size: %llu", 
            context().c_str(),
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

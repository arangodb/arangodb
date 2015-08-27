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

static void FreeElement (TRI_doc_mptr_t* element) {
  // TODO implement
}

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
                               TRI_doc_mptr_t const* element) {

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
        
size_t PrimaryIndex::size () const {
  return _primaryIndex->size();
}

size_t PrimaryIndex::memory () const {
  return static_cast<size_t>(
      _primaryIndex->size() * sizeof(TRI_doc_mptr_t*) +
      _primaryIndex->memoryUsage()
  );
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
  _primaryIndex->appendToJson(zone, json);

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
  // TODO we ignore the position right now. It should be the position it would fit into
  position = 0;
  return lookupKey(key);
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

TRI_doc_mptr_t* PrimaryIndex::lookupSequential(uint64_t& position,
                                                     uint64_t* total) {
  return _primaryIndex->findSequential(position, total);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief a method to iterate over all elements in the index in
///        reversed sequential order. 
///        Returns nullptr if all documents have been returned.
///        Convention: position === UINT64_MAX indicates a new start.
////////////////////////////////////////////////////////////////////////////////

TRI_doc_mptr_t* PrimaryIndex::lookupSequentialReverse(uint64_t& position) {
  return _primaryIndex->findSequentialReverse(position);
}


////////////////////////////////////////////////////////////////////////////////
/// @brief adds a key/element to the index
/// returns a status code, and *found will contain a found element (if any)
////////////////////////////////////////////////////////////////////////////////

int PrimaryIndex::insertKey (TRI_doc_mptr_t* header,
                             void const** found) {
  *found = nullptr;
  int res = _primaryIndex->insert(TRI_EXTRACT_MARKER_KEY(header), header, false);
  if (res == TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED) {
    *found = _primaryIndex->find(header);
  }
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a key/element to the index
/// this is a special, optimized (read: reduced) variant of the above insert
/// function 
////////////////////////////////////////////////////////////////////////////////

void PrimaryIndex::insertKey (TRI_doc_mptr_t* header) {
  _primaryIndex->insert(TRI_EXTRACT_MARKER_KEY(header), header, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a key/element to the index
/// this is a special, optimized version that receives the target slot index
/// from a previous lookupKey call
////////////////////////////////////////////////////////////////////////////////

void PrimaryIndex::insertKey (TRI_doc_mptr_t* header,
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

TRI_doc_mptr_t* PrimaryIndex::removeKey (char const* key) {
  return _primaryIndex->removeByKey(key);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief resizes the index
////////////////////////////////////////////////////////////////////////////////

int PrimaryIndex::resize (size_t targetSize) {
  return _primaryIndex->resize(targetSize);
}

uint64_t PrimaryIndex::calculateHash (char const* key) {
  return HashKey(key);
}

uint64_t PrimaryIndex::calculateHash (char const* key,
                                      size_t length) {
  return TRI_FnvHashPointer(static_cast<void const*>(key), length);
}

void PrimaryIndex::invokeOnAllElements (std::function<void(TRI_doc_mptr_t*)> work) {
  _primaryIndex->invokeOnAllElements(work);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

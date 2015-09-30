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
#include "Aql/AstNode.h"
#include "Aql/SortCondition.h"
#include "Basics/Exceptions.h"
#include "Basics/hashes.h"
#include "Basics/logging.h"
#include "Indexes/SimpleAttributeEqualityMatcher.h"
#include "VocBase/document-collection.h"
#include "VocBase/transaction.h"

using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

static inline uint64_t HashKey (char const* key) {
  return TRI_FnvHashString(key);
}

static inline uint64_t HashElement (TRI_doc_mptr_t const* element) {
  return element->_hash;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief determines if a key corresponds to an element
////////////////////////////////////////////////////////////////////////////////

static bool IsEqualKeyElement (char const* key,
                               uint64_t const hash,
                               TRI_doc_mptr_t const* element) {

  return (hash == element->_hash &&
          strcmp(key, TRI_EXTRACT_MARKER_KEY(element)) == 0);
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
        
// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

PrimaryIndex::PrimaryIndex (TRI_document_collection_t* collection) 
  : Index(0, collection, std::vector<std::vector<triagens::basics::AttributeName>>( { { { TRI_VOC_ATTRIBUTE_KEY, false } } } ), true, false),
    _primaryIndex(nullptr) {

  uint32_t indexBuckets = 1;

  if (collection != nullptr) {
    // document is a nullptr in the coordinator case
    indexBuckets = collection->_info._indexBuckets;
  }

  _primaryIndex = new TRI_PrimaryIndex_t(HashKey,
                                         HashElement,
                                         IsEqualKeyElement,
                                         IsEqualElementElement,
                                         IsEqualElementElement,
                                         indexBuckets,
                                         [] () -> std::string { return "primary"; }
  );
}

PrimaryIndex::~PrimaryIndex () {
  delete _primaryIndex;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------
        
size_t PrimaryIndex::size () const {
  return _primaryIndex->size();
}

size_t PrimaryIndex::memory () const {
  return _primaryIndex->memoryUsage();
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
/// parameter. also returns the hash value for the object
////////////////////////////////////////////////////////////////////////////////

TRI_doc_mptr_t* PrimaryIndex::lookupKey (char const* key,
                                         triagens::basics::BucketPosition& position,
                                         uint64_t& hash) const {
  return _primaryIndex->findByKey(key, position, hash);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief a method to iterate over all elements in the index in
///        a random order. 
///        Returns nullptr if all documents have been returned.
///        Convention: step === 0 indicates a new start.
////////////////////////////////////////////////////////////////////////////////

TRI_doc_mptr_t* PrimaryIndex::lookupRandom (triagens::basics::BucketPosition& initialPosition,
                                            triagens::basics::BucketPosition& position,
                                            uint64_t& step,
                                            uint64_t& total) {
  return _primaryIndex->findRandom(initialPosition, position, step, total);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief a method to iterate over all elements in the index in
///        a sequential order. 
///        Returns nullptr if all documents have been returned.
///        Convention: position === 0 indicates a new start.
////////////////////////////////////////////////////////////////////////////////

TRI_doc_mptr_t* PrimaryIndex::lookupSequential (triagens::basics::BucketPosition& position,
                                                uint64_t& total) {
  return _primaryIndex->findSequential(position, total);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief a method to iterate over all elements in the index in
///        reversed sequential order. 
///        Returns nullptr if all documents have been returned.
///        Convention: position === UINT64_MAX indicates a new start.
////////////////////////////////////////////////////////////////////////////////

TRI_doc_mptr_t* PrimaryIndex::lookupSequentialReverse (triagens::basics::BucketPosition& position) {
  return _primaryIndex->findSequentialReverse(position);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a key/element to the index
/// returns a status code, and *found will contain a found element (if any)
////////////////////////////////////////////////////////////////////////////////

int PrimaryIndex::insertKey (TRI_doc_mptr_t* header,
                             void const** found) {
  *found = nullptr;
  int res = _primaryIndex->insert(header);

  if (res == TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED) {
    *found = _primaryIndex->find(header);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a key/element to the index
/// this is a special, optimized version that receives the target slot index
/// from a previous lookupKey call
////////////////////////////////////////////////////////////////////////////////

int PrimaryIndex::insertKey (TRI_doc_mptr_t* header,
                             triagens::basics::BucketPosition const& position) {
  return _primaryIndex->insertAtPosition(header, position);
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

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether the index supports the condition
////////////////////////////////////////////////////////////////////////////////

bool PrimaryIndex::supportsFilterCondition (triagens::aql::AstNode const* node,
                                            triagens::aql::Variable const* reference,
                                            double& estimatedCost) const {
  SimpleAttributeEqualityMatcher matcher({ 
    { triagens::basics::AttributeName(TRI_VOC_ATTRIBUTE_ID, false) },
    { triagens::basics::AttributeName(TRI_VOC_ATTRIBUTE_KEY, false) } 
  });
  return matcher.matchOne(this, node, reference, estimatedCost);
}

IndexIterator* PrimaryIndex::iteratorForCondition (triagens::aql::AstNode const*) const {
  // TODO
  return nullptr;
}


// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

////////////////////////////////////////////////////////////////////////////////
/// @brief edge index
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

#include "EdgeIndex.h"
#include "Basics/Exceptions.h"
#include "Basics/fasthash.h"
#include "Basics/hashes.h"
#include "VocBase/document-collection.h"
#include "VocBase/edge-collection.h"
#include "VocBase/transaction.h"

using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes an edge key
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementKey (void const* data) {
  TRI_edge_header_t const* h = static_cast<TRI_edge_header_t const*>(data);
  char const* key = h->_key;

  uint64_t hash = h->_cid;
  hash ^=  (uint64_t) fasthash64(key, strlen(key), 0x87654321);

  return fasthash64(&hash, sizeof(hash), 0x56781234);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes an edge (_from case)
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementEdgeFrom (void const* data,
                                     bool byKey) {
  uint64_t hash;

  if (! byKey) {
    hash = (uint64_t) data;
  }
  else {
    TRI_doc_mptr_t const* mptr = static_cast<TRI_doc_mptr_t const*>(data);
    TRI_df_marker_t const* marker = static_cast<TRI_df_marker_t const*>(mptr->getDataPtrUnchecked());  // ONLY IN INDEX, PROTECTED by RUNTIME

    if (marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
      TRI_doc_edge_key_marker_t const* edge = reinterpret_cast<TRI_doc_edge_key_marker_t const*>(marker);  // ONLY IN INDEX, PROTECTED by RUNTIME
      char const* key = (char const*) edge + edge->_offsetFromKey;

      // LOG_TRACE("HASH FROM: COLLECTION: %llu, KEY: %s", (unsigned long long) edge->_fromCid, key);

      hash = edge->_fromCid;
      hash ^= (uint64_t) fasthash64(key, strlen(key), 0x87654321);
    }
    else if (marker->_type == TRI_WAL_MARKER_EDGE) {
      triagens::wal::edge_marker_t const* edge = reinterpret_cast<triagens::wal::edge_marker_t const*>(marker);  // ONLY IN INDEX, PROTECTED by RUNTIME
      char const* key = (char const*) edge + edge->_offsetFromKey;

      // LOG_TRACE("HASH FROM: COLLECTION: %llu, KEY: %s", (unsigned long long) edge->_fromCid, key);

      hash = edge->_fromCid;
      hash ^= (uint64_t) fasthash64(key, strlen(key), 0x87654321);
    }
  }

  return fasthash64(&hash, sizeof(hash), 0x56781234);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes an edge (_to case)
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementEdgeTo (void const* data,
                                   bool byKey) {
  uint64_t hash;

  if (! byKey) {
    hash = (uint64_t) data;
  }
  else {
    TRI_doc_mptr_t const* mptr = static_cast<TRI_doc_mptr_t const*>(data);
    TRI_df_marker_t const* marker = static_cast<TRI_df_marker_t const*>(mptr->getDataPtrUnchecked());  // ONLY IN INDEX, PROTECTED by RUNTIME

    if (marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
      TRI_doc_edge_key_marker_t const* edge = reinterpret_cast<TRI_doc_edge_key_marker_t const*>(marker);  // ONLY IN INDEX, PROTECTED by RUNTIME
      char const* key = (char const*) edge + edge->_offsetToKey;

      // LOG_TRACE("HASH TO: COLLECTION: %llu, KEY: %s", (unsigned long long) edge->_toCid, key);

      hash = edge->_toCid;
      hash ^= (uint64_t) fasthash64(key, strlen(key), 0x87654321);
    }
    else if (marker->_type == TRI_WAL_MARKER_EDGE) {
      triagens::wal::edge_marker_t const* edge = reinterpret_cast<triagens::wal::edge_marker_t const*>(marker);  // ONLY IN INDEX, PROTECTED by RUNTIME
      char const* key = (char const*) edge + edge->_offsetToKey;

      // LOG_TRACE("HASH TO: COLLECTION: %llu, KEY: %s", (unsigned long long) edge->_toCid, key);

      hash = edge->_toCid;
      hash ^= (uint64_t) fasthash64(key, strlen(key), 0x87654321);
    }
  }

  return fasthash64(&hash, sizeof(hash), 0x56781234);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if key and element match (_from case)
////////////////////////////////////////////////////////////////////////////////

static bool IsEqualKeyEdgeFrom (void const* left,
                                void const* right) {
  // left is a key
  // right is an element, that is a master pointer
  TRI_edge_header_t const* l = static_cast<TRI_edge_header_t const*>(left);
  char const* lKey = l->_key;

  TRI_doc_mptr_t const* rMptr = static_cast<TRI_doc_mptr_t const*>(right);
  TRI_df_marker_t const* marker = static_cast<TRI_df_marker_t const*>(rMptr->getDataPtrUnchecked());  // ONLY IN INDEX, PROTECTED by RUNTIME

  if (marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
    TRI_doc_edge_key_marker_t const* rEdge = reinterpret_cast<TRI_doc_edge_key_marker_t const*>(marker);  // ONLY IN INDEX, PROTECTED by RUNTIME
    char const* rKey = (char const*) rEdge + rEdge->_offsetFromKey;

    // LOG_TRACE("ISEQUAL FROM: LCOLLECTION: %llu, LKEY: %s, RCOLLECTION: %llu, RKEY: %s", (unsigned long long) l->_cid, lKey, (unsigned long long) rEdge->_fromCid, rKey);
    return (l->_cid == rEdge->_fromCid) && (strcmp(lKey, rKey) == 0);
  }
  else if (marker->_type == TRI_WAL_MARKER_EDGE) {
    triagens::wal::edge_marker_t const* rEdge = reinterpret_cast<triagens::wal::edge_marker_t const*>(marker);  // ONLY IN INDEX, PROTECTED by RUNTIME
    char const* rKey = (char const*) rEdge + rEdge->_offsetFromKey;

    // LOG_TRACE("ISEQUAL FROM: LCOLLECTION: %llu, LKEY: %s, RCOLLECTION: %llu, RKEY: %s", (unsigned long long) l->_cid, lKey, (unsigned long long) rEdge->_fromCid, rKey);

    return (l->_cid == rEdge->_fromCid) && (strcmp(lKey, rKey) == 0);
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if key and element match (_to case)
////////////////////////////////////////////////////////////////////////////////

static bool IsEqualKeyEdgeTo (void const* left,
                              void const* right) {
  // left is a key
  // right is an element, that is a master pointer
  TRI_edge_header_t const* l = static_cast<TRI_edge_header_t const*>(left);
  char const* lKey = l->_key;

  TRI_doc_mptr_t const* rMptr = static_cast<TRI_doc_mptr_t const*>(right);
  TRI_df_marker_t const* marker = static_cast<TRI_df_marker_t const*>(rMptr->getDataPtrUnchecked());  // ONLY IN INDEX, PROTECTED by RUNTIME

  if (marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
    TRI_doc_edge_key_marker_t const* rEdge = reinterpret_cast<TRI_doc_edge_key_marker_t const*>(marker);  // ONLY IN INDEX, PROTECTED by RUNTIME
    char const* rKey = (char const*) rEdge + rEdge->_offsetToKey;

    // LOG_TRACE("ISEQUAL TO: LCOLLECTION: %llu, LKEY: %s, RCOLLECTION: %llu, RKEY: %s", (unsigned long long) l->_cid, lKey, (unsigned long long) rEdge->_toCid, rKey);

    return (l->_cid == rEdge->_toCid) && (strcmp(lKey, rKey) == 0);
  }
  else if (marker->_type == TRI_WAL_MARKER_EDGE) {
    triagens::wal::edge_marker_t const* rEdge = reinterpret_cast<triagens::wal::edge_marker_t const*>(marker);  // ONLY IN INDEX, PROTECTED by RUNTIME
    char const* rKey = (char const*) rEdge + rEdge->_offsetToKey;

    // LOG_TRACE("ISEQUAL TO: LCOLLECTION: %llu, LKEY: %s, RCOLLECTION: %llu, RKEY: %s", (unsigned long long) l->_cid, lKey, (unsigned long long) rEdge->_toCid, rKey);

    return (l->_cid == rEdge->_toCid) && (strcmp(lKey, rKey) == 0);
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks for elements are equal (_from and _to case)
////////////////////////////////////////////////////////////////////////////////

static bool IsEqualElementEdge (void const* left,
                                void const* right) {
  return left == right;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks for elements are equal (_from case)
////////////////////////////////////////////////////////////////////////////////

static bool IsEqualElementEdgeFromByKey (void const* left,
                                         void const* right) {
  char const* lKey = nullptr;
  char const* rKey = nullptr;
  TRI_voc_cid_t lCid = 0;
  TRI_voc_cid_t rCid = 0;
  TRI_df_marker_t const* marker;

  // left element
  TRI_doc_mptr_t const* lMptr = static_cast<TRI_doc_mptr_t const*>(left);
  marker = static_cast<TRI_df_marker_t const*>(lMptr->getDataPtrUnchecked());  // ONLY IN INDEX, PROTECTED by RUNTIME

  if (marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
    TRI_doc_edge_key_marker_t const* lEdge = reinterpret_cast<TRI_doc_edge_key_marker_t const*>(marker);  // ONLY IN INDEX, PROTECTED by RUNTIME
    lKey = (char const*) lEdge + lEdge->_offsetFromKey;
    lCid = lEdge->_fromCid;
  }
  else if (marker->_type == TRI_WAL_MARKER_EDGE) {
    triagens::wal::edge_marker_t const* lEdge = reinterpret_cast<triagens::wal::edge_marker_t const*>(marker);  // ONLY IN INDEX, PROTECTED by RUNTIME
    lKey = (char const*) lEdge + lEdge->_offsetFromKey;
    lCid = lEdge->_fromCid;
  }

  // right element
  TRI_doc_mptr_t const* rMptr = static_cast<TRI_doc_mptr_t const*>(right);
  marker = static_cast<TRI_df_marker_t const*>(rMptr->getDataPtrUnchecked());  // ONLY IN INDEX, PROTECTED by RUNTIME

  if (marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
    TRI_doc_edge_key_marker_t const* rEdge = reinterpret_cast<TRI_doc_edge_key_marker_t const*>(marker);  // ONLY IN INDEX, PROTECTED by RUNTIME
    rKey = (char const*) rEdge + rEdge->_offsetFromKey;
    rCid = rEdge->_fromCid;
  }
  else if (marker->_type == TRI_WAL_MARKER_EDGE) {
    triagens::wal::edge_marker_t const* rEdge = reinterpret_cast<triagens::wal::edge_marker_t const*>(marker);  // ONLY IN INDEX, PROTECTED by RUNTIME
    rKey = (char const*) rEdge + rEdge->_offsetFromKey;
    rCid = rEdge->_fromCid;
  }

  if (lKey == nullptr || rKey == nullptr) {
    return false;
  }

  // LOG_TRACE("ISEQUALELEMENT FROM: LCOLLECTION: %llu, LKEY: %s, RCOLLECTION: %llu, RKEY: %s", (unsigned long long) lCid, lKey, (unsigned long long) rCid, rKey);

  return ((lCid == rCid) && (strcmp(lKey, rKey) == 0));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks for elements are equal (_to case)
////////////////////////////////////////////////////////////////////////////////

static bool IsEqualElementEdgeToByKey (void const* left,
                                       void const* right) {
  char const* lKey = nullptr;
  char const* rKey = nullptr;
  TRI_voc_cid_t lCid = 0;
  TRI_voc_cid_t rCid = 0;
  TRI_df_marker_t const* marker;

  // left element
  TRI_doc_mptr_t const* lMptr = static_cast<TRI_doc_mptr_t const*>(left);
  marker = static_cast<TRI_df_marker_t const*>(lMptr->getDataPtrUnchecked());  // ONLY IN INDEX, PROTECTED by RUNTIME

  if (marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
    TRI_doc_edge_key_marker_t const* lEdge = reinterpret_cast<TRI_doc_edge_key_marker_t const*>(marker);  // ONLY IN INDEX, PROTECTED by RUNTIME
    lKey = (char const*) lEdge + lEdge->_offsetToKey;
    lCid = lEdge->_toCid;
  }
  else if (marker->_type == TRI_WAL_MARKER_EDGE) {
    triagens::wal::edge_marker_t const* lEdge = reinterpret_cast<triagens::wal::edge_marker_t const*>(marker);  // ONLY IN INDEX, PROTECTED by RUNTIME
    lKey = (char const*) lEdge + lEdge->_offsetToKey;
    lCid = lEdge->_toCid;
  }

  // right element
  TRI_doc_mptr_t const* rMptr = static_cast<TRI_doc_mptr_t const*>(right);
  marker = static_cast<TRI_df_marker_t const*>(rMptr->getDataPtrUnchecked());  // ONLY IN INDEX, PROTECTED by RUNTIME

  if (marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
    TRI_doc_edge_key_marker_t const* rEdge = reinterpret_cast<TRI_doc_edge_key_marker_t const*>(marker);  // ONLY IN INDEX, PROTECTED by RUNTIME
    rKey = (char const*) rEdge + rEdge->_offsetToKey;
    rCid = rEdge->_toCid;
  }
  else if (marker->_type == TRI_WAL_MARKER_EDGE) {
    triagens::wal::edge_marker_t const* rEdge = reinterpret_cast<triagens::wal::edge_marker_t const*>(marker);  // ONLY IN INDEX, PROTECTED by RUNTIME
    rKey = (char const*) rEdge + rEdge->_offsetToKey;
    rCid = rEdge->_toCid;
  }

  if (lKey == nullptr || rKey == nullptr) {
    return false;
  }

  // LOG_TRACE("ISEQUALELEMENT TO: LCOLLECTION: %llu, LKEY: %s, RCOLLECTION: %llu, RKEY: %s", (unsigned long long) lCid, lKey, (unsigned long long) rCid, rKey);

  return ((lCid == rCid) && (strcmp(lKey, rKey) == 0));
}

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

EdgeIndex::EdgeIndex (TRI_idx_iid_t iid,
                      TRI_document_collection_t* collection) 
  : Index(iid, collection, std::vector<std::vector<triagens::basics::AttributeName const>>({ { { TRI_VOC_ATTRIBUTE_FROM, false } }, { { TRI_VOC_ATTRIBUTE_TO , false } } })),
    _edgesFrom(nullptr),
    _edgesTo(nullptr) {
  
  TRI_ASSERT(iid != 0);

  uint32_t indexBuckets = 1;
  if (collection != nullptr) {
    // document is a nullptr in the coordinator case
    indexBuckets = collection->_info._indexBuckets;
  }

  auto context = [this] () -> std::string {
    return this->context();
  };
  
  _edgesFrom = new TRI_EdgeIndexHash_t(HashElementKey,
                                       HashElementEdgeFrom,
                                       IsEqualKeyEdgeFrom,
                                       IsEqualElementEdge,
                                       IsEqualElementEdgeFromByKey,
                                       indexBuckets, 
                                       64,
                                       context);

  _edgesTo = new TRI_EdgeIndexHash_t(HashElementKey,
                                     HashElementEdgeTo,
                                     IsEqualKeyEdgeTo,
                                     IsEqualElementEdge,
                                     IsEqualElementEdgeToByKey,
                                     indexBuckets,
                                     64,
                                     context);
}

EdgeIndex::~EdgeIndex () {
  delete _edgesTo;
  delete _edgesFrom;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return a selectivity esimtate for the index
////////////////////////////////////////////////////////////////////////////////

double EdgeIndex::selectivityEstimate () const {  
  // return average selectivity of the two index parts
  double estimate = (_edgesFrom->selectivity() + _edgesTo->selectivity()) * 0.5;
  TRI_ASSERT(estimate >= 0.0 && estimate <= 1.00001); // floating-point tolerance 
  return estimate;
}
 
size_t EdgeIndex::memory () const {
  return _edgesFrom->memoryUsage() + _edgesTo->memoryUsage();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a JSON representation of the index
////////////////////////////////////////////////////////////////////////////////
      
triagens::basics::Json EdgeIndex::toJson (TRI_memory_zone_t* zone) const {
  auto json = Index::toJson(zone);
  
  // hard-coded
  json("unique", triagens::basics::Json(false))
      ("sparse", triagens::basics::Json(false));

  return json;
}

int EdgeIndex::insert (TRI_doc_mptr_t const* doc, 
                       bool isRollback) {
  auto element = const_cast<void*>(static_cast<void const*>(doc));
  _edgesFrom->insert(element, true, isRollback);

  try {
    _edgesTo->insert(element, true, isRollback);
  }
  catch (...) {
    _edgesFrom->remove(element);
    throw;
  }
  
  return TRI_ERROR_NO_ERROR;
}
         
int EdgeIndex::remove (TRI_doc_mptr_t const* doc, 
                       bool) {
  _edgesFrom->remove(doc);
  _edgesTo->remove(doc);
  
  return TRI_ERROR_NO_ERROR;
}
        
int EdgeIndex::batchInsert (std::vector<TRI_doc_mptr_t const*> const* documents, 
                            size_t numThreads) {
  _edgesFrom->batchInsert(reinterpret_cast<std::vector<void const*> const*>(documents), numThreads);
  _edgesTo->batchInsert(reinterpret_cast<std::vector<void const*> const*>(documents), numThreads);
  
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up edges using the index, restarting at the edge pointed at
/// by next
////////////////////////////////////////////////////////////////////////////////
      
void EdgeIndex::lookup (TRI_edge_index_iterator_t const* edgeIndexIterator,
                        std::vector<TRI_doc_mptr_copy_t>& result,
                        void*& next,
                        size_t batchSize) {

  std::function<void(void*)> callback = [&result] (void* data) -> void {
    TRI_doc_mptr_t* doc = static_cast<TRI_doc_mptr_t*>(data);

    result.emplace_back(*(doc));
  };

  std::vector<void*>* found = nullptr;
  if (next == nullptr) {
    if (edgeIndexIterator->_direction == TRI_EDGE_OUT) {
      found = _edgesFrom->lookupByKey(&(edgeIndexIterator->_edge), batchSize);
    }
    else if (edgeIndexIterator->_direction == TRI_EDGE_IN) {
      found = _edgesTo->lookupByKey(&(edgeIndexIterator->_edge), batchSize);
    }
    else {
      TRI_ASSERT(false);
    }
    if (found != nullptr && found->size() != 0) {
      next = found->back();
    }
  }
  else {
    if (edgeIndexIterator->_direction == TRI_EDGE_OUT) {
      found = _edgesFrom->lookupByKeyContinue(next, batchSize);
    }
    else if (edgeIndexIterator->_direction == TRI_EDGE_IN) {
      found = _edgesTo->lookupByKeyContinue(next, batchSize);
    }
    else {
      TRI_ASSERT(false);
    }
    if (found != nullptr && found->size() != 0) {
      next = found->back();
    }
    else {
      next = nullptr;
    }
  }

  if (found != nullptr) {
    for (auto& v : *found) {
      callback(v);
    }

    delete found;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief provides a size hint for the edge index
////////////////////////////////////////////////////////////////////////////////
        
int EdgeIndex::sizeHint (size_t size) {
  // we assume this is called when setting up the index and the index
  // is still empty
  TRI_ASSERT(_edgesFrom->size() == 0);

  // set an initial size for the index for some new nodes to be created
  // without resizing
  int err = _edgesFrom->resize(static_cast<uint32_t>(size + 2049));

  if (err != TRI_ERROR_NO_ERROR) {
    return err;
  }

  // we assume this is called when setting up the index and the index
  // is still empty
  TRI_ASSERT(_edgesTo->size() == 0);

  // set an initial size for the index for some new nodes to be created
  // without resizing
  return _edgesTo->resize(static_cast<uint32_t>(size + 2049));
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

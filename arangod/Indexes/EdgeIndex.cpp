////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "EdgeIndex.h"
#include "Aql/AstNode.h"
#include "Aql/SortCondition.h"
#include "Basics/Exceptions.h"
#include "Basics/fasthash.h"
#include "Basics/hashes.h"
#include "Indexes/SimpleAttributeEqualityMatcher.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/document-collection.h"
#include "VocBase/edge-collection.h"
#include "VocBase/transaction.h"

using namespace arangodb;

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes an edge key
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementKey(void* userData, TRI_edge_header_t const* data) {
  TRI_ASSERT(data != nullptr);

  TRI_edge_header_t const* h = static_cast<TRI_edge_header_t const*>(data);
  char const* key = h->_key;

  uint64_t hash = h->_cid;
  hash ^= (uint64_t)fasthash64(key, strlen(key), 0x87654321);

  return fasthash64(&hash, sizeof(hash), 0x56781234);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes an edge (_from case)
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementEdgeFrom(void* userData, TRI_doc_mptr_t const* data,
                                    bool byKey) {
  TRI_ASSERT(data != nullptr);

  uint64_t hash;

  if (!byKey) {
    hash = (uint64_t)data;
  } else {
    TRI_doc_mptr_t const* mptr = static_cast<TRI_doc_mptr_t const*>(data);
    TRI_df_marker_t const* marker = static_cast<TRI_df_marker_t const*>(
        mptr->getDataPtr());  // ONLY IN INDEX, PROTECTED by RUNTIME

    if (marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
      TRI_doc_edge_key_marker_t const* edge =
          reinterpret_cast<TRI_doc_edge_key_marker_t const*>(
              marker);  // ONLY IN INDEX, PROTECTED by RUNTIME
      char const* key = (char const*)edge + edge->_offsetFromKey;

      // LOG(TRACE) << "HASH FROM: COLLECTION: " << // edge->_fromCid << ", KEY: " << key;

      hash = edge->_fromCid;
      hash ^= (uint64_t)fasthash64(key, strlen(key), 0x87654321);
    } else if (marker->_type == TRI_WAL_MARKER_EDGE) {
      arangodb::wal::edge_marker_t const* edge =
          reinterpret_cast<arangodb::wal::edge_marker_t const*>(
              marker);  // ONLY IN INDEX, PROTECTED by RUNTIME
      char const* key = (char const*)edge + edge->_offsetFromKey;

      // LOG(TRACE) << "HASH FROM: COLLECTION: " << // edge->_fromCid << ", KEY: " << key;

      hash = edge->_fromCid;
      hash ^= (uint64_t)fasthash64(key, strlen(key), 0x87654321);
    }
  }

  return fasthash64(&hash, sizeof(hash), 0x56781234);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes an edge (_to case)
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementEdgeTo(void* userData, TRI_doc_mptr_t const* data,
                                  bool byKey) {
  TRI_ASSERT(data != nullptr);

  uint64_t hash;

  if (!byKey) {
    hash = (uint64_t)data;
  } else {
    TRI_doc_mptr_t const* mptr = static_cast<TRI_doc_mptr_t const*>(data);
    TRI_df_marker_t const* marker = static_cast<TRI_df_marker_t const*>(
        mptr->getDataPtr());  // ONLY IN INDEX, PROTECTED by RUNTIME

    if (marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
      TRI_doc_edge_key_marker_t const* edge =
          reinterpret_cast<TRI_doc_edge_key_marker_t const*>(
              marker);  // ONLY IN INDEX, PROTECTED by RUNTIME
      char const* key = (char const*)edge + edge->_offsetToKey;

      // LOG(TRACE) << "HASH TO: COLLECTION: " << // edge->_toCid << ", KEY: " << key;

      hash = edge->_toCid;
      hash ^= (uint64_t)fasthash64(key, strlen(key), 0x87654321);
    } else if (marker->_type == TRI_WAL_MARKER_EDGE) {
      arangodb::wal::edge_marker_t const* edge =
          reinterpret_cast<arangodb::wal::edge_marker_t const*>(
              marker);  // ONLY IN INDEX, PROTECTED by RUNTIME
      char const* key = (char const*)edge + edge->_offsetToKey;

      // LOG(TRACE) << "HASH TO: COLLECTION: " << // edge->_toCid << ", KEY: " << key;

      hash = edge->_toCid;
      hash ^= (uint64_t)fasthash64(key, strlen(key), 0x87654321);
    }
  }

  return fasthash64(&hash, sizeof(hash), 0x56781234);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if key and element match (_from case)
////////////////////////////////////////////////////////////////////////////////

static bool IsEqualKeyEdgeFrom(void* userData, TRI_edge_header_t const* left,
                               TRI_doc_mptr_t const* right) {
  TRI_ASSERT(left != nullptr);
  TRI_ASSERT(right != nullptr);

  // left is a key
  // right is an element, that is a master pointer
  TRI_edge_header_t const* l = static_cast<TRI_edge_header_t const*>(left);
  char const* lKey = l->_key;

  TRI_doc_mptr_t const* rMptr = static_cast<TRI_doc_mptr_t const*>(right);
  TRI_df_marker_t const* marker = static_cast<TRI_df_marker_t const*>(
      rMptr->getDataPtr());  // ONLY IN INDEX, PROTECTED by RUNTIME

  if (marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
    TRI_doc_edge_key_marker_t const* rEdge =
        reinterpret_cast<TRI_doc_edge_key_marker_t const*>(
            marker);  // ONLY IN INDEX, PROTECTED by RUNTIME
    char const* rKey = (char const*)rEdge + rEdge->_offsetFromKey;

    // LOG(TRACE) << "ISEQUAL FROM: LCOLLECTION: " << l->_cid << ", LKEY: " << lKey << ", RCOLLECTION: " << // rEdge->_fromCid << ",
    // RKEY: " << rKey;
    return (l->_cid == rEdge->_fromCid) && (strcmp(lKey, rKey) == 0);
  } else if (marker->_type == TRI_WAL_MARKER_EDGE) {
    arangodb::wal::edge_marker_t const* rEdge =
        reinterpret_cast<arangodb::wal::edge_marker_t const*>(
            marker);  // ONLY IN INDEX, PROTECTED by RUNTIME
    char const* rKey = (char const*)rEdge + rEdge->_offsetFromKey;

    // LOG(TRACE) << "ISEQUAL FROM: LCOLLECTION: " << l->_cid << ", LKEY: " << lKey << ", RCOLLECTION: " << // rEdge->_fromCid << ",
    // RKEY: " << rKey;

    return (l->_cid == rEdge->_fromCid) && (strcmp(lKey, rKey) == 0);
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if key and element match (_to case)
////////////////////////////////////////////////////////////////////////////////

static bool IsEqualKeyEdgeTo(void* userData, TRI_edge_header_t const* left,
                             TRI_doc_mptr_t const* right) {
  TRI_ASSERT(left != nullptr);
  TRI_ASSERT(right != nullptr);

  // left is a key
  // right is an element, that is a master pointer
  TRI_edge_header_t const* l = static_cast<TRI_edge_header_t const*>(left);
  char const* lKey = l->_key;

  TRI_doc_mptr_t const* rMptr = static_cast<TRI_doc_mptr_t const*>(right);
  TRI_df_marker_t const* marker = static_cast<TRI_df_marker_t const*>(
      rMptr->getDataPtr());  // ONLY IN INDEX, PROTECTED by RUNTIME

  if (marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
    TRI_doc_edge_key_marker_t const* rEdge =
        reinterpret_cast<TRI_doc_edge_key_marker_t const*>(
            marker);  // ONLY IN INDEX, PROTECTED by RUNTIME
    char const* rKey = (char const*)rEdge + rEdge->_offsetToKey;

    // LOG(TRACE) << "ISEQUAL TO: LCOLLECTION: " << l->_cid << ", LKEY: " << lKey << ", RCOLLECTION: " << // rEdge->_toCid << ",
    // RKEY: " << rKey;

    return (l->_cid == rEdge->_toCid) && (strcmp(lKey, rKey) == 0);
  } else if (marker->_type == TRI_WAL_MARKER_EDGE) {
    arangodb::wal::edge_marker_t const* rEdge =
        reinterpret_cast<arangodb::wal::edge_marker_t const*>(
            marker);  // ONLY IN INDEX, PROTECTED by RUNTIME
    char const* rKey = (char const*)rEdge + rEdge->_offsetToKey;

    // LOG(TRACE) << "ISEQUAL TO: LCOLLECTION: " << l->_cid << ", LKEY: " << lKey << ", RCOLLECTION: " << // rEdge->_toCid << ",
    // RKEY: " << rKey;

    return (l->_cid == rEdge->_toCid) && (strcmp(lKey, rKey) == 0);
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks for elements are equal (_from and _to case)
////////////////////////////////////////////////////////////////////////////////

static bool IsEqualElementEdge(void* userData, TRI_doc_mptr_t const* left,
                               TRI_doc_mptr_t const* right) {
  return left == right;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks for elements are equal (_from case)
////////////////////////////////////////////////////////////////////////////////

static bool IsEqualElementEdgeFromByKey(void* userData,
                                        TRI_doc_mptr_t const* left,
                                        TRI_doc_mptr_t const* right) {
  TRI_ASSERT(left != nullptr);
  TRI_ASSERT(right != nullptr);

  char const* lKey = nullptr;
  char const* rKey = nullptr;
  TRI_voc_cid_t lCid = 0;
  TRI_voc_cid_t rCid = 0;
  TRI_df_marker_t const* marker;

  // left element
  TRI_doc_mptr_t const* lMptr = static_cast<TRI_doc_mptr_t const*>(left);
  marker = static_cast<TRI_df_marker_t const*>(
      lMptr->getDataPtr());  // ONLY IN INDEX, PROTECTED by RUNTIME

  if (marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
    TRI_doc_edge_key_marker_t const* lEdge =
        reinterpret_cast<TRI_doc_edge_key_marker_t const*>(
            marker);  // ONLY IN INDEX, PROTECTED by RUNTIME
    lKey = (char const*)lEdge + lEdge->_offsetFromKey;
    lCid = lEdge->_fromCid;
  } else if (marker->_type == TRI_WAL_MARKER_EDGE) {
    arangodb::wal::edge_marker_t const* lEdge =
        reinterpret_cast<arangodb::wal::edge_marker_t const*>(
            marker);  // ONLY IN INDEX, PROTECTED by RUNTIME
    lKey = (char const*)lEdge + lEdge->_offsetFromKey;
    lCid = lEdge->_fromCid;
  }

  // right element
  TRI_doc_mptr_t const* rMptr = static_cast<TRI_doc_mptr_t const*>(right);
  marker = static_cast<TRI_df_marker_t const*>(
      rMptr->getDataPtr());  // ONLY IN INDEX, PROTECTED by RUNTIME

  if (marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
    TRI_doc_edge_key_marker_t const* rEdge =
        reinterpret_cast<TRI_doc_edge_key_marker_t const*>(
            marker);  // ONLY IN INDEX, PROTECTED by RUNTIME
    rKey = (char const*)rEdge + rEdge->_offsetFromKey;
    rCid = rEdge->_fromCid;
  } else if (marker->_type == TRI_WAL_MARKER_EDGE) {
    arangodb::wal::edge_marker_t const* rEdge =
        reinterpret_cast<arangodb::wal::edge_marker_t const*>(
            marker);  // ONLY IN INDEX, PROTECTED by RUNTIME
    rKey = (char const*)rEdge + rEdge->_offsetFromKey;
    rCid = rEdge->_fromCid;
  }

  if (lKey == nullptr || rKey == nullptr) {
    return false;
  }

  // LOG(TRACE) << "ISEQUALELEMENT FROM: LCOLLECTION: " << lCid << ", LKEY: " << lKey << ", RCOLLECTION:
  // " << // rCid << ", RKEY: " << rKey;

  return ((lCid == rCid) && (strcmp(lKey, rKey) == 0));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks for elements are equal (_to case)
////////////////////////////////////////////////////////////////////////////////

static bool IsEqualElementEdgeToByKey(void* userData,
                                      TRI_doc_mptr_t const* left,
                                      TRI_doc_mptr_t const* right) {
  TRI_ASSERT(left != nullptr);
  TRI_ASSERT(right != nullptr);

  char const* lKey = nullptr;
  char const* rKey = nullptr;
  TRI_voc_cid_t lCid = 0;
  TRI_voc_cid_t rCid = 0;
  TRI_df_marker_t const* marker;

  // left element
  TRI_doc_mptr_t const* lMptr = static_cast<TRI_doc_mptr_t const*>(left);
  marker = static_cast<TRI_df_marker_t const*>(
      lMptr->getDataPtr());  // ONLY IN INDEX, PROTECTED by RUNTIME

  if (marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
    TRI_doc_edge_key_marker_t const* lEdge =
        reinterpret_cast<TRI_doc_edge_key_marker_t const*>(
            marker);  // ONLY IN INDEX, PROTECTED by RUNTIME
    lKey = (char const*)lEdge + lEdge->_offsetToKey;
    lCid = lEdge->_toCid;
  } else if (marker->_type == TRI_WAL_MARKER_EDGE) {
    arangodb::wal::edge_marker_t const* lEdge =
        reinterpret_cast<arangodb::wal::edge_marker_t const*>(
            marker);  // ONLY IN INDEX, PROTECTED by RUNTIME
    lKey = (char const*)lEdge + lEdge->_offsetToKey;
    lCid = lEdge->_toCid;
  }

  // right element
  TRI_doc_mptr_t const* rMptr = static_cast<TRI_doc_mptr_t const*>(right);
  marker = static_cast<TRI_df_marker_t const*>(
      rMptr->getDataPtr());  // ONLY IN INDEX, PROTECTED by RUNTIME

  if (marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
    TRI_doc_edge_key_marker_t const* rEdge =
        reinterpret_cast<TRI_doc_edge_key_marker_t const*>(
            marker);  // ONLY IN INDEX, PROTECTED by RUNTIME
    rKey = (char const*)rEdge + rEdge->_offsetToKey;
    rCid = rEdge->_toCid;
  } else if (marker->_type == TRI_WAL_MARKER_EDGE) {
    arangodb::wal::edge_marker_t const* rEdge =
        reinterpret_cast<arangodb::wal::edge_marker_t const*>(
            marker);  // ONLY IN INDEX, PROTECTED by RUNTIME
    rKey = (char const*)rEdge + rEdge->_offsetToKey;
    rCid = rEdge->_toCid;
  }

  if (lKey == nullptr || rKey == nullptr) {
    return false;
  }

  // LOG(TRACE) << "ISEQUALELEMENT TO: LCOLLECTION: " << lCid << ", LKEY: " << lKey << ", RCOLLECTION:
  // " << // rCid << ", RKEY: " << rKey;

  return ((lCid == rCid) && (strcmp(lKey, rKey) == 0));
}

TRI_doc_mptr_t* EdgeIndexIterator::next() {
  while (true) {
    if (_position >= _keys.size()) {
      // we're at the end of the lookup values
      return nullptr;
    }

    if (_buffer == nullptr) {
      // We start a new lookup
      TRI_ASSERT(_position == 0);
      _posInBuffer = 0;
      _last = nullptr;
      _buffer = _index->lookupByKey(_trx, &_keys[_position], _batchSize);
      // fallthrough intentional
    } else if (_posInBuffer >= _buffer->size()) {
      // We have to refill the buffer
      delete _buffer;
      _buffer = nullptr;

      _posInBuffer = 0;
      if (_last != nullptr) {
        _buffer = _index->lookupByKeyContinue(_trx, _last, _batchSize);
      } else {
        _buffer = _index->lookupByKey(_trx, &_keys[_position], _batchSize);
      }
    }

    if (!_buffer->empty()) {
      // found something
      _last = _buffer->back();
      return _buffer->at(_posInBuffer++);
    }

    // found no result. now go to next lookup value in _keys
    ++_position;
    // reset the _last value
    _last = nullptr;
  }
}

void EdgeIndexIterator::reset() {
  _last = nullptr;
  _position = 0;
  _posInBuffer = 0;
  // Free the vector space, not the content
  delete _buffer;
  _buffer = nullptr;
}

EdgeIndex::EdgeIndex(TRI_idx_iid_t iid, TRI_document_collection_t* collection)
    : Index(iid, collection,
            std::vector<std::vector<arangodb::basics::AttributeName>>(
                {{{TRI_VOC_ATTRIBUTE_FROM, false}},
                 {{TRI_VOC_ATTRIBUTE_TO, false}}}),
            false, false),
      _edgesFrom(nullptr),
      _edgesTo(nullptr),
      _numBuckets(1) {
  TRI_ASSERT(iid != 0);

  if (collection != nullptr) {
    // document is a nullptr in the coordinator case
    _numBuckets = static_cast<size_t>(collection->_info.indexBuckets());
  }

  auto context = [this]() -> std::string { return this->context(); };

  _edgesFrom = new TRI_EdgeIndexHash_t(HashElementKey, HashElementEdgeFrom,
                                       IsEqualKeyEdgeFrom, IsEqualElementEdge,
                                       IsEqualElementEdgeFromByKey, _numBuckets,
                                       64, context);

  _edgesTo = new TRI_EdgeIndexHash_t(
      HashElementKey, HashElementEdgeTo, IsEqualKeyEdgeTo, IsEqualElementEdge,
      IsEqualElementEdgeToByKey, _numBuckets, 64, context);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an index stub with a hard-coded selectivity estimate
/// this is used in the cluster coordinator case
////////////////////////////////////////////////////////////////////////////////

EdgeIndex::EdgeIndex(VPackSlice const& slice)
    : Index(slice), _edgesFrom(nullptr), _edgesTo(nullptr), _numBuckets(1) {}

EdgeIndex::~EdgeIndex() {
  delete _edgesTo;
  delete _edgesFrom;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a selectivity estimate for the index
////////////////////////////////////////////////////////////////////////////////

double EdgeIndex::selectivityEstimate() const {
  if (_edgesFrom == nullptr || _edgesTo == nullptr) {
    // use hard-coded selectivity estimate in case of cluster coordinator
    return _selectivityEstimate;
  }

  // return average selectivity of the two index parts
  double estimate = (_edgesFrom->selectivity() + _edgesTo->selectivity()) * 0.5;
  TRI_ASSERT(estimate >= 0.0 &&
             estimate <= 1.00001);  // floating-point tolerance
  return estimate;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the memory usage for the index
////////////////////////////////////////////////////////////////////////////////

size_t EdgeIndex::memory() const {
  TRI_ASSERT(_edgesFrom != nullptr);
  TRI_ASSERT(_edgesTo != nullptr);
  return _edgesFrom->memoryUsage() + _edgesTo->memoryUsage();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a VelocyPack representation of the index
////////////////////////////////////////////////////////////////////////////////

void EdgeIndex::toVelocyPack(VPackBuilder& builder, bool withFigures) const {
  Index::toVelocyPack(builder, withFigures);

  // hard-coded
  builder.add("unique", VPackValue(false));
  builder.add("sparse", VPackValue(false));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a VelocyPack representation of the index figures
////////////////////////////////////////////////////////////////////////////////

void EdgeIndex::toVelocyPackFigures(VPackBuilder& builder) const {
  Index::toVelocyPackFigures(builder);
  builder.add("buckets", VPackValue(_numBuckets));
}

int EdgeIndex::insert(arangodb::Transaction* trx, TRI_doc_mptr_t const* doc,
                      bool isRollback) {
  auto element = const_cast<TRI_doc_mptr_t*>(doc);
  _edgesFrom->insert(trx, element, true, isRollback);

  try {
    _edgesTo->insert(trx, element, true, isRollback);
  } catch (...) {
    _edgesFrom->remove(trx, element);
    throw;
  }

  return TRI_ERROR_NO_ERROR;
}

int EdgeIndex::remove(arangodb::Transaction* trx, TRI_doc_mptr_t const* doc,
                      bool) {
  _edgesFrom->remove(trx, doc);
  _edgesTo->remove(trx, doc);

  return TRI_ERROR_NO_ERROR;
}

int EdgeIndex::batchInsert(arangodb::Transaction* trx,
                           std::vector<TRI_doc_mptr_t const*> const* documents,
                           size_t numThreads) {
  _edgesFrom->batchInsert(
      trx, reinterpret_cast<std::vector<TRI_doc_mptr_t*> const*>(documents),
      numThreads);
  _edgesTo->batchInsert(
      trx, reinterpret_cast<std::vector<TRI_doc_mptr_t*> const*>(documents),
      numThreads);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up edges using the index, restarting at the edge pointed at
/// by next
////////////////////////////////////////////////////////////////////////////////

void EdgeIndex::lookup(arangodb::Transaction* trx,
                       TRI_edge_index_iterator_t const* edgeIndexIterator,
                       std::vector<TRI_doc_mptr_copy_t>& result,
                       TRI_doc_mptr_t*& next, size_t batchSize) {
  auto callback =
      [&result](TRI_doc_mptr_t* data) -> void { result.emplace_back(*(data)); };

  std::vector<TRI_doc_mptr_t*>* found = nullptr;
  if (next == nullptr) {
    if (edgeIndexIterator->_direction == TRI_EDGE_OUT) {
      found =
          _edgesFrom->lookupByKey(trx, &(edgeIndexIterator->_edge), batchSize);
    } else if (edgeIndexIterator->_direction == TRI_EDGE_IN) {
      found =
          _edgesTo->lookupByKey(trx, &(edgeIndexIterator->_edge), batchSize);
    } else {
      TRI_ASSERT(false);
    }
    if (found != nullptr && found->size() != 0) {
      next = found->back();
    }
  } else {
    if (edgeIndexIterator->_direction == TRI_EDGE_OUT) {
      found = _edgesFrom->lookupByKeyContinue(trx, next, batchSize);
    } else if (edgeIndexIterator->_direction == TRI_EDGE_IN) {
      found = _edgesTo->lookupByKeyContinue(trx, next, batchSize);
    } else {
      TRI_ASSERT(false);
    }
    if (found != nullptr && found->size() != 0) {
      next = found->back();
    } else {
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

int EdgeIndex::sizeHint(arangodb::Transaction* trx, size_t size) {
  // we assume this is called when setting up the index and the index
  // is still empty
  TRI_ASSERT(_edgesFrom->size() == 0);

  // set an initial size for the index for some new nodes to be created
  // without resizing
  int err = _edgesFrom->resize(trx, static_cast<uint32_t>(size + 2049));

  if (err != TRI_ERROR_NO_ERROR) {
    return err;
  }

  // we assume this is called when setting up the index and the index
  // is still empty
  TRI_ASSERT(_edgesTo->size() == 0);

  // set an initial size for the index for some new nodes to be created
  // without resizing
  return _edgesTo->resize(trx, static_cast<uint32_t>(size + 2049));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether the index supports the condition
////////////////////////////////////////////////////////////////////////////////

bool EdgeIndex::supportsFilterCondition(
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, size_t itemsInIndex,
    size_t& estimatedItems, double& estimatedCost) const {
  SimpleAttributeEqualityMatcher matcher(
      {{arangodb::basics::AttributeName(TRI_VOC_ATTRIBUTE_FROM, false)},
       {arangodb::basics::AttributeName(TRI_VOC_ATTRIBUTE_TO, false)}});
  return matcher.matchOne(this, node, reference, itemsInIndex, estimatedItems,
                          estimatedCost);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an IndexIterator for the given Condition
////////////////////////////////////////////////////////////////////////////////

IndexIterator* EdgeIndex::iteratorForCondition(
    arangodb::Transaction* trx, IndexIteratorContext* context,
    arangodb::aql::Ast* ast, arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, bool reverse) const {
  TRI_ASSERT(node->type == aql::NODE_TYPE_OPERATOR_NARY_AND);

  SimpleAttributeEqualityMatcher matcher(
      {{arangodb::basics::AttributeName(TRI_VOC_ATTRIBUTE_FROM, false)},
       {arangodb::basics::AttributeName(TRI_VOC_ATTRIBUTE_TO, false)}});

  TRI_ASSERT(node->numMembers() == 1);

  auto comp = node->getMember(0);

  // assume a.b == value
  auto attrNode = comp->getMember(0);
  auto valNode = comp->getMember(1);

  if (attrNode->type != aql::NODE_TYPE_ATTRIBUTE_ACCESS) {
    // got value == a.b  -> flip sides
    attrNode = comp->getMember(1);
    valNode = comp->getMember(0);
  }
  TRI_ASSERT(attrNode->type == aql::NODE_TYPE_ATTRIBUTE_ACCESS);

  if (comp->type == aql::NODE_TYPE_OPERATOR_BINARY_EQ) {
    // a.b == value
    return createIterator(
        trx, context, attrNode,
        std::vector<arangodb::aql::AstNode const*>({valNode}));
  }

  if (comp->type == aql::NODE_TYPE_OPERATOR_BINARY_IN) {
    // a.b IN values
    if (!valNode->isArray()) {
      return nullptr;
    }

    std::vector<arangodb::aql::AstNode const*> valNodes;
    size_t const n = valNode->numMembers();
    valNodes.reserve(n);
    for (size_t i = 0; i < n; ++i) {
      valNodes.emplace_back(valNode->getMemberUnchecked(i));
      TRI_IF_FAILURE("EdgeIndex::iteratorValNodes") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }
    }

    return createIterator(trx, context, attrNode, valNodes);
  }

  // operator type unsupported
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief specializes the condition for use with the index
////////////////////////////////////////////////////////////////////////////////

arangodb::aql::AstNode* EdgeIndex::specializeCondition(
    arangodb::aql::AstNode* node,
    arangodb::aql::Variable const* reference) const {
  SimpleAttributeEqualityMatcher matcher(
      {{arangodb::basics::AttributeName(TRI_VOC_ATTRIBUTE_FROM, false)},
       {arangodb::basics::AttributeName(TRI_VOC_ATTRIBUTE_TO, false)}});

  return matcher.specializeOne(this, node, reference);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create the iterator
////////////////////////////////////////////////////////////////////////////////

IndexIterator* EdgeIndex::createIterator(
    arangodb::Transaction* trx, IndexIteratorContext* context,
    arangodb::aql::AstNode const* attrNode,
    std::vector<arangodb::aql::AstNode const*> const& valNodes) const {
  // only leave the valid elements in the vector
  size_t const n = valNodes.size();
  std::vector<TRI_edge_header_t> keys;
  keys.reserve(n);

  for (size_t i = 0; i < n; ++i) {
    auto valNode = valNodes[i];

    if (!valNode->isStringValue()) {
      continue;
    }
    if (valNode->getStringLength() == 0) {
      continue;
    }

    TRI_voc_cid_t cid;
    char const* key;
    int res = context->resolveId(valNode->getStringValue(), cid, key);

    if (res != TRI_ERROR_NO_ERROR) {
      continue;
    }

    TRI_ASSERT(key != nullptr);
    TRI_ASSERT(cid != 0);

    keys.emplace_back(TRI_edge_header_t(cid, const_cast<char*>(key)));
    TRI_IF_FAILURE("EdgeIndex::collectKeys") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
  }

  TRI_IF_FAILURE("EdgeIndex::noIterator") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  // _from or _to?
  bool const isFrom =
      (strcmp(attrNode->getStringValue(), TRI_VOC_ATTRIBUTE_FROM) == 0);

  return new EdgeIndexIterator(trx, isFrom ? _edgesFrom : _edgesTo, keys);
}

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

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes an edge key
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementKey(void*, VPackSlice const* key) {
  // TODO: Can we unify all HashElementKey functions for VPack?
  TRI_ASSERT(key != nullptr);
  uint64_t hash = 0x87654321;
  if (!key->isString()) {
    // Illegal Edge entry, key has to be string.
    TRI_ASSERT(false);
    return hash;
  }
  return key->hash(hash);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes an edge (_from case)
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementEdgeFrom(void*, TRI_doc_mptr_t const* mptr,
                                    bool byKey) {
  TRI_ASSERT(mptr != nullptr);

  uint64_t hash = 0x87654321;

  if (!byKey) {
    hash = (uint64_t)mptr;
    hash = fasthash64(&hash, sizeof(hash), 0x56781234);
  } else {
    // Is identical to HashElementKey
    VPackSlice tmp(mptr->vpack());
    tmp = tmp.get(TRI_VOC_ATTRIBUTE_FROM);
    TRI_ASSERT(tmp.isString());
    hash = tmp.hash(hash);
  }
  return hash;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes an edge (_to case)
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementEdgeTo(void*, TRI_doc_mptr_t const* mptr,
                                  bool byKey) {
  TRI_ASSERT(mptr != nullptr);

  uint64_t hash = 0x87654321;

  if (!byKey) {
    hash = (uint64_t)mptr;
    hash = fasthash64(&hash, sizeof(hash), 0x56781234);
  } else {
    // Is identical to HashElementKey
    VPackSlice tmp(mptr->vpack());
    TRI_ASSERT(tmp.isObject());
    tmp = tmp.get(TRI_VOC_ATTRIBUTE_TO);
    TRI_ASSERT(tmp.isString());
    hash = tmp.hash(hash);
  }
  return hash;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if key and element match (_from case)
////////////////////////////////////////////////////////////////////////////////

static bool IsEqualKeyEdgeFrom(void*, VPackSlice const* left,
                               TRI_doc_mptr_t const* right) {
  TRI_ASSERT(left != nullptr);
  TRI_ASSERT(right != nullptr);

  // left is a key
  // right is an element, that is a master pointer
  VPackSlice tmp(right->vpack());
  tmp = tmp.get(TRI_VOC_ATTRIBUTE_FROM);
  TRI_ASSERT(tmp.isString());
  return *left == tmp;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if key and element match (_to case)
////////////////////////////////////////////////////////////////////////////////

static bool IsEqualKeyEdgeTo(void*, VPackSlice const* left,
                             TRI_doc_mptr_t const* right) {
  TRI_ASSERT(left != nullptr);
  TRI_ASSERT(right != nullptr);

  // left is a key
  // right is an element, that is a master pointer
  VPackSlice tmp(right->vpack());
  tmp = tmp.get(TRI_VOC_ATTRIBUTE_TO);
  TRI_ASSERT(tmp.isString());
  return *left == tmp;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks for elements are equal (_from and _to case)
////////////////////////////////////////////////////////////////////////////////

static bool IsEqualElementEdge(void*, TRI_doc_mptr_t const* left,
                               TRI_doc_mptr_t const* right) {
  return left == right;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks for elements are equal (_from case)
////////////////////////////////////////////////////////////////////////////////

static bool IsEqualElementEdgeFromByKey(void*,
                                        TRI_doc_mptr_t const* left,
                                        TRI_doc_mptr_t const* right) {
  TRI_ASSERT(left != nullptr);
  TRI_ASSERT(right != nullptr);

  VPackSlice lSlice(left->vpack());
  lSlice = lSlice.get(TRI_VOC_ATTRIBUTE_FROM);
  TRI_ASSERT(lSlice.isString());

  VPackSlice rSlice(right->vpack());
  rSlice = rSlice.get(TRI_VOC_ATTRIBUTE_FROM);
  TRI_ASSERT(rSlice.isString());
  return lSlice == rSlice;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks for elements are equal (_to case)
////////////////////////////////////////////////////////////////////////////////

static bool IsEqualElementEdgeToByKey(void*,
                                      TRI_doc_mptr_t const* left,
                                      TRI_doc_mptr_t const* right) {
  TRI_ASSERT(left != nullptr);
  TRI_ASSERT(right != nullptr);

  VPackSlice lSlice(left->vpack());
  lSlice = lSlice.get(TRI_VOC_ATTRIBUTE_TO);
  TRI_ASSERT(lSlice.isString());

  VPackSlice rSlice(right->vpack());
  rSlice = rSlice.get(TRI_VOC_ATTRIBUTE_TO);
  TRI_ASSERT(rSlice.isString());
  return lSlice == rSlice;
}

TRI_doc_mptr_t* EdgeIndexIterator::next() {
  while (true) {
    if (_position >= static_cast<size_t>(_keys.length())) {
      // we're at the end of the lookup values
      return nullptr;
    }

    if (_buffer == nullptr) {
      // We start a new lookup
      TRI_ASSERT(_position == 0);
      _posInBuffer = 0;
      _last = nullptr;

      VPackSlice tmp = _keys.at(_position);
      if (tmp.isObject()) {
        tmp = tmp.get(TRI_SLICE_KEY_EQUAL);
      }
      _buffer = _index->lookupByKey(_trx, &tmp, _batchSize);
      // fallthrough intentional
    } else if (_posInBuffer >= _buffer->size()) {
      // We have to refill the buffer
      delete _buffer;
      _buffer = nullptr;

      _posInBuffer = 0;
      if (_last != nullptr) {
        _buffer = _index->lookupByKeyContinue(_trx, _last, _batchSize);
      } else {
        VPackSlice tmp = _keys.at(_position);
        if (tmp.isObject()) {
          tmp = tmp.get(TRI_SLICE_KEY_EQUAL);
        }
        _buffer = _index->lookupByKey(_trx, &tmp, _batchSize);
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

TRI_doc_mptr_t* AnyDirectionEdgeIndexIterator::next() {
  TRI_doc_mptr_t* res = nullptr;
  if (_useInbound) {
    do {
      res = _inbound->next();
    } while (res != nullptr && _seen.find(res) != _seen.end());
    return res;
  }
  res = _outbound->next();
  if (res == nullptr) {
    _useInbound = true;
    return next();
  }
  _seen.emplace(res);
  return res;
}

void AnyDirectionEdgeIndexIterator::reset() {
  _useInbound = false;
  _seen.clear();
  _outbound->reset();
  _inbound->reset();
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

void EdgeIndex::buildSearchValue(TRI_edge_direction_e dir,
                                 std::string const& id, VPackBuilder& builder) {
  builder.openArray();
  switch (dir) {
    case TRI_EDGE_OUT:
      builder.openArray();
      builder.openObject();
      builder.add(TRI_SLICE_KEY_EQUAL, VPackValue(id));
      builder.close();
      builder.close();
      builder.add(VPackValue(VPackValueType::Null));
      break;
    case TRI_EDGE_IN:
      builder.add(VPackValue(VPackValueType::Null));
      builder.openArray();
      builder.openObject();
      builder.add(TRI_SLICE_KEY_EQUAL, VPackValue(id));
      builder.close();
      builder.close();
      break;
    case TRI_EDGE_ANY:
      builder.openArray();
      builder.openObject();
      builder.add(TRI_SLICE_KEY_EQUAL, VPackValue(id));
      builder.close();
      builder.close();
      builder.openArray();
      builder.openObject();
      builder.add(TRI_SLICE_KEY_EQUAL, VPackValue(id));
      builder.close();
      builder.close();
  }
  builder.close();
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
                       std::vector<TRI_doc_mptr_t>& result,
                       TRI_doc_mptr_t*& next, size_t batchSize) {
  auto callback =
      [&result](TRI_doc_mptr_t* data) -> void { result.emplace_back(*(data)); };

  std::vector<TRI_doc_mptr_t*>* found = nullptr;
  if (next == nullptr) {
    VPackSlice edge = edgeIndexIterator->_edge;
    if (edgeIndexIterator->_direction == TRI_EDGE_OUT) {
      found = _edgesFrom->lookupByKey(trx, &edge, batchSize);
    } else if (edgeIndexIterator->_direction == TRI_EDGE_IN) {
      found = _edgesTo->lookupByKey(trx, &edge, batchSize);
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

//////////////////////////////////////////////////////////////////////////////
/// @brief Transform the list of search slices to search values.
///        This will multiply all IN entries and simply return all other
///        entries.
//////////////////////////////////////////////////////////////////////////////

void EdgeIndex::expandInSearchValues(VPackSlice const slice,
                                     VPackBuilder& builder) const {
  TRI_ASSERT(slice.isArray());
  builder.openArray();
  for (auto const& side : VPackArrayIterator(slice)) {
    if (side.isNull()) {
      builder.add(side);
    } else {
      TRI_ASSERT(side.isArray());
      builder.openArray();
      for (auto const& item : VPackArrayIterator(side)) {
        TRI_ASSERT(item.isObject());
        if (item.hasKey(TRI_SLICE_KEY_EQUAL)) {
          TRI_ASSERT(!item.hasKey(TRI_SLICE_KEY_IN));
          builder.add(item);
        } else {
          TRI_ASSERT(item.hasKey(TRI_SLICE_KEY_IN));
          VPackSlice list = item.get(TRI_SLICE_KEY_IN);
          TRI_ASSERT(list.isArray());
          for (auto const& it : VPackArrayIterator(list)) {
            builder.openObject();
            builder.add(TRI_SLICE_KEY_EQUAL, it);
            builder.close();
          }
        }
      }
      builder.close();
    }
  }
  builder.close();
}
////////////////////////////////////////////////////////////////////////////////
/// @brief creates an IndexIterator for the given VelocyPackSlices.
///        The searchValue is a an Array with exactly two Entries.
///        If the first is set it means we are searching for _from (OUTBOUND),
///        if the second is set we are searching for _to (INBOUND).
///        if both are set we are search for ANY direction. Result is made
///        DISTINCT.
///        Each defined slice that is set has to be list of keys to search for.
///        Each key needs to have the following formats:
///
///        1) {"eq": <compareValue>} // The value in index is exactly this
///        
///        Reverse is not supported, hence ignored
///        NOTE: The iterator is only valid as long as the slice points to
///        a valid memory region.
////////////////////////////////////////////////////////////////////////////////

IndexIterator* EdgeIndex::iteratorForSlice(
    arangodb::Transaction* trx, IndexIteratorContext*,
    arangodb::velocypack::Slice const searchValues, bool) const {
  if (!searchValues.isArray() || searchValues.length() != 2) {
    // Invalid searchValue
    return nullptr;
  }
  VPackSlice const from = searchValues.at(0);
  VPackSlice const to = searchValues.at(1);

  if (!from.isNull()) {
    TRI_ASSERT(from.isArray());
    if (!to.isNull()) {
      // ANY search
      TRI_ASSERT(to.isArray());
      auto left = std::make_unique<EdgeIndexIterator>(trx, _edgesFrom, from);
      auto right = std::make_unique<EdgeIndexIterator>(trx, _edgesTo, to);
      return new AnyDirectionEdgeIndexIterator(left.release(), right.release());
    }
    // OUTBOUND search
    TRI_ASSERT(to.isNull());
    return new EdgeIndexIterator(trx, _edgesFrom, from);
  } else {
    // INBOUND search
    TRI_ASSERT(to.isArray());
    return new EdgeIndexIterator(trx, _edgesTo, to);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create the iterator
////////////////////////////////////////////////////////////////////////////////

IndexIterator* EdgeIndex::createIterator(
    arangodb::Transaction* trx, IndexIteratorContext* context,
    arangodb::aql::AstNode const* attrNode,
    std::vector<arangodb::aql::AstNode const*> const& valNodes) const {
  // only leave the valid elements in the vector
  VPackBuilder keys;
  keys.openArray();

  for (auto const& valNode : valNodes) {
    if (!valNode->isStringValue()) {
      continue;
    }
    if (valNode->getStringLength() == 0) {
      continue;
    }

    keys.openObject();
    keys.add(TRI_SLICE_KEY_EQUAL, VPackValue(valNode->getStringValue()));
    keys.close();
    TRI_IF_FAILURE("EdgeIndex::collectKeys") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
  }

  TRI_IF_FAILURE("EdgeIndex::noIterator") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  keys.close();

  // _from or _to?
  bool const isFrom =
      (strcmp(attrNode->getStringValue(), TRI_VOC_ATTRIBUTE_FROM) == 0);

  return new EdgeIndexIterator(trx, isFrom ? _edgesFrom : _edgesTo, std::move(keys));
}

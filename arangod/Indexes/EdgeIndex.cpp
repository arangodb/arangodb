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
#include "Basics/StaticStrings.h"
#include "Basics/fasthash.h"
#include "Basics/hashes.h"
#include "Indexes/SimpleAttributeEqualityMatcher.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/Transaction.h"
#include "Utils/TransactionContext.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/transaction.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
   
////////////////////////////////////////////////////////////////////////////////
/// @brief hard-coded vector of the index attributes
/// note that the attribute names must be hard-coded here to avoid an init-order
/// fiasco with StaticStrings::FromString etc.
////////////////////////////////////////////////////////////////////////////////

static std::vector<std::vector<arangodb::basics::AttributeName>> const IndexAttributes
    {{arangodb::basics::AttributeName("_from", false)},
     {arangodb::basics::AttributeName("_to", false)}};

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes an edge key
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementKey(void*, VPackSlice const* key) {
  // TODO: Can we unify all HashElementKey functions for VPack?
  TRI_ASSERT(key != nullptr);
  uint64_t hash = 0x87654321;
  if (!key->isString()) {
    // Illegal edge entry, key has to be string.
    TRI_ASSERT(false);
    return hash;
  }
  // we can get away with the fast hash function here, as edge
  // index values are restricted to strings
  return key->hashString(hash);
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
    VPackSlice tmp = Transaction::extractFromFromDocument(VPackSlice(mptr->vpack()));
    TRI_ASSERT(tmp.isString());
    // we can get away with the fast hash function here, as edge
    // index values are restricted to strings
    hash = tmp.hashString(hash);
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
    VPackSlice tmp = Transaction::extractToFromDocument(VPackSlice(mptr->vpack()));
    TRI_ASSERT(tmp.isString());
    // we can get away with the fast hash function here, as edge
    // index values are restricted to strings
    hash = tmp.hashString(hash);
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
  VPackSlice tmp = Transaction::extractFromFromDocument(VPackSlice(right->vpack()));
  TRI_ASSERT(tmp.isString());
  return (*left).equals(tmp);
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
  VPackSlice tmp = Transaction::extractToFromDocument(VPackSlice(right->vpack()));
  TRI_ASSERT(tmp.isString());
  return (*left).equals(tmp);
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

  VPackSlice lSlice = Transaction::extractFromFromDocument(VPackSlice(left->vpack()));
  TRI_ASSERT(lSlice.isString());

  VPackSlice rSlice = Transaction::extractFromFromDocument(VPackSlice(right->vpack()));
  TRI_ASSERT(rSlice.isString());

  return lSlice.equals(rSlice);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks for elements are equal (_to case)
////////////////////////////////////////////////////////////////////////////////

static bool IsEqualElementEdgeToByKey(void*,
                                      TRI_doc_mptr_t const* left,
                                      TRI_doc_mptr_t const* right) {
  TRI_ASSERT(left != nullptr);
  TRI_ASSERT(right != nullptr);

  VPackSlice lSlice = Transaction::extractToFromDocument(VPackSlice(left->vpack()));
  TRI_ASSERT(lSlice.isString());
  
  VPackSlice rSlice = Transaction::extractToFromDocument(VPackSlice(right->vpack()));
  TRI_ASSERT(rSlice.isString());

  return lSlice.equals(rSlice);
}
    
EdgeIndexIterator::~EdgeIndexIterator() {
  if (_keys != nullptr) {
    // return the VPackBuilder to the transaction context 
    _trx->transactionContextPtr()->returnBuilder(_keys.release());
  }
}

TRI_doc_mptr_t* EdgeIndexIterator::next() {
  while (_iterator.valid()) {
    if (_buffer.empty()) {
      // We start a new lookup
      _posInBuffer = 0;

      VPackSlice tmp = _iterator.value();
      if (tmp.isObject()) {
        tmp = tmp.get(StaticStrings::IndexEq);
      }
      _index->lookupByKey(_trx, &tmp, _buffer, _batchSize);
      // fallthrough intentional
    } else if (_posInBuffer >= _buffer.size()) {
      // We have to refill the buffer
      auto last = _buffer.back();
      _buffer.clear();

      _posInBuffer = 0;
      _index->lookupByKeyContinue(_trx, last, _buffer, _batchSize);
    }

    if (!_buffer.empty()) {
      // found something
      return _buffer.at(_posInBuffer++);
    }

    // found no result. now go to next lookup value in _keys
    _iterator.next();
  }

  return nullptr;
}

void EdgeIndexIterator::nextBabies(std::vector<TRI_doc_mptr_t*>& buffer, size_t limit) {
  size_t atMost = _batchSize > limit ? limit : _batchSize;

  if (atMost == 0) {
    // nothing to do
    _buffer.clear();
    return;
  }

  while (_iterator.valid()) {
    if (buffer.empty()) {
      VPackSlice tmp = _iterator.value();
      if (tmp.isObject()) {
        tmp = tmp.get(StaticStrings::IndexEq);
      }
      _index->lookupByKey(_trx, &tmp, buffer, atMost);
      // fallthrough intentional
    } else {
      // Continue the lookup
      auto last = buffer.back();
      buffer.clear();

      _index->lookupByKeyContinue(_trx, last, buffer, atMost);
    }

    if (!buffer.empty()) {
      // found something
      return;
    }

    // found no result. now go to next lookup value in _keys
    _iterator.next();
  }

  buffer.clear();
}

void EdgeIndexIterator::reset() {
  _posInBuffer = 0;
  _buffer.clear();
  _iterator.reset();
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

void AnyDirectionEdgeIndexIterator::nextBabies(std::vector<TRI_doc_mptr_t*>& result, size_t limit) {
  result.clear();
  for (size_t i = 0; i < limit; ++i) {
    TRI_doc_mptr_t* res = next();
    if (res == nullptr) {
      return;
    }
    result.emplace_back(res);
  }
}

void AnyDirectionEdgeIndexIterator::reset() {
  _useInbound = false;
  _seen.clear();
  _outbound->reset();
  _inbound->reset();
}


EdgeIndex::EdgeIndex(TRI_idx_iid_t iid, arangodb::LogicalCollection* collection)
    : Index(iid, collection,
            std::vector<std::vector<arangodb::basics::AttributeName>>(
                {{arangodb::basics::AttributeName(StaticStrings::FromString, false)},
                 {arangodb::basics::AttributeName(StaticStrings::ToString, false)}}),
            false, false),
      _edgesFrom(nullptr),
      _edgesTo(nullptr),
      _numBuckets(1) {
  TRI_ASSERT(iid != 0);

  if (collection != nullptr) {
    // document is a nullptr in the coordinator case
    _numBuckets = static_cast<size_t>(collection->indexBuckets());
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
      builder.add(StaticStrings::IndexEq, VPackValue(id));
      builder.close();
      builder.close();
      builder.add(VPackValue(VPackValueType::Null));
      break;
    case TRI_EDGE_IN:
      builder.add(VPackValue(VPackValueType::Null));
      builder.openArray();
      builder.openObject();
      builder.add(StaticStrings::IndexEq, VPackValue(id));
      builder.close();
      builder.close();
      break;
    case TRI_EDGE_ANY:
      builder.openArray();
      builder.openObject();
      builder.add(StaticStrings::IndexEq, VPackValue(id));
      builder.close();
      builder.close();
      builder.openArray();
      builder.openObject();
      builder.add(StaticStrings::IndexEq, VPackValue(id));
      builder.close();
      builder.close();
  }
  builder.close();
}

void EdgeIndex::buildSearchValue(TRI_edge_direction_e dir,
                                 VPackSlice const& id, VPackBuilder& builder) {
  TRI_ASSERT(id.isString());
  builder.openArray();
  switch (dir) {
    case TRI_EDGE_OUT:
      builder.openArray();
      builder.openObject();
      builder.add(StaticStrings::IndexEq, id);
      builder.close();
      builder.close();
      builder.add(VPackValue(VPackValueType::Null));
      break;
    case TRI_EDGE_IN:
      builder.add(VPackValue(VPackValueType::Null));
      builder.openArray();
      builder.openObject();
      builder.add(StaticStrings::IndexEq, id);
      builder.close();
      builder.close();
      break;
    case TRI_EDGE_ANY:
      builder.openArray();
      builder.openObject();
      builder.add(StaticStrings::IndexEq, id);
      builder.close();
      builder.close();
      builder.openArray();
      builder.openObject();
      builder.add(StaticStrings::IndexEq, id);
      builder.close();
      builder.close();
  }
  builder.close();
}

void EdgeIndex::buildSearchValueFromArray(TRI_edge_direction_e dir,
                                          VPackSlice const ids,
                                          VPackBuilder& builder) {
  TRI_ASSERT(ids.isArray());
  builder.openArray();
  switch (dir) {
    case TRI_EDGE_OUT:
      builder.openArray();
      for (auto const& id : VPackArrayIterator(ids)) {
        if (id.isString()) {
          builder.openObject();
          builder.add(StaticStrings::IndexEq, id);
          builder.close();
        }
      }
      builder.close();
      builder.add(VPackValue(VPackValueType::Null));
      break;
    case TRI_EDGE_IN:
      builder.add(VPackValue(VPackValueType::Null));
      builder.openArray();
      for (auto const& id : VPackArrayIterator(ids)) {
        if (id.isString()) {
          builder.openObject();
          builder.add(StaticStrings::IndexEq, id);
          builder.close();
        }
      }
      builder.close();
      break;
    case TRI_EDGE_ANY:
      builder.openArray();
      for (auto const& id : VPackArrayIterator(ids)) {
        if (id.isString()) {
          builder.openObject();
          builder.add(StaticStrings::IndexEq, id);
          builder.close();
        }
      }
      builder.close();
      builder.openArray();
      for (auto const& id : VPackArrayIterator(ids)) {
        if (id.isString()) {
          builder.openObject();
          builder.add(StaticStrings::IndexEq, id);
          builder.close();
        }
      }
      builder.close();
  }
  builder.close();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a selectivity estimate for the index
////////////////////////////////////////////////////////////////////////////////

double EdgeIndex::selectivityEstimate() const {
  if (_edgesFrom == nullptr || 
      _edgesTo == nullptr || 
      ServerState::instance()->isCoordinator()) {
    // use hard-coded selectivity estimate in case of cluster coordinator
    return 0.1;
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
  if (!documents->empty()) {
    _edgesFrom->batchInsert(
        trx, reinterpret_cast<std::vector<TRI_doc_mptr_t*> const*>(documents),
        numThreads);
    _edgesTo->batchInsert(
        trx, reinterpret_cast<std::vector<TRI_doc_mptr_t*> const*>(documents),
        numThreads);
  }

  return TRI_ERROR_NO_ERROR;
}

/// @brief unload the index data from memory
int EdgeIndex::unload() {
  _edgesFrom->truncate([](TRI_doc_mptr_t*) -> bool { return true; });
  _edgesTo->truncate([](TRI_doc_mptr_t*) -> bool { return true; });

  return TRI_ERROR_NO_ERROR;
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

  SimpleAttributeEqualityMatcher matcher(IndexAttributes);
  return matcher.matchOne(this, node, reference, itemsInIndex, estimatedItems,
                          estimatedCost);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an IndexIterator for the given Condition
////////////////////////////////////////////////////////////////////////////////

IndexIterator* EdgeIndex::iteratorForCondition(
    arangodb::Transaction* trx, IndexIteratorContext* context,
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, bool reverse) const {
  TRI_ASSERT(node->type == aql::NODE_TYPE_OPERATOR_NARY_AND);

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
    return createEqIterator(trx, context, attrNode, valNode);
  }

  if (comp->type == aql::NODE_TYPE_OPERATOR_BINARY_IN) {
    // a.b IN values
    if (!valNode->isArray()) {
      return nullptr;
    }

    return createInIterator(trx, context, attrNode, valNode);
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
  
  SimpleAttributeEqualityMatcher matcher(IndexAttributes);
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
        if (item.hasKey(StaticStrings::IndexEq)) {
          TRI_ASSERT(!item.hasKey(StaticStrings::IndexIn));
          builder.add(item);
        } else {
          TRI_ASSERT(item.hasKey(StaticStrings::IndexIn));
          VPackSlice list = item.get(StaticStrings::IndexIn);
          TRI_ASSERT(list.isArray());
          for (auto const& it : VPackArrayIterator(list)) {
            builder.openObject();
            builder.add(StaticStrings::IndexEq, it);
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
      TransactionBuilderLeaser fromBuilder(trx);
      std::unique_ptr<VPackBuilder> fromKeys(fromBuilder.steal());
      fromKeys->add(from);
      auto left = std::make_unique<EdgeIndexIterator>(trx, _edgesFrom, fromKeys);

      TransactionBuilderLeaser toBuilder(trx);
      std::unique_ptr<VPackBuilder> toKeys(toBuilder.steal());
      toKeys->add(to);
      auto right = std::make_unique<EdgeIndexIterator>(trx, _edgesTo, toKeys);
      return new AnyDirectionEdgeIndexIterator(left.release(), right.release());
    }
    // OUTBOUND search
    TRI_ASSERT(to.isNull());
    TransactionBuilderLeaser builder(trx);
    std::unique_ptr<VPackBuilder> keys(builder.steal());
    keys->add(from);
    return new EdgeIndexIterator(trx, _edgesFrom, keys);
  } else {
    // INBOUND search
    TRI_ASSERT(to.isArray());
    TransactionBuilderLeaser builder(trx);
    std::unique_ptr<VPackBuilder> keys(builder.steal());
    keys->add(to);
    return new EdgeIndexIterator(trx, _edgesTo, keys);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create the iterator
////////////////////////////////////////////////////////////////////////////////

IndexIterator* EdgeIndex::createEqIterator(
    arangodb::Transaction* trx, IndexIteratorContext* context,
    arangodb::aql::AstNode const* attrNode,
    arangodb::aql::AstNode const* valNode) const {
  
  // lease builder, but immediately pass it to the unique_ptr so we don't leak  
  TransactionBuilderLeaser builder(trx);
  std::unique_ptr<VPackBuilder> keys(builder.steal());
  keys->openArray();

  handleValNode(keys.get(), valNode);
  TRI_IF_FAILURE("EdgeIndex::noIterator") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  keys->close();

  // _from or _to?
  bool const isFrom = (attrNode->stringEquals(StaticStrings::FromString));

  return new EdgeIndexIterator(trx, isFrom ? _edgesFrom : _edgesTo, keys);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create the iterator
////////////////////////////////////////////////////////////////////////////////

IndexIterator* EdgeIndex::createInIterator(
    arangodb::Transaction* trx, IndexIteratorContext* context,
    arangodb::aql::AstNode const* attrNode,
    arangodb::aql::AstNode const* valNode) const {
  
  // lease builder, but immediately pass it to the unique_ptr so we don't leak  
  TransactionBuilderLeaser builder(trx);
  std::unique_ptr<VPackBuilder> keys(builder.steal());
  keys->openArray();
    
  size_t const n = valNode->numMembers();
  for (size_t i = 0; i < n; ++i) {
    handleValNode(keys.get(), valNode->getMemberUnchecked(i));
    TRI_IF_FAILURE("EdgeIndex::iteratorValNodes") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
  }

  TRI_IF_FAILURE("EdgeIndex::noIterator") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  keys->close();

  // _from or _to?
  bool const isFrom = (attrNode->stringEquals(StaticStrings::FromString));

  return new EdgeIndexIterator(trx, isFrom ? _edgesFrom : _edgesTo, keys);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a single value node to the iterator's keys
////////////////////////////////////////////////////////////////////////////////
    
void EdgeIndex::handleValNode(VPackBuilder* keys,
                              arangodb::aql::AstNode const* valNode) const {
  if (!valNode->isStringValue() || valNode->getStringLength() == 0) {
    return;
  }

  keys->openObject();
  keys->add(StaticStrings::IndexEq, VPackValuePair(valNode->getStringValue(), valNode->getStringLength(), VPackValueType::String));
  keys->close();
  
  TRI_IF_FAILURE("EdgeIndex::collectKeys") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
}

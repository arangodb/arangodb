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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBEdgeIndex.h"
#include "Aql/AstNode.h"
#include "Aql/SortCondition.h"
#include "Basics/Exceptions.h"
#include "Basics/LocalTaskQueue.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringRef.h"
#include "Basics/VelocyPackHelper.h"
#include "Cache/CachedValue.h"
#include "Cache/TransactionalCache.h"
#include "Indexes/SimpleAttributeEqualityMatcher.h"
#include "Transaction/Context.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"

#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBCounterManager.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "RocksDBEngine/RocksDBMethods.h"
#include "RocksDBEngine/RocksDBToken.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "RocksDBEngine/RocksDBTypes.h"

#include <rocksdb/db.h>
#include <rocksdb/utilities/transaction_db.h>
#include <rocksdb/utilities/write_batch_with_index.h>

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include <cmath>

using namespace arangodb;
using namespace arangodb::basics;

RocksDBEdgeIndexIterator::RocksDBEdgeIndexIterator(
    LogicalCollection* collection, transaction::Methods* trx,
    ManagedDocumentResult* mmdr, arangodb::RocksDBEdgeIndex const* index,
    std::unique_ptr<VPackBuilder>& keys, cache::Cache* cache)
    : IndexIterator(collection, trx, mmdr, index),
      _keys(keys.get()),
      _keysIterator(_keys->slice()),
      _index(index),
      _iterator(
          rocksutils::toRocksMethods(trx)->NewIterator(index->columnFamily())),
      _bounds(RocksDBKeyBounds::EdgeIndex(0)),
      _cache(cache),
      _posInMemory(0),
      _memSize(26),
      _inplaceMemory(nullptr) {
  // We allocate enough memory for 25 elements + 1 size.
  // Maybe adjust this.
  _inplaceMemory = new uint64_t[_memSize];
  keys.release();  // now we have ownership for _keys
  TRI_ASSERT(_keys != nullptr);
  TRI_ASSERT(_keys->slice().isArray());
  resetInplaceMemory();
}

RocksDBEdgeIndexIterator::~RocksDBEdgeIndexIterator() {
  delete[] _inplaceMemory;
  if (_keys != nullptr) {
    // return the VPackBuilder to the transaction context
    _trx->transactionContextPtr()->returnBuilder(_keys.release());
  }
}

void RocksDBEdgeIndexIterator::resizeMemory() {
  // Increase size by factor of two.
  // TODO Adjust this has potential to kill memory...
  uint64_t* tmp = new uint64_t[_memSize * 2];
  std::memcpy(tmp, _inplaceMemory, _memSize * sizeof(uint64_t));
  _memSize *= 2;
  delete[] _inplaceMemory;
  _inplaceMemory = tmp;
}

void RocksDBEdgeIndexIterator::reserveInplaceMemory(uint64_t count) {
  // NOTE: count the number of cached edges, 1 is the size
  if (count + 1 > _memSize) {
    // In this case the current memory is too small.
    // Reserve more
    delete[] _inplaceMemory;
    _inplaceMemory = new uint64_t[count + 1];
    _memSize = count + 1;
  }
  // else NOOP, we have enough memory to write to
}

uint64_t RocksDBEdgeIndexIterator::valueLength() const {
  return *_inplaceMemory;
}

void RocksDBEdgeIndexIterator::resetInplaceMemory() {
  // It is sufficient to only set the first element
  // We always have to make sure to only access elements
  // at a position lower than valueLength()
  *_inplaceMemory = 0;

  // Position 0 points to the size.
  // This is defined as the invalid position
  _posInMemory = 0;
}

void RocksDBEdgeIndexIterator::reset() {
  resetInplaceMemory();
  _keysIterator.reset();
}

bool RocksDBEdgeIndexIterator::next(TokenCallback const& cb, size_t limit) {
  TRI_ASSERT(_trx->state()->isRunning());
#ifdef USE_MAINTAINER_MODE
  TRI_ASSERT(limit > 0);  // Someone called with limit == 0. Api broken
#else
  // Gracefully return in production code
  // Nothing bad has happened
  if (limit == 0) {
    return false;
  }
#endif

  while (limit > 0) {
    if (_posInMemory > 0) {
      // We still have unreturned edges in out memory.
      // Just plainly return those.
      size_t atMost = (std::min)(static_cast<uint64_t>(limit),
                                 valueLength() + 1 /*size*/ - _posInMemory);
      for (size_t i = 0; i < atMost; ++i) {
        cb(RocksDBToken{*(_inplaceMemory + _posInMemory++)});
      }
      limit -= atMost;
      if (_posInMemory == valueLength() + 1) {
        // We have returned everthing we had in local buffer, reset it
        resetInplaceMemory();
      } else {
        TRI_ASSERT(limit == 0);
        return true;
      }
    }

    if (!_keysIterator.valid()) {
      // We are done iterating
      return false;
    }

    // We have exhausted local memory.
    // Now fill it again:
    VPackSlice fromToSlice = _keysIterator.value();
    if (fromToSlice.isObject()) {
      fromToSlice = fromToSlice.get(StaticStrings::IndexEq);
    }
    TRI_ASSERT(fromToSlice.isString());
    StringRef fromTo(fromToSlice);

    bool needRocksLookup = true;
    if (_cache != nullptr) {
      // Try to read from cache
      auto finding = _cache->find(fromTo.data(), (uint32_t)fromTo.size());
      if (finding.found()) {
        needRocksLookup = false;
        // We got sth. in the cache
        uint64_t* cachedData = (uint64_t*)finding.value()->value();
        uint64_t cachedLength = *cachedData;
        if (cachedLength < limit) {
          // Save copies, just return it.
          for (uint64_t i = 0; i < cachedLength; ++i) {
            cb(RocksDBToken{*(cachedData + 1 + i)});
          }
          limit -= cachedLength;
        } else {
          // We need to copy it.
          // And then we just get back to beginning of the loop
          reserveInplaceMemory(cachedLength);
          // It is now guaranteed that memcpy will succeed
          std::memcpy(_inplaceMemory, cachedData,
                      (cachedLength + 1 /*size*/) * sizeof(uint64_t));
          TRI_ASSERT(valueLength() == cachedLength);
          // Set to first document.
          _posInMemory = 1;
          // Do not set limit
        }
      }
    }

    if (needRocksLookup) {
      lookupInRocksDB(fromTo);
    }

    // We cannot be more advanced here
    TRI_ASSERT(_posInMemory == 1 || _posInMemory == 0);

    _keysIterator.next();
  }
  TRI_ASSERT(limit == 0);
  return (_posInMemory != 0) || _keysIterator.valid();
}

void RocksDBEdgeIndexIterator::lookupInRocksDB(StringRef fromTo) {
  // Bad case read from RocksDB
  _bounds = RocksDBKeyBounds::EdgeIndexVertex(_index->_objectId, fromTo);
  _iterator->Seek(_bounds.start());
  resetInplaceMemory();
  _posInMemory = 1;
  RocksDBCollection* rocksColl = toRocksDBCollection(_collection);
  rocksdb::Comparator const* cmp = _index->comparator();

  RocksDBToken token;
  auto end = _bounds.end();
  while (_iterator->Valid() &&
         (cmp->Compare(_iterator->key(), end) < 0)) {
    StringRef edgeKey = RocksDBKey::primaryKey(_iterator->key());
    Result res = rocksColl->lookupDocumentToken(_trx, edgeKey, token);
    if (res.ok()) {
      *(_inplaceMemory + _posInMemory) = token.revisionId();
      _posInMemory++;
      (*_inplaceMemory)++;  // Increase size
      if (_posInMemory == _memSize) {
        resizeMemory();
      }
#ifdef USE_MAINTAINER_MODE
    } else {
      // Index inconsistency, we indexed a primaryKey => revision that is
      // not known any more
      TRI_ASSERT(res.ok());
#endif
    }
    _iterator->Next();
  }
  if (_cache != nullptr) {
    // TODO Add cache retry on next call
    // Now we have something in _inplaceMemory.
    // It may be an empty array or a filled one, never mind, we cache both
    auto entry = cache::CachedValue::construct(
        fromTo.data(), static_cast<uint32_t>(fromTo.size()), _inplaceMemory,
        sizeof(uint64_t) * (valueLength() + 1 /*size*/));
    bool cached = _cache->insert(entry);
    if (!cached) {
      delete entry;
    }
  }
  _posInMemory = 1;
}

// ============================= Index ====================================

uint64_t RocksDBEdgeIndex::HashForKey(const rocksdb::Slice& key) {
  std::hash<StringRef> hasher;
  // NOTE: This function needs to use the same hashing on the
  // indexed VPack as the initial inserter does
  StringRef tmp = RocksDBKey::vertexId(key);
  return static_cast<uint64_t>(hasher(tmp));
}

RocksDBEdgeIndex::RocksDBEdgeIndex(TRI_idx_iid_t iid,
                                   arangodb::LogicalCollection* collection,
                                   VPackSlice const& info,
                                   std::string const& attr)
    : RocksDBIndex(iid, collection, std::vector<std::vector<AttributeName>>(
                                        {{AttributeName(attr, false)}}),
                   false, false, RocksDBColumnFamily::edge(),
                   basics::VelocyPackHelper::stringUInt64(info, "objectId"),
                   !ServerState::instance()->isCoordinator() /*useCache*/
                   ),
      _directionAttr(attr),
      _estimator(nullptr) {
  if (!ServerState::instance()->isCoordinator()) {
    // We activate the estimator only on DBServers
    _estimator = std::make_unique<RocksDBCuckooIndexEstimator<uint64_t>>(
        RocksDBIndex::ESTIMATOR_SIZE);
    TRI_ASSERT(_estimator != nullptr);
  }
  TRI_ASSERT(iid != 0);
  TRI_ASSERT(_objectId != 0);
  // if we never hit the assertions we need to remove the
  // following code
  // FIXME
  if (_objectId == 0) {
    // disable cache?
    _useCache = false;
  }
}

RocksDBEdgeIndex::~RocksDBEdgeIndex() {}

/// @brief return a selectivity estimate for the index
double RocksDBEdgeIndex::selectivityEstimate(
    arangodb::StringRef const* attribute) const {
  if (ServerState::instance()->isCoordinator()) {
    // use hard-coded selectivity estimate in case of cluster coordinator
    return 0.1;
  }

  if (attribute != nullptr && attribute->compare(_directionAttr)) {
    return 0;
  }
  TRI_ASSERT(_estimator != nullptr);
  return _estimator->computeEstimate();
}

/// @brief return the memory usage for the index
size_t RocksDBEdgeIndex::memory() const {
  rocksdb::TransactionDB* db = rocksutils::globalRocksDB();
  RocksDBKeyBounds bounds = RocksDBKeyBounds::EdgeIndex(_objectId);
  rocksdb::Range r(bounds.start(), bounds.end());
  uint64_t out;
  db->GetApproximateSizes(&r, 1, &out, true);
  return (size_t)out;
}

/// @brief return a VelocyPack representation of the index
void RocksDBEdgeIndex::toVelocyPack(VPackBuilder& builder, bool withFigures,
                                    bool forPersistence) const {
  builder.openObject();
  RocksDBIndex::toVelocyPack(builder, withFigures, forPersistence);
  // add selectivity estimate hard-coded
  builder.add("unique", VPackValue(false));
  builder.add("sparse", VPackValue(false));
  builder.close();
}

int RocksDBEdgeIndex::insert(transaction::Methods* trx,
                             TRI_voc_rid_t revisionId, VPackSlice const& doc,
                             bool isRollback) {
  VPackSlice primaryKey = doc.get(StaticStrings::KeyString);
  VPackSlice fromTo = doc.get(_directionAttr);
  TRI_ASSERT(primaryKey.isString() && fromTo.isString());
  auto fromToRef = StringRef(fromTo);
  RocksDBKey key =
      RocksDBKey::EdgeIndexValue(_objectId, fromToRef, StringRef(primaryKey));
  // blacklist key in cache
  blackListKey(fromToRef);

  // acquire rocksdb transaction
  RocksDBMethods* mthd = rocksutils::toRocksMethods(trx);
  Result r = mthd->Put(_cf, rocksdb::Slice(key.string()), rocksdb::Slice(),
                       rocksutils::index);
  if (r.ok()) {
    std::hash<StringRef> hasher;
    uint64_t hash = static_cast<uint64_t>(hasher(fromToRef));
    _estimator->insert(hash);
    return TRI_ERROR_NO_ERROR;
  } else {
    return r.errorNumber();
  }
}

int RocksDBEdgeIndex::insertRaw(RocksDBMethods*, TRI_voc_rid_t,
                                VPackSlice const&) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

int RocksDBEdgeIndex::remove(transaction::Methods* trx,
                             TRI_voc_rid_t revisionId, VPackSlice const& doc,
                             bool isRollback) {
  VPackSlice primaryKey = doc.get(StaticStrings::KeyString);
  VPackSlice fromTo = doc.get(_directionAttr);
  auto fromToRef = StringRef(fromTo);
  TRI_ASSERT(primaryKey.isString() && fromTo.isString());
  RocksDBKey key =
      RocksDBKey::EdgeIndexValue(_objectId, fromToRef, StringRef(primaryKey));

  // blacklist key in cache
  blackListKey(fromToRef);

  // acquire rocksdb transaction
  RocksDBMethods* mthd = rocksutils::toRocksMethods(trx);
  Result res = mthd->Delete(_cf, rocksdb::Slice(key.string()));
  if (res.ok()) {
    std::hash<StringRef> hasher;
    uint64_t hash = static_cast<uint64_t>(hasher(fromToRef));
    _estimator->remove(hash);
    return TRI_ERROR_NO_ERROR;
  } else {
    return res.errorNumber();
  }
}

/// optimization for truncateNoTrx, never called in fillIndex
int RocksDBEdgeIndex::removeRaw(RocksDBMethods*, TRI_voc_rid_t,
                                VPackSlice const&) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

void RocksDBEdgeIndex::batchInsert(
    transaction::Methods* trx,
    std::vector<std::pair<TRI_voc_rid_t, VPackSlice>> const& documents,
    std::shared_ptr<arangodb::basics::LocalTaskQueue> queue) {
  RocksDBMethods* mthd = rocksutils::toRocksMethods(trx);
  for (std::pair<TRI_voc_rid_t, VPackSlice> const& doc : documents) {
    VPackSlice primaryKey = doc.second.get(StaticStrings::KeyString);
    VPackSlice fromTo = doc.second.get(_directionAttr);
    TRI_ASSERT(primaryKey.isString() && fromTo.isString());
    auto fromToRef = StringRef(fromTo);
    RocksDBKey key =
        RocksDBKey::EdgeIndexValue(_objectId, fromToRef, StringRef(primaryKey));

    blackListKey(fromToRef);
    Result r = mthd->Put(_cf, rocksdb::Slice(key.string()), rocksdb::Slice(),
                         rocksutils::index);
    if (!r.ok()) {
      queue->setStatus(r.errorNumber());
      break;
    }
  }
}

/// @brief called when the index is dropped
int RocksDBEdgeIndex::drop() {
  // First drop the cache all indexes can work without it.
  RocksDBIndex::drop();
  return rocksutils::removeLargeRange(rocksutils::globalRocksDB(),
                                      RocksDBKeyBounds::EdgeIndex(_objectId))
      .errorNumber();
}

/// @brief checks whether the index supports the condition
bool RocksDBEdgeIndex::supportsFilterCondition(
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, size_t itemsInIndex,
    size_t& estimatedItems, double& estimatedCost) const {
  SimpleAttributeEqualityMatcher matcher(this->_fields);
  return matcher.matchOne(this, node, reference, itemsInIndex, estimatedItems,
                          estimatedCost);
}

/// @brief creates an IndexIterator for the given Condition
IndexIterator* RocksDBEdgeIndex::iteratorForCondition(
    transaction::Methods* trx, ManagedDocumentResult* mmdr,
    arangodb::aql::AstNode const* node,

    arangodb::aql::Variable const* reference, bool reverse) {
  // get computation node
  TRI_ASSERT(node->type == aql::NODE_TYPE_OPERATOR_NARY_AND);
  TRI_ASSERT(node->numMembers() == 1);
  auto comp = node->getMember(0);

  // assume a.b == value
  auto attrNode = comp->getMember(0);
  auto valNode = comp->getMember(1);

  // got value == a.b  -> flip sides
  if (attrNode->type != aql::NODE_TYPE_ATTRIBUTE_ACCESS) {
    attrNode = comp->getMember(1);
    valNode = comp->getMember(0);
  }

  TRI_ASSERT(attrNode->type == aql::NODE_TYPE_ATTRIBUTE_ACCESS);
  TRI_ASSERT(attrNode->stringEquals(_directionAttr));

  if (comp->type == aql::NODE_TYPE_OPERATOR_BINARY_EQ) {
    // a.b == value
    return createEqIterator(trx, mmdr, attrNode, valNode);
  }

  if (comp->type == aql::NODE_TYPE_OPERATOR_BINARY_IN) {
    // a.b IN values
    if (!valNode->isArray()) {
      // a.b IN non-array
      return new EmptyIndexIterator(_collection, trx, mmdr, this);
    }
    return createInIterator(trx, mmdr, attrNode, valNode);
  }

  // operator type unsupported
  return new EmptyIndexIterator(_collection, trx, mmdr, this);
}

/// @brief specializes the condition for use with the index
arangodb::aql::AstNode* RocksDBEdgeIndex::specializeCondition(
    arangodb::aql::AstNode* node,
    arangodb::aql::Variable const* reference) const {
  // SimpleAttributeEqualityMatcher matcher(IndexAttributes);
  SimpleAttributeEqualityMatcher matcher(this->_fields);
  return matcher.specializeOne(this, node, reference);
}

/// @brief Transform the list of search slices to search values.
///        This will multiply all IN entries and simply return all other
///        entries.
void RocksDBEdgeIndex::expandInSearchValues(VPackSlice const slice,
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

// ===================== Helpers ==================

/// @brief create the iterator
IndexIterator* RocksDBEdgeIndex::createEqIterator(
    transaction::Methods* trx, ManagedDocumentResult* mmdr,
    arangodb::aql::AstNode const* attrNode,
    arangodb::aql::AstNode const* valNode) const {
  // lease builder, but immediately pass it to the unique_ptr so we don't leak
  transaction::BuilderLeaser builder(trx);
  std::unique_ptr<VPackBuilder> keys(builder.steal());
  keys->openArray();

  handleValNode(keys.get(), valNode);
  TRI_IF_FAILURE("EdgeIndex::noIterator") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  keys->close();

  return new RocksDBEdgeIndexIterator(_collection, trx, mmdr, this, keys,
                                      _cache.get());
}

/// @brief create the iterator
IndexIterator* RocksDBEdgeIndex::createInIterator(
    transaction::Methods* trx, ManagedDocumentResult* mmdr,
    arangodb::aql::AstNode const* attrNode,
    arangodb::aql::AstNode const* valNode) const {
  // lease builder, but immediately pass it to the unique_ptr so we don't leak
  transaction::BuilderLeaser builder(trx);
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

  return new RocksDBEdgeIndexIterator(_collection, trx, mmdr, this, keys,
                                      _cache.get());
}

/// @brief add a single value node to the iterator's keys
void RocksDBEdgeIndex::handleValNode(
    VPackBuilder* keys, arangodb::aql::AstNode const* valNode) const {
  if (!valNode->isStringValue() || valNode->getStringLength() == 0) {
    return;
  }

  keys->openObject();
  keys->add(StaticStrings::IndexEq,
            VPackValuePair(valNode->getStringValue(),
                           valNode->getStringLength(), VPackValueType::String));
  keys->close();

  TRI_IF_FAILURE("EdgeIndex::collectKeys") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
}

int RocksDBEdgeIndex::cleanup() {
  rocksdb::TransactionDB* db = rocksutils::globalRocksDB();
  rocksdb::CompactRangeOptions opts;
  RocksDBKeyBounds bounds = RocksDBKeyBounds::EdgeIndex(_objectId);
  rocksdb::Slice b = bounds.start(), e = bounds.end();
  db->CompactRange(opts, &b, &e);
  return TRI_ERROR_NO_ERROR;
}

void RocksDBEdgeIndex::serializeEstimate(std::string& output) const {
  TRI_ASSERT(_estimator != nullptr);
  _estimator->serialize(output);
}

bool RocksDBEdgeIndex::deserializeEstimate(RocksDBCounterManager* mgr) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  // We simply drop the current estimator and steal the one from recovery
  // We are than save for resizing issues in our _estimator format
  // and will use the old size.

  TRI_ASSERT(mgr != nullptr);
  auto tmp = mgr->stealIndexEstimator(_objectId);
  if (tmp == nullptr) {
    // We expected to receive a stored index estimate, however we got none.
    // We use the freshly created estimator but have to recompute it.
    return false;
  }
  _estimator.swap(tmp);
  TRI_ASSERT(_estimator != nullptr);
  return true;
}

void RocksDBEdgeIndex::recalculateEstimates() {
  TRI_ASSERT(_estimator != nullptr);
  _estimator->clear();

  auto bounds = RocksDBKeyBounds::EdgeIndex(_objectId);
  rocksutils::iterateBounds(bounds, [&](rocksdb::Iterator* it) {
    uint64_t hash = RocksDBEdgeIndex::HashForKey(it->key());
    _estimator->insert(hash);
  });
}

Result RocksDBEdgeIndex::postprocessRemove(transaction::Methods* trx,
                                           rocksdb::Slice const& key,
                                           rocksdb::Slice const& value) {
  // blacklist keys during truncate
  blackListKey(key.data(), key.size());

  uint64_t hash = RocksDBEdgeIndex::HashForKey(key);
  _estimator->remove(hash);
  return {TRI_ERROR_NO_ERROR};
}

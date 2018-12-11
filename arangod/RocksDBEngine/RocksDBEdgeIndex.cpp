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
/// @author Michael Hackstein
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
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "RocksDBEngine/RocksDBMethods.h"
#include "RocksDBEngine/RocksDBSettingsManager.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "RocksDBEngine/RocksDBTypes.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "Transaction/Context.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"

#include <rocksdb/db.h>
#include <rocksdb/utilities/transaction_db.h>
#include <rocksdb/utilities/write_batch_with_index.h>

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include <cmath>

using namespace arangodb;
using namespace arangodb::basics;

namespace {
constexpr bool EdgeIndexFillBlockCache = false;
}

RocksDBEdgeIndexWarmupTask::RocksDBEdgeIndexWarmupTask(
    std::shared_ptr<basics::LocalTaskQueue> const& queue,
    RocksDBEdgeIndex* index,
    transaction::Methods* trx,
    rocksdb::Slice const& lower,
    rocksdb::Slice const& upper)
  : LocalTask(queue),
    _index(index),
    _trx(trx),
    _lower(lower.data(), lower.size()),
    _upper(upper.data(), upper.size()) {}

void RocksDBEdgeIndexWarmupTask::run() {
  try {
    _index->warmupInternal(_trx, _lower, _upper);
  } catch (...) {
    _queue->setStatus(TRI_ERROR_INTERNAL);
  }
  _queue->join();
}

RocksDBEdgeIndexIterator::RocksDBEdgeIndexIterator(
    LogicalCollection* collection, transaction::Methods* trx,
    arangodb::RocksDBEdgeIndex const* index,
    std::unique_ptr<VPackBuilder> keys, std::shared_ptr<cache::Cache> cache)
    : IndexIterator(collection, trx),
      _keys(std::move(keys)),
      _keysIterator(_keys->slice()),
      _index(index),
      _bounds(RocksDBKeyBounds::EdgeIndex(0)),
      _cache(std::move(cache)),
      _builderIterator(arangodb::velocypack::Slice::emptyArraySlice()),
      _lastKey(VPackSlice::nullSlice()) {
  TRI_ASSERT(_keys != nullptr);
  TRI_ASSERT(_keys->slice().isArray());

  auto* mthds = RocksDBTransactionState::toMethods(trx);
  // intentional copy of the options
  rocksdb::ReadOptions options = mthds->iteratorReadOptions();
  options.fill_cache = EdgeIndexFillBlockCache;
  _iterator = mthds->NewIterator(options, index->columnFamily());
}

RocksDBEdgeIndexIterator::~RocksDBEdgeIndexIterator() {
  if (_keys != nullptr) {
    // return the VPackBuilder to the transaction context
    _trx->transactionContextPtr()->returnBuilder(_keys.release());
  }
}

void RocksDBEdgeIndexIterator::resetInplaceMemory() { _builder.clear(); }

void RocksDBEdgeIndexIterator::reset() {
  resetInplaceMemory();
  _keysIterator.reset();
  _lastKey = VPackSlice::nullSlice();
  _builderIterator =
      VPackArrayIterator(arangodb::velocypack::Slice::emptyArraySlice());
}

// returns true if we have one more key for the index lookup.
// if true, sets the `key` Slice to point to the new key's value
// note that the underlying data for the Slice must remain valid
// as long as the iterator is used and the key is not moved forward.
// returns false if there are no more keys to look for
bool RocksDBEdgeIndexIterator::initKey(VPackSlice& key) {
  if (!_keysIterator.valid()) {
    // no next key
    _lastKey = VPackSlice::nullSlice();
    return false;
  }

  key = _keysIterator.value();
  if (key.isObject()) {
    key = key.get(StaticStrings::IndexEq);
  }
  TRI_ASSERT(key.isString());
  _lastKey = key;
  return true;
}

bool RocksDBEdgeIndexIterator::next(LocalDocumentIdCallback const& cb, size_t limit) {
  TRI_ASSERT(_trx->state()->isRunning());
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  TRI_ASSERT(limit > 0);  // Someone called with limit == 0. Api broken
#else
  // Gracefully return in production code
  // Nothing bad has happened
  if (limit == 0) {
    return false;
  }
#endif

  while (limit > 0) {
    while (_builderIterator.valid()) {
      // We still have unreturned edges in out memory.
      // Just plainly return those.
      TRI_ASSERT(_builderIterator.value().isNumber());
      cb(LocalDocumentId{_builderIterator.value().getNumericValue<uint64_t>()});
      limit--;

      // Twice advance the iterator
      _builderIterator.next();
      // We always have <revision,_from> pairs
      TRI_ASSERT(_builderIterator.valid());
      _builderIterator.next();

      if (limit == 0) {
        // Limit reached bail out
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
    if (_cache) {
      for (size_t attempts = 0; attempts < 10; ++attempts) {
        // Try to read from cache
        auto finding = _cache->find(fromTo.data(), (uint32_t)fromTo.size());
        if (finding.found()) {
          needRocksLookup = false;
          // We got sth. in the cache
          VPackSlice cachedData(finding.value()->value());
          TRI_ASSERT(cachedData.isArray());
          if (cachedData.length() / 2 < limit) {
            // Directly return it, no need to copy
            _builderIterator = VPackArrayIterator(cachedData);
            while (_builderIterator.valid()) {
              TRI_ASSERT(_builderIterator.value().isNumber());
              cb(LocalDocumentId{
                  _builderIterator.value().getNumericValue<uint64_t>()});
              limit--;

              // Twice advance the iterator
              _builderIterator.next();
              // We always have <revision,_from> pairs
              TRI_ASSERT(_builderIterator.valid());
              _builderIterator.next();
            }
            _builderIterator = VPackArrayIterator(
                arangodb::velocypack::Slice::emptyArraySlice());
          } else {
            // We need to copy it.
            // And then we just get back to beginning of the loop
            _builder.clear();
            _builder.add(cachedData);
            TRI_ASSERT(_builder.slice().isArray());
            _builderIterator = VPackArrayIterator(_builder.slice());
            // Do not set limit
          }
          break;
        }
        if (finding.result().isNot(TRI_ERROR_LOCK_TIMEOUT)) {
          // We really have not found an entry.
          // Otherwise we do not know yet
          break;
        }
      }  // attempts
    }    // if (_cache)

    if (needRocksLookup) {
      lookupInRocksDB(fromTo);
    }

    _keysIterator.next();
  }
  TRI_ASSERT(limit == 0);
  return _builderIterator.valid() || _keysIterator.valid();
}

bool RocksDBEdgeIndexIterator::nextCovering(DocumentCallback const& cb, size_t limit) {
  TRI_ASSERT(_trx->state()->isRunning());
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  TRI_ASSERT(limit > 0);  // Someone called with limit == 0. Api broken
#else
  // Gracefully return in production code
  // Nothing bad has happened
  if (limit == 0) {
    return false;
  }
#endif

  while (limit > 0) {
    while (_builderIterator.valid()) {
      // We still have unreturned edges in memory.
      // Just plainly return those.
      TRI_ASSERT(_builderIterator.value().isNumber());
      cb(LocalDocumentId{_builderIterator.value().getNumericValue<uint64_t>()}, _lastKey);
      limit--;

      // Twice advance the iterator
      _builderIterator.next();
      // We always have <revision,_from> pairs
      TRI_ASSERT(_builderIterator.valid());
      _builderIterator.next();

      if (limit == 0) {
        // Limit reached. bail out
        return true;
      }
    }

    VPackSlice fromToSlice;
    if (!initKey(fromToSlice)) {
      return false;
    }

    StringRef fromTo(fromToSlice);

    bool needRocksLookup = true;
    if (_cache) {
      for (size_t attempts = 0; attempts < 10; ++attempts) {
        // Try to read from cache
        auto finding = _cache->find(fromTo.data(), (uint32_t)fromTo.size());
        if (finding.found()) {
          needRocksLookup = false;
          // We got sth. in the cache
          VPackSlice cachedData(finding.value()->value());
          TRI_ASSERT(cachedData.isArray());
          if (cachedData.length() / 2 < limit) {
            // Directly return it, no need to copy
            _builderIterator = VPackArrayIterator(cachedData);
            while (_builderIterator.valid()) {
              TRI_ASSERT(_builderIterator.value().isNumber());
              cb(LocalDocumentId{
                  _builderIterator.value().getNumericValue<uint64_t>()}, _lastKey);
              limit--;

              // Twice advance the iterator
              _builderIterator.next();
              // We always have <revision,_from> pairs
              TRI_ASSERT(_builderIterator.valid());
              _builderIterator.next();
            }
            _builderIterator = VPackArrayIterator(
                arangodb::velocypack::Slice::emptyArraySlice());
          } else {
            // We need to copy it.
            // And then we just get back to beginning of the loop
            _builder.clear();
            _builder.add(cachedData);
            TRI_ASSERT(_builder.slice().isArray());
            _builderIterator = VPackArrayIterator(_builder.slice());
            // Do not set limit
          }
          break;
        }
        if (finding.result().isNot(TRI_ERROR_LOCK_TIMEOUT)) {
          // We really have not found an entry.
          // Otherwise we do not know yet
          break;
        }
      }  // attempts
    }    // if (_cache)

    if (needRocksLookup) {
      lookupInRocksDB(fromTo);
    }

    _keysIterator.next();
  }
  TRI_ASSERT(limit == 0);
  return _builderIterator.valid() || _keysIterator.valid();
}

bool RocksDBEdgeIndexIterator::nextExtra(ExtraCallback const& cb,
                                         size_t limit) {
  TRI_ASSERT(_trx->state()->isRunning());
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  TRI_ASSERT(limit > 0);  // Someone called with limit == 0. Api broken
#else
  // Gracefully return in production code
  // Nothing bad has happened
  if (limit == 0) {
    return false;
  }
#endif

  while (limit > 0) {
    while (_builderIterator.valid()) {
      // We still have unreturned edges in out memory.
      // Just plainly return those.
      TRI_ASSERT(_builderIterator.value().isNumber());
      LocalDocumentId tkn{_builderIterator.value().getNumericValue<uint64_t>()};
      _builderIterator.next();
      TRI_ASSERT(_builderIterator.valid());
      // For now we store the complete opposite _from/_to value
      TRI_ASSERT(_builderIterator.value().isString());

      cb(tkn, _builderIterator.value());

      _builderIterator.next();
      limit--;

      if (limit == 0) {
        // Limit reached bail out
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
    if (_cache) {
      for (size_t attempts = 0; attempts < 10; ++attempts) {
        // Try to read from cache
        auto finding = _cache->find(fromTo.data(), (uint32_t)fromTo.size());
        if (finding.found()) {
          needRocksLookup = false;
          // We got sth. in the cache
          VPackSlice cachedData(finding.value()->value());
          TRI_ASSERT(cachedData.isArray());
          if (cachedData.length() / 2 < limit) {
            // Directly return it, no need to copy
            _builderIterator = VPackArrayIterator(cachedData);
            while (_builderIterator.valid()) {
              TRI_ASSERT(_builderIterator.value().isNumber());
              LocalDocumentId tkn{
                  _builderIterator.value().getNumericValue<uint64_t>()};

              _builderIterator.next();

              TRI_ASSERT(_builderIterator.valid());
              TRI_ASSERT(_builderIterator.value().isString());
              cb(tkn, _builderIterator.value());

              _builderIterator.next();
              limit--;
            }
            _builderIterator = VPackArrayIterator(
                arangodb::velocypack::Slice::emptyArraySlice());
          } else {
            // We need to copy it.
            // And then we just get back to beginning of the loop
            _builder.clear();
            _builder.add(cachedData);
            TRI_ASSERT(_builder.slice().isArray());
            _builderIterator = VPackArrayIterator(_builder.slice());
            // Do not set limit
          }
          break;
        }  // finding found
        if (finding.result().isNot(TRI_ERROR_LOCK_TIMEOUT)) {
          // We really have not found an entry.
          // Otherwise we do not know yet
          break;
        }
      }  // attempts
    }    // if (_cache)

    if (needRocksLookup) {
      lookupInRocksDB(fromTo);
    }

    _keysIterator.next();
  }
  TRI_ASSERT(limit == 0);
  return _builderIterator.valid() || _keysIterator.valid();
}

void RocksDBEdgeIndexIterator::lookupInRocksDB(StringRef fromTo) {
  // Bad case read from RocksDB
  _bounds = RocksDBKeyBounds::EdgeIndexVertex(_index->_objectId, fromTo);
  _iterator->Seek(_bounds.start());
  resetInplaceMemory();
  rocksdb::Comparator const* cmp = _index->comparator();

  cache::Cache* cc = _cache.get();
  _builder.openArray(true);
  auto end = _bounds.end();
  while (_iterator->Valid() && (cmp->Compare(_iterator->key(), end) < 0)) {
    LocalDocumentId const documentId = RocksDBKey::indexDocumentId(
        RocksDBEntryType::EdgeIndexValue, _iterator->key());

    // adding revision ID and _from or _to value
    _builder.add(VPackValue(documentId.id()));
    StringRef vertexId = RocksDBValue::vertexId(_iterator->value());
    _builder.add(VPackValuePair(vertexId.data(), vertexId.size(),
                                VPackValueType::String));

    _iterator->Next();
  }
  _builder.close();
  if (cc != nullptr) {
    // TODO Add cache retry on next call
    // Now we have something in _inplaceMemory.
    // It may be an empty array or a filled one, never mind, we cache both
    auto entry = cache::CachedValue::construct(
        fromTo.data(), static_cast<uint32_t>(fromTo.size()),
        _builder.slice().start(),
        static_cast<uint64_t>(_builder.slice().byteSize()));
    if (entry) {
      bool inserted = false;
      for (size_t attempts = 0; attempts < 10; attempts++) {
        auto status = cc->insert(entry);
        if (status.ok()) {
          inserted = true;
          break;
        }
        if (status.errorNumber() != TRI_ERROR_LOCK_TIMEOUT) {
          break;
        }
      }
      if (!inserted) {
        LOG_TOPIC(DEBUG, arangodb::Logger::CACHE) << "Failed to cache: "
                                                  << fromTo.toString();
        delete entry;
      }
    }
  }
  TRI_ASSERT(_builder.slice().isArray());
  _builderIterator = VPackArrayIterator(_builder.slice());
}

// ============================= Index ====================================

uint64_t RocksDBEdgeIndex::HashForKey(const rocksdb::Slice& key) {
  std::hash<StringRef> hasher;
  // NOTE: This function needs to use the same hashing on the
  // indexed VPack as the initial inserter does
  StringRef tmp = RocksDBKey::vertexId(key);
  return static_cast<uint64_t>(hasher(tmp));
}

RocksDBEdgeIndex::RocksDBEdgeIndex(
    TRI_idx_iid_t iid,
    arangodb::LogicalCollection& collection,
    arangodb::velocypack::Slice const& info,
    std::string const& attr
)
    : RocksDBIndex(iid, collection, std::vector<std::vector<AttributeName>>(
                                        {{AttributeName(attr, false)}}),
                   false, false, RocksDBColumnFamily::edge(),
                   basics::VelocyPackHelper::stringUInt64(info, "objectId"),
                   !ServerState::instance()->isCoordinator() /*useCache*/),
      _directionAttr(attr),
      _isFromIndex(attr == StaticStrings::FromString),
      _estimator(nullptr) {
  TRI_ASSERT(_cf == RocksDBColumnFamily::edge());

  if (!ServerState::instance()->isCoordinator()) {
    // We activate the estimator only on DBServers
    _estimator = std::make_unique<RocksDBCuckooIndexEstimator<uint64_t>>(
        RocksDBIndex::ESTIMATOR_SIZE);
    TRI_ASSERT(_estimator != nullptr);
  }
  // edge indexes are always created with ID 1 or 2
  TRI_ASSERT(iid == 1 || iid == 2);
  TRI_ASSERT(_objectId != 0);
}

RocksDBEdgeIndex::~RocksDBEdgeIndex() {}

/// @brief return a selectivity estimate for the index
double RocksDBEdgeIndex::selectivityEstimate(
    arangodb::StringRef const& attribute) const {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  if (_unique) {
    return 1.0;
  }
  if (!attribute.empty() && attribute.compare(_directionAttr)) {
    return 0.0;
  }
  TRI_ASSERT(_estimator != nullptr);
  return _estimator->computeEstimate();
}

/// @brief return a VelocyPack representation of the index
void RocksDBEdgeIndex::toVelocyPack(VPackBuilder& builder,
                                    std::underlying_type<Serialize>::type flags) const {
  builder.openObject();
  RocksDBIndex::toVelocyPack(builder, flags);
  builder.add(
    arangodb::StaticStrings::IndexUnique,
    arangodb::velocypack::Value(false)
  );
  builder.add(
    arangodb::StaticStrings::IndexSparse,
    arangodb::velocypack::Value(false)
  );
  builder.close();
}

Result RocksDBEdgeIndex::insertInternal(
    transaction::Methods& trx,
    RocksDBMethods* mthd,
    LocalDocumentId const& documentId,
    velocypack::Slice const& doc,
    Index::OperationMode mode
) {
  Result res;
  VPackSlice fromTo = doc.get(_directionAttr);
  TRI_ASSERT(fromTo.isString());
  auto fromToRef = StringRef(fromTo);
  RocksDBKeyLeaser key(&trx);

  key->constructEdgeIndexValue(_objectId, fromToRef, documentId);

  VPackSlice toFrom = _isFromIndex
                          ? transaction::helpers::extractToFromDocument(doc)
                          : transaction::helpers::extractFromFromDocument(doc);
  TRI_ASSERT(toFrom.isString());
  RocksDBValue value = RocksDBValue::EdgeIndexValue(StringRef(toFrom));

  // blacklist key in cache
  blackListKey(fromToRef);

  // acquire rocksdb transaction
  rocksdb::Status s = mthd->Put(_cf, key.ref(), value.string());

  if (s.ok()) {
    std::hash<StringRef> hasher;
    uint64_t hash = static_cast<uint64_t>(hasher(fromToRef));
    RocksDBTransactionState::toState(&trx)->trackIndexInsert(
      _collection.id(), id(), hash
    );
  } else {
    res.reset(rocksutils::convertStatus(s));
    addErrorMsg(res);
  }

  return res;
}

Result RocksDBEdgeIndex::removeInternal(
    transaction::Methods& trx,
    RocksDBMethods* mthd,
    LocalDocumentId const& documentId,
    velocypack::Slice const& doc,
    Index::OperationMode mode
) {
  Result res;

  // VPackSlice primaryKey = doc.get(StaticStrings::KeyString);
  VPackSlice fromTo = doc.get(_directionAttr);
  auto fromToRef = StringRef(fromTo);
  TRI_ASSERT(fromTo.isString());
  RocksDBKeyLeaser key(&trx);
  key->constructEdgeIndexValue(_objectId, fromToRef, documentId);
  VPackSlice toFrom = _isFromIndex
                          ? transaction::helpers::extractToFromDocument(doc)
                          : transaction::helpers::extractFromFromDocument(doc);
  TRI_ASSERT(toFrom.isString());
  RocksDBValue value = RocksDBValue::EdgeIndexValue(StringRef(toFrom));

  // blacklist key in cache
  blackListKey(fromToRef);

  rocksdb::Status s = mthd->Delete(_cf, key.ref());
  if (s.ok()) {
    std::hash<StringRef> hasher;
    uint64_t hash = static_cast<uint64_t>(hasher(fromToRef));
    RocksDBTransactionState::toState(&trx)->trackIndexRemove(
      _collection.id(), id(), hash
    );
  } else {
    res.reset(rocksutils::convertStatus(s));
    addErrorMsg(res);
  }
  
  return res;
}

void RocksDBEdgeIndex::batchInsert(
    transaction::Methods& trx,
    std::vector<std::pair<LocalDocumentId, VPackSlice>> const& documents,
    std::shared_ptr<arangodb::basics::LocalTaskQueue> queue) {
  auto* mthds = RocksDBTransactionState::toMethods(&trx);

  for (auto const& doc : documents) {
    VPackSlice fromTo = doc.second.get(_directionAttr);
    TRI_ASSERT(fromTo.isString());
    auto fromToRef = StringRef(fromTo);
    RocksDBKeyLeaser key(&trx);
    key->constructEdgeIndexValue(_objectId, fromToRef, doc.first);

    blackListKey(fromToRef);
    rocksdb::Status s = mthds->Put(_cf, key.ref(), rocksdb::Slice());
    if (!s.ok()) {
      queue->setStatus(rocksutils::convertStatus(s).errorNumber());
      break;
    }
  }
}

/// @brief checks whether the index supports the condition
bool RocksDBEdgeIndex::supportsFilterCondition(
    std::vector<std::shared_ptr<arangodb::Index>> const& allIndexes,
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, size_t itemsInIndex,
    size_t& estimatedItems, double& estimatedCost) const {
  SimpleAttributeEqualityMatcher matcher(this->_fields);
  return matcher.matchOne(this, node, reference, itemsInIndex, estimatedItems,
                          estimatedCost);
}

/// @brief creates an IndexIterator for the given Condition
IndexIterator* RocksDBEdgeIndex::iteratorForCondition(
    transaction::Methods* trx, ManagedDocumentResult*,
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference,
    IndexIteratorOptions const& opts) {
  TRI_ASSERT(!isSorted() || opts.sorted);
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
    return createEqIterator(trx, attrNode, valNode);
  }

  if (comp->type == aql::NODE_TYPE_OPERATOR_BINARY_IN) {
    // a.b IN values
    if (!valNode->isArray()) {
      // a.b IN non-array
      return new EmptyIndexIterator(&_collection, trx);
    }
    return createInIterator(trx, attrNode, valNode);
  }

  // operator type unsupported
  return new EmptyIndexIterator(&_collection, trx);
}

/// @brief specializes the condition for use with the index
arangodb::aql::AstNode* RocksDBEdgeIndex::specializeCondition(
    arangodb::aql::AstNode* node,
    arangodb::aql::Variable const* reference) const {
  // SimpleAttributeEqualityMatcher matcher(IndexAttributes);
  SimpleAttributeEqualityMatcher matcher(this->_fields);
  return matcher.specializeOne(this, node, reference);
}

static std::string FindMedian(rocksdb::Iterator* it,
                              std::string const& start,
                              std::string const& end) {

  // now that we do know the actual bounds calculate a
  // bad approximation for the index median key
  size_t min = std::min(start.size(), end.size());
  std::string median = std::string(min, '\0');
  for (size_t i = 0; i < min; i++) {
    median[i] = (start.data()[i] + end.data()[i]) / 2;
  }

  // now search the beginning of a new vertex ID
  it->Seek(median);
  if (!it->Valid()) {
    return end;
  }
  do {
    median = it->key().ToString();
    it->Next();
  } while (it->Valid() &&
           RocksDBKey::vertexId(it->key()) == RocksDBKey::vertexId(median));
  if (!it->Valid()) {
    return end;
  }
  return it->key().ToString();  // median is exclusive upper bound
}

void RocksDBEdgeIndex::warmup(transaction::Methods* trx,
                              std::shared_ptr<basics::LocalTaskQueue> queue) {
  if (!useCache()) {
    return;
  }

  // prepare transaction for parallel read access
  RocksDBTransactionState::toState(trx)->prepareForParallelReads();

  auto rocksColl = toRocksDBCollection(_collection);
  auto* mthds = RocksDBTransactionState::toMethods(trx);
  auto bounds = RocksDBKeyBounds::EdgeIndex(_objectId);

  uint64_t expectedCount = static_cast<uint64_t>(selectivityEstimate() *
                                                 rocksColl->numberDocuments());

  // Prepare the cache to be resized for this amount of objects to be inserted.
  _cache->sizeHint(expectedCount);
  if (expectedCount < 100000) {
    LOG_TOPIC(DEBUG, Logger::ENGINES) << "Skipping the multithreaded loading";
    auto task = std::make_shared<RocksDBEdgeIndexWarmupTask>(
        queue, this, trx, bounds.start(), bounds.end());
    queue->enqueue(task);
    return;
  }

  // try to find the right bounds
  rocksdb::ReadOptions ro = mthds->iteratorReadOptions();
  ro.prefix_same_as_start = false; // key-prefix includes edge (i.e. "collection/vertex")
  ro.total_order_seek = true; // otherwise full-index-scan does not work
  ro.verify_checksums = false;
  ro.fill_cache = EdgeIndexFillBlockCache;

  std::unique_ptr<rocksdb::Iterator> it(rocksutils::globalRocksDB()->NewIterator(ro, _cf));
  // get the first and last actual key
  it->Seek(bounds.start());
  if (!it->Valid()) {
    LOG_TOPIC(DEBUG, Logger::ENGINES) << "Cannot use multithreaded edge index warmup";
    auto task = std::make_shared<RocksDBEdgeIndexWarmupTask>(
        queue, this, trx, bounds.start(), bounds.end());
    queue->enqueue(task);
    return;
  }
  std::string firstKey = it->key().ToString();
  it->SeekForPrev(bounds.end());
  if (!it->Valid()) {
    LOG_TOPIC(DEBUG, Logger::ENGINES) << "Cannot use multithreaded edge index warmup";
    auto task = std::make_shared<RocksDBEdgeIndexWarmupTask>(
        queue, this, trx, bounds.start(), bounds.end());
    queue->enqueue(task);
    return;
  }
  std::string lastKey = it->key().ToString();

  std::string q1 = firstKey, q2, q3, q4, q5 = lastKey;
  q3 = FindMedian(it.get(), q1, q5);
  if (q3 == lastKey) {
    LOG_TOPIC(DEBUG, Logger::ENGINES) << "Cannot use multithreaded edge index warmup";
    auto task = std::make_shared<RocksDBEdgeIndexWarmupTask>(
        queue, this, trx, bounds.start(), bounds.end());
    queue->enqueue(task);
    return;
  }

  q2 = FindMedian(it.get(), q1, q3);
  q4 = FindMedian(it.get(), q3, q5);

  auto task1 = std::make_shared<RocksDBEdgeIndexWarmupTask>(
      queue, this, trx, q1, q2);
  queue->enqueue(task1);

  auto task2 = std::make_shared<RocksDBEdgeIndexWarmupTask>(
      queue, this, trx, q2, q3);
  queue->enqueue(task2);

  auto task3 = std::make_shared<RocksDBEdgeIndexWarmupTask>(
      queue, this, trx, q3, q4);
  queue->enqueue(task3);

  auto task4 = std::make_shared<RocksDBEdgeIndexWarmupTask>(
      queue, this, trx, q4, bounds.end());
  queue->enqueue(task4);
}

void RocksDBEdgeIndex::warmupInternal(transaction::Methods* trx,
                                      rocksdb::Slice const& lower,
                                      rocksdb::Slice const& upper) {
  auto scheduler = SchedulerFeature::SCHEDULER;
  auto rocksColl = toRocksDBCollection(_collection);
  bool needsInsert = false;
  std::string previous = "";
  VPackBuilder builder;

  // intentional copy of the read options
  auto* mthds = RocksDBTransactionState::toMethods(trx);
  rocksdb::Slice const end = upper;
  rocksdb::ReadOptions options = mthds->iteratorReadOptions();
  options.iterate_upper_bound = &end;  // safe to use on rocksb::DB directly
  options.prefix_same_as_start = false; // key-prefix includes edge
  options.total_order_seek = true; // otherwise full-index-scan does not work
  options.verify_checksums = false;
  options.fill_cache = EdgeIndexFillBlockCache;
  std::unique_ptr<rocksdb::Iterator> it(
      rocksutils::globalRocksDB()->NewIterator(options, _cf));

  size_t n = 0;
  cache::Cache* cc = _cache.get();
  for (it->Seek(lower); it->Valid(); it->Next()) {
    if (scheduler->isStopping()) {
      return;
    }
    n++;

    rocksdb::Slice key = it->key();
    StringRef v = RocksDBKey::vertexId(key);
    if (previous.empty()) {
      // First call.
      builder.clear();
      previous = v.toString();
      bool shouldTry = true;
      while (shouldTry) {
        auto finding = cc->find(previous.data(), (uint32_t)previous.size());
        if (finding.found()) {
          shouldTry = false;
          needsInsert = false;
        } else if (  // shouldTry if failed lookup was just a lock timeout
            finding.result().errorNumber() != TRI_ERROR_LOCK_TIMEOUT) {
          shouldTry = false;
          needsInsert = true;
          builder.openArray(true);
        }
      }
    }

    if (v != previous) {
      if (needsInsert) {
        // Switch to next vertex id.
        // Store what we have.
        builder.close();

        while (cc->isBusy()) {
          // We should wait here, the cache will reject
          // any inserts anyways.
          std::this_thread::sleep_for(std::chrono::microseconds(10000));
        }

        auto entry = cache::CachedValue::construct(
            previous.data(), static_cast<uint32_t>(previous.size()),
            builder.slice().start(),
            static_cast<uint64_t>(builder.slice().byteSize()));
        if (entry) {
          bool inserted = false;
          for (size_t attempts = 0; attempts < 10; attempts++) {
            auto status = cc->insert(entry);
            if (status.ok()) {
              inserted = true;
              break;
            }
            if (status.errorNumber() != TRI_ERROR_LOCK_TIMEOUT) {
              break;
            }
          }
          if (!inserted) {
            delete entry;
          }
        }
        builder.clear();
      }
      // Need to store
      previous = v.toString();
      auto finding = cc->find(previous.data(), (uint32_t)previous.size());
      if (finding.found()) {
        needsInsert = false;
      } else {
        needsInsert = true;
        builder.openArray(true);
      }
    }
    if (needsInsert) {
      LocalDocumentId const docId = RocksDBKey::indexDocumentId(RocksDBEntryType::EdgeIndexValue, key);
      if (!rocksColl->readDocumentWithCallback(trx, docId, [&](LocalDocumentId const&, VPackSlice doc) {
        builder.add(VPackValue(docId.id()));
        VPackSlice toFrom =
            _isFromIndex ? transaction::helpers::extractToFromDocument(doc)
                         : transaction::helpers::extractFromFromDocument(doc);
        TRI_ASSERT(toFrom.isString());
        builder.add(toFrom);
      })) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
        // Data Inconsistency.
        // We have a revision id without a document...
        TRI_ASSERT(false);
#endif
      }
    }
  }

  if (!previous.empty() && needsInsert) {
    // We still have something to store
    builder.close();

    auto entry = cache::CachedValue::construct(
        previous.data(), static_cast<uint32_t>(previous.size()),
        builder.slice().start(),
        static_cast<uint64_t>(builder.slice().byteSize()));
    if (entry) {
      bool inserted = false;
      for (size_t attempts = 0; attempts < 10; attempts++) {
        auto status = cc->insert(entry);
        if (status.ok()) {
          inserted = true;
          break;
        }
        if (status.errorNumber() != TRI_ERROR_LOCK_TIMEOUT) {
          break;
        }
      }
      if (!inserted) {
        delete entry;
      }
    }
  }
  LOG_TOPIC(DEBUG, Logger::ENGINES) << "loaded n: " << n ;
}

// ===================== Helpers ==================

/// @brief create the iterator
IndexIterator* RocksDBEdgeIndex::createEqIterator(
    transaction::Methods* trx,
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

  return new RocksDBEdgeIndexIterator(
    &_collection, trx, this, std::move(keys), _cache
  );
}

/// @brief create the iterator
IndexIterator* RocksDBEdgeIndex::createInIterator(
    transaction::Methods* trx,
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

  return new RocksDBEdgeIndexIterator(
    &_collection, trx, this, std::move(keys), _cache
  );
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

void RocksDBEdgeIndex::afterTruncate(TRI_voc_tick_t tick) {
  TRI_ASSERT(_estimator != nullptr);
  _estimator->bufferTruncate(tick);
  RocksDBIndex::afterTruncate(tick);
}

RocksDBCuckooIndexEstimator<uint64_t>* RocksDBEdgeIndex::estimator() {
  return _estimator.get();
}

void RocksDBEdgeIndex::setEstimator(std::unique_ptr<RocksDBCuckooIndexEstimator<uint64_t>> est) {
  _estimator = std::move(est);
}


void RocksDBEdgeIndex::recalculateEstimates() {
  TRI_ASSERT(_estimator != nullptr);
  _estimator->clear();
  
  rocksdb::TransactionDB* db = rocksutils::globalRocksDB();
  rocksdb::SequenceNumber seq = db->GetLatestSequenceNumber();
  
  auto bounds = RocksDBKeyBounds::EdgeIndex(_objectId);
  rocksdb::Slice const end = bounds.end();
  rocksdb::ReadOptions options;
  options.iterate_upper_bound = &end;  // safe to use on rocksb::DB directly
  options.prefix_same_as_start = false;  // key-prefix includes edge
  options.total_order_seek = true; // otherwise full scan fails
  options.verify_checksums = false;
  options.fill_cache = false;
  std::unique_ptr<rocksdb::Iterator> it(db->NewIterator(options, _cf));
  for (it->Seek(bounds.start()); it->Valid(); it->Next()) {
    uint64_t hash = RocksDBEdgeIndex::HashForKey(it->key());
    _estimator->insert(hash);
  }
  _estimator->setCommitSeq(seq);
}

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/AstNode.h"
#include "Aql/SortCondition.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/cpu-relax.h"
#include "Cache/BinaryKeyHasher.h"
#include "Cache/CachedValue.h"
#include "Cache/CacheManagerFeature.h"
#include "Cache/TransactionalCache.h"
#include "Indexes/SimpleAttributeEqualityMatcher.h"
#include "RestServer/DatabaseFeature.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBCuckooIndexEstimator.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "RocksDBEngine/RocksDBSettingsManager.h"
#include "RocksDBEngine/RocksDBTransactionMethods.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "RocksDBEngine/RocksDBTypes.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Context.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "Transaction/V8Context.h"
#include "Utils/CollectionGuard.h"
#include "Utils/DatabaseGuard.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"

#include <rocksdb/db.h>
#include <rocksdb/utilities/transaction_db.h>
#include <rocksdb/utilities/write_batch_with_index.h>

#include <velocypack/Iterator.h>

#include <cmath>

using namespace arangodb;
using namespace arangodb::basics;

namespace {
constexpr bool EdgeIndexFillBlockCache = false;

using EdgeIndexCacheType = cache::TransactionalCache<cache::BinaryKeyHasher>;
}  // namespace

RocksDBEdgeIndexWarmupTask::RocksDBEdgeIndexWarmupTask(
    DatabaseFeature& databaseFeature, std::string const& dbName,
    std::string const& collectionName, IndexId iid, rocksdb::Slice lower,
    rocksdb::Slice upper)
    : _databaseFeature(databaseFeature),
      _dbName(dbName),
      _collectionName(collectionName),
      _iid(iid),
      _lower(lower.data(), lower.size()),
      _upper(upper.data(), upper.size()) {}

Result RocksDBEdgeIndexWarmupTask::run() {
  DatabaseGuard databaseGuard(_databaseFeature, _dbName);
  CollectionGuard collectionGuard(&databaseGuard.database(), _collectionName);

  auto ctx = transaction::V8Context::CreateWhenRequired(
      databaseGuard.database(), false);
  SingleCollectionTransaction trx(ctx, *collectionGuard.collection(),
                                  AccessMode::Type::READ);
  Result res = trx.begin();

  if (res.fail()) {
    return res;
  }

  auto indexes = collectionGuard.collection()->getIndexes();
  for (auto const& idx : indexes) {
    if (idx->id() != _iid) {
      continue;
    }

    TRI_ASSERT(idx->type() == Index::TRI_IDX_TYPE_EDGE_INDEX);
    static_cast<RocksDBEdgeIndex*>(idx.get())->warmupInternal(&trx, _lower,
                                                              _upper);
  }

  return trx.commit();
}

namespace arangodb {
class RocksDBEdgeIndexLookupIterator final : public IndexIterator {
  // used for covering edge index lookups.
  // holds both the indexed attribute (_from/_to) and the opposite
  // attribute (_to/_from). Avoids copying the data and is thus
  // just an efficient, non-owning container for the _from/_to values
  // of a single edge during covering edge index lookups-
  class EdgeCoveringData final : public IndexIteratorCoveringData {
   public:
    explicit EdgeCoveringData(VPackSlice indexAttribute,
                              VPackSlice otherAttribute)
        : _indexAttribute(indexAttribute), _otherAttribute(otherAttribute) {}

    VPackSlice at(size_t i) const override {
      TRI_ASSERT(i <= 1);
      return i == 0 ? _indexAttribute : _otherAttribute;
    }

    VPackSlice value() const override {
      // we are assuming this API will never be called, as we are returning
      // isArray() -> true
      TRI_ASSERT(false);
      return VPackSlice::noneSlice();
    }

    bool isArray() const noexcept override { return true; }

    velocypack::ValueLength length() const override { return 2; }

   private:
    // the indexed attribute (_from/_to)
    VPackSlice _indexAttribute;
    // the opposite attribute (_to/_from)
    VPackSlice _otherAttribute;
  };

 public:
  RocksDBEdgeIndexLookupIterator(LogicalCollection* collection,
                                 transaction::Methods* trx,
                                 RocksDBEdgeIndex const* index,
                                 VPackBuilder&& keys,
                                 std::shared_ptr<cache::Cache> cache,
                                 ReadOwnWrites readOwnWrites)
      : IndexIterator(collection, trx, readOwnWrites),
        _index(index),
        _cache(std::static_pointer_cast<EdgeIndexCacheType>(std::move(cache))),
        _keys(std::move(keys)),
        _keysIterator(_keys.slice()),
        _bounds(RocksDBKeyBounds::EdgeIndex(0)),
        _builderIterator(VPackArrayIterator::Empty{}),
        _lastKey(VPackSlice::nullSlice()) {
    TRI_ASSERT(_keys.slice().isArray());

    TRI_ASSERT(_cache == nullptr || _cache->hasherName() == "BinaryKeyHasher");
  }

  std::string_view typeName() const noexcept final {
    return "edge-index-iterator";
  }

  // calls cb(documentId)
  bool nextImpl(LocalDocumentIdCallback const& cb, uint64_t limit) override {
    return nextImplementation(
        [&cb](LocalDocumentId docId, VPackSlice /*fromTo*/) { cb(docId); },
        limit);
  }

  // calls cb(documentId, [_from, _to]) or cb(documentId, [_to, _from])
  bool nextCoveringImpl(CoveringCallback const& cb, uint64_t limit) override {
    return nextImplementation(
        [&](LocalDocumentId docId, VPackSlice fromTo) {
          TRI_ASSERT(_lastKey.isString());
          TRI_ASSERT(fromTo.isString());
          auto data = EdgeCoveringData(_lastKey, fromTo);
          cb(docId, data);
        },
        limit);
  }

  void resetImpl() final {
    resetInplaceMemory();
    _keysIterator.reset();
    _lastKey = VPackSlice::nullSlice();
    _builderIterator = VPackArrayIterator(VPackArrayIterator::Empty{});
  }

  /// @brief index supports rearming
  bool canRearm() const override { return true; }

  /// @brief rearm the index iterator
  bool rearmImpl(aql::AstNode const* node, aql::Variable const* variable,
                 IndexIteratorOptions const& opts) override {
    TRI_ASSERT(!_index->isSorted() || opts.sorted);

    TRI_ASSERT(node != nullptr);
    TRI_ASSERT(node->type == aql::NODE_TYPE_OPERATOR_NARY_AND);
    TRI_ASSERT(node->numMembers() == 1);
    AttributeAccessParts aap(node->getMember(0), variable);

    TRI_ASSERT(aap.attribute->stringEquals(_index->_directionAttr));

    _keys.clear();
    TRI_ASSERT(_keys.isEmpty());

    if (aap.opType == aql::NODE_TYPE_OPERATOR_BINARY_EQ) {
      // a.b == value
      _index->fillLookupValue(_keys, aap.value);
      _keysIterator = VPackArrayIterator(_keys.slice());
      return true;
    }

    if (aap.opType == aql::NODE_TYPE_OPERATOR_BINARY_IN &&
        aap.value->isArray()) {
      // a.b IN values
      _index->fillInLookupValues(_trx, _keys, aap.value);
      _keysIterator = VPackArrayIterator(_keys.slice());
      return true;
    }

    // a.b IN non-array or operator type unsupported
    return false;
  }

 private:
  void resetInplaceMemory() { _builder.clear(); }

  /// internal retrieval loop
  template<typename F>
  inline bool nextImplementation(F&& cb, uint64_t limit) {
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
        LocalDocumentId docId{
            _builderIterator.value().getNumericValue<uint64_t>()};
        _builderIterator.next();
        TRI_ASSERT(_builderIterator.valid());
        // For now we store the complete opposite _from/_to value
        TRI_ASSERT(_builderIterator.value().isString());

        std::forward<F>(cb)(docId, _builderIterator.value());

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

      // We have exhausted local memory. Now fill it again:
      _lastKey = _keysIterator.value();
      TRI_ASSERT(_lastKey.isString());
      std::string_view fromTo = _lastKey.stringView();

      bool needRocksLookup = true;
      if (_cache) {
        // Try to read from cache
        auto finding =
            _cache->find(fromTo.data(), static_cast<uint32_t>(fromTo.size()));
        if (finding.found()) {
          incrCacheHits();
          needRocksLookup = false;
          // We got sth. in the cache
          VPackSlice cachedData(finding.value()->value());
          TRI_ASSERT(cachedData.isArray());
          if (cachedData.length() / 2 < limit) {
            // Directly return it, no need to copy
            _builderIterator = VPackArrayIterator(cachedData);
            while (_builderIterator.valid()) {
              TRI_ASSERT(_builderIterator.value().isNumber());
              LocalDocumentId docId{
                  _builderIterator.value().getNumericValue<uint64_t>()};

              _builderIterator.next();

              TRI_ASSERT(_builderIterator.valid());
              TRI_ASSERT(_builderIterator.value().isString());
              std::forward<F>(cb)(docId, _builderIterator.value());

              _builderIterator.next();
              limit--;
            }
            _builderIterator = VPackArrayIterator(VPackArrayIterator::Empty{});
          } else {
            // We need to copy it.
            // And then we just get back to beginning of the loop
            _builder.clear();
            _builder.add(cachedData);
            TRI_ASSERT(_builder.slice().isArray());
            _builderIterator = VPackArrayIterator(_builder.slice());
            // Do not set limit
          }
        } else {
          incrCacheMisses();
        }
      }  // if (_cache)

      if (needRocksLookup) {
        lookupInRocksDB(fromTo);
      }

      _keysIterator.next();
    }
    TRI_ASSERT(limit == 0);
    return _builderIterator.valid() || _keysIterator.valid();
  }

  void lookupInRocksDB(std::string_view fromTo) {
    // Bad (slow) case: read from RocksDB

    auto* mthds = RocksDBTransactionState::toMethods(_trx, _collection->id());

    // create iterator only on demand, so we save the allocation in case
    // the reads can be satisfied from the cache

    // unfortunately we *must* create a new RocksDB iterator here for each edge
    // lookup. the problem is that if we don't and reuse an existing RocksDB
    // iterator, it will not work properly with different prefixes. this will be
    // problematic if we do an edge lookup from an inner loop, e.g.
    //
    //   FOR doc IN ...
    //     FOR edge IN edgeCollection FILTER edge._to == doc._id
    //     ...
    //
    // in this setup, we do rearm the RocksDBEdgeIndexLookupIterator to look up
    // multiple times, with different _to values. However, if we reuse the same
    // RocksDB iterator for this, it may or may not find all the edges. Even
    // calling `Seek` using  a new bound does not fix this. It seems to have to
    // do with the Iterator preserving some state when there is a prefix
    // extractor in place.
    //
    // in order to safely return all existing edges, we need to recreate a new
    // RocksDB iterator every time we look for an edge. the performance hit is
    // mitigated by that edge lookups are normally using the in-memory edge
    // cache, so we only hit this method when connections are not yet in the
    // cache.
    std::unique_ptr<rocksdb::Iterator> iterator =
        mthds->NewIterator(_index->columnFamily(), [this](ReadOptions& ro) {
          ro.fill_cache = EdgeIndexFillBlockCache;
          ro.readOwnWrites = canReadOwnWrites() == ReadOwnWrites::yes;
        });

    TRI_ASSERT(iterator != nullptr);

    _bounds = RocksDBKeyBounds::EdgeIndexVertex(_index->objectId(), fromTo);
    rocksdb::Comparator const* cmp = _index->comparator();
    auto end = _bounds.end();

    resetInplaceMemory();
    _builder.openArray(true);
    for (iterator->Seek(_bounds.start());
         iterator->Valid() && (cmp->Compare(iterator->key(), end) < 0);
         iterator->Next()) {
      LocalDocumentId const docId = RocksDBKey::edgeDocumentId(iterator->key());

      // adding documentId and _from or _to value
      _builder.add(VPackValue(docId.id()));
      std::string_view vertexId = RocksDBValue::vertexId(iterator->value());
      _builder.add(VPackValue(vertexId));
    }
    _builder.close();

    // validate that Iterator is in a good shape and hasn't failed
    rocksutils::checkIteratorStatus(*iterator);

    if (_cache != nullptr) {
      // TODO Add cache retry on next call
      // Now we have something in _inplaceMemory.
      // It may be an empty array or a filled one, never mind, we cache both
      cache::Cache::SimpleInserter<EdgeIndexCacheType>{
          static_cast<EdgeIndexCacheType&>(*_cache), fromTo.data(),
          static_cast<uint32_t>(fromTo.size()), _builder.slice().start(),
          static_cast<uint64_t>(_builder.slice().byteSize())};
    }
    TRI_ASSERT(_builder.slice().isArray());
    _builderIterator = VPackArrayIterator(_builder.slice());
  }

  RocksDBEdgeIndex const* _index;

  std::shared_ptr<EdgeIndexCacheType> _cache;
  velocypack::Builder _keys;
  velocypack::ArrayIterator _keysIterator;

  RocksDBKeyBounds _bounds;

  // the following values are required for correct batch handling
  velocypack::Builder _builder;
  velocypack::ArrayIterator _builderIterator;
  velocypack::Slice _lastKey;
};

}  // namespace arangodb

// ============================= Index ====================================

uint64_t RocksDBEdgeIndex::HashForKey(rocksdb::Slice const& key) {
  std::hash<std::string_view> hasher{};
  // NOTE: This function needs to use the same hashing on the
  // indexed VPack as the initial inserter does
  std::string_view tmp = RocksDBKey::vertexId(key);
  // cppcheck-suppress uninitvar
  return static_cast<uint64_t>(hasher(tmp));
}

RocksDBEdgeIndex::RocksDBEdgeIndex(IndexId iid, LogicalCollection& collection,
                                   velocypack::Slice info,
                                   std::string const& attr)
    : RocksDBIndex(
          iid, collection,
          ((attr == StaticStrings::FromString)
               ? StaticStrings::IndexNameEdgeFrom
               : StaticStrings::IndexNameEdgeTo),
          std::vector<std::vector<AttributeName>>(
              {{AttributeName(attr, false)}}),
          /*unique*/ false, /*sparse*/ false,
          RocksDBColumnFamilyManager::get(
              RocksDBColumnFamilyManager::Family::EdgeIndex),
          basics::VelocyPackHelper::stringUInt64(info, StaticStrings::ObjectId),
          !ServerState::instance()->isCoordinator() &&
              collection.vocbase()
                  .server()
                  .getFeature<EngineSelectorFeature>()
                  .engine<RocksDBEngine>()
                  .useEdgeCache() /*useCache*/,
          /*cacheManager*/
          collection.vocbase()
              .server()
              .getFeature<CacheManagerFeature>()
              .manager(),
          /*engine*/
          collection.vocbase()
              .server()
              .getFeature<EngineSelectorFeature>()
              .engine<RocksDBEngine>()),
      _directionAttr(attr),
      _isFromIndex(attr == StaticStrings::FromString),
      _estimator(nullptr),
      _coveredFields({{AttributeName(attr, false)},
                      {AttributeName((_isFromIndex ? StaticStrings::ToString
                                                   : StaticStrings::FromString),
                                     false)}}) {
  TRI_ASSERT(_cf == RocksDBColumnFamilyManager::get(
                        RocksDBColumnFamilyManager::Family::EdgeIndex));

  if (!ServerState::instance()->isCoordinator() && !collection.isAStub()) {
    // We activate the estimator only on DBServers
    _estimator = std::make_unique<RocksDBCuckooIndexEstimatorType>(
        RocksDBIndex::ESTIMATOR_SIZE);
  }
  // edge indexes are always created with ID 1 or 2
  TRI_ASSERT(iid.isEdge());
  TRI_ASSERT(objectId() != 0);

  if (_cacheEnabled) {
    setupCache();
  }
}

RocksDBEdgeIndex::~RocksDBEdgeIndex() = default;

std::vector<std::vector<basics::AttributeName>> const&
RocksDBEdgeIndex::coveredFields() const {
  TRI_ASSERT(_coveredFields.size() == 2);  // _from/_to or _to/_from
  return _coveredFields;
}

/// @brief return a selectivity estimate for the index
double RocksDBEdgeIndex::selectivityEstimate(std::string_view attribute) const {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  if (_unique) {
    return 1.0;
  }
  if (!attribute.empty() && attribute != _directionAttr) {
    return 0.0;
  }
  if (_estimator == nullptr) {
    return 0.0;
  }
  TRI_ASSERT(_estimator != nullptr);
  return _estimator->computeEstimate();
}

/// @brief return a VelocyPack representation of the index
void RocksDBEdgeIndex::toVelocyPack(
    VPackBuilder& builder, std::underlying_type<Serialize>::type flags) const {
  builder.openObject();
  RocksDBIndex::toVelocyPack(builder, flags);
  builder.close();
}

Result RocksDBEdgeIndex::insert(transaction::Methods& trx, RocksDBMethods* mthd,
                                LocalDocumentId const& documentId,
                                velocypack::Slice doc,
                                OperationOptions const& /*options*/,
                                bool /*performChecks*/) {
  VPackSlice fromTo = doc.get(_directionAttr);
  TRI_ASSERT(fromTo.isString());
  std::string_view fromToRef = fromTo.stringView();

  RocksDBKeyLeaser key(&trx);
  key->constructEdgeIndexValue(objectId(), fromToRef, documentId);
  TRI_ASSERT(key->containsLocalDocumentId(documentId));

  VPackSlice toFrom = _isFromIndex
                          ? transaction::helpers::extractToFromDocument(doc)
                          : transaction::helpers::extractFromFromDocument(doc);
  TRI_ASSERT(toFrom.isString());
  RocksDBValue value = RocksDBValue::EdgeIndexValue(toFrom.stringView());

  // always invalidate cache entry for all edges with same _from / _to
  invalidateCacheEntry(fromToRef);

  Result res;
  rocksdb::Status s = mthd->PutUntracked(_cf, key.ref(), value.string());

  if (s.ok()) {
    std::hash<std::string_view> hasher;
    uint64_t hash = static_cast<uint64_t>(hasher(fromToRef));
    RocksDBTransactionState::toState(&trx)->trackIndexInsert(_collection.id(),
                                                             id(), hash);
  } else {
    res.reset(rocksutils::convertStatus(s));
    addErrorMsg(res);
  }

  return res;
}

Result RocksDBEdgeIndex::remove(transaction::Methods& trx, RocksDBMethods* mthd,
                                LocalDocumentId const& documentId,
                                velocypack::Slice doc) {
  VPackSlice fromTo = doc.get(_directionAttr);
  std::string_view fromToRef = fromTo.stringView();
  TRI_ASSERT(fromTo.isString());

  RocksDBKeyLeaser key(&trx);
  key->constructEdgeIndexValue(objectId(), fromToRef, documentId);

  VPackSlice toFrom = _isFromIndex
                          ? transaction::helpers::extractToFromDocument(doc)
                          : transaction::helpers::extractFromFromDocument(doc);
  TRI_ASSERT(toFrom.isString());
  RocksDBValue value = RocksDBValue::EdgeIndexValue(toFrom.stringView());

  // always invalidate cache entry for all edges with same _from / _to
  invalidateCacheEntry(fromToRef);

  Result res;
  rocksdb::Status s = mthd->Delete(_cf, key.ref());

  if (s.ok()) {
    std::hash<std::string_view> hasher;
    uint64_t hash = static_cast<uint64_t>(hasher(fromToRef));
    RocksDBTransactionState::toState(&trx)->trackIndexRemove(_collection.id(),
                                                             id(), hash);
  } else {
    res.reset(rocksutils::convertStatus(s));
    addErrorMsg(res);
  }

  return res;
}

/// @brief checks whether the index supports the condition
Index::FilterCosts RocksDBEdgeIndex::supportsFilterCondition(
    transaction::Methods& /*trx*/,
    std::vector<std::shared_ptr<Index>> const& /*allIndexes*/,
    aql::AstNode const* node, aql::Variable const* reference,
    size_t itemsInIndex) const {
  SimpleAttributeEqualityMatcher matcher(this->_fields);
  return matcher.matchOne(this, node, reference, itemsInIndex);
}

/// @brief creates an IndexIterator for the given Condition
std::unique_ptr<IndexIterator> RocksDBEdgeIndex::iteratorForCondition(
    transaction::Methods* trx, aql::AstNode const* node,
    aql::Variable const* reference, IndexIteratorOptions const& opts,
    ReadOwnWrites readOwnWrites, int) {
  TRI_ASSERT(!isSorted() || opts.sorted);

  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->type == aql::NODE_TYPE_OPERATOR_NARY_AND);
  TRI_ASSERT(node->numMembers() == 1);
  AttributeAccessParts aap(node->getMember(0), reference);

  TRI_ASSERT(aap.attribute->stringEquals(_directionAttr));

  TRI_ASSERT(aap.opType == aql::NODE_TYPE_OPERATOR_BINARY_EQ ||
             aap.opType == aql::NODE_TYPE_OPERATOR_BINARY_IN);

  if (aap.opType == aql::NODE_TYPE_OPERATOR_BINARY_EQ) {
    // a.b == value
    return createEqIterator(trx, aap.attribute, aap.value, opts.useCache,
                            readOwnWrites);
  }
  if (aap.opType == aql::NODE_TYPE_OPERATOR_BINARY_IN) {
    // "in"-checks never needs to observe own writes
    TRI_ASSERT(readOwnWrites == ReadOwnWrites::no);
    // a.b IN values
    if (aap.value->isArray()) {
      return createInIterator(trx, aap.attribute, aap.value, opts.useCache);
    }

    // a.b IN non-array
    // fallthrough to empty result
  }

  // operator type unsupported
  return std::make_unique<EmptyIndexIterator>(&_collection, trx);
}

/// @brief specializes the condition for use with the index
aql::AstNode* RocksDBEdgeIndex::specializeCondition(
    transaction::Methods& trx, aql::AstNode* node,
    aql::Variable const* reference) const {
  SimpleAttributeEqualityMatcher matcher(this->_fields);
  return matcher.specializeOne(this, node, reference);
}

static std::string FindMedian(rocksdb::Iterator* it, std::string const& start,
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

Result RocksDBEdgeIndex::scheduleWarmup() {
  if (!hasCache()) {
    return {};
  }

  auto ctx =
      transaction::V8Context::CreateWhenRequired(_collection.vocbase(), false);
  SingleCollectionTransaction trx(ctx, _collection, AccessMode::Type::READ);
  Result res = trx.begin();

  if (res.fail()) {
    return res;
  }

  auto rocksColl = toRocksDBCollection(_collection);
  auto* mthds = RocksDBTransactionState::toMethods(&trx, _collection.id());
  auto bounds = RocksDBKeyBounds::EdgeIndex(objectId());

  uint64_t expectedCount = rocksColl->meta().numberDocuments();
  expectedCount = static_cast<uint64_t>(expectedCount * selectivityEstimate());

  auto scheduleTask = [this](auto lower, auto upper) {
    auto& df = _collection.vocbase().server().getFeature<DatabaseFeature>();
    auto task = std::make_shared<RocksDBEdgeIndexWarmupTask>(
        df, _collection.vocbase().name(), _collection.name(), id(), lower,
        upper);
    SchedulerFeature::SCHEDULER->queue(
        RequestLane::INTERNAL_LOW, [task = std::move(task)]() {
          Result res = task->run();
          if (res.fail()) {
            LOG_TOPIC("0eb7d", WARN, Logger::ENGINES)
                << "unable to run index warmup: " << res.errorMessage();
          };
        });
  };

  // Prepare the cache to be resized for this amount of objects to be inserted.
  _cache->sizeHint(expectedCount);
  if (expectedCount < 100000) {
    LOG_TOPIC("ac653", DEBUG, Logger::ENGINES)
        << "Skipping the multithreaded loading";
    scheduleTask(bounds.start(), bounds.end());
    return {};
  }

  // try to find the right bounds
  rocksdb::ReadOptions ro = mthds->iteratorReadOptions();
  ro.prefix_same_as_start =
      false;  // key-prefix includes edge (i.e. "collection/vertex")
  ro.total_order_seek = true;  // otherwise full-index-scan does not work
  ro.verify_checksums = false;
  ro.fill_cache = EdgeIndexFillBlockCache;

  std::unique_ptr<rocksdb::Iterator> it(_engine.db()->NewIterator(ro, _cf));
  // get the first and last actual key
  it->Seek(bounds.start());
  if (!it->Valid()) {
    LOG_TOPIC("7b7dc", DEBUG, Logger::ENGINES)
        << "Cannot use multithreaded edge index warmup";
    scheduleTask(bounds.start(), bounds.end());
    return {};
  }
  std::string firstKey = it->key().ToString();
  it->SeekForPrev(bounds.end());
  if (!it->Valid()) {
    LOG_TOPIC("24334", DEBUG, Logger::ENGINES)
        << "Cannot use multithreaded edge index warmup";
    scheduleTask(bounds.start(), bounds.end());
    return {};
  }
  std::string lastKey = it->key().ToString();

  std::string q1 = firstKey, q2, q3, q4, q5 = lastKey;
  q3 = FindMedian(it.get(), q1, q5);
  if (q3 == lastKey) {
    LOG_TOPIC("14caa", DEBUG, Logger::ENGINES)
        << "Cannot use multithreaded edge index warmup";
    scheduleTask(bounds.start(), bounds.end());
    return {};
  }

  q2 = FindMedian(it.get(), q1, q3);
  q4 = FindMedian(it.get(), q3, q5);

  scheduleTask(q1, q2);
  scheduleTask(q2, q3);
  scheduleTask(q3, q4);
  scheduleTask(q4, bounds.end());

  return {};
}

void RocksDBEdgeIndex::warmupInternal(transaction::Methods* trx,
                                      rocksdb::Slice lower,
                                      rocksdb::Slice upper) {
  auto rocksColl = toRocksDBCollection(_collection);
  bool needsInsert = false;
  std::string previous;
  VPackBuilder builder;

  // intentional copy of the read options
  auto* mthds = RocksDBTransactionState::toMethods(trx, _collection.id());
  rocksdb::Slice const end = upper;
  rocksdb::ReadOptions options = mthds->iteratorReadOptions();
  options.iterate_upper_bound = &end;    // safe to use on rocksdb::DB directly
  options.prefix_same_as_start = false;  // key-prefix includes edge
  options.total_order_seek = true;  // otherwise full-index-scan does not work
  options.verify_checksums = false;
  options.fill_cache = EdgeIndexFillBlockCache;
  std::unique_ptr<rocksdb::Iterator> it(
      _engine.db()->NewIterator(options, _cf));

  ManagedDocumentResult mdr;

  size_t n = 0;
  cache::Cache* cc = _cache.get();
  for (it->Seek(lower); it->Valid(); it->Next()) {
    ++n;
    if (n % 1024 == 0) {
      if (collection().vocbase().server().isStopping()) {
        return;
      }
    }

    rocksdb::Slice key = it->key();
    std::string_view v = RocksDBKey::vertexId(key);
    if (previous.empty()) {
      // First call.
      builder.clear();
      previous = v;
      bool shouldTry = true;
      while (shouldTry) {
        auto finding = cc->find(previous.data(), (uint32_t)previous.size());
        if (finding.found()) {
          shouldTry = false;
          needsInsert = false;
        } else if (  // shouldTry if failed lookup was just a lock timeout
            finding.result() != TRI_ERROR_LOCK_TIMEOUT) {
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

        cache::Cache::SimpleInserter<EdgeIndexCacheType>{
            static_cast<EdgeIndexCacheType&>(*cc), previous.data(),
            static_cast<uint32_t>(previous.size()), builder.slice().start(),
            static_cast<uint64_t>(builder.slice().byteSize())};

        builder.clear();
      }
      // Need to store
      previous = v;
      auto finding = cc->find(previous.data(), (uint32_t)previous.size());
      if (finding.found()) {
        needsInsert = false;
      } else {
        needsInsert = true;
        builder.openArray(true);
      }
    }
    if (needsInsert) {
      LocalDocumentId const docId = RocksDBKey::edgeDocumentId(key);
      // warmup does not need to observe own writes
      if (!rocksColl->readDocument(trx, docId, mdr, ReadOwnWrites::no)) {
        // Data Inconsistency. revision id without a document...
        TRI_ASSERT(false);
        continue;
      }

      builder.add(VPackValue(docId.id()));
      VPackSlice doc(mdr.vpack());
      VPackSlice toFrom =
          _isFromIndex ? transaction::helpers::extractToFromDocument(doc)
                       : transaction::helpers::extractFromFromDocument(doc);
      TRI_ASSERT(toFrom.isString());
      builder.add(toFrom);
    }
  }

  if (!previous.empty() && needsInsert) {
    // We still have something to store
    builder.close();

    cache::Cache::SimpleInserter<EdgeIndexCacheType>{
        static_cast<EdgeIndexCacheType&>(*cc), previous.data(),
        static_cast<uint32_t>(previous.size()), builder.slice().start(),
        static_cast<uint64_t>(builder.slice().byteSize())};
  }
  LOG_TOPIC("99a29", DEBUG, Logger::ENGINES) << "loaded n: " << n;
}

// ===================== Helpers ==================

/// @brief create the iterator
std::unique_ptr<IndexIterator> RocksDBEdgeIndex::createEqIterator(
    transaction::Methods* trx, aql::AstNode const* /*attrNode*/,
    aql::AstNode const* valNode, bool useCache,
    ReadOwnWrites readOwnWrites) const {
  VPackBuilder keys;
  fillLookupValue(keys, valNode);
  return std::make_unique<RocksDBEdgeIndexLookupIterator>(
      &_collection, trx, this, std::move(keys), useCache ? _cache : nullptr,
      readOwnWrites);
}

/// @brief create the iterator
std::unique_ptr<IndexIterator> RocksDBEdgeIndex::createInIterator(
    transaction::Methods* trx, aql::AstNode const* /*attrNode*/,
    aql::AstNode const* valNode, bool useCache) const {
  VPackBuilder keys;
  fillInLookupValues(trx, keys, valNode);
  // "in"-checks never need to observe own writes.
  return std::make_unique<RocksDBEdgeIndexLookupIterator>(
      &_collection, trx, this, std::move(keys), useCache ? _cache : nullptr,
      ReadOwnWrites::no);
}

void RocksDBEdgeIndex::fillLookupValue(VPackBuilder& keys,
                                       aql::AstNode const* value) const {
  TRI_ASSERT(keys.isEmpty());
  keys.openArray(/*unindexed*/ true);
  handleValNode(&keys, value);
  TRI_IF_FAILURE("EdgeIndex::noIterator") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  keys.close();
}

void RocksDBEdgeIndex::fillInLookupValues(transaction::Methods* /*trx*/,
                                          VPackBuilder& keys,
                                          aql::AstNode const* values) const {
  TRI_ASSERT(values != nullptr);
  TRI_ASSERT(values->type == aql::NODE_TYPE_ARRAY);
  TRI_ASSERT(keys.isEmpty());

  keys.openArray(/*unindexed*/ true);
  size_t const n = values->numMembers();
  for (size_t i = 0; i < n; ++i) {
    handleValNode(&keys, values->getMemberUnchecked(i));
    TRI_IF_FAILURE("EdgeIndex::iteratorValNodes") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
  }

  TRI_IF_FAILURE("EdgeIndex::noIterator") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  keys.close();
}

/// @brief add a single value node to the iterator's keys
void RocksDBEdgeIndex::handleValNode(VPackBuilder* keys,
                                     aql::AstNode const* valNode) const {
  if (!valNode->isStringValue() || valNode->getStringLength() == 0) {
    return;
  }

  keys->add(VPackValuePair(valNode->getStringValue(),
                           valNode->getStringLength(), VPackValueType::String));

  TRI_IF_FAILURE("EdgeIndex::collectKeys") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
}

void RocksDBEdgeIndex::afterTruncate(TRI_voc_tick_t tick,
                                     transaction::Methods* trx) {
  TRI_ASSERT(!unique());
  if (_estimator != nullptr) {
    _estimator->bufferTruncate(tick);
  }
  RocksDBIndex::afterTruncate(tick, trx);
}

RocksDBCuckooIndexEstimatorType* RocksDBEdgeIndex::estimator() {
  return _estimator.get();
}

void RocksDBEdgeIndex::setEstimator(
    std::unique_ptr<RocksDBCuckooIndexEstimatorType> est) {
  TRI_ASSERT(_estimator == nullptr ||
             _estimator->appliedSeq() <= est->appliedSeq());
  _estimator = std::move(est);
}

void RocksDBEdgeIndex::recalculateEstimates() {
  if (_estimator == nullptr) {
    return;
  }
  TRI_ASSERT(_estimator != nullptr);
  _estimator->clear();

  rocksdb::TransactionDB* db = _engine.db();
  rocksdb::SequenceNumber seq = db->GetLatestSequenceNumber();

  auto bounds = RocksDBKeyBounds::EdgeIndex(objectId());
  rocksdb::Slice const end = bounds.end();
  rocksdb::ReadOptions options;
  options.iterate_upper_bound = &end;    // safe to use on rocksb::DB directly
  options.prefix_same_as_start = false;  // key-prefix includes edge
  options.total_order_seek = true;       // otherwise full scan fails
  options.verify_checksums = false;
  options.fill_cache = false;
  std::unique_ptr<rocksdb::Iterator> it(db->NewIterator(options, _cf));
  for (it->Seek(bounds.start()); it->Valid(); it->Next()) {
    TRI_ASSERT(it->key().compare(bounds.end()) < 1);
    uint64_t hash = RocksDBEdgeIndex::HashForKey(it->key());
    // cppcheck-suppress uninitvar ; doesn't understand above call
    _estimator->insert(hash);
  }
  _estimator->setAppliedSeq(seq);
}

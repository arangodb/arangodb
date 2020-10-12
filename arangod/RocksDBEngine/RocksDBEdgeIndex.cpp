////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
#include "Basics/LocalTaskQueue.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/cpu-relax.h"
#include "Cache/CachedValue.h"
#include "Cache/TransactionalCache.h"
#include "Indexes/SortedIndexAttributeMatcher.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "RocksDBEngine/RocksDBMethods.h"
#include "RocksDBEngine/RocksDBSettingsManager.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "RocksDBEngine/RocksDBTypes.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Context.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"

#include <rocksdb/db.h>
#include <rocksdb/utilities/transaction_db.h>
#include <rocksdb/utilities/write_batch_with_index.h>

#include <velocypack/Iterator.h>
#include <velocypack/StringRef.h>
#include <velocypack/velocypack-aliases.h>

#include <cmath>

using namespace arangodb;
using namespace arangodb::basics;

namespace {
constexpr bool EdgeIndexFillBlockCache = false;
}

RocksDBEdgeIndexWarmupTask::RocksDBEdgeIndexWarmupTask(
    std::shared_ptr<basics::LocalTaskQueue> const& queue, RocksDBEdgeIndex* index,
    transaction::Methods* trx, rocksdb::Slice const& lower, rocksdb::Slice const& upper)
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

namespace arangodb {
class RocksDBEdgeIndexLookupIterator final : public IndexIterator {
 public:
  RocksDBEdgeIndexLookupIterator(LogicalCollection* collection, transaction::Methods* trx,
                                 arangodb::RocksDBEdgeIndex const* index,
                                 std::unique_ptr<VPackBuilder> keys,
                                 std::shared_ptr<cache::Cache> cache)
      : IndexIterator(collection, trx),
        _index(index),
        _cache(std::move(cache)),
        _keys(std::move(keys)),
        _keysIterator(_keys->slice()),
        _bounds(RocksDBKeyBounds::EdgeIndex(0)),
        _builderIterator(VPackArrayIterator::Empty{}),
        _lastKey(VPackSlice::nullSlice()) {
    TRI_ASSERT(_keys != nullptr);
    TRI_ASSERT(_keys->slice().isArray());
  }

  ~RocksDBEdgeIndexLookupIterator() {
    if (_keys != nullptr) {
      // return the VPackBuilder to the transaction context
      _trx->transactionContextPtr()->returnBuilder(_keys.release());
    }
  }

  char const* typeName() const override { return "edge-index-iterator"; }

  bool hasExtra() const override { return true; }

  /// @brief we provide a method to provide the index attribute values
  /// while scanning the index
  bool hasCovering() const override { return true; }

  // calls cb(documentId)
  bool nextImpl(LocalDocumentIdCallback const& cb, size_t limit) override {
    return nextImplementation([&cb](LocalDocumentId docId,
                                    VPackSlice /*fromTo*/) {
      cb(docId);
    }, limit);
  }

  // calls cb(documentId, [_from, _to]) or cb(documentId, [_to, _from])
  bool nextCoveringImpl(DocumentCallback const& cb, size_t limit) override {
    transaction::BuilderLeaser coveringBuilder(_trx);
    return nextImplementation([&](LocalDocumentId docId,
                                  VPackSlice fromTo) {
      TRI_ASSERT(_lastKey.isString());
      TRI_ASSERT(fromTo.isString());
      coveringBuilder->clear();
      coveringBuilder->openArray(/*unindexed*/true);
      coveringBuilder->add(_lastKey);
      coveringBuilder->add(fromTo);
      coveringBuilder->close();
      cb(docId, coveringBuilder->slice());
    }, limit);
  }

  // calls cb(documentId, _from) or (documentId, _to)
  bool nextExtraImpl(ExtraCallback const& cb, size_t limit) override {
    return nextImplementation([&cb](LocalDocumentId docId,
                                    VPackSlice fromTo) {
      cb(docId, fromTo);
    }, limit);
  }

  void resetImpl() override {
    resetInplaceMemory();
    _keysIterator.reset();
    _lastKey = VPackSlice::nullSlice();
    _builderIterator = VPackArrayIterator(VPackArrayIterator::Empty{});
  }

  /// @brief index supports rearming
  bool canRearm() const override { return true; }

  /// @brief rearm the index iterator
  bool rearmImpl(arangodb::aql::AstNode const* node, arangodb::aql::Variable const* variable,
                 IndexIteratorOptions const& opts) override {
    TRI_ASSERT(!_index->isSorted() || opts.sorted);

    TRI_ASSERT(node != nullptr);
    TRI_ASSERT(node->type == aql::NODE_TYPE_OPERATOR_NARY_AND);
    TRI_ASSERT(node->numMembers() == 1);
    AttributeAccessParts aap(node->getMember(0), variable);

    TRI_ASSERT(aap.attribute->stringEquals(_index->_directionAttr));

    _keys->clear();
    TRI_ASSERT(_keys->isEmpty());

    if (aap.opType == aql::NODE_TYPE_OPERATOR_BINARY_EQ) {
      // a.b == value
      _index->fillLookupValue(*_keys, aap.value);
      _keysIterator = VPackArrayIterator(_keys->slice());
      reset();
      return true;
    }

    if (aap.opType == aql::NODE_TYPE_OPERATOR_BINARY_IN && aap.value->isArray()) {
      // a.b IN values
      _index->fillInLookupValues(_trx, *_keys, aap.value);
      _keysIterator = VPackArrayIterator(_keys->slice());
      reset();
      return true;
    }

    // a.b IN non-array or operator type unsupported
    return false;
  }

 private:

  void resetInplaceMemory() { _builder.clear(); }
  
  /// internal retrieval loop
  template <typename F>
  inline bool nextImplementation(F&& cb, size_t limit) {
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
        LocalDocumentId docId{_builderIterator.value().getNumericValue<uint64_t>()};
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
      arangodb::velocypack::StringRef fromTo(_lastKey);

      bool needRocksLookup = true;
      if (_cache) {
        for (size_t attempts = 0; attempts < 10; ++attempts) {
          // Try to read from cache
          auto finding = _cache->find(fromTo.data(), static_cast<uint32_t>(fromTo.size()));
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
                LocalDocumentId docId{_builderIterator.value().getNumericValue<uint64_t>()};

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
            break;
          }  // finding found
          if (finding.result().isNot(TRI_ERROR_LOCK_TIMEOUT)) {
            // We really have not found an entry.
            // Otherwise we do not know yet
            break;
          }
          cpu_relax();
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

  void lookupInRocksDB(VPackStringRef fromTo) {
    // Bad (slow) case: read from RocksDB
    
    auto* mthds = RocksDBTransactionState::toMethods(_trx);
    // intentional copy of the options
    rocksdb::ReadOptions ro = mthds->iteratorReadOptions();
    ro.fill_cache = EdgeIndexFillBlockCache;
    
    // create iterator only on demand, so we save the allocation in case
    // the reads can be satisfied from the cache
    
    // unfortunately we *must* create a new RocksDB iterator here for each edge lookup.
    // the problem is that if we don't and reuse an existing RocksDB iterator, it will not
    // work properly with different prefixes.
    // this will be problematic if we do an edge lookup from an inner loop, e.g.
    //
    //   FOR doc IN ...
    //     FOR edge IN edgeCollection FILTER edge._to == doc._id 
    //     ...
    //
    // in this setup, we do rearm the RocksDBEdgeIndexLookupIterator to look up multiple
    // times, with different _to values. However, if we reuse the same RocksDB iterator
    // for this, it may or may not find all the edges. Even calling `Seek` using  a new
    // bound does not fix this. It seems to have to do with the Iterator preserving some
    // state when there is a prefix extractor in place.
    //
    // in order to safely return all existing edges, we need to recreate a new RocksDB
    // iterator every time we look for an edge. the performance hit is mitigated by that
    // edge lookups are normally using the in-memory edge cache, so we only hit this
    // method when connections are not yet in the cache.
    std::unique_ptr<rocksdb::Iterator> iterator = mthds->NewIterator(ro, _index->columnFamily());

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
      VPackStringRef vertexId = RocksDBValue::vertexId(iterator->value());
      _builder.add(VPackValuePair(vertexId.data(), vertexId.size(), VPackValueType::String));
    }
    _builder.close();

    // validate that Iterator is in a good shape and hasn't failed
    arangodb::rocksutils::checkIteratorStatus(iterator.get());

    cache::Cache* cc = _cache.get();
    if (cc != nullptr) {
      // TODO Add cache retry on next call
      // Now we have something in _inplaceMemory.
      // It may be an empty array or a filled one, never mind, we cache both
      auto entry =
          cache::CachedValue::construct(fromTo.data(),
                                        static_cast<uint32_t>(fromTo.size()),
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
          cpu_relax();
        }
        if (!inserted) {
          LOG_TOPIC("c1809", DEBUG, arangodb::Logger::CACHE)
              << "Failed to cache: " << fromTo.toString();
          delete entry;
        }
      }
    }
    TRI_ASSERT(_builder.slice().isArray());
    _builderIterator = VPackArrayIterator(_builder.slice());
  }

  RocksDBEdgeIndex const* _index;

  std::shared_ptr<cache::Cache> _cache;
  std::unique_ptr<arangodb::velocypack::Builder> _keys;
  arangodb::velocypack::ArrayIterator _keysIterator;

  RocksDBKeyBounds _bounds;

  // the following values are required for correct batch handling
  arangodb::velocypack::Builder _builder;
  arangodb::velocypack::ArrayIterator _builderIterator;
  arangodb::velocypack::Slice _lastKey;
};

}  // namespace arangodb

// ============================= Index ====================================

uint64_t RocksDBEdgeIndex::HashForKey(const rocksdb::Slice& key) {
  std::hash<arangodb::velocypack::StringRef> hasher{};
  // NOTE: This function needs to use the same hashing on the
  // indexed VPack as the initial inserter does
  arangodb::velocypack::StringRef tmp = RocksDBKey::vertexId(key);
  // cppcheck-suppress uninitvar
  return static_cast<uint64_t>(hasher(tmp));
}

RocksDBEdgeIndex::RocksDBEdgeIndex(IndexId iid, arangodb::LogicalCollection& collection,
                                   arangodb::velocypack::Slice const& info,
                                   std::string const& attr)
    : RocksDBIndex(iid, collection,
                   ((attr == StaticStrings::FromString) ? StaticStrings::IndexNameEdgeFrom
                                                        : StaticStrings::IndexNameEdgeTo),
                   std::vector<std::vector<AttributeName>>({{AttributeName(attr, false)}}),
                   false, false, RocksDBColumnFamily::edge(),
                   basics::VelocyPackHelper::stringUInt64(info, StaticStrings::ObjectId),
                   !ServerState::instance()->isCoordinator() &&
                       collection.vocbase()
                           .server()
                           .getFeature<EngineSelectorFeature>()
                           .engine<RocksDBEngine>()
                           .useEdgeCache() /*useCache*/),
      _directionAttr(attr),
      _isFromIndex(attr == StaticStrings::FromString),
      _estimator(nullptr),
      _coveredFields({{AttributeName(attr, false)},
                      {AttributeName((_isFromIndex ? StaticStrings::ToString : StaticStrings::FromString),
                                     false)}}) {
  TRI_ASSERT(_cf == RocksDBColumnFamily::edge());

  if (!ServerState::instance()->isCoordinator()) {
    // We activate the estimator only on DBServers
    _estimator = std::make_unique<RocksDBCuckooIndexEstimator<uint64_t>>(
        RocksDBIndex::ESTIMATOR_SIZE);
    TRI_ASSERT(_estimator != nullptr);
  }
  // edge indexes are always created with ID 1 or 2
  TRI_ASSERT(iid.isEdge());
  TRI_ASSERT(objectId() != 0);
}

RocksDBEdgeIndex::~RocksDBEdgeIndex() = default;

std::vector<std::vector<arangodb::basics::AttributeName>> const& RocksDBEdgeIndex::coveredFields() const {
  TRI_ASSERT(_coveredFields.size() == 2);  // _from/_to or _to/_from
  return _coveredFields;
}

/// @brief return a selectivity estimate for the index
double RocksDBEdgeIndex::selectivityEstimate(arangodb::velocypack::StringRef const& attribute) const {
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
  builder.close();
}

Result RocksDBEdgeIndex::insert(transaction::Methods& trx, RocksDBMethods* mthd,
                                LocalDocumentId const& documentId,
                                velocypack::Slice const doc, OperationOptions& options) {
  Result res;
  VPackSlice fromTo = doc.get(_directionAttr);
  TRI_ASSERT(fromTo.isString());
  auto fromToRef = arangodb::velocypack::StringRef(fromTo);
  RocksDBKeyLeaser key(&trx);

  key->constructEdgeIndexValue(objectId(), fromToRef, documentId);
  TRI_ASSERT(key->containsLocalDocumentId(documentId));

  VPackSlice toFrom = _isFromIndex
                          ? transaction::helpers::extractToFromDocument(doc)
                          : transaction::helpers::extractFromFromDocument(doc);
  TRI_ASSERT(toFrom.isString());
  RocksDBValue value = RocksDBValue::EdgeIndexValue(VPackStringRef(toFrom));

  // always invalidate cache entry for all edges with same _from / _to
  invalidateCacheEntry(fromToRef);

  // acquire rocksdb transaction
  rocksdb::Status s = mthd->PutUntracked(_cf, key.ref(), value.string());

  if (s.ok()) {
    std::hash<arangodb::velocypack::StringRef> hasher;
    uint64_t hash = static_cast<uint64_t>(hasher(fromToRef));
    RocksDBTransactionState::toState(&trx)->trackIndexInsert(_collection.id(), id(), hash);
  } else {
    res.reset(rocksutils::convertStatus(s));
    addErrorMsg(res);
  }

  return res;
}

Result RocksDBEdgeIndex::remove(transaction::Methods& trx, RocksDBMethods* mthd,
                                LocalDocumentId const& documentId,
                                velocypack::Slice const doc) {
  Result res;

  // VPackSlice primaryKey = doc.get(StaticStrings::KeyString);
  VPackSlice fromTo = doc.get(_directionAttr);
  auto fromToRef = arangodb::velocypack::StringRef(fromTo);
  TRI_ASSERT(fromTo.isString());
  RocksDBKeyLeaser key(&trx);
  key->constructEdgeIndexValue(objectId(), fromToRef, documentId);
  VPackSlice toFrom = _isFromIndex
                          ? transaction::helpers::extractToFromDocument(doc)
                          : transaction::helpers::extractFromFromDocument(doc);
  TRI_ASSERT(toFrom.isString());
  RocksDBValue value =
      RocksDBValue::EdgeIndexValue(arangodb::velocypack::StringRef(toFrom));

  // always invalidate cache entry for all edges with same _from / _to
  invalidateCacheEntry(fromToRef);

  rocksdb::Status s = mthd->Delete(_cf, key.ref());
  if (s.ok()) {
    std::hash<arangodb::velocypack::StringRef> hasher;
    uint64_t hash = static_cast<uint64_t>(hasher(fromToRef));
    RocksDBTransactionState::toState(&trx)->trackIndexRemove(_collection.id(), id(), hash);
  } else {
    res.reset(rocksutils::convertStatus(s));
    addErrorMsg(res);
  }

  return res;
}

/// @brief checks whether the index supports the condition
Index::FilterCosts RocksDBEdgeIndex::supportsFilterCondition(
    std::vector<std::shared_ptr<arangodb::Index>> const& allIndexes,
    arangodb::aql::AstNode const* node, arangodb::aql::Variable const* reference,
    size_t itemsInIndex) const {
  return SortedIndexAttributeMatcher::supportsFilterCondition(allIndexes, this, node, reference, itemsInIndex);
}

/// @brief creates an IndexIterator for the given Condition
std::unique_ptr<IndexIterator> RocksDBEdgeIndex::iteratorForCondition(
    transaction::Methods* trx, arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, IndexIteratorOptions const& opts) {
  TRI_ASSERT(!isSorted() || opts.sorted);

  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->type == aql::NODE_TYPE_OPERATOR_NARY_AND);
  TRI_ASSERT(node->numMembers() == 1);
  AttributeAccessParts aap(node->getMember(0), reference);

  TRI_ASSERT(aap.attribute->stringEquals(_directionAttr));

  if (aap.opType == aql::NODE_TYPE_OPERATOR_BINARY_EQ) {
    // a.b == value
    return createEqIterator(trx, aap.attribute, aap.value);
  } else if (aap.opType == aql::NODE_TYPE_OPERATOR_BINARY_IN) {
    // a.b IN values
    if (aap.value->isArray()) {
      return createInIterator(trx, aap.attribute, aap.value);
    }

    // a.b IN non-array
    // fallthrough to empty result
  }

  // operator type unsupported
  return std::make_unique<EmptyIndexIterator>(&_collection, trx);
}

/// @brief specializes the condition for use with the index
arangodb::aql::AstNode* RocksDBEdgeIndex::specializeCondition(
    arangodb::aql::AstNode* node, arangodb::aql::Variable const* reference) const {
  return SortedIndexAttributeMatcher::specializeCondition(this, node, reference);
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
  } while (it->Valid() && RocksDBKey::vertexId(it->key()) == RocksDBKey::vertexId(median));
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
  auto bounds = RocksDBKeyBounds::EdgeIndex(objectId());

  uint64_t expectedCount = rocksColl->meta().numberDocuments();
  expectedCount = static_cast<uint64_t>(expectedCount * selectivityEstimate());

  // Prepare the cache to be resized for this amount of objects to be inserted.
  _cache->sizeHint(expectedCount);
  if (expectedCount < 100000) {
    LOG_TOPIC("ac653", DEBUG, Logger::ENGINES) << "Skipping the multithreaded loading";
    auto task = std::make_shared<RocksDBEdgeIndexWarmupTask>(queue, this, trx,
                                                             bounds.start(),
                                                             bounds.end());
    queue->enqueue(task);
    return;
  }

  // try to find the right bounds
  rocksdb::ReadOptions ro = mthds->iteratorReadOptions();
  ro.prefix_same_as_start = false;  // key-prefix includes edge (i.e. "collection/vertex")
  ro.total_order_seek = true;  // otherwise full-index-scan does not work
  ro.verify_checksums = false;
  ro.fill_cache = EdgeIndexFillBlockCache;

  auto& selector = _collection.vocbase().server().getFeature<EngineSelectorFeature>();
  auto& engine = selector.engine<RocksDBEngine>();
  std::unique_ptr<rocksdb::Iterator> it(engine.db()->NewIterator(ro, _cf));
  // get the first and last actual key
  it->Seek(bounds.start());
  if (!it->Valid()) {
    LOG_TOPIC("7b7dc", DEBUG, Logger::ENGINES)
        << "Cannot use multithreaded edge index warmup";
    auto task = std::make_shared<RocksDBEdgeIndexWarmupTask>(queue, this, trx,
                                                             bounds.start(),
                                                             bounds.end());
    queue->enqueue(task);
    return;
  }
  std::string firstKey = it->key().ToString();
  it->SeekForPrev(bounds.end());
  if (!it->Valid()) {
    LOG_TOPIC("24334", DEBUG, Logger::ENGINES)
        << "Cannot use multithreaded edge index warmup";
    auto task = std::make_shared<RocksDBEdgeIndexWarmupTask>(queue, this, trx,
                                                             bounds.start(),
                                                             bounds.end());
    queue->enqueue(task);
    return;
  }
  std::string lastKey = it->key().ToString();

  std::string q1 = firstKey, q2, q3, q4, q5 = lastKey;
  q3 = FindMedian(it.get(), q1, q5);
  if (q3 == lastKey) {
    LOG_TOPIC("14caa", DEBUG, Logger::ENGINES)
        << "Cannot use multithreaded edge index warmup";
    auto task = std::make_shared<RocksDBEdgeIndexWarmupTask>(queue, this, trx,
                                                             bounds.start(),
                                                             bounds.end());
    queue->enqueue(task);
    return;
  }

  q2 = FindMedian(it.get(), q1, q3);
  q4 = FindMedian(it.get(), q3, q5);

  auto task1 = std::make_shared<RocksDBEdgeIndexWarmupTask>(queue, this, trx, q1, q2);
  queue->enqueue(task1);

  auto task2 = std::make_shared<RocksDBEdgeIndexWarmupTask>(queue, this, trx, q2, q3);
  queue->enqueue(task2);

  auto task3 = std::make_shared<RocksDBEdgeIndexWarmupTask>(queue, this, trx, q3, q4);
  queue->enqueue(task3);

  auto task4 = std::make_shared<RocksDBEdgeIndexWarmupTask>(queue, this, trx,
                                                            q4, bounds.end());
  queue->enqueue(task4);
}

void RocksDBEdgeIndex::warmupInternal(transaction::Methods* trx, rocksdb::Slice const& lower,
                                      rocksdb::Slice const& upper) {
  auto rocksColl = toRocksDBCollection(_collection);
  bool needsInsert = false;
  std::string previous = "";
  VPackBuilder builder;

  // intentional copy of the read options
  auto* mthds = RocksDBTransactionState::toMethods(trx);
  rocksdb::Slice const end = upper;
  rocksdb::ReadOptions options = mthds->iteratorReadOptions();
  options.iterate_upper_bound = &end;    // safe to use on rocksb::DB directly
  options.prefix_same_as_start = false;  // key-prefix includes edge
  options.total_order_seek = true;  // otherwise full-index-scan does not work
  options.verify_checksums = false;
  options.fill_cache = EdgeIndexFillBlockCache;
  auto& selector = _collection.vocbase().server().getFeature<EngineSelectorFeature>();
  auto& engine = selector.engine<RocksDBEngine>();
  std::unique_ptr<rocksdb::Iterator> it(engine.db()->NewIterator(options, _cf));

  ManagedDocumentResult mdr;

  size_t n = 0;
  cache::Cache* cc = _cache.get();
  for (it->Seek(lower); it->Valid(); it->Next()) {
    if (collection().vocbase().server().isStopping()) {
      return;
    }
    n++;

    rocksdb::Slice key = it->key();
    arangodb::velocypack::StringRef v = RocksDBKey::vertexId(key);
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
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        auto entry =
            cache::CachedValue::construct(previous.data(),
                                          static_cast<uint32_t>(previous.size()),
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
      LocalDocumentId const docId = RocksDBKey::edgeDocumentId(key);
      if (!rocksColl->readDocument(trx, docId, mdr)) {
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

    auto entry =
        cache::CachedValue::construct(previous.data(),
                                      static_cast<uint32_t>(previous.size()),
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
  LOG_TOPIC("99a29", DEBUG, Logger::ENGINES) << "loaded n: " << n;
}

// ===================== Helpers ==================

/// @brief create the iterator
std::unique_ptr<IndexIterator> RocksDBEdgeIndex::createEqIterator(transaction::Methods* trx,
                                                                  arangodb::aql::AstNode const* attrNode,
                                                                  arangodb::aql::AstNode const* valNode) const {
  // lease builder, but immediately pass it to the unique_ptr so we don't leak
  transaction::BuilderLeaser builder(trx);
  std::unique_ptr<VPackBuilder> keys(builder.steal());

  fillLookupValue(*keys, valNode);
  return std::make_unique<RocksDBEdgeIndexLookupIterator>(&_collection, trx, this, std::move(keys), _cache);
}

/// @brief create the iterator
std::unique_ptr<IndexIterator> RocksDBEdgeIndex::createInIterator(transaction::Methods* trx,
                                                                  arangodb::aql::AstNode const* attrNode,
                                                                  arangodb::aql::AstNode const* valNode) const {
  // lease builder, but immediately pass it to the unique_ptr so we don't leak
  transaction::BuilderLeaser builder(trx);
  std::unique_ptr<VPackBuilder> keys(builder.steal());

  fillInLookupValues(trx, *(keys.get()), valNode);
  return std::make_unique<RocksDBEdgeIndexLookupIterator>(&_collection, trx, this, std::move(keys), _cache);
}

void RocksDBEdgeIndex::fillLookupValue(VPackBuilder& keys,
                                       arangodb::aql::AstNode const* value) const {
  TRI_ASSERT(keys.isEmpty());
  keys.openArray(/*unindexed*/true);
  handleValNode(&keys, value);
  TRI_IF_FAILURE("EdgeIndex::noIterator") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  keys.close();
}

void RocksDBEdgeIndex::fillInLookupValues(transaction::Methods* trx, VPackBuilder& keys,
                                          arangodb::aql::AstNode const* values) const {
  TRI_ASSERT(values != nullptr);
  TRI_ASSERT(values->type == arangodb::aql::NODE_TYPE_ARRAY);
  TRI_ASSERT(keys.isEmpty());

  keys.openArray(/*unindexed*/true);
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
                                     arangodb::aql::AstNode const* valNode) const {
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
                                     arangodb::transaction::Methods* trx) {
  TRI_ASSERT(_estimator != nullptr);
  _estimator->bufferTruncate(tick);
  RocksDBIndex::afterTruncate(tick, trx);
}

RocksDBCuckooIndexEstimator<uint64_t>* RocksDBEdgeIndex::estimator() {
  return _estimator.get();
}

void RocksDBEdgeIndex::setEstimator(std::unique_ptr<RocksDBCuckooIndexEstimator<uint64_t>> est) {
  TRI_ASSERT(_estimator == nullptr || _estimator->appliedSeq() <= est->appliedSeq());
  _estimator = std::move(est);
}

void RocksDBEdgeIndex::recalculateEstimates() {
  TRI_ASSERT(_estimator != nullptr);
  _estimator->clear();

  auto& selector = _collection.vocbase().server().getFeature<EngineSelectorFeature>();
  auto& engine = selector.engine<RocksDBEngine>();
  rocksdb::TransactionDB* db = engine.db();
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
    //cppcheck-suppress uninitvar ; doesn't understand above call
    _estimator->insert(hash);
  }
  _estimator->setAppliedSeq(seq);
}

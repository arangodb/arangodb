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
/// @author Jan Steemann
/// @author Daniel H. Larkin
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBVPackIndex.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/AstNode.h"
#include "Aql/SortCondition.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/cpu-relax.h"
#include "Cache/CachedValue.h"
#include "Cache/CacheManagerFeature.h"
#include "Cache/TransactionalCache.h"
#include "Cache/VPackKeyHasher.h"
#include "Containers/FlatHashMap.h"
#include "Containers/FlatHashSet.h"
#include "Indexes/SortedIndexAttributeMatcher.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBComparator.h"
#include "RocksDBEngine/RocksDBCuckooIndexEstimator.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "RocksDBEngine/RocksDBPrimaryIndex.h"
#include "RocksDBEngine/RocksDBSettingsManager.h"
#include "RocksDBEngine/RocksDBTransactionCollection.h"
#include "RocksDBEngine/RocksDBTransactionMethods.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "Utils/OperationOptions.h"
#include "VocBase/LogicalCollection.h"

#include <rocksdb/iterator.h>
#include <rocksdb/options.h>
#include <rocksdb/utilities/transaction.h>
#include <rocksdb/utilities/transaction_db.h>
#include <rocksdb/utilities/write_batch_with_index.h>

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

using namespace arangodb;

namespace {

inline rocksdb::Slice lookupValueFromSlice(rocksdb::Slice data) noexcept {
  // remove object id prefix (8 bytes)
  TRI_ASSERT(data.size() > sizeof(uint64_t));
  data.remove_prefix(sizeof(uint64_t));
  return data;
}

// largest "acceptable" cache value size
constexpr uint64_t kMaxCacheValueSize = 4 * 1024 * 1024;
static_assert(kMaxCacheValueSize < std::numeric_limits<uint32_t>::max());

using VPackIndexCacheType = cache::TransactionalCache<cache::VPackKeyHasher>;

}  // namespace

// .............................................................................
// recall for all of the following comparison functions:
//
// left < right  return -1
// left > right  return  1
// left == right return  0
//
// furthermore:
//
// the following order is currently defined for placing an order on documents
// undef < null < boolean < number < strings < lists < hash arrays
// note: undefined will be treated as NULL pointer not NULL JSON OBJECT
// within each type class we have the following order
// boolean: false < true
// number: natural order
// strings: lexicographical
// lists: lexicographically and within each slot according to these rules.
// ...........................................................................

/// @brief Iterator structure for RocksDB unique index.
/// This iterator can be used only for equality lookups that use all
/// index attributes. It uses a point lookup and no seeks

namespace arangodb {

// an index iterator for unique VPack indexes.
// this iterator will produce at most 1 result per lookup. it can only be
// used for unique indexes *and* if *all* index attributes are used in the
// filter condition using *equality* lookups. this iterator is not used
// for non-equality lookups or if not all index attributes are used in the
// lookup condition.
class RocksDBVPackUniqueIndexIterator final : public IndexIterator {
 private:
  friend class RocksDBVPackIndex;

 public:
  RocksDBVPackUniqueIndexIterator(LogicalCollection* collection,
                                  transaction::Methods* trx,
                                  arangodb::RocksDBVPackIndex const* index,
                                  std::shared_ptr<cache::Cache> cache,
                                  VPackSlice indexValues,
                                  ReadOwnWrites readOwnWrites)
      : IndexIterator(collection, trx, readOwnWrites),
        _index(index),
        _cmp(index->comparator()),
        _cache(std::static_pointer_cast<VPackIndexCacheType>(std::move(cache))),
        _key(trx),
        _done(false) {
    TRI_ASSERT(index->unique());
    TRI_ASSERT(index->columnFamily() ==
               RocksDBColumnFamilyManager::get(
                   RocksDBColumnFamilyManager::Family::VPackIndex));
    _key->constructUniqueVPackIndexValue(index->objectId(), indexValues);

    // if the cache is enabled, it must use the VPackKeyHasher!
    TRI_ASSERT(_cache == nullptr || _cache->hasherName() == "VPackKeyHasher");

    TRI_IF_FAILURE("VPackIndexFailWithoutCache") {
      if (_cache == nullptr) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }
    }
    TRI_IF_FAILURE("VPackIndexFailWithCache") {
      if (_cache != nullptr) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }
    }

    // search key _key is fine here and can be used for cache lookups
    // without any transformation!
  }

  std::string_view typeName() const noexcept final {
    return "rocksdb-unique-index-iterator";
  }

  /// @brief index does support rearming
  bool canRearm() const override { return true; }

  /// @brief rearm the index iterator
  bool rearmImpl(arangodb::aql::AstNode const* node,
                 arangodb::aql::Variable const* variable,
                 IndexIteratorOptions const& opts) override {
    TRI_ASSERT(node != nullptr);
    TRI_ASSERT(node->type == aql::NODE_TYPE_OPERATOR_NARY_AND);

    transaction::BuilderLeaser searchValues(_trx);
    bool needNormalize = false;
    // note that we can ignore normalization here, because if needNormalize
    // is true, then the iteratorForCondition call would have built a
    // MultiIndexIterator and no VPackUniqueIndexIterator. MultiIndexIterator
    // does not support rearming, so whenever we are called, we can be
    // sure that needNormalize is false
    RocksDBVPackIndexSearchValueFormat format =
        RocksDBVPackIndexSearchValueFormat::kValuesOnly;
    _index->buildSearchValues(node, variable, *searchValues, format,
                              needNormalize);
    TRI_ASSERT(!needNormalize);
    // note: we only support the simplified format when building index search
    // values! format must not have changed!
    TRI_ASSERT(format == RocksDBVPackIndexSearchValueFormat::kValuesOnly);

    VPackSlice searchSlice = searchValues->slice();
    TRI_ASSERT(searchSlice.length() == 1);
    searchSlice = searchSlice.at(0);

    TRI_ASSERT(searchSlice.length() > 0);
    _key->constructUniqueVPackIndexValue(_index->objectId(), searchSlice);
    return true;
  }

  bool nextImpl(LocalDocumentIdCallback const& cb, uint64_t limit) override {
    return nextImplementation(
        [&cb](VPackArrayIterator it) {
          LocalDocumentId documentId{it.value().getNumericValue<uint64_t>()};
          cb(documentId);
        },
        [&cb](rocksdb::PinnableSlice& ps) {
          LocalDocumentId documentId = RocksDBValue::documentId(ps);
          cb(documentId);
        },
        limit);
  }

  bool nextCoveringImpl(CoveringCallback const& cb, uint64_t limit) override {
    return nextImplementation(
        [&cb, this](VPackArrayIterator it) {
          LocalDocumentId documentId{it.value().getNumericValue<uint64_t>()};
          it.next();
          VPackSlice value = it.value();
          if (_index->hasStoredValues()) {
            it.next();
            VPackSlice storedValues = it.value();
            auto data = SliceCoveringDataWithStoredValues(value, storedValues);
            cb(documentId, data);
          } else {
            auto data = SliceCoveringData(value);
            cb(documentId, data);
          }
        },
        [&cb, this](rocksdb::PinnableSlice& ps) {
          if (_index->hasStoredValues()) {
            auto data = SliceCoveringDataWithStoredValues(
                RocksDBKey::indexedVPack(_key.ref()),
                RocksDBValue::uniqueIndexStoredValues(ps));
            cb(LocalDocumentId(RocksDBValue::documentId(ps)), data);
          } else {
            auto data = SliceCoveringData(RocksDBKey::indexedVPack(_key.ref()));
            cb(LocalDocumentId(RocksDBValue::documentId(ps)), data);
          }
        },
        limit);
  }

  /// @brief Reset the cursor
  void resetImpl() final {
    TRI_ASSERT(_trx->state()->isRunning());
    _done = false;
  }

 private:
  template<typename F1, typename F2>
  inline bool nextImplementation(F1&& handleCacheEntry, F2&& handleRocksDBValue,
                                 uint64_t limit) {
    TRI_ASSERT(_trx->state()->isRunning());

    if (limit == 0 || _done) {
      // already looked up something
      return false;
    }

    _done = true;

    if (_cache != nullptr) {
      rocksdb::Slice key = lookupValueForCache();

      // Try to read from cache
      auto finding =
          _cache->find(key.data(), static_cast<uint32_t>(key.size()));
      if (finding.found()) {
        incrCacheHits();
        // We got sth. in the cache
        VPackSlice cachedData(finding.value()->value());
        if (!cachedData.isEmptyArray()) {
          TRI_ASSERT(cachedData.length() ==
                     (_index->hasStoredValues() ? 3 : 2));
          VPackArrayIterator it(cachedData);
          TRI_ASSERT(it.value().isNumber());
          handleCacheEntry(it);
        }
        return false;
      } else {
        incrCacheMisses();
      }
    }

    rocksdb::PinnableSlice ps;
    RocksDBMethods* mthds =
        RocksDBTransactionState::toMethods(_trx, _collection->id());
    rocksdb::Status s = mthds->Get(_index->columnFamily(), _key->string(), &ps,
                                   canReadOwnWrites());

    if (s.ok()) {
      handleRocksDBValue(ps);

      if (_cache != nullptr) {
        transaction::BuilderLeaser builder(_trx);

        builder->openArray(true);
        // LocalDocumentId
        builder->add(VPackValue(RocksDBValue::documentId(ps).id()));
        // index values
        builder->add(RocksDBKey::indexedVPack(_key->string()));
        if (_index->hasStoredValues()) {
          // "storedValues"
          builder->add(RocksDBValue::uniqueIndexStoredValues(ps));
        }
        builder->close();

        // store result in cache
        storeInCache(builder->slice());
      }
    } else {
      // we found nothing. store this in the cache
      if (_cache != nullptr) {
        storeInCache(VPackSlice::emptyArraySlice());
      }
    }

    // there is at most one element, so we are done now
    return false;
  }

  // store a value inside the in-memory hash cache
  void storeInCache(VPackSlice slice) {
    TRI_ASSERT(_cache != nullptr);

    uint64_t byteSize = slice.byteSize();
    rocksdb::Slice key = lookupValueForCache();
    if (ADB_UNLIKELY(key.size() > kMaxCacheValueSize ||
                     byteSize > kMaxCacheValueSize)) {
      // if key or value are too large for the cache, do not store in cache
      return;
    }
    cache::Cache::SimpleInserter<VPackIndexCacheType>{
        static_cast<VPackIndexCacheType&>(*_cache), key.data(),
        static_cast<uint32_t>(key.size()), slice.start(),
        static_cast<uint64_t>(byteSize)};
  }

  rocksdb::Slice lookupValueForCache() const noexcept {
    // use bounds start value
    return ::lookupValueFromSlice(_key->string());
  }

  arangodb::RocksDBVPackIndex const* _index;
  rocksdb::Comparator const* _cmp;
  std::shared_ptr<VPackIndexCacheType> _cache;
  RocksDBKeyLeaser _key;
  bool _done;
};

/// @brief Iterator structure for RocksDB. We require a start and stop node
template<bool unique, bool reverse, bool mustCheckBounds>
class RocksDBVPackIndexIterator final : public IndexIterator {
 private:
  friend class RocksDBVPackIndex;

 public:
  RocksDBVPackIndexIterator(LogicalCollection* collection,
                            transaction::Methods* trx,
                            arangodb::RocksDBVPackIndex const* index,
                            RocksDBKeyBounds&& bounds,
                            std::shared_ptr<cache::Cache> cache,
                            ReadOwnWrites readOwnWrites,
                            RocksDBVPackIndexSearchValueFormat format)
      : IndexIterator(collection, trx, readOwnWrites),
        _index(index),
        _cmp(static_cast<RocksDBVPackComparator const*>(index->comparator())),
        _cache(std::static_pointer_cast<VPackIndexCacheType>(std::move(cache))),
        _builderOptions(VPackOptions::Defaults),
        _cacheKeyBuilder(&_builderOptions),
        _cacheKeyBuilderSize(0),
        _resultBuilder(&_builderOptions),
        _resultIterator(VPackArrayIterator(VPackArrayIterator::Empty{})),
        _bounds(std::move(bounds)),
        _rangeBound(reverse ? _bounds.start() : _bounds.end()),
        _format(format),
        _mustSeek(true) {
    TRI_ASSERT(index->columnFamily() ==
               RocksDBColumnFamilyManager::get(
                   RocksDBColumnFamilyManager::Family::VPackIndex));

    // if the cache is enabled, it must use the VPackKeyHasher!
    TRI_ASSERT(_cache == nullptr || _cache->hasherName() == "VPackKeyHasher");

    TRI_IF_FAILURE("VPackIndexFailWithoutCache") {
      if (_cache == nullptr) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }
    }
    TRI_IF_FAILURE("VPackIndexFailWithCache") {
      if (_cache != nullptr) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }
    }

    if (_cache != nullptr) {
      // don't care about initial padding, to avoid later memmove
      _builderOptions.paddingBehavior =
          VPackOptions::PaddingBehavior::UsePadding;

      // in case we can use the hash cache for looking up data, we need to
      // extract a useful lookup value from _bounds.start() first. this is
      // because the lookup value for RocksDB contains a "min key" vpack
      // value as its last element
      rebuildCacheLookupValue();
    }
  }

  std::string_view typeName() const noexcept final {
    return "rocksdb-index-iterator";
  }

  /// @brief index does support rearming
  bool canRearm() const override { return true; }

  /// @brief rearm the index iterator
  bool rearmImpl(arangodb::aql::AstNode const* node,
                 arangodb::aql::Variable const* variable,
                 IndexIteratorOptions const& opts) override {
    TRI_ASSERT(node != nullptr);
    TRI_ASSERT(node->type == aql::NODE_TYPE_OPERATOR_NARY_AND);

    transaction::BuilderLeaser searchValues(_trx);
    bool needNormalize = false;
    // note that we can ignore normalization here, because if needNormalize
    // is true, then the iteratorForCondition call would have built a
    // MultiIndexIterator and no VPackIndexIterator. MultiIndexIterator
    // does not support rearming, so whenever we are called, we can be
    // sure that needNormalize is false
    RocksDBVPackIndexSearchValueFormat format = _format;
    _index->buildSearchValues(node, variable, *searchValues, format,
                              needNormalize);
    TRI_ASSERT(!needNormalize);
    // note: we support two formats when building index search values:
    // - a generic format that supports < > <= >= and ==
    // - a simplified format that only supports ==
    // in the generic format, the contents of searchValues will be an array of
    // objects, containing the comparison operators as keys and the lookup
    // values as values. in the simplified format, searchValues will be an array
    // of just the lookup values.
    // format must not have changed!
    TRI_ASSERT(_format == format);

    VPackSlice searchSlice = searchValues->slice();
    TRI_ASSERT(searchSlice.length() == 1);
    searchSlice = searchSlice.at(0);

    VPackValueLength const l = searchSlice.length();
    // if l == 0, then we have an iterator over the entire collection.
    // no need to adjust the bounds. only need to reseek to the start
    if (l > 0) {
      // check if we only have equality lookups
      transaction::BuilderLeaser leftSearch(_trx);

      VPackSlice lastNonEq;
      leftSearch->openArray();
      for (VPackSlice it : VPackArrayIterator(searchSlice)) {
        if (_format ==
            RocksDBVPackIndexSearchValueFormat::kOperatorsAndValues) {
          TRI_ASSERT(it.isObject());
          VPackSlice eq = it.get(StaticStrings::IndexEq);
          if (eq.isNone()) {
            lastNonEq = it;
            break;
          }
          leftSearch->add(eq);
        } else {
          TRI_ASSERT(_format ==
                     RocksDBVPackIndexSearchValueFormat::kValuesOnly);
          leftSearch->add(it);
        }
      }

      TRI_ASSERT(lastNonEq.isNone() ||
                 _format != RocksDBVPackIndexSearchValueFormat::kValuesOnly);
      // we cannot use the cache if lastNonEq is set.
      TRI_ASSERT(lastNonEq.isNone() || _cache == nullptr);

      _index->buildIndexRangeBounds(_trx, searchSlice, *leftSearch, lastNonEq,
                                    _bounds);
      _rangeBound = reverse ? _bounds.start() : _bounds.end();

      // need to rebuild lookup value for cache as well
      if (_cache != nullptr) {
        rebuildCacheLookupValue();
      }
    }
    // if l == 0, there must also be no cache
    TRI_ASSERT(l > 0 || _cache == nullptr);

    return true;
  }

  /// @brief Get the next limit many elements in the index
  bool nextImpl(LocalDocumentIdCallback const& cb, uint64_t limit) override {
    return nextImplementation(
        [&cb, this]() {
          // read LocalDocumentId
          VPackSlice value = _resultIterator.value();
          TRI_ASSERT(value.isNumber());
          LocalDocumentId documentId{value.getNumericValue<uint64_t>()};
          _resultIterator.next();

          // skip over key
          TRI_ASSERT(_resultIterator.valid());
          _resultIterator.next();

          if (_index->hasStoredValues()) {
            // skip over "storedValues" if present
            _resultIterator.next();
          }

          cb(documentId);
        },
        [&cb, this]() {
          TRI_ASSERT(_index->objectId() ==
                     RocksDBKey::objectId(_iterator->key()));

          if constexpr (unique) {
            cb(RocksDBValue::documentId(_iterator->value()));
          } else {
            cb(RocksDBKey::indexDocumentId(_iterator->key()));
          }
        },
        limit);
  }

  bool nextCoveringImpl(CoveringCallback const& cb, uint64_t limit) override {
    return nextImplementation(
        [&cb, this]() {
          // read LocalDocumentId
          VPackSlice value = _resultIterator.value();
          TRI_ASSERT(value.isNumber());
          LocalDocumentId documentId{value.getNumericValue<uint64_t>()};
          _resultIterator.next();

          // read actual index value
          TRI_ASSERT(_resultIterator.valid());
          value = _resultIterator.value();
          _resultIterator.next();

          if (_index->hasStoredValues()) {
            // "storedValues"
            VPackSlice storedValues = _resultIterator.value();
            _resultIterator.next();

            auto data = SliceCoveringDataWithStoredValues(value, storedValues);
            cb(documentId, data);
          } else {
            auto data = SliceCoveringData(value);
            cb(documentId, data);
          }
        },
        [&cb, this]() {
          rocksdb::Slice key = _iterator->key();
          TRI_ASSERT(_index->objectId() == RocksDBKey::objectId(key));

          if constexpr (unique) {
            LocalDocumentId const documentId(
                RocksDBValue::documentId(_iterator->value()));
            if (_index->hasStoredValues()) {
              auto data = SliceCoveringDataWithStoredValues(
                  RocksDBKey::indexedVPack(key),
                  RocksDBValue::uniqueIndexStoredValues(_iterator->value()));
              cb(documentId, data);
            } else {
              auto data = SliceCoveringData(RocksDBKey::indexedVPack(key));
              cb(documentId, data);
            }
          } else {
            LocalDocumentId const documentId(RocksDBKey::indexDocumentId(key));
            if (_index->hasStoredValues()) {
              auto data = SliceCoveringDataWithStoredValues(
                  RocksDBKey::indexedVPack(key),
                  RocksDBValue::indexStoredValues(_iterator->value()));
              cb(documentId, data);
            } else {
              auto data = SliceCoveringData(RocksDBKey::indexedVPack(key));
              cb(documentId, data);
            }
          }
        },
        limit);
  }

  void skipImpl(uint64_t count, uint64_t& skipped) override {
    ensureIterator();
    TRI_ASSERT(_trx->state()->isRunning());
    TRI_ASSERT(_iterator != nullptr);

    if (_iterator->Valid() && !outOfRange() && count > 0) {
      do {
        TRI_ASSERT(_index->objectId() ==
                   RocksDBKey::objectId(_iterator->key()));

        --count;
        ++skipped;
        if (!advance()) {
          break;
        }

        if (count == 0) {
          return;
        }
      } while (true);
    }

    // validate that Iterator is in a good shape and hasn't failed
    rocksutils::checkIteratorStatus(*_iterator);
  }

  /// @brief Reset the cursor
  void resetImpl() final {
    TRI_ASSERT(_trx->state()->isRunning());
    _mustSeek = true;

    if (_cache != nullptr) {
      _resultBuilder.clear();
      _resultIterator = VPackArrayIterator(VPackArrayIterator::Empty{});
    }
  }

 private:
  void rebuildCacheLookupValue() {
    TRI_ASSERT(_cache != nullptr);

    // strip the object id from the lookup value
    rocksdb::Slice b = ::lookupValueFromSlice(_bounds.start());
    VPackSlice s(reinterpret_cast<uint8_t const*>(b.data()));
    TRI_ASSERT(s.isArray());

    _cacheKeyBuilder.clear();
    _cacheKeyBuilder.openArray(true);
    for (VPackSlice it : VPackArrayIterator(s)) {
      // don't include "min key" or "max key" here!
      if (it.type() == VPackValueType::MinKey ||
          it.type() == VPackValueType::MaxKey) {
        break;
      }
      _cacheKeyBuilder.add(it);
    }
    _cacheKeyBuilder.close();
    _cacheKeyBuilderSize = _cacheKeyBuilder.slice().byteSize();
  }

  enum class CacheLookupResult {
    kNotInCache,
    kInCacheAndFullyHandled,
    kInCacheAndPartlyHandled
  };

  /// internal retrieval loop
  template<typename F1, typename F2>
  inline bool nextImplementation(F1&& handleIndexEntry,
                                 F2&& consumeIteratorValue, uint64_t limit) {
    if (_cache) {
      while (true) {
        // this loop will only be left by return statements
        while (limit > 0) {
          // return values from local buffer first, if we still have any
          if (_resultIterator.valid()) {
            bool valid;
            do {
              handleIndexEntry();

              valid = _resultIterator.valid();

              if (--limit == 0) {
                // Limit reached. bail out
                return valid;
              }
            } while (valid);
            // all exhausted
            return false;
          }

          // no _resultIterator set (yet), so we need to consult the in-memory
          // cache or even do a lookup in RocksDB later.
          auto lookupResult = lookupInCache(handleIndexEntry, limit);

          if (lookupResult == CacheLookupResult::kInCacheAndFullyHandled) {
            // value was found in cache, and the number of cache results was <=
            // limit. this means we have produced all results for the lookup
            // value, and can exit the loop here.
            TRI_ASSERT(!_resultIterator.valid());
            return false;
          }
          if (lookupResult == CacheLookupResult::kNotInCache) {
            // value not found in in-memory cache. break out of the loop and
            // do a lookup in RocksDB.
            break;
          }

          // value was found in cache, but we have not yet reached limit.
          TRI_ASSERT(lookupResult ==
                     CacheLookupResult::kInCacheAndPartlyHandled);
          TRI_ASSERT(_resultIterator.valid());
        }

        TRI_ASSERT(!_resultIterator.valid());

        // look up the value in RocksDB
        ensureIterator();
        TRI_ASSERT(_iterator != nullptr);

        if (limit == 0 || !_iterator->Valid() || outOfRange()) {
          // No limit no data, or we are actually done. The last call should
          // have returned false
          TRI_ASSERT(limit > 0);  // Someone called with limit == 0. Api broken
          // validate that Iterator is in a good shape and hasn't failed
          rocksutils::checkIteratorStatus(*_iterator);

          // store in in-memory cache that we found nothing.
          storeInCache(VPackSlice::emptyArraySlice());

          // no data found
          return false;
        }

        TRI_ASSERT(limit > 0);

        fillResultBuilder();
        // now we should have data in the result builder.
        TRI_ASSERT(_resultIterator.valid());
        // go to next round
      }  // while (true)
    }

    TRI_ASSERT(!_resultIterator.valid());

    // look up the value in RocksDB
    ensureIterator();
    TRI_ASSERT(_iterator != nullptr);

    if (!_iterator->Valid() || outOfRange() || ADB_UNLIKELY(limit == 0)) {
      // No limit no data, or we are actually done. The last call should have
      // returned false
      TRI_ASSERT(limit > 0);  // Someone called with limit == 0. Api broken
      // validate that Iterator is in a good shape and hasn't failed
      rocksutils::checkIteratorStatus(*_iterator);

      // no data found
      return false;
    }

    TRI_ASSERT(limit > 0);

    // cannot get here if we have a cache
    do {
      consumeIteratorValue();

      if (!advance()) {
        // validate that Iterator is in a good shape and hasn't failed
        rocksutils::checkIteratorStatus(*_iterator);
        return false;
      }

      if (--limit == 0) {
        return true;
      }
    } while (true);
  }

  void fillResultBuilder() {
    TRI_ASSERT(_cache != nullptr);

    _resultBuilder.clear();
    _resultBuilder.openArray(true);

    while (true) {
      rocksdb::Slice key = _iterator->key();
      TRI_ASSERT(_index->objectId() == RocksDBKey::objectId(key));

      if constexpr (unique) {
        LocalDocumentId const documentId(
            RocksDBValue::documentId(_iterator->value()));
        _resultBuilder.add(VPackValue(documentId.id()));
        _resultBuilder.add(RocksDBKey::indexedVPack(key));
        if (_index->hasStoredValues()) {
          _resultBuilder.add(
              RocksDBValue::uniqueIndexStoredValues(_iterator->value()));
        }
      } else {
        LocalDocumentId const documentId(RocksDBKey::indexDocumentId(key));
        _resultBuilder.add(VPackValue(documentId.id()));
        _resultBuilder.add(RocksDBKey::indexedVPack(key));
        if (_index->hasStoredValues()) {
          _resultBuilder.add(
              RocksDBValue::indexStoredValues(_iterator->value()));
        }
      }

      if (!advance()) {
        // validate that Iterator is in a good shape and hasn't failed
        rocksutils::checkIteratorStatus(*_iterator);
        break;
      }
    }

    _resultBuilder.close();
    _resultIterator = VPackArrayIterator(_resultBuilder.slice());

    storeInCache(_resultBuilder.slice());
  }

  // look up a value in the in-memory hash cache
  template<typename F>
  inline CacheLookupResult lookupInCache(F&& cb, uint64_t& limit) {
    TRI_ASSERT(_cache != nullptr);

    // key too large to be cached. should almost never happen
    if (ADB_UNLIKELY(_cacheKeyBuilderSize) >
        std::numeric_limits<uint32_t>::max()) {
      return CacheLookupResult::kNotInCache;
    }

    size_t const numFields = _index->hasStoredValues() ? 3 : 2;
    VPackSlice key = _cacheKeyBuilder.slice();

    // Try to read from cache
    auto finding =
        _cache->find(key.start(), static_cast<uint32_t>(_cacheKeyBuilderSize));
    if (finding.found()) {
      incrCacheHits();
      // We got sth. in the cache
      VPackSlice cachedData(finding.value()->value());
      TRI_ASSERT(cachedData.isArray());
      if (cachedData.length() / numFields < limit) {
        // Directly return it, no need to copy
        _resultIterator = VPackArrayIterator(cachedData);
        while (_resultIterator.valid()) {
          cb();
          TRI_ASSERT(limit > 0);
          --limit;
        }
        _resultIterator = VPackArrayIterator(VPackArrayIterator::Empty{});
        return CacheLookupResult::kInCacheAndFullyHandled;
      }

      // We need to copy the data from the cache, and let the caller
      // handler the result.
      _resultBuilder.clear();
      _resultBuilder.add(cachedData);
      TRI_ASSERT(_resultBuilder.slice().isArray());
      _resultIterator = VPackArrayIterator(_resultBuilder.slice());
      return CacheLookupResult::kInCacheAndPartlyHandled;
    }
    incrCacheMisses();
    return CacheLookupResult::kNotInCache;
  }

  // store a value inside the in-memory hash cache
  void storeInCache(VPackSlice slice) {
    TRI_ASSERT(_cache != nullptr);

    uint64_t byteSize = slice.byteSize();
    if (ADB_UNLIKELY(_cacheKeyBuilderSize > kMaxCacheValueSize ||
                     byteSize > kMaxCacheValueSize)) {
      // if key or value are too large for the cache, do not store in cache
      return;
    }

    VPackSlice key = _cacheKeyBuilder.slice();
    cache::Cache::SimpleInserter<VPackIndexCacheType>{
        static_cast<VPackIndexCacheType&>(*_cache), key.start(),
        static_cast<uint32_t>(_cacheKeyBuilderSize), slice.start(),
        static_cast<uint64_t>(byteSize)};
  }

  inline bool outOfRange() const {
    // we can effectively disable the out-of-range checks for read-only
    // transactions, as our Iterator is a snapshot-based iterator with a
    // configured iterate_upper_bound/iterate_lower_bound value.
    // this makes RocksDB filter out non-matching keys automatically.
    // however, for a write transaction our Iterator is a rocksdb
    // BaseDeltaIterator, which will merge the values from a snapshot iterator
    // and the changes in the current transaction. here rocksdb will only apply
    // the bounds checks for the base iterator (from the snapshot), but not for
    // the delta iterator (from the current transaction), so we still have to
    // carry out the checks ourselves.
    if constexpr (mustCheckBounds) {
      int res = _cmp->Compare(_iterator->key(), _rangeBound);
      if constexpr (reverse) {
        return res < 0;
      } else {
        return res > 0;
      }
    } else {
      return false;
    }
  }

  void ensureIterator() {
    if (_iterator == nullptr) {
      auto state = RocksDBTransactionState::toState(_trx);
      RocksDBTransactionMethods* mthds =
          state->rocksdbMethods(_collection->id());
      _iterator =
          mthds->NewIterator(_index->columnFamily(), [&](ReadOptions& options) {
            TRI_ASSERT(options.prefix_same_as_start);
            // we need to have a pointer to a slice for the upper bound
            // so we need to assign the slice to an instance variable here
            if constexpr (reverse) {
              options.iterate_lower_bound = &_rangeBound;
            } else {
              options.iterate_upper_bound = &_rangeBound;
            }
            options.readOwnWrites = canReadOwnWrites() == ReadOwnWrites::yes;
          });
    }

    TRI_ASSERT(_iterator != nullptr);
    if (_mustSeek) {
      if constexpr (reverse) {
        _iterator->SeekForPrev(_bounds.end());
      } else {
        _iterator->Seek(_bounds.start());
      }
      _mustSeek = false;
    }
    TRI_ASSERT(!_mustSeek);
  }

  inline bool advance() {
    if constexpr (reverse) {
      _iterator->Prev();
    } else {
      _iterator->Next();
    }

    return _iterator->Valid() && !outOfRange();
  }

  arangodb::RocksDBVPackIndex const* _index;
  RocksDBVPackComparator const* _cmp;
  std::unique_ptr<rocksdb::Iterator> _iterator;
  std::shared_ptr<VPackIndexCacheType> _cache;
  // VPackOptions for _cacheKeyBuilder and _resultBuilder. only used when
  // _cache is set
  arangodb::velocypack::Options _builderOptions;
  // Builder that holds the cache lookup value. only used when _cache is set
  arangodb::velocypack::Builder _cacheKeyBuilder;
  // Amount of data (in bytes) in _cacheKeyBuilder. stored in a separate
  // variable so that we can avoid repeated calls to _resultBuilder.byteSize(),
  // which can be expensive
  size_t _cacheKeyBuilderSize;
  // Builder with cache lookup results (an array of 0..n index entries).
  // only used when _cache is set
  arangodb::velocypack::Builder _resultBuilder;
  // Iterator into cache lookup results, pointing into _resultBuilder. only
  // set when _cache is set
  arangodb::velocypack::ArrayIterator _resultIterator;
  RocksDBKeyBounds _bounds;
  // used for iterate_upper_bound iterate_lower_bound
  rocksdb::Slice _rangeBound;
  RocksDBVPackIndexSearchValueFormat const _format;
  bool _mustSeek;
};

}  // namespace arangodb

uint64_t RocksDBVPackIndex::HashForKey(rocksdb::Slice const& key) {
  // NOTE: This function needs to use the same hashing on the
  // indexed VPack as the initial inserter does
  VPackSlice tmp = RocksDBKey::indexedVPack(key);
  return tmp.normalizedHash();
}

/// @brief create the index
RocksDBVPackIndex::RocksDBVPackIndex(IndexId iid,
                                     arangodb::LogicalCollection& collection,
                                     arangodb::velocypack::Slice info)
    : RocksDBIndex(iid, collection, info,
                   RocksDBColumnFamilyManager::get(
                       RocksDBColumnFamilyManager::Family::VPackIndex),
                   /*useCache*/
                   arangodb::basics::VelocyPackHelper::getBooleanValue(
                       info, StaticStrings::CacheEnabled, false),
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
      _cacheEnabled(arangodb::basics::VelocyPackHelper::getBooleanValue(
          info, StaticStrings::CacheEnabled, false)),
      _deduplicate(arangodb::basics::VelocyPackHelper::getBooleanValue(
          info, StaticStrings::IndexDeduplicate, true)),
      _estimates(true),
      _estimator(nullptr),
      _storedValues(Index::parseFields(
          info.get(arangodb::StaticStrings::IndexStoredValues),
          /*allowEmpty*/ true, /*allowExpansion*/ false)),
      _coveredFields(Index::mergeFields(fields(), _storedValues)) {
  TRI_ASSERT(_cf == RocksDBColumnFamilyManager::get(
                        RocksDBColumnFamilyManager::Family::VPackIndex));

  if (_unique) {
    // unique indexes always have a hard-coded estimate of 1
    _estimates = true;
  } else if (VPackSlice s = info.get(StaticStrings::IndexEstimates);
             s.isBoolean()) {
    // read "estimates" flag from velocypack if it is present.
    // if it's not present, we go with the default (estimates = true)
    _estimates = s.getBoolean();
  }

  if (_estimates && !_unique && !ServerState::instance()->isCoordinator() &&
      !collection.isAStub()) {
    // We activate the estimator for all non unique-indexes.
    // And only on single servers and DBServers
    _estimator = std::make_unique<RocksDBCuckooIndexEstimatorType>(
        RocksDBIndex::ESTIMATOR_SIZE);
  }

  TRI_ASSERT(!_fields.empty());
  TRI_ASSERT(iid.isSet());

  fillPaths(_fields, _paths, &_expanding);
  fillPaths(_storedValues, _storedValuesPaths, nullptr);
  TRI_ASSERT(_fields.size() == _paths.size());
  TRI_ASSERT(_storedValues.size() == _storedValuesPaths.size());

  if (_cacheEnabled) {
    // create a hash cache in front of the index if requested.
    // note: _cacheEnabled contains the user's setting for caching.
    // the cache may effectively still be turned off for system
    // collections or on the coordinator...
    setupCache();
    // now, we may or may not have a cache, depending on whether the
    // collection/environment are eligible for caching.
  }
}

/// @brief destroy the index
RocksDBVPackIndex::~RocksDBVPackIndex() = default;

std::vector<std::vector<arangodb::basics::AttributeName>> const&
RocksDBVPackIndex::coveredFields() const {
  return _coveredFields;
}

bool RocksDBVPackIndex::hasSelectivityEstimate() const {
  // unique indexes always have a selectivity estimate (which is hard-coded
  // to a value of 1). non-unique indexes can have a selectivity estimate.
  return _unique || _estimates;
}

double RocksDBVPackIndex::selectivityEstimate(std::string_view) const {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  if (_unique) {
    return 1.0;
  }
  if (_estimator == nullptr || !_estimates) {
    // we turn off the estimates for some system collections to avoid updating
    // them too often. we also turn off estimates for stub collections on
    // coordinator and DB servers
    return 0.0;
  }
  TRI_ASSERT(_estimator != nullptr);
  return _estimator->computeEstimate();
}

/// @brief return a VelocyPack representation of the index
void RocksDBVPackIndex::toVelocyPack(
    VPackBuilder& builder, std::underlying_type<Serialize>::type flags) const {
  builder.openObject();
  RocksDBIndex::toVelocyPack(builder, flags);

  // serialize storedValues, if they exist
  if (!_storedValues.empty()) {
    builder.add(arangodb::velocypack::Value(
        arangodb::StaticStrings::IndexStoredValues));
    builder.openArray();

    for (auto const& field : _storedValues) {
      std::string fieldString;
      TRI_AttributeNamesToString(field, fieldString);
      builder.add(VPackValue(fieldString));
    }

    builder.close();
  }

  builder.add(StaticStrings::IndexDeduplicate, VPackValue(_deduplicate));
  builder.add(StaticStrings::IndexEstimates, VPackValue(_estimates));
  builder.add(StaticStrings::CacheEnabled, VPackValue(_cacheEnabled));
  builder.close();
}

/// @brief helper function to insert a document into any index type
/// Should result in an elements vector filled with the new index entries
/// uses the _unique field to determine the kind of key structure
ErrorCode RocksDBVPackIndex::fillElement(
    VPackBuilder& leased, LocalDocumentId const& documentId, VPackSlice doc,
    containers::SmallVector<RocksDBKey, 4>& elements,
    containers::SmallVector<uint64_t, 4>& hashes) {
  if (doc.isNone()) {
    LOG_TOPIC("51c6c", ERR, arangodb::Logger::ENGINES)
        << "encountered invalid marker with slice of type None";
    return TRI_ERROR_INTERNAL;
  }

  TRI_IF_FAILURE("FillElementIllegalSlice") { return TRI_ERROR_INTERNAL; }

  TRI_ASSERT(leased.isEmpty());
  if (!_useExpansion) {
    // fast path for inserts... no array elements used
    leased.openArray(true);

    size_t const n = _paths.size();
    for (size_t i = 0; i < n; ++i) {
      TRI_ASSERT(!_paths[i].empty());

      VPackSlice slice = doc.get(_paths[i]);
      if (slice.isNone() || slice.isNull()) {
        // attribute not found
        if (_sparse) {
          // if sparse we do not have to index, this is indicated by result
          // being shorter than n
          return TRI_ERROR_NO_ERROR;
        }
        // null, note that this will be copied later!
        leased.add(VPackSlice::nullSlice());
      } else {
        leased.add(slice);
      }
    }
    leased.close();

    TRI_IF_FAILURE("FillElementOOM") { return TRI_ERROR_OUT_OF_MEMORY; }
    TRI_IF_FAILURE("FillElementOOM2") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }

    elements.emplace_back();
    RocksDBKey& key = elements.back();
    if (_unique) {
      // Unique VPack index values are stored as follows:
      // - Key: 7 + 8-byte object ID of index + VPack array with index
      // value(s) + separator (NUL) byte
      // - Value: primary key
      key.constructUniqueVPackIndexValue(objectId(), leased.slice());
    } else {
      // Non-unique VPack index values are stored as follows:
      // - Key: 6 + 8-byte object ID of index + VPack array with index
      // value(s) + revisionID
      // - Value: empty
      key.constructVPackIndexValue(objectId(), leased.slice(), documentId);
      hashes.push_back(leased.slice().normalizedHash());
    }
  } else {
    // other path for handling array elements, too

    containers::SmallVector<VPackSlice, 4> sliceStack;

    try {
      buildIndexValues(leased, documentId, doc, 0, elements, hashes,
                       sliceStack);
    } catch (arangodb::basics::Exception const& ex) {
      return ex.code();
    } catch (std::bad_alloc const&) {
      return TRI_ERROR_OUT_OF_MEMORY;
    } catch (...) {
      // unknown error
      return TRI_ERROR_INTERNAL;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

void RocksDBVPackIndex::addIndexValue(
    VPackBuilder& leased, LocalDocumentId const& documentId,
    VPackSlice document, containers::SmallVector<RocksDBKey, 4>& elements,
    containers::SmallVector<uint64_t, 4>& hashes,
    std::span<VPackSlice const> sliceStack) {
  leased.clear();
  leased.openArray(true);  // unindexed
  for (VPackSlice const& s : sliceStack) {
    leased.add(s);
  }
  leased.close();

  if (_unique) {
    // Unique VPack index values are stored as follows:
    // - Key: 7 + 8-byte object ID of index + VPack array with index value(s)
    // - Value: primary key
    RocksDBKey key;
    key.constructUniqueVPackIndexValue(objectId(), leased.slice());
    elements.emplace_back(std::move(key));
  } else {
    // Non-unique VPack index values are stored as follows:
    // - Key: 6 + 8-byte object ID of index + VPack array with index value(s)
    // + primary key
    // - Value: empty
    RocksDBKey key;
    key.constructVPackIndexValue(objectId(), leased.slice(), documentId);
    elements.emplace_back(std::move(key));
    hashes.push_back(leased.slice().normalizedHash());
  }
}

/// @brief helper function to create a set of index combinations to insert
void RocksDBVPackIndex::buildIndexValues(
    VPackBuilder& leased, LocalDocumentId const& documentId,
    VPackSlice const doc, size_t level,
    containers::SmallVector<RocksDBKey, 4>& elements,
    containers::SmallVector<uint64_t, 4>& hashes,
    containers::SmallVector<VPackSlice, 4>& sliceStack) {
  // Invariant: level == sliceStack.size()

  // Stop the recursion:
  if (level == _paths.size()) {
    addIndexValue(leased, documentId, doc, elements, hashes, sliceStack);
    return;
  }

  if (_expanding[level] == -1) {  // the trivial, non-expanding case
    VPackSlice slice = doc.get(_paths[level]);
    if (slice.isNone() || slice.isNull()) {
      if (_sparse) {
        return;
      }
      sliceStack.emplace_back(arangodb::velocypack::Slice::nullSlice());
    } else {
      sliceStack.emplace_back(slice);
    }
    buildIndexValues(leased, documentId, doc, level + 1, elements, hashes,
                     sliceStack);
    sliceStack.pop_back();
    return;
  }

  // Finally, the complex case, where we have to expand one entry.
  // Note again that at most one step in the attribute path can be
  // an array step.

  // Trivial case to bottom out with Illegal types.
  auto finishWithNones = [&]() -> void {
    if (level != 0) {
      for (size_t i = level; i < _paths.size(); i++) {
        sliceStack.emplace_back(arangodb::velocypack::Slice::illegalSlice());
      }
      addIndexValue(leased, documentId, doc, elements, hashes, sliceStack);
      for (size_t i = level; i < _paths.size(); i++) {
        sliceStack.pop_back();
      }
    }
  };
  size_t const n = _paths[level].size();
  // We have 0 <= _expanding[level] < n.
  VPackSlice current(doc);
  for (size_t i = 0; i <= static_cast<size_t>(_expanding[level]); i++) {
    if (!current.isObject()) {
      finishWithNones();
      return;
    }
    current = current.get(_paths[level][i]);
    if (current.isNone()) {
      finishWithNones();
      return;
    }
  }
  // Now the expansion:
  if (!current.isArray() || current.length() == 0) {
    finishWithNones();
    return;
  }

  std::unordered_set<VPackSlice, arangodb::basics::VelocyPackHelper::VPackHash,
                     arangodb::basics::VelocyPackHelper::VPackEqual>
      seen(2, arangodb::basics::VelocyPackHelper::VPackHash(),
           arangodb::basics::VelocyPackHelper::VPackEqual());

  auto moveOn = [&](VPackSlice something) -> void {
    auto it = seen.find(something);
    if (it == seen.end()) {
      seen.insert(something);
      sliceStack.emplace_back(something);
      buildIndexValues(leased, documentId, doc, level + 1, elements, hashes,
                       sliceStack);
      sliceStack.pop_back();
    } else if (_unique && !_deduplicate) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED);
    }
  };
  for (VPackSlice member : VPackArrayIterator(current)) {
    VPackSlice current2(member);
    bool doneNull = false;
    for (size_t i = _expanding[level] + 1; i < n; i++) {
      if (!current2.isObject()) {
        if (!_sparse) {
          moveOn(arangodb::velocypack::Slice::nullSlice());
        }
        doneNull = true;
        break;
      }
      current2 = current2.get(_paths[level][i]);
      if (current2.isNone()) {
        if (!_sparse) {
          moveOn(arangodb::velocypack::Slice::nullSlice());
        }
        doneNull = true;
        break;
      }
    }
    if (!doneNull) {
      moveOn(current2);
    }
    // Finally, if, because of sparsity, we have not inserted anything by now,
    // we need to play the above trick with None because of the above
    // mentioned
    // reasons:
    if (seen.empty()) {
      finishWithNones();
    }
  }
}

/// @brief helper function to transform AttributeNames into strings.
void RocksDBVPackIndex::fillPaths(
    std::vector<std::vector<arangodb::basics::AttributeName>> const& source,
    std::vector<std::vector<std::string>>& paths, std::vector<int>* expanding) {
  paths.clear();
  if (expanding != nullptr) {
    expanding->clear();
  }
  for (std::vector<arangodb::basics::AttributeName> const& list : source) {
    paths.emplace_back();
    std::vector<std::string>& interior(paths.back());
    int expands = -1;
    int count = 0;
    for (auto const& att : list) {
      interior.emplace_back(att.name);
      if (att.shouldExpand) {
        expands = count;
      }
      ++count;
    }
    if (expanding != nullptr) {
      expanding->emplace_back(expands);
    }
  }
}

/// @brief returns whether the document can be inserted into the index
/// (or if there will be a conflict)
Result RocksDBVPackIndex::checkInsert(transaction::Methods& trx,
                                      RocksDBMethods* mthds,
                                      LocalDocumentId const& documentId,
                                      velocypack::Slice doc,
                                      OperationOptions const& options) {
  return checkOperation(trx, mthds, documentId, doc, options, false);
}

/// @brief returns whether the document can be replaced into the index
/// (or if there will be a conflict)
Result RocksDBVPackIndex::checkReplace(transaction::Methods& trx,
                                       RocksDBMethods* mthds,
                                       LocalDocumentId const& documentId,
                                       velocypack::Slice doc,
                                       OperationOptions const& options) {
  return checkOperation(trx, mthds, documentId, doc, options, true);
}

Result RocksDBVPackIndex::checkOperation(transaction::Methods& trx,
                                         RocksDBMethods* mthds,
                                         LocalDocumentId const& documentId,
                                         velocypack::Slice doc,
                                         OperationOptions const& options,
                                         bool ignoreExisting) {
  Result res;

  // non-unique indexes will not cause any constraint violation
  if (_unique) {
    // unique indexes...

    IndexOperationMode mode = options.indexOperationMode;
    rocksdb::Status s;
    containers::SmallVector<RocksDBKey, 4> elements;
    containers::SmallVector<uint64_t, 4> hashes;

    {
      // rethrow all types of exceptions from here...
      transaction::BuilderLeaser leased(&trx);
      auto r = fillElement(*leased, documentId, doc, elements, hashes);

      if (r != TRI_ERROR_NO_ERROR) {
        return addErrorMsg(res, r);
      }
    }

    transaction::StringLeaser leased(&trx);
    rocksdb::PinnableSlice existing(leased.get());

    bool const lock =
        !RocksDBTransactionState::toState(&trx)->isOnlyExclusiveTransaction();

    for (RocksDBKey const& key : elements) {
      if (lock) {
        s = mthds->GetForUpdate(_cf, key.string(), &existing);
      } else {
        // modifications always need to observe all changes in order to validate
        // uniqueness constraints
        s = mthds->Get(_cf, key.string(), &existing, ReadOwnWrites::yes);
      }

      if (s.ok()) {  // detected conflicting index entry
        LocalDocumentId docId = RocksDBValue::documentId(existing);
        if (docId == documentId && ignoreExisting) {
          // same document, this is ok!
          continue;
        }
        res.reset(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED);
        // find conflicting document's key
        auto readResult = _collection.getPhysical()->read(
            &trx, docId,
            [&](LocalDocumentId const&, VPackSlice doc) {
              VPackSlice key =
                  transaction::helpers::extractKeyFromDocument(doc);
              if (mode == IndexOperationMode::internal) {
                // in this error mode, we return the conflicting document's key
                // inside the error message string (and nothing else)!
                res = Result{res.errorNumber(), key.copyString()};
              } else {
                // normal mode: build a proper error message
                addErrorMsg(res, key.copyString());
              }
              return true;  // return value does not matter here
            },
            ReadOwnWrites::yes);  // modifications always need to observe all
                                  // changes in order to validate uniqueness
                                  // constraints
        if (readResult.fail()) {
          addErrorMsg(readResult);
          THROW_ARANGO_EXCEPTION(readResult);
        }
        TRI_ASSERT(res.is(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED));
        break;
      } else if (!s.IsNotFound()) {
        res.reset(rocksutils::convertStatus(s));
        addErrorMsg(res, doc.get(StaticStrings::KeyString).copyString());
      }
    }
  }

  return res;
}

/// @brief inserts a document into the index
Result RocksDBVPackIndex::insert(transaction::Methods& trx,
                                 RocksDBMethods* mthds,
                                 LocalDocumentId const& documentId,
                                 velocypack::Slice doc,
                                 OperationOptions const& options,
                                 bool performChecks) {
  Result res;
  containers::SmallVector<RocksDBKey, 4> elements;
  containers::SmallVector<uint64_t, 4> hashes;

  {
    // rethrow all types of exceptions from here...
    transaction::BuilderLeaser leased(&trx);
    auto r = fillElement(*leased, documentId, doc, elements, hashes);

    if (r != TRI_ERROR_NO_ERROR) {
      return addErrorMsg(res, r);
    }
  }

  bool isIndexCreation =
      trx.state()->hasHint(transaction::Hints::Hint::INDEX_CREATION);

  // now we are going to construct the value to insert into rocksdb
  if (_unique) {
    // build index value (storedValues array will be stored in value if
    // storedValues are used)
    RocksDBValue value = RocksDBValue::UniqueVPackIndexValue(documentId);
    if (!_storedValuesPaths.empty()) {
      transaction::BuilderLeaser leased(&trx);
      leased->openArray(true);
      for (auto const& it : _storedValuesPaths) {
        VPackSlice s = doc.get(it);
        if (s.isNone()) {
          s = VPackSlice::nullSlice();
        }
        leased->add(s);
      }
      leased->close();
      value = RocksDBValue::UniqueVPackIndexValue(documentId, leased->slice());
    }

    transaction::StringLeaser leased(&trx);
    rocksdb::PinnableSlice existing(leased.get());

    rocksdb::Status s;
    // unique indexes have a different key structure
    for (RocksDBKey const& key : elements) {
      if (!isIndexCreation) {
        // banish key in in-memory cache.
        // not necessary during index creation, because nothing
        // will be in the in-memory cache.
        invalidateCacheEntry(::lookupValueFromSlice(key.string()));
      }

      if (performChecks) {
        s = mthds->GetForUpdate(_cf, key.string(), &existing);
        if (s.ok()) {  // detected conflicting index entry
          res.reset(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED);
          break;
        } else if (!s.IsNotFound()) {
          res.reset(rocksutils::convertStatus(s));
          break;
        }
      }
      s = mthds->Put(_cf, key, value.string(), /*assume_tracked*/ true);
      if (!s.ok()) {
        res = rocksutils::convertStatus(s, rocksutils::index);
        break;
      }
    }

    if (res.fail()) {
      if (res.is(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED)) {
        // find conflicting document's key
        LocalDocumentId docId = RocksDBValue::documentId(existing);
        auto readResult = _collection.getPhysical()->read(
            &trx, docId,
            [&](LocalDocumentId const&, VPackSlice doc) {
              IndexOperationMode mode = options.indexOperationMode;
              VPackSlice key =
                  transaction::helpers::extractKeyFromDocument(doc);
              if (mode == IndexOperationMode::internal) {
                // in this error mode, we return the conflicting document's key
                // inside the error message string (and nothing else)!
                res = Result{res.errorNumber(), key.copyString()};
              } else {
                // normal mode: build a proper error message
                addErrorMsg(res, key.copyString());
              }
              return true;  // return value does not matter here
            },
            ReadOwnWrites::yes);  // modifications always need to observe all
                                  // changes in order to validate uniqueness
                                  // constraints
        if (readResult.fail()) {
          addErrorMsg(readResult);
          THROW_ARANGO_EXCEPTION(readResult);
        }
      } else {
        addErrorMsg(res, doc.get(StaticStrings::KeyString).copyString());
      }
    }

  } else {
    // non-unique index

    // AQL queries never read from the same collection, after writing into it
    IndexingDisabler guard(
        mthds,
        trx.state()->hasHint(transaction::Hints::Hint::FROM_TOPLEVEL_AQL) &&
            options.canDisableIndexing);

    // build index value (storedValues array will be stored in value if
    // storedValues are used)
    RocksDBValue value = RocksDBValue::VPackIndexValue();
    if (!_storedValuesPaths.empty()) {
      transaction::BuilderLeaser leased(&trx);
      leased->openArray(true);
      for (auto const& it : _storedValuesPaths) {
        VPackSlice s = doc.get(it);
        if (s.isNone()) {
          s = VPackSlice::nullSlice();
        }
        leased->add(s);
      }
      leased->close();
      value = RocksDBValue::VPackIndexValue(leased->slice());
    }

    rocksdb::Status s;
    for (RocksDBKey const& key : elements) {
      if (!isIndexCreation) {
        // banish key in in-memory cache.
        // not necessary during index creation, because nothing
        // will be in the in-memory cache.
        invalidateCacheEntry(::lookupValueFromSlice(key.string()));
      }

      TRI_ASSERT(key.containsLocalDocumentId(documentId));
      s = mthds->PutUntracked(_cf, key, value.string());
      if (!s.ok()) {
        res = rocksutils::convertStatus(s, rocksutils::index);
        break;
      }
    }

    if (res.fail()) {
      addErrorMsg(res, doc.get(StaticStrings::KeyString).copyString());
    } else if (_estimates) {
      auto* state = RocksDBTransactionState::toState(&trx);
      auto* trxc = static_cast<RocksDBTransactionCollection*>(
          state->findCollection(_collection.id()));
      TRI_ASSERT(trxc != nullptr);
      for (uint64_t hash : hashes) {
        trxc->trackIndexInsert(id(), hash);
      }
    }
  }

  return res;
}

namespace {
bool attributesEqual(
    VPackSlice first, VPackSlice second,
    std::vector<arangodb::basics::AttributeName>::const_iterator begin,
    std::vector<arangodb::basics::AttributeName>::const_iterator end) {
  for (; begin != end; ++begin) {
    // check if, after fetching the subattribute, we are point to a non-object.
    // e.g. if the index is on field ["a.b"], the first iteration of this loop
    // will look for subattribute "a" in the original document. this will always
    // work. however, when looking for "b", we have to make sure that "a" was
    // an object. otherwise we must not call Slice::get() on it. In case one of
    // the subattributes we found so far is not an object, we fall back to the
    // regular comparison
    if (!first.isObject() || !second.isObject()) {
      break;
    }

    // fetch subattribute
    first = first.get(begin->name);
    first = first.resolveExternal();
    second = second.get(begin->name);
    second = second.resolveExternal();

    if (begin->shouldExpand && first.isArray() && second.isArray()) {
      if (first.length() != second.length()) {
        // Nonequal length, so there is a difference!
        // We have to play this carefully here. It is possible that the
        // set of values found is the same, but we must err on the side
        // of caution in this case and use the slow path. Note in
        // particular that the following code returns `true`, if one
        // of the arrays is empty, which is not correct!
        return false;
      }
      auto next = begin + 1;
      VPackArrayIterator it1(first), it2(second);
      while (it1.valid() && it2.valid()) {
        if (!attributesEqual(*it1, *it2, next, end)) {
          return false;
        }
        it1++;
        it2++;
      }
      return true;
    }

    auto dist = std::distance(begin, end);
    bool notF1 = first.isNone() || (dist == 1 && !first.isObject());
    bool notF2 = second.isNone() || (dist == 1 && !second.isObject());
    if (notF1 != notF2) {
      return false;
    }
    if (notF1 || notF2) {  // one of the paths was not found
      break;
    }
  }

  return basics::VelocyPackHelper::equal(first, second, true);
}
}  // namespace

Result RocksDBVPackIndex::update(
    transaction::Methods& trx, RocksDBMethods* mthds,
    LocalDocumentId const& oldDocumentId, velocypack::Slice oldDoc,
    LocalDocumentId const& newDocumentId, velocypack::Slice newDoc,
    OperationOptions const& options, bool performChecks) {
  if (!_unique) {
    // only unique index supports in-place updates
    // lets also not handle the complex case of expanded arrays
    return RocksDBIndex::update(trx, mthds, oldDocumentId, oldDoc,
                                newDocumentId, newDoc, options, performChecks);
  }

  if (!std::all_of(_fields.cbegin(), _fields.cend(), [&](auto const& path) {
        return ::attributesEqual(oldDoc, newDoc, path.begin(), path.end());
      })) {
    // change detected in some index attribute value.
    // we can only use in-place updates if no indexed attributes changed
    return RocksDBIndex::update(trx, mthds, oldDocumentId, oldDoc,
                                newDocumentId, newDoc, options, performChecks);
  }

  // update-in-place following...

  Result res;
  containers::SmallVector<RocksDBKey, 4> elements;
  containers::SmallVector<uint64_t, 4> hashes;
  {
    // rethrow all types of exceptions from here...
    transaction::BuilderLeaser leased(&trx);
    auto r = fillElement(*leased, newDocumentId, newDoc, elements, hashes);

    if (r != TRI_ERROR_NO_ERROR) {
      return addErrorMsg(res, r);
    }
  }

  RocksDBValue value = RocksDBValue::UniqueVPackIndexValue(newDocumentId);
  for (auto const& key : elements) {
    // banish key in in-memory cache
    invalidateCacheEntry(::lookupValueFromSlice(key.string()));

    rocksdb::Status s =
        mthds->Put(_cf, key, value.string(), /*assume_tracked*/ false);
    if (!s.ok()) {
      res = rocksutils::convertStatus(s, rocksutils::index);
      addErrorMsg(res, newDoc.get(StaticStrings::KeyString).copyString());
      break;
    }
  }

  return res;
}

/// @brief removes a document from the index
Result RocksDBVPackIndex::remove(transaction::Methods& trx,
                                 RocksDBMethods* mthds,
                                 LocalDocumentId const& documentId,
                                 velocypack::Slice doc) {
  TRI_IF_FAILURE("BreakHashIndexRemove") {
    if (type() == arangodb::Index::IndexType::TRI_IDX_TYPE_HASH_INDEX) {
      // intentionally  break index removal
      return Result(TRI_ERROR_INTERNAL,
                    "BreakHashIndexRemove failure point triggered");
    }
  }
  Result res;
  rocksdb::Status s;
  containers::SmallVector<RocksDBKey, 4> elements;
  containers::SmallVector<uint64_t, 4> hashes;

  {
    // rethrow all types of exceptions from here...
    transaction::BuilderLeaser leased(&trx);
    auto r = fillElement(*leased, documentId, doc, elements, hashes);

    if (r != TRI_ERROR_NO_ERROR) {
      return addErrorMsg(res, r);
    }
  }

  IndexingDisabler guard(
      mthds, !_unique && trx.state()->hasHint(
                             transaction::Hints::Hint::FROM_TOPLEVEL_AQL));

  size_t const count = elements.size();

  if (_unique) {
    for (size_t i = 0; i < count; ++i) {
      // banish key in in-memory cache
      invalidateCacheEntry(::lookupValueFromSlice(elements[i].string()));

      s = mthds->Delete(_cf, elements[i]);

      if (!s.ok()) {
        res.reset(rocksutils::convertStatus(s, rocksutils::index));
      }
    }
    if (res.fail()) {
      TRI_ASSERT(doc.get(StaticStrings::KeyString).isString());
      addErrorMsg(res, doc.get(StaticStrings::KeyString).copyString());
    }
  } else {
    // non-unique index contain the unique objectID written exactly once
    for (size_t i = 0; i < count; ++i) {
      // banish key in in-memory cache
      invalidateCacheEntry(::lookupValueFromSlice(elements[i].string()));

      s = mthds->SingleDelete(_cf, elements[i]);

      if (!s.ok()) {
        res.reset(rocksutils::convertStatus(s, rocksutils::index));
      }
    }

    if (res.fail()) {
      TRI_ASSERT(doc.get(StaticStrings::KeyString).isString());
      addErrorMsg(res, doc.get(StaticStrings::KeyString).copyString());
    } else if (_estimates) {
      auto* state = RocksDBTransactionState::toState(&trx);
      auto* trxc = static_cast<RocksDBTransactionCollection*>(
          state->findCollection(_collection.id()));
      TRI_ASSERT(trxc != nullptr);
      for (uint64_t hash : hashes) {
        // The estimator is only useful if we are in a non-unique index
        TRI_ASSERT(!_unique);
        trxc->trackIndexRemove(id(), hash);
      }
    }
  }

  return res;
}

// build an index iterator from a VelocyPack range description
std::unique_ptr<IndexIterator> RocksDBVPackIndex::buildIterator(
    transaction::Methods* trx, VPackSlice searchValues,
    IndexIteratorOptions const& opts, ReadOwnWrites readOwnWrites,
    RocksDBVPackIndexSearchValueFormat format) const {
  TRI_ASSERT(searchValues.isArray());
  TRI_ASSERT(format != RocksDBVPackIndexSearchValueFormat::kDetect);

  bool reverse = !opts.ascending;
  bool useCache = opts.useCache;

  VPackArrayIterator it(searchValues);

  if (it.size() == 0) {
    // full range scan over the entire index
    return buildIteratorFromBounds(
        trx, reverse, readOwnWrites,
        _unique ? RocksDBKeyBounds::UniqueVPackIndex(objectId(), reverse)
                : RocksDBKeyBounds::VPackIndex(objectId(), reverse),
        RocksDBVPackIndexSearchValueFormat::kValuesOnly, /*useCache*/ false);
  }

  // check if we only have equality lookups
  transaction::BuilderLeaser leftSearch(trx);

  VPackSlice lastNonEq;
  leftSearch->openArray();
  for (VPackSlice searchValue : it) {
    TRI_ASSERT(searchValue.isObject());
    VPackSlice eq = searchValue.get(StaticStrings::IndexEq);
    if (eq.isNone()) {
      lastNonEq = searchValue;
      break;
    }
    leftSearch->add(eq);
  }

  // all index fields covered by equality lookups.
  bool allEq = lastNonEq.isNone() && it.size() == _fields.size();

  if (_unique && allEq) {
    // unique index and we only have equality lookups
    leftSearch->close();

    return std::make_unique<RocksDBVPackUniqueIndexIterator>(
        &_collection, trx, this, useCache ? _cache : nullptr,
        leftSearch->slice(), readOwnWrites);
  }

  // generic case: we have a non-unique index or have non-equality lookups
  TRI_ASSERT(leftSearch->isOpenArray());

  RocksDBKeyBounds bounds = RocksDBKeyBounds::Empty();
  buildIndexRangeBounds(trx, searchValues, *leftSearch, lastNonEq, bounds);

  return buildIteratorFromBounds(trx, reverse, readOwnWrites, std::move(bounds),
                                 format, /*useCache*/ allEq && useCache);
}

std::unique_ptr<IndexIterator> RocksDBVPackIndex::buildIteratorFromBounds(
    transaction::Methods* trx, bool reverse, ReadOwnWrites readOwnWrites,
    RocksDBKeyBounds&& bounds, RocksDBVPackIndexSearchValueFormat format,
    bool useCache) const {
  TRI_ASSERT(!bounds.empty());
  TRI_ASSERT(format != RocksDBVPackIndexSearchValueFormat::kDetect);

  bool mustCheckBounds =
      RocksDBTransactionState::toState(trx)->iteratorMustCheckBounds(
          _collection.id(), readOwnWrites);

  if (unique()) {
    // unique index
    if (reverse) {
      // reverse version
      if (mustCheckBounds) {
        return std::make_unique<RocksDBVPackIndexIterator<true, true, true>>(
            &_collection, trx, this, std::move(bounds),
            useCache ? _cache : nullptr, readOwnWrites, format);
      }
      return std::make_unique<RocksDBVPackIndexIterator<true, true, false>>(
          &_collection, trx, this, std::move(bounds),
          useCache ? _cache : nullptr, readOwnWrites, format);
    }
    // forward version
    if (mustCheckBounds) {
      return std::make_unique<RocksDBVPackIndexIterator<true, false, true>>(
          &_collection, trx, this, std::move(bounds),
          useCache ? _cache : nullptr, readOwnWrites, format);
    }
    return std::make_unique<RocksDBVPackIndexIterator<true, false, false>>(
        &_collection, trx, this, std::move(bounds), useCache ? _cache : nullptr,
        readOwnWrites, format);
  }

  // non-unique index
  if (reverse) {
    // reverse version
    if (mustCheckBounds) {
      return std::make_unique<RocksDBVPackIndexIterator<false, true, true>>(
          &_collection, trx, this, std::move(bounds),
          useCache ? _cache : nullptr, readOwnWrites, format);
    }
    return std::make_unique<RocksDBVPackIndexIterator<false, true, false>>(
        &_collection, trx, this, std::move(bounds), useCache ? _cache : nullptr,
        readOwnWrites, format);
  }
  // forward version
  if (mustCheckBounds) {
    return std::make_unique<RocksDBVPackIndexIterator<false, false, true>>(
        &_collection, trx, this, std::move(bounds), useCache ? _cache : nullptr,
        readOwnWrites, format);
  }
  return std::make_unique<RocksDBVPackIndexIterator<false, false, false>>(
      &_collection, trx, this, std::move(bounds), useCache ? _cache : nullptr,
      readOwnWrites, format);
}

// build bounds for an index range
void RocksDBVPackIndex::buildIndexRangeBounds(transaction::Methods* trx,
                                              VPackSlice searchValues,
                                              VPackBuilder& leftSearch,
                                              VPackSlice lastNonEq,
                                              RocksDBKeyBounds& bounds) const {
  TRI_ASSERT(searchValues.isArray());
  TRI_ASSERT(searchValues.length() <= _fields.size());
  TRI_ASSERT(leftSearch.isOpenArray());

  bounds.clear();

  // Copy rightSearch = leftSearch for right border
  VPackBuilder rightSearch = leftSearch;

  if (lastNonEq.isNone()) {
    // We only have equality lookups!
    leftSearch.add(VPackSlice::minKeySlice());
    rightSearch.add(VPackSlice::maxKeySlice());
  } else {
    // Define Lower-Bound
    VPackSlice lastLeft = lastNonEq.get(StaticStrings::IndexGe);
    if (!lastLeft.isNone()) {
      TRI_ASSERT(!lastNonEq.hasKey(StaticStrings::IndexGt));
      leftSearch.add(lastLeft);
      leftSearch.add(VPackSlice::minKeySlice());
    } else {
      lastLeft = lastNonEq.get(StaticStrings::IndexGt);
      if (!lastLeft.isNone()) {
        leftSearch.add(lastLeft);
        leftSearch.add(VPackSlice::maxKeySlice());
      } else {
        // No lower bound set default to (null <= x)
        leftSearch.add(VPackSlice::minKeySlice());
      }
    }

    // Define upper-bound
    VPackSlice lastRight = lastNonEq.get(StaticStrings::IndexLe);
    if (!lastRight.isNone()) {
      TRI_ASSERT(!lastNonEq.hasKey(StaticStrings::IndexLt));
      rightSearch.add(lastRight);
      rightSearch.add(VPackSlice::maxKeySlice());
    } else {
      lastRight = lastNonEq.get(StaticStrings::IndexLt);
      if (!lastRight.isNone()) {
        rightSearch.add(lastRight);
        rightSearch.add(VPackSlice::minKeySlice());
      } else {
        // No upper bound set default to (x <= INFINITY)
        rightSearch.add(VPackSlice::maxKeySlice());
      }
    }
  }

  leftSearch.close();
  rightSearch.close();

  bounds.fill(_unique ? RocksDBEntryType::UniqueVPackIndexValue
                      : RocksDBEntryType::VPackIndexValue,
              objectId(), leftSearch.slice(), rightSearch.slice());
}

Index::FilterCosts RocksDBVPackIndex::supportsFilterCondition(
    std::vector<std::shared_ptr<arangodb::Index>> const& allIndexes,
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, size_t itemsInIndex) const {
  return SortedIndexAttributeMatcher::supportsFilterCondition(
      allIndexes, this, node, reference, itemsInIndex);
}

Index::SortCosts RocksDBVPackIndex::supportsSortCondition(
    arangodb::aql::SortCondition const* sortCondition,
    arangodb::aql::Variable const* reference, size_t itemsInIndex) const {
  return SortedIndexAttributeMatcher::supportsSortCondition(
      this, sortCondition, reference, itemsInIndex);
}

/// @brief specializes the condition for use with the index
arangodb::aql::AstNode* RocksDBVPackIndex::specializeCondition(
    arangodb::aql::AstNode* node,
    arangodb::aql::Variable const* reference) const {
  return SortedIndexAttributeMatcher::specializeCondition(this, node,
                                                          reference);
}

std::unique_ptr<IndexIterator> RocksDBVPackIndex::iteratorForCondition(
    transaction::Methods* trx, arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, IndexIteratorOptions const& opts,
    ReadOwnWrites readOwnWrites, int) {
  TRI_ASSERT(!isSorted() || opts.sorted);

  transaction::BuilderLeaser searchValues(trx);
  bool needNormalize = false;
  RocksDBVPackIndexSearchValueFormat format =
      RocksDBVPackIndexSearchValueFormat::kDetect;
  buildSearchValues(node, reference, *searchValues, format, needNormalize);
  // format must have been set to something valid now.
  TRI_ASSERT(format != RocksDBVPackIndexSearchValueFormat::kDetect);

  if (needNormalize) {
    transaction::BuilderLeaser expandedSearchValues(trx);
    expandInSearchValues(searchValues->slice(), *expandedSearchValues);
    VPackSlice expandedSlice = expandedSearchValues->slice();

    VPackArrayIterator values(expandedSlice);
    std::vector<std::unique_ptr<IndexIterator>> iterators;
    iterators.reserve(values.size());
    for (VPackSlice val : values) {
      iterators.emplace_back(
          buildIterator(trx, val, opts, readOwnWrites, format));
    }

    if (!opts.ascending) {
      std::reverse(iterators.begin(), iterators.end());
    }

    return std::make_unique<MultiIndexIterator>(&_collection, trx, this,
                                                std::move(iterators));
  }

  VPackSlice searchSlice = searchValues->slice();
  TRI_ASSERT(searchSlice.length() == 1);
  searchSlice = searchSlice.at(0);
  return buildIterator(trx, searchSlice, opts, readOwnWrites, format);
}

void RocksDBVPackIndex::buildSearchValues(
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, VPackBuilder& searchValues,
    RocksDBVPackIndexSearchValueFormat& format, bool& needNormalize) const {
  searchValues.openArray();
  if (node == nullptr) {
    // We only use this index for sort. Empty searchValue
    VPackArrayBuilder guard(&searchValues);

    TRI_IF_FAILURE("PersistentIndex::noSortIterator") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
    TRI_IF_FAILURE("SkiplistIndex::noSortIterator") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
    TRI_IF_FAILURE("HashIndex::noSortIterator") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
  } else {
    // Create the search values for the lookup
    VPackArrayBuilder guard(&searchValues);

    arangodb::containers::FlatHashMap<
        size_t, std::vector<arangodb::aql::AstNode const*>>
        found;
    arangodb::containers::FlatHashSet<std::string> nonNullAttributes;
    [[maybe_unused]] size_t unused = 0;
    SortedIndexAttributeMatcher::matchAttributes(
        this, node, reference, found, unused, nonNullAttributes, true);

    // this will be reused for multiple invocations of getValueAccess
    std::pair<arangodb::aql::Variable const*,
              std::vector<arangodb::basics::AttributeName>>
        paramPair;

    // found contains all attributes that are relevant for this node.
    // It might be less than fields().
    //
    // Handle the first attributes. They can only be == or IN and only
    // one node per attribute
    auto getValueAccess = [&](arangodb::aql::AstNode const* comp,
                              arangodb::aql::AstNode const*& access,
                              arangodb::aql::AstNode const*& value) -> bool {
      access = comp->getMember(0);
      value = comp->getMember(1);
      if (!(access->isAttributeAccessForVariable(paramPair) &&
            paramPair.first == reference)) {
        access = comp->getMember(1);
        value = comp->getMember(0);
        if (!(access->isAttributeAccessForVariable(paramPair) &&
              paramPair.first == reference)) {
          // Both side do not have a correct AttributeAccess, this should not
          // happen and indicates
          // an error in the optimizer
          TRI_ASSERT(false);
        }
        return true;
      }
      return false;
    };

    size_t usedFields = 0;
    for (; usedFields < _fields.size(); ++usedFields) {
      auto it = found.find(usedFields);
      if (it == found.end()) {
        // We are either done
        // or this is a range.
        // Continue with more complicated loop
        break;
      }

      auto const& comp = it->second[0];
      TRI_ASSERT(comp->numMembers() == 2);
      arangodb::aql::AstNode const* access = nullptr;
      arangodb::aql::AstNode const* value = nullptr;
      getValueAccess(comp, access, value);
      // We found an access for this field

      if (format == RocksDBVPackIndexSearchValueFormat::kValuesOnly) {
        TRI_ASSERT(comp->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ);
        value->toVelocyPackValue(searchValues);
        continue;
      }

      TRI_ASSERT(format != RocksDBVPackIndexSearchValueFormat::kValuesOnly);

      if (comp->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ) {
        searchValues.openObject(true);
        searchValues.add(VPackValue(StaticStrings::IndexEq));
        TRI_IF_FAILURE("PersistentIndex::permutationEQ") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
        TRI_IF_FAILURE("SkiplistIndex::permutationEQ") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
        TRI_IF_FAILURE("HashIndex::permutationEQ") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
      } else if (comp->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN) {
        // complex condition. adjust format!
        format = RocksDBVPackIndexSearchValueFormat::kOperatorsAndValues;

        searchValues.openObject(true);
        if (isAttributeExpanded(usedFields)) {
          searchValues.add(VPackValue(StaticStrings::IndexEq));
          TRI_IF_FAILURE("PersistentIndex::permutationArrayIN") {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
          }
          TRI_IF_FAILURE("SkiplistIndex::permutationArrayIN") {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
          }
          TRI_IF_FAILURE("HashIndex::permutationArrayIN") {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
          }
        } else {
          needNormalize = true;
          searchValues.add(VPackValue(StaticStrings::IndexIn));
        }
      } else {
        // This is a one-sided range
        break;
      }
      // We have to add the value always, the key was added before
      value->toVelocyPackValue(searchValues);
      searchValues.close();
    }

    // Now handle the next element, which might be a range
    if (usedFields < _fields.size()) {
      auto it = found.find(usedFields);

      if (it != found.end()) {
        auto const& rangeConditions = it->second;
        TRI_ASSERT(rangeConditions.size() <= 2);

        VPackObjectBuilder searchElement(&searchValues, true);

        for (auto const& comp : rangeConditions) {
          TRI_ASSERT(comp->numMembers() == 2);
          arangodb::aql::AstNode const* access = nullptr;
          arangodb::aql::AstNode const* value = nullptr;
          bool isReverseOrder = getValueAccess(comp, access, value);

          TRI_ASSERT(format != RocksDBVPackIndexSearchValueFormat::kValuesOnly);

          // complex condition. adjust format!
          format = RocksDBVPackIndexSearchValueFormat::kOperatorsAndValues;

          // Add the key
          switch (comp->type) {
            case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT:
              if (isReverseOrder) {
                searchValues.add(VPackValue(StaticStrings::IndexGt));
              } else {
                searchValues.add(VPackValue(StaticStrings::IndexLt));
              }
              break;
            case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE:
              if (isReverseOrder) {
                searchValues.add(VPackValue(StaticStrings::IndexGe));
              } else {
                searchValues.add(VPackValue(StaticStrings::IndexLe));
              }
              break;
            case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT:
              if (isReverseOrder) {
                searchValues.add(VPackValue(StaticStrings::IndexLt));
              } else {
                searchValues.add(VPackValue(StaticStrings::IndexGt));
              }
              break;
            case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE:
              if (isReverseOrder) {
                searchValues.add(VPackValue(StaticStrings::IndexLe));
              } else {
                searchValues.add(VPackValue(StaticStrings::IndexGe));
              }
              break;
            default:
              // unsupported right now. Should have been rejected by
              // supportsFilterCondition
              THROW_ARANGO_EXCEPTION_MESSAGE(
                  TRI_ERROR_INTERNAL,
                  "unsupported state in RocksDBVPackIndex::buildSearchValues");
          }

          // If the value does not have a vpack representation the index cannot
          // use it, and the results of the query are wrong.
          TRI_ASSERT(value->valueHasVelocyPackRepresentation());
          value->toVelocyPackValue(searchValues);
        }
      }
    }
  }
  searchValues.close();

  TRI_IF_FAILURE("PersistentIndex::noIterator") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  TRI_IF_FAILURE("SkiplistIndex::noIterator") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  TRI_IF_FAILURE("HashIndex::noIterator") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  if (format == RocksDBVPackIndexSearchValueFormat::kDetect) {
    // if we haven't seen any complex condition, we can go with the
    // simple (values only) format!
    format = RocksDBVPackIndexSearchValueFormat::kValuesOnly;
  }
}

void RocksDBVPackIndex::afterTruncate(TRI_voc_tick_t tick,
                                      arangodb::transaction::Methods* trx) {
  if (_estimator != nullptr) {
    _estimator->bufferTruncate(tick);
  }
  RocksDBIndex::afterTruncate(tick, trx);
}

std::shared_ptr<cache::Cache> RocksDBVPackIndex::makeCache() const {
  TRI_ASSERT(_cacheManager != nullptr);
  return _cacheManager->createCache<cache::VPackKeyHasher>(
      cache::CacheType::Transactional);
}

RocksDBCuckooIndexEstimatorType* RocksDBVPackIndex::estimator() {
  return _estimator.get();
}

void RocksDBVPackIndex::setEstimator(
    std::unique_ptr<RocksDBCuckooIndexEstimatorType> est) {
  TRI_ASSERT(!_unique);
  TRI_ASSERT(_estimator == nullptr ||
             _estimator->appliedSeq() <= est->appliedSeq());
  _estimator = std::move(est);
}

void RocksDBVPackIndex::recalculateEstimates() {
  if (unique() || _estimator == nullptr) {
    return;
  }
  TRI_ASSERT(_estimator != nullptr);
  _estimator->clear();

  auto& selector =
      _collection.vocbase().server().getFeature<EngineSelectorFeature>();
  auto& engine = selector.engine<RocksDBEngine>();
  rocksdb::TransactionDB* db = engine.db();
  rocksdb::SequenceNumber seq = db->GetLatestSequenceNumber();

  auto bounds = getBounds();
  rocksdb::Slice const end = bounds.end();
  rocksdb::ReadOptions options;
  options.iterate_upper_bound = &end;   // safe to use on rocksb::DB directly
  options.prefix_same_as_start = true;  // key-prefix includes edge
  options.verify_checksums = false;
  options.fill_cache = false;
  std::unique_ptr<rocksdb::Iterator> it(db->NewIterator(options, _cf));
  for (it->Seek(bounds.start()); it->Valid(); it->Next()) {
    uint64_t hash = RocksDBVPackIndex::HashForKey(it->key());
    // cppcheck-suppress uninitvar ; doesn't understand above call
    _estimator->insert(hash);
  }
  _estimator->setAppliedSeq(seq);
}

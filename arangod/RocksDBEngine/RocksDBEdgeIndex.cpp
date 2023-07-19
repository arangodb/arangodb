////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
#include "Basics/Endian.h"
#include "Basics/Exceptions.h"
#include "Basics/GlobalResourceMonitor.h"
#include "Basics/ResourceUsage.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Cache/BinaryKeyHasher.h"
#include "Cache/CachedValue.h"
#include "Cache/CacheManagerFeature.h"
#include "Cache/TransactionalCache.h"
#include "Indexes/SimpleAttributeEqualityMatcher.h"
#include "Logger/LogMacros.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBCuckooIndexEstimator.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBIndexCacheRefillFeature.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "RocksDBEngine/RocksDBTransactionMethods.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "RocksDBEngine/RocksDBTypes.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Context.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"

#include <rocksdb/db.h>
#include <rocksdb/slice.h>
#include <rocksdb/utilities/transaction_db.h>
#include <rocksdb/utilities/write_batch_with_index.h>

#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

#include <array>
#include <cmath>
#include <tuple>

#include <lz4.h>

using namespace arangodb;
using namespace arangodb::basics;

namespace {
using EdgeIndexCacheType = cache::TransactionalCache<cache::BinaryKeyHasher>;

constexpr bool EdgeIndexFillBlockCache = false;

// values > this size will not be stored LZ4-compressed in the in-memory
// edge cache
constexpr size_t maxValueSizeForCompression = 1073741824;  // 1GB

// length of compressed header.
// the header format is
// - byte 0: hard-coded to 0xff:
// - byte 1-4: uncompressed size, encoded as a uint32_t in big endian order
constexpr size_t compressedHeaderLength = 5;

size_t resizeCompressionBuffer(std::string& scratch, size_t value) {
  if (scratch.size() < value) {
    size_t increment = value - scratch.size();
    scratch.resize(value);
    // return memory usage increase
    return increment;
  }
  return 0;
}

std::tuple<char const*, size_t, bool> tryCompress(
    velocypack::Slice slice, size_t size, size_t minValueSizeForCompression,
    int accelerationFactor, std::string& scratch,
    std::function<void(size_t)> const& memoryUsageCallback) {
  char const* data = slice.startAs<char>();

  if (size < minValueSizeForCompression || size > maxValueSizeForCompression) {
    // value too big or too small. return original value
    return {data, size, /*didCompress*/ false};
  }

  // determine maximum size for output buffer
  int maxLength = LZ4_compressBound(static_cast<int>(size));
  if (maxLength <= 0) {
    // error. return original value
    return {data, size, /*didCompress*/ false};
  }

  // resize output buffer if necessary
  size_t memoryUsage = resizeCompressionBuffer(
      scratch, compressedHeaderLength + static_cast<size_t>(maxLength));
  memoryUsageCallback(memoryUsage);

  uint32_t uncompressedSize = basics::hostToBig(static_cast<uint32_t>(size));
  // store compressed value using a velocypack Custom type.
  // the compressed value is prepended by the following header:
  // - byte 0: hard-coded to 0xff
  // - byte 1-4: uncompressed size, encoded as a uint32_t in big endian
  // order

  // write prefix byte, so the decoder can distinguish between compressed
  // values and uncompressed values. note that 0xff is a velocypack Custom
  // type and should thus not appear as the first byte in uncompressed
  // data.
  scratch[0] = 0xffU;
  // copy length of uncompressed data into bytes 1-4.
  memcpy(scratch.data() + 1, &uncompressedSize, sizeof(uncompressedSize));

  bool didCompress = false;

  // compress data into output buffer, starting at byte 5.
  int compressedSize = LZ4_compress_fast(
      data, scratch.data() + compressedHeaderLength, static_cast<int>(size),
      static_cast<int>(scratch.size() - compressedHeaderLength),
      accelerationFactor);
  if (compressedSize > 0 && (static_cast<size_t>(compressedSize) +
                             compressedHeaderLength) < size * 3 / 4) {
    // only store the compressed version if it saves at least 25%
    data = scratch.data();
    size = static_cast<size_t>(compressedSize + compressedHeaderLength);
    TRI_ASSERT(reinterpret_cast<uint8_t const*>(data)[0] == 0xffU);
    TRI_ASSERT(size <= scratch.size());
    didCompress = true;
  }
  // return compressed or uncompressed value
  return {data, size, didCompress};
}

template<std::size_t N>
class StringFromParts final : public velocypack::IStringFromParts {
 public:
  template<typename... Args>
  StringFromParts(Args&&... args) noexcept
      : _parts{std::forward<Args>(args)...} {
    static_assert(sizeof...(Args) == N);
  }

  std::size_t numberOfParts() const final { return N; }

  std::size_t totalLength() const final {
    size_t length = 0;
    for (size_t index = 0; index != N; ++index) {
      length += _parts[index].length();
    }
    return length;
  }

  std::string_view operator()(size_t index) const final {
    TRI_ASSERT(index < numberOfParts());
    return _parts[index];
  }

 private:
  std::array<std::string_view, N> _parts;
};

template<typename... Args>
auto makeStringFromParts(Args&&... args) noexcept {
  return StringFromParts<sizeof...(Args)>{std::forward<Args>(args)...};
}

}  // namespace

namespace arangodb {
class RocksDBEdgeIndexLookupIterator final : public IndexIterator {
  friend class RocksDBEdgeIndex;

  // used for covering edge index lookups.
  // holds both the indexed attribute (_from/_to) and the opposite
  // attribute (_to/_from). Avoids copying the data and is thus
  // just an efficient, non-owning container for the _from/_to values
  // of a single edge during covering edge index lookups.
  class EdgeCoveringData final : public IndexIteratorCoveringData {
   public:
    explicit EdgeCoveringData(VPackSlice indexAttribute,
                              VPackSlice otherAttribute) noexcept
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
  RocksDBEdgeIndexLookupIterator(ResourceMonitor& monitor,
                                 LogicalCollection* collection,
                                 transaction::Methods* trx,
                                 RocksDBEdgeIndex const* index,
                                 VPackBuilder&& keys,
                                 std::shared_ptr<cache::Cache> cache,
                                 ReadOwnWrites readOwnWrites)
      : IndexIterator(collection, trx, readOwnWrites),
        _resourceMonitor(monitor),
        _index(index),
        _cache(std::static_pointer_cast<EdgeIndexCacheType>(std::move(cache))),
        _keys(std::move(keys)),
        _keysIterator(_keys.slice()),
        _bounds(RocksDBKeyBounds::EdgeIndex(0)),
        _builderIterator(VPackArrayIterator::Empty{}),
        _lastKey(VPackSlice::nullSlice()),
        _memoryUsage(0),
        _totalCachedSizeInitial(0),
        _totalCachedSizeEffective(0),
        _totalInserts(0),
        _totalCompressed(0) {
    TRI_ASSERT(_keys.slice().isArray());
    TRI_ASSERT(_cache == nullptr || _cache->hasherName() == "BinaryKeyHasher");
    TRI_ASSERT(_builder.size() == 0);

    ResourceUsageScope scope(_resourceMonitor, _keys.size());
    // now we are responsible for tracking memory usage
    _memoryUsage += scope.trackedAndSteal();
  }

  ~RocksDBEdgeIndexLookupIterator() override {
    _resourceMonitor.decreaseMemoryUsage(_memoryUsage);

    // report compression metrics to storage engine
    _collection->vocbase()
        .server()
        .getFeature<EngineSelectorFeature>()
        .engine<RocksDBEngine>()
        .addCacheMetrics(_totalCachedSizeInitial, _totalCachedSizeEffective,
                         _totalInserts, _totalCompressed);
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

    size_t oldMemoryUsage = _keys.size();
    TRI_ASSERT(_memoryUsage >= oldMemoryUsage);
    _resourceMonitor.decreaseMemoryUsage(oldMemoryUsage);
    _memoryUsage -= oldMemoryUsage;

    _keys.clear();
    TRI_ASSERT(_keys.isEmpty());

    if (aap.opType == aql::NODE_TYPE_OPERATOR_BINARY_EQ) {
      // a.b == value
      _index->fillLookupValue(_keys, aap.value);
      _keysIterator = VPackArrayIterator(_keys.slice());

      size_t newMemoryUsage = _keys.size();
      _resourceMonitor.increaseMemoryUsage(newMemoryUsage);
      _memoryUsage += newMemoryUsage;
      return true;
    }

    if (aap.opType == aql::NODE_TYPE_OPERATOR_BINARY_IN &&
        aap.value->isArray()) {
      // a.b IN values
      _index->fillInLookupValues(_trx, _keys, aap.value);
      _keysIterator = VPackArrayIterator(_keys.slice());

      size_t newMemoryUsage = _keys.size();
      _resourceMonitor.increaseMemoryUsage(newMemoryUsage);
      _memoryUsage += newMemoryUsage;

      return true;
    }

    // a.b IN non-array or operator type unsupported
    return false;
  }

 private:
  void resetInplaceMemory() {
    size_t memoryUsage = _builder.size();
    TRI_ASSERT(_memoryUsage >= memoryUsage);
    _resourceMonitor.decreaseMemoryUsage(memoryUsage);
    _memoryUsage -= memoryUsage;

    _builder.clear();
  }

  // internal retrieval loop
  template<typename F>
  inline bool nextImplementation(F&& cb, uint64_t limit) {
    TRI_ASSERT(_trx->state()->isRunning());
    TRI_ASSERT(limit > 0);
    if (limit == 0) {
      // gracefully return in production mode. nothing bad has happened
      return false;
    }

    std::string const* cacheKeyCollection = nullptr;
    std::string const* cacheValueCollection = nullptr;

    auto handleSingleResult = [&, this]() {
      TRI_ASSERT(_builderIterator.value().isNumber());
      LocalDocumentId docId{
          _builderIterator.value().getNumericValue<uint64_t>()};
      _builderIterator.next();
      TRI_ASSERT(_builderIterator.valid());
      // For now we store the complete opposite _from/_to value
      VPackSlice fromTo = _builderIterator.value();
      TRI_ASSERT(fromTo.isString());

      std::string_view v = fromTo.stringView();
      TRI_ASSERT(!v.empty());
      if (v.front() == '/') {
        // prefix-compressed value
        cacheValueCollection =
            _index->cachedValueCollection(cacheValueCollection);
        TRI_ASSERT(cacheValueCollection != nullptr);
        _idBuilder.clear();
        // collection name  and  key including forward slash
        _idBuilder.add(::makeStringFromParts(*cacheValueCollection, v));
        fromTo = _idBuilder.slice();
      }

      std::forward<F>(cb)(docId, fromTo);
    };

    while (limit > 0) {
      while (_builderIterator.valid()) {
        // we still have unreturned edges in out memory. simply return them
        handleSingleResult();
        _builderIterator.next();
        --limit;

        if (limit == 0) {
          // Limit reached. bail out
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
      std::string_view cacheKey;

      bool needRocksLookup = true;
      if (_cache) {
        // try to read from cache

        // build actual cache lookup key. this is either the value of fromTo,
        // or a suffix of it in case fromTo starts with the cached collection
        // name, and an empty string_view if the lookup value is invalid.
        cacheKey = _index->buildCompressedCacheKey(cacheKeyCollection, fromTo);

        if (!cacheKey.empty()) {
          TRI_ASSERT(cacheKey == fromTo || fromTo.ends_with(cacheKey));
          // only look up a key in the cache if the key is syntactially valid.
          // this is important because we can only tell syntactically valid keys
          // apart from prefix-compressed keys.
          auto finding = _cache->find(cacheKey.data(),
                                      static_cast<uint32_t>(cacheKey.size()));

          if (finding.found()) {
            incrCacheHits();
            needRocksLookup = false;
            // We got sth. in the cache
            uint8_t const* data = finding.value()->value();
            if (data[0] == 0xffU) {
              // Custom type. this means the data is lz4-compressed
              TRI_ASSERT(finding.value()->size() >= ::compressedHeaderLength);
              // read size of uncompressed value
              uint32_t uncompressedSize;
              memcpy(&uncompressedSize, data + 1, sizeof(uncompressedSize));
              uncompressedSize = basics::bigToHost<uint32_t>(uncompressedSize);

              // prepare _builder's Buffer to hold the uncompressed data
              resetInplaceMemory();
              TRI_ASSERT(_builder.slice().isNone());
              _builder.reserve(uncompressedSize);
              TRI_ASSERT(_builder.bufferRef().data() == _builder.start());

              // uncompressed directly into _builder's Buffer.
              // this should not go wrong if we have a big enough output buffer.
              int size = LZ4_decompress_safe(
                  reinterpret_cast<char const*>(data) +
                      ::compressedHeaderLength,
                  reinterpret_cast<char*>(_builder.bufferRef().data()),
                  static_cast<int>(finding.value()->valueSize() -
                                   ::compressedHeaderLength),
                  static_cast<int>(uncompressedSize));
              TRI_ASSERT(uncompressedSize == static_cast<size_t>(size));

              // we have uncompressed data directly into the Buffer's memory.
              // we need to tell the Buffer to advance its end pointer.
              _builder.resetTo(uncompressedSize);

              size_t memoryUsage = _builder.size();
              _resourceMonitor.increaseMemoryUsage(memoryUsage);
              _memoryUsage += memoryUsage;

              TRI_ASSERT(_builder.slice().isArray());
              _builderIterator = VPackArrayIterator(_builder.slice());
            } else {
              VPackSlice cachedData(data);
              TRI_ASSERT(cachedData.isArray());
              VPackArrayIterator cachedIterator(cachedData);
              TRI_ASSERT(cachedIterator.size() % 2 == 0);
              if (cachedIterator.size() / 2 < limit) {
                // Directly return it, no need to copy
                _builderIterator = cachedIterator;
                while (_builderIterator.valid()) {
                  handleSingleResult();
                  _builderIterator.next();
                  TRI_ASSERT(limit > 0);
                  --limit;
                }
                _builderIterator =
                    VPackArrayIterator(VPackArrayIterator::Empty{});
              } else {
                // We need to copy it.
                // And then we just get back to beginning of the loop
                resetInplaceMemory();
                _builder.add(cachedData);

                size_t memoryUsage = _builder.size();
                _resourceMonitor.increaseMemoryUsage(memoryUsage);
                _memoryUsage += memoryUsage;

                TRI_ASSERT(_builder.slice().isArray());
                _builderIterator = VPackArrayIterator(_builder.slice());
                // Do not set limit
              }
            }
          } else {
            incrCacheMisses();
          }
        }
      }  // if (_cache)

      if (needRocksLookup) {
        lookupInRocksDB(fromTo, cacheKey);
      }

      _keysIterator.next();
    }
    TRI_ASSERT(limit == 0);
    return _builderIterator.valid() || _keysIterator.valid();
  }

  void lookupInRocksDB(std::string_view fromTo, std::string_view cacheKey) {
    // slow case: read from RocksDB
    auto* mthds = RocksDBTransactionState::toMethods(_trx, _collection->id());

    std::string const* cacheValueCollection = nullptr;

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
    ResourceUsageScope scope(_resourceMonitor, expectedIteratorMemoryUsage);

    {
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
        LocalDocumentId const docId =
            RocksDBKey::edgeDocumentId(iterator->key());

        // adding documentId and _from or _to value
        _builder.add(VPackValue(docId.id()));
        std::string_view vertexId = RocksDBValue::vertexId(iterator->value());

        // construct a potentially prefix-compressed vertex id from the original
        // _from/_to value
        std::string_view cacheValue =
            _index->buildCompressedCacheValue(cacheValueCollection, vertexId);
        TRI_ASSERT(!cacheValue.empty() && vertexId.ends_with(cacheValue));
        _builder.add(VPackValue(cacheValue));
      }
      _builder.close();

      // validate that Iterator is in a good shape and hasn't failed
      rocksutils::checkIteratorStatus(*iterator);
    }
    // iterator not needed anymore
    scope.decrease(expectedIteratorMemoryUsage);

    scope.increase(_builder.size());
    // now we are responsible for tracking the memory usage
    _memoryUsage += scope.trackedAndSteal();

    if (_cache != nullptr) {
      // key to store value under in the cache. we will use cacheKey is set, and
      // fromTo otherwise.
      std::string_view key = cacheKey.empty() ? fromTo : cacheKey;
      // the value we store in the cache may be an empty array or a non-empty
      // one. we don't care and cache both.
      insertIntoCache(key, _builder.slice());
    }
    TRI_ASSERT(_builder.slice().isArray());
    _builderIterator = VPackArrayIterator(_builder.slice());
  }

  void insertIntoCache(std::string_view key, velocypack::Slice slice) {
    TRI_ASSERT(_cache != nullptr);

    auto& cmf =
        _collection->vocbase().server().getFeature<CacheManagerFeature>();

    // values >= this size will be stored LZ4-compressed in the in-memory
    // edge cache
    size_t const minValueSizeForCompression =
        cmf.minValueSizeForEdgeCompression();
    int const accelerationFactor = cmf.accelerationFactorForEdgeCompression();

    size_t const originalSize = slice.byteSize();

    auto [data, size, didCompress] = tryCompress(
        slice, originalSize, minValueSizeForCompression, accelerationFactor,
        _lz4CompressBuffer, [this](size_t memoryUsage) {
          _resourceMonitor.increaseMemoryUsage(memoryUsage);
          _memoryUsage += memoryUsage;
        });

    cache::Cache::SimpleInserter<EdgeIndexCacheType>{
        static_cast<EdgeIndexCacheType&>(*_cache), key.data(),
        static_cast<uint32_t>(key.size()), data, static_cast<uint64_t>(size)};

    _totalCachedSizeInitial +=
        cache::kCachedValueHeaderSize + key.size() + originalSize;
    _totalCachedSizeEffective +=
        cache::kCachedValueHeaderSize + key.size() + size;
    ++_totalInserts;
    _totalCompressed += didCompress ? 1 : 0;
  }

  // expected number of bytes that a RocksDB iterator will use.
  // this is a guess and does not need to be fully accurate.
  static constexpr size_t expectedIteratorMemoryUsage = 8192;

  ResourceMonitor& _resourceMonitor;
  RocksDBEdgeIndex const* _index;

  std::shared_ptr<EdgeIndexCacheType> _cache;
  velocypack::Builder _keys;
  velocypack::ArrayIterator _keysIterator;

  RocksDBKeyBounds _bounds;
  // used to build temporary _from/_to values from compressed values
  velocypack::Builder _idBuilder;

  // the following values are required for correct batch handling
  velocypack::Builder _builder;
  velocypack::ArrayIterator _builderIterator;
  velocypack::Slice _lastKey;

  // reusable buffer for lz4 compression
  std::string _lz4CompressBuffer;
  size_t _memoryUsage;

  // total size of uncompressed values stored in the cache
  uint64_t _totalCachedSizeInitial;
  // total size of values stored in the cache (compressed/uncompressed)
  uint64_t _totalCachedSizeEffective;
  // total cache inserts
  uint64_t _totalInserts;
  // total cache compressions
  uint64_t _totalCompressed;
};

}  // namespace arangodb

// ============================= Index ====================================

uint64_t RocksDBEdgeIndex::HashForKey(rocksdb::Slice key) {
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
      _forceCacheRefill(collection.vocbase()
                            .server()
                            .getFeature<RocksDBIndexCacheRefillFeature>()
                            .autoRefill()),
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
                                OperationOptions const& options,
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

  Result res;
  rocksdb::Status s = mthd->PutUntracked(_cf, key.ref(), value.string());

  if (s.ok()) {
    std::hash<std::string_view> hasher;
    uint64_t hash = static_cast<uint64_t>(hasher(fromToRef));
    RocksDBTransactionState::toState(&trx)->trackIndexInsert(_collection.id(),
                                                             id(), hash);

    handleCacheInvalidation(trx, options, fromToRef);
  } else {
    res.reset(rocksutils::convertStatus(s));
    addErrorMsg(res);
  }

  return res;
}

Result RocksDBEdgeIndex::remove(transaction::Methods& trx, RocksDBMethods* mthd,
                                LocalDocumentId const& documentId,
                                velocypack::Slice doc,
                                OperationOptions const& options) {
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

  Result res;
  rocksdb::Status s = mthd->Delete(_cf, key.ref());

  if (s.ok()) {
    std::hash<std::string_view> hasher;
    uint64_t hash = static_cast<uint64_t>(hasher(fromToRef));
    RocksDBTransactionState::toState(&trx)->trackIndexRemove(_collection.id(),
                                                             id(), hash);
    handleCacheInvalidation(trx, options, fromToRef);
  } else {
    res.reset(rocksutils::convertStatus(s));
    addErrorMsg(res);
  }

  return res;
}

void RocksDBEdgeIndex::refillCache(transaction::Methods& trx,
                                   std::vector<std::string> const& keys) {
  if (_cache == nullptr || keys.empty()) {
    return;
  }

  VPackBuilder keysBuilder;

  // intentionally empty
  keysBuilder.add(VPackSlice::emptyArraySlice());

  ResourceMonitor monitor(GlobalResourceMonitor::instance());
  RocksDBEdgeIndexLookupIterator it(monitor, &_collection, &trx, this,
                                    std::move(keysBuilder), _cache,
                                    ReadOwnWrites::no);

  std::string const* cacheKeyCollection = nullptr;

  for (auto const& key : keys) {
    std::string_view fromTo = {key.data(), key.size()};
    std::string_view cacheKey =
        buildCompressedCacheKey(cacheKeyCollection, fromTo);
    TRI_ASSERT(cacheKey.empty() || cacheKey == fromTo ||
               fromTo.ends_with(cacheKey));
    it.lookupInRocksDB(fromTo, cacheKey);
  }
}

void RocksDBEdgeIndex::handleCacheInvalidation(transaction::Methods& trx,
                                               OperationOptions const& options,
                                               std::string_view fromToRef) {
  // always invalidate cache entry for all edges with same _from / _to

  // adjust key for potential prefix compression
  std::string const* cacheKeyCollection = nullptr;
  std::string_view cacheKey =
      buildCompressedCacheKey(cacheKeyCollection, fromToRef);
  if (!cacheKey.empty()) {
    invalidateCacheEntry(cacheKey);

    if (_cache != nullptr &&
        ((_forceCacheRefill &&
          options.refillIndexCaches != RefillIndexCaches::kDontRefill) ||
         options.refillIndexCaches == RefillIndexCaches::kRefill)) {
      RocksDBTransactionState::toState(&trx)->trackIndexCacheRefill(
          _collection.id(), id(), fromToRef);
    }
  }
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
    ResourceMonitor& monitor, transaction::Methods* trx,
    aql::AstNode const* node, aql::Variable const* reference,
    IndexIteratorOptions const& opts, ReadOwnWrites readOwnWrites, int) {
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
    return createEqIterator(monitor, trx, aap.attribute, aap.value,
                            opts.useCache, readOwnWrites);
  }
  if (aap.opType == aql::NODE_TYPE_OPERATOR_BINARY_IN) {
    // "in"-checks never needs to observe own writes
    TRI_ASSERT(readOwnWrites == ReadOwnWrites::no);
    // a.b IN values
    if (aap.value->isArray()) {
      return createInIterator(monitor, trx, aap.attribute, aap.value,
                              opts.useCache);
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

Result RocksDBEdgeIndex::warmup() {
  if (!hasCache()) {
    return {};
  }

  auto ctx = transaction::StandaloneContext::Create(_collection.vocbase());
  SingleCollectionTransaction trx(ctx, _collection, AccessMode::Type::READ);
  Result res = trx.begin();

  if (res.fail()) {
    return res;
  }

  auto rocksColl = toRocksDBCollection(_collection);
  // Prepare the cache to be resized for this amount of objects to be inserted.
  uint64_t expectedCount = rocksColl->meta().numberDocuments();
  expectedCount = static_cast<uint64_t>(expectedCount * selectivityEstimate());
  _cache->sizeHint(expectedCount);

  auto bounds = RocksDBKeyBounds::EdgeIndex(objectId());

  warmupInternal(&trx, bounds.start(), bounds.end());

  return trx.commit();
}

void RocksDBEdgeIndex::warmupInternal(transaction::Methods* trx,
                                      rocksdb::Slice lower,
                                      rocksdb::Slice upper) {
  cache::Cache* cc = _cache.get();
  TRI_ASSERT(cc != nullptr);

  auto& cmf = _collection.vocbase().server().getFeature<CacheManagerFeature>();

  size_t const minValueSizeForCompression =
      cmf.minValueSizeForEdgeCompression();
  int const accelerationFactor = cmf.accelerationFactorForEdgeCompression();

  auto rocksColl = toRocksDBCollection(_collection);

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

  bool needsInsert = false;
  std::string previous;
  VPackBuilder builder;
  velocypack::Builder docBuilder;
  std::string lz4CompressBuffer;
  std::string const* cacheKeyCollection = nullptr;
  std::string_view cacheKey;
  // byte size of values attempted to store in cache (before compression)
  uint64_t totalCachedSizeInitial = 0;
  // byte size of values effectively stored in cache (after compression)
  uint64_t totalCachedSizeEffective = 0;
  // total cache inserts
  uint64_t totalInserts = 0;
  // total cache compressions
  uint64_t totalCompressed = 0;

  auto storeInCache = [&](velocypack::Builder& b, std::string_view cacheKey) {
    builder.close();

    size_t const originalSize = b.slice().byteSize();

    auto [data, size, didCompress] = tryCompress(
        b.slice(), originalSize, minValueSizeForCompression, accelerationFactor,
        lz4CompressBuffer, [](size_t /*memoryUsage*/) {});

    cache::Cache::SimpleInserter<EdgeIndexCacheType>{
        static_cast<EdgeIndexCacheType&>(*cc), cacheKey.data(),
        static_cast<uint32_t>(cacheKey.size()), data,
        static_cast<uint64_t>(size)};
    builder.clear();

    totalCachedSizeInitial +=
        cache::kCachedValueHeaderSize + cacheKey.size() + originalSize;
    totalCachedSizeEffective +=
        cache::kCachedValueHeaderSize + cacheKey.size() + size;
    ++totalInserts;
    totalCompressed += didCompress ? 1 : 0;
  };

  size_t n = 0;
  for (it->Seek(lower); it->Valid(); it->Next()) {
    ++n;
    if (n % 1024 == 0) {
      if (collection().vocbase().server().isStopping()) {
        // periodically check for server shutdown
        THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
      }
      if (collection().vocbase().isDropped()) {
        // database was dropped
        THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
      }
      if (collection().deleted()) {
        // collection was dropped
        THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
      }
    }

    rocksdb::Slice key = it->key();
    std::string_view v = RocksDBKey::vertexId(key);
    if (previous.empty()) {
      // First call.
      builder.clear();
      previous = v;

      // build cache lookup key (can be prefix-compressed)
      cacheKey = buildCompressedCacheKey(cacheKeyCollection, previous);
      // the lookup keys we get from RocksDB *must* be valid
      TRI_ASSERT(!cacheKey.empty() && previous.ends_with(cacheKey));

      bool shouldTry = true;
      while (shouldTry) {
        auto finding =
            cc->find(cacheKey.data(), static_cast<uint32_t>(cacheKey.size()));
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
        storeInCache(builder, cacheKey);
        TRI_ASSERT(builder.isEmpty());
      }
      // Need to store
      previous = v;
      // build cache lookup key (can be prefix-compressed)
      cacheKey = buildCompressedCacheKey(cacheKeyCollection, previous);
      // the lookup keys we get from RocksDB *must* be valid
      TRI_ASSERT(!cacheKey.empty() && previous.ends_with(cacheKey));

      auto finding =
          cc->find(cacheKey.data(), static_cast<uint32_t>(cacheKey.size()));
      if (finding.found()) {
        needsInsert = false;
      } else {
        needsInsert = true;
        builder.openArray(true);
      }
    }
    if (needsInsert) {
      docBuilder.clear();
      LocalDocumentId const docId = RocksDBKey::edgeDocumentId(key);
      // warmup does not need to observe own writes
      if (rocksColl
              ->lookupDocument(*trx, docId, docBuilder, /*readCache*/ true,
                               /*fillCache*/ true, ReadOwnWrites::no)
              .fail()) {
        // Data Inconsistency. revision id without a document...
        TRI_ASSERT(false);
        continue;
      }

      builder.add(VPackValue(docId.id()));
      VPackSlice toFrom =
          _isFromIndex
              ? transaction::helpers::extractToFromDocument(docBuilder.slice())
              : transaction::helpers::extractFromFromDocument(
                    docBuilder.slice());
      TRI_ASSERT(toFrom.isString());
      builder.add(toFrom);
    }
  }

  if (!previous.empty() && needsInsert) {
    // We still have something to store
    TRI_ASSERT(!cacheKey.empty() && previous.ends_with(cacheKey));
    storeInCache(builder, cacheKey);
  }

  LOG_TOPIC("99a29", DEBUG, Logger::ENGINES) << "loaded n: " << n;

  // report compression metrics to storage engine
  _collection.vocbase()
      .server()
      .getFeature<EngineSelectorFeature>()
      .engine<RocksDBEngine>()
      .addCacheMetrics(totalCachedSizeInitial, totalCachedSizeEffective,
                       totalInserts, totalCompressed);
}

// ===================== Helpers ==================

/// @brief create the iterator
std::unique_ptr<IndexIterator> RocksDBEdgeIndex::createEqIterator(
    ResourceMonitor& monitor, transaction::Methods* trx,
    aql::AstNode const* /*attrNode*/, aql::AstNode const* valNode,
    bool useCache, ReadOwnWrites readOwnWrites) const {
  VPackBuilder keys;
  fillLookupValue(keys, valNode);
  return std::make_unique<RocksDBEdgeIndexLookupIterator>(
      monitor, &_collection, trx, this, std::move(keys),
      useCache ? _cache : nullptr, readOwnWrites);
}

/// @brief create the iterator
std::unique_ptr<IndexIterator> RocksDBEdgeIndex::createInIterator(
    ResourceMonitor& monitor, transaction::Methods* trx,
    aql::AstNode const* /*attrNode*/, aql::AstNode const* valNode,
    bool useCache) const {
  VPackBuilder keys;
  fillInLookupValues(trx, keys, valNode);
  // "in"-checks never need to observe own writes.
  return std::make_unique<RocksDBEdgeIndexLookupIterator>(
      monitor, &_collection, trx, this, std::move(keys),
      useCache ? _cache : nullptr, ReadOwnWrites::no);
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

void RocksDBEdgeIndex::truncateCommit(TruncateGuard&& guard,
                                      TRI_voc_tick_t tick,
                                      transaction::Methods* trx) {
  TRI_ASSERT(!unique());
  if (_estimator != nullptr) {
    _estimator->bufferTruncate(tick);
  }
  RocksDBIndex::truncateCommit(std::move(guard), tick, trx);
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

std::string_view RocksDBEdgeIndex::buildCompressedCacheKey(
    std::string const*& previous, std::string_view value) const {
  return _cacheKeyCollectionName.buildCompressedValue(previous, value);
}

std::string_view RocksDBEdgeIndex::buildCompressedCacheValue(
    std::string const*& previous, std::string_view value) const {
  return _cacheValueCollectionName.buildCompressedValue(previous, value);
}

std::string const* RocksDBEdgeIndex::cachedValueCollection(
    std::string const*& previous) const noexcept {
  if (previous == nullptr) {
    previous = _cacheValueCollectionName.get();
  }
  TRI_ASSERT(previous != nullptr);
  return previous;
}

RocksDBEdgeIndex::CachedCollectionName::CachedCollectionName() noexcept
    : _name(nullptr) {}

RocksDBEdgeIndex::CachedCollectionName::~CachedCollectionName() {
  delete get();
}

std::string_view RocksDBEdgeIndex::CachedCollectionName::buildCompressedValue(
    std::string const*& previous, std::string_view value) const {
  // split lookup value to determine collection name and key parts
  auto pos = value.find('/');

  if (pos == std::string_view::npos || pos == 0 || pos + 1 == value.size() ||
      value[pos + 1] == '/') {
    // totally invalid lookup value
    return {};
  }

  if (previous == nullptr) {
    // no context yet. now try looking up cached collection name
    previous = _name.load(std::memory_order_acquire);
    if (previous == nullptr) {
      // no cached collection name yet. now try to store the collection name
      // we determined ourselves. create a string with the collection name on
      // the heap
      auto cn = std::make_unique<std::string const>(value.data(), pos);
      // try to store the name. this can race with other threads.
      if (_name.compare_exchange_strong(previous, cn.get(),
                                        std::memory_order_release,
                                        std::memory_order_acquire)) {
        // we won the race and were able to store our value. now we are owning
        // the collection name.
        previous = cn.release();
      }
    }
  }

  // here we must have a collection name
  TRI_ASSERT(previous != nullptr);

  // now check if the collection name in the value we got matches the cached
  // name.
  TRI_ASSERT(!previous->empty());
  // must have at least 'c/k'
  TRI_ASSERT(value.size() > 2);
  if (*previous == value.substr(0, pos)) {
    // match. now return the remainder of the value, including the `/` at the
    // front.
    value = value.substr(pos);
    // must have at least '/k'
    TRI_ASSERT(value.size() > 1);
    TRI_ASSERT(value.starts_with('/'));
    // cannot have '//...'
    TRI_ASSERT(value[1] != '/');
  }

  return value;
}

std::string const* RocksDBEdgeIndex::CachedCollectionName::get()
    const noexcept {
  return _name.load(std::memory_order_acquire);
}

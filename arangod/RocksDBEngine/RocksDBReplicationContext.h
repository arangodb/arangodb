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
/// @author Daniel H. Larkin
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGO_ROCKSDB_ROCKSDB_REPLICATION_CONTEXT_H
#define ARANGO_ROCKSDB_ROCKSDB_REPLICATION_CONTEXT_H 1

#include "Basics/Common.h"
#include "Basics/Mutex.h"
#include "Indexes/IndexIterator.h"
#include "Replication/SyncerId.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "RocksDBEngine/RocksDBReplicationCommon.h"
#include "Transaction/Methods.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/Identifiers/DataSourceId.h"
#include "VocBase/Identifiers/ServerId.h"
#include "VocBase/vocbase.h"

#include <rocksdb/options.h>

#include <velocypack/Buffer.h>
#include <velocypack/Builder.h>
#include <velocypack/Options.h>
#include <velocypack/Slice.h>

namespace rocksdb {
class Comparator;
class Iterator;
class Snapshot;
}  // namespace rocksdb

namespace arangodb {
class LogicalCollection;
class RocksDBEngine;

namespace basics {
class StringBuffer;
}

class RocksDBReplicationContext {
 private:
  typedef std::function<void(LocalDocumentId const& token)> LocalDocumentIdCallback;

  /// collection abstraction
  struct CollectionIterator {
    CollectionIterator(TRI_vocbase_t&, std::shared_ptr<LogicalCollection> const&,
                       bool sorted, rocksdb::Snapshot const*);
    ~CollectionIterator();

    TRI_vocbase_t& vocbase;
    std::shared_ptr<LogicalCollection> logical;

    /// Iterator over primary index or documents
    std::unique_ptr<rocksdb::Iterator> iter;
    /// bounds used by the iterator
    RocksDBKeyBounds bounds;
    /// some incrementing number
    uint64_t currentTick;
    /// @brief offset in the collection used with the incremental sync
    uint64_t lastSortedIteratorOffset;

    arangodb::velocypack::Options vpackOptions;

    /// @brief number of documents in this collection
    /// only set in a very specific use-case
    uint64_t numberDocuments;
     /// @brief number of documents we iterated over in /dump
    uint64_t numberDocumentsDumped;
    /// @brief snapshot and number documents were fetched exclusively
    bool isNumberDocumentsExclusive;

    rocksdb::ReadOptions const& readOptions() const { return _readOptions; }
    bool sorted() const { return _sortedIterator; }
    void setSorted(bool);

    void use() noexcept {
      TRI_ASSERT(!isUsed());
      _isUsed.store(true, std::memory_order_release);
    }
    bool isUsed() const { return _isUsed.load(std::memory_order_acquire); }
    void release() noexcept { _isUsed.store(false, std::memory_order_release); }

    // iterator convenience functions
    bool hasMore() const;
    bool outOfRange() const;
    uint64_t skipKeys(uint64_t toSkip);
    void resetToStart();

   private:
    CollectionNameResolver _resolver;
    /// @brief type handler used to render documents
    std::unique_ptr<arangodb::velocypack::CustomTypeHandler> _cTypeHandler;
    /// @brief read options for iterators
    rocksdb::ReadOptions _readOptions;
    /// @brief upper limit for iterate_upper_bound
    rocksdb::Slice _upperLimit;
    rocksdb::Comparator const* _cmp;
    /// no one is allowed to use this concurrently
    std::atomic<bool> _isUsed;
    /// primary-index sorted iterator
    bool _sortedIterator;
  };

 public:
  RocksDBReplicationContext(RocksDBReplicationContext const&) = delete;
  RocksDBReplicationContext& operator=(RocksDBReplicationContext const&) = delete;

  RocksDBReplicationContext(RocksDBEngine&, double ttl, SyncerId syncerId, ServerId clientId);
  ~RocksDBReplicationContext();

  TRI_voc_tick_t id() const;  // batchId
  rocksdb::Snapshot const* snapshot();
  uint64_t snapshotTick();

  /// invalidate all iterators with that vocbase
  void removeVocbase(TRI_vocbase_t&);
  /// invalidate all iterators with that collection
  bool removeCollection(LogicalCollection&);

  /// remove matching iterator
  void releaseIterators(TRI_vocbase_t&, DataSourceId);

  std::tuple<Result, DataSourceId, uint64_t> bindCollectionIncremental(
      TRI_vocbase_t& vocbase, std::string const& cname);

  // returns inventory
  Result getInventory(TRI_vocbase_t& vocbase, bool includeSystem,
                      bool includeFoxxQueues, bool global,
                      velocypack::Builder&);

  void setPatchCount(std::string const& patchCount);
  std::string const& patchCount() const;

  // ========================= Dump API =============================

  struct DumpResult {
    explicit DumpResult(int res) : hasMore(false), includedTick(0), _result(res) {}
    DumpResult(int res, bool hm, uint64_t tick)
        : hasMore(hm), includedTick(tick), _result(res) {}
    bool hasMore;
    uint64_t includedTick;  // tick increases for each fetch

    // forwarded methods
    bool ok() const { return _result.ok(); }
    bool fail() const { return _result.fail(); }
    int errorNumber() const { return _result.errorNumber(); }
    std::string errorMessage() const { return _result.errorMessage(); }
    bool is(int code) const { return _result.is(code); }

    // access methods
    Result const& result() const& { return _result; }
    Result result() && { return std::move(_result); }

   private:
    Result _result;
  };

  // iterates over at most 'limit' documents in the collection specified,
  // creating a new iterator if one does not exist for this collection
  DumpResult dumpJson(TRI_vocbase_t& vocbase, std::string const& cname,
                      basics::StringBuffer&, uint64_t chunkSize,
                      bool useEnvelope);

  // iterates over at most 'limit' documents in the collection specified,
  // creating a new iterator if one does not exist for this collection
  DumpResult dumpVPack(TRI_vocbase_t& vocbase, std::string const& cname,
                       velocypack::Buffer<uint8_t>& buffer, uint64_t chunkSize,
                       bool useEnvelope);

  // ==================== Incremental Sync ===========================

  // iterates over all documents in a collection, previously bound with
  // bindCollection. Generates array of objects with minKey, maxKey and hash
  // per chunk. Distance between min and maxKey should be chunkSize
  arangodb::Result dumpKeyChunks(TRI_vocbase_t& vocbase, DataSourceId cid,
                                 velocypack::Builder& outBuilder, uint64_t chunkSize);
  /// dump all keys from collection
  arangodb::Result dumpKeys(TRI_vocbase_t& vocbase, DataSourceId cid,
                            velocypack::Builder& outBuilder, size_t chunk,
                            size_t chunkSize, std::string const& lowKey);
  /// dump keys and document
  arangodb::Result dumpDocuments(TRI_vocbase_t& vocbase, DataSourceId cid,
                                 velocypack::Builder& b, size_t chunk,
                                 size_t chunkSize, size_t offsetInChunk,
                                 size_t maxChunkSize, std::string const& lowKey,
                                 velocypack::Slice const& ids);

  // lifetime in seconds
  double expires() const;
  bool isDeleted() const;
  void setDeleted();
  bool isUsed() const;
  /// set use flag and extend lifetime
  void use(double ttl);
  /// remove use flag
  void release();
  /// extend lifetime without using the context
  void extendLifetime(double ttl);

  SyncerId syncerId() const {
    return _syncerId;
  }

  ServerId replicationClientServerId() const { return _clientId; }

  std::string const& clientInfo() const {
    return _clientInfo;
  }

 private:
  void lazyCreateSnapshot();

  CollectionIterator* getCollectionIterator(TRI_vocbase_t& vocbase, DataSourceId cid,
                                            bool sorted, bool allowCreate);

  void releaseDumpIterator(CollectionIterator*);

 private:
  RocksDBEngine& _engine;
  TRI_voc_tick_t const _id;  // batch id
  mutable Mutex _contextLock;
  SyncerId const _syncerId;
  ServerId const _clientId;
  std::string const _clientInfo;

  /// @brief collection for which we are allowed to patch counts. this can
  /// be empty, meaning that the counts should not be patched for any collection.
  /// if this is set to the name of any collection/shard, it is expected that the
  /// context will only be used for exactly one collection/shard.
  std::string _patchCount;

  uint64_t _snapshotTick;  // tick in WAL from _snapshot
  rocksdb::Snapshot const* _snapshot;
  std::map<DataSourceId, std::unique_ptr<CollectionIterator>> _iterators;

  double const _ttl;
  /// @brief expiration time, updated under lock by ReplicationManager
  double _expires;

  /// @brief true if context is deleted, updated under lock by
  /// ReplicationManager
  bool _isDeleted;
  /// @brief number of concurrent users, updated under lock by
  /// ReplicationManager
  size_t _users;
};

}  // namespace arangodb

#endif

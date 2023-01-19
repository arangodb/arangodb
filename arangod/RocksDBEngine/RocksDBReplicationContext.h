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
/// @author Daniel H. Larkin
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Common.h"
#include "Basics/Mutex.h"
#include "Containers/MerkleTree.h"
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
#include <rocksdb/snapshot.h>
#include <rocksdb/types.h>

#include <velocypack/Buffer.h>
#include <velocypack/Builder.h>
#include <velocypack/Options.h>
#include <velocypack/Slice.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>

namespace rocksdb {
class Comparator;
class Iterator;
}  // namespace rocksdb

namespace arangodb {
class LogicalCollection;
class RocksDBEngine;

namespace basics {
class StringBuffer;
}

class RocksDBReplicationContext {
 private:
  /// collection abstraction
  struct CollectionIterator {
    CollectionIterator(CollectionIterator const&) = delete;
    CollectionIterator& operator=(CollectionIterator const&) = delete;

    CollectionIterator(TRI_vocbase_t&,
                       std::shared_ptr<LogicalCollection> const&, bool sorted,
                       std::shared_ptr<rocksdb::ManagedSnapshot> snapshot);
    ~CollectionIterator();

    TRI_vocbase_t& vocbase;
    std::shared_ptr<LogicalCollection> logical;

    /// Iterator over primary index or documents
    std::unique_ptr<rocksdb::Iterator> iter;

    std::shared_ptr<rocksdb::ManagedSnapshot> _snapshot;

    /// bounds used by the iterator
    RocksDBKeyBounds _bounds;
    /// some incrementing number
    uint64_t currentTick;
    /// @brief offset in the collection used with the incremental sync
    uint64_t lastSortedIteratorOffset;

    velocypack::Options vpackOptions;

    /// @brief number of documents in this collection
    /// only set in a very specific use-case
    uint64_t numberDocuments;
    /// @brief number of documents we iterated over in /dump
    uint64_t numberDocumentsDumped;
    /// @brief if > 0, then snapshot and number documents were fetched
    /// exclusively. this ticket is also later used to update the collection
    /// count on the leader if it is considered to be wrong.
    uint64_t documentCountAdjustmentTicket;

    RocksDBKeyBounds const& bounds() const noexcept { return _bounds; }
    rocksdb::ReadOptions const& readOptions() const { return _readOptions; }
    bool sorted() const noexcept { return _sortedIterator; }
    void setSorted(bool value);

    void use() noexcept {
      TRI_ASSERT(!isUsed());
      _isUsed.store(true, std::memory_order_release);
    }
    bool isUsed() const noexcept {
      return _isUsed.load(std::memory_order_acquire);
    }
    void release() noexcept { _isUsed.store(false, std::memory_order_release); }

    // iterator convenience functions
    bool hasMore() const;
    bool outOfRange() const;
    uint64_t skipKeys(uint64_t toSkip);
    void resetToStart();

   private:
    CollectionNameResolver _resolver;
    /// @brief type handler used to render documents
    std::unique_ptr<velocypack::CustomTypeHandler> _cTypeHandler;
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
  RocksDBReplicationContext& operator=(RocksDBReplicationContext const&) =
      delete;

  RocksDBReplicationContext(RocksDBEngine&, double ttl, SyncerId syncerId,
                            ServerId clientId);
  ~RocksDBReplicationContext();

  TRI_voc_tick_t id() const;  // batchId
  std::shared_ptr<rocksdb::ManagedSnapshot> snapshot() const;
  uint64_t snapshotTick();

  /// invalidate all iterators with that vocbase
  [[nodiscard]] bool containsVocbase(TRI_vocbase_t&);
  /// invalidate all iterators with that collection
  [[nodiscard]] bool containsCollection(LogicalCollection&) const;

  /// remove matching iterator
  void releaseIterators(TRI_vocbase_t&, DataSourceId);

  std::tuple<Result, DataSourceId, uint64_t> bindCollectionIncremental(
      TRI_vocbase_t& vocbase, std::string const& cname);

  // returns inventory
  Result getInventory(TRI_vocbase_t& vocbase, bool includeSystem,
                      bool includeFoxxQueues, bool global,
                      velocypack::Builder&);

  // returns inventory for a single shard (DB server only!)
  Result getInventory(TRI_vocbase_t& vocbase, std::string const& collectionName,
                      velocypack::Builder&);

  void setPatchCount(std::string const& patchCount);
  std::string const& patchCount() const;

  // ========================= Dump API =============================

  struct DumpResult {
    explicit DumpResult(ErrorCode res)
        : hasMore(false), includedTick(0), _result(res) {}
    DumpResult(ErrorCode res, bool hm, uint64_t tick)
        : hasMore(hm), includedTick(tick), _result(res) {}
    bool hasMore;
    uint64_t includedTick;  // tick increases for each fetch

    // forwarded methods
    bool ok() const noexcept { return _result.ok(); }
    bool fail() const noexcept { return _result.fail(); }
    ErrorCode errorNumber() const noexcept { return _result.errorNumber(); }
    std::string_view errorMessage() const { return _result.errorMessage(); }
    bool is(ErrorCode code) const noexcept { return _result.is(code); }

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
                       bool useEnvelope, bool singleArray);

  // ==================== Incremental Sync ===========================

  // iterates over all documents in a collection, previously bound with
  // bindCollection. Generates array of objects with minKey, maxKey and hash
  // per chunk. Distance between min and maxKey should be chunkSize
  Result dumpKeyChunks(TRI_vocbase_t& vocbase, DataSourceId cid,
                       velocypack::Builder& outBuilder, uint64_t chunkSize);
  /// dump all keys from collection
  Result dumpKeys(TRI_vocbase_t& vocbase, DataSourceId cid,
                  velocypack::Builder& outBuilder, size_t chunk,
                  size_t chunkSize, std::string const& lowKey);
  /// dump keys and document
  Result dumpDocuments(TRI_vocbase_t& vocbase, DataSourceId cid,
                       velocypack::Builder& b, size_t chunk, size_t chunkSize,
                       size_t offsetInChunk, size_t maxChunkSize,
                       std::string const& lowKey, velocypack::Slice const& ids);

  // lifetime in seconds
  double expires() const;

  /// extend lifetime of the context
  void extendLifetime(double ttl = -1.0);

  SyncerId syncerId() const { return _syncerId; }

  ServerId replicationClientServerId() const { return _clientId; }

  std::string const& clientInfo() const { return _clientInfo; }

  void removeBlocker(std::string const& dbName, std::string const& collection);

  // Ask the context to get the RevisionTree for the specified collectionName
  // in the vocbase. If this is done successfully, one can (once only!) get
  // the tree using `getPrefetchedRevisionTree` below.
  std::unique_ptr<containers::RevisionTree> getPrefetchedRevisionTree(
      std::string const& collectionName);

 private:
  template<typename T>
  bool findCollection(
      std::string const& dbName, T const& collection,
      std::function<void(TRI_vocbase_t& vocbase,
                         LogicalCollection& collection)> const& cb);

  void lazyCreateSnapshot();

  CollectionIterator* getCollectionIterator(TRI_vocbase_t& vocbase,
                                            DataSourceId cid, bool sorted,
                                            bool allowCreate);

  void releaseDumpIterator(CollectionIterator*);

  void handleCollectionCountAdjustment(uint64_t documentCountAdjustmentTicket,
                                       int64_t adjustment,
                                       rocksdb::SequenceNumber blockerSeq,
                                       CollectionIterator* cIter);

  RocksDBEngine& _engine;
  TRI_voc_tick_t const _id;  // batch id
  mutable Mutex _contextLock;
  SyncerId const _syncerId;
  ServerId const _clientId;
  std::string const _clientInfo;

  /// @brief collection for which we are allowed to patch counts. this can
  /// be empty, meaning that the counts should not be patched for any
  /// collection. if this is set to the name of any collection/shard, it is
  /// expected that the context will only be used for exactly one
  /// collection/shard.
  std::string _patchCount;

  uint64_t _snapshotTick;  // tick in WAL from _snapshot
  std::shared_ptr<rocksdb::ManagedSnapshot> _snapshot;
  std::map<DataSourceId, std::unique_ptr<CollectionIterator>> _iterators;

  // db name => { collection id => transaction id }
  std::map<std::string, std::map<DataSourceId, uint64_t>> _blockers;

  double const _ttl;
  /// @brief expiration time, updated under lock by ReplicationManager
  double _expires;

  /// A context can hold a single revision tree for some collection, if it
  /// is told so. This is used during `SynchronizeShard` to make sure the
  /// leader actually has the revision tree available for the snapshot
  /// in the context. This happens in `bindCollectionIncremental`. The value
  /// of the string is a guid, that is a globally unique name of the shard.
  std::string _collectionGuidOfPrefetchedRevisionTree;
  std::unique_ptr<containers::RevisionTree> _prefetchedRevisionTree;
};

}  // namespace arangodb

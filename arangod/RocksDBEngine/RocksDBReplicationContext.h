////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGO_ROCKSDB_ROCKSDB_REPLICATION_CONTEXT_H
#define ARANGO_ROCKSDB_ROCKSDB_REPLICATION_CONTEXT_H 1

#include "Basics/Common.h"
#include "Basics/Mutex.h"
#include "Indexes/IndexIterator.h"
#include "RocksDBEngine/RocksDBIterators.h"
#include "RocksDBEngine/RocksDBReplicationCommon.h"
#include "Transaction/Methods.h"
#include "Utils/DatabaseGuard.h"
#include "VocBase/vocbase.h"

#include <velocypack/Buffer.h>
#include <velocypack/Builder.h>
#include <velocypack/Options.h>
#include <velocypack/Slice.h>

namespace arangodb {
class DatabaseGuard;

class RocksDBReplicationContext {
 private:
  typedef std::function<void(LocalDocumentId const& token)>
      LocalDocumentIdCallback;

  struct CollectionIterator {
    CollectionIterator(LogicalCollection&, transaction::Methods&, bool);
    LogicalCollection& logical;

    /// Iterator over primary index or documents
    std::unique_ptr<RocksDBGenericIterator> iter;

    uint64_t currentTick;
    std::atomic<bool> isUsed;
    bool hasMore;

    /// @brief type handler used to render documents
    std::shared_ptr<arangodb::velocypack::CustomTypeHandler> customTypeHandler;
    arangodb::velocypack::Options vpackOptions;

    bool sorted() const { return _sortedIterator; }
    void setSorted(bool, transaction::Methods*);

    void release();

  private:
    /// primary-index sorted iterator
    bool _sortedIterator;
  };

 public:
  RocksDBReplicationContext(RocksDBReplicationContext const&) = delete;
  RocksDBReplicationContext& operator=(RocksDBReplicationContext const&) = delete;

  RocksDBReplicationContext(TRI_vocbase_t* vocbase, double ttl, TRI_server_id_t server_id);
  ~RocksDBReplicationContext();

  TRI_voc_tick_t id() const; //batchId
  uint64_t lastTick() const;
  uint64_t count() const;

  TRI_vocbase_t* vocbase() const;

  // creates new transaction/snapshot
  void bind(TRI_vocbase_t& vocbase);

  /// Bind collection for incremental sync
  int bindCollectionIncremental(
    TRI_vocbase_t& vocbase,
    std::string const& collectionIdentifier
  );
  int chooseDatabase(TRI_vocbase_t& vocbase);

  // returns inventory
  Result getInventory(TRI_vocbase_t* vocbase, bool includeSystem,
                      bool global, velocypack::Builder&);

  // ========================= Dump API =============================

  // iterates over at most 'limit' documents in the collection specified,
  // creating a new iterator if one does not exist for this collection
  RocksDBReplicationResult dumpJson(TRI_vocbase_t* vocbase, std::string const& cname,
                                    basics::StringBuffer&, uint64_t chunkSize);

  // iterates over at most 'limit' documents in the collection specified,
  // creating a new iterator if one does not exist for this collection
  RocksDBReplicationResult dumpVPack(TRI_vocbase_t* vocbase, std::string const& cname,
                                     velocypack::Buffer<uint8_t>& buffer, uint64_t chunkSize);

  bool moreForDump(std::string const& collectionIdentifier);

  // ==================== Incremental Sync ===========================

  // iterates over all documents in a collection, previously bound with
  // bindCollection. Generates array of objects with minKey, maxKey and hash
  // per chunk. Distance between min and maxKey should be chunkSize
  arangodb::Result dumpKeyChunks(velocypack::Builder& outBuilder,
                                 uint64_t chunkSize);

  /// dump all keys from collection
  arangodb::Result dumpKeys(velocypack::Builder& outBuilder, size_t chunk,
                            size_t chunkSize, std::string const& lowKey);
  /// dump keys and document
  arangodb::Result dumpDocuments(velocypack::Builder& b, size_t chunk,
                                 size_t chunkSize, size_t offsetInChunk, size_t maxChunkSize,
                                 std::string const& lowKey, velocypack::Slice const& ids);

  // lifetime in seconds
  double expires() const;
  bool isDeleted() const;
  void setDeleted();
  bool isUsed() const;
  /// set use flag and extend lifetime
  bool use(double ttl, bool exclusive);
  /// remove use flag
  void release();
  /// extend lifetime without using the context
  void extendLifetime(double ttl);
  
  // buggy clients may not send the serverId
  TRI_server_id_t replicationClientId() const {
    return _serverId != 0 ? _serverId : _id;
  }

 private:
  
  void releaseDumpingResources();
  CollectionIterator* getCollectionIterator(TRI_voc_cid_t cid, bool sorted);
  void internalBind(TRI_vocbase_t& vocbase, bool allowChange = true);

  mutable Mutex _contextLock;
  TRI_vocbase_t* _vocbase;
  TRI_server_id_t const _serverId;
  TRI_voc_tick_t const _id; // batch id
  
  uint64_t _lastTick; // the time at which the snapshot was taken
  std::unique_ptr<DatabaseGuard> _guard;
  std::unique_ptr<transaction::Methods> _trx;
  std::unordered_map<TRI_voc_cid_t, std::unique_ptr<CollectionIterator>> _iterators;

  /// @brief bound collection iterator for single-threaded methods
  CollectionIterator* _collection;

  /// @brief offset in the collection used with the incremental sync
  uint64_t _lastIteratorOffset;
  double const _ttl;
  /// @brief expiration time, updated under lock by ReplicationManager
  double _expires;
  
  /// @brief true if context is deleted, updated under lock by ReplicationManager
  bool _isDeleted;
  /// @brief true if context cannot be used concurrently, updated under lock by ReplicationManager
  bool _exclusive;
  /// @brief number of concurrent users, updated under lock by ReplicationManager
  size_t _users;
};

}  // namespace arangodb

#endif

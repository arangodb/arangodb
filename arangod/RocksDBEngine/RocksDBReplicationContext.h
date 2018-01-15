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
#include "Indexes/IndexIterator.h"
#include "RocksDBEngine/RocksDBReplicationCommon.h"
#include "Transaction/Methods.h"
#include "Utils/DatabaseGuard.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Options.h>
#include <velocypack/Slice.h>

namespace arangodb {
class DatabaseGuard;

class RocksDBReplicationContext {
 public:
  /// default time-to-live for contexts
  static double const DefaultTTL;

 private:
  typedef std::function<void(LocalDocumentId const& token)>
      LocalDocumentIdCallback;

  struct CollectionIterator {
    LogicalCollection* logical;
    std::unique_ptr<IndexIterator> iter;
    bool isUsed;
    bool hasMore;

    bool isUsed() const;
    void use(double ttl);
    void release();

  private:
    double _expires;
  };

 public:
  RocksDBReplicationContext(RocksDBReplicationContext const&) = delete;
  RocksDBReplicationContext& operator=(RocksDBReplicationContext const&) = delete;

  explicit RocksDBReplicationContext(double ttl);
  ~RocksDBReplicationContext();

  TRI_voc_tick_t id() const; //batchId
  uint64_t lastTick() const;
  uint64_t count() const;

  TRI_vocbase_t* vocbase() const {
    if (!_guard) {
      return nullptr;
    }
    return _guard->database();
  }

  // creates new transaction/snapshot
  void bind(TRI_vocbase_t*);
  int bindCollection(TRI_vocbase_t*, std::string const& collectionIdentifier);

  // returns inventory
  std::pair<RocksDBReplicationResult, std::shared_ptr<velocypack::Builder>>
  getInventory(TRI_vocbase_t* vocbase, bool includeSystem, bool global);

  // iterates over at most 'limit' documents in the collection specified,
  // creating a new iterator if one does not exist for this collection
  RocksDBReplicationResult dump(TRI_vocbase_t* vocbase,
                                std::string const& collectionName,
                                basics::StringBuffer&, uint64_t chunkSize);

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

  double expires() const;
  bool isDeleted() const;
  void deleted();
  bool isUsed() const;
  void use(double ttl);
  /// remove use flag
  void release();

 private:
  void releaseDumpingResources();
  CollectionIterator* getCollectionIterator(TRI_voc_cid_t id);

 private:
  TRI_voc_tick_t _id; // batch id
  uint64_t _lastTick; // the time at which the snapshot was taken
  std::atomic<uint64_t> _currentTick; // shows how often dump was called
  std::unique_ptr<DatabaseGuard> _guard;
  std::unique_ptr<transaction::Methods> _trx;
  std::unordered_map<TRI_voc_cid_t, std::unique_ptr<CollectionIterator>> _iterators;
  Mutex _iteratorsLock;

  /// @brief bound collection iterator for single-threaded methods
  CollectionIterator* _collection;

  /// @brief offset in the collection used with the incremental sync
  uint64_t _lastIteratorOffset;

  /// @brief holds last document
  ManagedDocumentResult _mdr;
  /// @brief type handler used to render documents
  std::shared_ptr<arangodb::velocypack::CustomTypeHandler> _customTypeHandler;
  arangodb::velocypack::Options _vpackOptions;

  double _expires;
  bool _isDeleted;
  bool _isUsed;
};

}  // namespace arangodb

#endif

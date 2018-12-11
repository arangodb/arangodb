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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_ROCKSDB_ENGINE_ROCKSDB_COLLECTION_H
#define ARANGOD_ROCKSDB_ENGINE_ROCKSDB_COLLECTION_H 1

#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBCollectionMeta.h"
#include "StorageEngine/PhysicalCollection.h"
#include "VocBase/LogicalCollection.h"

namespace rocksdb {
class Transaction;
}

namespace arangodb {
namespace cache {
class Cache;
}
class LogicalCollection;
class ManagedDocumentResult;
class Result;
class RocksDBPrimaryIndex;
class RocksDBVPackIndex;
class LocalDocumentId;

class RocksDBCollection final : public PhysicalCollection {
  friend class RocksDBEngine;
  friend class RocksDBFulltextIndex;
  friend class RocksDBVPackIndex;

  constexpr static double defaultLockTimeout = 10.0 * 60.0;

 public:
  explicit RocksDBCollection(
    LogicalCollection& collection,
    arangodb::velocypack::Slice const& info
  );
  RocksDBCollection(LogicalCollection& collection, PhysicalCollection const*);  // use in cluster only!!!!!

  ~RocksDBCollection();

  std::string const& path() const override;
  void setPath(std::string const& path) override;

  arangodb::Result updateProperties(VPackSlice const& slice,
                                    bool doSync) override;
  virtual arangodb::Result persistProperties() override;

  virtual PhysicalCollection* clone(LogicalCollection& logical) const override;

  /// @brief export properties
  void getPropertiesVPack(velocypack::Builder&) const override;

  /// @brief closes an open collection
  int close() override;
  void load() override;
  void unload() override;

  TRI_voc_rid_t revision() const;
  TRI_voc_rid_t revision(arangodb::transaction::Methods* trx) const override;
  uint64_t numberDocuments() const;
  uint64_t numberDocuments(transaction::Methods* trx) const override;

  /// @brief report extra memory used by indexes etc.
  size_t memory() const override;
  void open(bool ignoreErrors) override;

  ////////////////////////////////////
  // -- SECTION Indexes --
  ///////////////////////////////////

  void prepareIndexes(arangodb::velocypack::Slice indexesSlice) override;

  /// @brief Find index by definition
  std::shared_ptr<Index> lookupIndex(velocypack::Slice const&) const override;

  std::shared_ptr<Index> createIndex(arangodb::velocypack::Slice const& info,
                                     bool restore, bool& created) override;

  /// @brief Drop an index with the given iid.
  bool dropIndex(TRI_idx_iid_t iid) override;
  std::unique_ptr<IndexIterator> getAllIterator(transaction::Methods* trx) const override;
  std::unique_ptr<IndexIterator> getAnyIterator(
      transaction::Methods* trx) const override;

  void invokeOnAllElements(
      transaction::Methods* trx,
      std::function<bool(LocalDocumentId const&)> callback) override;

  ////////////////////////////////////
  // -- SECTION DML Operations --
  ///////////////////////////////////

  Result truncate(
    transaction::Methods& trx,
    OperationOptions& options
  ) override;

  void deferDropCollection(
    std::function<bool(LogicalCollection&)> const& callback
  ) override;

  LocalDocumentId lookupKey(transaction::Methods* trx,
                            velocypack::Slice const& key) const override;

  bool lookupRevision(transaction::Methods* trx,
                      velocypack::Slice const& key,
                      TRI_voc_rid_t& revisionId) const;

  Result read(transaction::Methods*, arangodb::StringRef const& key,
              ManagedDocumentResult& result, bool) override;

  Result read(transaction::Methods* trx, arangodb::velocypack::Slice const& key,
              ManagedDocumentResult& result, bool locked) override {
    if (!key.isString()) {
      return Result(TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD);
    }
    return this->read(trx, arangodb::StringRef(key), result, locked);
  }

  bool readDocument(transaction::Methods* trx,
                    LocalDocumentId const& token,
                    ManagedDocumentResult& result) const override;

  bool readDocumentWithCallback(
      transaction::Methods* trx, LocalDocumentId const& token,
      IndexIterator::DocumentCallback const& cb) const override;

  Result insert(arangodb::transaction::Methods* trx,
                arangodb::velocypack::Slice newSlice,
                arangodb::ManagedDocumentResult& result,
                OperationOptions& options, TRI_voc_tick_t& resultMarkerTick,
                bool lock, TRI_voc_tick_t& revisionId,
                KeyLockInfo* /*keyLockInfo*/,
                std::function<Result(void)> callbackDuringLock) override;

  Result update(arangodb::transaction::Methods* trx,
                arangodb::velocypack::Slice newSlice,
                ManagedDocumentResult& result, OperationOptions& options,
                TRI_voc_tick_t& resultMarkerTick, bool lock,
                TRI_voc_rid_t& prevRev, ManagedDocumentResult& previous,
                arangodb::velocypack::Slice key,
                std::function<Result(void)> callbackDuringLock) override;

  Result replace(transaction::Methods* trx,
                 arangodb::velocypack::Slice newSlice,
                 ManagedDocumentResult& result, OperationOptions& options,
                 TRI_voc_tick_t& resultMarkerTick, bool lock,
                 TRI_voc_rid_t& prevRev, ManagedDocumentResult& previous,
                 std::function<Result(void)> callbackDuringLock) override;

  Result remove(
    transaction::Methods& trx,
    velocypack::Slice slice,
    ManagedDocumentResult& previous,
    OperationOptions& options,
    TRI_voc_tick_t& resultMarkerTick,
    bool lock,
    TRI_voc_rid_t& prevRev,
    TRI_voc_rid_t& revisionId,
    KeyLockInfo* keyLockInfo,
    std::function<Result(void)> callbackDuringLock
  ) override;

  /// adjust the current number of docs
  void adjustNumberDocuments(TRI_voc_rid_t revisionId, int64_t adjustment);
  /// load the number of docs from storage
  void loadInitialNumberDocuments();
  
  uint64_t objectId() const { return _objectId; }

  int lockWrite(double timeout = 0.0);
  int unlockWrite();
  int lockRead(double timeout = 0.0);
  int unlockRead();

  /// recalculte counts for collection in case of failure
  uint64_t recalculateCounts();

  /// trigger rocksdb compaction for documentDB and indexes
  void compact();
  void estimateSize(velocypack::Builder& builder);

  inline bool cacheEnabled() const { return _cacheEnabled; }
  
  RocksDBCollectionMeta& meta() { return _meta; }

 private:
  /// @brief track the usage of waitForSync option in an operation
  void trackWaitForSync(arangodb::transaction::Methods* trx, OperationOptions& options);

  /// @brief return engine-specific figures
  void figuresSpecific(std::shared_ptr<velocypack::Builder>&) override;
  void addIndex(std::shared_ptr<arangodb::Index> idx);

  arangodb::Result fillIndexes(
    transaction::Methods& trx,
    std::shared_ptr<arangodb::Index> indexes
  );

  // @brief return the primary index
  // WARNING: Make sure that this instance
  // is somehow protected. If it goes out of all scopes
  // or it's indexes are freed the pointer returned will get invalidated.
  arangodb::RocksDBPrimaryIndex* primaryIndex() const {
    TRI_ASSERT(_primaryIndex != nullptr);
    return _primaryIndex;
  }

  arangodb::Result insertDocument(
      arangodb::transaction::Methods* trx, LocalDocumentId const& documentId,
      arangodb::velocypack::Slice const& doc, OperationOptions& options) const;

  arangodb::Result removeDocument(
      arangodb::transaction::Methods* trx, LocalDocumentId const& documentId,
      arangodb::velocypack::Slice const& doc, OperationOptions& options) const;

  arangodb::Result updateDocument(
      transaction::Methods* trx, LocalDocumentId const& oldDocumentId,
      arangodb::velocypack::Slice const& oldDoc,
      LocalDocumentId const& newDocumentId,
      arangodb::velocypack::Slice const& newDoc, OperationOptions& options) const;

  arangodb::Result lookupDocumentVPack(LocalDocumentId const& documentId,
                                       transaction::Methods*,
                                       arangodb::ManagedDocumentResult&,
                                       bool withCache) const;

  arangodb::Result lookupDocumentVPack(
      LocalDocumentId const& documentId, transaction::Methods*,
      IndexIterator::DocumentCallback const& cb, bool withCache) const;

  void createCache() const;

  void destroyCache() const;

  /// is this collection using a cache
  inline bool useCache() const noexcept { return (_cacheEnabled && _cachePresent); }

  void blackListKey(char const* data, std::size_t len) const;

 private:
  uint64_t const _objectId;  // rocksdb-specific object id for collection
  RocksDBCollectionMeta _meta; /// collection metadata
  
  std::atomic<uint64_t> _numberDocuments;
  std::atomic<TRI_voc_rid_t> _revisionId;

  /// @brief cached ptr to primary index for performance, never delete
  RocksDBPrimaryIndex* _primaryIndex;
  /// @brief collection lock used for write access
  mutable basics::ReadWriteLock _exclusiveLock;
  /// @brief document cache (optional)
  mutable std::shared_ptr<cache::Cache> _cache;
  // we use this boolean for testing whether _cache is set.
  // it's quicker than accessing the shared_ptr each time
  mutable bool _cachePresent;
  bool _cacheEnabled;
};

inline RocksDBCollection* toRocksDBCollection(PhysicalCollection* physical) {
  auto rv = static_cast<RocksDBCollection*>(physical);
  TRI_ASSERT(rv != nullptr);
  return rv;
}

inline RocksDBCollection* toRocksDBCollection(LogicalCollection& logical) {
  auto phys = logical.getPhysical();
  TRI_ASSERT(phys != nullptr);
  return toRocksDBCollection(phys);
}

}  // namespace arangodb

#endif
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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Statistics/ServerStatistics.h"
#include "RocksDBEngine/RocksDBMetaCollection.h"
#include "VocBase/Identifiers/IndexId.h"

namespace rocksdb {
class PinnableSlice;
class Transaction;
}  // namespace rocksdb

namespace arangodb {
namespace cache {
class Cache;
}

class LogicalCollection;
class ManagedDocumentResult;
class RocksDBPrimaryIndex;
class RocksDBSavePoint;
class RocksDBVPackIndex;
class LocalDocumentId;

class RocksDBCollection final : public RocksDBMetaCollection {
  friend class RocksDBEngine;
  friend class RocksDBFulltextIndex;
  friend class RocksDBVPackIndex;

 public:
  explicit RocksDBCollection(LogicalCollection& collection,
                             arangodb::velocypack::Slice const& info);
  RocksDBCollection(LogicalCollection& collection,
                    PhysicalCollection const*);  // use in cluster only!!!!!

  ~RocksDBCollection();

  arangodb::Result updateProperties(VPackSlice const& slice,
                                    bool doSync) override;

  virtual PhysicalCollection* clone(LogicalCollection& logical) const override;

  /// @brief export properties
  void getPropertiesVPack(velocypack::Builder&) const override;

  /// @brief closes an open collection
  ErrorCode close() override;

  /// return bounds for all documents
  RocksDBKeyBounds bounds() const override;

  ////////////////////////////////////
  // -- SECTION Indexes --
  ///////////////////////////////////

  void prepareIndexes(arangodb::velocypack::Slice indexesSlice) override;

  std::shared_ptr<Index> createIndex(arangodb::velocypack::Slice const& info,
                                     bool restore, bool& created) override;

  /// @brief Drop an index with the given iid.
  bool dropIndex(IndexId iid) override;
  std::unique_ptr<IndexIterator> getAllIterator(
      transaction::Methods* trx, ReadOwnWrites readOwnWrites) const override;
  std::unique_ptr<IndexIterator> getAnyIterator(
      transaction::Methods* trx) const override;

  std::unique_ptr<ReplicationIterator> getReplicationIterator(
      ReplicationIterator::Ordering, uint64_t batchId) override;
  std::unique_ptr<ReplicationIterator> getReplicationIterator(
      ReplicationIterator::Ordering, transaction::Methods&) override;

  ////////////////////////////////////
  // -- SECTION DML Operations --
  ///////////////////////////////////

  Result truncate(transaction::Methods& trx,
                  OperationOptions& options) override;

  /// @brief returns the LocalDocumentId and the revision id for the document
  /// with the specified key.
  Result lookupKey(transaction::Methods* trx, std::string_view key,
                   std::pair<LocalDocumentId, RevisionId>& result,
                   ReadOwnWrites readOwnWrites) const override;

  bool lookupRevision(transaction::Methods* trx, velocypack::Slice const& key,
                      RevisionId& revisionId, ReadOwnWrites) const;

  Result read(transaction::Methods*, std::string_view key,
              IndexIterator::DocumentCallback const& cb,
              ReadOwnWrites readOwnWrites) const override;

  /// @brief lookup with callback, not thread-safe on same transaction::Context
  Result read(transaction::Methods* trx, LocalDocumentId const& token,
              IndexIterator::DocumentCallback const& cb,
              ReadOwnWrites readOwnWrites) const override;

  bool readDocument(transaction::Methods* trx, LocalDocumentId const& token,
                    ManagedDocumentResult& result,
                    ReadOwnWrites readOwnWrites) const override;

  Result insert(arangodb::transaction::Methods* trx,
                arangodb::velocypack::Slice newSlice,
                arangodb::ManagedDocumentResult& resultMdr,
                OperationOptions& options) override;

  Result update(arangodb::transaction::Methods* trx,
                arangodb::velocypack::Slice newSlice,
                ManagedDocumentResult& resultMdr, OperationOptions& options,
                ManagedDocumentResult& previousMdr) override;

  Result replace(transaction::Methods* trx,
                 arangodb::velocypack::Slice newSlice,
                 ManagedDocumentResult& resultMdr, OperationOptions& options,
                 ManagedDocumentResult& previousMdr) override;

  Result remove(transaction::Methods& trx, velocypack::Slice slice,
                ManagedDocumentResult& previous,
                OperationOptions& options) override;

  Result remove(transaction::Methods& trx, LocalDocumentId documentId,
                ManagedDocumentResult& previous,
                OperationOptions& options) override;

  inline bool cacheEnabled() const { return _cacheEnabled; }

  bool hasDocuments() override;

 private:
  [[nodiscard]] Result remove(transaction::Methods& trx,
                              LocalDocumentId documentId,
                              RevisionId expectedRev,
                              ManagedDocumentResult& previous,
                              OperationOptions& options);

  [[nodiscard]] Result performUpdateOrReplace(
      transaction::Methods* trx, velocypack::Slice newSlice,
      ManagedDocumentResult& resultMdr, OperationOptions& options,
      ManagedDocumentResult& previousMdr, bool isUpdate);

  /// @brief return engine-specific figures
  void figuresSpecific(bool details, velocypack::Builder&) override;

  // @brief return the primary index
  // WARNING: Make sure that this instance
  // is somehow protected. If it goes out of all scopes
  // or it's indexes are freed the pointer returned will get invalidated.
  arangodb::RocksDBPrimaryIndex* primaryIndex() const {
    TRI_ASSERT(_primaryIndex != nullptr);
    return _primaryIndex;
  }

  arangodb::Result insertDocument(arangodb::transaction::Methods* trx,
                                  RocksDBSavePoint& savepoint,
                                  LocalDocumentId const& documentId,
                                  arangodb::velocypack::Slice doc,
                                  OperationOptions const& options,
                                  RevisionId const& revisionId) const;

  arangodb::Result removeDocument(arangodb::transaction::Methods* trx,
                                  RocksDBSavePoint& savepoint,
                                  LocalDocumentId const& documentId,
                                  arangodb::velocypack::Slice doc,
                                  OperationOptions const& options,
                                  RevisionId const& revisionId) const;

  arangodb::Result modifyDocument(transaction::Methods* trx,
                                  RocksDBSavePoint& savepoint,
                                  LocalDocumentId const& oldDocumentId,
                                  arangodb::velocypack::Slice const& oldDoc,
                                  LocalDocumentId const& newDocumentId,
                                  arangodb::velocypack::Slice const& newDoc,
                                  OperationOptions& options,
                                  RevisionId const& revisionId) const;

  /// @brief lookup document in cache and / or rocksdb
  /// @param readCache attempt to read from cache
  /// @param fillCache fill cache with found document
  arangodb::Result lookupDocumentVPack(transaction::Methods* trx,
                                       LocalDocumentId const& documentId,
                                       rocksdb::PinnableSlice& ps,
                                       bool readCache, bool fillCache,
                                       ReadOwnWrites readOwnWrites) const;
  Result lookupDocumentVPack(transaction::Methods*,
                             LocalDocumentId const& documentId,
                             IndexIterator::DocumentCallback const& cb,
                             bool withCache, ReadOwnWrites readOwnWrites) const;

  /// @brief create hash-cache
  void createCache() const;
  /// @brief destory hash-cache
  void destroyCache() const;

  /// is this collection using a cache
  inline bool useCache() const noexcept { return (_cacheEnabled && _cache); }

  /// @brief track key in file
  void invalidateCacheEntry(RocksDBKey const& key) const;

  /// @brief can use non transactional range delete in write ahead log
  bool canUseRangeDeleteInWal() const;

 private:
  /// @brief cached ptr to primary index for performance, never delete
  RocksDBPrimaryIndex* _primaryIndex;
  /// @brief document cache (optional)
  mutable std::shared_ptr<cache::Cache> _cache;

  std::atomic<bool> _cacheEnabled;
  /// @brief number of index creations in progress
  std::atomic<int> _numIndexCreations;
  arangodb::TransactionStatistics& _statistics;
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

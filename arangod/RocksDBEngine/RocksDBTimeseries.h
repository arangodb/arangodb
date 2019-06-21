////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_ROCKSDB_ENGINE_ROCKSDB_TIMESERIES_H
#define ARANGOD_ROCKSDB_ENGINE_ROCKSDB_TIMESERIES_H 1

#include "RocksDBEngine/RocksDBMetaCollection.h"

namespace rocksdb {
class PinnableSlice;
class Transaction;
}

namespace arangodb {

class LogicalCollection;
class ManagedDocumentResult;
class Result;
class LocalDocumentId;


class RocksDBTimeseries final : public RocksDBMetaCollection {
  friend class RocksDBEngine;

 public:
  explicit RocksDBTimeseries(LogicalCollection& collection,
                             arangodb::velocypack::Slice const& info);
  RocksDBTimeseries(LogicalCollection& collection,
                    PhysicalCollection const*);  // use in cluster only!!!!!

  ~RocksDBTimeseries();

  arangodb::Result updateProperties(VPackSlice const& slice, bool doSync) override;

  virtual PhysicalCollection* clone(LogicalCollection& logical) const override;

  /// @brief export properties
  void getPropertiesVPack(velocypack::Builder&) const override;

  /// @brief closes an open collection
  int close() override;
  void load() override;
  void unload() override;

  TRI_voc_rid_t revision(arangodb::transaction::Methods* trx) const override;
  uint64_t numberDocuments(transaction::Methods* trx) const override;

  ////////////////////////////////////
  // -- SECTION Indexes --
  ///////////////////////////////////

  void prepareIndexes(arangodb::velocypack::Slice indexesSlice) override;

  std::shared_ptr<Index> createIndex(arangodb::velocypack::Slice const& info,
                                     bool restore, bool& created) override;

  /// @brief Drop an index with the given iid.
  bool dropIndex(TRI_idx_iid_t iid) override;
  std::unique_ptr<IndexIterator> getAllIterator(transaction::Methods* trx) const override;
  std::unique_ptr<IndexIterator> getAnyIterator(transaction::Methods* trx) const override;

  void invokeOnAllElements(transaction::Methods* trx,
                           std::function<bool(LocalDocumentId const&)> callback) override;

  ////////////////////////////////////
  // -- SECTION DML Operations --
  ///////////////////////////////////

  Result truncate(transaction::Methods& trx, OperationOptions& options) override;
  
  /// @brief compact-data operation
  /// triggers rocksdb compaction for documentDB and indexes
  Result compact() override;

  LocalDocumentId lookupKey(transaction::Methods* trx, velocypack::Slice const& key) const override;

  bool lookupRevision(transaction::Methods* trx, velocypack::Slice const& key,
                      TRI_voc_rid_t& revisionId) const;

  Result read(transaction::Methods*, arangodb::velocypack::StringRef const& key,
              ManagedDocumentResult& result, bool) override;

  Result read(transaction::Methods* trx, arangodb::velocypack::Slice const& key,
              ManagedDocumentResult& result, bool locked) override {
    if (!key.isString()) {
      return Result(TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD);
    }
    return this->read(trx, arangodb::velocypack::StringRef(key), result, locked);
  }

  bool readDocument(transaction::Methods* trx, LocalDocumentId const& token,
                    ManagedDocumentResult& result) const override;

  /// @brief lookup with callback, not thread-safe on same transaction::Context
  bool readDocumentWithCallback(transaction::Methods* trx, LocalDocumentId const& token,
                                IndexIterator::DocumentCallback const& cb) const override;

  Result insert(arangodb::transaction::Methods* trx, arangodb::velocypack::Slice newSlice,
                arangodb::ManagedDocumentResult& resultMdr, OperationOptions& options,
                bool lock, KeyLockInfo* /*keyLockInfo*/,
                std::function<void()> const& cbDuringLock) override;

  Result update(arangodb::transaction::Methods* trx, arangodb::velocypack::Slice newSlice,
                ManagedDocumentResult& resultMdr, OperationOptions& options,
                bool lock, ManagedDocumentResult& previousMdr) override;

  Result replace(transaction::Methods* trx, arangodb::velocypack::Slice newSlice,
                 ManagedDocumentResult& resultMdr, OperationOptions& options,
                 bool lock, ManagedDocumentResult& previousMdr) override;

  Result remove(transaction::Methods& trx, velocypack::Slice slice,
                ManagedDocumentResult& previous, OperationOptions& options,
                bool lock, KeyLockInfo* keyLockInfo,
                std::function<void()> const& cbDuringLock) override;

  /// recalculte counts for collection in case of failure
  uint64_t recalculateCounts();

  void estimateSize(velocypack::Builder& builder);


 private:
  /// @brief return engine-specific figures
  void figuresSpecific(std::shared_ptr<velocypack::Builder>&) override;

  arangodb::Result insertDocument(arangodb::transaction::Methods* trx,
                                  LocalDocumentId const& documentId,
                                  arangodb::velocypack::Slice const& doc,
                                  OperationOptions& options) const;

  arangodb::Result removeDocument(arangodb::transaction::Methods* trx,
                                  LocalDocumentId const& documentId,
                                  arangodb::velocypack::Slice const& doc,
                                  OperationOptions& options) const;

  arangodb::Result updateDocument(transaction::Methods* trx, LocalDocumentId const& oldDocumentId,
                                  arangodb::velocypack::Slice const& oldDoc,
                                  LocalDocumentId const& newDocumentId,
                                  arangodb::velocypack::Slice const& newDoc,
                                  OperationOptions& options) const;

  /// @brief lookup document in cache and / or rocksdb
  /// @param readCache attempt to read from cache
  /// @param fillCache fill cache with found document
  arangodb::Result lookupDocumentVPack(transaction::Methods* trx,
                                       LocalDocumentId const& documentId,
                                       rocksdb::PinnableSlice& ps,
                                       bool readCache,
                                       bool fillCache) const;
  
  bool lookupDocumentVPack(transaction::Methods*,
                           LocalDocumentId const& documentId,
                           IndexIterator::DocumentCallback const& cb,
                           bool withCache) const;
};

inline RocksDBTimeseries* toRocksDBTimeseries(PhysicalCollection* physical) {
  auto rv = static_cast<RocksDBTimeseries*>(physical);
  TRI_ASSERT(rv != nullptr);
  return rv;
}

inline RocksDBTimeseries* toRocksDBTimeseries(LogicalCollection& logical) {
  auto phys = logical.getPhysical();
  TRI_ASSERT(phys != nullptr);
  return toRocksDBCollection(phys);
}

}  // namespace arangodb

#endif

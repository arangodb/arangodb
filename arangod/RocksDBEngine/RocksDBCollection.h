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
#include "Indexes/IndexLookupContext.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "StorageEngine/PhysicalCollection.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/ManagedDocumentResult.h"

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
struct RocksDBToken;

class RocksDBCollection final : public PhysicalCollection {
  friend class RocksDBEngine;
  friend class RocksDBVPackIndex;
  friend class RocksDBFulltextIndex;

  constexpr static double defaultLockTimeout = 10.0 * 60.0;

 public:
 public:
  explicit RocksDBCollection(LogicalCollection*, VPackSlice const& info);
  explicit RocksDBCollection(LogicalCollection*,
                             PhysicalCollection*);  // use in cluster only!!!!!

  ~RocksDBCollection();

  std::string const& path() const override;
  void setPath(std::string const& path) override;

  arangodb::Result updateProperties(VPackSlice const& slice,
                                    bool doSync) override;
  virtual arangodb::Result persistProperties() override;

  virtual PhysicalCollection* clone(LogicalCollection*,
                                    PhysicalCollection*) override;

  void getPropertiesVPack(velocypack::Builder&) const override;
  void getPropertiesVPackCoordinator(velocypack::Builder&) const override;

  /// @brief closes an open collection
  int close() override;

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

  std::shared_ptr<Index> createIndex(transaction::Methods* trx,
                                     arangodb::velocypack::Slice const& info,
                                     bool& created) override;
  /// @brief Restores an index from VelocyPack.
  int restoreIndex(transaction::Methods*, velocypack::Slice const&,
                   std::shared_ptr<Index>&) override;
  /// @brief Drop an index with the given iid.
  bool dropIndex(TRI_idx_iid_t iid) override;
  std::unique_ptr<IndexIterator> getAllIterator(transaction::Methods* trx,
                                                ManagedDocumentResult* mdr,
                                                bool reverse) const override;
  std::unique_ptr<IndexIterator> getAnyIterator(
      transaction::Methods* trx, ManagedDocumentResult* mdr) const override;

  std::unique_ptr<IndexIterator> getSortedAllIterator(
      transaction::Methods* trx, ManagedDocumentResult* mdr) const;

  void invokeOnAllElements(
      transaction::Methods* trx,
      std::function<bool(DocumentIdentifierToken const&)> callback) override;

  ////////////////////////////////////
  // -- SECTION DML Operations --
  ///////////////////////////////////

  void truncate(transaction::Methods* trx, OperationOptions& options) override;
  /// non transactional truncate, will continoiusly commit the deletes
  /// and no fully rollback on failure. Uses trx snapshots to isolate
  /// against newer PUTs
  // void truncateNoTrx(transaction::Methods* trx);

  DocumentIdentifierToken lookupKey(
      transaction::Methods* trx,
      arangodb::velocypack::Slice const& key) override;

  int read(transaction::Methods*, arangodb::velocypack::Slice const key,
           ManagedDocumentResult& result, bool) override;

  bool readDocument(transaction::Methods* trx,
                    DocumentIdentifierToken const& token,
                    ManagedDocumentResult& result) override;

  bool readDocumentNoCache(transaction::Methods* trx,
                           DocumentIdentifierToken const& token,
                           ManagedDocumentResult& result);

  int insert(arangodb::transaction::Methods* trx,
             arangodb::velocypack::Slice const newSlice,
             arangodb::ManagedDocumentResult& result, OperationOptions& options,
             TRI_voc_tick_t& resultMarkerTick, bool lock) override;

  int update(arangodb::transaction::Methods* trx,
             arangodb::velocypack::Slice const newSlice,
             arangodb::ManagedDocumentResult& result, OperationOptions& options,
             TRI_voc_tick_t& resultMarkerTick, bool lock,
             TRI_voc_rid_t& prevRev, ManagedDocumentResult& previous,
             TRI_voc_rid_t const& revisionId,
             arangodb::velocypack::Slice const key) override;

  int replace(transaction::Methods* trx,
              arangodb::velocypack::Slice const newSlice,
              ManagedDocumentResult& result, OperationOptions& options,
              TRI_voc_tick_t& resultMarkerTick, bool lock,
              TRI_voc_rid_t& prevRev, ManagedDocumentResult& previous,
              TRI_voc_rid_t const revisionId,
              arangodb::velocypack::Slice const fromSlice,
              arangodb::velocypack::Slice const toSlice) override;

  int remove(arangodb::transaction::Methods* trx,
             arangodb::velocypack::Slice const slice,
             arangodb::ManagedDocumentResult& previous,
             OperationOptions& options, TRI_voc_tick_t& resultMarkerTick,
             bool lock, TRI_voc_rid_t const& revisionId,
             TRI_voc_rid_t& prevRev) override;

  void deferDropCollection(
      std::function<bool(LogicalCollection*)> callback) override;

  void setRevision(TRI_voc_rid_t revisionId);
  void adjustNumberDocuments(int64_t adjustment);
  uint64_t objectId() const { return _objectId; }

  Result lookupDocumentToken(transaction::Methods* trx, arangodb::StringRef key,
                             RocksDBToken& token) const;

  int lockWrite(double timeout = 0.0);
  int unlockWrite();
  int lockRead(double timeout = 0.0);
  int unlockRead();

  /// recalculte counts for collection in case of failure
  uint64_t recalculateCounts();

  /// trigger rocksdb compaction for documentDB and indexes
  void compact();
  void estimateSize(velocypack::Builder& builder);

  bool hasGeoIndex() { return _hasGeoIndex; }

  Result serializeIndexEstimates(rocksdb::Transaction*) const;
  void deserializeIndexEstimates(arangodb::RocksDBCounterManager* mgr);

  void recalculateIndexEstimates();

 private:
  /// @brief return engine-specific figures
  void figuresSpecific(
      std::shared_ptr<arangodb::velocypack::Builder>&) override;
  /// @brief creates the initial indexes for the collection
  void createInitialIndexes();
  void addIndex(std::shared_ptr<arangodb::Index> idx);
  void addIndexCoordinator(std::shared_ptr<arangodb::Index> idx);
  int saveIndex(transaction::Methods* trx,
                std::shared_ptr<arangodb::Index> idx);

  arangodb::Result fillIndexes(transaction::Methods*,
                               std::shared_ptr<arangodb::Index>);

  arangodb::RocksDBPrimaryIndex* primaryIndex() const;

  arangodb::RocksDBOperationResult insertDocument(
      arangodb::transaction::Methods* trx, TRI_voc_rid_t revisionId,
      arangodb::velocypack::Slice const& doc, bool& waitForSync) const;

  arangodb::RocksDBOperationResult removeDocument(
      arangodb::transaction::Methods* trx, TRI_voc_rid_t revisionId,
      arangodb::velocypack::Slice const& doc, bool isUpdate,
      bool& waitForSync) const;

  arangodb::RocksDBOperationResult lookupDocument(
      transaction::Methods* trx, arangodb::velocypack::Slice key,
      ManagedDocumentResult& result) const;

  arangodb::RocksDBOperationResult updateDocument(
      transaction::Methods* trx, TRI_voc_rid_t oldRevisionId,
      arangodb::velocypack::Slice const& oldDoc, TRI_voc_rid_t newRevisionId,
      arangodb::velocypack::Slice const& newDoc, bool& waitForSync) const;

  arangodb::Result lookupRevisionVPack(TRI_voc_rid_t, transaction::Methods*,
                                       arangodb::ManagedDocumentResult&,
                                       bool withCache) const;

  void recalculateIndexEstimates(std::vector<std::shared_ptr<Index>>& indexes);

  void createCache() const;

  void disableCache() const;

  inline bool useCache() const { return (_useCache && _cachePresent); }

  void blackListKey(char const* data, std::size_t len) const;

 private:
  uint64_t const _objectId;  // rocksdb-specific object id for collection
  std::atomic<uint64_t> _numberDocuments;
  std::atomic<TRI_voc_rid_t> _revisionId;
  mutable std::atomic<bool> _needToPersistIndexEstimates;

  /// upgrade write locks to exclusive locks if this flag is set
  bool _hasGeoIndex;
  mutable basics::ReadWriteLock _exclusiveLock;
  mutable std::shared_ptr<cache::Cache> _cache;
  // we use this boolean for testing whether _cache is set.
  // it's quicker than accessing the shared_ptr each time
  mutable bool _cachePresent;
  bool _useCache;
};

inline RocksDBCollection* toRocksDBCollection(PhysicalCollection* physical) {
  auto rv = static_cast<RocksDBCollection*>(physical);
  TRI_ASSERT(rv != nullptr);
  return rv;
}

inline RocksDBCollection* toRocksDBCollection(LogicalCollection* logical) {
  auto phys = logical->getPhysical();
  TRI_ASSERT(phys != nullptr);
  return toRocksDBCollection(phys);
}

}  // namespace arangodb

#endif

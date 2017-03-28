////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan-Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_ROCKSDB_ENGINE_ROCKSDB_COLLECTION_H
#define ARANGOD_ROCKSDB_ENGINE_ROCKSDB_COLLECTION_H 1

#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"
#include "Indexes/IndexLookupContext.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/PhysicalCollection.h"

namespace arangodb {
class LogicalCollection;
class ManagedDocumentResult;
class Result;
class RocksDBPrimaryMockIndex;
struct RocksDBToken;

class RocksDBCollection final : public PhysicalCollection {
  friend class RocksDBEngine;

 public:
  static inline RocksDBCollection* toRocksDBCollection(
      PhysicalCollection* physical) {
    auto rv = static_cast<RocksDBCollection*>(physical);
    TRI_ASSERT(rv != nullptr);
    return rv;
  }

  static inline RocksDBCollection* toRocksDBCollection(
      LogicalCollection* logical) {
    auto phys = logical->getPhysical();
    TRI_ASSERT(phys != nullptr);
    return toRocksDBCollection(phys);
  }

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

  TRI_voc_rid_t revision() const override;
  
  int64_t initialCount() const override;
  void updateCount(int64_t) override;

  void getPropertiesVPack(velocypack::Builder&) const override;
  void getPropertiesVPackCoordinator(velocypack::Builder&) const override;

  /// @brief closes an open collection
  int close() override;

  uint64_t numberDocuments() const override;

  /// @brief report extra memory used by indexes etc.
  size_t memory() const override;
  void open(bool ignoreErrors) override;

  /// @brief iterate all markers of a collection on load
  int iterateMarkersOnLoad(arangodb::transaction::Methods* trx) override;

  bool isFullyCollected() const override;

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
                                                bool reverse) override;
  std::unique_ptr<IndexIterator> getAnyIterator(
      transaction::Methods* trx, ManagedDocumentResult* mdr) override;
  void invokeOnAllElements(
      std::function<bool(DocumentIdentifierToken const&)> callback) override;

  ////////////////////////////////////
  // -- SECTION DML Operations --
  ///////////////////////////////////

  void truncate(transaction::Methods* trx, OperationOptions& options) override;

  int read(transaction::Methods*, arangodb::velocypack::Slice const key,
           ManagedDocumentResult& result, bool) override;

  bool readDocument(transaction::Methods* trx,
                    DocumentIdentifierToken const& token,
                    ManagedDocumentResult& result) override;

  bool readDocumentConditional(transaction::Methods* trx,
                               DocumentIdentifierToken const& token,
                               TRI_voc_tick_t maxTick,
                               ManagedDocumentResult& result) override;

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
  
  uint64_t objectId() const { return _objectId; }
  
  Result lookupDocumentToken(transaction::Methods* trx,
                             arangodb::velocypack::Slice key, RocksDBToken &outToken);

 private:
  /// @brief return engine-specific figures
  void figuresSpecific(std::shared_ptr<arangodb::velocypack::Builder>&) override;
  /// @brief creates the initial indexes for the collection
  void createInitialIndexes();
  void addIndex(std::shared_ptr<arangodb::Index> idx);
  void addIndexCoordinator(std::shared_ptr<arangodb::Index> idx);
  int saveIndex(transaction::Methods* trx, std::shared_ptr<arangodb::Index> idx);
  arangodb::RocksDBPrimaryMockIndex* primaryIndex() const;

  int insertDocument(arangodb::transaction::Methods* trx,
                     TRI_voc_rid_t revisionId,
                     arangodb::velocypack::Slice const& doc,
                     bool& waitForSync);

  int removeDocument(arangodb::transaction::Methods* trx,
                     TRI_voc_rid_t revisionId,
                     arangodb::velocypack::Slice const& doc,
                     bool& waitForSync);

  int lookupDocument(transaction::Methods* trx,
                     arangodb::velocypack::Slice key,
                     ManagedDocumentResult& result);

  int updateDocument(transaction::Methods* trx, TRI_voc_rid_t oldRevisionId,
                     arangodb::velocypack::Slice const& oldDoc, TRI_voc_rid_t newRevisionId,
                     arangodb::velocypack::Slice const& newDoc, bool& waitForSync);

 private:
  uint64_t _objectId; // rocksdb-specific object id for collection
};

}

#endif

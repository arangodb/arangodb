//////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 EMC Corporation
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_IRESEARCH__IRESEARCH_STORAGE_ENGINE_MOCK_H
#define ARANGODB_IRESEARCH__IRESEARCH_STORAGE_ENGINE_MOCK_H 1

#include "Basics/Result.h"
#include "Basics/StringRef.h"
#include "Indexes/IndexIterator.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionCollection.h"
#include "StorageEngine/TransactionState.h"
#include "StorageEngine/PhysicalCollection.h"
#include "Transaction/ContextData.h"
#include "VocBase/LocalDocumentId.h"

namespace arangodb {

struct KeyLockInfo;
class TransactionManager;
class WalAccess;

} // arangodb

class ContextDataMock: public arangodb::transaction::ContextData {
 public:
  std::set<TRI_voc_cid_t> pinned;

  void pinData(arangodb::LogicalCollection* collection) override;
  bool isPinned(TRI_voc_cid_t cid) const override;
};

class PhysicalCollectionMock: public arangodb::PhysicalCollection {
 public:
  static std::function<void()> before;
  std::string physicalPath;
  std::deque<std::pair<arangodb::velocypack::Builder, bool>> documents; // std::pair<jSON, valid>, deque -> pointers remain valid

  PhysicalCollectionMock(arangodb::LogicalCollection& collection, arangodb::velocypack::Slice const& info);
  virtual PhysicalCollection* clone(arangodb::LogicalCollection& collection) const override;
  virtual int close() override;
  virtual std::shared_ptr<arangodb::Index> createIndex(arangodb::velocypack::Slice const& info, bool restore, bool& created) override;
  virtual void deferDropCollection(std::function<bool(arangodb::LogicalCollection&)> const& callback) override;
  virtual bool dropIndex(TRI_idx_iid_t iid) override;
  virtual void figuresSpecific(std::shared_ptr<arangodb::velocypack::Builder>&) override;
  virtual std::unique_ptr<arangodb::IndexIterator> getAllIterator(arangodb::transaction::Methods* trx) const override;
  virtual std::unique_ptr<arangodb::IndexIterator> getAnyIterator(arangodb::transaction::Methods* trx) const override;
  virtual void getPropertiesVPack(arangodb::velocypack::Builder&) const override;
  virtual arangodb::Result insert(
      arangodb::transaction::Methods* trx,
      arangodb::velocypack::Slice const newSlice,
      arangodb::ManagedDocumentResult& result,
      arangodb::OperationOptions& options, TRI_voc_tick_t& resultMarkerTick,
      bool lock, TRI_voc_tick_t& revisionId,
      arangodb::KeyLockInfo* /*keyLockInfo*/,
      std::function<arangodb::Result(void)> callbackDuringLock) override;
  virtual void invokeOnAllElements(arangodb::transaction::Methods* trx, std::function<bool(arangodb::LocalDocumentId const&)> callback) override;
  virtual std::shared_ptr<arangodb::Index> lookupIndex(arangodb::velocypack::Slice const&) const override;
  virtual arangodb::LocalDocumentId lookupKey(arangodb::transaction::Methods*, arangodb::velocypack::Slice const&) const override;
  virtual size_t memory() const override;
  virtual uint64_t numberDocuments(arangodb::transaction::Methods* trx) const override;
  virtual void open(bool ignoreErrors) override;
  virtual std::string const& path() const override;
  virtual arangodb::Result persistProperties() override;
  virtual void prepareIndexes(arangodb::velocypack::Slice indexesSlice) override;
  virtual arangodb::Result read(arangodb::transaction::Methods*,
                      arangodb::StringRef const& key,
                      arangodb::ManagedDocumentResult& result, bool) override;
  virtual arangodb::Result read(arangodb::transaction::Methods*, arangodb::velocypack::Slice const& key, arangodb::ManagedDocumentResult& result, bool) override;
  virtual bool readDocument(arangodb::transaction::Methods* trx, arangodb::LocalDocumentId const& token, arangodb::ManagedDocumentResult& result) const override;
  virtual bool readDocumentWithCallback(arangodb::transaction::Methods* trx, arangodb::LocalDocumentId const& token, arangodb::IndexIterator::DocumentCallback const& cb) const override;
  virtual arangodb::Result remove(
    arangodb::transaction::Methods& trx,
    arangodb::velocypack::Slice slice,
    arangodb::ManagedDocumentResult& previous,
    arangodb::OperationOptions& options,
    TRI_voc_tick_t& resultMarkerTick,
    bool lock,
    TRI_voc_rid_t& prevRev,
    TRI_voc_rid_t& revisionId,
    arangodb::KeyLockInfo* /*keyLockInfo*/,
    std::function<arangodb::Result(void)> callbackDuringLock
  ) override;
  virtual arangodb::Result replace(
      arangodb::transaction::Methods* trx,
      arangodb::velocypack::Slice const newSlice,
      arangodb::ManagedDocumentResult& result,
      arangodb::OperationOptions& options, TRI_voc_tick_t& resultMarkerTick,
      bool lock, TRI_voc_rid_t& prevRev,
      arangodb::ManagedDocumentResult& previous,
      std::function<arangodb::Result(void)> callbackDuringLock) override;
  virtual TRI_voc_rid_t revision(arangodb::transaction::Methods* trx) const override;
  virtual void setPath(std::string const&) override;
  virtual arangodb::Result truncate(
    arangodb::transaction::Methods& trx,
    arangodb::OperationOptions& options
  ) override;
  virtual arangodb::Result update(
      arangodb::transaction::Methods* trx,
      arangodb::velocypack::Slice const newSlice,
      arangodb::ManagedDocumentResult& result,
      arangodb::OperationOptions& options, TRI_voc_tick_t& resultMarkerTick,
      bool lock, TRI_voc_rid_t& prevRev,
      arangodb::ManagedDocumentResult& previous,
      arangodb::velocypack::Slice const key,
      std::function<arangodb::Result(void)> callbackDuringLock) override;
  virtual void load() override {}
  virtual void unload() override {}
  virtual arangodb::Result updateProperties(arangodb::velocypack::Slice const& slice, bool doSync) override;

 private:
  bool addIndex(std::shared_ptr<arangodb::Index> idx);
};

class TransactionCollectionMock: public arangodb::TransactionCollection {
 public:
  TransactionCollectionMock(arangodb::TransactionState *state, TRI_voc_cid_t cid, arangodb::AccessMode::Type accessType);
  virtual bool canAccess(arangodb::AccessMode::Type accessType) const override;
  virtual void freeOperations(arangodb::transaction::Methods* activeTrx, bool mustRollback) override;
  virtual bool hasOperations() const override;
  virtual void release() override;
  virtual int updateUsage(arangodb::AccessMode::Type accessType, int nestingLevel) override;
  virtual void unuse(int nestingLevel) override;
  virtual int use(int nestingLevel) override;
 private:
  int doLock(arangodb::AccessMode::Type type, int nestingLevel) override;
  int doUnlock(arangodb::AccessMode::Type type, int nestingLevel) override;
};

class TransactionStateMock: public arangodb::TransactionState {
 public:
  static size_t abortTransactionCount;
  static size_t beginTransactionCount;
  static size_t commitTransactionCount;

  TransactionStateMock(TRI_vocbase_t& vocbase, arangodb::transaction::Options const& options);
  virtual arangodb::Result abortTransaction(arangodb::transaction::Methods* trx) override;
  virtual arangodb::Result beginTransaction(arangodb::transaction::Hints hints) override;
  virtual arangodb::Result commitTransaction(arangodb::transaction::Methods* trx) override;
  virtual bool hasFailedOperations() const override;
};

class StorageEngineMock: public arangodb::StorageEngine {
 public:
  static std::function<void()> before;
  static bool inRecoveryResult;
  std::map<std::pair<TRI_voc_tick_t, TRI_voc_cid_t>, arangodb::velocypack::Builder> views;
  std::atomic<size_t> vocbaseCount;

  explicit StorageEngineMock(arangodb::application_features::ApplicationServer& server);
  virtual void addOptimizerRules() override;
  virtual void addRestHandlers(arangodb::rest::RestHandlerFactory& handlerFactory) override;
  virtual void addV8Functions() override;
  virtual void changeCollection(TRI_vocbase_t& vocbase, TRI_voc_cid_t id, arangodb::LogicalCollection const& collection, bool doSync) override;
  virtual arangodb::Result changeView(TRI_vocbase_t& vocbase, arangodb::LogicalView const& view, bool doSync) override;
  virtual std::string collectionPath(TRI_vocbase_t const& vocbase, TRI_voc_cid_t id) const override;
  virtual std::string createCollection(TRI_vocbase_t& vocbase, TRI_voc_cid_t id, arangodb::LogicalCollection const& collection) override;
  virtual std::unique_ptr<TRI_vocbase_t> createDatabase(TRI_voc_tick_t id, arangodb::velocypack::Slice const& args, int& status) override;
  virtual arangodb::Result createLoggerState(TRI_vocbase_t*, VPackBuilder&) override;
  virtual std::unique_ptr<arangodb::PhysicalCollection> createPhysicalCollection(arangodb::LogicalCollection& collection, arangodb::velocypack::Slice const& info) override;
  virtual arangodb::Result createTickRanges(VPackBuilder&) override;
  virtual std::unique_ptr<arangodb::TransactionCollection> createTransactionCollection(arangodb::TransactionState& state, TRI_voc_cid_t cid, arangodb::AccessMode::Type, int nestingLevel) override;
  virtual std::unique_ptr<arangodb::transaction::ContextData> createTransactionContextData() override;
  virtual std::unique_ptr<arangodb::TransactionManager> createTransactionManager() override;
  virtual std::unique_ptr<arangodb::TransactionState> createTransactionState(TRI_vocbase_t& vocbase, arangodb::transaction::Options const& options) override;
  virtual arangodb::Result createView(TRI_vocbase_t& vocbase, TRI_voc_cid_t id, arangodb::LogicalView const& view) override;
  virtual void getViewProperties(TRI_vocbase_t& vocbase, arangodb::LogicalView const& view, VPackBuilder& builder) override;
  virtual TRI_voc_tick_t currentTick() const override;
  virtual std::string databasePath(TRI_vocbase_t const* vocbase) const override;
  virtual void destroyCollection(TRI_vocbase_t& vocbase, arangodb::LogicalCollection& collection) override;
  virtual void destroyView(TRI_vocbase_t& vocbase, arangodb::LogicalView& view) noexcept override;
  virtual arangodb::Result dropCollection(TRI_vocbase_t& vocbase, arangodb::LogicalCollection& collection) override;
  virtual arangodb::Result dropDatabase(TRI_vocbase_t& vocbase) override;
  virtual arangodb::Result dropView(TRI_vocbase_t& vocbase, arangodb::LogicalView& view) override;
  virtual arangodb::Result firstTick(uint64_t&) override;
  virtual std::vector<std::string> currentWalFiles() const override;
  virtual arangodb::Result flushWal(bool waitForSync, bool waitForCollector, bool writeShutdownFile) override;
  virtual void getCollectionInfo(TRI_vocbase_t& vocbase, TRI_voc_cid_t cid, arangodb::velocypack::Builder& result, bool includeIndexes, TRI_voc_tick_t maxTick) override;
  virtual int getCollectionsAndIndexes(TRI_vocbase_t& vocbase, arangodb::velocypack::Builder& result, bool wasCleanShutdown, bool isUpgrade) override;
  virtual void getDatabases(arangodb::velocypack::Builder& result) override;
  virtual arangodb::velocypack::Builder getReplicationApplierConfiguration(TRI_vocbase_t& vocbase, int& result) override;
  virtual arangodb::velocypack::Builder getReplicationApplierConfiguration(int& result) override;
  virtual int getViews(TRI_vocbase_t& vocbase, arangodb::velocypack::Builder& result) override;
  virtual arangodb::Result handleSyncKeys(arangodb::DatabaseInitialSyncer& syncer, arangodb::LogicalCollection& col, std::string const& keysId) override;
  virtual bool inRecovery() override;
  virtual arangodb::Result lastLogger(TRI_vocbase_t& vocbase, std::shared_ptr<arangodb::transaction::Context> transactionContext, uint64_t tickStart, uint64_t tickEnd, std::shared_ptr<VPackBuilder>& builderSPtr) override;
  virtual double minimumSyncReplicationTimeout() const override { return 1.0; }
  virtual std::unique_ptr<TRI_vocbase_t> openDatabase(arangodb::velocypack::Slice const& args, bool isUpgrade, int& status) override;
  virtual arangodb::Result persistCollection(TRI_vocbase_t& vocbase, arangodb::LogicalCollection const& collection) override;
  virtual void prepareDropDatabase(TRI_vocbase_t& vocbase, bool useWriteMarker, int& status) override;
  using StorageEngine::registerCollection;
  using StorageEngine::registerView;
  virtual TRI_voc_tick_t releasedTick() const override;
  virtual void releaseTick(TRI_voc_tick_t) override;
  virtual int removeReplicationApplierConfiguration(TRI_vocbase_t& vocbase) override;
  virtual int removeReplicationApplierConfiguration() override;
  virtual arangodb::Result renameCollection(TRI_vocbase_t& vocbase, arangodb::LogicalCollection const& collection, std::string const& oldName) override;
  virtual int saveReplicationApplierConfiguration(TRI_vocbase_t& vocbase, arangodb::velocypack::Slice slice, bool doSync) override;
  virtual int saveReplicationApplierConfiguration(arangodb::velocypack::Slice, bool) override;
  virtual int shutdownDatabase(TRI_vocbase_t& vocbase) override;
  virtual void signalCleanup(TRI_vocbase_t& vocbase) override;
  virtual bool supportsDfdb() const override;
  virtual void unloadCollection(TRI_vocbase_t& vocbase, arangodb::LogicalCollection& collection) override;
  virtual bool useRawDocumentPointers() override { return false; }
  virtual std::string versionFilename(TRI_voc_tick_t) const override;
  virtual void waitForEstimatorSync(std::chrono::milliseconds maxWaitTime) override;
  virtual void waitForSyncTick(TRI_voc_tick_t tick) override;
  virtual void waitUntilDeletion(TRI_voc_tick_t id, bool force, int& status) override;
  virtual arangodb::WalAccess const* walAccess() const override;
  virtual int writeCreateDatabaseMarker(TRI_voc_tick_t id, VPackSlice const& slice) override;

 private:
  TRI_voc_tick_t _releasedTick;
};

#endif
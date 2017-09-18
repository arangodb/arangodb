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

#include "Indexes/IndexIterator.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionCollection.h"
#include "StorageEngine/TransactionState.h"
#include "StorageEngine/PhysicalCollection.h"
#include "StorageEngine/DocumentIdentifierToken.h"
#include "Transaction/ContextData.h"
#include "VocBase/PhysicalView.h"

namespace arangodb {

class TransactionManager;

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
  TRI_idx_iid_t lastId;
  std::string physicalPath;
  std::deque<std::pair<arangodb::velocypack::Builder, bool>> documents; // std::pair<jSON, valid>, deque -> pointers remain valid

  PhysicalCollectionMock(arangodb::LogicalCollection* collection, arangodb::velocypack::Slice const& info);
  virtual PhysicalCollection* clone(arangodb::LogicalCollection*) override;
  virtual int close() override;
  virtual std::shared_ptr<arangodb::Index> createIndex(arangodb::transaction::Methods* trx, arangodb::velocypack::Slice const& info, bool& created) override;
  virtual void deferDropCollection(std::function<bool(arangodb::LogicalCollection*)> callback) override;
  virtual bool dropIndex(TRI_idx_iid_t iid) override;
  virtual void figuresSpecific(std::shared_ptr<arangodb::velocypack::Builder>&) override;
  virtual std::unique_ptr<arangodb::IndexIterator> getAllIterator(arangodb::transaction::Methods* trx, arangodb::ManagedDocumentResult* mdr, bool reverse) const override;
  virtual std::unique_ptr<arangodb::IndexIterator> getAnyIterator(arangodb::transaction::Methods* trx, arangodb::ManagedDocumentResult* mdr) const override;
  virtual void getPropertiesVPack(arangodb::velocypack::Builder&) const override;
  virtual void getPropertiesVPackCoordinator(arangodb::velocypack::Builder&) const override;
  virtual arangodb::Result insert(arangodb::transaction::Methods* trx, arangodb::velocypack::Slice const newSlice, arangodb::ManagedDocumentResult& result, arangodb::OperationOptions& options, TRI_voc_tick_t& resultMarkerTick, bool lock) override;
  virtual void invokeOnAllElements(arangodb::transaction::Methods* trx, std::function<bool(arangodb::DocumentIdentifierToken const&)> callback) override;
  virtual std::shared_ptr<arangodb::Index> lookupIndex(arangodb::velocypack::Slice const&) const override;
  virtual arangodb::DocumentIdentifierToken lookupKey(arangodb::transaction::Methods*, arangodb::velocypack::Slice const&) override;
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
  virtual bool readDocument(arangodb::transaction::Methods* trx, arangodb::DocumentIdentifierToken const& token, arangodb::ManagedDocumentResult& result) override;
  virtual bool readDocumentWithCallback(arangodb::transaction::Methods* trx, arangodb::DocumentIdentifierToken const& token, arangodb::IndexIterator::DocumentCallback const& cb) override;
  virtual arangodb::Result remove(arangodb::transaction::Methods* trx, arangodb::velocypack::Slice const slice, arangodb::ManagedDocumentResult& previous, arangodb::OperationOptions& options, TRI_voc_tick_t& resultMarkerTick, bool lock, TRI_voc_rid_t const& revisionId, TRI_voc_rid_t& prevRev) override;
  virtual arangodb::Result replace(arangodb::transaction::Methods* trx, arangodb::velocypack::Slice const newSlice, arangodb::ManagedDocumentResult& result, arangodb::OperationOptions& options, TRI_voc_tick_t& resultMarkerTick, bool lock, TRI_voc_rid_t& prevRev, arangodb::ManagedDocumentResult& previous, TRI_voc_rid_t const revisionId, arangodb::velocypack::Slice const fromSlice, arangodb::velocypack::Slice const toSlice) override;
  virtual int restoreIndex(arangodb::transaction::Methods*, arangodb::velocypack::Slice const&, std::shared_ptr<arangodb::Index>&) override;
  virtual TRI_voc_rid_t revision(arangodb::transaction::Methods* trx) const override;
  virtual void setPath(std::string const&) override;
  virtual void truncate(arangodb::transaction::Methods* trx, arangodb::OperationOptions& options) override;
  virtual arangodb::Result update(arangodb::transaction::Methods* trx, arangodb::velocypack::Slice const newSlice, arangodb::ManagedDocumentResult& result, arangodb::OperationOptions& options, TRI_voc_tick_t& resultMarkerTick, bool lock, TRI_voc_rid_t& prevRev, arangodb::ManagedDocumentResult& previous, TRI_voc_rid_t const& revisionId, arangodb::velocypack::Slice const key) override;
  virtual void load() override {}
  virtual void unload() override {}
  virtual arangodb::Result updateProperties(arangodb::velocypack::Slice const& slice, bool doSync) override;
};

class PhysicalViewMock: public arangodb::PhysicalView {
 public:
  static std::function<void()> before;
  static int persistPropertiesResult;
  std::string physicalPath;

  PhysicalViewMock(arangodb::LogicalView* view, arangodb::velocypack::Slice const& info);
  virtual PhysicalView* clone(arangodb::LogicalView*, arangodb::PhysicalView*) override;
  virtual void drop() override;
  virtual void getPropertiesVPack(arangodb::velocypack::Builder&, bool includeSystem = false) const override;
  virtual void open() override;
  virtual std::string const& path() const override;
  virtual arangodb::Result persistProperties() override;
  virtual void setPath(std::string const&) override;
  virtual arangodb::Result updateProperties(arangodb::velocypack::Slice const& slice, bool doSync) override;
};

class TransactionCollectionMock: public arangodb::TransactionCollection {
 public:
  arangodb::AccessMode::Type lockType;

  TransactionCollectionMock(arangodb::TransactionState* state, TRI_voc_cid_t cid);
  virtual bool canAccess(arangodb::AccessMode::Type accessType) const override;
  virtual void freeOperations(arangodb::transaction::Methods* activeTrx, bool mustRollback) override;
  virtual bool hasOperations() const override;
  virtual bool isLocked() const override;
  virtual bool isLocked(arangodb::AccessMode::Type type, int nestingLevel) const override;
  virtual int lock() override;
  virtual int lock(arangodb::AccessMode::Type type, int nestingLevel) override;
  virtual void release() override;
  virtual int unlock(arangodb::AccessMode::Type, int nestingLevel) override;
  virtual int updateUsage(arangodb::AccessMode::Type accessType, int nestingLevel) override;
  virtual void unuse(int nestingLevel) override;
  virtual int use(int nestingLevel) override;
};

class TransactionStateMock: public arangodb::TransactionState {
 public:
  static size_t abortTransactionCount;
  static size_t beginTransactionCount;
  static size_t commitTransactionCount;

  TransactionStateMock(TRI_vocbase_t* vocbase, arangodb::transaction::Options const& options);
  virtual arangodb::Result abortTransaction(arangodb::transaction::Methods* trx) override;
  virtual arangodb::Result beginTransaction(arangodb::transaction::Hints hints) override;
  virtual arangodb::Result commitTransaction(arangodb::transaction::Methods* trx) override;
  virtual bool hasFailedOperations() const override;
};

class StorageEngineMock: public arangodb::StorageEngine {
 public:
  static bool inRecoveryResult;
  std::vector<std::unique_ptr<TRI_vocbase_t>> vocbases; // must allocate on heap because TRI_vocbase_t does not have a 'noexcept' move constructor

  StorageEngineMock();
  virtual bool useRawDocumentPointers() override { return false; }
  virtual void addAqlFunctions() override;
  virtual void addOptimizerRules() override;
  virtual void addRestHandlers(arangodb::rest::RestHandlerFactory*) override;
  virtual void addV8Functions() override;
  virtual void changeCollection(TRI_vocbase_t* vocbase, TRI_voc_cid_t id, arangodb::LogicalCollection const* parameters, bool doSync) override;
  virtual void changeView(TRI_vocbase_t* vocbase, TRI_voc_cid_t id, arangodb::LogicalView const*, bool doSync) override;
  virtual arangodb::Result renameView(TRI_vocbase_t*, std::shared_ptr<arangodb::LogicalView>, std::string const&) override;
    
  virtual std::string collectionPath(TRI_vocbase_t const* vocbase, TRI_voc_cid_t id) const override;
  virtual std::string createCollection(TRI_vocbase_t* vocbase, TRI_voc_cid_t id, arangodb::LogicalCollection const*) override;
  virtual TRI_vocbase_t* createDatabase(TRI_voc_tick_t id, arangodb::velocypack::Slice const& args, int& status) override;
  virtual void createIndex(TRI_vocbase_t* vocbase, TRI_voc_cid_t collectionId, TRI_idx_iid_t id, arangodb::velocypack::Slice const& data) override;
  virtual arangodb::Result createLoggerState(TRI_vocbase_t*, VPackBuilder&) override;
  virtual arangodb::PhysicalCollection* createPhysicalCollection(arangodb::LogicalCollection* collection, VPackSlice const& info) override;
  virtual arangodb::PhysicalView* createPhysicalView(arangodb::LogicalView* view, VPackSlice const& info) override;
  virtual arangodb::Result createTickRanges(VPackBuilder&) override;
  virtual arangodb::TransactionCollection* createTransactionCollection(arangodb::TransactionState* state, TRI_voc_cid_t cid, arangodb::AccessMode::Type, int nestingLevel) override;
  virtual arangodb::transaction::ContextData* createTransactionContextData() override;
  virtual arangodb::TransactionManager* createTransactionManager() override;
  virtual arangodb::TransactionState* createTransactionState(TRI_vocbase_t* vocbase, arangodb::transaction::Options const& options) override;
  virtual void createView(TRI_vocbase_t* vocbase, TRI_voc_cid_t id, arangodb::LogicalView const*) override;
  virtual TRI_voc_tick_t currentTick() const override;
  virtual std::string databasePath(TRI_vocbase_t const* vocbase) const override;
  virtual void destroyCollection(TRI_vocbase_t* vocbase, arangodb::LogicalCollection* collection) override;
  virtual void destroyView(TRI_vocbase_t* vocbase, arangodb::LogicalView*) override;
  virtual arangodb::Result dropCollection(TRI_vocbase_t* vocbase, arangodb::LogicalCollection* collection) override;
  virtual arangodb::Result dropDatabase(TRI_vocbase_t*) override;
  virtual arangodb::Result dropView(TRI_vocbase_t* vocbase, arangodb::LogicalView*) override;
  virtual arangodb::Result firstTick(uint64_t&) override;
  virtual void getCollectionInfo(TRI_vocbase_t* vocbase, TRI_voc_cid_t cid, arangodb::velocypack::Builder& result, bool includeIndexes, TRI_voc_tick_t maxTick) override;
  virtual int getCollectionsAndIndexes(TRI_vocbase_t* vocbase, arangodb::velocypack::Builder& result, bool wasCleanShutdown, bool isUpgrade) override;
  virtual void getDatabases(arangodb::velocypack::Builder& result) override;
  virtual std::shared_ptr<arangodb::velocypack::Builder> getReplicationApplierConfiguration(TRI_vocbase_t* vocbase, int& result) override;
  virtual int getViews(TRI_vocbase_t* vocbase, arangodb::velocypack::Builder& result) override;
  virtual int handleSyncKeys(arangodb::InitialSyncer&, arangodb::LogicalCollection*, std::string const&, std::string const&, std::string const&, TRI_voc_tick_t, std::string&) override;
  virtual bool inRecovery() override;
  virtual arangodb::Result lastLogger(TRI_vocbase_t*, std::shared_ptr<arangodb::transaction::Context>, uint64_t, uint64_t, std::shared_ptr<VPackBuilder>&) override;
  virtual TRI_vocbase_t* openDatabase(arangodb::velocypack::Slice const& args, bool isUpgrade, int& status) override;
  virtual arangodb::Result persistCollection(TRI_vocbase_t* vocbase, arangodb::LogicalCollection const* collection) override;
  virtual arangodb::Result persistView(TRI_vocbase_t* vocbase, arangodb::LogicalView const*) override;
  virtual void prepareDropDatabase(TRI_vocbase_t* vocbase, bool useWriteMarker, int& status) override;
  virtual TRI_voc_tick_t releasedTick() const override;
  virtual void releaseTick(TRI_voc_tick_t) override;
  virtual int removeReplicationApplierConfiguration(TRI_vocbase_t*) override;
  virtual arangodb::Result renameCollection(TRI_vocbase_t* vocbase, arangodb::LogicalCollection const* collection, std::string const& oldName) override;
  virtual int saveReplicationApplierConfiguration(TRI_vocbase_t*, arangodb::velocypack::Slice, bool) override;
  virtual int shutdownDatabase(TRI_vocbase_t* vocbase) override;
  virtual void signalCleanup(TRI_vocbase_t* vocbase) override;
  virtual bool supportsDfdb() const override;
  virtual void unloadCollection(TRI_vocbase_t* vocbase, arangodb::LogicalCollection* collection) override;
  virtual std::string versionFilename(TRI_voc_tick_t) const override;
  virtual void waitForSync(TRI_voc_tick_t tick) override;
  virtual void waitUntilDeletion(TRI_voc_tick_t id, bool force, int& status) override;
  virtual int writeCreateDatabaseMarker(TRI_voc_tick_t id, VPackSlice const& slice) override;

 private:
  TRI_voc_tick_t _releasedTick;
};

#endif

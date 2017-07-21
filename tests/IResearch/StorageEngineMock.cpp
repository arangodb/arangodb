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

#include "StorageEngineMock.h"

#include "Basics/Result.h"
#include "Indexes/IndexIterator.h"
#include "IResearch/IResearchLink.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"

PhysicalCollectionMock::PhysicalCollectionMock(arangodb::LogicalCollection* collection, arangodb::velocypack::Slice const& info)
  : PhysicalCollection(collection, info) {
}

arangodb::PhysicalCollection* PhysicalCollectionMock::clone(arangodb::LogicalCollection*, PhysicalCollection*) {
  TRI_ASSERT(false);
  return nullptr;
}

int PhysicalCollectionMock::close() {
  for (auto& index: _indexes) {
    index->unload();
  }

  return TRI_ERROR_NO_ERROR; // assume close successful
}

std::shared_ptr<arangodb::Index> PhysicalCollectionMock::createIndex(arangodb::transaction::Methods* trx, arangodb::velocypack::Slice const& info, bool& created) {
  _indexes.emplace_back(arangodb::iresearch::IResearchLink::make(1, _logicalCollection, info));
  created = true;
  return _indexes.back();
}

void PhysicalCollectionMock::deferDropCollection(std::function<bool(arangodb::LogicalCollection*)> callback) {
  callback(_logicalCollection); // assume noone is using this collection (drop immediately)
}

bool PhysicalCollectionMock::dropIndex(TRI_idx_iid_t iid) {
  for (auto itr = _indexes.begin(), end = _indexes.end(); itr != end; ++itr) {
    if ((*itr)->id() == iid) {
      if (TRI_ERROR_NO_ERROR == (*itr)->drop()) {
        _indexes.erase(itr); return true;
      }
    }
  }

  return false;
}

void PhysicalCollectionMock::figuresSpecific(std::shared_ptr<arangodb::velocypack::Builder>&) {
  TRI_ASSERT(false);
}

std::unique_ptr<arangodb::IndexIterator> PhysicalCollectionMock::getAllIterator(arangodb::transaction::Methods* trx, arangodb::ManagedDocumentResult* mdr, bool reverse) const {
  TRI_ASSERT(false);
  return nullptr;
}

std::unique_ptr<arangodb::IndexIterator> PhysicalCollectionMock::getAnyIterator(arangodb::transaction::Methods* trx, arangodb::ManagedDocumentResult* mdr) const {
  TRI_ASSERT(false);
  return nullptr;
}

void PhysicalCollectionMock::getPropertiesVPack(arangodb::velocypack::Builder&) const {
  TRI_ASSERT(false);
}

void PhysicalCollectionMock::getPropertiesVPackCoordinator(arangodb::velocypack::Builder&) const {
  TRI_ASSERT(false);
}

arangodb::Result PhysicalCollectionMock::insert(arangodb::transaction::Methods* trx, arangodb::velocypack::Slice const newSlice, arangodb::ManagedDocumentResult& result, arangodb::OperationOptions& options, TRI_voc_tick_t& resultMarkerTick, bool lock) {
  documents.emplace_back(newSlice);

  const TRI_voc_rid_t revId = documents.size() - 1;
  result.setUnmanaged(documents.back().data(), revId);

  for (auto& index : _indexes) {
    if (!index->insert(trx, revId, newSlice, false).ok()) {
      return arangodb::Result(TRI_ERROR_BAD_PARAMETER);
    }
  }

  return arangodb::Result();
}

void PhysicalCollectionMock::invokeOnAllElements(arangodb::transaction::Methods*, std::function<bool(arangodb::DocumentIdentifierToken const&)> callback) {
  TRI_ASSERT(false);
}

std::shared_ptr<arangodb::Index> PhysicalCollectionMock::lookupIndex(arangodb::velocypack::Slice const&) const {
  TRI_ASSERT(false);
  return nullptr;
}

arangodb::DocumentIdentifierToken PhysicalCollectionMock::lookupKey(arangodb::transaction::Methods*, arangodb::velocypack::Slice const&) {
  TRI_ASSERT(false);
  return arangodb::DocumentIdentifierToken();
}

size_t PhysicalCollectionMock::memory() const {
  TRI_ASSERT(false);
  return 0;
}

uint64_t PhysicalCollectionMock::numberDocuments(arangodb::transaction::Methods*) const {
  TRI_ASSERT(false);
  return 0;
}

void PhysicalCollectionMock::open(bool ignoreErrors) {
  TRI_ASSERT(false);
}

std::string const& PhysicalCollectionMock::path() const {
  return physicalPath;
}

arangodb::Result PhysicalCollectionMock::persistProperties() {
  TRI_ASSERT(false);
  return arangodb::Result(TRI_ERROR_INTERNAL);
}

void PhysicalCollectionMock::prepareIndexes(arangodb::velocypack::Slice indexesSlice) {
  // NOOP
}

arangodb::Result PhysicalCollectionMock::read(arangodb::transaction::Methods*, arangodb::velocypack::Slice const key, arangodb::ManagedDocumentResult& result, bool) {
  TRI_ASSERT(false);
  return TRI_ERROR_INTERNAL;
}

bool PhysicalCollectionMock::readDocument(arangodb::transaction::Methods* trx, arangodb::DocumentIdentifierToken const& token, arangodb::ManagedDocumentResult& result) {
  if (token._data >= documents.size()) {
    return false;
  }

  result.setUnmanaged(documents[token._data].data(), token._data);

  return true;
}

arangodb::Result PhysicalCollectionMock::remove(arangodb::transaction::Methods* trx, arangodb::velocypack::Slice const slice, arangodb::ManagedDocumentResult& previous, arangodb::OperationOptions& options, TRI_voc_tick_t& resultMarkerTick, bool lock, TRI_voc_rid_t const& revisionId, TRI_voc_rid_t& prevRev) {
  TRI_ASSERT(false);
  return TRI_ERROR_INTERNAL;
}

arangodb::Result PhysicalCollectionMock::replace(arangodb::transaction::Methods* trx, arangodb::velocypack::Slice const newSlice, arangodb::ManagedDocumentResult& result, arangodb::OperationOptions& options, TRI_voc_tick_t& resultMarkerTick, bool lock, TRI_voc_rid_t& prevRev, arangodb::ManagedDocumentResult& previous, TRI_voc_rid_t const revisionId, arangodb::velocypack::Slice const fromSlice, arangodb::velocypack::Slice const toSlice) {
  TRI_ASSERT(false);
  return TRI_ERROR_INTERNAL;
}

int PhysicalCollectionMock::restoreIndex(arangodb::transaction::Methods*, arangodb::velocypack::Slice const&, std::shared_ptr<arangodb::Index>&) {
  TRI_ASSERT(false);
  return TRI_ERROR_INTERNAL;
}

TRI_voc_rid_t PhysicalCollectionMock::revision(arangodb::transaction::Methods*) const {
  TRI_ASSERT(false);
  return 0;
}

void PhysicalCollectionMock::setPath(std::string const& value) {
  physicalPath = value;
}

void PhysicalCollectionMock::truncate(arangodb::transaction::Methods* trx, arangodb::OperationOptions& options) {
  TRI_ASSERT(false);
}

arangodb::Result PhysicalCollectionMock::update(arangodb::transaction::Methods* trx, arangodb::velocypack::Slice const newSlice, arangodb::ManagedDocumentResult& result, arangodb::OperationOptions& options, TRI_voc_tick_t& resultMarkerTick, bool lock, TRI_voc_rid_t& prevRev, arangodb::ManagedDocumentResult& previous, TRI_voc_rid_t const& revisionId, arangodb::velocypack::Slice const key) {
  TRI_ASSERT(false);
  return TRI_ERROR_INTERNAL;
}

arangodb::Result PhysicalCollectionMock::updateProperties(arangodb::velocypack::Slice const& slice, bool doSync) {
  return arangodb::Result(TRI_ERROR_NO_ERROR); // assume mock collection updated OK
}

int PhysicalViewMock::persistPropertiesResult;

PhysicalViewMock::PhysicalViewMock(arangodb::LogicalView* view, arangodb::velocypack::Slice const& info)
  : PhysicalView(view, info) {
}

arangodb::PhysicalView* PhysicalViewMock::clone(arangodb::LogicalView*, arangodb::PhysicalView*) {
  TRI_ASSERT(false);
  return nullptr;
}

void PhysicalViewMock::drop() {
  // NOOP, assume physical view dropped OK
}

void PhysicalViewMock::getPropertiesVPack(arangodb::velocypack::Builder&, bool includeSystem /*= false*/) const {
  TRI_ASSERT(false);
}

void PhysicalViewMock::open() {
  TRI_ASSERT(false);
}

std::string const& PhysicalViewMock::path() const {
  return physicalPath;
}

arangodb::Result PhysicalViewMock::persistProperties() {
  return arangodb::Result(persistPropertiesResult);
}

void PhysicalViewMock::setPath(std::string const& value) {
  physicalPath = value;
}

arangodb::Result PhysicalViewMock::updateProperties(arangodb::velocypack::Slice const& slice, bool doSync) {
  TRI_ASSERT(false);
  return arangodb::Result(TRI_ERROR_INTERNAL);
}

StorageEngineMock::StorageEngineMock()
  : StorageEngine(nullptr, "", "", nullptr) {
}

void StorageEngineMock::addAqlFunctions() {
  // NOOP
}

void StorageEngineMock::addOptimizerRules() {
  TRI_ASSERT(false);
}

void StorageEngineMock::addRestHandlers(arangodb::rest::RestHandlerFactory*) {
  TRI_ASSERT(false);
}

void StorageEngineMock::addV8Functions() {
  TRI_ASSERT(false);
}

void StorageEngineMock::changeCollection(TRI_vocbase_t* vocbase, TRI_voc_cid_t id, arangodb::LogicalCollection const* parameters, bool doSync) {
  // NOOP, assume physical collection changed OK
}

void StorageEngineMock::changeView(TRI_vocbase_t* vocbase, TRI_voc_cid_t id, arangodb::LogicalView const*, bool doSync) {
  TRI_ASSERT(false);
}

std::string StorageEngineMock::collectionPath(TRI_vocbase_t const* vocbase, TRI_voc_cid_t id) const {
  TRI_ASSERT(false);
  return "<invalid>";
}

std::string StorageEngineMock::createCollection(TRI_vocbase_t* vocbase, TRI_voc_cid_t id, arangodb::LogicalCollection const*) {
  return "<invalid>"; // physical path of the new collection
}

TRI_vocbase_t* StorageEngineMock::createDatabase(TRI_voc_tick_t id, arangodb::velocypack::Slice const& args, int& status) {
  TRI_ASSERT(false);
  return nullptr;
}

void StorageEngineMock::createIndex(TRI_vocbase_t* vocbase, TRI_voc_cid_t collectionId, TRI_idx_iid_t id, arangodb::velocypack::Slice const& data) {
  TRI_ASSERT(false);
}

arangodb::Result StorageEngineMock::createLoggerState(TRI_vocbase_t*, VPackBuilder&) {
  TRI_ASSERT(false);
  return arangodb::Result(false);
}

arangodb::PhysicalCollection* StorageEngineMock::createPhysicalCollection(arangodb::LogicalCollection* collection, VPackSlice const& info) {
  return new PhysicalCollectionMock(collection, info);
}

arangodb::PhysicalView* StorageEngineMock::createPhysicalView(arangodb::LogicalView* view, VPackSlice const& info) {
  return new PhysicalViewMock(view, info);
}

arangodb::Result StorageEngineMock::createTickRanges(VPackBuilder&) {
  TRI_ASSERT(false);
  return arangodb::Result(false);
}

arangodb::TransactionCollection* StorageEngineMock::createTransactionCollection(arangodb::TransactionState* state, TRI_voc_cid_t cid, arangodb::AccessMode::Type, int nestingLevel) {
  return new TransactionCollectionMock(state, cid);
}

arangodb::transaction::ContextData* StorageEngineMock::createTransactionContextData() {
  TRI_ASSERT(false);
  return nullptr;
}

arangodb::TransactionManager* StorageEngineMock::createTransactionManager() {
  TRI_ASSERT(false);
  return nullptr;
}

arangodb::TransactionState* StorageEngineMock::createTransactionState(TRI_vocbase_t* vocbase, arangodb::transaction::Options const& options) {
  return new TransactionStateMock(vocbase, options);
}

void StorageEngineMock::createView(TRI_vocbase_t* vocbase, TRI_voc_cid_t id, arangodb::LogicalView const*) {
  // NOOP, assume physical view created OK
}

std::string StorageEngineMock::databasePath(TRI_vocbase_t const* vocbase) const {
  TRI_ASSERT(false);
  return nullptr;
}

void StorageEngineMock::destroyCollection(TRI_vocbase_t* vocbase, arangodb::LogicalCollection* collection) {
  // NOOP, assume physical collection destroyed OK
}

void StorageEngineMock::destroyView(TRI_vocbase_t* vocbase, arangodb::LogicalView*) {
  TRI_ASSERT(false);
}

arangodb::Result StorageEngineMock::dropCollection(TRI_vocbase_t* vocbase, arangodb::LogicalCollection* collection) {
  return arangodb::Result(TRI_ERROR_NO_ERROR); // assume physical collection dropped OK
}

arangodb::Result StorageEngineMock::dropDatabase(TRI_vocbase_t*) {
  TRI_ASSERT(false);
  return arangodb::Result();
}

arangodb::Result StorageEngineMock::dropView(TRI_vocbase_t* vocbase, arangodb::LogicalView*) {
  return arangodb::Result(TRI_ERROR_NO_ERROR); // assume mock view dropped OK
}

arangodb::Result StorageEngineMock::firstTick(uint64_t&) {
  TRI_ASSERT(false);
  return arangodb::Result(false);
}

void StorageEngineMock::getCollectionInfo(TRI_vocbase_t* vocbase, TRI_voc_cid_t cid, arangodb::velocypack::Builder& result, bool includeIndexes, TRI_voc_tick_t maxTick) {
  arangodb::velocypack::Builder parameters;

  parameters.openObject();
  parameters.close();

  result.openObject();
  result.add("parameters", parameters.slice()); // required entry of type object
  result.close();

  // nothing more required, assume info used for PhysicalCollectionMock
}

int StorageEngineMock::getCollectionsAndIndexes(TRI_vocbase_t* vocbase, arangodb::velocypack::Builder& result, bool wasCleanShutdown, bool isUpgrade) {
  TRI_ASSERT(false);
  return TRI_ERROR_INTERNAL;
}

void StorageEngineMock::getDatabases(arangodb::velocypack::Builder& result) {
  TRI_ASSERT(false);
}

std::shared_ptr<arangodb::velocypack::Builder> StorageEngineMock::getReplicationApplierConfiguration(TRI_vocbase_t*, int&) {
  TRI_ASSERT(false);
  return nullptr;
}

int StorageEngineMock::getViews(TRI_vocbase_t* vocbase, arangodb::velocypack::Builder& result) {
  TRI_ASSERT(false);
  return TRI_ERROR_INTERNAL;
}

int StorageEngineMock::handleSyncKeys(arangodb::InitialSyncer&, arangodb::LogicalCollection*, std::string const&, std::string const&, std::string const&, TRI_voc_tick_t, std::string&) {
  TRI_ASSERT(false);
  return 0;
}

arangodb::Result StorageEngineMock::lastLogger(TRI_vocbase_t*, std::shared_ptr<arangodb::transaction::Context>, uint64_t, uint64_t, std::shared_ptr<VPackBuilder>&) {
  TRI_ASSERT(false);
  return arangodb::Result(false);
}

TRI_vocbase_t* StorageEngineMock::openDatabase(arangodb::velocypack::Slice const& args, bool isUpgrade, int& status) {
  TRI_ASSERT(false);
  return nullptr;
}

arangodb::Result StorageEngineMock::persistCollection(TRI_vocbase_t* vocbase, arangodb::LogicalCollection const* collection) {
  return arangodb::Result(TRI_ERROR_NO_ERROR); // assume mock collection persisted OK
}

arangodb::Result StorageEngineMock::persistView(TRI_vocbase_t* vocbase, arangodb::LogicalView const*) {
  return arangodb::Result(TRI_ERROR_NO_ERROR); // assume mock view persisted OK
}

void StorageEngineMock::prepareDropDatabase(TRI_vocbase_t* vocbase, bool useWriteMarker, int& status) {
  TRI_ASSERT(false);
}

int StorageEngineMock::removeReplicationApplierConfiguration(TRI_vocbase_t*) {
  TRI_ASSERT(false);
  return 0;
}

arangodb::Result StorageEngineMock::renameCollection(TRI_vocbase_t* vocbase, arangodb::LogicalCollection const* collection, std::string const& oldName) {
  TRI_ASSERT(false);
  return arangodb::Result(TRI_ERROR_INTERNAL);
}

int StorageEngineMock::saveReplicationApplierConfiguration(TRI_vocbase_t*, arangodb::velocypack::Slice, bool) {
  TRI_ASSERT(false);
  return 0;
}

int StorageEngineMock::shutdownDatabase(TRI_vocbase_t* vocbase) {
  return TRI_ERROR_NO_ERROR; // assume shutdown successful
}

void StorageEngineMock::signalCleanup(TRI_vocbase_t* vocbase) {
  // NOOP, assume cleanup thread signaled OK
}

bool StorageEngineMock::supportsDfdb() const {
  TRI_ASSERT(false);
  return false;
}

void StorageEngineMock::unloadCollection(TRI_vocbase_t* vocbase, arangodb::LogicalCollection* collection) {
  TRI_ASSERT(false);
}

std::string StorageEngineMock::versionFilename(TRI_voc_tick_t) const {
  TRI_ASSERT(false);
  return std::string();
}

void StorageEngineMock::waitForSync(TRI_voc_tick_t tick) {
  TRI_ASSERT(false);
}

void StorageEngineMock::waitUntilDeletion(TRI_voc_tick_t id, bool force, int& status) {
  TRI_ASSERT(false);
}

int StorageEngineMock::writeCreateDatabaseMarker(TRI_voc_tick_t id, VPackSlice const& slice) {
  TRI_ASSERT(false);
  return TRI_ERROR_INTERNAL;
}

TransactionCollectionMock::TransactionCollectionMock(arangodb::TransactionState* state, TRI_voc_cid_t cid): TransactionCollection(state, cid) {
}

bool TransactionCollectionMock::canAccess(arangodb::AccessMode::Type accessType) const {
  return true;
}

void TransactionCollectionMock::freeOperations(arangodb::transaction::Methods* activeTrx, bool mustRollback) {
  TRI_ASSERT(false);
}

bool TransactionCollectionMock::hasOperations() const {
  TRI_ASSERT(false);
  return false;
}

bool TransactionCollectionMock::isLocked() const {
  TRI_ASSERT(false);
  return false;
}

bool TransactionCollectionMock::isLocked(arangodb::AccessMode::Type, int nestingLevel) const {
  TRI_ASSERT(false);
  return false;
}

int TransactionCollectionMock::lock() {
  TRI_ASSERT(false);
  return TRI_ERROR_INTERNAL;
}

int TransactionCollectionMock::lock(arangodb::AccessMode::Type, int nestingLevel) {
  TRI_ASSERT(false);
  return TRI_ERROR_INTERNAL;
}

void TransactionCollectionMock::release() {
  if (_collection) {
    _transaction->vocbase()->releaseCollection(_collection);
    _collection = nullptr;
  }
}

int TransactionCollectionMock::unlock(arangodb::AccessMode::Type, int nestingLevel) {
  TRI_ASSERT(false);
  return TRI_ERROR_INTERNAL;
}

int TransactionCollectionMock::updateUsage(arangodb::AccessMode::Type accessType, int nestingLevel) {
  return TRI_ERROR_NO_ERROR;
}

void TransactionCollectionMock::unuse(int nestingLevel) {
  // NOOP, assume success
}

int TransactionCollectionMock::use(int nestingLevel) {
  TRI_vocbase_col_status_e status;
  _collection = _transaction->vocbase()->useCollection(_cid, status);
  return _collection ? TRI_ERROR_NO_ERROR : TRI_ERROR_INTERNAL;
}

size_t TransactionStateMock::abortTransactionCount;
size_t TransactionStateMock::beginTransactionCount;
size_t TransactionStateMock::commitTransactionCount;

TransactionStateMock::TransactionStateMock(TRI_vocbase_t* vocbase, arangodb::transaction::Options const& options)
  : TransactionState(vocbase, options) {
}

arangodb::Result TransactionStateMock::abortTransaction(arangodb::transaction::Methods* trx) {
  ++abortTransactionCount;
  updateStatus(arangodb::transaction::Status::ABORTED);
  unuseCollections(_nestingLevel);
  return arangodb::Result();
}

arangodb::Result TransactionStateMock::beginTransaction(arangodb::transaction::Hints hints) {
  ++beginTransactionCount;
  useCollections(_nestingLevel);
  updateStatus(arangodb::transaction::Status::RUNNING);
  return arangodb::Result();
}

arangodb::Result TransactionStateMock::commitTransaction(arangodb::transaction::Methods* trx) {
  ++commitTransactionCount;
  updateStatus(arangodb::transaction::Status::COMMITTED);
  unuseCollections(_nestingLevel);
  return arangodb::Result();
}

bool TransactionStateMock::hasFailedOperations() const {
  return false; // assume no failed operations
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
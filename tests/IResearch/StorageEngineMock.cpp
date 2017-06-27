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

//bool PhysicalCollectionMock::applyForTickRange(TRI_voc_tick_t dataMin, TRI_voc_tick_t dataMax, std::function<bool(TRI_voc_tick_t foundTick, MMFilesMarker const* marker)> const& callback) {
//  TRI_ASSERT(false);
//  return false;
//}

arangodb::PhysicalCollection* PhysicalCollectionMock::clone(arangodb::LogicalCollection*, PhysicalCollection*) {
  TRI_ASSERT(false);
  return nullptr;
}

int PhysicalCollectionMock::close() {
  TRI_ASSERT(false);
  return TRI_ERROR_INTERNAL;
}

std::shared_ptr<arangodb::Index> PhysicalCollectionMock::createIndex(arangodb::transaction::Methods* trx, arangodb::velocypack::Slice const& info, bool& created) {
  _indexes.emplace_back(arangodb::iresearch::IResearchLink::make(1, _logicalCollection, info));
  created = true;
  return _indexes.back();
}

void PhysicalCollectionMock::deferDropCollection(std::function<bool(arangodb::LogicalCollection*)> callback) {
  TRI_ASSERT(false);
}

//bool PhysicalCollectionMock::doCompact() const {
//  TRI_ASSERT(false);
//  return false;
//}

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

//uint32_t PhysicalCollectionMock::indexBuckets() const {
//  TRI_ASSERT(false);
//  return 0;
//}
//
//int64_t PhysicalCollectionMock::initialCount() const {
//  TRI_ASSERT(false);
//  return 0;
//}
//
int PhysicalCollectionMock::insert(arangodb::transaction::Methods* trx, arangodb::velocypack::Slice const newSlice, arangodb::ManagedDocumentResult& result, arangodb::OperationOptions& options, TRI_voc_tick_t& resultMarkerTick, bool lock) {
  TRI_ASSERT(false);
  return TRI_ERROR_INTERNAL;
}

void PhysicalCollectionMock::invokeOnAllElements(arangodb::transaction::Methods*, std::function<bool(arangodb::DocumentIdentifierToken const&)> callback) {
  TRI_ASSERT(false);
}

//bool PhysicalCollectionMock::isFullyCollected() const {
//  TRI_ASSERT(false);
//  return false;
//}

//int PhysicalCollectionMock::iterateMarkersOnLoad(arangodb::transaction::Methods* trx) {
//  TRI_ASSERT(false);
//  return TRI_ERROR_INTERNAL;
//}
//
//size_t PhysicalCollectionMock::journalSize() const {
//  TRI_ASSERT(false);
//  return 0;
//}

std::shared_ptr<arangodb::Index> PhysicalCollectionMock::lookupIndex(arangodb::velocypack::Slice const&) const {
  TRI_ASSERT(false);
  return nullptr;
}

//uint8_t const* PhysicalCollectionMock::lookupRevisionVPack(TRI_voc_rid_t revisionId) const {
//  TRI_ASSERT(false);
//  return nullptr;
//}
//
//uint8_t const* PhysicalCollectionMock::lookupRevisionVPackConditional(TRI_voc_rid_t revisionId, TRI_voc_tick_t maxTick, bool excludeWal) const {
//  TRI_ASSERT(false);
//  return nullptr;
//}

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

int PhysicalCollectionMock::read(arangodb::transaction::Methods*, arangodb::velocypack::Slice const key, arangodb::ManagedDocumentResult& result, bool) {
  TRI_ASSERT(false);
  return TRI_ERROR_INTERNAL;
}

bool PhysicalCollectionMock::readDocument(arangodb::transaction::Methods* trx, arangodb::DocumentIdentifierToken const& token, arangodb::ManagedDocumentResult& result) {
  TRI_ASSERT(false);
  return false;
}

//bool PhysicalCollectionMock::readDocumentConditional(arangodb::transaction::Methods* trx, arangodb::DocumentIdentifierToken const& token, TRI_voc_tick_t maxTick, arangodb::ManagedDocumentResult& result) {
//  TRI_ASSERT(false);
//  return false;
//}

int PhysicalCollectionMock::remove(arangodb::transaction::Methods* trx, arangodb::velocypack::Slice const slice, arangodb::ManagedDocumentResult& previous, arangodb::OperationOptions& options, TRI_voc_tick_t& resultMarkerTick, bool lock, TRI_voc_rid_t const& revisionId, TRI_voc_rid_t& prevRev) {
  TRI_ASSERT(false);
  return TRI_ERROR_INTERNAL;
}

int PhysicalCollectionMock::replace(arangodb::transaction::Methods* trx, arangodb::velocypack::Slice const newSlice, arangodb::ManagedDocumentResult& result, arangodb::OperationOptions& options, TRI_voc_tick_t& resultMarkerTick, bool lock, TRI_voc_rid_t& prevRev, arangodb::ManagedDocumentResult& previous, TRI_voc_rid_t const revisionId, arangodb::velocypack::Slice const fromSlice, arangodb::velocypack::Slice const toSlice) {
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

//int PhysicalCollectionMock::rotateActiveJournal() {
//  TRI_ASSERT(false);
//  return TRI_ERROR_INTERNAL;
//}

void PhysicalCollectionMock::setPath(std::string const& value) {
  physicalPath = value;
}

//void PhysicalCollectionMock::sizeHint(arangodb::transaction::Methods* trx, int64_t hint) {
//  TRI_ASSERT(false);
//}

void PhysicalCollectionMock::truncate(arangodb::transaction::Methods* trx, arangodb::OperationOptions& options) {
  TRI_ASSERT(false);
}

int PhysicalCollectionMock::update(arangodb::transaction::Methods* trx, arangodb::velocypack::Slice const newSlice, arangodb::ManagedDocumentResult& result, arangodb::OperationOptions& options, TRI_voc_tick_t& resultMarkerTick, bool lock, TRI_voc_rid_t& prevRev, arangodb::ManagedDocumentResult& previous, TRI_voc_rid_t const& revisionId, arangodb::velocypack::Slice const key) {
  TRI_ASSERT(false);
  return TRI_ERROR_INTERNAL;
}

//void PhysicalCollectionMock::updateCount(int64_t) {
//  TRI_ASSERT(false);
//}

arangodb::Result PhysicalCollectionMock::updateProperties(arangodb::velocypack::Slice const& slice, bool doSync) {
  TRI_ASSERT(false);
  return arangodb::Result(TRI_ERROR_INTERNAL);
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
  TRI_ASSERT(false);
}

//void StorageEngineMock::addDocumentRevision(TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId, arangodb::velocypack::Slice const& document) {
//  TRI_ASSERT(false);
//}

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
  TRI_ASSERT(false);
}

void StorageEngineMock::changeView(TRI_vocbase_t* vocbase, TRI_voc_cid_t id, arangodb::LogicalView const*, bool doSync) {
  TRI_ASSERT(false);
}

//bool StorageEngineMock::cleanupCompactionBlockers(TRI_vocbase_t* vocbase) {
//  TRI_ASSERT(false);
//  return false;
//}

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

arangodb::PhysicalCollection* StorageEngineMock::createPhysicalCollection(arangodb::LogicalCollection* collection, VPackSlice const& info) {
  return new PhysicalCollectionMock(collection, info);
}

arangodb::PhysicalView* StorageEngineMock::createPhysicalView(arangodb::LogicalView* view, VPackSlice const& info) {
  return new PhysicalViewMock(view, info);
}

arangodb::TransactionCollection* StorageEngineMock::createTransactionCollection(arangodb::TransactionState* state, TRI_voc_cid_t cid, arangodb::AccessMode::Type, int nestingLevel) {
  return new TransactionCollectionMock(state, cid);
}

arangodb::transaction::ContextData* StorageEngineMock::createTransactionContextData() {
  TRI_ASSERT(false);
  return nullptr;
}

arangodb::TransactionState* StorageEngineMock::createTransactionState(TRI_vocbase_t* vocbase, arangodb::transaction::Options const&) {
  return new TransactionStateMock(vocbase);
}

void StorageEngineMock::createView(TRI_vocbase_t* vocbase, TRI_voc_cid_t id, arangodb::LogicalView const*) {
  // NOOP, assume physical view created OK
}

std::string StorageEngineMock::databasePath(TRI_vocbase_t const* vocbase) const {
  TRI_ASSERT(false);
  return nullptr;
}

void StorageEngineMock::destroyCollection(TRI_vocbase_t* vocbase, arangodb::LogicalCollection* collection) {
  TRI_ASSERT(false);
}

void StorageEngineMock::destroyView(TRI_vocbase_t* vocbase, arangodb::LogicalView*) {
  TRI_ASSERT(false);
}

arangodb::Result StorageEngineMock::dropCollection(TRI_vocbase_t* vocbase, arangodb::LogicalCollection* collection) {
  TRI_ASSERT(false);
  return arangodb::Result(TRI_ERROR_INTERNAL);
}

arangodb::Result StorageEngineMock::dropDatabase(TRI_vocbase_t*) {
  TRI_ASSERT(false);
  return arangodb::Result();
}

//void StorageEngineMock::dropIndex(TRI_vocbase_t* vocbase, TRI_voc_cid_t collectionId, TRI_idx_iid_t id) {
//  TRI_ASSERT(false);
//}
//
//void StorageEngineMock::dropIndexWalMarker(TRI_vocbase_t* vocbase, TRI_voc_cid_t collectionId, arangodb::velocypack::Slice const& data, bool useMarker, int&) {
//  TRI_ASSERT(false);
//}

arangodb::Result StorageEngineMock::dropView(TRI_vocbase_t* vocbase, arangodb::LogicalView*) {
  return arangodb::Result(TRI_ERROR_NO_ERROR); // assume mock view dropped OK
}

//int StorageEngineMock::extendCompactionBlocker(TRI_vocbase_t* vocbase, TRI_voc_tick_t id, double ttl) {
//  TRI_ASSERT(false);
//  return TRI_ERROR_INTERNAL;
//}

void StorageEngineMock::getCollectionInfo(TRI_vocbase_t* vocbase, TRI_voc_cid_t cid, arangodb::velocypack::Builder& result, bool includeIndexes, TRI_voc_tick_t maxTick) {
  TRI_ASSERT(false);
}

int StorageEngineMock::getCollectionsAndIndexes(TRI_vocbase_t* vocbase, arangodb::velocypack::Builder& result, bool wasCleanShutdown, bool isUpgrade) {
  TRI_ASSERT(false);
  return TRI_ERROR_INTERNAL;
}

void StorageEngineMock::getDatabases(arangodb::velocypack::Builder& result) {
  TRI_ASSERT(false);
}

int StorageEngineMock::getViews(TRI_vocbase_t* vocbase, arangodb::velocypack::Builder& result) {
  TRI_ASSERT(false);
  return TRI_ERROR_INTERNAL;
}

//int StorageEngineMock::insertCompactionBlocker(TRI_vocbase_t* vocbase, double ttl, TRI_voc_tick_t& id) {
//  TRI_ASSERT(false);
//  return TRI_ERROR_INTERNAL;
//}

//void StorageEngineMock::iterateDocuments(TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId, std::function<void(arangodb::velocypack::Slice const&)> const& cb) {
//  TRI_ASSERT(false);
//}
//
//int StorageEngineMock::openCollection(TRI_vocbase_t* vocbase, arangodb::LogicalCollection* collection, bool ignoreErrors) {
//  TRI_ASSERT(false);
//  return TRI_ERROR_INTERNAL;
//}

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

//void StorageEngineMock::preventCompaction(TRI_vocbase_t* vocbase, std::function<void(TRI_vocbase_t*)> const& callback) {
//  TRI_ASSERT(false);
//}
//
//int StorageEngineMock::removeCompactionBlocker(TRI_vocbase_t* vocbase, TRI_voc_tick_t id) {
//  TRI_ASSERT(false);
//  return TRI_ERROR_INTERNAL;
//}

//void StorageEngineMock::removeDocumentRevision(TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId, arangodb::velocypack::Slice const& document) {
//  TRI_ASSERT(false);
//}

arangodb::Result StorageEngineMock::renameCollection(TRI_vocbase_t* vocbase, arangodb::LogicalCollection const* collection, std::string const& oldName) {
  TRI_ASSERT(false);
  return arangodb::Result(TRI_ERROR_INTERNAL);
}

int StorageEngineMock::shutdownDatabase(TRI_vocbase_t* vocbase) {
  return TRI_ERROR_NO_ERROR; // assume shutdown successful
}

void StorageEngineMock::signalCleanup(TRI_vocbase_t* vocbase) {
  TRI_ASSERT(false);
}

//bool StorageEngineMock::tryPreventCompaction(TRI_vocbase_t* vocbase, std::function<void(TRI_vocbase_t*)> const& callback, bool checkForActiveBlockers) {
//  TRI_ASSERT(false);
//  return false;
//}

void StorageEngineMock::unloadCollection(TRI_vocbase_t* vocbase, arangodb::LogicalCollection* collection) {
  TRI_ASSERT(false);
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

TransactionStateMock::TransactionStateMock(TRI_vocbase_t* vocbase)
  : TransactionState(vocbase, arangodb::transaction::Options()) {
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
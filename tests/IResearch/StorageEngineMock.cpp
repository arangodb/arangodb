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

#include "Basics/LocalTaskQueue.h"
#include "Basics/Result.h"
#include "Basics/StaticStrings.h"
#include "Indexes/IndexIterator.h"
#include "IResearch/IResearchMMFilesLink.h"
#include "IResearch/VelocyPackHelper.h"
#include "Utils/OperationOptions.h"
#include "velocypack/Iterator.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/ticks.h"

void ContextDataMock::pinData(arangodb::LogicalCollection* collection) {
  if (collection) {
    pinned.emplace(collection->cid());
  }
}

bool ContextDataMock::isPinned(TRI_voc_cid_t cid) const {
  return pinned.find(cid) != pinned.end();
}

std::function<void()> PhysicalCollectionMock::before = []()->void {};

PhysicalCollectionMock::PhysicalCollectionMock(arangodb::LogicalCollection* collection, arangodb::velocypack::Slice const& info)
  : PhysicalCollection(collection, info), lastId(0) {
}

arangodb::PhysicalCollection* PhysicalCollectionMock::clone(arangodb::LogicalCollection*) {
  before();
  TRI_ASSERT(false);
  return nullptr;
}

int PhysicalCollectionMock::close() {
  before();

  for (auto& index: _indexes) {
    index->unload();
  }

  return TRI_ERROR_NO_ERROR; // assume close successful
}

std::shared_ptr<arangodb::Index> PhysicalCollectionMock::createIndex(arangodb::transaction::Methods* trx, arangodb::velocypack::Slice const& info, bool& created) {
  before();

  std::vector<std::pair<TRI_voc_rid_t, arangodb::velocypack::Slice>> docs;

  for (size_t i = 0, count = documents.size(); i < count; ++i) {
    auto& entry = documents[i];

    if (entry.second) {
      TRI_voc_rid_t revId = i + 1; // always > 0

      docs.emplace_back(revId, entry.first.slice());
    }
  }

  auto index = arangodb::iresearch::IResearchMMFilesLink::make(++lastId, _logicalCollection, info);

  if (!index) {
    return nullptr;
  }

  boost::asio::io_service ioService;
  auto poster = [&ioService](std::function<void()> fn) -> void {
    ioService.post(fn);
  };
  arangodb::basics::LocalTaskQueue taskQueue(poster);
  std::shared_ptr<arangodb::basics::LocalTaskQueue> taskQueuePtr(&taskQueue, [](arangodb::basics::LocalTaskQueue*)->void{});

  index->batchInsert(trx, docs, taskQueuePtr);

  if (TRI_ERROR_NO_ERROR != taskQueue.status()) {
    return nullptr;
  }

  _indexes.emplace_back(std::move(index));
  created = true;

  return _indexes.back();
}

void PhysicalCollectionMock::deferDropCollection(std::function<bool(arangodb::LogicalCollection*)> callback) {
  before();

  callback(_logicalCollection); // assume noone is using this collection (drop immediately)
}

bool PhysicalCollectionMock::dropIndex(TRI_idx_iid_t iid) {
  before();

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
  before();
  TRI_ASSERT(false);
}

std::unique_ptr<arangodb::IndexIterator> PhysicalCollectionMock::getAllIterator(arangodb::transaction::Methods* trx, arangodb::ManagedDocumentResult* mdr, bool reverse) const {
  before();
  TRI_ASSERT(false);
  return nullptr;
}

std::unique_ptr<arangodb::IndexIterator> PhysicalCollectionMock::getAnyIterator(arangodb::transaction::Methods* trx, arangodb::ManagedDocumentResult* mdr) const {
  before();
  TRI_ASSERT(false);
  return nullptr;
}

void PhysicalCollectionMock::getPropertiesVPack(arangodb::velocypack::Builder&) const {
  before();
  TRI_ASSERT(false);
}

void PhysicalCollectionMock::getPropertiesVPackCoordinator(arangodb::velocypack::Builder&) const {
  before();
  TRI_ASSERT(false);
}

arangodb::Result PhysicalCollectionMock::insert(arangodb::transaction::Methods* trx, arangodb::velocypack::Slice const newSlice, arangodb::ManagedDocumentResult& result, arangodb::OperationOptions& options, TRI_voc_tick_t& resultMarkerTick, bool lock) {
  before();

  arangodb::velocypack::Builder builder;
  arangodb::velocypack::Slice fromSlice;
  arangodb::velocypack::Slice toSlice;
  auto isEdgeCollection = _logicalCollection->type() == TRI_COL_TYPE_EDGE;

  if (isEdgeCollection) {
    fromSlice = newSlice.get(arangodb::StaticStrings::FromString);
    toSlice = newSlice.get(arangodb::StaticStrings::ToString);
  }

  auto res = newObjectForInsert(
    trx,
    newSlice,
    fromSlice,
    toSlice,
    isEdgeCollection,
    builder,
    options.isRestore
  );

  if (TRI_ERROR_NO_ERROR != res) {
    return res;
  }

  documents.emplace_back(std::move(builder), true);

  TRI_voc_rid_t revId = documents.size(); // always > 0

  result.setUnmanaged(documents.back().first.data(), revId);

  for (auto& index : _indexes) {
    if (!index->insert(trx, revId, newSlice, false).ok()) {
      return arangodb::Result(TRI_ERROR_BAD_PARAMETER);
    }
  }

  return arangodb::Result();
}

void PhysicalCollectionMock::invokeOnAllElements(arangodb::transaction::Methods*, std::function<bool(arangodb::DocumentIdentifierToken const&)> callback) {
  before();

  arangodb::DocumentIdentifierToken token;

  for (size_t i = 0, count = documents.size(); i < count; ++i) {
    token._data = i + 1; // '_data' always > 0

    if (documents[i].second && !callback(token)) {
      return;
    }
  }
}

std::shared_ptr<arangodb::Index> PhysicalCollectionMock::lookupIndex(arangodb::velocypack::Slice const&) const {
  before();
  TRI_ASSERT(false);
  return nullptr;
}

arangodb::DocumentIdentifierToken PhysicalCollectionMock::lookupKey(arangodb::transaction::Methods*, arangodb::velocypack::Slice const&) {
  before();
  TRI_ASSERT(false);
  return arangodb::DocumentIdentifierToken();
}

size_t PhysicalCollectionMock::memory() const {
  before();
  TRI_ASSERT(false);
  return 0;
}

uint64_t PhysicalCollectionMock::numberDocuments(arangodb::transaction::Methods*) const {
  before();
  TRI_ASSERT(false);
  return 0;
}

void PhysicalCollectionMock::open(bool ignoreErrors) {
  before();
  TRI_ASSERT(false);
}

std::string const& PhysicalCollectionMock::path() const {
  before();

  return physicalPath;
}

arangodb::Result PhysicalCollectionMock::persistProperties() {
  before();
  TRI_ASSERT(false);
  return arangodb::Result(TRI_ERROR_INTERNAL);
}

void PhysicalCollectionMock::prepareIndexes(arangodb::velocypack::Slice indexesSlice) {
  before();
  // NOOP
}

arangodb::Result PhysicalCollectionMock::read(arangodb::transaction::Methods*, arangodb::StringRef const& key, arangodb::ManagedDocumentResult& result, bool) {
  before();
  TRI_ASSERT(false);
  return TRI_ERROR_INTERNAL;
}

arangodb::Result PhysicalCollectionMock::read(arangodb::transaction::Methods*, arangodb::velocypack::Slice const& key, arangodb::ManagedDocumentResult& result, bool) {
  before();
  TRI_ASSERT(false);
  return TRI_ERROR_INTERNAL;
}

bool PhysicalCollectionMock::readDocument(arangodb::transaction::Methods* trx, arangodb::DocumentIdentifierToken const& token, arangodb::ManagedDocumentResult& result) {
  before();

  if (token._data > documents.size()) {
    return false;
  }

  auto& entry = documents[token._data - 1]; // '_data' always > 0

  if (!entry.second) {
    return false; // removed document
  }

  result.setUnmanaged(entry.first.data(), token._data);

  return true;
}

bool PhysicalCollectionMock::readDocumentWithCallback(arangodb::transaction::Methods* trx, arangodb::DocumentIdentifierToken const& token, arangodb::IndexIterator::DocumentCallback const& cb) {
  before();

  if (token._data > documents.size()) {
    return false;
  }

  auto& entry = documents[token._data - 1]; // '_data' always > 0

  if (!entry.second) {
    return false; // removed document
  }

  cb(token, VPackSlice(entry.first.data()));

  return true;
}

arangodb::Result PhysicalCollectionMock::remove(arangodb::transaction::Methods* trx, arangodb::velocypack::Slice const slice, arangodb::ManagedDocumentResult& previous, arangodb::OperationOptions& options, TRI_voc_tick_t& resultMarkerTick, bool lock, TRI_voc_rid_t const& revisionId, TRI_voc_rid_t& prevRev) {
  before();

  auto key = slice.get(arangodb::StaticStrings::KeyString);

  for (size_t i = documents.size(); i; --i) {
    auto& entry = documents[i - 1];

    if (!entry.second) {
      continue; // removed document
    }

    auto& doc = entry.first;

    if (key == doc.slice().get(arangodb::StaticStrings::KeyString)) {
      TRI_voc_rid_t revId = i; // always > 0

      entry.second = false;
      previous.setUnmanaged(doc.data(), revId);
      prevRev = revId;

      return arangodb::Result(TRI_ERROR_NO_ERROR); // assume document was removed
    }
  }

  return arangodb::Result(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
}

arangodb::Result PhysicalCollectionMock::replace(arangodb::transaction::Methods* trx, arangodb::velocypack::Slice const newSlice, arangodb::ManagedDocumentResult& result, arangodb::OperationOptions& options, TRI_voc_tick_t& resultMarkerTick, bool lock, TRI_voc_rid_t& prevRev, arangodb::ManagedDocumentResult& previous, TRI_voc_rid_t const revisionId, arangodb::velocypack::Slice const fromSlice, arangodb::velocypack::Slice const toSlice) {
  before();
  TRI_ASSERT(false);
  return TRI_ERROR_INTERNAL;
}

int PhysicalCollectionMock::restoreIndex(arangodb::transaction::Methods*, arangodb::velocypack::Slice const&, std::shared_ptr<arangodb::Index>&) {
  before();
  TRI_ASSERT(false);
  return TRI_ERROR_INTERNAL;
}

TRI_voc_rid_t PhysicalCollectionMock::revision(arangodb::transaction::Methods*) const {
  before();
  TRI_ASSERT(false);
  return 0;
}

void PhysicalCollectionMock::setPath(std::string const& value) {
  before();
  physicalPath = value;
}

void PhysicalCollectionMock::truncate(arangodb::transaction::Methods* trx, arangodb::OperationOptions& options) {
  before();
  documents.clear();
}

arangodb::Result PhysicalCollectionMock::update(arangodb::transaction::Methods* trx, arangodb::velocypack::Slice const newSlice, arangodb::ManagedDocumentResult& result, arangodb::OperationOptions& options, TRI_voc_tick_t& resultMarkerTick, bool lock, TRI_voc_rid_t& prevRev, arangodb::ManagedDocumentResult& previous, TRI_voc_rid_t const& revisionId, arangodb::velocypack::Slice const key) {
  before();

  for (size_t i = documents.size(); i; --i) {
    auto& entry = documents[i - 1];

    if (!entry.second) {
      continue; // removed document
    }

    auto& doc = entry.first;

    if (key == doc.slice().get(arangodb::StaticStrings::KeyString)) {
      TRI_voc_rid_t revId = i; // always > 0

      if (!options.mergeObjects) {
        entry.second = false;
        previous.setUnmanaged(doc.data(), revId);
        prevRev = revId;

        return insert(trx, newSlice, result, options, resultMarkerTick, lock);
      }

      arangodb::velocypack::Builder builder;

      builder.openObject();

      if (!arangodb::iresearch::mergeSlice(builder, newSlice)) {
        return arangodb::Result(TRI_ERROR_BAD_PARAMETER);
      }

      for (arangodb::velocypack::ObjectIterator itr(doc.slice()); itr.valid(); ++itr) {
        auto key = itr.key().copyString();

        if (!newSlice.hasKey(key)) {
          builder.add(key, itr.value());
        }
      }

      builder.close();
      entry.second = false;
      previous.setUnmanaged(doc.data(), revId);
      prevRev = revId;

      return insert(trx, builder.slice(), result, options, resultMarkerTick, lock);
    }
  }

  return arangodb::Result(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
}

arangodb::Result PhysicalCollectionMock::updateProperties(arangodb::velocypack::Slice const& slice, bool doSync) {
  before();

  return arangodb::Result(TRI_ERROR_NO_ERROR); // assume mock collection updated OK
}

std::function<void()> PhysicalViewMock::before = []()->void {};
int PhysicalViewMock::persistPropertiesResult;

PhysicalViewMock::PhysicalViewMock(arangodb::LogicalView* view, arangodb::velocypack::Slice const& info)
  : PhysicalView(view, info) {
}

arangodb::PhysicalView* PhysicalViewMock::clone(arangodb::LogicalView*, arangodb::PhysicalView*) {
  before();
  TRI_ASSERT(false);
  return nullptr;
}

void PhysicalViewMock::drop() {
  before();
  // NOOP, assume physical view dropped OK
}

void PhysicalViewMock::getPropertiesVPack(arangodb::velocypack::Builder&, bool includeSystem /*= false*/) const {
  before();
  TRI_ASSERT(false);
}

void PhysicalViewMock::open() {
  before();
  TRI_ASSERT(false);
}

std::string const& PhysicalViewMock::path() const {
  before();

  return physicalPath;
}

arangodb::Result PhysicalViewMock::persistProperties() {
  before();

  return arangodb::Result(persistPropertiesResult);
}

void PhysicalViewMock::setPath(std::string const& value) {
  before();
  physicalPath = value;
}

arangodb::Result PhysicalViewMock::updateProperties(arangodb::velocypack::Slice const& slice, bool doSync) {
  before();
  TRI_ASSERT(false);
  return arangodb::Result(TRI_ERROR_INTERNAL);
}

bool StorageEngineMock::inRecoveryResult = false;

StorageEngineMock::StorageEngineMock()
  : StorageEngine(nullptr, "", "", nullptr),
    _releasedTick(0) {
}

void StorageEngineMock::addAqlFunctions() {
  // NOOP
}

void StorageEngineMock::addOptimizerRules() {
  // NOOP
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
  // does nothing
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
  return new ContextDataMock();
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

TRI_voc_tick_t StorageEngineMock::currentTick() const {
  return TRI_CurrentTickServer();
}

std::string StorageEngineMock::databasePath(TRI_vocbase_t const* vocbase) const {
  return ""; // no valid path filesystem persisted, return empty string
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

arangodb::Result StorageEngineMock::renameView(TRI_vocbase_t* vocbase, std::shared_ptr<arangodb::LogicalView>, 
                                               std::string const& newName) {
  return arangodb::Result(TRI_ERROR_NO_ERROR); // assume mock view renames OK
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
  arangodb::velocypack::Builder system;

  system.openObject();
  system.add("name", arangodb::velocypack::Value(TRI_VOC_SYSTEM_DATABASE));
  system.close();

  // array expected
  result.openArray();
  result.add(system.slice());
  result.close();
}

std::shared_ptr<arangodb::velocypack::Builder> StorageEngineMock::getReplicationApplierConfiguration(TRI_vocbase_t* vocbase, int& result) {
  result = TRI_ERROR_FILE_NOT_FOUND; // assume no ReplicationApplierConfiguration for vocbase

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

bool StorageEngineMock::inRecovery() {
  return inRecoveryResult;
}

arangodb::Result StorageEngineMock::lastLogger(TRI_vocbase_t*, std::shared_ptr<arangodb::transaction::Context>, uint64_t, uint64_t, std::shared_ptr<VPackBuilder>&) {
  TRI_ASSERT(false);
  return arangodb::Result(false);
}

TRI_vocbase_t* StorageEngineMock::openDatabase(arangodb::velocypack::Slice const& args, bool isUpgrade, int& status) {
  if (!args.isObject() || !args.hasKey("name") || !args.get("name").isString()) {
    status = TRI_ERROR_ARANGO_DATABASE_NAME_INVALID;

    return nullptr;
  }

  auto vocbase = irs::memory::make_unique<TRI_vocbase_t>(
    TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
    vocbases.size(),
    args.get("name").copyString()
  );

  vocbases.emplace_back(std::move(vocbase));

  return vocbases.back().get();
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

TRI_voc_tick_t StorageEngineMock::releasedTick() const {
  return _releasedTick;
}

void StorageEngineMock::releaseTick(TRI_voc_tick_t tick) {
  _releasedTick = tick;
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

TransactionCollectionMock::TransactionCollectionMock(arangodb::TransactionState* state, TRI_voc_cid_t cid)
  : TransactionCollection(state, cid, arangodb::AccessMode::Type::NONE) {
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

bool TransactionCollectionMock::isLocked(arangodb::AccessMode::Type type, int nestingLevel) const {
  return lockType == type;
}

int TransactionCollectionMock::lock() {
  TRI_ASSERT(false);
  return TRI_ERROR_INTERNAL;
}

int TransactionCollectionMock::lock(arangodb::AccessMode::Type type, int nestingLevel) {
  lockType = type;

  return TRI_ERROR_NO_ERROR;
}

void TransactionCollectionMock::release() {
  if (_collection) {
    _transaction->vocbase()->releaseCollection(_collection);
    _collection = nullptr;
  }
}

int TransactionCollectionMock::unlock(arangodb::AccessMode::Type type, int nestingLevel) {
  if (lockType != type) {
    return TRI_ERROR_BAD_PARAMETER;
  }

  lockType = arangodb::AccessMode::Type::NONE;

  return TRI_ERROR_NO_ERROR;
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

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "StorageEngineMock.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/AstNode.h"
#include "Basics/Result.h"
#include "Basics/DownCast.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/asio_ns.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "ClusterEngine/ClusterEngine.h"
#include "ClusterEngine/ClusterIndexFactory.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchInvertedIndex.h"
#include "IResearch/IResearchLinkCoordinator.h"
#include "IResearch/IResearchRocksDBLink.h"
#include "IResearch/VelocyPackHelper.h"
#include "Indexes/IndexIterator.h"
#include "Indexes/SimpleAttributeEqualityMatcher.h"
#include "Indexes/SortedIndexAttributeMatcher.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "RestServer/FlushFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Helpers.h"
#include "Transaction/Hints.h"
#include "Transaction/Manager.h"
#include "Transaction/ManagerFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "VocBase/ticks.h"
#include "Futures/Future.h"

#include "Mocks/IResearchLinkMock.h"
#include "Mocks/IResearchInvertedIndexMock.h"
#include "Mocks/PhysicalCollectionMock.h"

#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>

namespace {

struct IndexFactoryMock : arangodb::IndexFactory {
  IndexFactoryMock(arangodb::ArangodServer& server, bool injectClusterIndexes)
      : IndexFactory(server) {
    if (injectClusterIndexes) {
      arangodb::ClusterIndexFactory::linkIndexFactories(server, *this);
    }
  }

  virtual void fillSystemIndexes(arangodb::LogicalCollection& col,
                                 std::vector<std::shared_ptr<arangodb::Index>>&
                                     systemIndexes) const override {
    // NOOP
  }

  /// @brief create indexes from a list of index definitions
  virtual void prepareIndexes(
      arangodb::LogicalCollection& col,
      arangodb::velocypack::Slice indexesSlice,
      std::vector<std::shared_ptr<arangodb::Index>>& indexes) const override {
    // NOOP
  }
};

}  // namespace

std::shared_ptr<arangodb::iresearch::IResearchLinkMock>
StorageEngineMock::buildLinkMock(arangodb::IndexId id,
                                 arangodb::LogicalCollection& collection,
                                 VPackSlice const& info) {
  auto index = std::shared_ptr<arangodb::iresearch::IResearchLinkMock>(
      new arangodb::iresearch::IResearchLinkMock(id, collection));
  bool pathExists = false;
  auto cleanup = arangodb::scopeGuard([&]() noexcept {
    if (pathExists) {
      try {
        index->unload();  // TODO(MBkkt) unload should be implicit noexcept?
      } catch (...) {
      }
    } else {
      index->drop();
    }
  });
  auto res =
      static_cast<arangodb::iresearch::IResearchLinkMock*>(index.get())
          ->init(info, pathExists, []() -> irs::directory_attributes {
            if (arangodb::iresearch::IResearchLinkMock::InitCallback !=
                nullptr) {
              return arangodb::iresearch::IResearchLinkMock::InitCallback();
            }
            return irs::directory_attributes{};
          });

  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }
  cleanup.cancel();

  return index;
}

std::shared_ptr<arangodb::iresearch::IResearchInvertedIndexMock>
StorageEngineMock::buildInvertedIndexMock(
    arangodb::IndexId id, arangodb::LogicalCollection& collection,
    VPackSlice const& info) {
  std::string name;

  if (info.isObject()) {
    VPackSlice sub = info.get(arangodb::StaticStrings::IndexName);
    if (sub.isString()) {
      name = sub.copyString();
    }
  }
  if (name.empty()) {
    // we couldn't extract name of index for some reason
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_ATTRIBUTE_PARSER_FAILED);
  }

  arangodb::iresearch::IResearchInvertedIndexMeta meta;
  std::string errField;
  meta.init(collection.vocbase().server(), info, false, errField,
            collection.vocbase().name());

  auto attrs = arangodb::iresearch::IResearchInvertedIndex::fields(meta);
  auto index = std::shared_ptr<arangodb::iresearch::IResearchInvertedIndexMock>(
      new arangodb::iresearch::IResearchInvertedIndexMock(id, collection, name,
                                                          attrs, false, true));

  bool pathExists = false;
  auto cleanup = arangodb::scopeGuard([&]() noexcept {
    if (pathExists) {
      try {
        index->unload();  // TODO(MBkkt) unload should be implicit noexcept?
      } catch (...) {
      }
    } else {
      index->drop();
    }
  });

  auto res =
      static_cast<arangodb::iresearch::IResearchInvertedIndexMock*>(index.get())
          ->init(info, pathExists, []() -> irs::directory_attributes {
            if (arangodb::iresearch::IResearchInvertedIndexMock::InitCallback !=
                nullptr) {
              return arangodb::iresearch::IResearchInvertedIndexMock::
                  InitCallback();
            }
            return irs::directory_attributes{};
          });

  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }
  cleanup.cancel();
  return index;
}

std::function<void()> StorageEngineMock::before = []() -> void {};
arangodb::RecoveryState StorageEngineMock::recoveryStateResult =
    arangodb::RecoveryState::DONE;
TRI_voc_tick_t StorageEngineMock::recoveryTickResult = 0;
std::function<void()> StorageEngineMock::recoveryTickCallback = []() -> void {};

/*static*/ std::string StorageEngineMock::versionFilenameResult;

StorageEngineMock::StorageEngineMock(arangodb::ArangodServer& server,
                                     bool injectClusterIndexes)
    : StorageEngine(server, "Mock", "", std::numeric_limits<size_t>::max(),
                    std::unique_ptr<arangodb::IndexFactory>(
                        new IndexFactoryMock(server, injectClusterIndexes))),
      vocbaseCount(1),
      _releasedTick(0) {}

arangodb::HealthData StorageEngineMock::healthCheck() { return {}; }

arangodb::WalAccess const* StorageEngineMock::walAccess() const {
  TRI_ASSERT(false);
  return nullptr;
}

bool StorageEngineMock::autoRefillIndexCaches() const { return false; }

bool StorageEngineMock::autoRefillIndexCachesOnFollowers() const {
  return false;
}

void StorageEngineMock::addOptimizerRules(
    arangodb::aql::OptimizerRulesFeature& /*feature*/) {
  before();
  // NOOP
}

void StorageEngineMock::addRestHandlers(
    arangodb::rest::RestHandlerFactory& handlerFactory) {
  TRI_ASSERT(false);
}

void StorageEngineMock::addV8Functions() { TRI_ASSERT(false); }

void StorageEngineMock::changeCollection(
    TRI_vocbase_t& vocbase, arangodb::LogicalCollection const& collection) {
  // NOOP, assume physical collection changed OK
}

arangodb::Result StorageEngineMock::changeView(
    arangodb::LogicalView const& view, arangodb::velocypack::Slice update) {
  before();
  std::pair key{view.vocbase().id(), view.id()};
  TRI_ASSERT(views.find(key) != views.end());
  views.emplace(key, update);
  return {};
}

void StorageEngineMock::createCollection(
    TRI_vocbase_t& vocbase, arangodb::LogicalCollection const& collection) {}

arangodb::Result StorageEngineMock::createLoggerState(TRI_vocbase_t*,
                                                      VPackBuilder&) {
  TRI_ASSERT(false);
  return arangodb::Result(TRI_ERROR_NOT_IMPLEMENTED);
}

std::unique_ptr<arangodb::PhysicalCollection>
StorageEngineMock::createPhysicalCollection(
    arangodb::LogicalCollection& collection,
    arangodb::velocypack::Slice /*info*/) {
  before();
  return std::make_unique<PhysicalCollectionMock>(collection);
}

arangodb::Result StorageEngineMock::createTickRanges(VPackBuilder&) {
  TRI_ASSERT(false);
  return arangodb::Result(TRI_ERROR_NOT_IMPLEMENTED);
}

std::unique_ptr<arangodb::transaction::Manager>
StorageEngineMock::createTransactionManager(
    arangodb::transaction::ManagerFeature& feature) {
  return std::make_unique<arangodb::transaction::Manager>(feature);
}

std::shared_ptr<arangodb::TransactionState>
StorageEngineMock::createTransactionState(
    TRI_vocbase_t& vocbase, arangodb::TransactionId tid,
    arangodb::transaction::Options const& options) {
  return std::make_shared<TransactionStateMock>(vocbase, tid, options, *this);
}

arangodb::Result StorageEngineMock::createView(
    TRI_vocbase_t& vocbase, arangodb::DataSourceId id,
    arangodb::LogicalView const& view) {
  before();
  TRI_ASSERT(views.find(std::make_pair(vocbase.id(), view.id())) ==
             views.end());  // called after createView()
  arangodb::velocypack::Builder builder;

  builder.openObject();
  auto res = view.properties(
      builder, arangodb::LogicalDataSource::Serialization::Persistence);
  if (!res.ok()) {
    return res;
  }
  builder.close();
  views[std::make_pair(vocbase.id(), view.id())] = std::move(builder);

  return arangodb::Result(TRI_ERROR_NO_ERROR);  // assume mock view persisted OK
}

arangodb::Result StorageEngineMock::compactAll(bool changeLevels,
                                               bool compactBottomMostLevel) {
  TRI_ASSERT(false);
  return arangodb::Result();
}

TRI_voc_tick_t StorageEngineMock::currentTick() const {
  return _engineTick.load();
}

arangodb::Result StorageEngineMock::dropCollection(
    TRI_vocbase_t& vocbase, arangodb::LogicalCollection& collection) {
  return arangodb::Result(
      TRI_ERROR_NO_ERROR);  // assume physical collection dropped OK
}

arangodb::Result StorageEngineMock::dropDatabase(TRI_vocbase_t& vocbase) {
  TRI_ASSERT(false);
  return arangodb::Result();
}

arangodb::Result StorageEngineMock::dropView(
    TRI_vocbase_t const& vocbase, arangodb::LogicalView const& view) {
  before();
  TRI_ASSERT(views.find(std::make_pair(vocbase.id(), view.id())) !=
             views.end());
  views.erase(std::make_pair(vocbase.id(), view.id()));

  return arangodb::Result(TRI_ERROR_NO_ERROR);  // assume mock view dropped OK
}

arangodb::Result StorageEngineMock::firstTick(uint64_t&) {
  TRI_ASSERT(false);
  return arangodb::Result(TRI_ERROR_NOT_IMPLEMENTED);
}

void StorageEngineMock::getCollectionInfo(TRI_vocbase_t& vocbase,
                                          arangodb::DataSourceId cid,
                                          arangodb::velocypack::Builder& result,
                                          bool includeIndexes,
                                          TRI_voc_tick_t maxTick) {
  arangodb::velocypack::Builder parameters;

  parameters.openObject();
  parameters.close();

  result.openObject();
  result.add("parameters",
             parameters.slice());  // required entry of type object
  result.close();

  // nothing more required, assume info used for PhysicalCollectionMock
}

ErrorCode StorageEngineMock::getCollectionsAndIndexes(
    TRI_vocbase_t& vocbase, arangodb::velocypack::Builder& result,
    bool wasCleanShutdown, bool isUpgrade) {
  TRI_ASSERT(false);
  return TRI_ERROR_INTERNAL;
}

void StorageEngineMock::getDatabases(arangodb::velocypack::Builder& result) {
  before();
  arangodb::velocypack::Builder system;

  system.openObject();
  system.add("name", arangodb::velocypack::Value(
                         arangodb::StaticStrings::SystemDatabase));
  system.close();

  // array expected
  result.openArray();
  result.add(system.slice());
  result.close();
}

void StorageEngineMock::cleanupReplicationContexts() {
  // nothing to do here
}

arangodb::velocypack::Builder
StorageEngineMock::getReplicationApplierConfiguration(TRI_vocbase_t& vocbase,
                                                      ErrorCode& result) {
  before();
  result =
      TRI_ERROR_FILE_NOT_FOUND;  // assume no ReplicationApplierConfiguration
                                 // for vocbase

  return arangodb::velocypack::Builder();
}

arangodb::velocypack::Builder
StorageEngineMock::getReplicationApplierConfiguration(ErrorCode& status) {
  before();
  status = TRI_ERROR_FILE_NOT_FOUND;

  return arangodb::velocypack::Builder();
}

ErrorCode StorageEngineMock::getViews(TRI_vocbase_t& vocbase,
                                      arangodb::velocypack::Builder& result) {
  result.openArray();

  for (auto& entry : views) {
    result.add(entry.second.slice());
  }

  result.close();

  return TRI_ERROR_NO_ERROR;
}

arangodb::Result StorageEngineMock::handleSyncKeys(
    arangodb::DatabaseInitialSyncer& syncer, arangodb::LogicalCollection& col,
    std::string const& keysId) {
  TRI_ASSERT(false);
  return arangodb::Result();
}

arangodb::RecoveryState StorageEngineMock::recoveryState() {
  return recoveryStateResult;
}
TRI_voc_tick_t StorageEngineMock::recoveryTick() {
  if (recoveryTickCallback) {
    recoveryTickCallback();
  }
  return recoveryTickResult;
}

arangodb::Result StorageEngineMock::lastLogger(
    TRI_vocbase_t& vocbase, uint64_t tickStart, uint64_t tickEnd,
    arangodb::velocypack::Builder& builderSPtr) {
  TRI_ASSERT(false);
  return arangodb::Result(TRI_ERROR_NOT_IMPLEMENTED);
}

std::unique_ptr<TRI_vocbase_t> StorageEngineMock::openDatabase(
    arangodb::CreateDatabaseInfo&& info, bool isUpgrade) {
  before();

  auto new_info = info;
  new_info.setId(++vocbaseCount);

  return std::make_unique<TRI_vocbase_t>(std::move(new_info));
}

TRI_voc_tick_t StorageEngineMock::releasedTick() const {
  before();
  return _releasedTick;
}

void StorageEngineMock::releaseTick(TRI_voc_tick_t tick) {
  before();
  _releasedTick = tick;
}

ErrorCode StorageEngineMock::removeReplicationApplierConfiguration(
    TRI_vocbase_t& vocbase) {
  TRI_ASSERT(false);
  return TRI_ERROR_NO_ERROR;
}

ErrorCode StorageEngineMock::removeReplicationApplierConfiguration() {
  TRI_ASSERT(false);
  return TRI_ERROR_NO_ERROR;
}

arangodb::Result StorageEngineMock::renameCollection(
    TRI_vocbase_t& vocbase, arangodb::LogicalCollection const& collection,
    std::string const& oldName) {
  TRI_ASSERT(false);
  return arangodb::Result(TRI_ERROR_INTERNAL);
}

ErrorCode StorageEngineMock::saveReplicationApplierConfiguration(
    TRI_vocbase_t& vocbase, arangodb::velocypack::Slice slice, bool doSync) {
  TRI_ASSERT(false);
  return TRI_ERROR_NO_ERROR;
}

ErrorCode StorageEngineMock::saveReplicationApplierConfiguration(
    arangodb::velocypack::Slice, bool) {
  TRI_ASSERT(false);
  return TRI_ERROR_NO_ERROR;
}

std::string StorageEngineMock::versionFilename(TRI_voc_tick_t) const {
  return versionFilenameResult;
}

void StorageEngineMock::waitForEstimatorSync(std::chrono::milliseconds) {
  TRI_ASSERT(false);
}

std::vector<std::string> StorageEngineMock::currentWalFiles() const {
  return std::vector<std::string>();
}

arangodb::Result StorageEngineMock::flushWal(bool waitForSync,
                                             bool waitForCollector) {
  TRI_ASSERT(false);
  return arangodb::Result();
}
arangodb::Result StorageEngineMock::dropReplicatedState(
    TRI_vocbase_t& vocbase,
    std::unique_ptr<arangodb::replication2::storage::IStorageEngineMethods>&
        ptr) {
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
arangodb::ResultT<
    std::unique_ptr<arangodb::replication2::storage::IStorageEngineMethods>>
StorageEngineMock::createReplicatedState(
    TRI_vocbase_t& vocbase, arangodb::replication2::LogId id,
    const arangodb::replication2::storage::PersistedStateInfo& info) {
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

class TransactionCollectionMock : public arangodb::TransactionCollection {
 public:
  TransactionCollectionMock(arangodb::TransactionState* state,
                            arangodb::DataSourceId cid,
                            arangodb::AccessMode::Type accessType);
  bool canAccess(arangodb::AccessMode::Type accessType) const override;
  bool hasOperations() const override;
  void releaseUsage() override;
  arangodb::Result lockUsage() override;

 private:
  arangodb::Result doLock(arangodb::AccessMode::Type type) override;
  arangodb::Result doUnlock(arangodb::AccessMode::Type type) override;
};

TransactionCollectionMock::TransactionCollectionMock(
    arangodb::TransactionState* state, arangodb::DataSourceId cid,
    arangodb::AccessMode::Type accessType)
    : TransactionCollection(state, cid, accessType) {}

bool TransactionCollectionMock::canAccess(
    arangodb::AccessMode::Type accessType) const {
  return nullptr != _collection;  // collection must have be opened previously
}

bool TransactionCollectionMock::hasOperations() const {
  TRI_ASSERT(false);
  return false;
}

void TransactionCollectionMock::releaseUsage() {
  if (_collection) {
    if (!arangodb::ServerState::instance()->isCoordinator()) {
      _transaction->vocbase().releaseCollection(_collection.get());
    }
    _collection = nullptr;
  }
}

arangodb::Result TransactionCollectionMock::lockUsage() {
  bool shouldLock = !arangodb::AccessMode::isNone(_accessType);

  if (shouldLock && !isLocked()) {
    // r/w lock the collection
    arangodb::Result res = doLock(_accessType);

    if (res.is(TRI_ERROR_LOCKED)) {
      // TRI_ERROR_LOCKED is not an error, but it indicates that the lock
      // operation has actually acquired the lock (and that the lock has not
      // been held before)
      res.reset();
    } else if (res.fail()) {
      return res;
    }
  }

  if (!_collection) {
    if (arangodb::ServerState::instance()->isCoordinator()) {
      auto& ci = _transaction->vocbase()
                     .server()
                     .getFeature<arangodb::ClusterFeature>()
                     .clusterInfo();
      _collection = ci.getCollectionNT(_transaction->vocbase().name(),
                                       std::to_string(_cid.id()));
    } else {
      _collection = _transaction->vocbase().useCollection(_cid, true);
    }
  }

  return arangodb::Result(_collection ? TRI_ERROR_NO_ERROR
                                      : TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
}

arangodb::Result TransactionCollectionMock::doLock(
    arangodb::AccessMode::Type type) {
  if (_lockType > _accessType) {
    return {TRI_ERROR_INTERNAL};
  }

  _lockType = type;

  return {};
}

arangodb::Result TransactionCollectionMock::doUnlock(
    arangodb::AccessMode::Type type) {
  if (_lockType != type) {
    return {TRI_ERROR_INTERNAL};
  }

  _lockType = arangodb::AccessMode::Type::NONE;

  return {};
}

std::atomic_size_t TransactionStateMock::abortTransactionCount{0};
std::atomic_size_t TransactionStateMock::beginTransactionCount{0};
std::atomic_size_t TransactionStateMock::commitTransactionCount{0};

// ensure each transaction state has a unique ID
TransactionStateMock::TransactionStateMock(
    TRI_vocbase_t& vocbase, arangodb::TransactionId tid,
    arangodb::transaction::Options const& options, StorageEngineMock& engine)
    : TransactionState(vocbase, tid, options), _engine{engine} {}

arangodb::Result TransactionStateMock::abortTransaction(
    arangodb::transaction::Methods* trx) {
  ++abortTransactionCount;
  updateStatus(arangodb::transaction::Status::ABORTED);
  //  releaseUsage();
  resetTransactionId();

  return arangodb::Result();
}

arangodb::Result TransactionStateMock::beginTransaction(
    arangodb::transaction::Hints hints) {
  ++beginTransactionCount;
  _hints = hints;

  arangodb::Result res = useCollections();
  if (res.fail()) {  // something is wrong
    return res;
  }

  if (!res.ok()) {
    updateStatus(arangodb::transaction::Status::ABORTED);
    resetTransactionId();
    return res;
  }
  updateStatus(arangodb::transaction::Status::RUNNING);
  return arangodb::Result();
}

arangodb::futures::Future<arangodb::Result>
TransactionStateMock::commitTransaction(arangodb::transaction::Methods* trx) {
  applyBeforeCommitCallbacks();
  TRI_ASSERT(this == trx->state());
  _engine.incrementTick(numPrimitiveOperations() + 1);
  ++commitTransactionCount;
  updateStatus(arangodb::transaction::Status::COMMITTED);
  resetTransactionId();
  //  releaseUsage();
  applyAfterCommitCallbacks();
  return arangodb::Result();
}

arangodb::Result TransactionStateMock::triggerIntermediateCommit() {
  ADB_PROD_ASSERT(false) << "triggerIntermediateCommit is not supported in "
                            "TransactionStateMock";
  return arangodb::Result{TRI_ERROR_INTERNAL};
}

arangodb::futures::Future<arangodb::Result>
TransactionStateMock::performIntermediateCommitIfRequired(
    arangodb::DataSourceId cid) {
  return arangodb::Result();
}

uint64_t TransactionStateMock::numCommits() const noexcept {
  return commitTransactionCount;
}

uint64_t TransactionStateMock::numIntermediateCommits() const noexcept {
  return 0;
}

void TransactionStateMock::addIntermediateCommits(uint64_t /*value*/) {
  // should never be called during testing
  TRI_ASSERT(false);
}

bool TransactionStateMock::hasFailedOperations() const noexcept {
  return false;  // assume no failed operations
}

TRI_voc_tick_t TransactionStateMock::lastOperationTick() const noexcept {
  return _engine.currentTick();
}

std::unique_ptr<arangodb::TransactionCollection>
TransactionStateMock::createTransactionCollection(
    arangodb::DataSourceId cid, arangodb::AccessMode::Type accessType) {
  return std::make_unique<TransactionCollectionMock>(this, cid, accessType);
}

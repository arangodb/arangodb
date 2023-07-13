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

#pragma once

#include "Basics/Result.h"
#include "Futures/Future.h"
#include "StorageEngine/HealthData.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionState.h"
#include "VocBase/Identifiers/IndexId.h"

#include <atomic>
#include <memory>
#include <string_view>

namespace arangodb {

class PhysicalCollection;
class TransactionCollection;
class TransactionManager;
class WalAccess;
namespace aql {
class OptimizerRulesFeature;
}
namespace iresearch {
class IResearchLinkMock;
class IResearchInvertedIndexMock;
}  // namespace iresearch

}  // namespace arangodb

class StorageEngineMock;

class TransactionStateMock : public arangodb::TransactionState {
 public:
  static std::atomic_size_t abortTransactionCount;
  static std::atomic_size_t beginTransactionCount;
  static std::atomic_size_t commitTransactionCount;

  TransactionStateMock(TRI_vocbase_t& vocbase, arangodb::TransactionId tid,
                       arangodb::transaction::Options const& options,
                       StorageEngineMock& engine);
  [[nodiscard]] bool ensureSnapshot() override { return false; }
  arangodb::Result abortTransaction(
      arangodb::transaction::Methods* trx) override;
  arangodb::Result beginTransaction(
      arangodb::transaction::Hints hints) override;
  arangodb::futures::Future<arangodb::Result> commitTransaction(
      arangodb::transaction::Methods* trx) override;
  arangodb::futures::Future<arangodb::Result>
  performIntermediateCommitIfRequired(arangodb::DataSourceId cid) override;

  void incrementInsert() noexcept { ++_numInserts; }
  void incrementRemove() noexcept { ++_numRemoves; }
  uint64_t numPrimitiveOperations() const noexcept override {
    return _numInserts + _numRemoves;
  }

  uint64_t numCommits() const noexcept override;
  uint64_t numIntermediateCommits() const noexcept override;
  void addIntermediateCommits(uint64_t value) override;
  bool hasFailedOperations() const noexcept override;
  TRI_voc_tick_t lastOperationTick() const noexcept override;

  arangodb::Result triggerIntermediateCommit() override;
  std::unique_ptr<arangodb::TransactionCollection> createTransactionCollection(
      arangodb::DataSourceId cid,
      arangodb::AccessMode::Type accessType) override;

 private:
  uint64_t _numInserts{0};
  uint64_t _numRemoves{0};
  StorageEngineMock& _engine;
};

class StorageEngineMockSnapshot final : public arangodb::StorageSnapshot {
 public:
  StorageEngineMockSnapshot(TRI_voc_tick_t t) : _t(t) {}
  TRI_voc_tick_t tick() const noexcept override { return _t; }

 private:
  TRI_voc_tick_t _t;
};

class StorageEngineMock : public arangodb::StorageEngine {
 public:
  static std::function<void()> before;
  static arangodb::Result flushSubscriptionResult;
  static arangodb::RecoveryState recoveryStateResult;
  static TRI_voc_tick_t recoveryTickResult;
  static std::string versionFilenameResult;
  static std::function<void()> recoveryTickCallback;
  std::map<std::pair<TRI_voc_tick_t, arangodb::DataSourceId>,
           arangodb::velocypack::Builder>
      views;
  std::atomic<size_t> vocbaseCount;

  explicit StorageEngineMock(arangodb::ArangodServer& server,
                             bool injectClusterIndexes = false);
  arangodb::HealthData healthCheck() override;
  void addOptimizerRules(
      arangodb::aql::OptimizerRulesFeature& feature) override;
  void addRestHandlers(
      arangodb::rest::RestHandlerFactory& handlerFactory) override;
  void addV8Functions() override;
  void changeCollection(TRI_vocbase_t& vocbase,
                        arangodb::LogicalCollection const& collection) override;
  arangodb::Result changeView(arangodb::LogicalView const& view,
                              arangodb::velocypack::Slice update) override;

  void createCollection(TRI_vocbase_t& vocbase,
                        arangodb::LogicalCollection const& collection) override;
  arangodb::Result createLoggerState(TRI_vocbase_t*, VPackBuilder&) override;
  std::unique_ptr<arangodb::PhysicalCollection> createPhysicalCollection(
      arangodb::LogicalCollection& collection,
      arangodb::velocypack::Slice /*info*/) override;
  arangodb::Result createTickRanges(VPackBuilder&) override;
  std::unique_ptr<arangodb::transaction::Manager> createTransactionManager(
      arangodb::transaction::ManagerFeature&) override;
  std::shared_ptr<arangodb::TransactionState> createTransactionState(
      TRI_vocbase_t& vocbase, arangodb::TransactionId tid,
      arangodb::transaction::Options const& options) override;
  arangodb::Result createView(TRI_vocbase_t& vocbase, arangodb::DataSourceId id,
                              arangodb::LogicalView const& view) override;
  arangodb::Result compactAll(bool changeLevels,
                              bool compactBottomMostLevel) override;
  TRI_voc_tick_t currentTick() const override;
  arangodb::Result dropCollection(
      TRI_vocbase_t& vocbase, arangodb::LogicalCollection& collection) override;
  arangodb::Result dropDatabase(TRI_vocbase_t& vocbase) override;
  arangodb::Result dropView(TRI_vocbase_t const& vocbase,
                            arangodb::LogicalView const& view) override;
  arangodb::Result firstTick(uint64_t&) override;
  std::vector<std::string> currentWalFiles() const override;
  arangodb::Result flushWal(bool waitForSync, bool waitForCollector) override;
  void getCollectionInfo(TRI_vocbase_t& vocbase, arangodb::DataSourceId cid,
                         arangodb::velocypack::Builder& result,
                         bool includeIndexes, TRI_voc_tick_t maxTick) override;
  ErrorCode getCollectionsAndIndexes(TRI_vocbase_t& vocbase,
                                     arangodb::velocypack::Builder& result,
                                     bool wasCleanShutdown,
                                     bool isUpgrade) override;
  void getDatabases(arangodb::velocypack::Builder& result) override;
  void cleanupReplicationContexts() override;
  arangodb::velocypack::Builder getReplicationApplierConfiguration(
      TRI_vocbase_t& vocbase, ErrorCode& result) override;
  arangodb::velocypack::Builder getReplicationApplierConfiguration(
      ErrorCode& result) override;
  ErrorCode getViews(TRI_vocbase_t& vocbase,
                     arangodb::velocypack::Builder& result) override;
  arangodb::Result handleSyncKeys(arangodb::DatabaseInitialSyncer& syncer,
                                  arangodb::LogicalCollection& col,
                                  std::string const& keysId) override;
  arangodb::RecoveryState recoveryState() override;
  TRI_voc_tick_t recoveryTick() override;

  arangodb::Result lastLogger(
      TRI_vocbase_t& vocbase, uint64_t tickStart, uint64_t tickEnd,
      arangodb::velocypack::Builder& builderSPtr) override;

  std::unique_ptr<TRI_vocbase_t> openDatabase(arangodb::CreateDatabaseInfo&&,
                                              bool isUpgrade) override;
  using StorageEngine::registerCollection;
  using StorageEngine::registerView;
  TRI_voc_tick_t releasedTick() const override;
  void releaseTick(TRI_voc_tick_t) override;
  ErrorCode removeReplicationApplierConfiguration(
      TRI_vocbase_t& vocbase) override;
  ErrorCode removeReplicationApplierConfiguration() override;
  arangodb::Result renameCollection(
      TRI_vocbase_t& vocbase, arangodb::LogicalCollection const& collection,
      std::string const& oldName) override;
  ErrorCode saveReplicationApplierConfiguration(
      TRI_vocbase_t& vocbase, arangodb::velocypack::Slice slice,
      bool doSync) override;
  ErrorCode saveReplicationApplierConfiguration(arangodb::velocypack::Slice,
                                                bool) override;
  std::string versionFilename(TRI_voc_tick_t) const override;
  void waitForEstimatorSync(std::chrono::milliseconds maxWaitTime) override;
  arangodb::WalAccess const* walAccess() const override;

  bool autoRefillIndexCaches() const override;
  bool autoRefillIndexCachesOnFollowers() const override;

  static std::shared_ptr<arangodb::iresearch::IResearchLinkMock> buildLinkMock(
      arangodb::IndexId id, arangodb::LogicalCollection& collection,
      VPackSlice const& info);

  static std::shared_ptr<arangodb::iresearch::IResearchInvertedIndexMock>
  buildInvertedIndexMock(arangodb::IndexId id,
                         arangodb::LogicalCollection& collection,
                         VPackSlice const& info);

  auto dropReplicatedState(
      TRI_vocbase_t& vocbase,
      std::unique_ptr<arangodb::replication2::storage::IStorageEngineMethods>&
          ptr) -> arangodb::Result override;
  auto createReplicatedState(
      TRI_vocbase_t& vocbase, arangodb::replication2::LogId id,
      const arangodb::replication2::storage::PersistedStateInfo& info)
      -> arangodb::ResultT<std::unique_ptr<
          arangodb::replication2::storage::IStorageEngineMethods>> override;

  std::shared_ptr<arangodb::StorageSnapshot> currentSnapshot() override {
    return std::make_shared<StorageEngineMockSnapshot>(currentTick());
  }

  void incrementTick(uint64_t tick) { _engineTick.fetch_add(tick); }

 private:
  TRI_voc_tick_t _releasedTick;
  std::atomic_uint64_t _engineTick{100};
};

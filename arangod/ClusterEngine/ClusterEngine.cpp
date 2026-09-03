////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#include "ClusterEngine.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Basics/Exceptions.h"
#include "Basics/Result.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterAdminOperations.h"
#include "ClusterEngine/ClusterCollection.h"
#include "ClusterEngine/ClusterIndexFactory.h"
#include "ClusterEngine/ClusterTransactionState.h"
#ifdef USE_V8
#include "ClusterEngine/ClusterV8Functions.h"
#endif
#include "Indexes/DefaultIndexFactories.h"
#include "IResearch/IResearchRocksDBInvertedIndex.h"
#include "Logger/Logger.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/Storage/IStorageEngineMethods.h"
#include "RestServer/DatabaseFeature.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "Transaction/Context.h"
#include "Transaction/Manager.h"
#include "Transaction/Options.h"
#include "VocBase/ticks.h"

#include <velocypack/Iterator.h>

using namespace arangodb;
using namespace arangodb::application_features;

namespace {

class ClusterIndexDefinitions final : public IndexFactory {
 public:
  ClusterIndexDefinitions(application_features::ApplicationServer& server,
                          IVectorIndexProvider const& vectorIndexProvider)
      : IndexFactory(server) {
    static const EdgeIndexDefinition edgeIndexDefinition(server);
    static const FulltextIndexDefinition fulltextIndexDefinition(server);
    static const GeoIndexDefinition geoIndexDefinition(server);
    static const Geo1IndexDefinition geo1IndexDefinition(server);
    static const Geo2IndexDefinition geo2IndexDefinition(server);
    static const SecondaryIndexDefinition hashIndexDefinition(server,
                                                              IndexType::Hash);
    static const SecondaryIndexDefinition persistentIndexDefinition(
        server, IndexType::Persistent);
    static const SecondaryIndexDefinition skiplistIndexDefinition(
        server, IndexType::Skiplist);
    static const TtlIndexDefinition ttlIndexDefinition(server, IndexType::TTL);
    static const PrimaryIndexDefinition primaryIndexDefinition(server);
    static const MdiIndexDefinition zkdIndexDefinition(server, IndexType::Zkd);
    static const MdiIndexDefinition mdiIndexDefinition(server, IndexType::MDI);
    static const MdiPrefixedIndexDefinition mdiPrefixedIndexDefinition(server);
    static const VectorIndexDefinition vectorIndexDefinition(
        server, IndexType::Vector, vectorIndexProvider);
    static const iresearch::IResearchInvertedIndexDefinition
        invertedIndexDefinition(server);

    emplace("edge", edgeIndexDefinition);
    emplace("fulltext", fulltextIndexDefinition);
    emplace("geo", geoIndexDefinition);
    emplace("geo1", geo1IndexDefinition);
    emplace("geo2", geo2IndexDefinition);
    emplace("hash", hashIndexDefinition);
    emplace("persistent", persistentIndexDefinition);
    emplace("primary", primaryIndexDefinition);
    emplace("rocksdb", persistentIndexDefinition);
    emplace("skiplist", skiplistIndexDefinition);
    emplace("ttl", ttlIndexDefinition);
    emplace("zkd", zkdIndexDefinition);
    emplace("mdi", mdiIndexDefinition);
    emplace("mdi-prefixed", mdiPrefixedIndexDefinition);
    emplace("vector", vectorIndexDefinition);
    emplace(arangodb::iresearch::IRESEARCH_INVERTED_INDEX_TYPE.data(),
            invertedIndexDefinition);
  }

  std::vector<std::pair<std::string_view, std::string_view>> indexAliases(
      uint32_t apiVersion) const override {
    if (apiVersion == 0) {
      return {
          {"hash", "persistent"},
          {"skiplist", "persistent"},
          {"zkd", "mdi"},
      };
    }
    return {{"zkd", "mdi"}};
  }

  // ClusterIndexFactory implements these itself, never delegated here
  void fillSystemIndexes(LogicalCollection&,
                         std::vector<std::shared_ptr<Index>>&) const override {
    TRI_ASSERT(false);
  }
  void prepareIndexes(LogicalCollection&, velocypack::Slice,
                      std::vector<std::shared_ptr<Index>>&) const override {
    TRI_ASSERT(false);
  }
};

}  // namespace

std::string const ClusterEngine::EngineName("Cluster");

#ifdef ARANGODB_USE_GOOGLE_TESTS
// fall back to the using the mock storage engine
bool ClusterEngine::Mocking = false;
#endif

// create the storage engine
ClusterEngine::ClusterEngine(application_features::ApplicationServer& server,
                             ClusterFeature& clusterFeature,
                             DatabaseFeature& database,
                             metrics::IRegistry& metrics,
                             IVectorIndexProvider const& vectorIndexProvider)
    : StorageEngine(server, EngineName, name(),
                    std::make_unique<ClusterIndexFactory>(server, *this),
                    database),
      _clusterFeature(clusterFeature),
      _metrics(metrics),
      _indexDefinitions(std::make_unique<ClusterIndexDefinitions>(
          server, vectorIndexProvider)) {
  setOptional(true);
}

ClusterEngine::~ClusterEngine() = default;

std::string_view ClusterEngine::typeName() const {
  return RocksDBEngine::kEngineName;
}

HealthData ClusterEngine::healthCheck() { return {}; }

ClusterEngineType ClusterEngine::engineType() const {
#ifdef ARANGODB_USE_GOOGLE_TESTS
  if (ClusterEngine::Mocking) {
    return ClusterEngineType::MockEngine;
  }
#endif
  return ClusterEngineType::RocksDBEngine;
}

// inherited from ApplicationFeature
// ---------------------------------

// preparation phase for storage engine. can be used for internal setup.
// the storage engine must not start any threads here or write any files
void ClusterEngine::prepare() {
  if (!ServerState::instance()->isCoordinator()) {
    setEnabled(false);
  }
}

void ClusterEngine::start() {
  TRI_ASSERT(ServerState::instance()->isCoordinator());
  initTransactionStatistics(_metrics);
}

std::shared_ptr<TransactionState> ClusterEngine::createTransactionState(
    TRI_vocbase_t& vocbase, TransactionId tid,
    transaction::Options const& options,
    transaction::OperationOrigin operationOrigin) {
  return std::make_shared<ClusterTransactionState>(
      vocbase, tid, options, operationOrigin, transactionManager());
}

void ClusterEngine::addParametersForNewCollection(VPackBuilder& builder,
                                                  VPackSlice info) {
  if (engineType() == ClusterEngineType::RocksDBEngine) {
    // deliberately not add objectId
    if (!info.get(StaticStrings::CacheEnabled).isBool()) {
      builder.add(StaticStrings::CacheEnabled, VPackValue(false));
    }
  }
}

// create storage-engine specific collection
std::unique_ptr<PhysicalCollection> ClusterEngine::createPhysicalCollection(
    LogicalCollection& collection, velocypack::Slice info) {
  return std::make_unique<ClusterCollection>(collection, engineType(), info);
}

void ClusterEngine::getStatistics(velocypack::Builder& builder) const {
  Result res = getEngineStatsFromDBServers(_clusterFeature, builder);
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }
}

// inventory functionality
// -----------------------

void ClusterEngine::getDatabases(arangodb::velocypack::Builder& result) {
  LOG_TOPIC("4e3f9", TRACE, Logger::STARTUP) << "getting existing databases";
  // we should only ever need system here
  VPackArrayBuilder arr(&result);
  VPackObjectBuilder obj(&result);
  obj->add(StaticStrings::DataSourceId, VPackValue("1"));  // always pick 1
  obj->add(StaticStrings::DataSourceDeleted, VPackValue(false));
  obj->add(StaticStrings::DataSourceName,
           VPackValue(StaticStrings::SystemDatabase));
}

void ClusterEngine::getCollectionInfo(TRI_vocbase_t& vocbase, DataSourceId cid,
                                      arangodb::velocypack::Builder& builder,
                                      bool includeIndexes,
                                      TRI_voc_tick_t maxTick) {}

ErrorCode ClusterEngine::getCollectionsAndIndexes(
    TRI_vocbase_t& vocbase, arangodb::velocypack::Builder& result,
    bool wasCleanShutdown, bool isUpgrade) {
  return TRI_ERROR_NO_ERROR;
}

ErrorCode ClusterEngine::getViews(TRI_vocbase_t& vocbase,
                                  arangodb::velocypack::Builder& result) {
  return TRI_ERROR_NO_ERROR;
}

// database, collection and index management
// -----------------------------------------

std::unique_ptr<TRI_vocbase_t> ClusterEngine::openDatabase(
    arangodb::CreateDatabaseInfo&& info, bool /*isUpgrade*/) {
  return createDatabase(std::move(info));
}

Result ClusterEngine::dropDatabase(TRI_vocbase_t& database) {
  TRI_ASSERT(false);
  return TRI_ERROR_NOT_IMPLEMENTED;
}

// current recovery state
RecoveryState ClusterEngine::recoveryState() {
  return RecoveryState::DONE;  // never in recovery
}

// current recovery tick
TRI_voc_tick_t ClusterEngine::recoveryTick() {
  return 0;  // never in recovery
}

void ClusterEngine::createCollection(TRI_vocbase_t& vocbase,
                                     LogicalCollection const& collection) {
  TRI_ASSERT(collection.id().isSet());
  TRI_UpdateTickServer(static_cast<TRI_voc_tick_t>(collection.id().id()));
}

arangodb::Result ClusterEngine::dropCollection(TRI_vocbase_t& vocbase,
                                               LogicalCollection& collection) {
  return TRI_ERROR_NOT_IMPLEMENTED;
}

void ClusterEngine::changeCollection(TRI_vocbase_t& vocbase,
                                     LogicalCollection const& collection) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

arangodb::Result ClusterEngine::renameCollection(
    TRI_vocbase_t& vocbase, LogicalCollection const& collection,
    std::string const& oldName) {
  return TRI_ERROR_NOT_IMPLEMENTED;
}

Result ClusterEngine::createView(TRI_vocbase_t& vocbase, DataSourceId id,
                                 arangodb::LogicalView const& /*view*/
) {
  return TRI_ERROR_NOT_IMPLEMENTED;
}

arangodb::Result ClusterEngine::dropView(TRI_vocbase_t const& vocbase,
                                         LogicalView const& view) {
  return TRI_ERROR_NOT_IMPLEMENTED;
}

Result ClusterEngine::changeView(LogicalView const&, velocypack::Slice) {
  if (inRecovery()) {
    return {};
  }
  return TRI_ERROR_NOT_IMPLEMENTED;
}

Result ClusterEngine::compactAll(bool changeLevel,
                                 bool compactBottomMostLevel) {
  return compactOnAllDBServers(_clusterFeature, changeLevel,
                               compactBottomMostLevel);
}

#ifdef USE_V8
/// @brief Add engine-specific V8 functions
void ClusterEngine::addV8Functions() {
  ClusterV8Functions::registerResources();
}
#endif

void ClusterEngine::waitForEstimatorSync() {
  // fixes tests by allowing us to reload the cluster selectivity estimates
  // If test `shell-cluster-collection-selectivity.js` fails consider increasing
  // timeout
  std::this_thread::sleep_for(std::chrono::seconds(5));
}
Result ClusterEngine::dropReplicatedState(
    TRI_vocbase_t& vocbase,
    std::unique_ptr<replication2::storage::IStorageEngineMethods>& ptr) {
  return {TRI_ERROR_NOT_IMPLEMENTED};
}

ResultT<std::unique_ptr<replication2::storage::IStorageEngineMethods>>
ClusterEngine::createReplicatedState(
    TRI_vocbase_t& vocbase, arangodb::replication2::LogId id,
    replication2::storage::PersistedStateInfo const& info) {
  return ResultT<std::unique_ptr<replication2::storage::IStorageEngineMethods>>{
      TRI_ERROR_NOT_IMPLEMENTED};
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

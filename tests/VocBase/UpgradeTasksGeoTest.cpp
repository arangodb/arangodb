////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use it except in compliance with the License.
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
/// @author Copyright 2026, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "VocBase/Methods/UpgradeTasks.h"
#include "VocBase/vocbase.h"

#include "Agency/AgencyFeature.h"
#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "Basics/FeatureFlags.h"
#include "Basics/StaticStrings.h"
#include "Basics/files.h"
#include "Basics/system-functions.h"
#include "Cache/CacheManagerFeature.h"
#include "Cache/CacheOptionsFeature.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/MaintenanceFeature.h"
#include "Cluster/ServerState.h"
#include "Indexes/Index.h"
#include "Metrics/ClusterMetricsFeature.h"
#include "Metrics/MetricsFeature.h"
#include "RestServer/arangod.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/DumpLimitsFeature.h"
#include "RestServer/FlushFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SharedPRNGFeature.h"
#include "RestServer/VectorIndexFeature.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "Sharding/ShardingFeature.h"
#include "RocksDBEngine/RocksDBIndexCacheRefillFeature.h"
#include "RocksDBEngine/RocksDBOptionFeature.h"
#include "RocksDBEngine/RocksDBRecoveryManager.h"
#include "Scheduler/SchedulerFeature.h"
#include "Statistics/StatisticsFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/PhysicalCollection.h"
#include "Utils/ExecContext.h"
#include "Replication2/ReplicatedLog/ReplicatedLogFeature.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/VocbaseInfo.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>

#include <memory>

using namespace arangodb;

// -----------------------------------------------------------------------------
// Test UpgradeTasks::dropLegacyGeoIndexes (arangod/VocBase/Methods/UpgradeTasks.cpp).
// Single-server: loading a definition with geo1/geo2 is accepted and routed to
// geo (index has needsLegacyGeoDrop()). dropLegacyGeoIndexes() drops those
// indexes; it does not rewrite them.
// -----------------------------------------------------------------------------

class UpgradeTasksGeoTest : public ::testing::Test {
 protected:
  std::shared_ptr<options::ProgramOptions> _po;
  ArangodServer _server;
  std::unique_ptr<RocksDBEngine> _engine;
  std::unique_ptr<TRI_vocbase_t> _vocbaseHolder;

  UpgradeTasksGeoTest()
      : _po(std::make_shared<options::ProgramOptions>("", "", "", nullptr)),
        _server(_po, nullptr) {
    ServerState::instance()->setRole(ServerState::ROLE_SINGLE);

    auto& agencyFeature = _server.addFeature<AgencyFeature>();
    auto& roOptions =
        _server.addFeature<RocksDBOptionFeature>(&agencyFeature);
    _server.addFeature<application_features::GreetingsFeaturePhase>(
        std::false_type{});
    auto& selector = _server.addFeature<EngineSelectorFeature>();
    auto& metrics = _server.addFeature<metrics::MetricsFeature>(
        LazyApplicationFeatureReference<QueryRegistryFeature>(nullptr),
        LazyApplicationFeatureReference<StatisticsFeature>(nullptr), selector,
        LazyApplicationFeatureReference<metrics::ClusterMetricsFeature>(nullptr),
        LazyApplicationFeatureReference<ClusterFeature>(nullptr));
    _server.addFeature<ClusterFeature>();
    _server.addFeature<ShardingFeature>();
    _server.addFeature<MaintenanceFeature>();
    auto& dbpath = _server.addFeature<DatabasePathFeature>();
    std::string path =
        std::string(TRI_GetTempPath()) + "arangodb_geo_upgrade_test_" +
        std::to_string(static_cast<uint64_t>(TRI_microtime()));
    dbpath.setDirectory(path);
    auto& vectorIndex = _server.addFeature<VectorIndexFeature>();
    auto& flush = _server.addFeature<FlushFeature>();
    auto& dumpLimits = _server.addFeature<DumpLimitsFeature>();
    auto& schedulerFeature = _server.addFeature<SchedulerFeature>(metrics);
    auto& rocksDbRecoveryManager = _server.addFeature<RocksDBRecoveryManager>();
    auto& databaseFeature = _server.addFeature<DatabaseFeature>();
    auto& rocksDbIndexCacheRefillFeature =
        _server.addFeature<RocksDBIndexCacheRefillFeature>(databaseFeature,
                                                          nullptr, metrics);
    auto& cacheOptions = _server.addFeature<CacheOptionsFeature>();
    auto& sharedPrngFeature = _server.addFeature<SharedPRNGFeature>();
    auto& cacheManagerFeature =
        _server.addFeature<CacheManagerFeature>(cacheOptions, sharedPrngFeature);
    auto* replicatedLogFeature =
        replication2::EnableReplication2
            ? &_server.addFeature<ReplicatedLogFeature>()
            : nullptr;
    _engine = std::make_unique<RocksDBEngine>(
        _server, roOptions, metrics, dbpath, vectorIndex, flush, dumpLimits,
        schedulerFeature, replicatedLogFeature, rocksDbRecoveryManager,
        databaseFeature, rocksDbIndexCacheRefillFeature, cacheManagerFeature,
        agencyFeature);
    selector.setEngineTesting(_engine.get());

    _server.addFeature<QueryRegistryFeature>(
        _server.getFeature<metrics::MetricsFeature>());

    _server.setupDependencies(false);
    for (auto& ref : _server.getOrderedFeatures()) {
      auto& f = ref.get();
      if (f.name() == "DatabasePath" || f.name() == "Database" ||
          f.name() == "Sharding") {
        f.prepare();
      }
    }
    _engine->prepare();
    _engine->start();
  }

  ~UpgradeTasksGeoTest() {
    _vocbaseHolder.reset();

#ifdef TEST_VIRTUAL
    _server.setStateUnsafe(application_features::ApplicationServer::State::IN_SHUTDOWN);
#endif
    _engine->stop();  // Shut down RocksDB before clearing engine pointer.
    _server.getFeature<EngineSelectorFeature>().setEngineTesting(nullptr);
    ServerState::instance()->setRole(ServerState::ROLE_UNDEFINED);
  }

  TRI_vocbase_t& createVocbase(char const* name, uint64_t id) {
    CreateDatabaseInfo info(_server, ExecContext::current());
    EXPECT_TRUE(info.load(name, id).ok());
    _vocbaseHolder = _engine->createDatabase(std::move(info));
    return *_vocbaseHolder;
  }
};

TEST_F(UpgradeTasksGeoTest, dropLegacyGeoIndexes_empty_vocbase_succeeds) {
  TRI_vocbase_t& vocbase = createVocbase("testDbEmpty", 1);
  Result res = methods::UpgradeTasks::dropLegacyGeoIndexes(
      vocbase, VPackSlice::emptyObjectSlice());
  EXPECT_TRUE(res.ok()) << res.errorMessage();
}

TEST_F(UpgradeTasksGeoTest,
       dropLegacyGeoIndexes_vocbase_with_collection_no_legacy_geo_succeeds) {
  TRI_vocbase_t& vocbase = createVocbase("testDbNoLegacy", 2);
  auto collectionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testCollection\", \"globallyUniqueId\": "
      "\"testCollectionGUID\" }");
  auto collection = vocbase.createCollection(collectionJson->slice());
  ASSERT_NE(collection, nullptr);

  Result res = methods::UpgradeTasks::dropLegacyGeoIndexes(
      vocbase, VPackSlice::emptyObjectSlice());
  EXPECT_TRUE(res.ok()) << res.errorMessage();
}

TEST_F(UpgradeTasksGeoTest, index_factory_reroutes_geo1_to_geo_with_legacy_flag) {
  // Load path: collection definition with geo1 index is accepted and routed
  // to geo; IndexFactory sets the legacy flag so the index has needsLegacyGeoDrop().
  TRI_vocbase_t& vocbase = createVocbase("testDb", 1);

  velocypack::Builder def;
  def.openObject();
  def.add("id", velocypack::Value("100"));
  def.add(StaticStrings::DataSourcePlanId, velocypack::Value("100"));
  def.add(StaticStrings::DataSourceName, velocypack::Value("testCol"));
  def.add(StaticStrings::DataSourceGuid, velocypack::Value("testColGuid"));
  def.add(StaticStrings::Version,
          velocypack::Value(static_cast<uint32_t>(
              LogicalCollection::currentVersion())));
  def.add(StaticStrings::DataSourceType,
          velocypack::Value(TRI_COL_TYPE_DOCUMENT));
  def.add(StaticStrings::ObjectId, velocypack::Value("12345"));
  def.add(VPackValue(StaticStrings::Indexes));
  {
    velocypack::ArrayBuilder ab(&def);
    def.openObject();
    def.add(StaticStrings::IndexType, velocypack::Value("primary"));
    def.add(StaticStrings::IndexId, velocypack::Value("1"));
    def.add(VPackValue(StaticStrings::IndexFields));
    { velocypack::ArrayBuilder fb(&def); def.add(VPackValue("_key")); }
    def.close();
    def.openObject();
    def.add(StaticStrings::IndexType, velocypack::Value("geo1"));
    def.add(StaticStrings::IndexId, velocypack::Value("2"));
    def.add(VPackValue(StaticStrings::IndexFields));
    { velocypack::ArrayBuilder fb(&def); def.add(VPackValue("loc")); }
    def.close();
  }
  def.close();

  std::shared_ptr<LogicalCollection> col =
      vocbase.createCollectionObject(def.slice(), false);
  ASSERT_NE(col, nullptr);

  auto indexes = col->getPhysical()->getReadyIndexes();
  bool foundLegacyGeo = false;
  for (auto const& idx : indexes) {
    if (idx->needsLegacyGeoDrop()) {
      foundLegacyGeo = true;
      EXPECT_EQ(idx->type(), Index::TRI_IDX_TYPE_GEO_INDEX)
          << "Legacy geo index should be reported as geo type";
      break;
    }
  }
  EXPECT_TRUE(foundLegacyGeo)
      << "IndexFactory should produce one index with needsLegacyGeoDrop "
         "when loading a geo1 definition";
}

TEST_F(UpgradeTasksGeoTest, dropLegacyGeoIndexes_removes_legacy_geo_index) {
  // Load a collection with a geo1 index (accepted and routed to geo with
  // needsLegacyGeoDrop). Run dropLegacyGeoIndexes; the legacy index must be
  // dropped (not rewritten).
  TRI_vocbase_t& vocbase = createVocbase("testDbDrop", 3);

  velocypack::Builder def;
  def.openObject();
  def.add("id", velocypack::Value("101"));
  def.add(StaticStrings::DataSourcePlanId, velocypack::Value("101"));
  def.add(StaticStrings::DataSourceName, velocypack::Value("testColDrop"));
  def.add(StaticStrings::DataSourceGuid, velocypack::Value("testColDropGuid"));
  def.add(StaticStrings::Version,
          velocypack::Value(static_cast<uint32_t>(
              LogicalCollection::currentVersion())));
  def.add(StaticStrings::DataSourceType,
          velocypack::Value(TRI_COL_TYPE_DOCUMENT));
  def.add(StaticStrings::ObjectId, velocypack::Value("67890"));
  def.add(VPackValue(StaticStrings::Indexes));
  {
    velocypack::ArrayBuilder ab(&def);
    def.openObject();
    def.add(StaticStrings::IndexType, velocypack::Value("primary"));
    def.add(StaticStrings::IndexId, velocypack::Value("1"));
    def.add(VPackValue(StaticStrings::IndexFields));
    { velocypack::ArrayBuilder fb(&def); def.add(VPackValue("_key")); }
    def.close();
    def.openObject();
    def.add(StaticStrings::IndexType, velocypack::Value("geo1"));
    def.add(StaticStrings::IndexId, velocypack::Value("2"));
    def.add(VPackValue(StaticStrings::IndexFields));
    { velocypack::ArrayBuilder fb(&def); def.add(VPackValue("loc")); }
    def.close();
  }
  def.close();

  // Use createCollection so the collection is registered in the vocbase;
  // dropLegacyGeoIndexes iterates vocbase.collections(false).
  std::shared_ptr<LogicalCollection> col = vocbase.createCollection(def.slice());
  ASSERT_NE(col, nullptr);

  auto indexesBefore = col->getPhysical()->getReadyIndexes();
  size_t legacyCountBefore = 0;
  for (auto const& idx : indexesBefore) {
    if (idx->needsLegacyGeoDrop()) ++legacyCountBefore;
  }
  EXPECT_EQ(legacyCountBefore, 1u)
      << "Should have exactly one legacy geo index before drop task";

  Result res = methods::UpgradeTasks::dropLegacyGeoIndexes(
      vocbase, VPackSlice::emptyObjectSlice());
  ASSERT_TRUE(res.ok()) << res.errorMessage();

  auto indexesAfter = col->getPhysical()->getReadyIndexes();
  size_t legacyCountAfter = 0;
  for (auto const& idx : indexesAfter) {
    if (idx->needsLegacyGeoDrop()) ++legacyCountAfter;
  }
  EXPECT_EQ(legacyCountAfter, 0u)
      << "dropLegacyGeoIndexes should drop the legacy geo index";
  EXPECT_EQ(indexesAfter.size(), indexesBefore.size() - 1u)
      << "One index (the legacy geo) should be gone";
}

TEST_F(UpgradeTasksGeoTest,
       index_factory_reroutes_geo1_and_geo2_to_geo_with_legacy_flags) {
  // Build a mocked "old" collection definition containing geo1 and geo2.
  // Loading must succeed, both legacy entries must be represented as geo
  // indexes, and both must carry needsLegacyGeoDrop().
  TRI_vocbase_t& vocbase = createVocbase("testDbLegacyBoth", 4);

  velocypack::Builder def;
  def.openObject();
  def.add("id", velocypack::Value("102"));
  def.add(StaticStrings::DataSourcePlanId, velocypack::Value("102"));
  def.add(StaticStrings::DataSourceName, velocypack::Value("testColLegacyBoth"));
  def.add(StaticStrings::DataSourceGuid,
          velocypack::Value("testColLegacyBothGuid"));
  def.add(StaticStrings::Version,
          velocypack::Value(static_cast<uint32_t>(
              LogicalCollection::currentVersion())));
  def.add(StaticStrings::DataSourceType,
          velocypack::Value(TRI_COL_TYPE_DOCUMENT));
  def.add(StaticStrings::ObjectId, velocypack::Value("12346"));
  def.add(VPackValue(StaticStrings::Indexes));
  {
    velocypack::ArrayBuilder ab(&def);
    def.openObject();
    def.add(StaticStrings::IndexType, velocypack::Value("primary"));
    def.add(StaticStrings::IndexId, velocypack::Value("1"));
    def.add(VPackValue(StaticStrings::IndexFields));
    { velocypack::ArrayBuilder fb(&def); def.add(VPackValue("_key")); }
    def.close();

    def.openObject();
    def.add(StaticStrings::IndexType, velocypack::Value("geo1"));
    def.add(StaticStrings::IndexId, velocypack::Value("2"));
    def.add(VPackValue(StaticStrings::IndexFields));
    { velocypack::ArrayBuilder fb(&def); def.add(VPackValue("loc")); }
    def.close();

    def.openObject();
    def.add(StaticStrings::IndexType, velocypack::Value("geo2"));
    def.add(StaticStrings::IndexId, velocypack::Value("3"));
    def.add(VPackValue(StaticStrings::IndexFields));
    {
      velocypack::ArrayBuilder fb(&def);
      def.add(VPackValue("lat"));
      def.add(VPackValue("lon"));
    }
    def.close();
  }
  def.close();

  std::shared_ptr<LogicalCollection> col =
      vocbase.createCollectionObject(def.slice(), false);
  ASSERT_NE(col, nullptr);

  auto indexes = col->getPhysical()->getReadyIndexes();
  size_t legacyGeoCount = 0;
  size_t legacyGeo1FieldCount = 0;
  size_t legacyGeo2FieldCount = 0;

  for (auto const& idx : indexes) {
    if (!idx->needsLegacyGeoDrop()) {
      continue;
    }
    ++legacyGeoCount;
    EXPECT_EQ(idx->type(), Index::TRI_IDX_TYPE_GEO_INDEX)
        << "Legacy geo1/geo2 definitions must load as geo indexes";
    EXPECT_FALSE(idx->unique()) << "Geo indexes must be non-unique";
    EXPECT_TRUE(idx->sparse()) << "Geo indexes must be sparse";

    if (idx->fields().size() == 1 &&
        idx->fields()[0].size() == 1 &&
        idx->fields()[0][0].name == "loc") {
      ++legacyGeo1FieldCount;
    } else if (idx->fields().size() == 2 &&
               idx->fields()[0].size() == 1 &&
               idx->fields()[1].size() == 1 &&
               idx->fields()[0][0].name == "lat" &&
               idx->fields()[1][0].name == "lon") {
      ++legacyGeo2FieldCount;
    }
  }

  EXPECT_EQ(legacyGeoCount, 2u)
      << "Expected both geo1 and geo2 mocked definitions to be loaded";
  EXPECT_EQ(legacyGeo1FieldCount, 1u)
      << "Expected exactly one legacy single-field geo definition";
  EXPECT_EQ(legacyGeo2FieldCount, 1u)
      << "Expected exactly one legacy two-field geo definition";
}

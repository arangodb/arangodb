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

#include "gtest/gtest.h"

#include "velocypack/Iterator.h"
#include "velocypack/Parser.h"

#include "IResearch/common.h"
#include "Mocks/LogLevels.h"
#include "Mocks/Servers.h"
#include "Mocks/StorageEngineMock.h"

#include "Agency/AgencyComm.h"
#include "Cluster/AgencyCache.h"
#include "Cluster/ClusterFeature.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Logger/Logger.h"
#include "Metrics/ClusterMetricsFeature.h"
#include "Metrics/MetricsFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/arangod.h"
#include "Statistics/StatisticsFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "VocBase/Methods/UpgradeTasks.h"

// =============================================================================
// Single-server fixture (no cluster role)
// =============================================================================

class UpgradeTasksTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION,
                                            arangodb::LogLevel::WARN>,
      public arangodb::tests::LogSuppressor<arangodb::Logger::ENGINES,
                                            arangodb::LogLevel::FATAL> {
 protected:
  arangodb::ArangodServer server;
  StorageEngineMock engine;
  std::vector<
      std::pair<arangodb::application_features::ApplicationFeature&, bool>>
      features;

  UpgradeTasksTest() : server(nullptr, nullptr), engine(server) {
    features.emplace_back(
        server.addFeature<arangodb::metrics::MetricsFeature>(
            arangodb::LazyApplicationFeatureReference<
                arangodb::QueryRegistryFeature>(server),
            arangodb::LazyApplicationFeatureReference<
                arangodb::StatisticsFeature>(nullptr),
            arangodb::LazyApplicationFeatureReference<
                arangodb::EngineSelectorFeature>(server),
            arangodb::LazyApplicationFeatureReference<
                arangodb::metrics::ClusterMetricsFeature>(nullptr),
            arangodb::LazyApplicationFeatureReference<arangodb::ClusterFeature>(
                nullptr)),
        false);
    features.emplace_back(server.addFeature<arangodb::AuthenticationFeature>(),
                          false);
    features.emplace_back(server.addFeature<arangodb::ClusterFeature>(),
                          false);
    features.emplace_back(server.addFeature<arangodb::DatabaseFeature>(),
                          false);
    auto& selector = server.addFeature<arangodb::EngineSelectorFeature>();
    features.emplace_back(selector, false);
    selector.setEngineTesting(&engine);
    features.emplace_back(
        server.addFeature<arangodb::QueryRegistryFeature>(
            server.getFeature<arangodb::metrics::MetricsFeature>()),
        false);

    for (auto& f : features) {
      f.first.prepare();
    }

    for (auto& f : features) {
      if (f.second) {
        f.first.start();
      }
    }
  }

  ~UpgradeTasksTest() {
    server.getFeature<arangodb::EngineSelectorFeature>().setEngineTesting(
        nullptr);

    for (auto& f : features) {
      if (f.second) {
        f.first.stop();
      }
    }

    for (auto& f : features) {
      f.first.unprepare();
    }
  }
};

// =============================================================================
// DB server fixture
// =============================================================================

class UpgradeTasksDBServerTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AGENCY,
                                            arangodb::LogLevel::FATAL>,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION,
                                            arangodb::LogLevel::ERR>,
      public arangodb::tests::LogSuppressor<arangodb::Logger::CLUSTER,
                                            arangodb::LogLevel::WARN>,
      public arangodb::tests::LogSuppressor<arangodb::Logger::ENGINES,
                                            arangodb::LogLevel::FATAL> {
 protected:
  arangodb::tests::mocks::MockDBServer server;

  UpgradeTasksDBServerTest() : server("PRMR_0001") {}
};

// =============================================================================
// Coordinator fixture
// =============================================================================

class UpgradeTasksCoordinatorTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AGENCY,
                                            arangodb::LogLevel::FATAL>,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION,
                                            arangodb::LogLevel::ERR>,
      public arangodb::tests::LogSuppressor<arangodb::Logger::CLUSTER,
                                            arangodb::LogLevel::WARN>,
      public arangodb::tests::LogSuppressor<arangodb::Logger::ENGINES,
                                            arangodb::LogLevel::FATAL> {
 protected:
  arangodb::tests::mocks::MockCoordinator server;

  UpgradeTasksCoordinatorTest() : server("CRDN_0001") {}

  void injectCollectionWithIndexes(std::string const& dbName,
                                   std::string const& collId,
                                   std::string const& indexesJson) {
    // Build a minimal collection plan entry with the given indexes.
    std::string collJson =
        R"({"id":")" + collId +
        R"(","name":"testColl","type":2,"status":3,)"
        R"("indexes":)" + indexesJson + "}";

    // Build an agency transaction that writes the collection.
    arangodb::velocypack::Builder trx;
    trx.openArray();
    trx.openArray();
    trx.openObject();
    trx.add("/arango/Plan/Collections/" + dbName + "/" + collId,
            arangodb::velocypack::Parser::fromJson(collJson)->slice());
    trx.close();
    trx.close();
    trx.close();

    server.getFeature<arangodb::ClusterFeature>()
        .agencyCache()
        .applyTestTransaction(trx.slice());

    // Bump plan version so the cache picks up the change.
    arangodb::velocypack::Builder bumpTrx;
    bumpTrx.openArray();
    bumpTrx.openArray();
    bumpTrx.openObject();
    bumpTrx.add("/arango/Plan/Version",
                arangodb::velocypack::Parser::fromJson(
                    R"({"op":"increment"})")->slice());
    bumpTrx.close();
    bumpTrx.close();
    bumpTrx.close();

    server.getFeature<arangodb::ClusterFeature>()
        .agencyCache()
        .applyTestTransaction(bumpTrx.slice());
  }

  arangodb::velocypack::Slice readPlanIndexes(std::string const& dbName,
                                              std::string const& collId) {
    auto& agencyCache =
        server.getFeature<arangodb::ClusterFeature>().agencyCache();
    auto [acb, idx] = agencyCache.read(
        std::vector<std::string>{arangodb::AgencyCommHelper::path("Plan/Collections")});

    _lastReadResult = std::move(acb);
    return _lastReadResult->slice()[0].get(
        std::initializer_list<std::string_view>{arangodb::AgencyCommHelper::path(),
                                                "Plan", "Collections", dbName,
                                                collId, "indexes"});
  }

 private:
  arangodb::consensus::query_t _lastReadResult;
};

// =============================================================================
// Single-server tests
// =============================================================================

TEST_F(UpgradeTasksTest, convert_hash_skiplist_empty_vocbase) {
  auto& dbFeature = server.getFeature<arangodb::DatabaseFeature>();
  TRI_vocbase_t* vocbase = nullptr;
  ASSERT_TRUE(dbFeature.createDatabase(testDBInfo(server), vocbase).ok());
  ASSERT_NE(vocbase, nullptr);

  auto result =
      arangodb::methods::UpgradeTasks::migrateHashSkiplistToPersistent(
          *vocbase, arangodb::velocypack::Slice::emptyObjectSlice());

  EXPECT_TRUE(result.ok()) << result.errorMessage();
}

TEST_F(UpgradeTasksTest, convert_hash_skiplist_vocbase_with_collection) {
  auto& dbFeature = server.getFeature<arangodb::DatabaseFeature>();
  TRI_vocbase_t* vocbase = nullptr;
  ASSERT_TRUE(dbFeature.createDatabase(testDBInfo(server), vocbase).ok());
  ASSERT_NE(vocbase, nullptr);

  auto collectionJson = arangodb::velocypack::Parser::fromJson(
      R"({ "id": 100, "name": "testCollection" })");
  auto collection = vocbase->createCollection(collectionJson->slice());
  ASSERT_NE(collection, nullptr);

  auto result =
      arangodb::methods::UpgradeTasks::migrateHashSkiplistToPersistent(
          *vocbase, arangodb::velocypack::Slice::emptyObjectSlice());

  EXPECT_TRUE(result.ok()) << result.errorMessage();
}

// =============================================================================
// DB server tests
// =============================================================================

TEST_F(UpgradeTasksDBServerTest, convert_hash_skiplist_dbserver_empty) {
  auto& dbFeature = server.getFeature<arangodb::DatabaseFeature>();
  auto* vocbase = dbFeature.lookupDatabase("_system");
  ASSERT_NE(vocbase, nullptr);

  auto result =
      arangodb::methods::UpgradeTasks::migrateHashSkiplistToPersistent(
          *vocbase, arangodb::velocypack::Slice::emptyObjectSlice());

  EXPECT_TRUE(result.ok()) << result.errorMessage();
}

// =============================================================================
// Coordinator tests
// =============================================================================

TEST_F(UpgradeTasksCoordinatorTest, convert_hash_skiplist_no_deprecated_indexes) {
  auto& dbFeature = server.getFeature<arangodb::DatabaseFeature>();
  auto* vocbase = dbFeature.lookupDatabase("_system");
  ASSERT_NE(vocbase, nullptr);

  auto result =
      arangodb::methods::UpgradeTasks::migrateHashSkiplistToPersistent(
          *vocbase, arangodb::velocypack::Slice::emptyObjectSlice());

  EXPECT_TRUE(result.ok()) << result.errorMessage();
}

TEST_F(UpgradeTasksCoordinatorTest,
       convert_hash_index_to_persistent_in_plan) {
  auto& dbFeature = server.getFeature<arangodb::DatabaseFeature>();
  auto* vocbase = dbFeature.lookupDatabase("_system");
  ASSERT_NE(vocbase, nullptr);

  std::string const collId = "12345";
  injectCollectionWithIndexes(
      "_system", collId,
      R"([
        {"id":"0","type":"primary","name":"primary","fields":["_key"],
         "unique":true,"sparse":false},
        {"id":"10","type":"hash","name":"hashIdx","fields":["a","b"],
         "unique":false,"sparse":false}
      ])");

  auto result =
      arangodb::methods::UpgradeTasks::migrateHashSkiplistToPersistent(
          *vocbase, arangodb::velocypack::Slice::emptyObjectSlice());
  ASSERT_TRUE(result.ok()) << result.errorMessage();

  auto indexesSlice = readPlanIndexes("_system", collId);
  ASSERT_TRUE(indexesSlice.isArray()) << indexesSlice.toJson();

  bool foundConverted = false;
  for (auto idx : arangodb::velocypack::ArrayIterator(indexesSlice)) {
    auto idSlice = idx.get("id");
    if (idSlice.isString() && idSlice.stringView() == "10") {
      auto typeSlice = idx.get("type");
      ASSERT_TRUE(typeSlice.isString());
      EXPECT_EQ(typeSlice.stringView(), "persistent")
          << "hash index should have been converted to persistent";
      auto nameSlice = idx.get("name");
      ASSERT_TRUE(nameSlice.isString());
      EXPECT_EQ(nameSlice.stringView(), "hashIdx_migrated");
      foundConverted = true;
    }
  }
  EXPECT_TRUE(foundConverted) << "converted index not found in plan";
}

TEST_F(UpgradeTasksCoordinatorTest,
       convert_skiplist_index_to_persistent_in_plan) {
  auto& dbFeature = server.getFeature<arangodb::DatabaseFeature>();
  auto* vocbase = dbFeature.lookupDatabase("_system");
  ASSERT_NE(vocbase, nullptr);

  std::string const collId = "12346";
  injectCollectionWithIndexes(
      "_system", collId,
      R"([
        {"id":"0","type":"primary","name":"primary","fields":["_key"],
         "unique":true,"sparse":false},
        {"id":"20","type":"skiplist","name":"skipIdx","fields":["x"],
         "unique":true,"sparse":true}
      ])");

  auto result =
      arangodb::methods::UpgradeTasks::migrateHashSkiplistToPersistent(
          *vocbase, arangodb::velocypack::Slice::emptyObjectSlice());
  ASSERT_TRUE(result.ok()) << result.errorMessage();

  auto indexesSlice = readPlanIndexes("_system", collId);
  ASSERT_TRUE(indexesSlice.isArray());

  bool foundConverted = false;
  for (auto idx : arangodb::velocypack::ArrayIterator(indexesSlice)) {
    auto idSlice = idx.get("id");
    if (idSlice.isString() && idSlice.stringView() == "20") {
      EXPECT_EQ(idx.get("type").stringView(), "persistent");
      EXPECT_EQ(idx.get("name").stringView(), "skipIdx_migrated");
      EXPECT_TRUE(idx.get("unique").isTrue());
      EXPECT_TRUE(idx.get("sparse").isTrue());
      foundConverted = true;
    }
  }
  EXPECT_TRUE(foundConverted) << "converted index not found in plan";
}

TEST_F(UpgradeTasksCoordinatorTest,
       convert_hash_renames_on_name_conflict_with_persistent) {
  auto& dbFeature = server.getFeature<arangodb::DatabaseFeature>();
  auto* vocbase = dbFeature.lookupDatabase("_system");
  ASSERT_NE(vocbase, nullptr);

  std::string const collId = "12347";
  injectCollectionWithIndexes(
      "_system", collId,
      R"([
        {"id":"0","type":"primary","name":"primary","fields":["_key"],
         "unique":true,"sparse":false},
        {"id":"30","type":"persistent","name":"myIdx","fields":["a"],
         "unique":false,"sparse":false},
        {"id":"31","type":"hash","name":"myIdx","fields":["b"],
         "unique":false,"sparse":false}
      ])");

  auto result =
      arangodb::methods::UpgradeTasks::migrateHashSkiplistToPersistent(
          *vocbase, arangodb::velocypack::Slice::emptyObjectSlice());
  ASSERT_TRUE(result.ok()) << result.errorMessage();

  auto indexesSlice = readPlanIndexes("_system", collId);
  ASSERT_TRUE(indexesSlice.isArray());

  bool foundRenamed = false;
  for (auto idx : arangodb::velocypack::ArrayIterator(indexesSlice)) {
    auto idSlice = idx.get("id");
    if (idSlice.isString() && idSlice.stringView() == "31") {
      EXPECT_EQ(idx.get("type").stringView(), "persistent");
      EXPECT_EQ(idx.get("name").stringView(), "myIdx_migrated");
      foundRenamed = true;
    }
  }
  EXPECT_TRUE(foundRenamed) << "renamed index not found in plan";
}

TEST_F(UpgradeTasksCoordinatorTest,
       convert_mixed_hash_skiplist_and_persistent) {
  auto& dbFeature = server.getFeature<arangodb::DatabaseFeature>();
  auto* vocbase = dbFeature.lookupDatabase("_system");
  ASSERT_NE(vocbase, nullptr);

  std::string const collId = "12348";
  injectCollectionWithIndexes(
      "_system", collId,
      R"([
        {"id":"0","type":"primary","name":"primary","fields":["_key"],
         "unique":true,"sparse":false},
        {"id":"40","type":"persistent","name":"persIdx","fields":["a"],
         "unique":false,"sparse":false},
        {"id":"41","type":"hash","name":"hashIdx","fields":["b"],
         "unique":false,"sparse":false},
        {"id":"42","type":"skiplist","name":"skipIdx","fields":["c"],
         "unique":true,"sparse":false}
      ])");

  auto result =
      arangodb::methods::UpgradeTasks::migrateHashSkiplistToPersistent(
          *vocbase, arangodb::velocypack::Slice::emptyObjectSlice());
  ASSERT_TRUE(result.ok()) << result.errorMessage();

  auto indexesSlice = readPlanIndexes("_system", collId);
  ASSERT_TRUE(indexesSlice.isArray());

  for (auto idx : arangodb::velocypack::ArrayIterator(indexesSlice)) {
    auto typeSlice = idx.get("type");
    if (typeSlice.isString()) {
      EXPECT_NE(typeSlice.stringView(), "hash")
          << "no hash indexes should remain";
      EXPECT_NE(typeSlice.stringView(), "skiplist")
          << "no skiplist indexes should remain";
    }
  }

  // Verify specific indexes were converted
  for (auto idx : arangodb::velocypack::ArrayIterator(indexesSlice)) {
    auto id = idx.get("id").stringView();
    if (id == "40") {
      EXPECT_EQ(idx.get("type").stringView(), "persistent");
      EXPECT_EQ(idx.get("name").stringView(), "persIdx");
    } else if (id == "41") {
      EXPECT_EQ(idx.get("type").stringView(), "persistent");
      EXPECT_EQ(idx.get("name").stringView(), "hashIdx_migrated");
    } else if (id == "42") {
      EXPECT_EQ(idx.get("type").stringView(), "persistent");
      EXPECT_EQ(idx.get("name").stringView(), "skipIdx_migrated");
    }
  }
}

TEST_F(UpgradeTasksCoordinatorTest, no_changes_when_only_persistent_indexes) {
  auto& dbFeature = server.getFeature<arangodb::DatabaseFeature>();
  auto* vocbase = dbFeature.lookupDatabase("_system");
  ASSERT_NE(vocbase, nullptr);

  std::string const collId = "12350";
  injectCollectionWithIndexes(
      "_system", collId,
      R"([
        {"id":"0","type":"primary","name":"primary","fields":["_key"],
         "unique":true,"sparse":false},
        {"id":"60","type":"persistent","name":"idx1","fields":["a"],
         "unique":false,"sparse":false},
        {"id":"61","type":"persistent","name":"idx2","fields":["b","c"],
         "unique":true,"sparse":true}
      ])");

  auto indexesBefore = readPlanIndexes("_system", collId);
  std::string beforeJson = indexesBefore.toJson();

  auto result =
      arangodb::methods::UpgradeTasks::migrateHashSkiplistToPersistent(
          *vocbase, arangodb::velocypack::Slice::emptyObjectSlice());
  ASSERT_TRUE(result.ok()) << result.errorMessage();

  auto indexesAfter = readPlanIndexes("_system", collId);
  EXPECT_EQ(indexesAfter.toJson(), beforeJson)
      << "indexes should not change when there are no hash/skiplist indexes";
}

TEST_F(UpgradeTasksCoordinatorTest,
       convert_hash_and_skiplist_on_same_fields) {
  auto& dbFeature = server.getFeature<arangodb::DatabaseFeature>();
  auto* vocbase = dbFeature.lookupDatabase("_system");
  ASSERT_NE(vocbase, nullptr);

  std::string const collId = "12351";
  injectCollectionWithIndexes(
      "_system", collId,
      R"([
        {"id":"0","type":"primary","name":"primary","fields":["_key"],
         "unique":true,"sparse":false},
        {"id":"70","type":"hash","name":"hashOnAB","fields":["a","b"],
         "unique":false,"sparse":false},
        {"id":"71","type":"skiplist","name":"skipOnAB","fields":["a","b"],
         "unique":false,"sparse":false}
      ])");

  auto result =
      arangodb::methods::UpgradeTasks::migrateHashSkiplistToPersistent(
          *vocbase, arangodb::velocypack::Slice::emptyObjectSlice());
  ASSERT_TRUE(result.ok()) << result.errorMessage();

  auto indexesSlice = readPlanIndexes("_system", collId);
  ASSERT_TRUE(indexesSlice.isArray());

  for (auto idx : arangodb::velocypack::ArrayIterator(indexesSlice)) {
    auto id = idx.get("id").stringView();
    if (id == "70") {
      EXPECT_EQ(idx.get("type").stringView(), "persistent");
      EXPECT_EQ(idx.get("name").stringView(), "hashOnAB_migrated");
    } else if (id == "71") {
      EXPECT_EQ(idx.get("type").stringView(), "persistent");
      EXPECT_EQ(idx.get("name").stringView(), "skipOnAB_migrated");
    }
  }
}

TEST_F(UpgradeTasksCoordinatorTest,
       convert_preserves_all_index_properties) {
  auto& dbFeature = server.getFeature<arangodb::DatabaseFeature>();
  auto* vocbase = dbFeature.lookupDatabase("_system");
  ASSERT_NE(vocbase, nullptr);

  std::string const collId = "12352";
  injectCollectionWithIndexes(
      "_system", collId,
      R"([
        {"id":"0","type":"primary","name":"primary","fields":["_key"],
         "unique":true,"sparse":false},
        {"id":"80","type":"hash","name":"richIdx","fields":["x","y","z"],
         "unique":true,"sparse":true,"deduplicate":false,
         "inBackground":true,"cacheEnabled":true}
      ])");

  auto result =
      arangodb::methods::UpgradeTasks::migrateHashSkiplistToPersistent(
          *vocbase, arangodb::velocypack::Slice::emptyObjectSlice());
  ASSERT_TRUE(result.ok()) << result.errorMessage();

  auto indexesSlice = readPlanIndexes("_system", collId);
  ASSERT_TRUE(indexesSlice.isArray());

  for (auto idx : arangodb::velocypack::ArrayIterator(indexesSlice)) {
    if (idx.get("id").stringView() != "80") {
      continue;
    }
    EXPECT_EQ(idx.get("type").stringView(), "persistent");
    EXPECT_EQ(idx.get("name").stringView(), "richIdx_migrated");
    EXPECT_TRUE(idx.get("unique").isTrue());
    EXPECT_TRUE(idx.get("sparse").isTrue());
    EXPECT_TRUE(idx.get("deduplicate").isFalse());
    EXPECT_TRUE(idx.get("inBackground").isTrue());
    EXPECT_TRUE(idx.get("cacheEnabled").isTrue());

    auto fields = idx.get("fields");
    ASSERT_TRUE(fields.isArray());
    ASSERT_EQ(fields.length(), 3);
    EXPECT_EQ(fields.at(0).stringView(), "x");
    EXPECT_EQ(fields.at(1).stringView(), "y");
    EXPECT_EQ(fields.at(2).stringView(), "z");
  }
}

TEST_F(UpgradeTasksCoordinatorTest,
       convert_hash_with_same_name_as_persistent) {
  auto& dbFeature = server.getFeature<arangodb::DatabaseFeature>();
  auto* vocbase = dbFeature.lookupDatabase("_system");
  ASSERT_NE(vocbase, nullptr);

  std::string const collId = "12353";
  injectCollectionWithIndexes(
      "_system", collId,
      R"([
        {"id":"0","type":"primary","name":"primary","fields":["_key"],
         "unique":true,"sparse":false},
        {"id":"90","type":"persistent","name":"foo","fields":["a"],
         "unique":false,"sparse":false},
        {"id":"92","type":"hash","name":"foo","fields":["c"],
         "unique":false,"sparse":false}
      ])");

  auto result =
      arangodb::methods::UpgradeTasks::migrateHashSkiplistToPersistent(
          *vocbase, arangodb::velocypack::Slice::emptyObjectSlice());
  ASSERT_TRUE(result.ok()) << result.errorMessage();

  auto indexesSlice = readPlanIndexes("_system", collId);
  ASSERT_TRUE(indexesSlice.isArray());

  for (auto idx : arangodb::velocypack::ArrayIterator(indexesSlice)) {
    if (idx.get("id").stringView() == "90") {
      EXPECT_EQ(idx.get("name").stringView(), "foo");
    } else if (idx.get("id").stringView() == "92") {
      EXPECT_EQ(idx.get("type").stringView(), "persistent");
      EXPECT_EQ(idx.get("name").stringView(), "foo_migrated");
    }
  }
}

TEST_F(UpgradeTasksCoordinatorTest,
       convert_multiple_deprecated_indexes_same_name) {
  auto& dbFeature = server.getFeature<arangodb::DatabaseFeature>();
  auto* vocbase = dbFeature.lookupDatabase("_system");
  ASSERT_NE(vocbase, nullptr);

  std::string const collId = "12349";
  injectCollectionWithIndexes(
      "_system", collId,
      R"([
        {"id":"0","type":"primary","name":"primary","fields":["_key"],
         "unique":true,"sparse":false},
        {"id":"50","type":"persistent","name":"myIdx","fields":["a"],
         "unique":false,"sparse":false},
        {"id":"51","type":"hash","name":"myIdx","fields":["b"],
         "unique":false,"sparse":false},
        {"id":"52","type":"skiplist","name":"myIdx","fields":["c"],
         "unique":false,"sparse":false}
      ])");

  auto result =
      arangodb::methods::UpgradeTasks::migrateHashSkiplistToPersistent(
          *vocbase, arangodb::velocypack::Slice::emptyObjectSlice());
  ASSERT_TRUE(result.ok()) << result.errorMessage();

  auto indexesSlice = readPlanIndexes("_system", collId);
  ASSERT_TRUE(indexesSlice.isArray());

  for (auto idx : arangodb::velocypack::ArrayIterator(indexesSlice)) {
    auto id = idx.get("id").stringView();
    if (id == "50") {
      EXPECT_EQ(idx.get("type").stringView(), "persistent");
      EXPECT_EQ(idx.get("name").stringView(), "myIdx");
    } else if (id == "51") {
      EXPECT_EQ(idx.get("type").stringView(), "persistent");
      EXPECT_EQ(idx.get("name").stringView(), "myIdx_migrated");
    } else if (id == "52") {
      EXPECT_EQ(idx.get("type").stringView(), "persistent");
      EXPECT_EQ(idx.get("name").stringView(), "myIdx_migrated");
    }
  }
}

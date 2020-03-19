////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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

#include "gtest/gtest.h"

#include "utils/misc.hpp"
#include "utils/string.hpp"
#include "utils/thread_utils.hpp"
#include "utils/utf8_path.hpp"
#include "utils/version_defines.hpp"

#include "IResearch/AgencyMock.h"
#include "IResearch/common.h"
#include "Mocks/LogLevels.h"
#include "Mocks/Servers.h"
#include "Mocks/StorageEngineMock.h"

#include "Agency/Store.h"
#include "ApplicationFeatures/CommunicationFeaturePhase.h"
#include "Aql/AqlFunctionFeature.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "ClusterEngine/ClusterEngine.h"
#include "Cluster/ClusterTypes.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "GeneralServer/ServerSecurityFeature.h"
#include "IResearch/ApplicationServerHelper.h"
#include "IResearch/Containers.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchLinkCoordinator.h"
#include "IResearch/IResearchLinkHelper.h"
#include "IResearch/IResearchMMFilesLink.h"
#include "IResearch/IResearchView.h"
#include "Rest/Version.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/FlushFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/UpgradeFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "Sharding/ShardingFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Indexes.h"
#include "VocBase/Methods/Upgrade.h"

#if USE_ENTERPRISE
#include "Enterprise/Ldap/LdapFeature.h"
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class IResearchFeatureTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AGENCY, arangodb::LogLevel::FATAL>,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION, arangodb::LogLevel::ERR>,
      public arangodb::tests::LogSuppressor<arangodb::Logger::CLUSTER, arangodb::LogLevel::FATAL> {
 protected:

  arangodb::tests::mocks::MockV8Server server;

  IResearchFeatureTest() : server(false) {
    arangodb::tests::init();

    server.addFeature<arangodb::iresearch::IResearchAnalyzerFeature>(false);
    server.addFeature<arangodb::FlushFeature>(false);
    server.addFeature<arangodb::QueryRegistryFeature>(false);
    server.addFeature<arangodb::ServerSecurityFeature>(false);
    server.startFeatures();
  }

  ~IResearchFeatureTest() {}

  // version 0 data-source path
  irs::utf8_path getPersistedPath0(arangodb::LogicalView const& view) {
    auto& dbPathFeature = server.getFeature<arangodb::DatabasePathFeature>();
    irs::utf8_path dataPath(dbPathFeature.directory());
    dataPath /= "databases";
    dataPath /= "database-";
    dataPath += std::to_string(view.vocbase().id());
    dataPath /= arangodb::iresearch::DATA_SOURCE_TYPE.name();
    dataPath += "-";
    dataPath += std::to_string(view.id());
    return dataPath;
  };

  // version 1 data-source path
  irs::utf8_path getPersistedPath1(arangodb::iresearch::IResearchLink const& link) {
    auto& dbPathFeature = server.getFeature<arangodb::DatabasePathFeature>();
    irs::utf8_path dataPath(dbPathFeature.directory());
    dataPath /= "databases";
    dataPath /= "database-";
    dataPath += std::to_string(link.collection().vocbase().id());
    dataPath /= arangodb::iresearch::DATA_SOURCE_TYPE.name();
    dataPath += "-";
    dataPath += std::to_string(link.collection().id());
    dataPath += "_";
    dataPath += std::to_string(link.id());
    return dataPath;
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(IResearchFeatureTest, test_start) {
  auto& functions = server.addFeatureUntracked<arangodb::aql::AqlFunctionFeature>();
  auto& iresearch = server.addFeatureUntracked<arangodb::iresearch::IResearchFeature>();
  auto cleanup =
      irs::make_finally([&functions]() -> void { functions.unprepare(); });

  enum class FunctionType { FILTER = 0, SCORER };

  std::map<irs::string_ref, std::pair<irs::string_ref, FunctionType>> expected = {
    // filter functions
    { "EXISTS", { ".|.,.", FunctionType::FILTER } },
    { "PHRASE", { ".,.|.+", FunctionType::FILTER } },
    { "STARTS_WITH", { ".,.|.", FunctionType::FILTER } },
    { "MIN_MATCH", { ".,.|.+", FunctionType::FILTER } },

    // context functions
    { "ANALYZER", { ".,.", FunctionType::FILTER } },
    { "BOOST", { ".,.", FunctionType::FILTER } },

    // scorer functions
    { "BM25", { ".|+", FunctionType::SCORER } },
    { "TFIDF", { ".|+", FunctionType::SCORER } },
  };

  functions.prepare();

  for (auto& entry : expected) {
    auto* function = arangodb::iresearch::getFunction(functions, entry.first);
    EXPECT_EQ(nullptr, function);
  };

  iresearch.start();

  for (auto& entry : expected) {
    auto* function = arangodb::iresearch::getFunction(functions, entry.first);
    EXPECT_NE(nullptr, function);
    EXPECT_EQ(entry.second.first, function->arguments);
    EXPECT_TRUE(((entry.second.second == FunctionType::FILTER &&
                  arangodb::iresearch::isFilter(*function)) ||
                 (entry.second.second == FunctionType::SCORER &&
                  arangodb::iresearch::isScorer(*function))));
  };

  iresearch.stop();
  functions.unprepare();
}

TEST_F(IResearchFeatureTest, test_upgrade0_1_no_directory) {
  // test single-server (no directory)
  auto collectionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testCollection\" }");
  auto linkJson = arangodb::velocypack::Parser::fromJson(
      "{ \"view\": \"testView\", \"type\": \"arangosearch\", "
      "\"includeAllFields\": true }");
  auto viewJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", \"type\": \"arangosearch\", \"version\": 0 "
      "}");
  auto versionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"version\": 0, \"tasks\": {} }");

  // add the UpgradeFeature, but make sure it is not prepared
  server.addFeatureUntracked<arangodb::UpgradeFeature>(nullptr, std::vector<std::type_index>{});

  auto& feature = server.addFeatureUntracked<arangodb::iresearch::IResearchFeature>();
  feature.prepare();  // register iresearch view type
  feature.start();    // register upgrade tasks

  server.getFeature<arangodb::DatabaseFeature>().enableUpgrade();  // skip IResearchView validation

  auto& dbPathFeature = server.getFeature<arangodb::DatabasePathFeature>();
  arangodb::tests::setDatabasePath(dbPathFeature);  // ensure test data is stored in a unique directory
  auto versionFilename = StorageEngineMock::versionFilenameResult;
  auto versionFilenameRestore = irs::make_finally([&versionFilename]() -> void {
    StorageEngineMock::versionFilenameResult = versionFilename;
  });
  StorageEngineMock::versionFilenameResult =
      (irs::utf8_path(dbPathFeature.directory()) /= "version").utf8();
  ASSERT_TRUE(irs::utf8_path(dbPathFeature.directory()).mkdir());
  ASSERT_TRUE((arangodb::basics::VelocyPackHelper::velocyPackToFile(
      StorageEngineMock::versionFilenameResult, versionJson->slice(), false)));

  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
  auto logicalCollection = vocbase.createCollection(collectionJson->slice());
  ASSERT_NE(logicalCollection, nullptr);
  auto logicalView0 = vocbase.createView(viewJson->slice());
  ASSERT_NE(logicalView0, nullptr);
  bool created = false;
  auto index = logicalCollection->createIndex(linkJson->slice(), created);
  ASSERT_TRUE(created);
  ASSERT_NE(index, nullptr);
  auto link0 = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
  ASSERT_NE(link0, nullptr);

  index->unload();  // release file handles
  bool result;
  auto linkDataPath = getPersistedPath1(*link0);
  EXPECT_TRUE(linkDataPath.remove());  // remove link directory
  auto viewDataPath = getPersistedPath0(*logicalView0);
  EXPECT_TRUE(viewDataPath.exists(result) && !result);  // ensure no view directory
  arangodb::velocypack::Builder builder;
  builder.openObject();
  EXPECT_TRUE(logicalView0->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence).ok());
  builder.close();
  EXPECT_EQ(0, builder.slice().get("version").getNumber<uint32_t>());  // ensure 'version == 0 before upgrade

  EXPECT_TRUE(arangodb::methods::Upgrade::startup(vocbase, true, false).ok());  // run upgrade
  auto logicalView1 = vocbase.lookupView(logicalView0->name());
  EXPECT_FALSE(!logicalView1);  // ensure view present after upgrade
  EXPECT_EQ(logicalView0->id(), logicalView1->id());  // ensure same id for view
  auto link1 = arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection, *logicalView1);
  EXPECT_FALSE(!link1);  // ensure link present after upgrade
  EXPECT_NE(link0->id(), link1->id());  // ensure new link
  linkDataPath = getPersistedPath1(*link1);
  EXPECT_TRUE(linkDataPath.exists(result) && result);  // ensure link directory created after upgrade
  EXPECT_TRUE(viewDataPath.exists(result) && !result);  // ensure view directory not present
  viewDataPath = getPersistedPath0(*logicalView1);
  EXPECT_TRUE(viewDataPath.exists(result) && !result);  // ensure view directory not created
  builder.clear();
  builder.openObject();
  EXPECT_TRUE(logicalView1->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence).ok());
  builder.close();
  EXPECT_EQ(1, builder.slice().get("version").getNumber<uint32_t>());  // ensure 'version == 1 after upgrade
}

TEST_F(IResearchFeatureTest, test_upgrade0_1_with_directory) {
  // test single-server (with directory)
  auto collectionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testCollection\" }");
  auto linkJson = arangodb::velocypack::Parser::fromJson(
      "{ \"view\": \"testView\", \"type\": \"arangosearch\", "
      "\"includeAllFields\": true }");
  auto viewJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", \"type\": \"arangosearch\", \"version\": 0 "
      "}");
  auto versionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"version\": 0, \"tasks\": {} }");

  // add the UpgradeFeature, but make sure it is not prepared
  server.addFeatureUntracked<arangodb::UpgradeFeature>(nullptr, std::vector<std::type_index>{});

  auto& feature = server.addFeatureUntracked<arangodb::iresearch::IResearchFeature>();
  feature.prepare();  // register iresearch view type
  feature.start();    // register upgrade tasks

  server.getFeature<arangodb::DatabaseFeature>().enableUpgrade();  // skip IResearchView validation

  auto& dbPathFeature = server.getFeature<arangodb::DatabasePathFeature>();
  arangodb::tests::setDatabasePath(dbPathFeature);  // ensure test data is stored in a unique directory
  auto versionFilename = StorageEngineMock::versionFilenameResult;
  auto versionFilenameRestore = irs::make_finally([&versionFilename]() -> void {
    StorageEngineMock::versionFilenameResult = versionFilename;
  });
  StorageEngineMock::versionFilenameResult =
      (irs::utf8_path(dbPathFeature.directory()) /= "version").utf8();
  ASSERT_TRUE(irs::utf8_path(dbPathFeature.directory()).mkdir());
  ASSERT_TRUE((arangodb::basics::VelocyPackHelper::velocyPackToFile(
      StorageEngineMock::versionFilenameResult, versionJson->slice(), false)));

  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
  auto logicalCollection = vocbase.createCollection(collectionJson->slice());
  ASSERT_FALSE(!logicalCollection);
  auto logicalView0 = vocbase.createView(viewJson->slice());
  ASSERT_FALSE(!logicalView0);
  bool created;
  auto index = logicalCollection->createIndex(linkJson->slice(), created);
  ASSERT_TRUE(created);
  ASSERT_FALSE(!index);
  auto link0 = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
  ASSERT_FALSE(!link0);

  index->unload();  // release file handles
  bool result;
  auto linkDataPath = getPersistedPath1(*link0);
  EXPECT_TRUE(linkDataPath.remove());  // remove link directory
  auto viewDataPath = getPersistedPath0(*logicalView0);
  EXPECT_TRUE(viewDataPath.exists(result) && !result);
  EXPECT_TRUE(viewDataPath.mkdir());  // create view directory
  EXPECT_TRUE(viewDataPath.exists(result) && result);
  arangodb::velocypack::Builder builder;
  builder.openObject();
  EXPECT_TRUE(logicalView0->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence).ok());
  builder.close();
  EXPECT_EQ(0, builder.slice().get("version").getNumber<uint32_t>());  // ensure 'version == 0 before upgrade

  EXPECT_TRUE(arangodb::methods::Upgrade::startup(vocbase, true, false).ok());  // run upgrade
  auto logicalView1 = vocbase.lookupView(logicalView0->name());
  EXPECT_FALSE(!logicalView1);  // ensure view present after upgrade
  EXPECT_EQ(logicalView0->id(), logicalView1->id());  // ensure same id for view
  auto link1 = arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection, *logicalView1);
  EXPECT_FALSE(!link1);  // ensure link present after upgrade
  EXPECT_NE(link0->id(), link1->id());  // ensure new link
  linkDataPath = getPersistedPath1(*link1);
  EXPECT_TRUE(linkDataPath.exists(result) && result);  // ensure link directory created after upgrade
  EXPECT_TRUE(viewDataPath.exists(result) && !result);  // ensure view directory removed after upgrade
  viewDataPath = getPersistedPath0(*logicalView1);
  EXPECT_TRUE(viewDataPath.exists(result) && !result);  // ensure view directory not created
  builder.clear();
  builder.openObject();
  EXPECT_TRUE(logicalView1->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence).ok());
  builder.close();
  EXPECT_EQ(1, builder.slice().get("version").getNumber<uint32_t>());  // ensure 'version == 1 after upgrade
}

TEST_F(IResearchFeatureTest, IResearch_version_test) {
  EXPECT_EQ(IResearch_version, arangodb::rest::Version::getIResearchVersion());
  EXPECT_TRUE(IResearch_version ==
              arangodb::rest::Version::Values["iresearch-version"]);
}

// Temporarily surpress for MSVC
#ifndef _MSC_VER
TEST_F(IResearchFeatureTest, test_async_schedule_test_null_resource_mutex) {
  bool deallocated = false;  // declare above 'feature' to ensure proper destruction order
  arangodb::iresearch::IResearchFeature feature(server.server());
  feature.prepare();  // start thread pool
  std::condition_variable cond;
  std::mutex mutex;
  SCOPED_LOCK_NAMED(mutex, lock);

  {
    std::shared_ptr<bool> flag(&deallocated,
                               [](bool* ptr) -> void { *ptr = true; });
    feature.async(nullptr, [&cond, &mutex, flag](size_t&, bool) -> bool {
      SCOPED_LOCK(mutex);
      cond.notify_all();
      return false;
    });
  }
  EXPECT_TRUE((std::cv_status::timeout !=
               cond.wait_for(lock, std::chrono::milliseconds(100))));
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_TRUE(deallocated);
}

TEST_F(IResearchFeatureTest, test_async_schedule_task_null_resource_mutex_value) {
  bool deallocated = false;  // declare above 'feature' to ensure proper destruction order
  arangodb::iresearch::IResearchFeature feature(server.server());
  feature.prepare();  // start thread pool
  auto resourceMutex = std::make_shared<arangodb::iresearch::ResourceMutex>(nullptr);
  std::condition_variable cond;
  std::mutex mutex;
  SCOPED_LOCK_NAMED(mutex, lock);

  {
    std::shared_ptr<bool> flag(&deallocated,
                               [](bool* ptr) -> void { *ptr = true; });
    feature.async(resourceMutex, [&cond, &mutex, flag](size_t&, bool) -> bool {
      SCOPED_LOCK(mutex);
      cond.notify_all();
      return false;
    });
  }
  EXPECT_TRUE((std::cv_status::timeout ==
               cond.wait_for(lock, std::chrono::milliseconds(100))));
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_TRUE(deallocated);
}

TEST_F(IResearchFeatureTest, test_async_schedule_task_null_functr) {
  auto& feature = server.addFeatureUntracked<arangodb::iresearch::IResearchFeature>();
  feature.prepare();  // start thread pool
  auto resourceMutex =
      std::make_shared<arangodb::iresearch::ResourceMutex>(&server.server());
  std::condition_variable cond;
  std::mutex mutex;
  SCOPED_LOCK_NAMED(mutex, lock);

  feature.async(resourceMutex, {});
  EXPECT_TRUE((std::cv_status::timeout ==
               cond.wait_for(lock, std::chrono::milliseconds(100))));
  resourceMutex->reset();  // should not deadlock if task released
}

TEST_F(IResearchFeatureTest, test_async_schedule_task_wait_indefinite) {
  bool deallocated = false;  // declare above 'feature' to ensure proper destruction order
  arangodb::iresearch::IResearchFeature feature(server.server());
  feature.prepare();  // start thread pool
  std::condition_variable cond;
  std::mutex mutex;
  size_t count = 0;
  SCOPED_LOCK_NAMED(mutex, lock);

  {
    std::shared_ptr<bool> flag(&deallocated,
                               [](bool* ptr) -> void { *ptr = true; });
    feature.async(nullptr, [&cond, &mutex, flag, &count](size_t&, bool) -> bool {
      ++count;
      SCOPED_LOCK(mutex);
      cond.notify_all();
      return true;
    });
  }
  EXPECT_TRUE((std::cv_status::timeout !=
               cond.wait_for(lock, std::chrono::milliseconds(100))));  // first run invoked immediately
  EXPECT_FALSE(deallocated);
  EXPECT_TRUE((std::cv_status::timeout ==
               cond.wait_for(lock, std::chrono::milliseconds(100))));
  EXPECT_FALSE(deallocated);  // still scheduled
  EXPECT_EQ(1, count);
}

TEST_F(IResearchFeatureTest, test_async_single_run_task) {
  bool deallocated = false;  // declare above 'feature' to ensure proper destruction order
  arangodb::iresearch::IResearchFeature feature(server.server());
  feature.prepare();  // start thread pool
  auto resourceMutex =
      std::make_shared<arangodb::iresearch::ResourceMutex>(&server.server());
  std::condition_variable cond;
  std::mutex mutex;
  SCOPED_LOCK_NAMED(mutex, lock);

  {
    std::shared_ptr<bool> flag(&deallocated,
                               [](bool* ptr) -> void { *ptr = true; });
    feature.async(resourceMutex, [&cond, &mutex, flag](size_t&, bool) -> bool {
      SCOPED_LOCK(mutex);
      cond.notify_all();
      return false;
    });
  }
  EXPECT_TRUE((std::cv_status::timeout !=
               cond.wait_for(lock, std::chrono::milliseconds(100))));
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_TRUE(deallocated);
}

TEST_F(IResearchFeatureTest, test_async_multi_run_task) {
  bool deallocated = false;  // declare above 'feature' to ensure proper destruction order
  arangodb::iresearch::IResearchFeature feature(server.server());
  feature.prepare();  // start thread pool
  auto resourceMutex =
      std::make_shared<arangodb::iresearch::ResourceMutex>(&server.server());
  std::condition_variable cond;
  std::mutex mutex;
  size_t count = 0;
  auto last = std::chrono::system_clock::now();
  std::chrono::system_clock::duration diff;
  SCOPED_LOCK_NAMED(mutex, lock);

  {
    std::shared_ptr<bool> flag(&deallocated,
                               [](bool* ptr) -> void { *ptr = true; });
    feature.async(resourceMutex,
                  [&cond, &mutex, flag, &count, &last, &diff](size_t& timeoutMsec, bool) -> bool {
                    diff = std::chrono::system_clock::now() - last;
                    last = std::chrono::system_clock::now();
                    timeoutMsec = 100;
                    if (++count <= 1) return true;
                    SCOPED_LOCK(mutex);
                    cond.notify_all();
                    return false;
                  });
  }
  EXPECT_TRUE((std::cv_status::timeout !=
               cond.wait_for(lock, std::chrono::milliseconds(1000))));
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_TRUE(deallocated);
  EXPECT_EQ(2, count);
  EXPECT_TRUE(std::chrono::milliseconds(100) < diff);
}

TEST_F(IResearchFeatureTest, test_async_trigger_task_by_notify) {
  bool deallocated = false;  // declare above 'feature' to ensure proper destruction order
  arangodb::iresearch::IResearchFeature feature(server.server());
  feature.prepare();  // start thread pool
  auto resourceMutex =
      std::make_shared<arangodb::iresearch::ResourceMutex>(&server.server());
  bool execVal = true;
  std::condition_variable cond;
  std::mutex mutex;
  size_t count = 0;
  auto last = std::chrono::system_clock::now();
  SCOPED_LOCK_NAMED(mutex, lock);

  {
    std::shared_ptr<bool> flag(&deallocated,
                               [](bool* ptr) -> void { *ptr = true; });
    feature.async(resourceMutex,
                  [&cond, &mutex, flag, &execVal, &count](size_t&, bool exec) -> bool {
                    execVal = exec;
                    SCOPED_LOCK(mutex);
                    cond.notify_all();
                    return ++count < 2;
                  });
  }
  EXPECT_TRUE((std::cv_status::timeout !=
               cond.wait_for(lock, std::chrono::milliseconds(100))));  // first run invoked immediately
  EXPECT_FALSE(deallocated);
  EXPECT_TRUE((std::cv_status::timeout ==
               cond.wait_for(lock, std::chrono::milliseconds(100))));
  EXPECT_FALSE(deallocated);
  feature.asyncNotify();
  EXPECT_TRUE((std::cv_status::timeout !=
               cond.wait_for(lock, std::chrono::milliseconds(100))));
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_TRUE(deallocated);
  EXPECT_FALSE(execVal);
  auto diff = std::chrono::system_clock::now() - last;
  EXPECT_TRUE(std::chrono::milliseconds(1000) > diff);
}

TEST_F(IResearchFeatureTest, test_async_trigger_by_timeout) {
  bool deallocated = false;  // declare above 'feature' to ensure proper destruction order
  arangodb::iresearch::IResearchFeature feature(server.server());
  feature.prepare();  // start thread pool
  auto resourceMutex =
      std::make_shared<arangodb::iresearch::ResourceMutex>(&server.server());
  bool execVal = false;
  std::condition_variable cond;
  std::mutex mutex;
  size_t count = 0;
  auto last = std::chrono::system_clock::now();
  SCOPED_LOCK_NAMED(mutex, lock);

  {
    std::shared_ptr<bool> flag(&deallocated,
                               [](bool* ptr) -> void { *ptr = true; });
    feature.async(resourceMutex,
                  [&cond, &mutex, flag, &execVal, &count](size_t& timeoutMsec, bool exec) -> bool {
                    execVal = exec;
                    SCOPED_LOCK(mutex);
                    cond.notify_all();
                    timeoutMsec = 100;
                    return ++count < 2;
                  });
  }
  EXPECT_TRUE((std::cv_status::timeout !=
               cond.wait_for(lock, std::chrono::milliseconds(100))));  // first run invoked immediately
  EXPECT_FALSE(deallocated);
  EXPECT_TRUE((std::cv_status::timeout !=
               cond.wait_for(lock, std::chrono::milliseconds(1000))));
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_TRUE(deallocated);
  EXPECT_TRUE(execVal);
  auto diff = std::chrono::system_clock::now() - last;
  EXPECT_TRUE(std::chrono::milliseconds(300) >= diff);  // could be a little more then 100ms+100ms
}

TEST_F(IResearchFeatureTest, test_async_deallocate_empty) {
  auto& feature = server.addFeatureUntracked<arangodb::iresearch::IResearchFeature>();
  feature.prepare();  // start thread pool
}

TEST_F(IResearchFeatureTest, test_async_deallocate_with_running_tasks) {
  auto resourceMutex =
      std::make_shared<arangodb::iresearch::ResourceMutex>(&server.server());
  bool deallocated = false;
  std::condition_variable cond;
  std::mutex mutex;
  SCOPED_LOCK_NAMED(mutex, lock);

  {
    arangodb::iresearch::IResearchFeature feature(server.server());
    feature.prepare(); // start thread pool
    std::shared_ptr<bool> flag(&deallocated,
                               [](bool* ptr) -> void { *ptr = true; });

    feature.async(resourceMutex, [&cond, &mutex, flag](size_t& timeoutMsec, bool) -> bool {
      SCOPED_LOCK(mutex);
      cond.notify_all();
      timeoutMsec = 100;
      return true;
    });
    EXPECT_TRUE((std::cv_status::timeout !=
                 cond.wait_for(lock, std::chrono::milliseconds(100))));
  }

  EXPECT_TRUE(deallocated);
}

TEST_F(IResearchFeatureTest, test_async_multiple_tasks_with_same_resource_mutex) {
  bool deallocated0 = false;  // declare above 'feature' to ensure proper destruction order
  bool deallocated1 = false;  // declare above 'feature' to ensure proper destruction order
  arangodb::iresearch::IResearchFeature feature(server.server());
  feature.prepare();  // start thread pool
  auto resourceMutex =
      std::make_shared<arangodb::iresearch::ResourceMutex>(&server.server());
  std::condition_variable cond;
  std::mutex mutex;
  size_t count = 0;
  SCOPED_LOCK_NAMED(mutex, lock);

  {
    std::shared_ptr<bool> flag(&deallocated0,
                               [](bool* ptr) -> void { *ptr = true; });
    feature.async(resourceMutex,
                  [&cond, &mutex, flag, &count](size_t& timeoutMsec, bool) -> bool {
                    if (++count > 1) return false;
                    timeoutMsec = 100;
                    SCOPED_LOCK_NAMED(mutex, lock);
                    cond.notify_all();
                    cond.wait(lock);
                    return true;
                  });
  }
  EXPECT_TRUE((std::cv_status::timeout !=
               cond.wait_for(lock, std::chrono::milliseconds(1000))));  // wait for the first task to start

  std::thread thread([resourceMutex]() -> void { resourceMutex->reset(); });  // try to acquire a write lock
  std::this_thread::sleep_for(std::chrono::milliseconds(100));  // hopefully a write-lock aquisition attempt is in progress

  {
    TRY_SCOPED_LOCK_NAMED(resourceMutex->mutex(), resourceLock);
    EXPECT_FALSE(resourceLock.owns_lock());  // write-lock acquired successfully (read-locks blocked)
  }

  {
    std::shared_ptr<bool> flag(&deallocated1,
                               [](bool* ptr) -> void { *ptr = true; });
    feature.async(resourceMutex, [flag](size_t&, bool) -> bool { return false; });  // will never get invoked because resourceMutex is reset
  }
  cond.notify_all();  // wake up first task after resourceMutex write-lock acquired (will process pending tasks)
  lock.unlock();      // allow first task to run
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_TRUE(deallocated0);

  // expectation is currently deactivated as it is causing sporadic test failures:
  // EXPECT_TRUE(deallocated1);
  //
  // the reason is that the read_write_mutex::unlock() function in 3rdParty/iresearch/core/utils/async_utils.cpp 
  // does not acquire a mutex reproducibly.
  // excerpt from that code:
  //
  //  220   // FIXME: this should be changed to SCOPED_LOCK_NAMED, as right now it is not
  //  221   // guaranteed that we can succesfully acquire the mutex here. and if we don't,
  //  222   // there is no guarantee that the notify_all will wake up queued waiter.
  //  223
  //  224   TRY_SCOPED_LOCK_NAMED(mutex_, lock); // try to acquire mutex for use with cond
  //  225
  //  226   // wake only writers since this is a reader
  //  227   // wake even without lock since writer may be waiting in lock_write() on cond
  //  228   // the latter might also indicate a bug if deadlock occurs with SCOPED_LOCK()
  //  229   writer_cond_.notify_all();
  //
  //  related bug issue: https://github.com/arangodb/backlog/issues/618

  thread.join();
}

TEST_F(IResearchFeatureTest, test_async_schedule_task_resize_pool) {
  bool deallocated = false;  // declare above 'feature' to ensure proper destruction order
  arangodb::iresearch::IResearchFeature feature(server.server());
  arangodb::options::ProgramOptions options("", "", "", nullptr);
  auto optionsPtr = std::shared_ptr<arangodb::options::ProgramOptions>(
      &options, [](arangodb::options::ProgramOptions*) -> void {});
  feature.collectOptions(optionsPtr);
  options.get<arangodb::options::UInt64Parameter>("arangosearch.threads")
      ->set("8");
  auto resourceMutex =
      std::make_shared<arangodb::iresearch::ResourceMutex>(&server.server());
  std::condition_variable cond;
  std::mutex mutex;
  size_t count = 0;
  auto last = std::chrono::system_clock::now();
  std::chrono::system_clock::duration diff;
  SCOPED_LOCK_NAMED(mutex, lock);

  {
    std::shared_ptr<bool> flag(&deallocated,
                               [](bool* ptr) -> void { *ptr = true; });
    feature.async(resourceMutex,
                  [&cond, &mutex, flag, &count, &last, &diff](size_t& timeoutMsec, bool) -> bool {
                    diff = std::chrono::system_clock::now() - last;
                    last = std::chrono::system_clock::now();
                    timeoutMsec = 100;
                    if (++count <= 1) return true;
                    SCOPED_LOCK(mutex);
                    cond.notify_all();
                    return false;
                  });
  }
  feature.prepare();  // start thread pool after a task has been scheduled, to trigger resize with a task
  EXPECT_TRUE((std::cv_status::timeout !=
               cond.wait_for(lock, std::chrono::milliseconds(1000))));
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_TRUE(deallocated);
  EXPECT_EQ(2, count);
  EXPECT_TRUE(std::chrono::milliseconds(100) < diff);
}
#endif

class IResearchFeatureTestCoordinator
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AGENCY, arangodb::LogLevel::FATAL>,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION, arangodb::LogLevel::ERR>,
      public arangodb::tests::LogSuppressor<arangodb::Logger::CLUSTER, arangodb::LogLevel::FATAL> {
 protected:
  arangodb::tests::mocks::MockV8Server server;

 private:
  arangodb::consensus::Store _agencyStore;
  arangodb::ServerState::RoleEnum _serverRoleBefore;

 protected:
  IResearchFeatureTestCoordinator()
      : server(false),
        _agencyStore(server.server(), nullptr, "arango"),
        _serverRoleBefore(arangodb::ServerState::instance()->getRole()) {
    auto* agencyCommManager =
        new AgencyCommManagerMock(server.server(), "arango");
    std::ignore = agencyCommManager->addConnection<GeneralClientConnectionAgencyMock>(
        server.server(), _agencyStore);
    std::ignore = agencyCommManager->addConnection<GeneralClientConnectionAgencyMock>(
        server.server(),
        _agencyStore);  // need 2 connections or Agency callbacks will fail
    arangodb::AgencyCommManager::MANAGER.reset(agencyCommManager);

    arangodb::tests::init();

    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_COORDINATOR);
    arangodb::ServerState::instance()->setRebootId(arangodb::RebootId{1});  // Hack.

    server.addFeature<arangodb::iresearch::IResearchAnalyzerFeature>(false);
    server.addFeature<arangodb::FlushFeature>(false);
    server.addFeature<arangodb::QueryRegistryFeature>(false);
    server.addFeature<arangodb::ServerSecurityFeature>(false);
    server.startFeatures();

    arangodb::AgencyCommManager::MANAGER->start();  // initialize agency
  }

  ~IResearchFeatureTestCoordinator() {
    arangodb::ServerState::instance()->setRole(_serverRoleBefore);
  }
};

TEST_F(IResearchFeatureTestCoordinator, test_upgrade0_1) {
  // test coordinator
  auto collectionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"id\": \"1\", \"name\": \"testCollection\", \"shards\":{} }");
  auto linkJson = arangodb::velocypack::Parser::fromJson(
      "{ \"view\": \"testView\", \"type\": \"arangosearch\", "
      "\"includeAllFields\": true }");
  auto viewJson = arangodb::velocypack::Parser::fromJson(
      "{ \"id\": 42, \"name\": \"testView\", \"type\": \"arangosearch\", "
      "\"version\": 0 }");
  auto versionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"version\": 0, \"tasks\": {} }");
  auto collectionId = std::to_string(1);
  auto viewId = std::to_string(42);

  // add the UpgradeFeature, but make sure it is not prepared
  server.addFeatureUntracked<arangodb::UpgradeFeature>(nullptr, std::vector<std::type_index>{});

  auto& feature = server.addFeatureUntracked<arangodb::iresearch::IResearchFeature>();
  feature.prepare();  // register iresearch view type
  feature.start();    // register upgrade tasks

  server.getFeature<arangodb::DatabaseFeature>().enableUpgrade();  // skip IResearchView validation

  auto& engine = server.getFeature<arangodb::EngineSelectorFeature>().engine();
  auto& factory =
      server.getFeature<arangodb::iresearch::IResearchFeature>().factory<arangodb::ClusterEngine>();
  const_cast<arangodb::IndexFactory&>(engine.indexFactory())
      .emplace(  // required for Indexes::ensureIndex(...)
          arangodb::iresearch::DATA_SOURCE_TYPE.name(), factory);
  auto& ci = server.getFeature<arangodb::ClusterFeature>().clusterInfo();
  TRI_vocbase_t* vocbase;  // will be owned by DatabaseFeature

  auto& database = server.getFeature<arangodb::DatabaseFeature>();
  ASSERT_TRUE(database.createDatabase(testDBInfo(server.server()), vocbase).ok());

  // simulate heartbeat thread (create database in current)
  // this is stupid.
  {
    auto const path = "/Current/Databases/" + vocbase->name();
    auto const value = arangodb::velocypack::Parser::fromJson(
        // TODO: This one asserts with "not an object". No idea why.
        // "{ \"id\": \"1\" }" );
        "{ \"id\": { \"id\": \"1\" } }");
    EXPECT_TRUE(arangodb::AgencyComm(server.server())
                    .setValue(path, value->slice(), 0.0)
                    .successful());
  }

  ASSERT_TRUE(
      (arangodb::methods::Databases::create(server.server(), vocbase->name(),
                                            arangodb::velocypack::Slice::emptyArraySlice(),
                                            arangodb::velocypack::Slice::emptyObjectSlice())
           .ok()));

  ASSERT_TRUE((ci.createCollectionCoordinator(vocbase->name(), collectionId, 0, 1, 1, false,
                                              collectionJson->slice(), 0.0, false, nullptr)
                   .ok()));

  auto logicalCollection = ci.getCollection(vocbase->name(), collectionId);
  ASSERT_FALSE(!logicalCollection);
  EXPECT_TRUE(
      (ci.createViewCoordinator(vocbase->name(), viewId, viewJson->slice()).ok()));
  auto logicalView0 = ci.getView(vocbase->name(), viewId);
  ASSERT_FALSE(!logicalView0);

  // simulate heartbeat thread (create index in current)
  {
    auto const path = "/Current/Collections/" + vocbase->name() + "/" +
                      std::to_string(logicalCollection->id());
    auto const value = arangodb::velocypack::Parser::fromJson(
        "{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"2\" } "
        "] } }");
    EXPECT_TRUE(arangodb::AgencyComm(server.server())
                    .setValue(path, value->slice(), 0.0)
                    .successful());
  }

  arangodb::velocypack::Builder tmp;
  ASSERT_TRUE((arangodb::methods::Indexes::ensureIndex(logicalCollection.get(),
                                                       linkJson->slice(), true, tmp)
                   .ok()));
  logicalCollection = ci.getCollection(vocbase->name(), collectionId);
  ASSERT_FALSE(!logicalCollection);
  auto link0 = arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection, *logicalView0);
  ASSERT_FALSE(!link0);

  arangodb::velocypack::Builder builder;
  builder.openObject();
  EXPECT_TRUE(logicalView0->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence).ok());
  builder.close();
  EXPECT_EQ(0, builder.slice().get("version").getNumber<uint32_t>());  // ensure 'version == 0 before upgrade

  // ensure no upgrade on coordinator
  // simulate heartbeat thread (create index in current)
  {
    auto const path = "/Current/Collections/" + vocbase->name() + "/" +
                      std::to_string(logicalCollection->id());
    auto const value = arangodb::velocypack::Parser::fromJson(
        "{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"2\" } "
        "] } }");
    EXPECT_TRUE(arangodb::AgencyComm(server.server())
                    .setValue(path, value->slice(), 0.0)
                    .successful());
  }
  EXPECT_TRUE(arangodb::methods::Upgrade::clusterBootstrap(*vocbase).ok());  // run upgrade
  auto logicalCollection2 = ci.getCollection(vocbase->name(), collectionId);
  ASSERT_FALSE(!logicalCollection2);
  auto logicalView1 = ci.getView(vocbase->name(), viewId);
  EXPECT_FALSE(!logicalView1);  // ensure view present after upgrade
  EXPECT_EQ(logicalView0->id(), logicalView1->id());  // ensure same id for view
  auto link1 = arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection2, *logicalView1);
  EXPECT_FALSE(!link1);  // ensure link present after upgrade
  EXPECT_EQ(link0->id(), link1->id());  // ensure new link
  builder.clear();
  builder.openObject();
  EXPECT_TRUE(logicalView1->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence).ok());
  builder.close();
  EXPECT_EQ(0, builder.slice().get("version").getNumber<uint32_t>());  // ensure 'version == 0 after upgrade
}

class IResearchFeatureTestDBServer
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AGENCY, arangodb::LogLevel::FATAL>,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION, arangodb::LogLevel::ERR>,
      public arangodb::tests::LogSuppressor<arangodb::Logger::CLUSTER, arangodb::LogLevel::FATAL> {
 protected:
  arangodb::tests::mocks::MockV8Server server;

 private:
  arangodb::consensus::Store _agencyStore;
  arangodb::ServerState::RoleEnum _serverRoleBefore;

 protected:
  IResearchFeatureTestDBServer()
      : server(false),
        _agencyStore(server.server(), nullptr, "arango"),
        _serverRoleBefore(arangodb::ServerState::instance()->getRole()) {
    auto* agencyCommManager =
        new AgencyCommManagerMock(server.server(), "arango");
    std::ignore = agencyCommManager->addConnection<GeneralClientConnectionAgencyMock>(
        server.server(), _agencyStore);
    std::ignore = agencyCommManager->addConnection<GeneralClientConnectionAgencyMock>(
        server.server(), _agencyStore);  // need 2 connections or Agency callbacks will fail
    arangodb::AgencyCommManager::MANAGER.reset(agencyCommManager);

    arangodb::tests::init();

    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_DBSERVER);
    arangodb::ServerState::instance()->setRebootId(arangodb::RebootId{1});  // Hack.

    server.addFeature<arangodb::iresearch::IResearchAnalyzerFeature>(false);
    server.addFeature<arangodb::FlushFeature>(false);
    server.addFeature<arangodb::QueryRegistryFeature>(false);
    server.addFeature<arangodb::ServerSecurityFeature>(false);
    server.startFeatures();

    arangodb::AgencyCommManager::MANAGER->start();  // initialize agency
  }

  ~IResearchFeatureTestDBServer() {
    arangodb::ServerState::instance()->setRole(_serverRoleBefore);
  }

  // version 0 data-source path
  irs::utf8_path getPersistedPath0(arangodb::LogicalView const& view) {
    auto& dbPathFeature = server.getFeature<arangodb::DatabasePathFeature>();
    irs::utf8_path dataPath(dbPathFeature.directory());
    dataPath /= "databases";
    dataPath /= "database-";
    dataPath += std::to_string(view.vocbase().id());
    dataPath /= arangodb::iresearch::DATA_SOURCE_TYPE.name();
    dataPath += "-";
    dataPath += std::to_string(view.id());
    return dataPath;
  };

  // version 1 data-source path
  irs::utf8_path getPersistedPath1(arangodb::iresearch::IResearchLink const& link) {
    auto& dbPathFeature = server.getFeature<arangodb::DatabasePathFeature>();
    irs::utf8_path dataPath(dbPathFeature.directory());
    dataPath /= "databases";
    dataPath /= "database-";
    dataPath += std::to_string(link.collection().vocbase().id());
    dataPath /= arangodb::iresearch::DATA_SOURCE_TYPE.name();
    dataPath += "-";
    dataPath += std::to_string(link.collection().id());
    dataPath += "_";
    dataPath += std::to_string(link.id());
    return dataPath;
  }
};

TEST_F(IResearchFeatureTestDBServer, test_upgrade0_1_no_directory) {
  // test db-server (no directory)
  auto collectionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testCollection\" }");
  auto linkJson = arangodb::velocypack::Parser::fromJson(
      "{ \"view\": \"testView\", \"type\": \"arangosearch\", "
      "\"includeAllFields\": true }");
  auto viewJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", \"type\": \"arangosearch\", \"version\": 0 "
      "}");
  auto versionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"version\": 0, \"tasks\": {} }");

  // add the UpgradeFeature, but make sure it is not prepared
  server.addFeatureUntracked<arangodb::UpgradeFeature>(nullptr, std::vector<std::type_index>{});

  auto& feature = server.addFeatureUntracked<arangodb::iresearch::IResearchFeature>();
  feature.prepare();  // register iresearch view type
  feature.start();    // register upgrade tasks

  server.getFeature<arangodb::DatabaseFeature>().enableUpgrade();  // skip IResearchView validation

  auto& dbPathFeature = server.getFeature<arangodb::DatabasePathFeature>();
  arangodb::tests::setDatabasePath(dbPathFeature);  // ensure test data is stored in a unique directory
  auto versionFilename = StorageEngineMock::versionFilenameResult;
  auto versionFilenameRestore = irs::make_finally([&versionFilename]() -> void {
    StorageEngineMock::versionFilenameResult = versionFilename;
  });
  StorageEngineMock::versionFilenameResult =
      (irs::utf8_path(dbPathFeature.directory()) /= "version").utf8();
  ASSERT_TRUE(irs::utf8_path(dbPathFeature.directory()).mkdir());
  ASSERT_TRUE((arangodb::basics::VelocyPackHelper::velocyPackToFile(
      StorageEngineMock::versionFilenameResult, versionJson->slice(), false)));

  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
  auto logicalCollection = vocbase.createCollection(collectionJson->slice());
  ASSERT_FALSE(!logicalCollection);
  auto logicalView = vocbase.createView(viewJson->slice());
  ASSERT_FALSE(!logicalView);
  auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
  ASSERT_FALSE(!view);
  bool created = false;
  auto index = logicalCollection->createIndex(linkJson->slice(), created);
  ASSERT_TRUE(created);
  ASSERT_FALSE(!index);
  auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
  ASSERT_FALSE(!link);
  ASSERT_TRUE(view->link(link->self()).ok());  // link will not notify view in 'vocbase', hence notify manually

  index->unload();  // release file handles
  bool result;
  auto linkDataPath = getPersistedPath1(*link);
  EXPECT_TRUE(linkDataPath.remove());  // remove link directory
  auto viewDataPath = getPersistedPath0(*logicalView);
  EXPECT_TRUE(viewDataPath.exists(result) && !result);  // ensure no view directory
  arangodb::velocypack::Builder builder;
  builder.openObject();
  EXPECT_TRUE(logicalView->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence).ok());
  builder.close();
  EXPECT_EQ(0, builder.slice().get("version").getNumber<uint32_t>());  // ensure 'version == 0 before upgrade

  EXPECT_TRUE(arangodb::methods::Upgrade::startup(vocbase, true, false).ok());  // run upgrade
  logicalView = vocbase.lookupView(logicalView->name());
  EXPECT_FALSE(logicalView);  // ensure view removed after upgrade
  EXPECT_TRUE(viewDataPath.exists(result) && !result);  // ensure view directory not present
}

TEST_F(IResearchFeatureTestDBServer, test_upgrade0_1_with_directory) {
  // test db-server (with directory)
  auto collectionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testCollection\" }");
  auto linkJson = arangodb::velocypack::Parser::fromJson(
      "{ \"view\": \"testView\", \"type\": \"arangosearch\", "
      "\"includeAllFields\": true }");
  auto viewJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", \"type\": \"arangosearch\", \"version\": 0 "
      "}");
  auto versionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"version\": 0, \"tasks\": {} }");

  // add the UpgradeFeature, but make sure it is not prepared
  server.addFeatureUntracked<arangodb::UpgradeFeature>(nullptr, std::vector<std::type_index>{});

  auto& feature = server.addFeatureUntracked<arangodb::iresearch::IResearchFeature>();
  feature.prepare();  // register iresearch view type
  feature.start();    // register upgrade tasks

  server.getFeature<arangodb::DatabaseFeature>().enableUpgrade();  // skip IResearchView validation

  auto& dbPathFeature = server.getFeature<arangodb::DatabasePathFeature>();
  arangodb::tests::setDatabasePath(dbPathFeature);  // ensure test data is stored in a unique directory
  auto versionFilename = StorageEngineMock::versionFilenameResult;
  auto versionFilenameRestore = irs::make_finally([&versionFilename]() -> void {
    StorageEngineMock::versionFilenameResult = versionFilename;
  });
  StorageEngineMock::versionFilenameResult =
      (irs::utf8_path(dbPathFeature.directory()) /= "version").utf8();
  ASSERT_TRUE(irs::utf8_path(dbPathFeature.directory()).mkdir());
  ASSERT_TRUE((arangodb::basics::VelocyPackHelper::velocyPackToFile(
      StorageEngineMock::versionFilenameResult, versionJson->slice(), false)));

  auto& engine = *static_cast<StorageEngineMock*>(
      &server.getFeature<arangodb::EngineSelectorFeature>().engine());
  engine.views.clear();
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
  auto logicalCollection = vocbase.createCollection(collectionJson->slice());
  ASSERT_FALSE(!logicalCollection);
  auto logicalView = vocbase.createView(viewJson->slice());
  ASSERT_FALSE(!logicalView);
  auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
  ASSERT_FALSE(!view);
  bool created;
  auto index = logicalCollection->createIndex(linkJson->slice(), created);
  ASSERT_TRUE(created);
  ASSERT_FALSE(!index);
  auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
  ASSERT_FALSE(!link);
  ASSERT_TRUE(view->link(link->self()).ok());  // link will not notify view in 'vocbase', hence notify manually

  index->unload();  // release file handles
  bool result;
  auto linkDataPath = getPersistedPath1(*link);
  EXPECT_TRUE(linkDataPath.remove());  // remove link directory
  auto viewDataPath = getPersistedPath0(*logicalView);
  EXPECT_TRUE(viewDataPath.exists(result) && !result);
  EXPECT_TRUE(viewDataPath.mkdir());  // create view directory
  EXPECT_TRUE(viewDataPath.exists(result) && result);
  arangodb::velocypack::Builder builder;
  builder.openObject();
  EXPECT_TRUE(logicalView->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence).ok());
  builder.close();
  EXPECT_EQ(0, builder.slice().get("version").getNumber<uint32_t>());  // ensure 'version == 0 before upgrade

  EXPECT_TRUE(arangodb::methods::Upgrade::startup(vocbase, true, false).ok());  // run upgrade
  //    EXPECT_TRUE(arangodb::methods::Upgrade::clusterBootstrap(vocbase).ok()); // run upgrade
  logicalView = vocbase.lookupView(logicalView->name());
  EXPECT_FALSE(logicalView);  // ensure view removed after upgrade
  EXPECT_TRUE(viewDataPath.exists(result) && !result);  // ensure view directory removed after upgrade
}

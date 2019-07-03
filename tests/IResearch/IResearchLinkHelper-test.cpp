////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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

#include "Aql/QueryRegistry.h"
#include "Basics/files.h"
#include "Cluster/ClusterFeature.h"
#include "common.h"
#include "shared.hpp"

#if USE_ENTERPRISE
#include "Enterprise/Ldap/LdapFeature.h"
#endif

#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchLinkHelper.h"
#include "Mocks/StorageEngineMock.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/TraverserEngineRegistryFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Utils/ExecContext.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/LogicalCollection.h"
#include "utils/misc.hpp"
#include "velocypack/Parser.h"

static const VPackBuilder systemDatabaseBuilder = dbArgsBuilder();
static const VPackSlice   systemDatabaseArgs = systemDatabaseBuilder.slice();
// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class IResearchLinkHelperTest : public ::testing::Test {
 protected:
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;
  std::string testFilesystemPath;

  IResearchLinkHelperTest() : engine(server), server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    // suppress WARNING {authentication} --server.jwt-secret is insecure. Use --server.jwt-secret-keyfile instead
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(),
                                    arangodb::LogLevel::ERR);

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AGENCYCOMM.name(),
                                    arangodb::LogLevel::FATAL);
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(),
                                    arangodb::LogLevel::FATAL);

    features.emplace_back(new arangodb::AqlFeature(server),
                          true);  // required for UserManager::loadFromDB()
    features.emplace_back(new arangodb::AuthenticationFeature(server), false);  // required for authentication tests
    features.emplace_back(new arangodb::DatabaseFeature(server), false);
    features.emplace_back(new arangodb::DatabasePathFeature(server), false);  // required for IResearchLink::init(...)
    features.emplace_back(new arangodb::QueryRegistryFeature(server), false);  // required for constructing TRI_vocbase_t
    features.emplace_back(new arangodb::SystemDatabaseFeature(server), true);  // required by IResearchAnalyzerFeature::storeAnalyzer(...)
    features.emplace_back(new arangodb::TraverserEngineRegistryFeature(server),
                          false);  // required for AqlFeature::stop()
    features.emplace_back(new arangodb::V8DealerFeature(server),
                          false);  // required for DatabaseFeature::createDatabase(...)
    features.emplace_back(new arangodb::ViewTypesFeature(server),
                          false);  // required for LogicalView::instantiate(...)
    features.emplace_back(new arangodb::iresearch::IResearchAnalyzerFeature(server),
                          false);  // required for IResearchLinkMeta::init(...)
    features.emplace_back(new arangodb::iresearch::IResearchFeature(server), false);  // required for creating views of type 'iresearch'

#if USE_ENTERPRISE
    features.emplace_back(new arangodb::LdapFeature(server),
                          false);  // required for AuthenticationFeature with USE_ENTERPRISE
#endif

    // required for V8DealerFeature::prepare(), ClusterFeature::prepare() not required
    arangodb::application_features::ApplicationServer::server->addFeature(
        new arangodb::ClusterFeature(server));

    for (auto& f : features) {
      arangodb::application_features::ApplicationServer::server->addFeature(f.first);
    }

    for (auto& f : features) {
      f.first->prepare();
    }

    auto databases = VPackBuilder();
    databases.openArray();
    databases.add(systemDatabaseArgs);
    databases.close();

    auto* dbFeature =
        arangodb::application_features::ApplicationServer::lookupFeature<arangodb::DatabaseFeature>(
            "Database");
    dbFeature->loadDatabases(databases.slice());

    for (auto& f : features) {
      if (f.second) {
        f.first->start();
      }
    }

    auto* dbPathFeature =
        arangodb::application_features::ApplicationServer::getFeature<arangodb::DatabasePathFeature>(
            "DatabasePath");
    arangodb::tests::setDatabasePath(*dbPathFeature);  // ensure test data is stored in a unique directory
    testFilesystemPath = dbPathFeature->directory();

    long systemError;
    std::string systemErrorStr;
    TRI_CreateDirectory(testFilesystemPath.c_str(), systemError, systemErrorStr);
  }

  ~IResearchLinkHelperTest() {
    arangodb::application_features::ApplicationServer::server = nullptr;

    for (auto& f : features) {
      if (f.second) {
        f.first->stop();
      }
    }

    for (auto& f : features) {
      f.first->unprepare();
    }

    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(),
                                    arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AGENCYCOMM.name(),
                                    arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(),
                                    arangodb::LogLevel::DEFAULT);
    arangodb::EngineSelectorFeature::ENGINE = nullptr;
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(IResearchLinkHelperTest, test_equals) {
  // test slice not both object
  {
    auto lhs = arangodb::velocypack::Parser::fromJson("123");
    auto rhs = arangodb::velocypack::Parser::fromJson("{}");
    EXPECT_TRUE((false == arangodb::iresearch::IResearchLinkHelper::equal(lhs->slice(),
                                                                          rhs->slice())));
    EXPECT_TRUE((false == arangodb::iresearch::IResearchLinkHelper::equal(rhs->slice(),
                                                                          lhs->slice())));
  }

  // test view id same type (validate only meta)
  {
    auto lhs = arangodb::velocypack::Parser::fromJson("{ \"view\": 123 }");
    auto rhs = arangodb::velocypack::Parser::fromJson("{ \"view\": 123 }");
    EXPECT_TRUE((true == arangodb::iresearch::IResearchLinkHelper::equal(lhs->slice(),
                                                                         rhs->slice())));
    EXPECT_TRUE((true == arangodb::iresearch::IResearchLinkHelper::equal(rhs->slice(),
                                                                         lhs->slice())));
  }

  // test view id not same type (at least one non-string)
  {
    auto lhs = arangodb::velocypack::Parser::fromJson("{ \"view\": 123 }");
    auto rhs = arangodb::velocypack::Parser::fromJson("{ \"view\": \"abc\" }");
    EXPECT_TRUE((false == arangodb::iresearch::IResearchLinkHelper::equal(lhs->slice(),
                                                                          rhs->slice())));
    EXPECT_TRUE((false == arangodb::iresearch::IResearchLinkHelper::equal(rhs->slice(),
                                                                          lhs->slice())));
  }

  // test view id prefix (up to /) not equal (at least one empty)
  {
    auto lhs = arangodb::velocypack::Parser::fromJson("{ \"view\": \"\" }");
    auto rhs = arangodb::velocypack::Parser::fromJson("{ \"view\": \"abc\" }");
    EXPECT_TRUE((false == arangodb::iresearch::IResearchLinkHelper::equal(lhs->slice(),
                                                                          rhs->slice())));
    EXPECT_TRUE((false == arangodb::iresearch::IResearchLinkHelper::equal(rhs->slice(),
                                                                          lhs->slice())));
  }

  // test view id prefix (up to /) not equal (shorter does not end with '/')
  {
    auto lhs = arangodb::velocypack::Parser::fromJson("{ \"view\": \"a\" }");
    auto rhs = arangodb::velocypack::Parser::fromJson("{ \"view\": \"abc\" }");
    EXPECT_TRUE((false == arangodb::iresearch::IResearchLinkHelper::equal(lhs->slice(),
                                                                          rhs->slice())));
    EXPECT_TRUE((false == arangodb::iresearch::IResearchLinkHelper::equal(rhs->slice(),
                                                                          lhs->slice())));
  }

  // test view id prefix (up to /) not equal (shorter ends with '/' but not a prefix of longer)
  {
    auto lhs = arangodb::velocypack::Parser::fromJson("{ \"view\": \"a/\" }");
    auto rhs = arangodb::velocypack::Parser::fromJson("{ \"view\": \"ab/c\" }");
    EXPECT_TRUE((false == arangodb::iresearch::IResearchLinkHelper::equal(lhs->slice(),
                                                                          rhs->slice())));
    EXPECT_TRUE((false == arangodb::iresearch::IResearchLinkHelper::equal(rhs->slice(),
                                                                          lhs->slice())));
  }

  // test view id prefix (up to /) equal
  {
    auto lhs = arangodb::velocypack::Parser::fromJson("{ \"view\": \"a/\" }");
    auto rhs = arangodb::velocypack::Parser::fromJson("{ \"view\": \"a/bc\" }");
    EXPECT_TRUE((true == arangodb::iresearch::IResearchLinkHelper::equal(lhs->slice(),
                                                                         rhs->slice())));
    EXPECT_TRUE((true == arangodb::iresearch::IResearchLinkHelper::equal(rhs->slice(),
                                                                         lhs->slice())));
  }

  // test meta init fail
  {
    auto lhs = arangodb::velocypack::Parser::fromJson("{ \"view\": \"a/\" }");
    auto rhs = arangodb::velocypack::Parser::fromJson(
        "{ \"view\": \"a/bc\", \"includeAllFields\": 42 }");
    EXPECT_TRUE((false == arangodb::iresearch::IResearchLinkHelper::equal(lhs->slice(),
                                                                          rhs->slice())));
    EXPECT_TRUE((false == arangodb::iresearch::IResearchLinkHelper::equal(rhs->slice(),
                                                                          lhs->slice())));
  }

  // test meta not equal
  {
    auto lhs = arangodb::velocypack::Parser::fromJson(
        "{ \"view\": \"a/\", \"includeAllFields\": false }");
    auto rhs = arangodb::velocypack::Parser::fromJson(
        "{ \"view\": \"a/bc\", \"includeAllFields\": true }");
    EXPECT_TRUE((false == arangodb::iresearch::IResearchLinkHelper::equal(lhs->slice(),
                                                                          rhs->slice())));
    EXPECT_TRUE((false == arangodb::iresearch::IResearchLinkHelper::equal(rhs->slice(),
                                                                          lhs->slice())));
  }

  // test equal
  {
    auto lhs = arangodb::velocypack::Parser::fromJson(
        "{ \"view\": \"a/\", \"includeAllFields\": false }");
    auto rhs = arangodb::velocypack::Parser::fromJson(
        "{ \"view\": \"a/bc\", \"includeAllFields\": false }");
    EXPECT_TRUE((true == arangodb::iresearch::IResearchLinkHelper::equal(lhs->slice(),
                                                                         rhs->slice())));
    EXPECT_TRUE((true == arangodb::iresearch::IResearchLinkHelper::equal(rhs->slice(),
                                                                         lhs->slice())));
  }
}

TEST_F(IResearchLinkHelperTest, test_normalize) {
  auto* analyzers =
      arangodb::application_features::ApplicationServer::lookupFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
  arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
  auto* sysDatabase =
      arangodb::application_features::ApplicationServer::lookupFeature<arangodb::SystemDatabaseFeature>();
  auto sysVocbase = sysDatabase->use();

  // create analyzer collection
  {
    static std::string const ANALYZER_COLLECTION_NAME("_iresearch_analyzers");

    if (!sysVocbase->lookupCollection(ANALYZER_COLLECTION_NAME)) {
      auto collectionJson = arangodb::velocypack::Parser::fromJson(
          std::string("{ \"name\": \"") + ANALYZER_COLLECTION_NAME +
          "\", \"isSystem\": true }");
      auto logicalCollection = sysVocbase->createCollection(collectionJson->slice());
      ASSERT_TRUE((false == !logicalCollection));
    }
  }

  // analyzer single-server
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \
      \"analyzerDefinitions\": [ { \"name\": \"testAnalyzer0\", \"type\": \"identity\" } ], \
      \"analyzers\": [\"testAnalyzer0\" ] \
    }");
    arangodb::velocypack::Builder builder;
    builder.openObject();
    EXPECT_TRUE((false == arangodb::iresearch::IResearchLinkHelper::normalize(
                              builder, json->slice(), false, *sysVocbase)
                              .ok()));
    EXPECT_TRUE((true == !analyzers->get(arangodb::StaticStrings::SystemDatabase +
                                         "::testAnalyzer1")));
  }

  // analyzer single-server (inRecovery) fail persist in recovery
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \
      \"analyzerDefinitions\": [ { \"name\": \"testAnalyzer1\", \"type\": \"identity\" } ], \
      \"analyzers\": [\"testAnalyzer1\" ] \
    }");
    auto before = StorageEngineMock::inRecoveryResult;
    StorageEngineMock::inRecoveryResult = true;
    auto restore = irs::make_finally(
        [&before]() -> void { StorageEngineMock::inRecoveryResult = before; });
    arangodb::velocypack::Builder builder;
    builder.openObject();
    EXPECT_TRUE((false == arangodb::iresearch::IResearchLinkHelper::normalize(
                              builder, json->slice(), false, *sysVocbase)
                              .ok()));
    EXPECT_TRUE((true == !analyzers->get(arangodb::StaticStrings::SystemDatabase +
                                         "::testAnalyzer2")));
  }

  // analyzer single-server (no engine) fail persist if not storage engine, else SEGFAULT in Methods(...)
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \
      \"analyzerDefinitions\": [ { \"name\": \"testAnalyzer2\", \"type\": \"identity\" } ], \
      \"analyzers\": [\"testAnalyzer2\" ] \
    }");
    auto* before = arangodb::EngineSelectorFeature::ENGINE;
    arangodb::EngineSelectorFeature::ENGINE = nullptr;
    auto restore = irs::make_finally([&before]() -> void {
      arangodb::EngineSelectorFeature::ENGINE = before;
    });
    arangodb::velocypack::Builder builder;
    builder.openObject();
    EXPECT_TRUE((false == arangodb::iresearch::IResearchLinkHelper::normalize(
                              builder, json->slice(), false, *sysVocbase)
                              .ok()));
    EXPECT_TRUE((true == !analyzers->get(arangodb::StaticStrings::SystemDatabase +
                                         "::testAnalyzer3")));
  }

  // analyzer coordinator
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \
      \"analyzerDefinitions\": [ { \"name\": \"testAnalyzer3\", \"type\": \"identity\" } ], \
      \"analyzers\": [\"testAnalyzer3\" ] \
    }");
    auto serverRoleBefore = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_COORDINATOR);
    auto serverRoleRestore = irs::make_finally([&serverRoleBefore]() -> void {
      arangodb::ServerState::instance()->setRole(serverRoleBefore);
    });
    arangodb::velocypack::Builder builder;
    builder.openObject();
    EXPECT_TRUE((false == arangodb::iresearch::IResearchLinkHelper::normalize(
                              builder, json->slice(), false, *sysVocbase)
                              .ok()));
    EXPECT_TRUE((true == !analyzers->get(arangodb::StaticStrings::SystemDatabase +
                                         "::testAnalyzer4")));
  }

  // analyzer coordinator (inRecovery) fail persist in recovery
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \
      \"analyzerDefinitions\": [ { \"name\": \"testAnalyzer5\", \"type\": \"identity\" } ], \
      \"analyzers\": [\"testAnalyzer5\" ] \
     }");
    auto serverRoleBefore = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_COORDINATOR);
    auto serverRoleRestore = irs::make_finally([&serverRoleBefore]() -> void {
      arangodb::ServerState::instance()->setRole(serverRoleBefore);
    });
    auto inRecoveryBefore = StorageEngineMock::inRecoveryResult;
    StorageEngineMock::inRecoveryResult = true;
    auto restore = irs::make_finally([&inRecoveryBefore]() -> void {
      StorageEngineMock::inRecoveryResult = inRecoveryBefore;
    });
    arangodb::velocypack::Builder builder;
    builder.openObject();
    EXPECT_TRUE((true == arangodb::iresearch::IResearchLinkHelper::normalize(
                             builder, json->slice(), false, *sysVocbase)
                             .ok()));
    EXPECT_TRUE((true == !analyzers->get(arangodb::StaticStrings::SystemDatabase +
                                         "::testAnalyzer5")));
  }

  // analyzer coordinator (no engine)
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \
      \"analyzerDefinitions\": [ { \"name\": \"testAnalyzer6\", \"type\": \"identity\" } ], \
      \"analyzers\": [\"testAnalyzer6\" ] \
    }");
    auto serverRoleBefore = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_COORDINATOR);
    auto serverRoleRestore = irs::make_finally([&serverRoleBefore]() -> void {
      arangodb::ServerState::instance()->setRole(serverRoleBefore);
    });
    auto* engineBefore = arangodb::EngineSelectorFeature::ENGINE;
    arangodb::EngineSelectorFeature::ENGINE = nullptr;
    auto restore = irs::make_finally([&engineBefore]() -> void {
      arangodb::EngineSelectorFeature::ENGINE = engineBefore;
    });
    arangodb::velocypack::Builder builder;
    builder.openObject();
    EXPECT_TRUE((false == arangodb::iresearch::IResearchLinkHelper::normalize(
                              builder, json->slice(), false, *sysVocbase)
                              .ok()));
    EXPECT_TRUE((true == !analyzers->get(arangodb::StaticStrings::SystemDatabase +
                                         "::testAnalyzer6")));
  }

  // analyzer db-server
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \
      \"analyzerDefinitions\": [ { \"name\": \"testAnalyzer7\", \"type\": \"identity\" } ], \
      \"analyzers\": [\"testAnalyzer7\" ] \
    }");
    auto before = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_DBSERVER);
    auto serverRoleRestore = irs::make_finally([&before]() -> void {
      arangodb::ServerState::instance()->setRole(before);
    });
    arangodb::velocypack::Builder builder;
    builder.openObject();
    EXPECT_TRUE((true == !analyzers->get(arangodb::StaticStrings::SystemDatabase +
                                         "::testAnalyzer7")));
    EXPECT_TRUE((true == arangodb::iresearch::IResearchLinkHelper::normalize(
                             builder, json->slice(), false, *sysVocbase)
                             .ok()));
    EXPECT_TRUE((false == !analyzers->get(arangodb::StaticStrings::SystemDatabase +
                                          "::testAnalyzer7")));
  }

  // meta has analyzer which is not authorised
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"type\": \"arangosearch\", \"view\": \"43\", \"analyzers\": [ "
        "\"::unAuthorsedAnalyzer\" ] }");
    auto* analyzers =
        arangodb::application_features::ApplicationServer::lookupFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    ASSERT_TRUE((analyzers
                     ->emplace(result, arangodb::StaticStrings::SystemDatabase + "::unAuthorsedAnalyzer",
                               "identity", VPackSlice::nullSlice())
                     .ok()));
    ASSERT_TRUE((false == !result.first));

    // not authorised
    {
      struct ExecContext : public arangodb::ExecContext {
        ExecContext()
            : arangodb::ExecContext(arangodb::ExecContext::Type::Default, "",
                                    "", arangodb::auth::Level::NONE,
                                    arangodb::auth::Level::NONE) {}
      } execContext;
      arangodb::ExecContextScope execContextScope(&execContext);
      auto* authFeature = arangodb::AuthenticationFeature::instance();
      auto* userManager = authFeature->userManager();
      arangodb::aql::QueryRegistry queryRegistry(0);  // required for UserManager::loadFromDB()
      userManager->setQueryRegistry(&queryRegistry);
      auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(
          userManager,
          [](arangodb::auth::UserManager* ptr) -> void { ptr->removeAllUsers(); });

      arangodb::velocypack::Builder builder;
      builder.openObject();
      EXPECT_TRUE((false == arangodb::iresearch::IResearchLinkHelper::normalize(
                                builder, json->slice(), false, *sysVocbase)
                                .ok()));
    }

    // authorsed
    {
      arangodb::velocypack::Builder builder;
      builder.openObject();
      EXPECT_TRUE((true == arangodb::iresearch::IResearchLinkHelper::normalize(
                               builder, json->slice(), false, *sysVocbase)
                               .ok()));
    }
  }
}

TEST_F(IResearchLinkHelperTest, test_updateLinks) {
  // meta has analyzer which is not authorised
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection\", \"id\": 101 }");
    auto linkUpdateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"testCollection\": { \"type\": \"arangosearch\", \"view\": \"43\", "
        "\"analyzers\": [ \"::unAuthorsedAnalyzer\" ] } }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"id\": 43, \"type\": \"arangosearch\" }");
    auto* analyzers =
        arangodb::application_features::ApplicationServer::lookupFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    ASSERT_TRUE((nullptr != analyzers));
    auto* dbFeature =
        arangodb::application_features::ApplicationServer::lookupFeature<arangodb::DatabaseFeature>(
            "Database");
    TRI_vocbase_t* vocbase;
    ASSERT_TRUE(
        (TRI_ERROR_NO_ERROR == dbFeature->createDatabase(1, "testVocbase", arangodb::velocypack::Slice::emptyObjectSlice(), vocbase)));  // required for IResearchAnalyzerFeature::emplace(...)
    ASSERT_TRUE((nullptr != vocbase));
    auto dropDB = irs::make_finally([dbFeature]() -> void {
      dbFeature->dropDatabase("testVocbase", true, true);
    });
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    ASSERT_TRUE((analyzers
                     ->emplace(result, arangodb::StaticStrings::SystemDatabase + "::unAuthorsedAnalyzer",
                               "identity", VPackSlice::nullSlice())
                     .ok()));
    ASSERT_TRUE((false == !result.first));

    auto logicalCollection = vocbase->createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection));
    auto logicalView = vocbase->createView(viewCreateJson->slice());
    ASSERT_TRUE((false == !logicalView));

    // not authorized
    {
      struct ExecContext : public arangodb::ExecContext {
        ExecContext()
            : arangodb::ExecContext(arangodb::ExecContext::Type::Default, "",
                                    "", arangodb::auth::Level::NONE,
                                    arangodb::auth::Level::NONE) {}
      } execContext;
      arangodb::ExecContextScope execContextScope(&execContext);
      auto* authFeature = arangodb::AuthenticationFeature::instance();
      auto* userManager = authFeature->userManager();
      arangodb::aql::QueryRegistry queryRegistry(0);  // required for UserManager::loadFromDB()
      userManager->setQueryRegistry(&queryRegistry);
      auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(
          userManager,
          [](arangodb::auth::UserManager* ptr) -> void { ptr->removeAllUsers(); });

      std::unordered_set<TRI_voc_cid_t> modified;
      EXPECT_TRUE((0 == logicalCollection->getIndexes().size()));
      EXPECT_TRUE((false == arangodb::iresearch::IResearchLinkHelper::updateLinks(
                                modified, *logicalView, linkUpdateJson->slice())
                                .ok()));
      EXPECT_TRUE((0 == logicalCollection->getIndexes().size()));
    }

    // authorzed
    {
      std::unordered_set<TRI_voc_cid_t> modified;
      EXPECT_TRUE((0 == logicalCollection->getIndexes().size()));
      EXPECT_TRUE((true == arangodb::iresearch::IResearchLinkHelper::updateLinks(
                               modified, *logicalView, linkUpdateJson->slice())
                               .ok()));
      EXPECT_TRUE((1 == logicalCollection->getIndexes().size()));
    }
  }
}

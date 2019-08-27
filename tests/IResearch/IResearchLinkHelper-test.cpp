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
#include "common.h"
#include "shared.hpp"

#include "Auth/UserManager.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchLinkCoordinator.h"
#include "IResearch/IResearchLinkHelper.h"
#include "Mocks/StorageEngineMock.h"
#include "Mocks/Servers.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Utils/ExecContext.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"
#include "VocBase/Methods/Databases.h"
#include "utils/misc.hpp"
#include "velocypack/Parser.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class IResearchLinkHelperTestSingle : public ::testing::Test {
 protected:
  arangodb::tests::mocks::MockAqlServer server;

  IResearchLinkHelperTestSingle() {
    auto* dbFeature =
      arangodb::application_features::ApplicationServer::lookupFeature<arangodb::DatabaseFeature>("Database");
    {
      TRI_vocbase_t* vocbase = dbFeature->useDatabase(arangodb::StaticStrings::SystemDatabase);
      arangodb::methods::Collections::createSystem(
        *vocbase,
        arangodb::tests::AnalyzerCollectionName, false);
    }
    {
      TRI_vocbase_t* vocbase;
      dbFeature->createDatabase(1, "testVocbaseWithAnalyzer", vocbase);
      arangodb::methods::Collections::createSystem(
         *vocbase,
         arangodb::tests::AnalyzerCollectionName, false);
    }
    {
      TRI_vocbase_t* vocbase;
      dbFeature->createDatabase(2, "testVocbaseWithView", vocbase);
      arangodb::methods::Collections::createSystem(
        *vocbase,
        arangodb::tests::AnalyzerCollectionName, false);
      auto collectionJson = arangodb::velocypack::Parser::fromJson(
          "{ \"id\":102, \"name\": \"foo\" }");
      EXPECT_NE(nullptr, vocbase->createCollection(collectionJson->slice()));
    }
  }

  ~IResearchLinkHelperTestSingle() {
  }
};

class IResearchLinkHelperTestCoordinator : public ::testing::Test {
 protected:
  arangodb::tests::mocks::MockCoordinator server;
  arangodb::consensus::Store& _agencyStore;

  IResearchLinkHelperTestCoordinator() : server(), _agencyStore(server.getAgencyStore()) {
    TRI_vocbase_t* vocbase;
    createTestDatabase(vocbase, "testVocbaseWithAnalyzer");
    createTestDatabase(vocbase, "testVocbaseWithView");
  }

  ~IResearchLinkHelperTestCoordinator() {
  }

  void createTestDatabase(TRI_vocbase_t*& vocbase, std::string const name = "testDatabase") {
    vocbase = server.createDatabase(name);
    ASSERT_NE(nullptr, vocbase);
    ASSERT_EQ(name, vocbase->name());
    ASSERT_EQ(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_COORDINATOR, vocbase->type());
  }
};

class IResearchLinkHelperTestDBServer : public ::testing::Test {
 protected:
  arangodb::tests::mocks::MockDBServer server;
  arangodb::consensus::Store& _agencyStore;

  IResearchLinkHelperTestDBServer() : server(), _agencyStore(server.getAgencyStore()) {
    TRI_vocbase_t* vocbase;
    createTestDatabase(vocbase, "testVocbaseWithAnalyzer");
    createTestDatabase(vocbase, "testVocbaseWithView");
  }

  ~IResearchLinkHelperTestDBServer() {
  }

  void createTestDatabase(TRI_vocbase_t*& vocbase, std::string const name = "testDatabase") {
    vocbase = server.createDatabase(name);
    ASSERT_NE(nullptr, vocbase);
    ASSERT_EQ(name, vocbase->name());
    ASSERT_EQ(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, vocbase->type());
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(IResearchLinkHelperTestSingle, test_equals) {
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

TEST_F(IResearchLinkHelperTestSingle, test_validate_cross_db_analyzer) {
  auto* analyzers =
    arangodb::application_features::ApplicationServer::lookupFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
  ASSERT_NE(nullptr, analyzers);
  auto* dbFeature =
    arangodb::application_features::ApplicationServer::lookupFeature<arangodb::DatabaseFeature>("Database");
  ASSERT_NE(nullptr, dbFeature);
  {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult emplaceResult;
    ASSERT_TRUE(analyzers->emplace(emplaceResult, "testVocbaseWithAnalyzer::myIdentity", "identity",
      VPackParser::fromJson("{ }")->slice()).ok());
  }

  // existing analyzer  but wrong database
  {
    auto vocbaseLocal = dbFeature->useDatabase("testVocbaseWithView");
    ASSERT_NE(nullptr, vocbaseLocal);
    auto json = VPackParser::fromJson(
      "{ \"foo\": "
      "         { "
      "           \"analyzers\": [ \"testVocbaseWithAnalyzer::myIdentity\" ] "
      "         } "
      " }");
    auto validateResult = arangodb::iresearch::IResearchLinkHelper::validateLinks(
      *vocbaseLocal, json->slice());
    EXPECT_FALSE(validateResult.ok());
    EXPECT_EQ(TRI_ERROR_BAD_PARAMETER, validateResult.errorNumber());
  }

}

TEST_F(IResearchLinkHelperTestSingle, test_normalize_single) {
  auto* analyzers =
      arangodb::application_features::ApplicationServer::lookupFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
  arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
  TRI_vocbase_t& sysVocbase = server.getSystemDatabase();

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
                              builder, json->slice(), false, sysVocbase)
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
    auto before = StorageEngineMock::recoveryStateResult;
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
    auto restore = irs::make_finally(
        [&before]() -> void { StorageEngineMock::recoveryStateResult = before; });
    arangodb::velocypack::Builder builder;
    builder.openObject();
    EXPECT_TRUE((false == arangodb::iresearch::IResearchLinkHelper::normalize(
                              builder, json->slice(), false, sysVocbase)
                              .ok()));
    EXPECT_TRUE((true == !analyzers->get(arangodb::StaticStrings::SystemDatabase +
                                         "::testAnalyzer2")));
  }
}

TEST_F(IResearchLinkHelperTestCoordinator, DISABLED_test_normalize_coordinator) {
  auto* analyzers =
      arangodb::application_features::ApplicationServer::lookupFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
  arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
  TRI_vocbase_t& sysVocbase = server.getSystemDatabase();

  // analyzer coordinator
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \
      \"analyzerDefinitions\": [ { \"name\": \"testAnalyzer3\", \"type\": \"identity\" } ], \
      \"analyzers\": [\"testAnalyzer3\" ] \
    }");
    arangodb::velocypack::Builder builder;
    builder.openObject();
    EXPECT_TRUE((false == arangodb::iresearch::IResearchLinkHelper::normalize(
                              builder, json->slice(), false, sysVocbase)
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
    auto inRecoveryBefore = StorageEngineMock::recoveryStateResult;
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
    auto restore = irs::make_finally([&inRecoveryBefore]() -> void {
      StorageEngineMock::recoveryStateResult = inRecoveryBefore;
    });
    arangodb::velocypack::Builder builder;
    builder.openObject();
    EXPECT_TRUE((true == arangodb::iresearch::IResearchLinkHelper::normalize(
                             builder, json->slice(), false, sysVocbase)
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
    auto* engineBefore = arangodb::EngineSelectorFeature::ENGINE;
    arangodb::EngineSelectorFeature::ENGINE = nullptr;
    auto restore = irs::make_finally([&engineBefore]() -> void {
      arangodb::EngineSelectorFeature::ENGINE = engineBefore;
    });
    arangodb::velocypack::Builder builder;
    builder.openObject();
    EXPECT_TRUE((false == arangodb::iresearch::IResearchLinkHelper::normalize(
                              builder, json->slice(), false, sysVocbase)
                              .ok()));
    EXPECT_TRUE((true == !analyzers->get(arangodb::StaticStrings::SystemDatabase +
                                         "::testAnalyzer6")));
  }
}

TEST_F(IResearchLinkHelperTestDBServer, DISABLED_test_normalize_dbserver) {
  auto* analyzers =
      arangodb::application_features::ApplicationServer::lookupFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
  arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
  TRI_vocbase_t& sysVocbase = server.getSystemDatabase();

  // analyzer db-server
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \
      \"analyzerDefinitions\": [ { \"name\": \"testAnalyzer7\", \"type\": \"identity\" } ], \
      \"analyzers\": [\"testAnalyzer7\" ] \
    }");
    arangodb::velocypack::Builder builder;
    builder.openObject();
    EXPECT_TRUE((true == !analyzers->get(arangodb::StaticStrings::SystemDatabase +
                                         "::testAnalyzer7")));
    EXPECT_TRUE((true == arangodb::iresearch::IResearchLinkHelper::normalize(
                             builder, json->slice(), false, sysVocbase)
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
                                builder, json->slice(), false, sysVocbase)
                                .ok()));
    }

    // authorsed
    {
      arangodb::velocypack::Builder builder;
      builder.openObject();
      EXPECT_TRUE((true == arangodb::iresearch::IResearchLinkHelper::normalize(
                               builder, json->slice(), false, sysVocbase)
                               .ok()));
    }
  }
}

TEST_F(IResearchLinkHelperTestSingle, test_updateLinks) {
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
    TRI_vocbase_t* vocbase;
    auto* dbFeature =
      arangodb::application_features::ApplicationServer::lookupFeature<arangodb::DatabaseFeature>("Database");
    ASSERT_NE(nullptr, dbFeature);
    ASSERT_TRUE(
        (TRI_ERROR_NO_ERROR == dbFeature->createDatabase(1, "testVocbase", vocbase)));  // required for IResearchAnalyzerFeature::emplace(...)
    ASSERT_TRUE((nullptr != vocbase));
  
    arangodb::methods::Collections::createSystem(
        *vocbase,
        arangodb::tests::AnalyzerCollectionName, false);

    {
      auto* sysDb = dbFeature->useDatabase(arangodb::StaticStrings::SystemDatabase);
      arangodb::methods::Collections::createSystem(
          *sysDb,
          arangodb::tests::AnalyzerCollectionName, false);
    }

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

    // register factories & normalizers
    auto* engine = arangodb::EngineSelectorFeature::ENGINE;
    auto& indexFactory = const_cast<arangodb::IndexFactory&>(engine->indexFactory());
    indexFactory.emplace(arangodb::iresearch::DATA_SOURCE_TYPE.name(),
                         arangodb::iresearch::IResearchLinkCoordinator::factory());

    // authorized
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

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
    auto& dbFeature = server.getFeature<arangodb::DatabaseFeature>();
    {
      TRI_vocbase_t* vocbase = dbFeature.useDatabase(arangodb::StaticStrings::SystemDatabase);
      arangodb::methods::Collections::createSystem(
        *vocbase,
        arangodb::tests::AnalyzerCollectionName, false);
    }
    {
      TRI_vocbase_t* vocbase;
      dbFeature.createDatabase(testDBInfo(server.server(), "testVocbaseWithAnalyzer", 1), vocbase);
      arangodb::methods::Collections::createSystem(
        *vocbase,
         arangodb::tests::AnalyzerCollectionName, false);
    }
    {
      TRI_vocbase_t* vocbase;
      dbFeature.createDatabase(testDBInfo(server.server(), "testVocbaseWithView",2), vocbase);
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
  auto& analyzers = server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
  auto& dbFeature = server.getFeature<arangodb::DatabaseFeature>();
  {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult emplaceResult;
    ASSERT_TRUE(analyzers
                    .emplace(emplaceResult,
                             "testVocbaseWithAnalyzer::myIdentity", "identity",
                             VPackParser::fromJson("{ }")->slice())
                    .ok());
  }

  // existing analyzer  but wrong database
  {
    auto vocbaseLocal = dbFeature.useDatabase("testVocbaseWithView");
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

TEST_F(IResearchLinkHelperTestSingle, test_normalize) {
  auto& analyzers = server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
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
    EXPECT_TRUE((true == !analyzers.get(arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1")));
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
    EXPECT_TRUE((true == !analyzers.get(arangodb::StaticStrings::SystemDatabase + "::testAnalyzer2")));
  }
}

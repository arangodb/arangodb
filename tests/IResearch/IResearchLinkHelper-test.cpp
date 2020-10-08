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
    arangodb::OperationOptions options(arangodb::ExecContext::current());
    {
      TRI_vocbase_t* vocbase = dbFeature.useDatabase(arangodb::StaticStrings::SystemDatabase);
      std::shared_ptr<arangodb::LogicalCollection> unused;
      arangodb::methods::Collections::createSystem(*vocbase, options,
                                                   arangodb::tests::AnalyzerCollectionName,
                                                   false, unused);
    }
    {
      TRI_vocbase_t* vocbase;
      dbFeature.createDatabase(testDBInfo(server.server(), "testVocbaseWithAnalyzer", 1), vocbase);
      std::shared_ptr<arangodb::LogicalCollection> unused;
      arangodb::methods::Collections::createSystem(*vocbase, options,
                                                   arangodb::tests::AnalyzerCollectionName,
                                                   false, unused);
    }
    {
      TRI_vocbase_t* vocbase;
      dbFeature.createDatabase(testDBInfo(server.server(), "testVocbaseWithView",2), vocbase);
      std::shared_ptr<arangodb::LogicalCollection> unused;
      arangodb::methods::Collections::createSystem(*vocbase, options,
                                                   arangodb::tests::AnalyzerCollectionName,
                                                   false, unused);
      auto collectionJson = arangodb::velocypack::Parser::fromJson(
          "{ \"id\":102, \"name\": \"foo\" }");
      EXPECT_NE(nullptr, vocbase->createCollection(collectionJson->slice()));
    }
    }

  ~IResearchLinkHelperTestSingle() = default;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(IResearchLinkHelperTestSingle, test_equals) {
  // test slice not both object
  {
    auto lhs = arangodb::velocypack::Parser::fromJson("123");
    auto rhs = arangodb::velocypack::Parser::fromJson("{}");
    EXPECT_TRUE((false == arangodb::iresearch::IResearchLinkHelper::equal(
                              server.server(), lhs->slice(), rhs->slice(), irs::string_ref::NIL)));
    EXPECT_TRUE((false == arangodb::iresearch::IResearchLinkHelper::equal(
                              server.server(), rhs->slice(), lhs->slice(), irs::string_ref::NIL)));
  }

  // test view id same type (validate only meta)
  {
    auto lhs = arangodb::velocypack::Parser::fromJson("{ \"view\": 123 }");
    auto rhs = arangodb::velocypack::Parser::fromJson("{ \"view\": 123 }");
    EXPECT_TRUE((true == arangodb::iresearch::IResearchLinkHelper::equal(
                             server.server(), lhs->slice(), rhs->slice(), irs::string_ref::NIL)));
    EXPECT_TRUE((true == arangodb::iresearch::IResearchLinkHelper::equal(
                             server.server(), rhs->slice(), lhs->slice(), irs::string_ref::NIL)));
  }

  // test view id not same type (at least one non-string)
  {
    auto lhs = arangodb::velocypack::Parser::fromJson("{ \"view\": 123 }");
    auto rhs = arangodb::velocypack::Parser::fromJson("{ \"view\": \"abc\" }");
    EXPECT_TRUE((false == arangodb::iresearch::IResearchLinkHelper::equal(
                              server.server(), lhs->slice(), rhs->slice(), irs::string_ref::NIL)));
    EXPECT_TRUE((false == arangodb::iresearch::IResearchLinkHelper::equal(
                              server.server(), rhs->slice(), lhs->slice(), irs::string_ref::NIL)));
  }

  // test view id prefix (up to /) not equal (at least one empty)
  {
    auto lhs = arangodb::velocypack::Parser::fromJson("{ \"view\": \"\" }");
    auto rhs = arangodb::velocypack::Parser::fromJson("{ \"view\": \"abc\" }");
    EXPECT_TRUE((false == arangodb::iresearch::IResearchLinkHelper::equal(
                              server.server(), lhs->slice(), rhs->slice(), irs::string_ref::NIL)));
    EXPECT_TRUE((false == arangodb::iresearch::IResearchLinkHelper::equal(
                              server.server(), rhs->slice(), lhs->slice(), irs::string_ref::NIL)));
  }

  // test view id prefix (up to /) not equal (shorter does not end with '/')
  {
    auto lhs = arangodb::velocypack::Parser::fromJson("{ \"view\": \"a\" }");
    auto rhs = arangodb::velocypack::Parser::fromJson("{ \"view\": \"abc\" }");
    EXPECT_TRUE((false == arangodb::iresearch::IResearchLinkHelper::equal(
                              server.server(), lhs->slice(), rhs->slice(), irs::string_ref::NIL)));
    EXPECT_TRUE((false == arangodb::iresearch::IResearchLinkHelper::equal(
                              server.server(), rhs->slice(), lhs->slice(), irs::string_ref::NIL)));
  }

  // test view id prefix (up to /) not equal (shorter ends with '/' but not a prefix of longer)
  {
    auto lhs = arangodb::velocypack::Parser::fromJson("{ \"view\": \"a/\" }");
    auto rhs = arangodb::velocypack::Parser::fromJson("{ \"view\": \"ab/c\" }");
    EXPECT_TRUE((false == arangodb::iresearch::IResearchLinkHelper::equal(
                              server.server(), lhs->slice(), rhs->slice(), irs::string_ref::NIL)));
    EXPECT_TRUE((false == arangodb::iresearch::IResearchLinkHelper::equal(
                              server.server(), rhs->slice(), lhs->slice(), irs::string_ref::NIL)));
  }

  // test view id prefix (up to /) equal
  {
    auto lhs = arangodb::velocypack::Parser::fromJson("{ \"view\": \"a/\" }");
    auto rhs = arangodb::velocypack::Parser::fromJson("{ \"view\": \"a/bc\" }");
    EXPECT_TRUE((true == arangodb::iresearch::IResearchLinkHelper::equal(
                             server.server(), lhs->slice(), rhs->slice(), irs::string_ref::NIL)));
    EXPECT_TRUE((true == arangodb::iresearch::IResearchLinkHelper::equal(
                             server.server(), rhs->slice(), lhs->slice(), irs::string_ref::NIL)));
  }

  // test meta init fail
  {
    auto lhs = arangodb::velocypack::Parser::fromJson("{ \"view\": \"a/\" }");
    auto rhs = arangodb::velocypack::Parser::fromJson(
        "{ \"view\": \"a/bc\", \"includeAllFields\": 42 }");
    EXPECT_TRUE((false == arangodb::iresearch::IResearchLinkHelper::equal(
                              server.server(), lhs->slice(), rhs->slice(), irs::string_ref::NIL)));
    EXPECT_TRUE((false == arangodb::iresearch::IResearchLinkHelper::equal(
                              server.server(), rhs->slice(), lhs->slice(), irs::string_ref::NIL)));
  }

  // test meta not equal
  {
    auto lhs = arangodb::velocypack::Parser::fromJson(
        "{ \"view\": \"a/\", \"includeAllFields\": false }");
    auto rhs = arangodb::velocypack::Parser::fromJson(
        "{ \"view\": \"a/bc\", \"includeAllFields\": true }");
    EXPECT_TRUE((false == arangodb::iresearch::IResearchLinkHelper::equal(
                              server.server(), lhs->slice(), rhs->slice(), irs::string_ref::NIL)));
    EXPECT_TRUE((false == arangodb::iresearch::IResearchLinkHelper::equal(
                              server.server(), rhs->slice(), lhs->slice(), irs::string_ref::NIL)));
  }

  // test equal
  {
    auto lhs = arangodb::velocypack::Parser::fromJson(
        "{ \"view\": \"a/\", \"includeAllFields\": false }");
    auto rhs = arangodb::velocypack::Parser::fromJson(
        "{ \"view\": \"a/bc\", \"includeAllFields\": false }");
    EXPECT_TRUE((true == arangodb::iresearch::IResearchLinkHelper::equal(
                             server.server(), lhs->slice(), rhs->slice(), irs::string_ref::NIL)));
    EXPECT_TRUE((true == arangodb::iresearch::IResearchLinkHelper::equal(
                             server.server(), rhs->slice(), lhs->slice(), irs::string_ref::NIL)));
  }

  // test analyzers with definitions
  {
    auto lhs = arangodb::velocypack::Parser::fromJson(
      "{ \"view\": \"a/\", \"includeAllFields\": false, \"analyzers\":[\"testAnalyzer\", \"mydb::testAnalyzer2\"]," 
      "  \"analyzerDefinitions\":[ "
      "    {\"name\":\"testAnalyzer\", \"type\":\"ngram\", \"properties\":{\"min\":2, \"max\":2, \"preserveOriginal\": false}}, "
      "    {\"name\":\"testAnalyzer2\", \"type\":\"ngram\", \"properties\":{\"min\":3, \"max\":3, \"preserveOriginal\": false}} "
      "  ]}");
    auto rhs = arangodb::velocypack::Parser::fromJson(
      "{ \"view\": \"a/bc\", \"includeAllFields\": false, \"analyzers\":[\"mydb::testAnalyzer\", \"testAnalyzer2\"], "
      "  \"analyzerDefinitions\":[ "
      "    {\"name\":\"testAnalyzer\", \"type\":\"ngram\", \"properties\":{\"min\":2, \"max\":2, \"preserveOriginal\": false}}, "
      "    {\"name\":\"testAnalyzer2\", \"type\":\"ngram\", \"properties\":{\"min\":3, \"max\":3, \"preserveOriginal\": false}} "
      "  ]}");
    EXPECT_TRUE((true == arangodb::iresearch::IResearchLinkHelper::equal(
      server.server(), lhs->slice(), rhs->slice(), "mydb")));
    EXPECT_TRUE((true == arangodb::iresearch::IResearchLinkHelper::equal(
      server.server(), rhs->slice(), lhs->slice(), "mydb")));
  }

  // test analyzers with definitions different order
  {
    auto lhs = arangodb::velocypack::Parser::fromJson(
      "{ \"view\": \"a/\", \"includeAllFields\": false, \"analyzers\":[\"testAnalyzer\", \"mydb::testAnalyzer2\"]," 
      "  \"analyzerDefinitions\":[ "
      "    {\"name\":\"testAnalyzer\", \"type\":\"ngram\", \"properties\":{\"min\":2, \"max\":2, \"preserveOriginal\": false}}, "
      "    {\"name\":\"testAnalyzer2\", \"type\":\"ngram\", \"properties\":{\"min\":3, \"max\":3, \"preserveOriginal\": false}} "
      "  ]}");
    auto rhs = arangodb::velocypack::Parser::fromJson(
      "{ \"view\": \"a/bc\", \"includeAllFields\": false, \"analyzers\":[\"testAnalyzer2\", \"testAnalyzer\"], "
      "  \"analyzerDefinitions\":[ "
      "    {\"name\":\"testAnalyzer\", \"type\":\"ngram\", \"properties\":{\"min\":2, \"max\":2, \"preserveOriginal\": false}}, "
      "    {\"name\":\"testAnalyzer2\", \"type\":\"ngram\", \"properties\":{\"min\":3, \"max\":3, \"preserveOriginal\": false}} "
      "  ]}");
    EXPECT_TRUE((true == arangodb::iresearch::IResearchLinkHelper::equal(
      server.server(), lhs->slice(), rhs->slice(), "mydb")));
    EXPECT_TRUE((true == arangodb::iresearch::IResearchLinkHelper::equal(
      server.server(), rhs->slice(), lhs->slice(), "mydb")));
  }

  // test analyzers with different names
  {
    auto lhs = arangodb::velocypack::Parser::fromJson(
      "{ \"view\": \"a/\", \"includeAllFields\": false, \"analyzers\":[\"testAnalyzer\", \"testAnalyzer2\"]," 
      "  \"analyzerDefinitions\":[ "
      "    {\"name\":\"testAnalyzer\", \"type\":\"ngram\", \"properties\":{\"min\":2, \"max\":2, \"preserveOriginal\": false}}, "
      "    {\"name\":\"testAnalyzer2\", \"type\":\"ngram\", \"properties\":{\"min\":3, \"max\":3, \"preserveOriginal\": false}} "
      "  ]}");
    auto rhs = arangodb::velocypack::Parser::fromJson(
      "{ \"view\": \"a/bc\", \"includeAllFields\": false, \"analyzers\":[\"testAnalyzer\", \"testAnalyzer3\"], "
      "  \"analyzerDefinitions\":[ "
      "    {\"name\":\"testAnalyzer\", \"type\":\"ngram\", \"properties\":{\"min\":2, \"max\":2, \"preserveOriginal\": false}}, "
      "    {\"name\":\"testAnalyzer3\", \"type\":\"ngram\", \"properties\":{\"min\":3, \"max\":3, \"preserveOriginal\": false}} "
      "  ]}");
    EXPECT_FALSE((true == arangodb::iresearch::IResearchLinkHelper::equal(
      server.server(), lhs->slice(), rhs->slice(), "mydb")));
    EXPECT_FALSE((true == arangodb::iresearch::IResearchLinkHelper::equal(
      server.server(), rhs->slice(), lhs->slice(), "mydb")));
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
  TRI_vocbase_t& sysVocbase = server.getSystemDatabase();

  // analyzer single-server, for creation
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \
      \"analyzerDefinitions\": [ { \"name\": \"testAnalyzer0\", \"type\": \"identity\" } ], \
      \"analyzers\": [\"testAnalyzer0\" ], \
      \"storedValues\":[[], [\"\"], [\"test.t\"], [\"a.a\", \"b.b\"]] \
    }");
    arangodb::velocypack::Builder builder;
    builder.openObject();
    EXPECT_TRUE(arangodb::iresearch::IResearchLinkHelper::normalize(
                  builder, json->slice(), true, sysVocbase).ok());
    builder.close();
    EXPECT_EQ(nullptr, analyzers.get(arangodb::StaticStrings::SystemDatabase + "::testAnalyzer0",
                                     arangodb::QueryAnalyzerRevisions::QUERY_LATEST));

    auto expected_json = arangodb::velocypack::Parser::fromJson(
    "{ \
      \"type\":\"arangosearch\", \
      \"primarySort\":[], \
      \"primarySortCompression\":\"lz4\",\
      \"fields\":{}, \
      \"includeAllFields\": false, \
      \"trackListPositions\": false, \
      \"storeValues\": \"none\", \
      \"analyzerDefinitions\": [ \
        { \"name\": \"testAnalyzer0\", \"type\": \"identity\", \"properties\":{}, \"features\":[]} \
      ], \
      \"analyzers\": [\"testAnalyzer0\" ], \
      \"storedValues\":[{\"fields\":[\"test.t\"], \"compression\":\"lz4\"}, {\"fields\":[\"a.a\", \"b.b\"], \"compression\":\"lz4\"}] \
    }");
    EXPECT_EQUAL_SLICES(expected_json->slice(), builder.slice());
  }

  // analyzer single-server, user definition
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \
      \"analyzerDefinitions\": [ { \"name\": \"testAnalyzer0\", \"type\": \"identity\" } ], \
      \"analyzers\": [\"testAnalyzer0\" ] \
    }");
    arangodb::velocypack::Builder builder;
    builder.openObject();
    EXPECT_TRUE(arangodb::iresearch::IResearchLinkHelper::normalize(
                  builder, json->slice(), false, sysVocbase).ok());
    builder.close();
    EXPECT_EQ(nullptr, analyzers.get(arangodb::StaticStrings::SystemDatabase + "::testAnalyzer0",
                                     arangodb::QueryAnalyzerRevisions::QUERY_LATEST));

    auto expected_json = arangodb::velocypack::Parser::fromJson(
    "{ \
      \"type\":\"arangosearch\", \
      \"fields\":{}, \
      \"includeAllFields\": false, \
      \"trackListPositions\": false, \
      \"storeValues\": \"none\", \
      \"analyzers\": [\"testAnalyzer0\" ] \
    }");
    EXPECT_EQUAL_SLICES(expected_json->slice(), builder.slice());
  }

  // analyzer single-server, not for creation, missing "testAanalyzer0"
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \
      \"analyzers\": [\"testAnalyzer0\" ] \
    }");
    arangodb::velocypack::Builder builder;
    builder.openObject();
    EXPECT_FALSE(arangodb::iresearch::IResearchLinkHelper::normalize(
                  builder, json->slice(), false, sysVocbase).ok());
    builder.close();
    EXPECT_EQ(nullptr, analyzers.get(arangodb::StaticStrings::SystemDatabase + "::testAnalyzer0",
                                     arangodb::QueryAnalyzerRevisions::QUERY_LATEST));
  }

  // analyzer single-server, for creation, missing "testAanalyzer0"
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \
      \"analyzers\": [\"testAnalyzer0\" ] \
    }");
    arangodb::velocypack::Builder builder;
    builder.openObject();
    EXPECT_FALSE(arangodb::iresearch::IResearchLinkHelper::normalize(
                  builder, json->slice(), false, sysVocbase).ok());
    builder.close();
    EXPECT_EQ(nullptr, analyzers.get(arangodb::StaticStrings::SystemDatabase + "::testAnalyzer0", 
                                     arangodb::QueryAnalyzerRevisions::QUERY_LATEST));
  }

  // analyzer single-server (inRecovery), for creation
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \
      \"analyzerDefinitions\": [ { \"name\": \"testAnalyzer1\", \"type\": \"identity\" } ], \
      \"analyzers\": [\"testAnalyzer1\" ], \
      \"storedValues\":[[], [\"\"], [\"test.t\"], [\"a.a\", \"b.b\"]] \
    }");
    auto before = StorageEngineMock::recoveryStateResult;
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
    auto restore = irs::make_finally(
        [&before]() -> void { StorageEngineMock::recoveryStateResult = before; });
    arangodb::velocypack::Builder builder;
    builder.openObject();
    EXPECT_TRUE(arangodb::iresearch::IResearchLinkHelper::normalize(
                  builder, json->slice(), true, sysVocbase).ok());
    builder.close();
    EXPECT_EQ(nullptr, analyzers.get(arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1",
                                     arangodb::QueryAnalyzerRevisions::QUERY_LATEST));

    auto expected_json = arangodb::velocypack::Parser::fromJson(
    "{ \
      \"type\":\"arangosearch\", \
      \"primarySort\":[], \
      \"primarySortCompression\":\"lz4\",\
      \"fields\":{}, \
      \"includeAllFields\": false, \
      \"trackListPositions\": false, \
      \"storeValues\": \"none\", \
      \"analyzerDefinitions\": [ \
        { \"name\": \"testAnalyzer1\", \"type\": \"identity\", \"properties\":{}, \"features\":[] } \
      ], \
      \"analyzers\": [\"testAnalyzer1\" ], \
      \"storedValues\":[{\"fields\":[\"test.t\"], \"compression\":\"lz4\"}, {\"fields\":[\"a.a\", \"b.b\"], \"compression\":\"lz4\"}] \
    }");
    EXPECT_EQUAL_SLICES(expected_json->slice(), builder.slice());
  }

  // analyzer single-server (inRecovery), not for creation
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
    EXPECT_TRUE(arangodb::iresearch::IResearchLinkHelper::normalize(
                  builder, json->slice(), false, sysVocbase).ok());
    builder.close();
    EXPECT_EQ(nullptr, analyzers.get(arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1",
                                     arangodb::QueryAnalyzerRevisions::QUERY_LATEST));

    auto expected_json = arangodb::velocypack::Parser::fromJson(
    "{ \
      \"type\":\"arangosearch\", \
      \"fields\":{}, \
      \"includeAllFields\": false, \
      \"trackListPositions\": false, \
      \"storeValues\": \"none\", \
      \"analyzers\": [\"testAnalyzer1\" ] \
    }");
    EXPECT_EQUAL_SLICES(expected_json->slice(), builder.slice());
  }
  // analyzer single-server (inRecovery), for creation with specified compression
  {
    auto json = arangodb::velocypack::Parser::fromJson(
      "{ \
      \"analyzerDefinitions\": [ { \"name\": \"testAnalyzer1\", \"type\": \"identity\" } ], \
      \"analyzers\": [\"testAnalyzer1\" ], \
      \"storedValues\":[[], [\"\"], {\"fields\":[\"test.t\"], \"compression\":\"lz4\",\
      \"some_unknown\":1}, {\"fields\":[\"a.a\", \"b.b\"], \"compression\":\"none\"}] \
    }");
    auto before = StorageEngineMock::recoveryStateResult;
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
    auto restore = irs::make_finally(
      [&before]() -> void { StorageEngineMock::recoveryStateResult = before; });
    arangodb::velocypack::Builder builder;
    builder.openObject();
    EXPECT_TRUE(arangodb::iresearch::IResearchLinkHelper::normalize(
      builder, json->slice(), true, sysVocbase).ok());
    builder.close();
    EXPECT_EQ(nullptr, analyzers.get(arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1", 
                                     arangodb::QueryAnalyzerRevisions::QUERY_LATEST));

    auto expected_json = arangodb::velocypack::Parser::fromJson(
      "{ \
      \"type\":\"arangosearch\", \
      \"primarySort\":[], \
      \"primarySortCompression\":\"lz4\",\
      \"fields\":{}, \
      \"includeAllFields\": false, \
      \"trackListPositions\": false, \
      \"storeValues\": \"none\", \
      \"analyzerDefinitions\": [ \
        { \"name\": \"testAnalyzer1\", \"type\": \"identity\", \"properties\":{}, \"features\":[]} \
      ], \
      \"analyzers\": [\"testAnalyzer1\" ], \
      \"storedValues\":[{\"fields\":[\"test.t\"], \"compression\":\"lz4\"}, {\"fields\":[\"a.a\", \"b.b\"], \"compression\":\"none\"}] \
    }");
    EXPECT_EQUAL_SLICES(expected_json->slice(), builder.slice());
  }
  // with primary sort
  {
    auto json = arangodb::velocypack::Parser::fromJson(
      "{ \
      \"analyzerDefinitions\": [ { \"name\": \"testAnalyzer1\", \"type\": \"identity\" } ], \
      \"analyzers\": [\"testAnalyzer1\" ], \
      \"storedValues\":[[], [\"\"], {\"fields\":[\"test.t\"], \"compression\":\"lz4\",\
      \"some_unknown\":1}, {\"fields\":[\"a.a\", \"b.b\"], \"compression\":\"none\"}] \
    }");
    auto before = StorageEngineMock::recoveryStateResult;
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
    auto restore = irs::make_finally(
      [&before]() -> void { StorageEngineMock::recoveryStateResult = before; });
    arangodb::velocypack::Builder builder;
    builder.openObject();
    arangodb::iresearch::IResearchViewSort sort;
    sort.emplace_back({ arangodb::basics::AttributeName(std::string("abc"), false) }, false);
    EXPECT_TRUE(arangodb::iresearch::IResearchLinkHelper::normalize(
      builder, json->slice(), true, sysVocbase, &sort).ok());
    builder.close();
    EXPECT_EQ(nullptr, analyzers.get(arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1", 
      arangodb::QueryAnalyzerRevisions::QUERY_LATEST));

    auto expected_json = arangodb::velocypack::Parser::fromJson(
      "{ \
      \"type\":\"arangosearch\", \
      \"primarySort\":[{\"field\":\"abc\", \"asc\": false}], \
      \"primarySortCompression\":\"lz4\",\
      \"fields\":{}, \
      \"includeAllFields\": false, \
      \"trackListPositions\": false, \
      \"storeValues\": \"none\", \
      \"analyzerDefinitions\": [ \
        { \"name\": \"testAnalyzer1\", \"type\": \"identity\", \"properties\":{}, \"features\":[]} \
      ], \
      \"analyzers\": [\"testAnalyzer1\" ], \
      \"storedValues\":[{\"fields\":[\"test.t\"], \"compression\":\"lz4\"}, {\"fields\":[\"a.a\", \"b.b\"], \"compression\":\"none\"}] \
    }");
    EXPECT_EQUAL_SLICES(expected_json->slice(), builder.slice());
  }
  // with primary sort and custom primary compression
  {
    auto json = arangodb::velocypack::Parser::fromJson(
      "{ \
      \"analyzerDefinitions\": [ { \"name\": \"testAnalyzer1\", \"type\": \"identity\" } ], \
      \"analyzers\": [\"testAnalyzer1\" ], \
      \"storedValues\":[[], [\"\"], {\"fields\":[\"test.t\"], \"compression\":\"lz4\",\
      \"some_unknown\":1}, {\"fields\":[\"a.a\", \"b.b\"], \"compression\":\"none\"}] \
    }");
    auto before = StorageEngineMock::recoveryStateResult;
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
    auto restore = irs::make_finally(
      [&before]() -> void { StorageEngineMock::recoveryStateResult = before; });
    arangodb::velocypack::Builder builder;
    builder.openObject();
    arangodb::iresearch::IResearchViewSort sort;
    sort.emplace_back({ arangodb::basics::AttributeName(std::string("abc"), false) }, true);
    auto compression = irs::type<irs::compression::none>::id();
    EXPECT_TRUE(arangodb::iresearch::IResearchLinkHelper::normalize(
      builder, json->slice(), true, sysVocbase, &sort, &compression).ok());
    builder.close();
    EXPECT_EQ(nullptr, analyzers.get(arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1", 
      arangodb::QueryAnalyzerRevisions::QUERY_LATEST));

    auto expected_json = arangodb::velocypack::Parser::fromJson(
      "{ \
      \"type\":\"arangosearch\", \
      \"primarySort\":[{\"field\":\"abc\", \"asc\": true}], \
      \"primarySortCompression\":\"none\",\
      \"fields\":{}, \
      \"includeAllFields\": false, \
      \"trackListPositions\": false, \
      \"storeValues\": \"none\", \
      \"analyzerDefinitions\": [ \
        { \"name\": \"testAnalyzer1\", \"type\": \"identity\", \"properties\":{}, \"features\":[]} \
      ], \
      \"analyzers\": [\"testAnalyzer1\" ], \
      \"storedValues\":[{\"fields\":[\"test.t\"], \"compression\":\"lz4\"}, {\"fields\":[\"a.a\", \"b.b\"], \"compression\":\"none\"}] \
    }");
    EXPECT_EQUAL_SLICES(expected_json->slice(), builder.slice());
  }
} 

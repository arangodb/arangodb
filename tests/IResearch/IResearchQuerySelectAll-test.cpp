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

#include "common.h"
#include "gtest/gtest.h"

#include "../Mocks/StorageEngineMock.h"

#if USE_ENTERPRISE
#include "Enterprise/Ldap/LdapFeature.h"
#endif

#include "3rdParty/iresearch/tests/tests_config.hpp"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/Ast.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Aql/Query.h"
#include "Basics/VelocyPackHelper.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchFilterFactory.h"
#include "IResearch/IResearchView.h"
#include "Logger/LogTopic.h"
#include "Logger/Logger.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/TraverserEngineRegistryFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "Sharding/ShardingFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "V8/v8-globals.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "VocBase/ManagedDocumentResult.h"

#include "IResearch/VelocyPackHelper.h"
#include "analysis/analyzers.hpp"
#include "analysis/token_attributes.hpp"
#include "utils/utf8_path.hpp"

#include <velocypack/Iterator.h>

extern const char* ARGV0;  // defined in main.cpp

namespace {

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class IResearchQuerySelectAllTest : public ::testing::Test {
 protected:
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::unique_ptr<TRI_vocbase_t> system;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  IResearchQuerySelectAllTest() : engine(server), server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

    arangodb::tests::init(true);

    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    // suppress WARNING {authentication} --server.jwt-secret is insecure. Use --server.jwt-secret-keyfile instead
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(),
                                    arangodb::LogLevel::ERR);

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(), arangodb::LogLevel::ERR);  // suppress WARNING DefaultCustomTypeHandler called
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(),
                                    arangodb::LogLevel::FATAL);
    irs::logger::output_le(iresearch::logger::IRL_FATAL, stderr);

    // setup required application features
    features.emplace_back(new arangodb::ViewTypesFeature(server), true);
    features.emplace_back(new arangodb::AuthenticationFeature(server), true);
    features.emplace_back(new arangodb::DatabasePathFeature(server), false);
    features.emplace_back(new arangodb::DatabaseFeature(server), false);
    features.emplace_back(new arangodb::QueryRegistryFeature(server), false);  // must be first
    arangodb::application_features::ApplicationServer::server->addFeature(
        features.back().first);  // need QueryRegistryFeature feature to be added now in order to create the system database
    system = irs::memory::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                                                     0, TRI_VOC_SYSTEM_DATABASE);
    features.emplace_back(new arangodb::SystemDatabaseFeature(server, system.get()),
                          false);  // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::TraverserEngineRegistryFeature(server), false);  // must be before AqlFeature
    features.emplace_back(new arangodb::AqlFeature(server), true);
    features.emplace_back(new arangodb::aql::OptimizerRulesFeature(server), true);
    features.emplace_back(new arangodb::aql::AqlFunctionFeature(server), true);  // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::ShardingFeature(server), true);
    features.emplace_back(new arangodb::iresearch::IResearchAnalyzerFeature(server), true);
    features.emplace_back(new arangodb::iresearch::IResearchFeature(server), true);

#if USE_ENTERPRISE
    features.emplace_back(new arangodb::LdapFeature(server),
                          false);  // required for AuthenticationFeature with USE_ENTERPRISE
#endif

    for (auto& f : features) {
      arangodb::application_features::ApplicationServer::server->addFeature(f.first);
    }

    for (auto& f : features) {
      f.first->prepare();
    }

    for (auto& f : features) {
      if (f.second) {
        f.first->start();
      }
    }

    auto* dbPathFeature =
        arangodb::application_features::ApplicationServer::getFeature<arangodb::DatabasePathFeature>(
            "DatabasePath");
    arangodb::tests::setDatabasePath(*dbPathFeature);  // ensure test data is stored in a unique directory
  }

  ~IResearchQuerySelectAllTest() {
    system.reset();  // destroy before reseting the 'ENGINE'
    arangodb::AqlFeature(server).stop();  // unset singleton instance
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(),
                                    arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(),
                                    arangodb::LogLevel::DEFAULT);
    arangodb::application_features::ApplicationServer::server = nullptr;
    arangodb::EngineSelectorFeature::ENGINE = nullptr;

    // destroy application features
    for (auto& f : features) {
      if (f.second) {
        f.first->stop();
      }
    }

    for (auto& f : features) {
      f.first->unprepare();
    }

    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(),
                                    arangodb::LogLevel::DEFAULT);
  }
};  // IResearchQuerySetup

}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(IResearchQuerySelectAllTest, test) {
  static std::vector<std::string> const EMPTY;

  auto createJson = arangodb::velocypack::Parser::fromJson(
      "{ \
    \"name\": \"testView\", \
    \"type\": \"arangosearch\" \
  }");

  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                        "testVocbase");
  std::shared_ptr<arangodb::LogicalCollection> logicalCollection1;
  std::shared_ptr<arangodb::LogicalCollection> logicalCollection2;

  // add collection_1
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"collection_1\" }");
    logicalCollection1 = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection1));
  }

  // add collection_2
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"collection_2\" }");
    logicalCollection2 = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection2));
  }

  // add view
  auto view = std::dynamic_pointer_cast<arangodb::iresearch::IResearchView>(
      vocbase.createView(createJson->slice()));
  ASSERT_TRUE((false == !view));

  // add link to collection
  {
    auto updateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\" : {"
        "\"collection_1\" : { \"includeAllFields\" : true },"
        "\"collection_2\" : { \"includeAllFields\" : true }"
        "}}");
    EXPECT_TRUE((view->properties(updateJson->slice(), true).ok()));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->properties(builder, true, false);
    builder.close();

    auto slice = builder.slice();
    EXPECT_TRUE(slice.isObject());
    EXPECT_TRUE(slice.get("name").copyString() == "testView");
    EXPECT_TRUE(slice.get("type").copyString() ==
                arangodb::iresearch::DATA_SOURCE_TYPE.name());
    EXPECT_TRUE(slice.get("deleted").isNone());  // no system properties
    auto tmpSlice = slice.get("links");
    EXPECT_TRUE((true == tmpSlice.isObject() && 2 == tmpSlice.length()));
  }

  std::vector<arangodb::ManagedDocumentResult> insertedDocs(2 * 42);

  // populate view with the data
  {
    arangodb::OperationOptions opt;

    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                       EMPTY, EMPTY, EMPTY,
                                       arangodb::transaction::Options());
    EXPECT_TRUE((trx.begin().ok()));

    size_t i = 0;

    // insert into collection_1
    for (; i < insertedDocs.size() / 2; ++i) {
      auto const doc = arangodb::velocypack::Parser::fromJson(
          "{ \"key\": " + std::to_string(i) + "}");
      auto const res =
          logicalCollection1->insert(&trx, doc->slice(), insertedDocs[i], opt, false);
      EXPECT_TRUE(res.ok());
    }

    // insert into collection_2
    for (; i < insertedDocs.size(); ++i) {
      auto const doc = arangodb::velocypack::Parser::fromJson(
          "{ \"key\": " + std::to_string(i) + "}");
      auto const res =
          logicalCollection1->insert(&trx, doc->slice(), insertedDocs[i], opt, false);
      EXPECT_TRUE(res.ok());
    }

    EXPECT_TRUE((trx.commit().ok()));
    EXPECT_TRUE(
        (arangodb::tests::executeQuery(vocbase,
                                       "FOR d IN testView SEARCH 1 ==1 OPTIONS "
                                       "{ waitForSync: true } RETURN d")
             .result.ok()));  // commit
  }

  // unordered
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto keySlice = docSlice.get("key");
      expectedDocs.emplace(keySlice.getNumber<size_t>(), &doc);
    }

    auto queryResult =
        arangodb::tests::executeQuery(vocbase, "FOR d IN testView RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    for (auto const actualDoc : arangodb::velocypack::ArrayIterator(result)) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("key");
      auto const key = keySlice.getNumber<size_t>();

      auto expectedDoc = expectedDocs.find(key);
      ASSERT_TRUE(expectedDoc != expectedDocs.end());
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice(expectedDoc->second->vpack()),
                           resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    EXPECT_TRUE(expectedDocs.empty());
  }

  // key ASC
  {
    auto const& expectedDocs = insertedDocs;

    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SORT d.key ASC RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    auto expectedDoc = expectedDocs.begin();
    for (auto const actualDoc : arangodb::velocypack::ArrayIterator(result)) {
      auto const resolved = actualDoc.resolveExternals();
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice(expectedDoc->vpack()), resolved, true));
      ++expectedDoc;
    }
    EXPECT_TRUE(expectedDoc == expectedDocs.end());
  }

  // key DESC
  {
    auto const& expectedDocs = insertedDocs;

    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SORT d.key DESC RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : arangodb::velocypack::ArrayIterator(result)) {
      auto const resolved = actualDoc.resolveExternals();
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice(expectedDoc->vpack()), resolved, true));
      ++expectedDoc;
    }
    EXPECT_TRUE(expectedDoc == expectedDocs.rend());
  }

  // TFIDF() ASC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto keySlice = docSlice.get("key");
      expectedDocs.emplace(keySlice.getNumber<size_t>(), &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SORT TFIDF(d) RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    for (auto const actualDoc : arangodb::velocypack::ArrayIterator(result)) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("key");
      auto const key = keySlice.getNumber<size_t>();

      auto expectedDoc = expectedDocs.find(key);
      ASSERT_TRUE(expectedDoc != expectedDocs.end());
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice(expectedDoc->second->vpack()),
                           resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    EXPECT_TRUE(expectedDocs.empty());
  }

  // TFIDF() DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto keySlice = docSlice.get("key");
      expectedDocs.emplace(keySlice.getNumber<size_t>(), &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SORT TFIDF(d) DESC RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    for (auto const actualDoc : arangodb::velocypack::ArrayIterator(result)) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("key");
      auto const key = keySlice.getNumber<size_t>();

      auto expectedDoc = expectedDocs.find(key);
      ASSERT_TRUE(expectedDoc != expectedDocs.end());
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice(expectedDoc->second->vpack()),
                           resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    EXPECT_TRUE(expectedDocs.empty());
  }

  // BM25() ASC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto keySlice = docSlice.get("key");
      expectedDocs.emplace(keySlice.getNumber<size_t>(), &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SORT BM25(d) RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    for (auto const actualDoc : arangodb::velocypack::ArrayIterator(result)) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("key");
      auto const key = keySlice.getNumber<size_t>();

      auto expectedDoc = expectedDocs.find(key);
      ASSERT_TRUE(expectedDoc != expectedDocs.end());
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice(expectedDoc->second->vpack()),
                           resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    EXPECT_TRUE(expectedDocs.empty());
  }

  // BM25() DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto keySlice = docSlice.get("key");
      expectedDocs.emplace(keySlice.getNumber<size_t>(), &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SORT BM25(d) DESC RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    for (auto const actualDoc : arangodb::velocypack::ArrayIterator(result)) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("key");
      auto const key = keySlice.getNumber<size_t>();

      auto expectedDoc = expectedDocs.find(key);
      ASSERT_TRUE(expectedDoc != expectedDocs.end());
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice(expectedDoc->second->vpack()),
                           resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    EXPECT_TRUE(expectedDocs.empty());
  }

  // TFIDF() ASC, key ASC
  {
    auto const& expectedDocs = insertedDocs;

    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SORT TFIDF(d), d.key ASC RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    auto expectedDoc = expectedDocs.begin();
    for (auto const actualDoc : arangodb::velocypack::ArrayIterator(result)) {
      auto const resolved = actualDoc.resolveExternals();
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice(expectedDoc->vpack()), resolved, true));
      ++expectedDoc;
    }
    EXPECT_TRUE(expectedDoc == expectedDocs.end());
  }

  // TFIDF ASC, key DESC
  {
    auto const& expectedDocs = insertedDocs;

    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SORT TFIDF(d), d.key DESC RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : arangodb::velocypack::ArrayIterator(result)) {
      auto const resolved = actualDoc.resolveExternals();
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice(expectedDoc->vpack()), resolved, true));
      ++expectedDoc;
    }
    EXPECT_TRUE(expectedDoc == expectedDocs.rend());
  }

  // check full stats with optimization
  {
    auto const queryString =
        "FOR d IN testView SORT BM25(d), d.key DESC LIMIT 10, 10 RETURN d";
    auto const& expectedDocs = insertedDocs;

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase, queryString,
                                             {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
                                              arangodb::aql::OptimizerRule::applySortLimitRule}));

    auto queryResult = arangodb::tests::executeQuery(
        vocbase, queryString, {},
        //"{ \"fullCount\": true }" // FIXME uncomment
        "{ \"optimizer\" : { \"rules\": [ \"-sort-limit\"] }, \"fullCount\": "
        "true }");
    ASSERT_TRUE(queryResult.result.ok());

    auto root = queryResult.extra->slice();
    ASSERT_TRUE(root.isObject());
    auto stats = root.get("stats");
    ASSERT_TRUE(stats.isObject());
    auto fullCountSlice = stats.get("fullCount");
    ASSERT_TRUE(fullCountSlice.isNumber());
    EXPECT_TRUE(insertedDocs.size() == fullCountSlice.getNumber<size_t>());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    auto expectedDoc = expectedDocs.rbegin() + 10;
    for (auto const actualDoc : arangodb::velocypack::ArrayIterator(result)) {
      auto const resolved = actualDoc.resolveExternals();
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice(expectedDoc->vpack()), resolved, true));
      ++expectedDoc;
    }
    EXPECT_TRUE(expectedDoc == expectedDocs.rbegin() + 20);
  }

  // check full stats without optimization
  {
    auto const queryString =
        "FOR d IN testView SORT BM25(d), d.key DESC LIMIT 10, 10 RETURN d";
    auto const& expectedDocs = insertedDocs;

    auto queryResult = arangodb::tests::executeQuery(
        vocbase, queryString, {},
        "{ \"optimizer\" : { \"rules\": [ \"-sort-limit\"] }, \"fullCount\": "
        "true }");
    ASSERT_TRUE(queryResult.result.ok());

    auto root = queryResult.extra->slice();
    ASSERT_TRUE(root.isObject());
    auto stats = root.get("stats");
    ASSERT_TRUE(stats.isObject());
    auto fullCountSlice = stats.get("fullCount");
    ASSERT_TRUE(fullCountSlice.isNumber());
    EXPECT_TRUE(insertedDocs.size() == fullCountSlice.getNumber<size_t>());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    auto expectedDoc = expectedDocs.rbegin() + 10;
    for (auto const actualDoc : arangodb::velocypack::ArrayIterator(result)) {
      auto const resolved = actualDoc.resolveExternals();
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice(expectedDoc->vpack()), resolved, true));
      ++expectedDoc;
    }
    EXPECT_TRUE(expectedDoc == expectedDocs.rbegin() + 20);
  }
}

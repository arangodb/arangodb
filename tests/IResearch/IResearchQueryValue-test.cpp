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
#include "RestServer/FlushFeature.h"
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

#include "IResearch/VelocyPackHelper.h"
#include "analysis/analyzers.hpp"
#include "analysis/token_attributes.hpp"
#include "utils/utf8_path.hpp"

#include <velocypack/Iterator.h>

extern const char* ARGV0;  // defined in main.cpp

namespace {

static const VPackBuilder systemDatabaseBuilder = dbArgsBuilder();
static const VPackSlice   systemDatabaseArgs = systemDatabaseBuilder.slice();
static const VPackBuilder testDatabaseBuilder = dbArgsBuilder("testVocbase");
static const VPackSlice   testDatabaseArgs = testDatabaseBuilder.slice();
// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class IResearchQueryValueTest : public ::testing::Test {
 protected:
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::unique_ptr<TRI_vocbase_t> system;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  IResearchQueryValueTest() : engine(server), server(nullptr, nullptr) {
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
    features.emplace_back(new arangodb::FlushFeature(server), false);
    features.emplace_back(new arangodb::ViewTypesFeature(server), true);
    features.emplace_back(new arangodb::AuthenticationFeature(server), true);
    features.emplace_back(new arangodb::DatabasePathFeature(server), false);
    features.emplace_back(new arangodb::DatabaseFeature(server), false);
    features.emplace_back(new arangodb::ShardingFeature(server), false);
    features.emplace_back(new arangodb::QueryRegistryFeature(server), false);  // must be first
    arangodb::application_features::ApplicationServer::server->addFeature(
        features.back().first);  // need QueryRegistryFeature feature to be added now in order to create the system database
    system = irs::memory::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                                                     0, systemDatabaseArgs);
    features.emplace_back(new arangodb::SystemDatabaseFeature(server, system.get()),
                          false);  // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::TraverserEngineRegistryFeature(server), false);  // must be before AqlFeature
    features.emplace_back(new arangodb::AqlFeature(server), true);
    features.emplace_back(new arangodb::aql::OptimizerRulesFeature(server), true);
    features.emplace_back(new arangodb::aql::AqlFunctionFeature(server), true);  // required for IResearchAnalyzerFeature
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

  ~IResearchQueryValueTest() {
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

TEST_F(IResearchQueryValueTest, test) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                        testDatabaseArgs);
  std::vector<arangodb::velocypack::Builder> insertedDocs;
  arangodb::LogicalView* view;

  // create collection0
  {
    auto createJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection0\" }");
    auto collection = vocbase.createCollection(createJson->slice());
    ASSERT_TRUE((nullptr != collection));

    std::vector<std::shared_ptr<arangodb::velocypack::Builder>> docs{
        arangodb::velocypack::Parser::fromJson(
            "{ \"seq\": -6, \"value\": null }"),
        arangodb::velocypack::Parser::fromJson(
            "{ \"seq\": -5, \"value\": true }"),
        arangodb::velocypack::Parser::fromJson(
            "{ \"seq\": -4, \"value\": \"abc\" }"),
        arangodb::velocypack::Parser::fromJson(
            "{ \"seq\": -3, \"value\": 3.14 }"),
        arangodb::velocypack::Parser::fromJson(
            "{ \"seq\": -2, \"value\": [ 1, \"abc\" ] }"),
        arangodb::velocypack::Parser::fromJson(
            "{ \"seq\": -1, \"value\": { \"a\": 7, \"b\": \"c\" } }"),
    };

    arangodb::OperationOptions options;
    options.returnNew = true;
    arangodb::SingleCollectionTransaction trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                              *collection,
                                              arangodb::AccessMode::Type::WRITE);
    EXPECT_TRUE((trx.begin().ok()));

    for (auto& entry : docs) {
      auto res = trx.insert(collection->name(), entry->slice(), options);
      EXPECT_TRUE((res.ok()));
      insertedDocs.emplace_back(res.slice().get("new"));
    }

    EXPECT_TRUE((trx.commit().ok()));
  }

  // create collection1
  {
    auto createJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection1\" }");
    auto collection = vocbase.createCollection(createJson->slice());
    ASSERT_TRUE((nullptr != collection));

    irs::utf8_path resource;
    resource /= irs::string_ref(arangodb::tests::testResourceDir);
    resource /= irs::string_ref("simple_sequential.json");

    auto builder =
        arangodb::basics::VelocyPackHelper::velocyPackFromFile(resource.utf8());
    auto slice = builder.slice();
    ASSERT_TRUE(slice.isArray());

    arangodb::OperationOptions options;
    options.returnNew = true;
    arangodb::SingleCollectionTransaction trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                              *collection,
                                              arangodb::AccessMode::Type::WRITE);
    EXPECT_TRUE((trx.begin().ok()));

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto res = trx.insert(collection->name(), itr.value(), options);
      EXPECT_TRUE((res.ok()));
      insertedDocs.emplace_back(res.slice().get("new"));
    }

    EXPECT_TRUE((trx.commit().ok()));
  }

  // create view
  {
    auto createJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto logicalView = vocbase.createView(createJson->slice());
    ASSERT_TRUE((false == !logicalView));

    view = logicalView.get();
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(view);
    ASSERT_TRUE((false == !impl));

    auto updateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": {"
        "\"testCollection0\": { \"includeAllFields\": true, "
        "\"trackListPositions\": true },"
        "\"testCollection1\": { \"includeAllFields\": true }"
        "}}");
    EXPECT_TRUE((impl->properties(updateJson->slice(), true).ok()));
    std::set<TRI_voc_cid_t> cids;
    impl->visitCollections([&cids](TRI_voc_cid_t cid) -> bool {
      cids.emplace(cid);
      return true;
    });
    EXPECT_TRUE((2 == cids.size()));
    EXPECT_TRUE(
        (arangodb::tests::executeQuery(vocbase,
                                       "FOR d IN testView SEARCH 1 ==1 OPTIONS "
                                       "{ waitForSync: true } RETURN d")
             .result.ok()));  // commit
  }

  // test empty array (true)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[0].slice(),  insertedDocs[1].slice(),
        insertedDocs[2].slice(),  insertedDocs[3].slice(),
        insertedDocs[4].slice(),  insertedDocs[5].slice(),
        insertedDocs[6].slice(),  insertedDocs[7].slice(),
        insertedDocs[8].slice(),  insertedDocs[9].slice(),
        insertedDocs[10].slice(), insertedDocs[11].slice(),
        insertedDocs[12].slice(), insertedDocs[13].slice(),
        insertedDocs[14].slice(), insertedDocs[15].slice(),
        insertedDocs[16].slice(), insertedDocs[17].slice(),
        insertedDocs[18].slice(), insertedDocs[19].slice(),
        insertedDocs[20].slice(), insertedDocs[21].slice(),
        insertedDocs[22].slice(), insertedDocs[23].slice(),
        insertedDocs[24].slice(), insertedDocs[25].slice(),
        insertedDocs[26].slice(), insertedDocs[27].slice(),
        insertedDocs[28].slice(), insertedDocs[29].slice(),
        insertedDocs[30].slice(), insertedDocs[31].slice(),
        insertedDocs[32].slice(), insertedDocs[33].slice(),
        insertedDocs[34].slice(), insertedDocs[35].slice(),
        insertedDocs[36].slice(), insertedDocs[37].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH [ ] SORT BM25(d) ASC, TFIDF(d) DESC, d.seq "
        "RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      EXPECT_TRUE((i < expected.size()));
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_TRUE((i == expected.size()));
  }

  // test non-empty array (true)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[0].slice(),  insertedDocs[1].slice(),
        insertedDocs[2].slice(),  insertedDocs[3].slice(),
        insertedDocs[4].slice(),  insertedDocs[5].slice(),
        insertedDocs[6].slice(),  insertedDocs[7].slice(),
        insertedDocs[8].slice(),  insertedDocs[9].slice(),
        insertedDocs[10].slice(), insertedDocs[11].slice(),
        insertedDocs[12].slice(), insertedDocs[13].slice(),
        insertedDocs[14].slice(), insertedDocs[15].slice(),
        insertedDocs[16].slice(), insertedDocs[17].slice(),
        insertedDocs[18].slice(), insertedDocs[19].slice(),
        insertedDocs[20].slice(), insertedDocs[21].slice(),
        insertedDocs[22].slice(), insertedDocs[23].slice(),
        insertedDocs[24].slice(), insertedDocs[25].slice(),
        insertedDocs[26].slice(), insertedDocs[27].slice(),
        insertedDocs[28].slice(), insertedDocs[29].slice(),
        insertedDocs[30].slice(), insertedDocs[31].slice(),
        insertedDocs[32].slice(), insertedDocs[33].slice(),
        insertedDocs[34].slice(), insertedDocs[35].slice(),
        insertedDocs[36].slice(), insertedDocs[37].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH [ 'abc', 'def' ] SORT BM25(d) ASC, TFIDF(d) "
        "DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      EXPECT_TRUE((i < expected.size()));
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_TRUE((i == expected.size()));
  }

  // test numeric range (true)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[0].slice(),  insertedDocs[1].slice(),
        insertedDocs[2].slice(),  insertedDocs[3].slice(),
        insertedDocs[4].slice(),  insertedDocs[5].slice(),
        insertedDocs[6].slice(),  insertedDocs[7].slice(),
        insertedDocs[8].slice(),  insertedDocs[9].slice(),
        insertedDocs[10].slice(), insertedDocs[11].slice(),
        insertedDocs[12].slice(), insertedDocs[13].slice(),
        insertedDocs[14].slice(), insertedDocs[15].slice(),
        insertedDocs[16].slice(), insertedDocs[17].slice(),
        insertedDocs[18].slice(), insertedDocs[19].slice(),
        insertedDocs[20].slice(), insertedDocs[21].slice(),
        insertedDocs[22].slice(), insertedDocs[23].slice(),
        insertedDocs[24].slice(), insertedDocs[25].slice(),
        insertedDocs[26].slice(), insertedDocs[27].slice(),
        insertedDocs[28].slice(), insertedDocs[29].slice(),
        insertedDocs[30].slice(), insertedDocs[31].slice(),
        insertedDocs[32].slice(), insertedDocs[33].slice(),
        insertedDocs[34].slice(), insertedDocs[35].slice(),
        insertedDocs[36].slice(), insertedDocs[37].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH [ 1 .. 42 ] SORT BM25(d) ASC, TFIDF(d) DESC, "
        "d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      EXPECT_TRUE((i < expected.size()));
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_TRUE((i == expected.size()));
  }

  // test boolean (false)
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH false SORT BM25(d) ASC, TFIDF(d) DESC, d.seq "
        "RETURN d");
    ASSERT_TRUE(queryResult.result.ok());
    auto slice = queryResult.data->slice();
    EXPECT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      EXPECT_TRUE((i < expected.size()));
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_TRUE((i == expected.size()));
  }

  // test boolean (true)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[0].slice(),  insertedDocs[1].slice(),
        insertedDocs[2].slice(),  insertedDocs[3].slice(),
        insertedDocs[4].slice(),  insertedDocs[5].slice(),
        insertedDocs[6].slice(),  insertedDocs[7].slice(),
        insertedDocs[8].slice(),  insertedDocs[9].slice(),
        insertedDocs[10].slice(), insertedDocs[11].slice(),
        insertedDocs[12].slice(), insertedDocs[13].slice(),
        insertedDocs[14].slice(), insertedDocs[15].slice(),
        insertedDocs[16].slice(), insertedDocs[17].slice(),
        insertedDocs[18].slice(), insertedDocs[19].slice(),
        insertedDocs[20].slice(), insertedDocs[21].slice(),
        insertedDocs[22].slice(), insertedDocs[23].slice(),
        insertedDocs[24].slice(), insertedDocs[25].slice(),
        insertedDocs[26].slice(), insertedDocs[27].slice(),
        insertedDocs[28].slice(), insertedDocs[29].slice(),
        insertedDocs[30].slice(), insertedDocs[31].slice(),
        insertedDocs[32].slice(), insertedDocs[33].slice(),
        insertedDocs[34].slice(), insertedDocs[35].slice(),
        insertedDocs[36].slice(), insertedDocs[37].slice(),
    };
    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH true SORT BM25(d) ASC, TFIDF(d) DESC, d.seq "
        "RETURN d");
    ASSERT_TRUE(queryResult.result.ok());
    auto slice = queryResult.data->slice();
    EXPECT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      EXPECT_TRUE((i < expected.size()));
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_TRUE((i == expected.size()));
  }

  // test numeric (false)
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto queryResult =
        arangodb::tests::executeQuery(vocbase,
                                      "FOR d IN testView SEARCH 0 SORT BM25(d) "
                                      "ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(queryResult.result.ok());
    auto slice = queryResult.data->slice();
    EXPECT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      EXPECT_TRUE((i < expected.size()));
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_TRUE((i == expected.size()));
  }

  // test numeric (true)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[0].slice(),  insertedDocs[1].slice(),
        insertedDocs[2].slice(),  insertedDocs[3].slice(),
        insertedDocs[4].slice(),  insertedDocs[5].slice(),
        insertedDocs[6].slice(),  insertedDocs[7].slice(),
        insertedDocs[8].slice(),  insertedDocs[9].slice(),
        insertedDocs[10].slice(), insertedDocs[11].slice(),
        insertedDocs[12].slice(), insertedDocs[13].slice(),
        insertedDocs[14].slice(), insertedDocs[15].slice(),
        insertedDocs[16].slice(), insertedDocs[17].slice(),
        insertedDocs[18].slice(), insertedDocs[19].slice(),
        insertedDocs[20].slice(), insertedDocs[21].slice(),
        insertedDocs[22].slice(), insertedDocs[23].slice(),
        insertedDocs[24].slice(), insertedDocs[25].slice(),
        insertedDocs[26].slice(), insertedDocs[27].slice(),
        insertedDocs[28].slice(), insertedDocs[29].slice(),
        insertedDocs[30].slice(), insertedDocs[31].slice(),
        insertedDocs[32].slice(), insertedDocs[33].slice(),
        insertedDocs[34].slice(), insertedDocs[35].slice(),
        insertedDocs[36].slice(), insertedDocs[37].slice(),
    };
    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH 3.14 SORT BM25(d) ASC, TFIDF(d) DESC, d.seq "
        "RETURN d");
    ASSERT_TRUE(queryResult.result.ok());
    auto slice = queryResult.data->slice();
    EXPECT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      EXPECT_TRUE((i < expected.size()));
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_TRUE((i == expected.size()));
  }

  // test null
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH null SORT BM25(d) ASC, TFIDF(d) DESC, d.seq "
        "RETURN d");
    ASSERT_TRUE(queryResult.result.ok());
    auto slice = queryResult.data->slice();
    EXPECT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      EXPECT_TRUE((i < expected.size()));
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_TRUE((i == expected.size()));
  }

  // test null via bind parameter
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH @param SORT BM25(d) ASC, TFIDF(d) DESC, "
        "d.seq RETURN d",
        arangodb::velocypack::Parser::fromJson("{ \"param\" : null }"));
    ASSERT_TRUE(queryResult.result.ok());
    auto slice = queryResult.data->slice();
    EXPECT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      EXPECT_TRUE((i < expected.size()));
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_TRUE((i == expected.size()));
  }

  // test null via bind parameter
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH 1 - @param SORT BM25(d) ASC, TFIDF(d) DESC, "
        "d.seq RETURN d",
        arangodb::velocypack::Parser::fromJson("{ \"param\" : 1 }"));
    ASSERT_TRUE(queryResult.result.ok());
    auto slice = queryResult.data->slice();
    EXPECT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      EXPECT_TRUE((i < expected.size()));
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_TRUE((i == expected.size()));
  }

  // test empty object (true)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[0].slice(),  insertedDocs[1].slice(),
        insertedDocs[2].slice(),  insertedDocs[3].slice(),
        insertedDocs[4].slice(),  insertedDocs[5].slice(),
        insertedDocs[6].slice(),  insertedDocs[7].slice(),
        insertedDocs[8].slice(),  insertedDocs[9].slice(),
        insertedDocs[10].slice(), insertedDocs[11].slice(),
        insertedDocs[12].slice(), insertedDocs[13].slice(),
        insertedDocs[14].slice(), insertedDocs[15].slice(),
        insertedDocs[16].slice(), insertedDocs[17].slice(),
        insertedDocs[18].slice(), insertedDocs[19].slice(),
        insertedDocs[20].slice(), insertedDocs[21].slice(),
        insertedDocs[22].slice(), insertedDocs[23].slice(),
        insertedDocs[24].slice(), insertedDocs[25].slice(),
        insertedDocs[26].slice(), insertedDocs[27].slice(),
        insertedDocs[28].slice(), insertedDocs[29].slice(),
        insertedDocs[30].slice(), insertedDocs[31].slice(),
        insertedDocs[32].slice(), insertedDocs[33].slice(),
        insertedDocs[34].slice(), insertedDocs[35].slice(),
        insertedDocs[36].slice(), insertedDocs[37].slice(),
    };
    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH { } SORT BM25(d) ASC, TFIDF(d) DESC, d.seq "
        "RETURN d");
    ASSERT_TRUE(queryResult.result.ok());
    auto slice = queryResult.data->slice();
    EXPECT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      EXPECT_TRUE((i < expected.size()));
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_TRUE((i == expected.size()));
  }

  // test non-empty object (true)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[0].slice(),  insertedDocs[1].slice(),
        insertedDocs[2].slice(),  insertedDocs[3].slice(),
        insertedDocs[4].slice(),  insertedDocs[5].slice(),
        insertedDocs[6].slice(),  insertedDocs[7].slice(),
        insertedDocs[8].slice(),  insertedDocs[9].slice(),
        insertedDocs[10].slice(), insertedDocs[11].slice(),
        insertedDocs[12].slice(), insertedDocs[13].slice(),
        insertedDocs[14].slice(), insertedDocs[15].slice(),
        insertedDocs[16].slice(), insertedDocs[17].slice(),
        insertedDocs[18].slice(), insertedDocs[19].slice(),
        insertedDocs[20].slice(), insertedDocs[21].slice(),
        insertedDocs[22].slice(), insertedDocs[23].slice(),
        insertedDocs[24].slice(), insertedDocs[25].slice(),
        insertedDocs[26].slice(), insertedDocs[27].slice(),
        insertedDocs[28].slice(), insertedDocs[29].slice(),
        insertedDocs[30].slice(), insertedDocs[31].slice(),
        insertedDocs[32].slice(), insertedDocs[33].slice(),
        insertedDocs[34].slice(), insertedDocs[35].slice(),
        insertedDocs[36].slice(), insertedDocs[37].slice(),
    };
    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH { 'a': 123, 'b': 'cde' } SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(queryResult.result.ok());
    auto slice = queryResult.data->slice();
    EXPECT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      EXPECT_TRUE((i < expected.size()));
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_TRUE((i == expected.size()));
  }

  // test empty string (false)
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH '' SORT BM25(d) ASC, TFIDF(d) DESC, d.seq "
        "RETURN d");
    ASSERT_TRUE(queryResult.result.ok());
    auto slice = queryResult.data->slice();
    EXPECT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      EXPECT_TRUE((i < expected.size()));
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_TRUE((i == expected.size()));
  }

  // test non-empty string (true)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[0].slice(),  insertedDocs[1].slice(),
        insertedDocs[2].slice(),  insertedDocs[3].slice(),
        insertedDocs[4].slice(),  insertedDocs[5].slice(),
        insertedDocs[6].slice(),  insertedDocs[7].slice(),
        insertedDocs[8].slice(),  insertedDocs[9].slice(),
        insertedDocs[10].slice(), insertedDocs[11].slice(),
        insertedDocs[12].slice(), insertedDocs[13].slice(),
        insertedDocs[14].slice(), insertedDocs[15].slice(),
        insertedDocs[16].slice(), insertedDocs[17].slice(),
        insertedDocs[18].slice(), insertedDocs[19].slice(),
        insertedDocs[20].slice(), insertedDocs[21].slice(),
        insertedDocs[22].slice(), insertedDocs[23].slice(),
        insertedDocs[24].slice(), insertedDocs[25].slice(),
        insertedDocs[26].slice(), insertedDocs[27].slice(),
        insertedDocs[28].slice(), insertedDocs[29].slice(),
        insertedDocs[30].slice(), insertedDocs[31].slice(),
        insertedDocs[32].slice(), insertedDocs[33].slice(),
        insertedDocs[34].slice(), insertedDocs[35].slice(),
        insertedDocs[36].slice(), insertedDocs[37].slice(),
    };
    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH 'abc' SORT BM25(d) ASC, TFIDF(d) DESC, d.seq "
        "RETURN d");
    ASSERT_TRUE(queryResult.result.ok());
    auto slice = queryResult.data->slice();
    EXPECT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      EXPECT_TRUE((i < expected.size()));
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_TRUE((i == expected.size()));
  }

  // test non-empty string (true), LIMIT 5
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[0].slice(), insertedDocs[1].slice(),
        insertedDocs[2].slice(), insertedDocs[3].slice(),
        insertedDocs[4].slice(),
    };
    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH 'abc' SORT BM25(d) ASC, TFIDF(d) DESC, d.seq "
        "LIMIT 5 RETURN d");
    ASSERT_TRUE(queryResult.result.ok());
    auto slice = queryResult.data->slice();
    EXPECT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      EXPECT_TRUE((i < expected.size()));
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_TRUE((i == expected.size()));
  }

  // test non-empty string (true), LIMIT 5
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[0].slice(), insertedDocs[1].slice(),
        insertedDocs[2].slice(), insertedDocs[3].slice(),
        insertedDocs[4].slice(),
    };
    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH @param SORT BM25(d) ASC, TFIDF(d) DESC, "
        "d.seq LIMIT 5 RETURN d",
        arangodb::velocypack::Parser::fromJson("{ \"param\" : \"abc\" }"));
    ASSERT_TRUE(queryResult.result.ok());
    auto slice = queryResult.data->slice();
    EXPECT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      EXPECT_TRUE((i < expected.size()));
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_TRUE((i == expected.size()));
  }

  // test bind parameter (true)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[0].slice(),  insertedDocs[1].slice(),
        insertedDocs[2].slice(),  insertedDocs[3].slice(),
        insertedDocs[4].slice(),  insertedDocs[5].slice(),
        insertedDocs[6].slice(),  insertedDocs[7].slice(),
        insertedDocs[8].slice(),  insertedDocs[9].slice(),
        insertedDocs[10].slice(), insertedDocs[11].slice(),
        insertedDocs[12].slice(), insertedDocs[13].slice(),
        insertedDocs[14].slice(), insertedDocs[15].slice(),
        insertedDocs[16].slice(), insertedDocs[17].slice(),
        insertedDocs[18].slice(), insertedDocs[19].slice(),
        insertedDocs[20].slice(), insertedDocs[21].slice(),
        insertedDocs[22].slice(), insertedDocs[23].slice(),
        insertedDocs[24].slice(), insertedDocs[25].slice(),
        insertedDocs[26].slice(), insertedDocs[27].slice(),
        insertedDocs[28].slice(), insertedDocs[29].slice(),
        insertedDocs[30].slice(), insertedDocs[31].slice(),
        insertedDocs[32].slice(), insertedDocs[33].slice(),
        insertedDocs[34].slice(), insertedDocs[35].slice(),
        insertedDocs[36].slice(), insertedDocs[37].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH @param SORT BM25(d) ASC, TFIDF(d) DESC, "
        "d.seq RETURN d",
        arangodb::velocypack::Parser::fromJson("{ \"param\" : [] }"));
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      EXPECT_TRUE((i < expected.size()));
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_TRUE((i == expected.size()));
  }
}

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

#include "Aql/AqlFunctionFeature.h"
#include "Aql/Ast.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Aql/Query.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterFeature.h"
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
#include "V8/v8-globals.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "VocBase/Methods/Collections.h"

#include "3rdParty/iresearch/tests/tests_config.hpp"
#include "Transaction/StandaloneContext.h"
#include "Utils/SingleCollectionTransaction.h"

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

class IResearchQueryAndTest : public ::testing::Test {
 protected:
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  IResearchQueryAndTest() : engine(server), server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

    arangodb::tests::init(true);

    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    // suppress WARNING {authentication} --server.jwt-secret is insecure. Use --server.jwt-secret-keyfile instead
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(),
                                    arangodb::LogLevel::ERR);

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AQL.name(), arangodb::LogLevel::ERR);  // suppress WARNING {aql} Suboptimal AqlItemMatrix index lookup:
    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(), arangodb::LogLevel::ERR);  // suppress WARNING DefaultCustomTypeHandler called
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(),
                                    arangodb::LogLevel::FATAL);
    irs::logger::output_le(iresearch::logger::IRL_FATAL, stderr);

    // setup required application features
    features.emplace_back(new arangodb::FlushFeature(server), false);
    features.emplace_back(new arangodb::V8DealerFeature(server),
                          false);  // required for DatabaseFeature::createDatabase(...)
    features.emplace_back(new arangodb::ViewTypesFeature(server), true);
    features.emplace_back(new arangodb::AuthenticationFeature(server), true);
    features.emplace_back(new arangodb::DatabasePathFeature(server), false);
    features.emplace_back(new arangodb::DatabaseFeature(server), false);
    features.emplace_back(new arangodb::ShardingFeature(server), false);  //
    features.emplace_back(new arangodb::QueryRegistryFeature(server), false);  // must be first
    arangodb::application_features::ApplicationServer::server->addFeature(
        features.back().first);  // need QueryRegistryFeature feature to be added now in order to create the system database
    features.emplace_back(new arangodb::SystemDatabaseFeature(server), true);  // required for IResearchAnalyzerFeature
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

    auto* analyzers =
        arangodb::application_features::ApplicationServer::lookupFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    TRI_vocbase_t* vocbase;

    dbFeature->createDatabase(1, "testVocbase", arangodb::velocypack::Slice::emptyObjectSlice(), vocbase);  // required for IResearchAnalyzerFeature::emplace(...)
    arangodb::methods::Collections::createSystem(
        *vocbase,
        arangodb::tests::AnalyzerCollectionName);

    analyzers->emplace(result, "testVocbase::test_analyzer", "TestAnalyzer",
                       VPackParser::fromJson("\"abc\"")->slice());  // cache analyzer

    analyzers->emplace(result, "testVocbase::test_csv_analyzer",
                       "TestDelimAnalyzer",
                       VPackParser::fromJson("\",\"")->slice());  // cache analyzer
    auto* dbPathFeature =
        arangodb::application_features::ApplicationServer::getFeature<arangodb::DatabasePathFeature>(
            "DatabasePath");
    arangodb::tests::setDatabasePath(*dbPathFeature);  // ensure test data is stored in a unique directory
  }

  ~IResearchQueryAndTest() {
    arangodb::AqlFeature(server).stop();  // unset singleton instance
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(),
                                    arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(),
                                    arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AQL.name(), arangodb::LogLevel::DEFAULT);
    arangodb::application_features::ApplicationServer::server = nullptr;

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
    arangodb::EngineSelectorFeature::ENGINE = nullptr;
  }
};  // IResearchQuerySetup

}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_F(IResearchQueryAndTest, test) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                        testDatabaseArgs);
  std::vector<arangodb::velocypack::Builder> insertedDocs;
  arangodb::LogicalView* view;

  // create collection0
  {
    auto createJson = VPackParser::fromJson(
        "{ \"name\": \"testCollection0\" }");
    auto collection = vocbase.createCollection(createJson->slice());
    ASSERT_TRUE((nullptr != collection));

    std::vector<std::shared_ptr<arangodb::velocypack::Builder>> docs{
        VPackParser::fromJson(
            "{ \"seq\": -6, \"value\": null }"),
        VPackParser::fromJson(
            "{ \"seq\": -5, \"value\": true }"),
        VPackParser::fromJson(
            "{ \"seq\": -4, \"value\": \"abc\" }"),
        VPackParser::fromJson(
            "{ \"seq\": -3, \"value\": 3.14 }"),
        VPackParser::fromJson(
            "{ \"seq\": -2, \"value\": [ 1, \"abc\" ] }"),
        VPackParser::fromJson(
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
    auto createJson = VPackParser::fromJson(
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
    auto createJson = VPackParser::fromJson(
        "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto logicalView = vocbase.createView(createJson->slice());
    ASSERT_TRUE((false == !logicalView));

    view = logicalView.get();
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(view);
    ASSERT_TRUE((false == !impl));

    auto updateJson = VPackParser::fromJson(
        "{ \"links\": {"
        "\"testCollection0\": { \"analyzers\": [ \"test_analyzer\", "
        "\"identity\" ], \"includeAllFields\": true, \"trackListPositions\": "
        "true, \"storeValues\":\"id\" },"
        "\"testCollection1\": { \"analyzers\": [ \"test_analyzer\", "
        "\"identity\" ], \"includeAllFields\": true, \"storeValues\":\"id\" }"
        "}}");
    EXPECT_TRUE((impl->properties(updateJson->slice(), true).ok()));
    std::set<TRI_voc_cid_t> cids;
    impl->visitCollections([&cids](TRI_voc_cid_t cid) -> bool {
      cids.emplace(cid);
      return true;
    });
    EXPECT_TRUE((2 == cids.size()));
    EXPECT_TRUE(
        (TRI_ERROR_NO_ERROR ==
         arangodb::tests::executeQuery(vocbase,
                                       "FOR d IN testView SEARCH 1 == 1 "
                                       "OPTIONS { waitForSync: true } RETURN d")
             .result.errorNumber()));  // commit
  }

  // field and missing field
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d['same'] == 'xyz' AND d.invalid == 2 SORT "
        "BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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

  // two different fields
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[6].slice(),  insertedDocs[10].slice(),
        insertedDocs[12].slice(), insertedDocs[14].slice(),
        insertedDocs[15].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d['same'] == 'xyz' AND d.value == 100 SORT "
        "BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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

  // not field and field
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[6].slice(),  insertedDocs[10].slice(),
        insertedDocs[12].slice(), insertedDocs[14].slice(),
        insertedDocs[15].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH NOT (d['same'] == 'abc') AND d.value == 100 "
        "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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

  // field and phrase
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.same == 'xyz' AND "
        "ANALYZER(PHRASE(d['duplicated'], 'z'), 'test_analyzer') SORT BM25(d) "
        "ASC, TFIDF(d) DESC, d.seq RETURN d");
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

  // not phrase and field
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[6].slice(),  insertedDocs[9].slice(),
        insertedDocs[10].slice(), insertedDocs[11].slice(),
        insertedDocs[12].slice(), insertedDocs[14].slice(),
        insertedDocs[15].slice(), insertedDocs[16].slice(),
        insertedDocs[17].slice(), insertedDocs[18].slice(),
        insertedDocs[20].slice(), insertedDocs[21].slice(),
        insertedDocs[23].slice(), insertedDocs[25].slice(),
        insertedDocs[26].slice(), insertedDocs[27].slice(),
        insertedDocs[28].slice(), insertedDocs[30].slice(),
        insertedDocs[31].slice(), insertedDocs[32].slice(),
        insertedDocs[33].slice(), insertedDocs[34].slice(),
        insertedDocs[35].slice(), insertedDocs[36].slice(),
        insertedDocs[37].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH NOT ANALYZER(PHRASE(d['duplicated'], 'z'), "
        "'test_analyzer') AND d.same == 'xyz' SORT BM25(d) ASC, TFIDF(d) DESC, "
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

  // not phrase and field
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[6].slice(),  insertedDocs[9].slice(),
        insertedDocs[10].slice(), insertedDocs[11].slice(),
        insertedDocs[12].slice(), insertedDocs[14].slice(),
        insertedDocs[15].slice(), insertedDocs[16].slice(),
        insertedDocs[17].slice(), insertedDocs[18].slice(),
        insertedDocs[20].slice(), insertedDocs[21].slice(),
        insertedDocs[23].slice(), insertedDocs[25].slice(),
        insertedDocs[26].slice(), insertedDocs[27].slice(),
        insertedDocs[28].slice(), insertedDocs[30].slice(),
        insertedDocs[31].slice(), insertedDocs[32].slice(),
        insertedDocs[33].slice(), insertedDocs[34].slice(),
        insertedDocs[35].slice(), insertedDocs[36].slice(),
        insertedDocs[37].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(NOT PHRASE(d['duplicated'], 'z'), "
        "'test_analyzer') AND d.same == 'xyz' SORT BM25(d) ASC, TFIDF(d) DESC, "
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

  // field and prefix
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[36].slice(), insertedDocs[37].slice(),
        insertedDocs[6].slice(),  insertedDocs[9].slice(),
        insertedDocs[26].slice(), insertedDocs[31].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.same == 'xyz' AND STARTS_WITH(d['prefix'], "
        "'abc') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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

  // not prefix and field
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[10].slice(), insertedDocs[11].slice(),
        insertedDocs[12].slice(), insertedDocs[13].slice(),
        insertedDocs[14].slice(), insertedDocs[15].slice(),
        insertedDocs[16].slice(), insertedDocs[17].slice(),
        insertedDocs[18].slice(), insertedDocs[19].slice(),
        insertedDocs[20].slice(), insertedDocs[21].slice(),
        insertedDocs[22].slice(), insertedDocs[23].slice(),
        insertedDocs[24].slice(), insertedDocs[25].slice(),
        insertedDocs[27].slice(), insertedDocs[28].slice(),
        insertedDocs[29].slice(), insertedDocs[30].slice(),
        insertedDocs[32].slice(), insertedDocs[33].slice(),
        insertedDocs[34].slice(), insertedDocs[35].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH NOT STARTS_WITH(d['prefix'], 'abc') AND "
        "d.same == 'xyz' SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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

  // field and exists
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[6].slice(),  insertedDocs[9].slice(),
        insertedDocs[14].slice(), insertedDocs[21].slice(),
        insertedDocs[26].slice(), insertedDocs[29].slice(),
        insertedDocs[31].slice(), insertedDocs[34].slice(),
        insertedDocs[36].slice(), insertedDocs[37].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.same == 'xyz' AND EXISTS(d['prefix']) SORT "
        "BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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

  // not exists and field
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[10].slice(), insertedDocs[11].slice(),
        insertedDocs[12].slice(), insertedDocs[13].slice(),
        insertedDocs[15].slice(), insertedDocs[16].slice(),
        insertedDocs[17].slice(), insertedDocs[18].slice(),
        insertedDocs[19].slice(), insertedDocs[20].slice(),
        insertedDocs[22].slice(), insertedDocs[23].slice(),
        insertedDocs[24].slice(), insertedDocs[25].slice(),
        insertedDocs[27].slice(), insertedDocs[28].slice(),
        insertedDocs[30].slice(), insertedDocs[32].slice(),
        insertedDocs[33].slice(), insertedDocs[35].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH NOT EXISTS(d['prefix']) AND d.same == 'xyz' "
        "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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

  // phrase and not field and exists
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d['duplicated'], 'z'), "
        "'test_analyzer') AND NOT (d.same == 'abc') AND EXISTS(d['prefix']) "
        "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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

  // prefix and not exists and field
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[37].slice(),
        insertedDocs[9].slice(),
        insertedDocs[31].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH STARTS_WITH(d['prefix'], 'abc') AND NOT "
        "EXISTS(d.duplicated) AND d.same == 'xyz' SORT BM25(d) ASC, TFIDF(d) "
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

  // prefix and not exists and field with limit
  {
    std::vector<arangodb::velocypack::Slice> expected = {insertedDocs[37].slice(),
                                                         insertedDocs[9].slice()};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH STARTS_WITH(d['prefix'], 'abc') AND NOT "
        "EXISTS(d.duplicated) AND d.same == 'xyz' SORT BM25(d) ASC, TFIDF(d) "
        "DESC, d.seq LIMIT 2 RETURN d");
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

  // exists and not prefix and phrase and not field and range
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH EXISTS(d.name) AND NOT "
        "STARTS_WITH(d['prefix'], 'abc') AND ANALYZER(PHRASE(d['duplicated'], "
        "'z'), 'test_analyzer') AND NOT (d.same == 'abc') AND d.seq >= 23 SORT "
        "BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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

  // exists and not prefix and phrase and not field and range
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH EXISTS(d.name) AND NOT "
        "STARTS_WITH(d['prefix'], 'abc') AND ANALYZER(PHRASE(d['duplicated'], "
        "'z'), 'test_analyzer') AND NOT (d.same == 'abc') AND d.seq >= 23 SORT "
        "BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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

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
#include "V8Server/V8DealerFeature.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "VocBase/Methods/Collections.h"

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

class IResearchQueryPhraseTest : public ::testing::Test {
 protected:
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;
  TRI_vocbase_t* sysVocBase{};

  IResearchQueryPhraseTest() : engine(server), server(nullptr, nullptr) {
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
    features.emplace_back(new arangodb::V8DealerFeature(server),
                          false);  // required for DatabaseFeature::createDatabase(...)
    features.emplace_back(new arangodb::ViewTypesFeature(server), true);
    features.emplace_back(new arangodb::AuthenticationFeature(server), true);
    features.emplace_back(new arangodb::DatabasePathFeature(server), false);
    features.emplace_back(new arangodb::DatabaseFeature(server), false);
    features.emplace_back(new arangodb::ShardingFeature(server), false);
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

    auto const databases = arangodb::velocypack::Parser::fromJson(
        std::string("[ { \"name\": \"") +
        arangodb::StaticStrings::SystemDatabase + "\" } ]");
    auto* dbFeature =
        arangodb::application_features::ApplicationServer::lookupFeature<arangodb::DatabaseFeature>(
            "Database");
    dbFeature->loadDatabases(databases->slice());

    for (auto& f : features) {
      if (f.second) {
        f.first->start();
      }
    }

    auto* sysVocBaseFeature =
        arangodb::application_features::ApplicationServer::lookupFeature<arangodb::SystemDatabaseFeature>();
    EXPECT_TRUE(sysVocBaseFeature);

    auto sysVocBasePtr = sysVocBaseFeature->use();
    arangodb::methods::Collections::createSystem(
        *sysVocBasePtr, 
        arangodb::tests::AnalyzerCollectionName);

    auto* analyzers =
        arangodb::application_features::ApplicationServer::lookupFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    TRI_vocbase_t* vocbase;

    dbFeature->createDatabase(1, "testVocbase", vocbase);  // required for IResearchAnalyzerFeature::emplace(...)
    arangodb::methods::Collections::createSystem(
        *vocbase, 
        arangodb::tests::AnalyzerCollectionName);
    analyzers->emplace(result, "testVocbase::test_analyzer", "TestAnalyzer",
                       VPackParser::fromJson("\"abc\"")->slice(), 
                       irs::flags{irs::frequency::type(), irs::position::type()}  // required for PHRASE
    );  // cache analyzer

    analyzers->emplace(result, "testVocbase::test_csv_analyzer",
                       "TestDelimAnalyzer",
                       VPackParser::fromJson("\",\"")->slice());  // cache analyzer

    analyzers->emplace(result, "_system::test_analyzer", "TestAnalyzer", 
                       VPackParser::fromJson("\"abc\"")->slice(),
                       irs::flags{irs::frequency::type(), irs::position::type()}  // required for PHRASE
    );  // cache analyzer

    analyzers->emplace(result, "_system::test_csv_analyzer", "TestDelimAnalyzer",
                       VPackParser::fromJson("\",\"")->slice());  // cache analyzer

    auto* dbPathFeature =
        arangodb::application_features::ApplicationServer::getFeature<arangodb::DatabasePathFeature>(
            "DatabasePath");
    arangodb::tests::setDatabasePath(*dbPathFeature);  // ensure test data is stored in a unique directory
  }

  ~IResearchQueryPhraseTest() {
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

TEST_F(IResearchQueryPhraseTest, SysVocbase) {
  std::vector<arangodb::velocypack::Builder> insertedDocs;
  arangodb::LogicalView* view;

  auto* sysVocBaseFeature =
      arangodb::application_features::ApplicationServer::lookupFeature<arangodb::SystemDatabaseFeature>();
  ASSERT_TRUE(sysVocBaseFeature);

  auto sysVocBasePtr = sysVocBaseFeature->use();
  auto& vocbase = *sysVocBasePtr;

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
        "\"testCollection0\": { \"analyzers\": [ \"test_analyzer\", "
        "\"identity\" ], \"includeAllFields\": true, \"trackListPositions\": "
        "true },"
        "\"testCollection1\": { \"analyzers\": [ \"::test_analyzer\", "
        "\"identity\" ], \"includeAllFields\": true }"
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

  // test missing field
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.missing, 'abc') SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
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

  // test missing field via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d['missing'], 'abc') SORT BM25(d) "
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

  // test invalid column type
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.seq, '0') SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
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

  // test invalid column type via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d['seq'], '0') SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
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

  // test invalid input type (empty-array)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.value, [ ]) SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (empty-array) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d['value'], [ ]) SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (array)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.value, [ 1, \"abc\" ]) SORT BM25(d) "
        "ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (array) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d['value'], [ 1, \"abc\" ]) SORT "
        "BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (boolean)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.value, true) SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (boolean) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d['value'], false) SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (null)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.value, null) SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (null) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d['value'], null) SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (numeric)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.value, 3.14) SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (numeric) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d['value'], 1234) SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (object)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.value, { \"a\": 7, \"b\": \"c\" }) "
        "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (object) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d['value'], { \"a\": 7, \"b\": \"c\" "
        "}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test missing value
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.value) SORT BM25(d) ASC, TFIDF(d) "
        "DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH));
  }

  // test missing value via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d['value']) SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH));
  }

  // test invalid analyzer type (array)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, 'z'), [ 1, "
        "\"abc\" ]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid analyzer type (array) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d['duplicated'], 'z'), [ 1, "
        "\"abc\" ]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid analyzer type (boolean)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, 'z'), true) "
        "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid analyzer type (boolean) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH analyzer(PHRASE(d['duplicated'], 'z'), "
        "false) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid analyzer type (null)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH analyzer(PHRASE(d.duplicated, 'z'), null) "
        "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid analyzer type (null) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH analyzer(PHRASE(d['duplicated'], 'z'), null) "
        "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid analyzer type (numeric)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH analyzer(PHRASE(d.duplicated, 'z'), 3.14) "
        "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid analyzer type (numeric) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH analyzer(PHRASE(d['duplicated'], 'z'), 1234) "
        "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid analyzer type (object)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH analyzer(PHRASE(d.duplicated, 'z'), { \"a\": "
        "7, \"b\": \"c\" }) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid analyzer type (object) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH analyzer(PHRASE(d['duplicated'], 'z'), { "
        "\"a\": 7, \"b\": \"c\" }) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq "
        "RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test undefined analyzer
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH analyzer(PHRASE(d.duplicated, 'z'), "
        "'invalid_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test undefined analyzer via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d['duplicated'], 'z'), "
        "'invalid_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // can't access to local analyzer in other database
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, 'z'), "
        "'testVocbase::test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq "
        "RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // FIXME currently optimizer tries to evaluate ANALYZER function
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(1==1, 'test_analyzer') && ANALYZER(PHRASE(d.duplicated, 'z'), "
        "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_FALSE(result.result.ok());
    ASSERT_TRUE(result.result.is(TRI_ERROR_NOT_IMPLEMENTED));
  }

  // test custom analyzer
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, 'z'), "
        "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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

  // test custom analyzer
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.duplicated, 'z', '::test_analyzer') "
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

  // test custom analyzer
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.duplicated, 'z', "
        "'_system::test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq "
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

  // test custom analyzer via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH analyzer(PHRASE(d['duplicated'], 'z'), "
        "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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

  // test custom analyzer with offsets
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, 'v', 1, 'z'), "
        "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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

  // test custom analyzer with offsets
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.duplicated, 'v', 1, 'z', "
        "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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

  // test custom analyzer with offsets via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d['duplicated'], 'v', 2, "
        "'c'), 'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN "
        "d");
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

  // test custom analyzer with offsets via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d['duplicated'], 'v', 2, "
        "'c', 'test_analyzer'), 'identity') SORT BM25(d) ASC, TFIDF(d) DESC, "
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

  // test custom analyzer with offsets (no match)
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, 'v', 0, 'z'), "
        "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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

  // test custom analyzer with offsets (no match) via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d['duplicated'], 'v', 1, "
        "'c'), 'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN "
        "d");
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

  // test custom analyzer with [ phrase ] arg
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, [ 'z' ]), "
        "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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

  // test custom analyzer via [] with [ phrase ] arg
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d['duplicated'], [ 'z' ]), "
        "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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

  // test custom analyzer with offsets with [ phrase ] arg
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, [ 'v', 1, 'z' "
        "]), 'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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

  // test custom analyzer with offsets via [] with [ phrase ] arg
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d['duplicated'], [ 'v', 2, "
        "'c' ]), 'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq "
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

  // test custom analyzer with offsets (no match) with [ phrase ] arg
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, [ 'v', 0, 'z' "
        "]), 'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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

  // test custom analyzer with offsets (no match) via [] with [ phrase ] arg
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d['duplicated'], [ 'v', 1, "
        "'c' ]), 'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq "
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

  // test custom analyzer with offsets (no match) via [] with [ phrase ] arg
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d['duplicated'], [ 'v', 1, 'c' ], "
        "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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

  // test custom analyzer with offsets (no match) via [] with [ phrase ] arg
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d['duplicated'], [ 'v', 1, "
        "'c' ], 'test_analyzer'), 'identity') SORT BM25(d) ASC, TFIDF(d) DESC, "
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
}

TEST_F(IResearchQueryPhraseTest, test) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                        "testVocbase");
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
        "\"testCollection0\": { \"analyzers\": [ \"test_analyzer\", "
        "\"::test_analyzer\", \"identity\" ], \"includeAllFields\": true, "
        "\"trackListPositions\": true },"
        "\"testCollection1\": { \"analyzers\": [ \"test_analyzer\", "
        "\"_system::test_analyzer\", \"identity\" ], \"includeAllFields\": "
        "true }"
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

  // test missing field
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.missing, 'abc') SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
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

  // test missing field via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d['missing'], 'abc') SORT BM25(d) "
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

  // test invalid column type
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.seq, '0') SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
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

  // test invalid column type via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d['seq'], '0') SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
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

  // test invalid input type (empty-array)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.value, [ ]) SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (empty-array) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d['value'], [ ]) SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (array)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.value, [ 1, \"abc\" ]) SORT BM25(d) "
        "ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (array) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d['value'], [ 1, \"abc\" ]) SORT "
        "BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (boolean)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.value, true) SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (boolean) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d['value'], false) SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (null)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.value, null) SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (null) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d['value'], null) SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (numeric)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.value, 3.14) SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (numeric) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d['value'], 1234) SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (object)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.value, { \"a\": 7, \"b\": \"c\" }) "
        "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (object) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d['value'], { \"a\": 7, \"b\": \"c\" "
        "}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test missing value
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.value) SORT BM25(d) ASC, TFIDF(d) "
        "DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH));
  }

  // test missing value via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d['value']) SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH));
  }

  // test invalid analyzer type (array)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, 'z'), [ 1, "
        "\"abc\" ]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid analyzer type (array) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d['duplicated'], 'z'), [ 1, "
        "\"abc\" ]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid analyzer type (boolean)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, 'z'), true) "
        "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid analyzer type (boolean) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH analyzer(PHRASE(d['duplicated'], 'z'), "
        "false) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid analyzer type (null)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH analyzer(PHRASE(d.duplicated, 'z'), null) "
        "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid analyzer type (null) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH analyzer(PHRASE(d['duplicated'], 'z'), null) "
        "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid analyzer type (numeric)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH analyzer(PHRASE(d.duplicated, 'z'), 3.14) "
        "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid analyzer type (numeric) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH analyzer(PHRASE(d['duplicated'], 'z'), 1234) "
        "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid analyzer type (object)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH analyzer(PHRASE(d.duplicated, 'z'), { \"a\": "
        "7, \"b\": \"c\" }) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid analyzer type (object) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH analyzer(PHRASE(d['duplicated'], 'z'), { "
        "\"a\": 7, \"b\": \"c\" }) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq "
        "RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test undefined analyzer
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH analyzer(PHRASE(d.duplicated, 'z'), "
        "'invalid_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test undefined analyzer via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d['duplicated'], 'z'), "
        "'invalid_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test custom analyzer (local)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, 'z'), "
        "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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

  // test custom analyzer (local)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, 'z'), "
        "'testVocbase::test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq "
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

  // test custom analyzer (system)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, 'z'), "
        "'::test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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

  // test custom analyzer (system)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, 'z'), "
        "'_system::test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq "
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

  // test custom analyzer
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.duplicated, 'z', 'test_analyzer') "
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

  // test custom analyzer via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH analyzer(PHRASE(d['duplicated'], 'z'), "
        "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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

  // test custom analyzer with offsets
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, 'v', 1, 'z'), "
        "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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

  // test custom analyzer with offsets
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.duplicated, 'v', 1, 'z', "
        "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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

  // test custom analyzer with offsets via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d['duplicated'], 'v', 2, "
        "'c'), 'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN "
        "d");
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

  // test custom analyzer with offsets via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d['duplicated'], 'v', 2, "
        "'c', 'test_analyzer'), 'identity') SORT BM25(d) ASC, TFIDF(d) DESC, "
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

  // test custom analyzer with offsets (no match)
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, 'v', 0, 'z'), "
        "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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

  // test custom analyzer with offsets (no match) via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d['duplicated'], 'v', 1, "
        "'c'), 'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN "
        "d");
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

  // test custom analyzer with [ phrase ] arg
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, [ 'z' ]), "
        "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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

  // test custom analyzer via [] with [ phrase ] arg
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d['duplicated'], [ 'z' ]), "
        "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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

  // test custom analyzer with offsets with [ phrase ] arg
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, [ 'v', 1, 'z' "
        "]), 'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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

  // test custom analyzer with offsets via [] with [ phrase ] arg
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d['duplicated'], [ 'v', 2, "
        "'c' ]), 'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq "
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

  // test custom analyzer with offsets (no match) with [ phrase ] arg
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, [ 'v', 0, 'z' "
        "]), 'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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

  // test custom analyzer with offsets (no match) via [] with [ phrase ] arg
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d['duplicated'], [ 'v', 1, "
        "'c' ]), 'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq "
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

  // test custom analyzer with offsets (no match) via [] with [ phrase ] arg
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d['duplicated'], [ 'v', 1, 'c' ], "
        "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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

  // test custom analyzer with offsets (no match) via [] with [ phrase ] arg
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d['duplicated'], [ 'v', 1, "
        "'c' ], 'test_analyzer'), 'identity') SORT BM25(d) ASC, TFIDF(d) DESC, "
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
}

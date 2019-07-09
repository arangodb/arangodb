////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
#include "Aql/ExecutionPlan.h"
#include "Aql/ExpressionContext.h"
#include "Aql/IResearchViewNode.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Aql/Query.h"
#include "Basics/SmallVector.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterFeature.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/ApplicationServerHelper.h"
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
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "V8/v8-globals.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/Methods/Collections.h"

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

class IResearchQueryScorerTest : public ::testing::Test {
 protected:
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  IResearchQueryScorerTest() : engine(server), server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;
    arangodb::aql::AqlFunctionFeature* functions = nullptr;

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
    features.emplace_back(functions = new arangodb::aql::AqlFunctionFeature(server),
                          true);  // required for IResearchAnalyzerFeature
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

    // register fake non-deterministic function in order to suppress optimizations
    functions->add(arangodb::aql::Function{
        "_NONDETERM_", ".",
        arangodb::aql::Function::makeFlags(
            // fake non-deterministic
            arangodb::aql::Function::Flags::CanRunOnDBServer),
        [](arangodb::aql::ExpressionContext*, arangodb::transaction::Methods*,
           arangodb::aql::VPackFunctionParameters const& params) {
          TRI_ASSERT(!params.empty());
          return params[0];
        }});

    // register fake non-deterministic function in order to suppress optimizations
    functions->add(arangodb::aql::Function{
        "_FORWARD_", ".",
        arangodb::aql::Function::makeFlags(
            // fake deterministic
            arangodb::aql::Function::Flags::Deterministic, arangodb::aql::Function::Flags::Cacheable,
            arangodb::aql::Function::Flags::CanRunOnDBServer),
        [](arangodb::aql::ExpressionContext*, arangodb::transaction::Methods*,
           arangodb::aql::VPackFunctionParameters const& params) {
          TRI_ASSERT(!params.empty());
          return params[0];
        }});

    // external function names must be registred in upper-case
    // user defined functions have ':' in the external function name
    // function arguments string format: requiredArg1[,requiredArg2]...[|optionalArg1[,optionalArg2]...]
    arangodb::aql::Function customScorer(
        "CUSTOMSCORER", ".|+",
        arangodb::aql::Function::makeFlags(arangodb::aql::Function::Flags::Deterministic,
                                           arangodb::aql::Function::Flags::Cacheable,
                                           arangodb::aql::Function::Flags::CanRunOnDBServer));
    arangodb::iresearch::addFunction(*arangodb::aql::AqlFunctionFeature::AQLFUNCTIONS,
                                     customScorer);

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

  ~IResearchQueryScorerTest() {
    arangodb::AqlFeature(server).stop();  // unset singleton instance
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(),
                                    arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(),
                                    arangodb::LogLevel::DEFAULT);
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
};  // IResearchQueryScorerSetup

}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(IResearchQueryScorerTest, test) {
  static std::vector<std::string> const EMPTY;

  auto createJson = arangodb::velocypack::Parser::fromJson(
      "{ \
    \"name\": \"testView\", \
    \"type\": \"arangosearch\" \
  }");

  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                        testDatabaseArgs);
  std::shared_ptr<arangodb::LogicalCollection> logicalCollection1;
  std::shared_ptr<arangodb::LogicalCollection> logicalCollection2;
  std::shared_ptr<arangodb::LogicalCollection> logicalCollection3;

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

  // add collection_3
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"collection_3\" }");
    logicalCollection3 = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection3));
  }

  // add view
  auto view = std::dynamic_pointer_cast<arangodb::iresearch::IResearchView>(
      vocbase.createView(createJson->slice()));
  ASSERT_TRUE((false == !view));

  // add link to collection
  {
    auto updateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": {"
        "\"collection_1\": { \"analyzers\": [ \"test_analyzer\", \"identity\" "
        "], \"includeAllFields\": true, \"trackListPositions\": true },"
        "\"collection_2\": { \"analyzers\": [ \"test_analyzer\", \"identity\" "
        "], \"includeAllFields\": true }"
        "}}");
    EXPECT_TRUE((view->properties(updateJson->slice(), true).ok()));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                  arangodb::LogicalDataSource::Serialize::Detailed));
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

  std::deque<arangodb::ManagedDocumentResult> insertedDocsView;

  // populate view with the data
  {
    arangodb::OperationOptions opt;

    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                       EMPTY, EMPTY, EMPTY,
                                       arangodb::transaction::Options());
    EXPECT_TRUE((trx.begin().ok()));

    // insert into collections
    {
      irs::utf8_path resource;
      resource /= irs::string_ref(arangodb::tests::testResourceDir);
      resource /= irs::string_ref("simple_sequential.json");

      auto builder =
          arangodb::basics::VelocyPackHelper::velocyPackFromFile(resource.utf8());
      auto root = builder.slice();
      ASSERT_TRUE(root.isArray());

      size_t i = 0;

      std::shared_ptr<arangodb::LogicalCollection> collections[]{logicalCollection1,
                                                                 logicalCollection2};

      for (auto doc : arangodb::velocypack::ArrayIterator(root)) {
        insertedDocsView.emplace_back();
        auto const res =
            collections[i % 2]->insert(&trx, doc, insertedDocsView.back(), opt, false);
        EXPECT_TRUE(res.ok());
        ++i;
      }
    }

    // insert into collection_3
    std::deque<arangodb::ManagedDocumentResult> insertedDocsCollection;

    {
      irs::utf8_path resource;
      resource /= irs::string_ref(arangodb::tests::testResourceDir);
      resource /= irs::string_ref("simple_sequential_order.json");

      auto builder =
          arangodb::basics::VelocyPackHelper::velocyPackFromFile(resource.utf8());
      auto root = builder.slice();
      ASSERT_TRUE(root.isArray());

      for (auto doc : arangodb::velocypack::ArrayIterator(root)) {
        insertedDocsCollection.emplace_back();
        auto const res =
            logicalCollection3->insert(&trx, doc, insertedDocsCollection.back(), opt, false);
        EXPECT_TRUE(res.ok());
      }
    }

    EXPECT_TRUE((trx.commit().ok()));
    EXPECT_TRUE(
        (arangodb::tests::executeQuery(vocbase,
                                       "FOR d IN testView SEARCH 1 ==1 OPTIONS "
                                       "{ waitForSync: true } RETURN d")
             .result.ok()));  // commit
  }

  // wrong number of arguments
  {
    std::string const query =
      "FOR d IN testView SEARCH BOOST(d.name == 'A') "
      "RETURN { d, score: BOOSTSCORER(d) }";

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_FALSE(queryResult.result.ok());
    ASSERT_TRUE(queryResult.result.is(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH));
  }

  // invalid argument
  {
    std::string const query =
      "FOR d IN testView SEARCH BOOST(d.name == 'A', {}) "
      "RETURN { d, score: BOOSTSCORER(d) }";

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_FALSE(queryResult.result.ok());
    ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // invalid argument
  {
    std::string const query =
      "FOR d IN testView SEARCH BOOST(d.name == 'A', []) "
      "RETURN { d, score: BOOSTSCORER(d) }";

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_FALSE(queryResult.result.ok());
    ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // invalid argument
  {
    std::string const query =
      "FOR d IN testView SEARCH BOOST(d.name == 'A', true) "
      "RETURN { d, score: BOOSTSCORER(d) }";

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_FALSE(queryResult.result.ok());
    ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // invalid argument
  {
    std::string const query =
      "FOR d IN testView SEARCH BOOST(d.name == 'A', null) "
      "RETURN { d, score: BOOSTSCORER(d) }";

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_FALSE(queryResult.result.ok());
    ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // invalid argument
  {
    std::string const query =
      "FOR d IN testView SEARCH BOOST(d.name == 'A', '42') "
      "RETURN { d, score: BOOSTSCORER(d) }";

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_FALSE(queryResult.result.ok());
    ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // non-deterministic argument
  {
    std::string const query =
      "FOR d IN testView SEARCH BOOST(d.name == 'A', RAND()) "
      "RETURN { d, score: BOOSTSCORER(d) }";

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_FALSE(queryResult.result.ok());
    ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // FIXME currently optimizer tries to evaluate BOOST function
  {
    std::string const query =
      "FOR d IN testView SEARCH BOOST(1==1, 42) "
      "LIMIT 1 "
      "RETURN { d, score: BOOSTSCORER(d) }";

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_FALSE(queryResult.result.ok());
    ASSERT_TRUE(queryResult.result.is(TRI_ERROR_NOT_IMPLEMENTED));
  }

  {
    std::string const query =
      "FOR d IN testView SEARCH BOOST(d.name == 'A', 42) "
      "RETURN { d, score: BOOSTSCORER(d) }";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase, query, {
      arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
    }));

    std::map<float, arangodb::velocypack::Slice> expectedDocs{
      {42.f, arangodb::velocypack::Slice(insertedDocsView[0].vpack())}
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_TRUE(expectedDocs.size() == resultIt.size());

    // Check documents
    for (; resultIt.valid(); resultIt.next()) {
      auto const actualValue = resultIt.value();

      auto actualScoreSlice = actualValue.get("score");
      ASSERT_TRUE(actualScoreSlice.isNumber());
      auto const actualScore = actualScoreSlice.getNumber<float>();
      auto expectedValue = expectedDocs.find(actualScore);
      ASSERT_TRUE(expectedValue != expectedDocs.end());

      auto const actualDoc = actualValue.get("d");
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                            arangodb::velocypack::Slice(expectedValue->second),
                            resolved, true)));
      expectedDocs.erase(expectedValue);
    }
    EXPECT_TRUE(expectedDocs.empty());
  }

  {
    std::string const query =
        "LET arr = [0,1] "
        "FOR i in 0..1 "
        "  LET rnd = _NONDETERM_(i) "
        "  FOR d IN testView SEARCH IN_RANGE(d.name, 'A', 'C', true, true) "
        "LIMIT 10 "
        "RETURN { d, score: d.seq + 3*customscorer(d, arr[TO_NUMBER(rnd != "
        "0)]) }";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase, query,
                                             {
                                                 arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
                                             }));

    std::map<size_t, arangodb::velocypack::Slice> expectedDocs{
        {0, arangodb::velocypack::Slice(insertedDocsView[0].vpack())},
        {1, arangodb::velocypack::Slice(insertedDocsView[1].vpack())},
        {2, arangodb::velocypack::Slice(insertedDocsView[2].vpack())},
        {3, arangodb::velocypack::Slice(insertedDocsView[0].vpack())},
        {4, arangodb::velocypack::Slice(insertedDocsView[1].vpack())},
        {5, arangodb::velocypack::Slice(insertedDocsView[2].vpack())},
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_TRUE(expectedDocs.size() == resultIt.size());

    // Check documents
    for (; resultIt.valid(); resultIt.next()) {
      auto const actualValue = resultIt.value();

      auto actualScoreSlice = actualValue.get("score");
      ASSERT_TRUE(actualScoreSlice.isNumber());
      auto const actualScore = actualScoreSlice.getNumber<size_t>();
      auto expectedValue = expectedDocs.find(actualScore);
      ASSERT_TRUE(expectedValue != expectedDocs.end());

      auto const actualDoc = actualValue.get("d");
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                            arangodb::velocypack::Slice(expectedValue->second),
                            resolved, true)));
      expectedDocs.erase(expectedValue);
    }
    EXPECT_TRUE(expectedDocs.empty());
  }

  // ensure subqueries outstide a loop work fine
  {
    std::string const query =
        "LET x = (FOR j IN testView SEARCH j.name == 'A' SORT BM25(j) RETURN "
        "j) "
        "FOR d in testView SEARCH d.name == 'B' "
        "SORT customscorer(d, x[0].seq) "
        "RETURN { d, 'score' : customscorer(d, x[0].seq) }";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase, query,
                                             {
                                                 arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
                                             }));

    std::map<size_t, arangodb::velocypack::Slice> expectedDocs{
        {0, arangodb::velocypack::Slice(insertedDocsView[1].vpack())},
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_TRUE(expectedDocs.size() == resultIt.size());

    // Check documents
    for (; resultIt.valid(); resultIt.next()) {
      auto const actualValue = resultIt.value();

      auto actualScoreSlice = actualValue.get("score");
      ASSERT_TRUE(actualScoreSlice.isNumber());
      auto const actualScore = actualScoreSlice.getNumber<size_t>();
      auto expectedValue = expectedDocs.find(actualScore);
      ASSERT_TRUE(expectedValue != expectedDocs.end());

      auto const actualDoc = actualValue.get("d");
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                            arangodb::velocypack::Slice(expectedValue->second),
                            resolved, true)));
      expectedDocs.erase(expectedValue);
    }
    EXPECT_TRUE(expectedDocs.empty());
  }

  // FIXME
  // inline subqueries aren't supported, e.g. the query below will be transformed into
  //
  // FOR d in testView SEARCH d.name == 'B' LET #1 = customscorer(d, #2[0].seq)
  // LET #2 = (FOR j IN testView SEARCH j.name == 'A' SORT BM25(j) RETURN j)
  // RETURN { d, 'score' : #1 ) }
  {
    std::string const query =
        "FOR d in testView SEARCH d.name == 'B' "
        "RETURN { d, 'score' : customscorer(d, (FOR j IN testView SEARCH "
        "j.name == 'A' SORT BM25(j) RETURN j)[0].seq) }";

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.is(TRI_ERROR_INTERNAL));
  }

  // ensure scorers are deduplicated
  {
    std::string const queryString =
        "LET i = 1"
        "FOR d IN testView SEARCH IN_RANGE(d.name, 'A', 'B', true, false) "
        "RETURN [ customscorer(d, i), customscorer(d, 1) ] ";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase, queryString,
                                             {
                                                 arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
                                             }));

    arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString(queryString),
                               std::shared_ptr<arangodb::velocypack::Builder>(),
                               arangodb::velocypack::Parser::fromJson("{}"),
                               arangodb::aql::PART_MAIN);

    query.prepare(arangodb::QueryRegistryFeature::registry());
    auto* plan = query.plan();
    ASSERT_TRUE(plan);

    arangodb::SmallVector<arangodb::aql::ExecutionNode*>::allocator_type::arena_type a;
    arangodb::SmallVector<arangodb::aql::ExecutionNode*> nodes{a};

    // only one scorer
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, true);
    ASSERT_TRUE(1 == nodes.size());
    auto* viewNode =
        arangodb::aql::ExecutionNode::castTo<arangodb::iresearch::IResearchViewNode*>(
            nodes.front());
    ASSERT_TRUE(viewNode);
    auto& scorers = viewNode->scorers();
    ASSERT_TRUE(1 == scorers.size());
    auto* var = scorers.front().var;
    ASSERT_TRUE(var);

    // check scorer
    {
      auto* expr = scorers.front().node;
      ASSERT_TRUE(expr);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_FCALL == expr->type);
      auto* fn = static_cast<arangodb::aql::Function*>(expr->getData());
      ASSERT_TRUE(fn);
      ASSERT_TRUE(arangodb::iresearch::isScorer(*fn));
      EXPECT_TRUE("CUSTOMSCORER" == fn->name);

      ASSERT_TRUE(1 == expr->numMembers());
      auto* args = expr->getMember(0);
      ASSERT_TRUE(args);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_ARRAY == args->type);
      ASSERT_TRUE(2 == args->numMembers());
      auto* arg0 = args->getMember(0);  // reference to d
      ASSERT_TRUE(arg0);
      ASSERT_TRUE(static_cast<void const*>(&viewNode->outVariable()) == arg0->getData());
      auto* arg1 = args->getMember(1);
      ASSERT_TRUE(arg1);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_VALUE == arg1->type);
      ASSERT_TRUE(arangodb::aql::VALUE_TYPE_INT == arg1->value.type);
      ASSERT_TRUE(1 == arg1->getIntValue());
    }

    // and 2 references
    nodes.clear();
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::CALCULATION, true);
    ASSERT_TRUE(1 == nodes.size());
    auto* calcNode =
        arangodb::aql::ExecutionNode::castTo<arangodb::aql::CalculationNode*>(
            nodes.front());
    ASSERT_TRUE(calcNode);
    ASSERT_TRUE(calcNode->expression());
    auto* node = calcNode->expression()->node();
    ASSERT_TRUE(node);
    ASSERT_TRUE(arangodb::aql::NODE_TYPE_ARRAY == node->type);
    ASSERT_TRUE(2 == node->numMembers());
    for (size_t i = 0; i < node->numMembers(); ++i) {
      auto* sub = node->getMember(i);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_REFERENCE == sub->type);
      EXPECT_TRUE(static_cast<const void*>(var) == sub->getData());
    }

    // check execution
    auto queryResult = arangodb::tests::executeQuery(vocbase, queryString);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_TRUE(1 == resultIt.size());

    for (; resultIt.valid(); resultIt.next()) {
      auto const actualValue = resultIt.value();
      ASSERT_TRUE(actualValue.isArray());

      VPackArrayIterator scoreIt(actualValue);
      EXPECT_TRUE(2 == scoreIt.size());

      for (; scoreIt.valid(); scoreIt.next()) {
        auto const value = scoreIt.value();
        ASSERT_TRUE(value.isNumber());
        EXPECT_TRUE(1 == value.getNumber<size_t>());
      }
    }
  }

  // ensure scorers are deduplicated (attribute access)
  {
    std::string const queryString =
        "LET obj = _NONDETERM_({ value : 2 }) "
        "FOR d IN testView SEARCH IN_RANGE(d.name, 'A', 'C', true, true) "
        "RETURN [ customscorer(d, obj.value), customscorer(d, obj.value) ] ";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase, queryString,
                                             {
                                                 arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
                                             }));

    arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString(queryString),
                               std::shared_ptr<arangodb::velocypack::Builder>(),
                               arangodb::velocypack::Parser::fromJson("{}"),
                               arangodb::aql::PART_MAIN);

    query.prepare(arangodb::QueryRegistryFeature::registry());
    auto* plan = query.plan();
    ASSERT_TRUE(plan);

    arangodb::SmallVector<arangodb::aql::ExecutionNode*>::allocator_type::arena_type a;
    arangodb::SmallVector<arangodb::aql::ExecutionNode*> nodes{a};

    // only one scorer
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, true);
    ASSERT_TRUE(1 == nodes.size());
    auto* viewNode =
        arangodb::aql::ExecutionNode::castTo<arangodb::iresearch::IResearchViewNode*>(
            nodes.front());
    ASSERT_TRUE(viewNode);
    auto& scorers = viewNode->scorers();
    ASSERT_TRUE(1 == scorers.size());
    auto* var = scorers.front().var;
    ASSERT_TRUE(var);

    // check scorer
    {
      auto* expr = scorers.front().node;
      ASSERT_TRUE(expr);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_FCALL == expr->type);
      auto* fn = static_cast<arangodb::aql::Function*>(expr->getData());
      ASSERT_TRUE(fn);
      ASSERT_TRUE(arangodb::iresearch::isScorer(*fn));
      EXPECT_TRUE("CUSTOMSCORER" == fn->name);

      ASSERT_TRUE(1 == expr->numMembers());
      auto* args = expr->getMember(0);
      ASSERT_TRUE(args);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_ARRAY == args->type);
      ASSERT_TRUE(2 == args->numMembers());
      auto* arg0 = args->getMember(0);  // reference to d
      ASSERT_TRUE(arg0);
      ASSERT_TRUE(static_cast<void const*>(&viewNode->outVariable()) == arg0->getData());
      auto* arg1 = args->getMember(1);
      ASSERT_TRUE(arg1);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS == arg1->type);
    }

    // and 2 references
    nodes.clear();
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::CALCULATION, true);
    ASSERT_TRUE(2 == nodes.size());
    for (auto const* node : nodes) {
      auto* calcNode =
          arangodb::aql::ExecutionNode::castTo<arangodb::aql::CalculationNode const*>(node);
      ASSERT_TRUE(calcNode);
      ASSERT_TRUE(calcNode->expression());

      if (calcNode->outVariable()->name == "obj") {
        continue;
      }

      auto* exprNode = calcNode->expression()->node();
      ASSERT_TRUE(exprNode);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_ARRAY == exprNode->type);
      ASSERT_TRUE(2 == exprNode->numMembers());
      for (size_t i = 0; i < exprNode->numMembers(); ++i) {
        auto* sub = exprNode->getMember(i);
        ASSERT_TRUE(arangodb::aql::NODE_TYPE_REFERENCE == sub->type);
        EXPECT_TRUE(static_cast<const void*>(var) == sub->getData());
      }
    }

    // check execution
    auto queryResult = arangodb::tests::executeQuery(vocbase, queryString);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_TRUE(3 == resultIt.size());

    for (; resultIt.valid(); resultIt.next()) {
      auto const actualValue = resultIt.value();
      ASSERT_TRUE(actualValue.isArray());

      VPackArrayIterator scoreIt(actualValue);
      EXPECT_TRUE(2 == scoreIt.size());

      for (; scoreIt.valid(); scoreIt.next()) {
        auto const value = scoreIt.value();
        ASSERT_TRUE(value.isNumber());
        EXPECT_TRUE(2 == value.getNumber<size_t>());
      }
    }
  }

  // ensure scorers are deduplicated (expression)
  {
    std::string const queryString =
        "LET obj = _NONDETERM_({ value : 2 }) "
        "FOR d IN testView SEARCH IN_RANGE(d.name, 'A', 'C', true, true) "
        "RETURN [ customscorer(d, obj.value+1), customscorer(d, obj.value+1) "
        "] ";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase, queryString,
                                             {
                                                 arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
                                             }));

    arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString(queryString),
                               std::shared_ptr<arangodb::velocypack::Builder>(),
                               arangodb::velocypack::Parser::fromJson("{}"),
                               arangodb::aql::PART_MAIN);

    query.prepare(arangodb::QueryRegistryFeature::registry());
    auto* plan = query.plan();
    ASSERT_TRUE(plan);

    arangodb::SmallVector<arangodb::aql::ExecutionNode*>::allocator_type::arena_type a;
    arangodb::SmallVector<arangodb::aql::ExecutionNode*> nodes{a};

    // only one scorer
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, true);
    ASSERT_TRUE(1 == nodes.size());
    auto* viewNode =
        arangodb::aql::ExecutionNode::castTo<arangodb::iresearch::IResearchViewNode*>(
            nodes.front());
    ASSERT_TRUE(viewNode);
    auto& scorers = viewNode->scorers();
    ASSERT_TRUE(1 == scorers.size());
    auto* var = scorers.front().var;
    ASSERT_TRUE(var);

    // check scorer
    {
      auto* expr = scorers.front().node;
      ASSERT_TRUE(expr);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_FCALL == expr->type);
      auto* fn = static_cast<arangodb::aql::Function*>(expr->getData());
      ASSERT_TRUE(fn);
      ASSERT_TRUE(arangodb::iresearch::isScorer(*fn));
      EXPECT_TRUE("CUSTOMSCORER" == fn->name);

      ASSERT_TRUE(1 == expr->numMembers());
      auto* args = expr->getMember(0);
      ASSERT_TRUE(args);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_ARRAY == args->type);
      ASSERT_TRUE(2 == args->numMembers());
      auto* arg0 = args->getMember(0);  // reference to d
      ASSERT_TRUE(arg0);
      ASSERT_TRUE(static_cast<void const*>(&viewNode->outVariable()) == arg0->getData());
      auto* arg1 = args->getMember(1);
      ASSERT_TRUE(arg1);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_OPERATOR_BINARY_PLUS == arg1->type);
    }

    // and 2 references
    nodes.clear();
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::CALCULATION, true);
    ASSERT_TRUE(2 == nodes.size());
    for (auto const* node : nodes) {
      auto* calcNode =
          arangodb::aql::ExecutionNode::castTo<arangodb::aql::CalculationNode const*>(node);
      ASSERT_TRUE(calcNode);
      ASSERT_TRUE(calcNode->expression());

      if (calcNode->outVariable()->name == "obj") {
        continue;
      }

      auto* exprNode = calcNode->expression()->node();
      ASSERT_TRUE(exprNode);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_ARRAY == exprNode->type);
      ASSERT_TRUE(2 == exprNode->numMembers());
      for (size_t i = 0; i < exprNode->numMembers(); ++i) {
        auto* sub = exprNode->getMember(i);
        ASSERT_TRUE(arangodb::aql::NODE_TYPE_REFERENCE == sub->type);
        EXPECT_TRUE(static_cast<const void*>(var) == sub->getData());
      }
    }

    // check execution
    auto queryResult = arangodb::tests::executeQuery(vocbase, queryString);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_TRUE(3 == resultIt.size());

    for (; resultIt.valid(); resultIt.next()) {
      auto const actualValue = resultIt.value();
      ASSERT_TRUE(actualValue.isArray());

      VPackArrayIterator scoreIt(actualValue);
      EXPECT_TRUE(2 == scoreIt.size());

      for (; scoreIt.valid(); scoreIt.next()) {
        auto const value = scoreIt.value();
        ASSERT_TRUE(value.isNumber());
        EXPECT_TRUE(3 == value.getNumber<size_t>());
      }
    }
  }

  // ensure scorers are deduplicated (indexed access)
  {
    std::string const queryString =
        "LET obj = _NONDETERM_([ 2, 5 ]) "
        "FOR d IN testView SEARCH IN_RANGE(d.name, 'A', 'C', true, true) "
        "RETURN [ customscorer(d, obj[1]), customscorer(d, obj[1]) ] ";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase, queryString,
                                             {
                                                 arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
                                             }));

    arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString(queryString),
                               std::shared_ptr<arangodb::velocypack::Builder>(),
                               arangodb::velocypack::Parser::fromJson("{}"),
                               arangodb::aql::PART_MAIN);

    query.prepare(arangodb::QueryRegistryFeature::registry());
    auto* plan = query.plan();
    ASSERT_TRUE(plan);

    arangodb::SmallVector<arangodb::aql::ExecutionNode*>::allocator_type::arena_type a;
    arangodb::SmallVector<arangodb::aql::ExecutionNode*> nodes{a};

    // only one scorer
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, true);
    ASSERT_TRUE(1 == nodes.size());
    auto* viewNode =
        arangodb::aql::ExecutionNode::castTo<arangodb::iresearch::IResearchViewNode*>(
            nodes.front());
    ASSERT_TRUE(viewNode);
    auto& scorers = viewNode->scorers();
    ASSERT_TRUE(1 == scorers.size());
    auto* var = scorers.front().var;
    ASSERT_TRUE(var);

    // check scorer
    {
      auto* expr = scorers.front().node;
      ASSERT_TRUE(expr);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_FCALL == expr->type);
      auto* fn = static_cast<arangodb::aql::Function*>(expr->getData());
      ASSERT_TRUE(fn);
      ASSERT_TRUE(arangodb::iresearch::isScorer(*fn));
      EXPECT_TRUE("CUSTOMSCORER" == fn->name);

      ASSERT_TRUE(1 == expr->numMembers());
      auto* args = expr->getMember(0);
      ASSERT_TRUE(args);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_ARRAY == args->type);
      ASSERT_TRUE(2 == args->numMembers());
      auto* arg0 = args->getMember(0);  // reference to d
      ASSERT_TRUE(arg0);
      ASSERT_TRUE(static_cast<void const*>(&viewNode->outVariable()) == arg0->getData());
      auto* arg1 = args->getMember(1);
      ASSERT_TRUE(arg1);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_INDEXED_ACCESS == arg1->type);
    }

    // and 2 references
    nodes.clear();
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::CALCULATION, true);
    ASSERT_TRUE(2 == nodes.size());
    for (auto const* node : nodes) {
      auto* calcNode =
          arangodb::aql::ExecutionNode::castTo<arangodb::aql::CalculationNode const*>(node);
      ASSERT_TRUE(calcNode);
      ASSERT_TRUE(calcNode->expression());

      if (calcNode->outVariable()->name == "obj") {
        continue;
      }

      auto* exprNode = calcNode->expression()->node();
      ASSERT_TRUE(exprNode);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_ARRAY == exprNode->type);
      ASSERT_TRUE(2 == exprNode->numMembers());
      for (size_t i = 0; i < exprNode->numMembers(); ++i) {
        auto* sub = exprNode->getMember(i);
        ASSERT_TRUE(arangodb::aql::NODE_TYPE_REFERENCE == sub->type);
        EXPECT_TRUE(static_cast<const void*>(var) == sub->getData());
      }
    }

    // check execution
    auto queryResult = arangodb::tests::executeQuery(vocbase, queryString);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_TRUE(3 == resultIt.size());

    for (; resultIt.valid(); resultIt.next()) {
      auto const actualValue = resultIt.value();
      ASSERT_TRUE(actualValue.isArray());

      VPackArrayIterator scoreIt(actualValue);
      EXPECT_TRUE(2 == scoreIt.size());

      for (; scoreIt.valid(); scoreIt.next()) {
        auto const value = scoreIt.value();
        ASSERT_TRUE(value.isNumber());
        EXPECT_TRUE(5 == value.getNumber<size_t>());
      }
    }
  }

  // ensure scorers are deduplicated (ternary)
  {
    std::string const queryString =
        "LET obj = _NONDETERM_([ 2, 5 ]) "
        "FOR d IN testView SEARCH IN_RANGE(d.name, 'A', 'C', true, true) "
        "RETURN [ customscorer(d, obj[0] > obj[1] ? 1 : 2), customscorer(d, "
        "obj[0] > obj[1] ? 1 : 2) ] ";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase, queryString,
                                             {
                                                 arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
                                             }));

    arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString(queryString),
                               std::shared_ptr<arangodb::velocypack::Builder>(),
                               arangodb::velocypack::Parser::fromJson("{}"),
                               arangodb::aql::PART_MAIN);

    query.prepare(arangodb::QueryRegistryFeature::registry());
    auto* plan = query.plan();
    ASSERT_TRUE(plan);

    arangodb::SmallVector<arangodb::aql::ExecutionNode*>::allocator_type::arena_type a;
    arangodb::SmallVector<arangodb::aql::ExecutionNode*> nodes{a};

    // only one scorer
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, true);
    ASSERT_TRUE(1 == nodes.size());
    auto* viewNode =
        arangodb::aql::ExecutionNode::castTo<arangodb::iresearch::IResearchViewNode*>(
            nodes.front());
    ASSERT_TRUE(viewNode);
    auto& scorers = viewNode->scorers();
    ASSERT_TRUE(1 == scorers.size());
    auto* var = scorers.front().var;
    ASSERT_TRUE(var);

    // check scorer
    {
      auto* expr = scorers.front().node;
      ASSERT_TRUE(expr);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_FCALL == expr->type);
      auto* fn = static_cast<arangodb::aql::Function*>(expr->getData());
      ASSERT_TRUE(fn);
      ASSERT_TRUE(arangodb::iresearch::isScorer(*fn));
      EXPECT_TRUE("CUSTOMSCORER" == fn->name);

      ASSERT_TRUE(1 == expr->numMembers());
      auto* args = expr->getMember(0);
      ASSERT_TRUE(args);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_ARRAY == args->type);
      ASSERT_TRUE(2 == args->numMembers());
      auto* arg0 = args->getMember(0);  // reference to d
      ASSERT_TRUE(arg0);
      ASSERT_TRUE(static_cast<void const*>(&viewNode->outVariable()) == arg0->getData());
      auto* arg1 = args->getMember(1);
      ASSERT_TRUE(arg1);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_OPERATOR_TERNARY == arg1->type);
    }

    // and 2 references
    nodes.clear();
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::CALCULATION, true);
    ASSERT_TRUE(2 == nodes.size());
    for (auto const* node : nodes) {
      auto* calcNode =
          arangodb::aql::ExecutionNode::castTo<arangodb::aql::CalculationNode const*>(node);
      ASSERT_TRUE(calcNode);
      ASSERT_TRUE(calcNode->expression());

      if (calcNode->outVariable()->name == "obj") {
        continue;
      }

      auto* exprNode = calcNode->expression()->node();
      ASSERT_TRUE(exprNode);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_ARRAY == exprNode->type);
      ASSERT_TRUE(2 == exprNode->numMembers());
      for (size_t i = 0; i < exprNode->numMembers(); ++i) {
        auto* sub = exprNode->getMember(i);
        ASSERT_TRUE(arangodb::aql::NODE_TYPE_REFERENCE == sub->type);
        EXPECT_TRUE(static_cast<const void*>(var) == sub->getData());
      }
    }

    // check execution
    auto queryResult = arangodb::tests::executeQuery(vocbase, queryString);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_TRUE(3 == resultIt.size());

    for (; resultIt.valid(); resultIt.next()) {
      auto const actualValue = resultIt.value();
      ASSERT_TRUE(actualValue.isArray());

      VPackArrayIterator scoreIt(actualValue);
      EXPECT_TRUE(2 == scoreIt.size());

      for (; scoreIt.valid(); scoreIt.next()) {
        auto const value = scoreIt.value();
        ASSERT_TRUE(value.isNumber());
        EXPECT_TRUE(2 == value.getNumber<size_t>());
      }
    }
  }

  // ensure scorers aren't deduplicated (ternary)
  {
    std::string const queryString =
        "LET obj = _NONDETERM_([ 2, 5 ]) "
        "FOR d IN testView SEARCH IN_RANGE(d.name, 'A', 'C', true, true) "
        "RETURN [ customscorer(d, obj[0] > obj[1] ? 1 : 2), customscorer(d, "
        "obj[1] > obj[2] ? 1 : 2) ] ";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase, queryString,
                                             {
                                                 arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
                                             }));

    arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString(queryString),
                               std::shared_ptr<arangodb::velocypack::Builder>(),
                               arangodb::velocypack::Parser::fromJson("{}"),
                               arangodb::aql::PART_MAIN);

    query.prepare(arangodb::QueryRegistryFeature::registry());
    auto* plan = query.plan();
    ASSERT_TRUE(plan);

    arangodb::SmallVector<arangodb::aql::ExecutionNode*>::allocator_type::arena_type a;
    arangodb::SmallVector<arangodb::aql::ExecutionNode*> nodes{a};

    // only one scorer
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, true);
    ASSERT_TRUE(1 == nodes.size());
    auto* viewNode =
        arangodb::aql::ExecutionNode::castTo<arangodb::iresearch::IResearchViewNode*>(
            nodes.front());
    ASSERT_TRUE(viewNode);
    auto& scorers = viewNode->scorers();
    ASSERT_TRUE(2 == scorers.size());
    auto* var = scorers.front().var;
    ASSERT_TRUE(var);

    // check scorers
    {
      auto* expr = scorers[0].node;
      ASSERT_TRUE(expr);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_FCALL == expr->type);
      auto* fn = static_cast<arangodb::aql::Function*>(expr->getData());
      ASSERT_TRUE(fn);
      ASSERT_TRUE(arangodb::iresearch::isScorer(*fn));
      EXPECT_TRUE("CUSTOMSCORER" == fn->name);

      ASSERT_TRUE(1 == expr->numMembers());
      auto* args = expr->getMember(0);
      ASSERT_TRUE(args);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_ARRAY == args->type);
      ASSERT_TRUE(2 == args->numMembers());
      auto* arg0 = args->getMember(0);  // reference to d
      ASSERT_TRUE(arg0);
      ASSERT_TRUE(static_cast<void const*>(&viewNode->outVariable()) == arg0->getData());
      auto* arg1 = args->getMember(1);
      ASSERT_TRUE(arg1);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_OPERATOR_TERNARY == arg1->type);
    }

    {
      auto* expr = scorers[1].node;
      ASSERT_TRUE(expr);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_FCALL == expr->type);
      auto* fn = static_cast<arangodb::aql::Function*>(expr->getData());
      ASSERT_TRUE(fn);
      ASSERT_TRUE(arangodb::iresearch::isScorer(*fn));
      EXPECT_TRUE("CUSTOMSCORER" == fn->name);

      ASSERT_TRUE(1 == expr->numMembers());
      auto* args = expr->getMember(0);
      ASSERT_TRUE(args);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_ARRAY == args->type);
      ASSERT_TRUE(2 == args->numMembers());
      auto* arg0 = args->getMember(0);  // reference to d
      ASSERT_TRUE(arg0);
      ASSERT_TRUE(static_cast<void const*>(&viewNode->outVariable()) == arg0->getData());
      auto* arg1 = args->getMember(1);
      ASSERT_TRUE(arg1);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_OPERATOR_TERNARY == arg1->type);
    }

    // check execution
    auto queryResult = arangodb::tests::executeQuery(vocbase, queryString);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_TRUE(3 == resultIt.size());

    for (; resultIt.valid(); resultIt.next()) {
      auto const actualValue = resultIt.value();
      ASSERT_TRUE(actualValue.isArray());

      VPackArrayIterator scoreIt(actualValue);
      EXPECT_TRUE(2 == scoreIt.size());
      ASSERT_TRUE(scoreIt.valid());

      {
        auto const value = scoreIt.value();
        ASSERT_TRUE(value.isNumber());
        EXPECT_TRUE(2 == value.getNumber<size_t>());
        scoreIt.next();
      }

      {
        auto const value = scoreIt.value();
        ASSERT_TRUE(value.isNumber());
        EXPECT_TRUE(1 == value.getNumber<size_t>());
        scoreIt.next();
      }

      ASSERT_TRUE(!scoreIt.valid());
    }
  }

  // ensure scorers are deduplicated (complex expression)
  {
    std::string const queryString =
        "LET obj = _NONDETERM_([ 2, 5 ]) "
        "FOR d IN testView SEARCH IN_RANGE(d.name, 'A', 'C', true, true) "
        "RETURN [ customscorer(d, 5*obj[0]*TO_NUMBER(obj[1] > obj[2])/obj[1] - "
        "1), customscorer(d, 5*obj[0]*TO_NUMBER(obj[1] > obj[2])/obj[1] - 1) "
        "] ";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase, queryString,
                                             {
                                                 arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
                                             }));

    arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString(queryString),
                               std::shared_ptr<arangodb::velocypack::Builder>(),
                               arangodb::velocypack::Parser::fromJson("{}"),
                               arangodb::aql::PART_MAIN);

    query.prepare(arangodb::QueryRegistryFeature::registry());
    auto* plan = query.plan();
    ASSERT_TRUE(plan);

    arangodb::SmallVector<arangodb::aql::ExecutionNode*>::allocator_type::arena_type a;
    arangodb::SmallVector<arangodb::aql::ExecutionNode*> nodes{a};

    // only one scorer
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, true);
    ASSERT_TRUE(1 == nodes.size());
    auto* viewNode =
        arangodb::aql::ExecutionNode::castTo<arangodb::iresearch::IResearchViewNode*>(
            nodes.front());
    ASSERT_TRUE(viewNode);
    auto& scorers = viewNode->scorers();
    ASSERT_TRUE(1 == scorers.size());
    auto* var = scorers.front().var;
    ASSERT_TRUE(var);

    // check scorer
    {
      auto* expr = scorers.front().node;
      ASSERT_TRUE(expr);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_FCALL == expr->type);
      auto* fn = static_cast<arangodb::aql::Function*>(expr->getData());
      ASSERT_TRUE(fn);
      ASSERT_TRUE(arangodb::iresearch::isScorer(*fn));
      EXPECT_TRUE("CUSTOMSCORER" == fn->name);

      ASSERT_TRUE(1 == expr->numMembers());
      auto* args = expr->getMember(0);
      ASSERT_TRUE(args);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_ARRAY == args->type);
      ASSERT_TRUE(2 == args->numMembers());
      auto* arg0 = args->getMember(0);  // reference to d
      ASSERT_TRUE(arg0);
      ASSERT_TRUE(static_cast<void const*>(&viewNode->outVariable()) == arg0->getData());
      auto* arg1 = args->getMember(1);
      ASSERT_TRUE(arg1);
      EXPECT_TRUE(arangodb::aql::NODE_TYPE_OPERATOR_BINARY_MINUS == arg1->type);
    }

    // and 2 references
    nodes.clear();
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::CALCULATION, true);
    ASSERT_TRUE(2 == nodes.size());
    for (auto const* node : nodes) {
      auto* calcNode =
          arangodb::aql::ExecutionNode::castTo<arangodb::aql::CalculationNode const*>(node);
      ASSERT_TRUE(calcNode);
      ASSERT_TRUE(calcNode->expression());

      if (calcNode->outVariable()->name == "obj") {
        continue;
      }

      auto* exprNode = calcNode->expression()->node();
      ASSERT_TRUE(exprNode);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_ARRAY == exprNode->type);
      ASSERT_TRUE(2 == exprNode->numMembers());
      for (size_t i = 0; i < exprNode->numMembers(); ++i) {
        auto* sub = exprNode->getMember(i);
        ASSERT_TRUE(arangodb::aql::NODE_TYPE_REFERENCE == sub->type);
        EXPECT_TRUE(static_cast<const void*>(var) == sub->getData());
      }
    }

    // check execution
    auto queryResult = arangodb::tests::executeQuery(vocbase, queryString);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_TRUE(3 == resultIt.size());

    for (; resultIt.valid(); resultIt.next()) {
      auto const actualValue = resultIt.value();
      ASSERT_TRUE(actualValue.isArray());

      VPackArrayIterator scoreIt(actualValue);
      EXPECT_TRUE(2 == scoreIt.size());

      for (; scoreIt.valid(); scoreIt.next()) {
        auto const value = scoreIt.value();
        ASSERT_TRUE(value.isNumber());
        EXPECT_TRUE(1 == value.getNumber<size_t>());
      }
    }
  }

  // ensure scorers are deduplicated (dynamic object attribute name)
  {
    std::string const queryString =
        "LET obj = _NONDETERM_([ 2, 5 ]) "
        "FOR d IN testView SEARCH IN_RANGE(d.name, 'A', 'C', true, true) "
        "RETURN [ customscorer(d, { [ CONCAT(obj[0], obj[1]) ] : 1 }), "
        "customscorer(d, { [ CONCAT(obj[0], obj[1]) ] : 1 }) ]";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase, queryString,
                                             {
                                                 arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
                                             }));

    arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString(queryString),
                               std::shared_ptr<arangodb::velocypack::Builder>(),
                               arangodb::velocypack::Parser::fromJson("{}"),
                               arangodb::aql::PART_MAIN);

    query.prepare(arangodb::QueryRegistryFeature::registry());
    auto* plan = query.plan();
    ASSERT_TRUE(plan);

    arangodb::SmallVector<arangodb::aql::ExecutionNode*>::allocator_type::arena_type a;
    arangodb::SmallVector<arangodb::aql::ExecutionNode*> nodes{a};

    // only one scorer
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, true);
    ASSERT_TRUE(1 == nodes.size());
    auto* viewNode =
        arangodb::aql::ExecutionNode::castTo<arangodb::iresearch::IResearchViewNode*>(
            nodes.front());
    ASSERT_TRUE(viewNode);
    auto& scorers = viewNode->scorers();
    ASSERT_TRUE(1 == scorers.size());
    auto* var = scorers.front().var;
    ASSERT_TRUE(var);

    // check scorer
    {
      auto* expr = scorers.front().node;
      ASSERT_TRUE(expr);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_FCALL == expr->type);
      auto* fn = static_cast<arangodb::aql::Function*>(expr->getData());
      ASSERT_TRUE(fn);
      ASSERT_TRUE(arangodb::iresearch::isScorer(*fn));
      EXPECT_TRUE("CUSTOMSCORER" == fn->name);

      ASSERT_TRUE(1 == expr->numMembers());
      auto* args = expr->getMember(0);
      ASSERT_TRUE(args);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_ARRAY == args->type);
      ASSERT_TRUE(2 == args->numMembers());
      auto* arg0 = args->getMember(0);  // reference to d
      ASSERT_TRUE(arg0);
      ASSERT_TRUE(static_cast<void const*>(&viewNode->outVariable()) == arg0->getData());
      auto* arg1 = args->getMember(1);
      ASSERT_TRUE(arg1);
      EXPECT_TRUE(arangodb::aql::NODE_TYPE_OBJECT == arg1->type);
    }

    // and 2 references
    nodes.clear();
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::CALCULATION, true);
    ASSERT_TRUE(2 == nodes.size());
    for (auto const* node : nodes) {
      auto* calcNode =
          arangodb::aql::ExecutionNode::castTo<arangodb::aql::CalculationNode const*>(node);
      ASSERT_TRUE(calcNode);
      ASSERT_TRUE(calcNode->expression());

      if (calcNode->outVariable()->name == "obj") {
        continue;
      }

      auto* exprNode = calcNode->expression()->node();
      ASSERT_TRUE(exprNode);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_ARRAY == exprNode->type);
      ASSERT_TRUE(2 == exprNode->numMembers());
      for (size_t i = 0; i < exprNode->numMembers(); ++i) {
        auto* sub = exprNode->getMember(i);
        ASSERT_TRUE(arangodb::aql::NODE_TYPE_REFERENCE == sub->type);
        EXPECT_TRUE(static_cast<const void*>(var) == sub->getData());
      }
    }
  }

  // ensure scorers are deduplicated (dynamic object value)
  {
    std::string const queryString =
        "LET obj = _NONDETERM_([ 2, 5 ]) "
        "FOR d IN testView SEARCH IN_RANGE(d.name, 'A', 'C', true, true) "
        "RETURN [ customscorer(d, { foo : obj[1] }), customscorer(d, { foo : "
        "obj[1] }) ]";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase, queryString,
                                             {
                                                 arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
                                             }));

    arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString(queryString),
                               std::shared_ptr<arangodb::velocypack::Builder>(),
                               arangodb::velocypack::Parser::fromJson("{}"),
                               arangodb::aql::PART_MAIN);

    query.prepare(arangodb::QueryRegistryFeature::registry());
    auto* plan = query.plan();
    ASSERT_TRUE(plan);

    arangodb::SmallVector<arangodb::aql::ExecutionNode*>::allocator_type::arena_type a;
    arangodb::SmallVector<arangodb::aql::ExecutionNode*> nodes{a};

    // only one scorer
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, true);
    ASSERT_TRUE(1 == nodes.size());
    auto* viewNode =
        arangodb::aql::ExecutionNode::castTo<arangodb::iresearch::IResearchViewNode*>(
            nodes.front());
    ASSERT_TRUE(viewNode);
    auto& scorers = viewNode->scorers();
    ASSERT_TRUE(1 == scorers.size());
    auto* var = scorers.front().var;
    ASSERT_TRUE(var);

    // check scorer
    {
      auto* expr = scorers.front().node;
      ASSERT_TRUE(expr);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_FCALL == expr->type);
      auto* fn = static_cast<arangodb::aql::Function*>(expr->getData());
      ASSERT_TRUE(fn);
      ASSERT_TRUE(arangodb::iresearch::isScorer(*fn));
      EXPECT_TRUE("CUSTOMSCORER" == fn->name);

      ASSERT_TRUE(1 == expr->numMembers());
      auto* args = expr->getMember(0);
      ASSERT_TRUE(args);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_ARRAY == args->type);
      ASSERT_TRUE(2 == args->numMembers());
      auto* arg0 = args->getMember(0);  // reference to d
      ASSERT_TRUE(arg0);
      ASSERT_TRUE(static_cast<void const*>(&viewNode->outVariable()) == arg0->getData());
      auto* arg1 = args->getMember(1);
      ASSERT_TRUE(arg1);
      EXPECT_TRUE(arangodb::aql::NODE_TYPE_OBJECT == arg1->type);
    }

    // and 2 references
    nodes.clear();
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::CALCULATION, true);
    ASSERT_TRUE(2 == nodes.size());
    for (auto const* node : nodes) {
      auto* calcNode =
          arangodb::aql::ExecutionNode::castTo<arangodb::aql::CalculationNode const*>(node);
      ASSERT_TRUE(calcNode);
      ASSERT_TRUE(calcNode->expression());

      if (calcNode->outVariable()->name == "obj") {
        continue;
      }

      auto* exprNode = calcNode->expression()->node();
      ASSERT_TRUE(exprNode);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_ARRAY == exprNode->type);
      ASSERT_TRUE(2 == exprNode->numMembers());
      for (size_t i = 0; i < exprNode->numMembers(); ++i) {
        auto* sub = exprNode->getMember(i);
        ASSERT_TRUE(arangodb::aql::NODE_TYPE_REFERENCE == sub->type);
        EXPECT_TRUE(static_cast<const void*>(var) == sub->getData());
      }
    }
  }

  // ensure scorers aren't deduplicated (complex expression)
  {
    std::string const queryString =
        "LET obj = _NONDETERM_([ 2, 5 ]) "
        "FOR d IN testView SEARCH IN_RANGE(d.name, 'A', 'C', true, true) "
        "RETURN [ customscorer(d, 5*obj[0]*TO_NUMBER(obj[1] > obj[2])/obj[1] - "
        "1), customscorer(d, 5*obj[0]*TO_NUMBER(obj[1] > obj[2])/obj[1] - 2) "
        "] ";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase, queryString,
                                             {
                                                 arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
                                             }));

    arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString(queryString),
                               std::shared_ptr<arangodb::velocypack::Builder>(),
                               arangodb::velocypack::Parser::fromJson("{}"),
                               arangodb::aql::PART_MAIN);

    query.prepare(arangodb::QueryRegistryFeature::registry());
    auto* plan = query.plan();
    ASSERT_TRUE(plan);

    arangodb::SmallVector<arangodb::aql::ExecutionNode*>::allocator_type::arena_type a;
    arangodb::SmallVector<arangodb::aql::ExecutionNode*> nodes{a};

    // only one scorer
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, true);
    ASSERT_TRUE(1 == nodes.size());
    auto* viewNode =
        arangodb::aql::ExecutionNode::castTo<arangodb::iresearch::IResearchViewNode*>(
            nodes.front());
    ASSERT_TRUE(viewNode);
    auto& scorers = viewNode->scorers();
    ASSERT_TRUE(2 == scorers.size());
    auto* var = scorers.front().var;
    ASSERT_TRUE(var);

    // check scorers
    {
      auto* expr = scorers[0].node;
      ASSERT_TRUE(expr);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_FCALL == expr->type);
      auto* fn = static_cast<arangodb::aql::Function*>(expr->getData());
      ASSERT_TRUE(fn);
      ASSERT_TRUE(arangodb::iresearch::isScorer(*fn));
      EXPECT_TRUE("CUSTOMSCORER" == fn->name);

      ASSERT_TRUE(1 == expr->numMembers());
      auto* args = expr->getMember(0);
      ASSERT_TRUE(args);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_ARRAY == args->type);
      ASSERT_TRUE(2 == args->numMembers());
      auto* arg0 = args->getMember(0);  // reference to d
      ASSERT_TRUE(arg0);
      ASSERT_TRUE(static_cast<void const*>(&viewNode->outVariable()) == arg0->getData());
      auto* arg1 = args->getMember(1);
      ASSERT_TRUE(arg1);
      EXPECT_TRUE(arangodb::aql::NODE_TYPE_OPERATOR_BINARY_MINUS == arg1->type);
    }

    {
      auto* expr = scorers[1].node;
      ASSERT_TRUE(expr);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_FCALL == expr->type);
      auto* fn = static_cast<arangodb::aql::Function*>(expr->getData());
      ASSERT_TRUE(fn);
      ASSERT_TRUE(arangodb::iresearch::isScorer(*fn));
      EXPECT_TRUE("CUSTOMSCORER" == fn->name);

      ASSERT_TRUE(1 == expr->numMembers());
      auto* args = expr->getMember(0);
      ASSERT_TRUE(args);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_ARRAY == args->type);
      ASSERT_TRUE(2 == args->numMembers());
      auto* arg0 = args->getMember(0);  // reference to d
      ASSERT_TRUE(arg0);
      ASSERT_TRUE(static_cast<void const*>(&viewNode->outVariable()) == arg0->getData());
      auto* arg1 = args->getMember(1);
      ASSERT_TRUE(arg1);
      EXPECT_TRUE(arangodb::aql::NODE_TYPE_OPERATOR_BINARY_MINUS == arg1->type);
    }

    // check execution
    auto queryResult = arangodb::tests::executeQuery(vocbase, queryString);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_TRUE(3 == resultIt.size());

    for (; resultIt.valid(); resultIt.next()) {
      auto const actualValue = resultIt.value();
      ASSERT_TRUE(actualValue.isArray());

      VPackArrayIterator scoreIt(actualValue);
      EXPECT_TRUE(2 == scoreIt.size());
      ASSERT_TRUE(scoreIt.valid());

      {
        auto const value = scoreIt.value();
        ASSERT_TRUE(value.isNumber());
        EXPECT_TRUE(1 == value.getNumber<size_t>());
        scoreIt.next();
      }

      {
        auto const value = scoreIt.value();
        ASSERT_TRUE(value.isNumber());
        EXPECT_TRUE(0 == value.getNumber<size_t>());
        scoreIt.next();
      }

      ASSERT_TRUE(!scoreIt.valid());
    }
  }

  // ensure scorers are deduplicated (array comparison operators)
  {
    std::string const queryString =
        "LET obj = _NONDETERM_([ 2, 5 ]) "
        "FOR d IN testView SEARCH IN_RANGE(d.name, 'A', 'C', true, true) "
        "RETURN [ customscorer(d, obj any == 3), customscorer(d, obj any == 3) "
        "]";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase, queryString,
                                             {
                                                 arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
                                             }));

    arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString(queryString),
                               std::shared_ptr<arangodb::velocypack::Builder>(),
                               arangodb::velocypack::Parser::fromJson("{}"),
                               arangodb::aql::PART_MAIN);

    query.prepare(arangodb::QueryRegistryFeature::registry());
    auto* plan = query.plan();
    ASSERT_TRUE(plan);

    arangodb::SmallVector<arangodb::aql::ExecutionNode*>::allocator_type::arena_type a;
    arangodb::SmallVector<arangodb::aql::ExecutionNode*> nodes{a};

    // only one scorer
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, true);
    ASSERT_TRUE(1 == nodes.size());
    auto* viewNode =
        arangodb::aql::ExecutionNode::castTo<arangodb::iresearch::IResearchViewNode*>(
            nodes.front());
    ASSERT_TRUE(viewNode);
    auto& scorers = viewNode->scorers();
    ASSERT_TRUE(1 == scorers.size());
    auto* var = scorers.front().var;
    ASSERT_TRUE(var);

    // check scorer
    {
      auto* expr = scorers.front().node;
      ASSERT_TRUE(expr);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_FCALL == expr->type);
      auto* fn = static_cast<arangodb::aql::Function*>(expr->getData());
      ASSERT_TRUE(fn);
      ASSERT_TRUE(arangodb::iresearch::isScorer(*fn));
      EXPECT_TRUE("CUSTOMSCORER" == fn->name);

      ASSERT_TRUE(1 == expr->numMembers());
      auto* args = expr->getMember(0);
      ASSERT_TRUE(args);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_ARRAY == args->type);
      ASSERT_TRUE(2 == args->numMembers());
      auto* arg0 = args->getMember(0);  // reference to d
      ASSERT_TRUE(arg0);
      ASSERT_TRUE(static_cast<void const*>(&viewNode->outVariable()) == arg0->getData());
      auto* arg1 = args->getMember(1);
      ASSERT_TRUE(arg1);
      EXPECT_TRUE(arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ == arg1->type);
    }

    // and 2 references
    nodes.clear();
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::CALCULATION, true);
    ASSERT_TRUE(2 == nodes.size());
    for (auto const* node : nodes) {
      auto* calcNode =
          arangodb::aql::ExecutionNode::castTo<arangodb::aql::CalculationNode const*>(node);
      ASSERT_TRUE(calcNode);
      ASSERT_TRUE(calcNode->expression());

      if (calcNode->outVariable()->name == "obj") {
        continue;
      }

      auto* exprNode = calcNode->expression()->node();
      ASSERT_TRUE(exprNode);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_ARRAY == exprNode->type);
      ASSERT_TRUE(2 == exprNode->numMembers());
      for (size_t i = 0; i < exprNode->numMembers(); ++i) {
        auto* sub = exprNode->getMember(i);
        ASSERT_TRUE(arangodb::aql::NODE_TYPE_REFERENCE == sub->type);
        EXPECT_TRUE(static_cast<const void*>(var) == sub->getData());
      }
    }
  }

  // ensure scorers aren't deduplicated (array comparison operator)
  {
    std::string const queryString =
        "LET obj = _NONDETERM_([ 2, 5 ]) "
        "FOR d IN testView SEARCH IN_RANGE(d.name, 'A', 'C', true, true) "
        "RETURN [ customscorer(d, obj any == 3), customscorer(d, obj all == 3) "
        "]";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase, queryString,
                                             {
                                                 arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
                                             }));

    arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString(queryString),
                               std::shared_ptr<arangodb::velocypack::Builder>(),
                               arangodb::velocypack::Parser::fromJson("{}"),
                               arangodb::aql::PART_MAIN);

    query.prepare(arangodb::QueryRegistryFeature::registry());
    auto* plan = query.plan();
    ASSERT_TRUE(plan);

    arangodb::SmallVector<arangodb::aql::ExecutionNode*>::allocator_type::arena_type a;
    arangodb::SmallVector<arangodb::aql::ExecutionNode*> nodes{a};

    // only one scorer
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, true);
    ASSERT_TRUE(1 == nodes.size());
    auto* viewNode =
        arangodb::aql::ExecutionNode::castTo<arangodb::iresearch::IResearchViewNode*>(
            nodes.front());
    ASSERT_TRUE(viewNode);
    auto& scorers = viewNode->scorers();
    ASSERT_TRUE(2 == scorers.size());
    auto* var = scorers.front().var;
    ASSERT_TRUE(var);

    // check scorers
    {
      auto* expr = scorers[0].node;
      ASSERT_TRUE(expr);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_FCALL == expr->type);
      auto* fn = static_cast<arangodb::aql::Function*>(expr->getData());
      ASSERT_TRUE(fn);
      ASSERT_TRUE(arangodb::iresearch::isScorer(*fn));
      EXPECT_TRUE("CUSTOMSCORER" == fn->name);

      ASSERT_TRUE(1 == expr->numMembers());
      auto* args = expr->getMember(0);
      ASSERT_TRUE(args);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_ARRAY == args->type);
      ASSERT_TRUE(2 == args->numMembers());
      auto* arg0 = args->getMember(0);  // reference to d
      ASSERT_TRUE(arg0);
      ASSERT_TRUE(static_cast<void const*>(&viewNode->outVariable()) == arg0->getData());
      auto* arg1 = args->getMember(1);
      ASSERT_TRUE(arg1);
      EXPECT_TRUE(arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ == arg1->type);
    }

    {
      auto* expr = scorers[1].node;
      ASSERT_TRUE(expr);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_FCALL == expr->type);
      auto* fn = static_cast<arangodb::aql::Function*>(expr->getData());
      ASSERT_TRUE(fn);
      ASSERT_TRUE(arangodb::iresearch::isScorer(*fn));
      EXPECT_TRUE("CUSTOMSCORER" == fn->name);

      ASSERT_TRUE(1 == expr->numMembers());
      auto* args = expr->getMember(0);
      ASSERT_TRUE(args);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_ARRAY == args->type);
      ASSERT_TRUE(2 == args->numMembers());
      auto* arg0 = args->getMember(0);  // reference to d
      ASSERT_TRUE(arg0);
      ASSERT_TRUE(static_cast<void const*>(&viewNode->outVariable()) == arg0->getData());
      auto* arg1 = args->getMember(1);
      ASSERT_TRUE(arg1);
      EXPECT_TRUE(arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ == arg1->type);
    }
  }

  // con't deduplicate scorers with default values
  {
    std::string const queryString =
        "FOR d IN testView SEARCH IN_RANGE(d.name, 'A', 'C', true, true) "
        "RETURN [ tfidf(d), tfidf(d, false) ] ";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase, queryString,
                                             {
                                                 arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
                                             }));

    arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString(queryString),
                               std::shared_ptr<arangodb::velocypack::Builder>(),
                               arangodb::velocypack::Parser::fromJson("{}"),
                               arangodb::aql::PART_MAIN);

    query.prepare(arangodb::QueryRegistryFeature::registry());
    auto* plan = query.plan();
    ASSERT_TRUE(plan);

    arangodb::SmallVector<arangodb::aql::ExecutionNode*>::allocator_type::arena_type a;
    arangodb::SmallVector<arangodb::aql::ExecutionNode*> nodes{a};

    // 2 scorers scorer
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, true);
    ASSERT_TRUE(1 == nodes.size());
    auto* viewNode =
        arangodb::aql::ExecutionNode::castTo<arangodb::iresearch::IResearchViewNode*>(
            nodes.front());
    ASSERT_TRUE(viewNode);
    auto scorers = viewNode->scorers();
    std::sort(
        scorers.begin(), scorers.end(), [](auto const& lhs, auto const& rhs) noexcept {
          return lhs.var->name < rhs.var->name;
        });

    // check "tfidf(d)" scorer
    {
      auto* expr = scorers[0].node;
      ASSERT_TRUE(expr);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_FCALL == expr->type);
      auto* fn = static_cast<arangodb::aql::Function*>(expr->getData());
      ASSERT_TRUE(fn);
      ASSERT_TRUE(arangodb::iresearch::isScorer(*fn));
      EXPECT_TRUE("TFIDF" == fn->name);

      ASSERT_TRUE(1 == expr->numMembers());
      auto* args = expr->getMember(0);
      ASSERT_TRUE(args);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_ARRAY == args->type);
      ASSERT_TRUE(1 == args->numMembers());
      auto* arg0 = args->getMember(0);  // reference to d
      ASSERT_TRUE(arg0);
      ASSERT_TRUE(static_cast<void const*>(&viewNode->outVariable()) == arg0->getData());
    }

    // check "tfidf(d, false)" scorer
    {
      auto* expr = scorers[1].node;
      ASSERT_TRUE(expr);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_FCALL == expr->type);
      auto* fn = static_cast<arangodb::aql::Function*>(expr->getData());
      ASSERT_TRUE(fn);
      ASSERT_TRUE(arangodb::iresearch::isScorer(*fn));
      EXPECT_TRUE("TFIDF" == fn->name);

      ASSERT_TRUE(1 == expr->numMembers());
      auto* args = expr->getMember(0);
      ASSERT_TRUE(args);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_ARRAY == args->type);
      ASSERT_TRUE(2 == args->numMembers());
      auto* arg0 = args->getMember(0);  // reference to d
      ASSERT_TRUE(arg0);
      ASSERT_TRUE(static_cast<void const*>(&viewNode->outVariable()) == arg0->getData());
      auto* arg1 = args->getMember(1);
      ASSERT_TRUE(arg1);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_VALUE == arg1->type);
      ASSERT_TRUE(arangodb::aql::VALUE_TYPE_BOOL == arg1->value.type);
      ASSERT_TRUE(false == arg1->getBoolValue());
    }

    // and 2 references
    nodes.clear();
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::CALCULATION, true);
    ASSERT_TRUE(1 == nodes.size());
    auto* calcNode =
        arangodb::aql::ExecutionNode::castTo<arangodb::aql::CalculationNode*>(
            nodes.front());
    ASSERT_TRUE(calcNode);
    ASSERT_TRUE(calcNode->expression());
    auto* node = calcNode->expression()->node();
    ASSERT_TRUE(node);
    ASSERT_TRUE(arangodb::aql::NODE_TYPE_ARRAY == node->type);
    ASSERT_TRUE(2 == node->numMembers());

    for (size_t i = 0; i < node->numMembers(); ++i) {
      auto* sub = node->getMember(i);
      ASSERT_TRUE(arangodb::aql::NODE_TYPE_REFERENCE == sub->type);
      EXPECT_TRUE(static_cast<const void*>(scorers[i].var) == sub->getData());
    }
  }
}

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
#include "Aql/ExecutionPlan.h"
#include "Aql/ExpressionContext.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Aql/Query.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterFeature.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/ApplicationServerHelper.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchFilterFactory.h"
#include "IResearch/IResearchLink.h"
#include "IResearch/IResearchLinkHelper.h"
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

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class IResearchQueryJoinTest : public ::testing::Test {
 protected:
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  IResearchQueryJoinTest() : engine(server), server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;
    arangodb::aql::AqlFunctionFeature* functions = nullptr;

    arangodb::tests::init(true);

    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    // suppress WARNING {authentication} --server.jwt-secret is insecure. Use --server.jwt-secret-keyfile instead
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(),
                                    arangodb::LogLevel::ERR);

    // suppress log messages since tests check error conditions
    features.emplace_back(new arangodb::FlushFeature(server), false);
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

    auto const databases = VPackParser::fromJson(
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

    dbFeature->createDatabase(1, "testVocbase", vocbase);  // required for IResearchAnalyzerFeature::emplace(...)
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

  ~IResearchQueryJoinTest() {
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

TEST_F(IResearchQueryJoinTest, Subquery) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                        "testVocbase");

  std::shared_ptr<arangodb::LogicalCollection> entities;
  std::shared_ptr<arangodb::LogicalCollection> links;
  std::shared_ptr<arangodb::LogicalView> entities_view;
  std::shared_ptr<arangodb::LogicalView> links_view;

  // entities collection
  {
    auto json =
        VPackParser::fromJson("{ \"name\": \"entities\" }");
    entities = vocbase.createCollection(json->slice());
    ASSERT_TRUE((nullptr != entities));
  }

  // links collection
  {
    auto json = VPackParser::fromJson(
        "{ \"name\": \"links\", \"type\": 3 }");
    links = vocbase.createCollection(json->slice());
    ASSERT_TRUE((nullptr != links));
  }

  // entities view
  {
    auto json = VPackParser::fromJson(
        "{ \"name\" : \"entities_view\", \"writebufferSizeMax\": 33554432, "
        "\"consolidationPolicy\": { \"type\": \"bytes_accum\", \"threshold\": "
        "0.10000000149011612 }, \"globallyUniqueId\": \"hB4A95C21732A/218\", "
        "\"id\": \"218\", \"writebufferActive\": 0, "
        "\"consolidationIntervalMsec\": 60000, \"cleanupIntervalStep\": 10, "
        "\"links\": { \"entities\": { \"analyzers\": [ \"identity\" ], "
        "\"fields\": {}, \"includeAllFields\": true, \"storeValues\": \"id\", "
        "\"trackListPositions\": false } }, \"type\": \"arangosearch\", "
        "\"writebufferIdle\": 64 }");
    ASSERT_TRUE(
        arangodb::LogicalView::create(entities_view, vocbase, json->slice()).ok());
    ASSERT_TRUE((nullptr != entities_view));
  }

  // links view
  {
    auto json = VPackParser::fromJson(
        "{ \"name\" : \"links_view\", \"writebufferSizeMax\": 33554432, "
        "\"consolidationPolicy\": { \"type\": \"bytes_accum\", \"threshold\": "
        "0.10000000149011612 }, \"globallyUniqueId\": \"hB4A95C21732A/181\", "
        "\"id\": \"181\", \"writebufferActive\": 0, "
        "\"consolidationIntervalMsec\": 60000, \"cleanupIntervalStep\": 10, "
        "\"links\": { \"links\": { \"analyzers\": [ \"identity\" ], "
        "\"fields\": {}, \"includeAllFields\": true, \"storeValues\": \"id\", "
        "\"trackListPositions\": false } }, \"type\": \"arangosearch\", "
        "\"writebufferIdle\": 64 }");
    ASSERT_TRUE(
        arangodb::LogicalView::create(links_view, vocbase, json->slice()).ok());
    ASSERT_TRUE((nullptr != links_view));
  }

  std::vector<std::string> const collections{"entities", "links"};

  // populate views with the data
  {
    arangodb::OperationOptions opt;

    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                       collections, collections, collections,
                                       arangodb::transaction::Options());
    EXPECT_TRUE((trx.begin().ok()));

    // insert into entities collection
    {
      auto builder = VPackParser::fromJson(
          "[{ \"_key\": \"person1\", \"_id\": \"entities/person1\", \"_rev\": "
          "\"_YOr40eu--_\", \"type\": \"person\", \"id\": \"person1\" },"
          " { \"_key\": \"person5\", \"_id\": \"entities/person5\", \"_rev\": "
          "\"_YOr48rO---\", \"type\": \"person\", \"id\": \"person5\" },"
          " { \"_key\": \"person4\", \"_id\": \"entities/person4\", \"_rev\": "
          "\"_YOr5IGu--_\", \"type\": \"person\", \"id\": \"person4\" },"
          " { \"_key\": \"person3\", \"_id\": \"entities/person3\", \"_rev\": "
          "\"_YOr5PBK--_\", \"type\": \"person\", \"id\": \"person3\" }, "
          " { \"_key\": \"person2\", \"_id\": \"entities/person2\", \"_rev\": "
          "\"_YOr5Umq--_\", \"type\": \"person\", \"id\": \"person2\" } ]");

      auto root = builder->slice();
      ASSERT_TRUE(root.isArray());

      arangodb::ManagedDocumentResult mmdr;
      for (auto doc : arangodb::velocypack::ArrayIterator(root)) {
        auto const res = entities->insert(&trx, doc, mmdr, opt, false);
        EXPECT_TRUE(res.ok());
      }
    }

    // insert into links collection
    {
      auto builder = VPackParser::fromJson(
          "[ { \"_key\": \"3301\", \"_id\": \"links/3301\", \"_from\": "
          "\"entities/person1\", \"_to\": \"entities/person2\", \"_rev\": "
          "\"_YOrbp_S--_\", \"type\": \"relationship\", \"subType\": "
          "\"married\", \"from\": \"person1\", \"to\": \"person2\" },"
          "  { \"_key\": \"3377\", \"_id\": \"links/3377\", \"_from\": "
          "\"entities/person4\", \"_to\": \"entities/person5\", \"_rev\": "
          "\"_YOrbxN2--_\", \"type\": \"relationship\", \"subType\": "
          "\"married\", \"from\": \"person4\", \"to\": \"person5\" },"
          "  { \"_key\": \"3346\", \"_id\": \"links/3346\", \"_from\": "
          "\"entities/person1\", \"_to\": \"entities/person3\", \"_rev\": "
          "\"_YOrb4kq--_\", \"type\": \"relationship\", \"subType\": "
          "\"married\", \"from\": \"person1\", \"to\": \"person3\" }]");

      auto root = builder->slice();
      ASSERT_TRUE(root.isArray());

      arangodb::ManagedDocumentResult mmdr;
      for (auto doc : arangodb::velocypack::ArrayIterator(root)) {
        auto const res = links->insert(&trx, doc, mmdr, opt, false);
        EXPECT_TRUE(res.ok());
      }
    }

    EXPECT_TRUE((trx.commit().ok()));
    EXPECT_TRUE((arangodb::iresearch::IResearchLinkHelper::find(*entities, *entities_view)
                     ->commit()
                     .ok()));
    EXPECT_TRUE((arangodb::iresearch::IResearchLinkHelper::find(*links, *links_view)
                     ->commit()
                     .ok()));
  }

  // check query
  {
    auto expectedResultBuilder = VPackParser::fromJson(
        "[ { \"id\": \"person1\", \"marriedIds\": [\"person2\", \"person3\"] },"
        "  { \"id\": \"person2\", \"marriedIds\": [\"person1\" ] },"
        "  { \"id\": \"person3\", \"marriedIds\": [\"person1\" ] },"
        "  { \"id\": \"person4\", \"marriedIds\": [\"person5\" ] },"
        "  { \"id\": \"person5\", \"marriedIds\": [\"person4\" ] } ]");

    std::string const query =
        "FOR org IN entities_view SEARCH org.type == 'person' "
        "LET marriedIds = ("
        "LET entityIds = ("
        "  FOR l IN links_view SEARCH l.type == 'relationship' AND l.subType "
        "== 'married' AND (l.from == org.id OR l.to == org.id)"
        "  RETURN DISTINCT l.from == org.id ? l.to : l.from"
        ") "
        "FOR entityId IN entityIds SORT entityId RETURN entityId "
        ") "
        "LIMIT 10 "
        "SORT org._key "
        "RETURN { id: org._key, marriedIds: marriedIds }";

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    auto expectedResult = expectedResultBuilder->slice();
    EXPECT_TRUE(expectedResult.isArray());

    arangodb::velocypack::ArrayIterator expectedResultIt(expectedResult);
    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_TRUE(expectedResultIt.size() == resultIt.size());

    // Check documents
    for (; resultIt.valid(); resultIt.next(), expectedResultIt.next()) {
      ASSERT_TRUE(expectedResultIt.valid());
      auto const expectedDoc = expectedResultIt.value();
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                            arangodb::velocypack::Slice(expectedDoc), resolved, true)));
    }
    EXPECT_TRUE(!expectedResultIt.valid());
  }
}

TEST_F(IResearchQueryJoinTest, DuplicateDataSource) {
  static std::vector<std::string> const EMPTY;

  auto createJson = VPackParser::fromJson(
      "{ \
    \"name\": \"testView\", \
    \"type\": \"arangosearch\" \
  }");

  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                        "testVocbase");
  std::shared_ptr<arangodb::LogicalCollection> logicalCollection1;
  std::shared_ptr<arangodb::LogicalCollection> logicalCollection2;
  std::shared_ptr<arangodb::LogicalCollection> logicalCollection3;

  // add collection_1
  {
    auto collectionJson = VPackParser::fromJson(
        "{ \"name\": \"collection_1\" }");
    logicalCollection1 = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection1));
  }

  // add collection_2
  {
    auto collectionJson = VPackParser::fromJson(
        "{ \"name\": \"collection_2\" }");
    logicalCollection2 = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection2));
  }

  // add collection_3
  {
    auto collectionJson = VPackParser::fromJson(
        "{ \"name\": \"collection_3\" }");
    logicalCollection3 = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection3));
  }

  // add view
  auto view = std::dynamic_pointer_cast<arangodb::iresearch::IResearchView>(
      vocbase.createView(createJson->slice()));
  ASSERT_TRUE((false == !view));

  // add logical collection with the same name as view
  {
    auto collectionJson =
        VPackParser::fromJson("{ \"name\": \"testView\" }");
    // TRI_vocbase_t::createCollection(...) throws exception instead of returning a nullptr
    EXPECT_ANY_THROW(vocbase.createCollection(collectionJson->slice()));
  }

  // add link to collection
  {
    auto updateJson = VPackParser::fromJson(
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

  // using search keyword for collection is prohibited
  {
    std::string const query =
        "LET c=5 FOR x IN collection_1 SEARCH x.seq == c RETURN x";
    auto const boundParameters = VPackParser::fromJson("{ }");

    // arangodb::aql::ExecutionPlan::fromNodeFor(...) throws TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND
    auto queryResult = arangodb::tests::executeQuery(vocbase, query, boundParameters);
    EXPECT_TRUE(queryResult.result.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND));
  }

  // using search keyword for bound collection is prohibited
  {
    std::string const query =
        "LET c=5 FOR x IN @@dataSource SEARCH x.seq == c  RETURN x";
    auto const boundParameters = VPackParser::fromJson(
        "{ \"@dataSource\" : \"collection_1\" }");
    auto queryResult = arangodb::tests::executeQuery(vocbase, query, boundParameters);
    EXPECT_TRUE(queryResult.result.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND));
  }
}

TEST_F(IResearchQueryJoinTest, test) {
  static std::vector<std::string> const EMPTY;

  auto createJson = VPackParser::fromJson(
      "{ \
    \"name\": \"testView\", \
    \"type\": \"arangosearch\" \
  }");

  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                        "testVocbase");
  std::shared_ptr<arangodb::LogicalCollection> logicalCollection1;
  std::shared_ptr<arangodb::LogicalCollection> logicalCollection2;
  std::shared_ptr<arangodb::LogicalCollection> logicalCollection3;

  // add collection_1
  {
    auto collectionJson = VPackParser::fromJson(
        "{ \"name\": \"collection_1\" }");
    logicalCollection1 = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection1));
  }

  // add collection_2
  {
    auto collectionJson = VPackParser::fromJson(
        "{ \"name\": \"collection_2\" }");
    logicalCollection2 = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection2));
  }

  // add collection_3
  {
    auto collectionJson = VPackParser::fromJson(
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
    auto updateJson = VPackParser::fromJson(
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

  // deterministic filter condition in a loop
  // (should not recreate view iterator each loop iteration, `reset` instead)
  //
  // LET c=5
  // FOR x IN 1..7
  //   FOR d IN testView
  //   SEARCH c == x.seq
  // RETURN d;
  {
    std::string const query =
        "LET c=5 FOR x IN 1..7 FOR d IN testView SEARCH c == d.seq RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase, query,
                                             {
                                                 arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
                                             }));

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_TRUE(expectedDocs.size() == resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                            arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_TRUE(expectedDoc == expectedDocs.end());
  }

  // non deterministic filter condition in a loop
  // (must recreate view iterator each loop iteration)
  //
  // FOR x IN 1..7
  //   FOR d IN testView
  //   SEARCH _FORWARD_(5) == x.seq
  // RETURN d;
  {
    std::string const query =
        "FOR x IN 1..7 FOR d IN testView SEARCH _FORWARD_(5) == d.seq RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase, query,
                                             {
                                                 arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
                                             }));

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_TRUE(expectedDocs.size() == resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                            arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_TRUE(expectedDoc == expectedDocs.end());
  }

  // non deterministic filter condition with self-reference in a loop
  // (must recreate view iterator each loop iteration)
  //
  // FOR x IN 1..7
  //   FOR d IN testView
  //   SEARCH _NONDETERM_(5) == x.seq
  // RETURN d;
  {
    std::string const query =
        "FOR x IN 1..7 FOR d IN testView SEARCH _NONDETERM_(5) == d.seq RETURN "
        "d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase, query,
                                             {
                                                 arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
                                             }));

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.is(TRI_ERROR_NOT_IMPLEMENTED));  // can't handle self-referenced variable now

    //    auto result = queryResult.data->slice();
    //    EXPECT_TRUE(result.isArray());
    //
    //    arangodb::velocypack::ArrayIterator resultIt(result);
    //    ASSERT_TRUE(expectedDocs.size() == resultIt.size());
    //
    //    // Check documents
    //    auto expectedDoc = expectedDocs.begin();
    //    for (;resultIt.valid(); resultIt.next(), ++expectedDoc) {
    //      auto const actualDoc = resultIt.value();
    //      auto const resolved = actualDoc.resolveExternals();
    //
    //      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    //    }
    //    EXPECT_TRUE(expectedDoc == expectedDocs.end());
  }

  // nondeterministic filter condition in a loop
  // (must recreate view iterator each loop iteration)
  //
  // LET c=_NONDETERM_(4)
  // FOR x IN 1..7
  //   FOR d IN testView
  //   SEARCH c == x.seq
  // RETURN d;
  {
    std::string const query =
        "LET c=_NONDETERM_(4) FOR x IN 1..7 FOR d IN testView SEARCH c == "
        "d.seq RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase, query,
                                             {
                                                 arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
                                             }));

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocsView[4].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[4].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[4].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[4].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[4].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[4].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[4].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_TRUE(expectedDocs.size() == resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                            arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_TRUE(expectedDoc == expectedDocs.end());
  }

  // nondeterministic range
  // (must recreate view iterator each loop iteration)
  //
  // LET range=_NONDETERM_(0).._NONDETERM_(7)
  // FOR x IN range
  //   FOR d IN testView
  //   SEARCH d.seq == x.seq
  // RETURN d;
  {
    std::string const query =
        " FOR x IN _NONDETERM_(0).._NONDETERM_(7) FOR d IN testView SEARCH x "
        "== d.seq RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase, query,
                                             {
                                                 arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
                                             }));

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocsView[0].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[1].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[2].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[3].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[4].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[6].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[7].vpack())};

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_TRUE(expectedDocs.size() == resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                            arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_TRUE(expectedDoc == expectedDocs.end());
  }

  // FOR x IN collection_3
  //   FOR d IN testView
  //   SEARCH d.seq == x.seq
  // RETURN d;
  {
    std::string const query =
        "FOR x IN collection_3 FOR d IN testView SEARCH x.seq == d.seq RETURN "
        "d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase, query,
                                             {
                                                 arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
                                             }));

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocsView[0].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[1].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[2].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[3].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[4].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[6].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[7].vpack())};

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_TRUE(expectedDocs.size() == resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                            arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_TRUE(expectedDoc == expectedDocs.end());
  }

  // FOR x IN collection_3
  //   FOR d IN testView
  //   SEARCH d.seq == x.seq
  // SORT d.seq DESC
  // RETURN d;
  {
    std::string const query =
        "FOR x IN collection_3 FOR d IN testView SEARCH x.seq == d.seq SORT "
        "d.seq DESC RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase, query,
                                             {
                                                 arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
                                             }));

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocsView[7].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[6].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[4].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[3].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[2].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[1].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[0].vpack())};

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_TRUE(expectedDocs.size() == resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                            arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_TRUE(expectedDoc == expectedDocs.end());
  }

  // FOR x IN collection_3
  //   FOR d IN testView
  //   SEARCH d.seq == x.seq
  // SORT d.seq DESC
  // LIMIT 3
  // RETURN d;
  {
    std::string const query =
        "FOR x IN collection_3 FOR d IN testView SEARCH x.seq == d.seq SORT "
        "d.seq DESC LIMIT 3 RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase, query,
                                             {
                                                 arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
                                             }));

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocsView[7].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[6].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[5].vpack())};

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_TRUE(expectedDocs.size() == resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                            arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_TRUE(expectedDoc == expectedDocs.end());
  }

  // FOR x IN collection_3
  //   FOR d IN testView
  //   SEARCH d.seq == x.seq && (d.value > 5 && d.value <= 100)
  // RETURN d;
  {
    std::string const query =
        "FOR x IN collection_3 FOR d IN testView SEARCH x.seq == d.seq && "
        "(d.value > 5 && d.value <= 100) SORT d.seq DESC RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase, query,
                                             {
                                                 arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
                                             }));

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocsView[6].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[4].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[3].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[0].vpack())};

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_TRUE(expectedDocs.size() == resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                            arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_TRUE(expectedDoc == expectedDocs.end());
  }

  // FOR x IN collection_3
  //   FOR d IN testView
  //   SEARCH d.seq == x.seq
  //   SORT BM25(d) ASC, d.seq DESC
  // RETURN d;
  {
    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocsView[7].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[6].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[4].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[3].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[2].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[1].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[0].vpack())};

    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR x IN collection_3 FOR d IN testView SEARCH x.seq == d.seq SORT "
        "BM25(d) ASC, d.seq DESC RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_TRUE(expectedDocs.size() == resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                            arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_TRUE(expectedDoc == expectedDocs.end());
  }

  // Note: unable to push condition to the `View` now
  // FOR d IN testView
  //   FOR x IN collection_3
  //   SEARCH d.seq == x.seq
  // RETURN d;
  {
    std::string const query =
        "FOR d IN testView FOR x IN collection_3 FILTER d.seq == x.seq SORT "
        "d.seq RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase, query, {}));

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocsView[0].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[1].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[2].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[3].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[4].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[6].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[7].vpack())};

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_TRUE(expectedDocs.size() == resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                            arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_TRUE(expectedDoc == expectedDocs.end());
  }

  // Note: unable to push condition to the `View` now
  // FOR d IN testView
  //   FOR x IN collection_3
  //   SEARCH d.seq == x.seq && d.name == 'B'
  // RETURN d;
  {
    std::string const query =
        "FOR d IN testView FOR x IN collection_3 FILTER d.seq == x.seq && "
        "d.name == 'B' RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase, query, {}));

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocsView[1].vpack())};

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_TRUE(expectedDocs.size() == resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                            arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_TRUE(expectedDoc == expectedDocs.end());
  }

  // Note: unable to push condition to the `View` now
  // FOR d IN (FOR c IN testView SEARCH c.name >= 'E' && c.seq < 10 RETURN c)
  //   FOR x IN collection_3
  //   SEARCH d.seq == x.seq
  // RETURN d;
  {
    std::string const query =
        "FOR d IN (FOR c IN testView SEARCH c.name >= 'E' && c.seq < 10 RETURN "
        "c) FOR x IN collection_3 FILTER d.seq == x.seq SORT d.seq RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase, query,
                                             {
                                                 arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
                                             }));

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocsView[4].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[6].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[7].vpack())};

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_TRUE(expectedDocs.size() == resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                            arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_TRUE(expectedDoc == expectedDocs.end());
  }

  // Note: unable to push condition to the `View` now
  // FOR d IN (FOR c IN testView SEARCH c.name >= 'E' && c.seq < 10 SORT TFIDF(c) ASC, c.seq DESC RETURN c)
  //   FOR x IN collection_3
  //   SEARCH d.seq == x.seq
  // RETURN d;
  {
    std::string const query =
        "FOR d IN (FOR c IN testView SEARCH c.name >= 'E' && c.seq < 10 SORT "
        "TFIDF(c) ASC, c.seq DESC RETURN c) FOR x IN collection_3 FILTER d.seq "
        "== x.seq RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase, query,
                                             {
                                                 arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
                                             }));

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocsView[7].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[6].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[4].vpack())};

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_TRUE(expectedDocs.size() == resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                            arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_TRUE(expectedDoc == expectedDocs.end());
  }

  // Note: unable to push condition to the `View` now
  // FOR d IN (FOR c IN testView SEARCH c.name >= 'E' && c.seq < 10 SORT TFIDF(c) ASC, c.seq DESC RETURN c)
  //   FOR x IN collection_3
  //   SEARCH d.seq == x.seq
  // LIMIT 2
  // RETURN d;
  {
    std::string const query =
        "FOR d IN (FOR c IN testView SEARCH c.name >= 'E' && c.seq < 10 SORT "
        "TFIDF(c) ASC, c.seq DESC RETURN c) FOR x IN collection_3 FILTER d.seq "
        "== x.seq LIMIT 2 RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase, query,
                                             {
                                                 arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
                                             }));

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocsView[7].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[6].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_TRUE(expectedDocs.size() == resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                            arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_TRUE(expectedDoc == expectedDocs.end());
  }

  // Note: unable to push condition to the `View` now
  // FOR d IN (FOR c IN testView SEARCH c.name >= 'E' && c.seq < 10 SORT TFIDF(c) ASC, c.seq DESC LIMIT 3 RETURN c)
  //   FOR x IN collection_3
  //   SEARCH d.seq == x.seq
  // RETURN d;
  {
    std::string const query =
        "FOR d IN (FOR c IN testView SEARCH c.name >= 'E' && c.seq < 10 SORT "
        "TFIDF(c) ASC, c.seq DESC LIMIT 5 RETURN c) FOR x IN collection_3 "
        "FILTER d.seq == x.seq RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(
        vocbase, query, {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule}));

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocsView[7].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[6].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[5].vpack())};

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_TRUE(expectedDocs.size() == resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                            arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_TRUE(expectedDoc == expectedDocs.end());
  }

  // Invalid bound collection name
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN (FOR c IN testView SEARCH c.name >= 'E' && c.seq < 10 SORT "
        "TFIDF(c) ASC, c.seq DESC LIMIT 5 RETURN c) FOR x IN @@collection "
        "SEARCH d.seq == x.seq RETURN d",
        VPackParser::fromJson(
            "{ \"@collection\": \"invlaidCollectionName\" }"));

    ASSERT_TRUE(queryResult.result.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND));
  }

  // dependent sort condition in inner loop + custom scorer
  // (must recreate view iterator each loop iteration)
  //
  // FOR x IN 0..5
  //   FOR d IN testView
  //   SEARCH d.seq == x
  //   SORT customscorer(d,x)
  // RETURN d;
  {
    std::string const query =
        "FOR x IN 0..5 FOR d IN testView SEARCH d.seq == x SORT "
        "customscorer(d, x) DESC RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase, query,
                                             {
                                                 arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
                                             }));

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[4].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[3].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[2].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[1].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_TRUE(expectedDocs.size() == resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                            arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_TRUE(expectedDoc == expectedDocs.end());
  }

  // invalid reference in scorer
  {
    std::string const query =
        "FOR d IN testView FOR i IN 0..5 SORT tfidf(i) DESC RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase, query, {}));

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.is(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH));
  }

  // FOR i IN 1..5
  //  FOR x IN collection_0
  //    FOR d IN  SEARCH d.seq == i && d.name == x.name
  // SORT customscorer(d, x.seq)
  {
    std::string const query =
        "FOR i IN 1..5 FOR x IN collection_1 FOR d IN testView SEARCH d.seq == "
        "i AND d.name == x.name SORT customscorer(d, x.seq) DESC RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase, query,
                                             {
                                                 arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
                                             }));

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocsView[4].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[2].vpack())};

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_TRUE(expectedDocs.size() == resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                            arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_TRUE(expectedDoc == expectedDocs.end());
  }

  // FOR i IN 1..5
  //  FOR x IN collection_0 SEARCH x.seq == i
  //    FOR d IN  SEARCH d.seq == x.seq && d.name == x.name
  // SORT customscorer(d, x.seq)
  {
    std::string const query =
        "FOR i IN 1..5 FOR x IN collection_1 FILTER x.seq == i FOR d IN "
        "testView SEARCH d.seq == x.seq AND d.name == x.name SORT "
        "customscorer(d, x.seq) DESC RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase, query,
                                             {
                                                 arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
                                             }));

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocsView[4].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[2].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_TRUE(expectedDocs.size() == resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                            arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_TRUE(expectedDoc == expectedDocs.end());
  }

  {
    std::string const query =
        "LET attr = _NONDETERM_('seq') "
        "FOR i IN 1..5 "
        "  FOR x IN collection_1 FILTER x.seq == i "
        "    FOR d IN testView SEARCH d.seq == x.seq AND d.name == x.name "
        "      SORT customscorer(d, x[attr]) DESC "
        "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase, query,
                                             {
                                                 arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
                                             }));

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocsView[4].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[2].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_TRUE(expectedDocs.size() == resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                            arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_TRUE(expectedDoc == expectedDocs.end());
  }

  // FOR i IN 1..5
  //  FOR x IN collection_0 SEARCH x.seq == i
  //    FOR d IN  SEARCH d.seq == x.seq && d.name == x.name
  // SORT customscorer(d, x.seq)
  {
    std::string const query =
        "FOR i IN 1..5 FOR x IN collection_1 FILTER x.seq == i FOR d IN "
        "testView SEARCH d.seq == x.seq AND d.name == x.name SORT "
        "customscorer(d, x['seq']) DESC RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase, query,
                                             {
                                                 arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
                                             }));

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocsView[4].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[2].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_TRUE(expectedDocs.size() == resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                            arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_TRUE(expectedDoc == expectedDocs.end());
  }

  // unable to retrieve `d.seq` from self-referenced variable
  // FOR i IN 1..5
  //  FOR d IN  SEARCH d.seq == i SORT customscorer(d, d.seq)
  //    FOR x IN collection_0 SEARCH x.seq == d.seq && x.name == d.name
  // SORT customscorer(d, d.seq) DESC
  {
    std::string const query =
        "FOR i IN 1..5 FOR d IN testView SEARCH d.seq == i FOR x IN "
        "collection_1 FILTER x.seq == d.seq && x.name == d.name SORT "
        "customscorer(d, d.seq) DESC RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase, query,
                                             {
                                                 arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
                                             }));

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // unable to retrieve `x.seq` from inner loop
  // FOR i IN 1..5
  //  FOR d IN  SEARCH d.seq == i SORT customscorer(d, d.seq)
  //    FOR x IN collection_0 SEARCH x.seq == d.seq && x.name == d.name
  // SORT customscorer(d, x.seq) DESC
  {
    std::string const query =
        "FOR i IN 1..5 FOR d IN testView SEARCH d.seq == i FOR x IN "
        "collection_1 FILTER x.seq == d.seq && x.name == d.name SORT "
        "customscorer(d, x.seq) DESC RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase, query,
                                             {
                                                 arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
                                             }));

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // FOR i IN 1..5
  //  FOR d IN  SEARCH d.seq == i SORT customscorer(d, i) ASC
  //    FOR x IN collection_0 SEARCH x.seq == d.seq && x.name == d.name
  // SORT customscorer(d, i) DESC
  {
    std::string const query =
        "FOR i IN 1..5 "
        "  FOR d IN testView SEARCH d.seq == i SORT customscorer(d, i) ASC "
        "    FOR x IN collection_1 FILTER x.seq == d.seq && x.name == d.name "
        "SORT customscorer(d, i) DESC RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase, query,
                                             {
                                                 arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
                                             }));

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocsView[4].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[2].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_TRUE(expectedDocs.size() == resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                            arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_TRUE(expectedDoc == expectedDocs.end());
  }

  // dedicated to https://github.com/arangodb/planning/issues/3065$
  // Optimizer rule "inline sub-queries" which doesn't handle views correctly$
  {
    std::string const query =
        "LET fullAccounts = (FOR acc1 IN [1] RETURN { 'key': 'A' }) for a IN "
        "fullAccounts for d IN testView SEARCH d.name == a.key return d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase, query,
                                             {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
                                              arangodb::aql::OptimizerRule::inlineSubqueriesRule}));

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocsView[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_TRUE(expectedDocs.size() == resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                            arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_TRUE(expectedDoc == expectedDocs.end());
  }

  // FOR i IN 1..5
  //   FOR d IN testView SEARCH d.seq == i
  //     FOR x IN collection_1 FILTER x.seq == d.seq && x.seq == TFIDF(d)
  {
    std::string const query =
        "FOR i IN 1..5 "
        "  FOR d IN testView SEARCH d.seq == i "
        "    FOR x IN collection_1 FILTER x.seq == d.seq && x.seq == "
        "customscorer(d, i)"
        "RETURN x";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase, query,
                                             {
                                                 arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
                                             }));

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocsView[2].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[4].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_TRUE(expectedDocs.size() == resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                            arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_TRUE(expectedDoc == expectedDocs.end());
  }

  {
    std::string const query =
        "FOR i IN 1..5 "
        "  FOR d IN testView SEARCH d.seq == i "
        "    FOR x IN collection_1 FILTER x.seq == d.seq "
        "SORT 1 + customscorer(d, i) DESC "
        "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase, query,
                                             {
                                                 arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
                                             }));

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocsView[4].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[2].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_TRUE(expectedDocs.size() == resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                            arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_TRUE(expectedDoc == expectedDocs.end());
  }

  // multiple sorts
  {
    std::string const query =
        "FOR i IN 1..5 "
        "  FOR d IN testView SEARCH d.seq == i SORT tfidf(d, i > 0) ASC "
        "    FOR x IN collection_1 FILTER x.seq == d.seq && x.name == d.name "
        "SORT customscorer(d, i) DESC RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase, query,
                                             {
                                                 arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
                                             }));

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocsView[4].vpack()),
        arangodb::velocypack::Slice(insertedDocsView[2].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_TRUE(expectedDocs.size() == resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                            arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_TRUE(expectedDoc == expectedDocs.end());
  }

  // x.seq is used before being assigned
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name >= 'E' && d.seq < 10 "
        "  SORT customscorer(d) DESC "
        "  LIMIT 3 "
        "  FOR x IN collection_1 FILTER x.seq == d.seq "
        "    SORT customscorer(d, x.seq) "
        "RETURN x";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase, query,
                                             {
                                                 arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
                                             }));

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // x.seq is used before being assigned
  {
    std::string const query =
        "FOR d IN (FOR c IN testView SEARCH c.name >= 'E' && c.seq < 10 SORT "
        "customscorer(c) DESC LIMIT 3 RETURN c) "
        "  FOR x IN collection_1 FILTER x.seq == d.seq "
        "    SORT customscorer(d, x.seq) "
        "RETURN x";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase, query,
                                             {
                                                 arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
                                             }));

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
  }
}

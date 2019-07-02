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
#include "Aql/IResearchViewNode.h"
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
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
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

static const VPackBuilder systemDatabaseBuilder = dbArgsBuilder();
static const VPackSlice   systemDatabaseArgs = systemDatabaseBuilder.slice();
static const VPackBuilder testDatabaseBuilder = dbArgsBuilder("testVocbase");
static const VPackSlice   testDatabaseArgs = testDatabaseBuilder.slice();
// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class IResearchViewSortedTest : public ::testing::Test {
 protected:
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::unique_ptr<TRI_vocbase_t> system;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  IResearchViewSortedTest() : engine(server), server(nullptr, nullptr) {
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
    features.emplace_back(functions = new arangodb::aql::AqlFunctionFeature(server),
                          true);  // required for IResearchAnalyzerFeature
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

    auto* dbPathFeature =
        arangodb::application_features::ApplicationServer::getFeature<arangodb::DatabasePathFeature>(
            "DatabasePath");
    arangodb::tests::setDatabasePath(*dbPathFeature);  // ensure test data is stored in a unique directory
  }

  ~IResearchViewSortedTest() {
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
};  // IResearchViewSortedSetup

}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(IResearchViewSortedTest, SingleField) {
  // ArangoDB specific string comparer
  struct StringComparer {
    bool operator()(irs::string_ref const& lhs, irs::string_ref const& rhs) const {
      return arangodb::basics::VelocyPackHelper::compareStringValues(
                 lhs.c_str(), lhs.size(), rhs.c_str(), rhs.size(), true) < 0;
    }
  };  // StringComparer

  // ==, !=, <, <=, >, >=, range
  static std::vector<std::string> const EMPTY;

  auto createJson = arangodb::velocypack::Parser::fromJson(
      "{ \
    \"name\": \"testView\", \
    \"type\": \"arangosearch\", \
    \"primarySort\": [ { \"field\" : \"seq\", \"direction\": \"desc\" } ] \
  }");

  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                        testDatabaseArgs);
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
  EXPECT_TRUE(!view->primarySort().empty());
  EXPECT_TRUE(1 == view->primarySort().size());

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

  std::deque<arangodb::ManagedDocumentResult> insertedDocs;

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
      resource /= irs::string_ref(IResearch_test_resource_dir);
      resource /= irs::string_ref("simple_sequential.json");

      auto builder =
          arangodb::basics::VelocyPackHelper::velocyPackFromFile(resource.utf8());
      auto root = builder.slice();
      ASSERT_TRUE(root.isArray());

      size_t i = 0;

      std::shared_ptr<arangodb::LogicalCollection> collections[]{logicalCollection1,
                                                                 logicalCollection2};

      for (auto doc : arangodb::velocypack::ArrayIterator(root)) {
        insertedDocs.emplace_back();
        auto const res =
            collections[i % 2]->insert(&trx, doc, insertedDocs.back(), opt, false);
        EXPECT_TRUE(res.ok());
        ++i;
      }
    }

    EXPECT_TRUE((trx.commit().ok()));
    EXPECT_TRUE((
        arangodb::tests::executeQuery(
            vocbase, "FOR d IN testView OPTIONS { waitForSync: true } RETURN d")
            .result.ok()));  // commit

    auto snapshot =
        view->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate);
    ASSERT_TRUE(snapshot);
    EXPECT_TRUE(snapshot->size() > 1);  // ensure more than 1 segment in index snapshot
  }

  // return all
  {
    std::string const query = "FOR d IN testView SORT d.seq DESC RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(
        vocbase, query, {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule}));

    arangodb::SmallVector<arangodb::aql::ExecutionNode*>::allocator_type::arena_type a;
    arangodb::SmallVector<arangodb::aql::ExecutionNode*> nodes{a};
    auto preparedQuery = arangodb::tests::prepareQuery(vocbase, query);
    auto plan = preparedQuery->plan();
    ASSERT_TRUE(plan);
    plan->findVarUsage();

    // ensure sort node is optimized out
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::SORT, true);
    EXPECT_TRUE(nodes.empty());

    // check sort is set
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, true);
    EXPECT_TRUE(1 == nodes.size());
    auto* viewNode =
        arangodb::aql::ExecutionNode::castTo<arangodb::iresearch::IResearchViewNode*>(
            nodes.front());
    ASSERT_TRUE(viewNode);
    EXPECT_TRUE(viewNode->sort().first);
    EXPECT_EQ(1, viewNode->sort().second);

    // check query execution
    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(insertedDocs.size() == resultIt.size());

    auto expectedDoc = insertedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice(expectedDoc->vpack()), resolved, true));
      ++expectedDoc;
    }
    EXPECT_TRUE(expectedDoc == insertedDocs.rend());
  }

  // return subset
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name IN [ 'B', 'A', 'C', 'D', 'E' ] SORT "
        "d.seq DESC RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(
        vocbase, query, {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule}));

    arangodb::SmallVector<arangodb::aql::ExecutionNode*>::allocator_type::arena_type a;
    arangodb::SmallVector<arangodb::aql::ExecutionNode*> nodes{a};
    auto preparedQuery = arangodb::tests::prepareQuery(vocbase, query);
    auto plan = preparedQuery->plan();
    ASSERT_TRUE(plan);
    plan->findVarUsage();

    // ensure sort node is optimized out
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::SORT, true);
    EXPECT_TRUE(nodes.empty());

    // check sort is set
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, true);
    EXPECT_TRUE(1 == nodes.size());
    auto* viewNode =
        arangodb::aql::ExecutionNode::castTo<arangodb::iresearch::IResearchViewNode*>(
            nodes.front());
    ASSERT_TRUE(viewNode);
    EXPECT_TRUE(viewNode->sort().first);
    EXPECT_EQ(1, viewNode->sort().second);

    // check query execution
    std::vector<arangodb::ManagedDocumentResult const*> expectedDocs{
        &insertedDocs[4],  // seq == 4
        &insertedDocs[3],  // seq == 3
        &insertedDocs[2],  // seq == 2
        &insertedDocs[1],  // seq == 1
        &insertedDocs[0]   // seq == 0
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.begin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice((*expectedDoc)->vpack()),
                           resolved, true));
      ++expectedDoc;
    }
    EXPECT_TRUE(expectedDoc == expectedDocs.end());
  }

  // return subset + limit
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name IN [ 'B', 'A', 'C', 'D', 'E' ] SORT "
        "d.seq DESC LIMIT 2, 10 RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(
        vocbase, query, {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule}));

    arangodb::SmallVector<arangodb::aql::ExecutionNode*>::allocator_type::arena_type a;
    arangodb::SmallVector<arangodb::aql::ExecutionNode*> nodes{a};
    auto preparedQuery = arangodb::tests::prepareQuery(vocbase, query);
    auto plan = preparedQuery->plan();
    ASSERT_TRUE(plan);
    plan->findVarUsage();

    // ensure sort node is optimized out
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::SORT, true);
    EXPECT_TRUE(nodes.empty());

    // check sort is set
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, true);
    EXPECT_TRUE(1 == nodes.size());
    auto* viewNode =
        arangodb::aql::ExecutionNode::castTo<arangodb::iresearch::IResearchViewNode*>(
            nodes.front());
    ASSERT_TRUE(viewNode);
    EXPECT_TRUE(viewNode->sort().first);
    EXPECT_EQ(1, viewNode->sort().second);

    // check query execution
    std::vector<arangodb::ManagedDocumentResult const*> expectedDocs{
        &insertedDocs[2],  // seq == 2
        &insertedDocs[1],  // seq == 1
        &insertedDocs[0]   // seq == 0
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.begin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice((*expectedDoc)->vpack()),
                           resolved, true));
      ++expectedDoc;
    }
    EXPECT_TRUE(expectedDoc == expectedDocs.end());
  }
}

TEST_F(IResearchViewSortedTest, MultipleFields) {
  // ArangoDB specific string comparer
  struct StringComparer {
    bool operator()(irs::string_ref const& lhs, irs::string_ref const& rhs) const {
      return arangodb::basics::VelocyPackHelper::compareStringValues(
                 lhs.c_str(), lhs.size(), rhs.c_str(), rhs.size(), true) < 0;
    }
  };  // StringComparer

  // ==, !=, <, <=, >, >=, range
  static std::vector<std::string> const EMPTY;

  auto createJson = arangodb::velocypack::Parser::fromJson(
      "{ \
    \"name\": \"testView\", \
    \"type\": \"arangosearch\", \
    \"primarySort\": [ { \"field\": \"same\", \"asc\": true }, { \"field\": \"same\", \"asc\": false }, { \"field\" : \"seq\", \"direction\": \"desc\" }, { \"field\" : \"name\", \"direction\": \"asc\" } ] \
  }");

  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                        testDatabaseArgs);
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
  EXPECT_TRUE(!view->primarySort().empty());
  EXPECT_TRUE(4 == view->primarySort().size());

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

  std::deque<arangodb::ManagedDocumentResult> insertedDocs;

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
      resource /= irs::string_ref(IResearch_test_resource_dir);
      resource /= irs::string_ref("simple_sequential.json");

      auto builder =
          arangodb::basics::VelocyPackHelper::velocyPackFromFile(resource.utf8());
      auto root = builder.slice();
      ASSERT_TRUE(root.isArray());

      size_t i = 0;

      std::shared_ptr<arangodb::LogicalCollection> collections[]{logicalCollection1,
                                                                 logicalCollection2};

      for (auto doc : arangodb::velocypack::ArrayIterator(root)) {
        insertedDocs.emplace_back();
        auto const res =
            collections[i % 2]->insert(&trx, doc, insertedDocs.back(), opt, false);
        EXPECT_TRUE(res.ok());
        ++i;
      }
    }

    EXPECT_TRUE((trx.commit().ok()));
    EXPECT_TRUE((
        arangodb::tests::executeQuery(
            vocbase, "FOR d IN testView OPTIONS { waitForSync: true } RETURN d")
            .result.ok()));  // commit

    auto snapshot =
        view->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate);
    ASSERT_TRUE(snapshot);
    EXPECT_TRUE(snapshot->size() > 1);  // ensure more than 1 segment in index snapshot
  }

  // return all
  {
    std::string const query =
        "FOR d IN testView SORT d.same, d.same DESC, d.seq DESC, d.name ASC "
        "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(
        vocbase, query, {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule}));

    arangodb::SmallVector<arangodb::aql::ExecutionNode*>::allocator_type::arena_type a;
    arangodb::SmallVector<arangodb::aql::ExecutionNode*> nodes{a};
    auto preparedQuery = arangodb::tests::prepareQuery(vocbase, query);
    auto plan = preparedQuery->plan();
    ASSERT_TRUE(plan);
    plan->findVarUsage();

    // ensure sort node is optimized out
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::SORT, true);
    EXPECT_TRUE(nodes.empty());

    // check sort is set
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, true);
    EXPECT_TRUE(1 == nodes.size());
    auto* viewNode =
        arangodb::aql::ExecutionNode::castTo<arangodb::iresearch::IResearchViewNode*>(
            nodes.front());
    ASSERT_TRUE(viewNode);
    EXPECT_TRUE(viewNode->sort().first);
    EXPECT_EQ(4, viewNode->sort().second);

    // check query execution
    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(insertedDocs.size() == resultIt.size());

    auto expectedDoc = insertedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice(expectedDoc->vpack()), resolved, true));
      ++expectedDoc;
    }
    EXPECT_TRUE(expectedDoc == insertedDocs.rend());
  }

  // return all
  {
    std::string const query =
        "FOR d IN testView SORT d.same, d.same DESC, d.seq DESC RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(
        vocbase, query, {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule}));

    arangodb::SmallVector<arangodb::aql::ExecutionNode*>::allocator_type::arena_type a;
    arangodb::SmallVector<arangodb::aql::ExecutionNode*> nodes{a};
    auto preparedQuery = arangodb::tests::prepareQuery(vocbase, query);
    auto plan = preparedQuery->plan();
    ASSERT_TRUE(plan);
    plan->findVarUsage();

    // ensure sort node is optimized out
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::SORT, true);
    EXPECT_TRUE(nodes.empty());

    // check sort is set
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, true);
    EXPECT_TRUE(1 == nodes.size());
    auto* viewNode =
        arangodb::aql::ExecutionNode::castTo<arangodb::iresearch::IResearchViewNode*>(
            nodes.front());
    ASSERT_TRUE(viewNode);
    EXPECT_TRUE(viewNode->sort().first);
    EXPECT_EQ(3, viewNode->sort().second);

    // check query execution
    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(insertedDocs.size() == resultIt.size());

    auto expectedDoc = insertedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice(expectedDoc->vpack()), resolved, true));
      ++expectedDoc;
    }
    EXPECT_TRUE(expectedDoc == insertedDocs.rend());
  }

  // return subset
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name IN [ 'B', 'A', 'C', 'D', 'E' ] SORT "
        "d.same ASC, d.same DESC, d.seq DESC RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(
        vocbase, query, {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule}));

    arangodb::SmallVector<arangodb::aql::ExecutionNode*>::allocator_type::arena_type a;
    arangodb::SmallVector<arangodb::aql::ExecutionNode*> nodes{a};
    auto preparedQuery = arangodb::tests::prepareQuery(vocbase, query);
    auto plan = preparedQuery->plan();
    ASSERT_TRUE(plan);
    plan->findVarUsage();

    // ensure sort node is optimized out
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::SORT, true);
    EXPECT_TRUE(nodes.empty());

    // check sort is set
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, true);
    EXPECT_TRUE(1 == nodes.size());
    auto* viewNode =
        arangodb::aql::ExecutionNode::castTo<arangodb::iresearch::IResearchViewNode*>(
            nodes.front());
    ASSERT_TRUE(viewNode);
    EXPECT_TRUE(viewNode->sort().first);
    EXPECT_EQ(3, viewNode->sort().second);

    // check query execution
    std::vector<arangodb::ManagedDocumentResult const*> expectedDocs{
        &insertedDocs[4],  // seq == 4
        &insertedDocs[3],  // seq == 3
        &insertedDocs[2],  // seq == 2
        &insertedDocs[1],  // seq == 1
        &insertedDocs[0]   // seq == 0
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.begin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice((*expectedDoc)->vpack()),
                           resolved, true));
      ++expectedDoc;
    }
    EXPECT_TRUE(expectedDoc == expectedDocs.end());
  }

  // return subset + limit
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name IN [ 'B', 'A', 'C', 'D', 'E' ] SORT "
        "d.same, d.same DESC, d.seq DESC, d.name ASC LIMIT 2, 10 RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(
        vocbase, query, {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule}));

    arangodb::SmallVector<arangodb::aql::ExecutionNode*>::allocator_type::arena_type a;
    arangodb::SmallVector<arangodb::aql::ExecutionNode*> nodes{a};
    auto preparedQuery = arangodb::tests::prepareQuery(vocbase, query);
    auto plan = preparedQuery->plan();
    ASSERT_TRUE(plan);
    plan->findVarUsage();

    // ensure sort node is optimized out
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::SORT, true);
    EXPECT_TRUE(nodes.empty());

    // check sort is set
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, true);
    EXPECT_TRUE(1 == nodes.size());
    auto* viewNode =
        arangodb::aql::ExecutionNode::castTo<arangodb::iresearch::IResearchViewNode*>(
            nodes.front());
    ASSERT_TRUE(viewNode);
    EXPECT_TRUE(viewNode->sort().first);
    EXPECT_EQ(4, viewNode->sort().second);

    // check query execution
    std::vector<arangodb::ManagedDocumentResult const*> expectedDocs{
        &insertedDocs[2],  // seq == 2
        &insertedDocs[1],  // seq == 1
        &insertedDocs[0]   // seq == 0
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.begin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice((*expectedDoc)->vpack()),
                           resolved, true));
      ++expectedDoc;
    }
    EXPECT_TRUE(expectedDoc == expectedDocs.end());
  }

  // return subset + limit
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name IN [ 'B', 'A', 'C', 'D', 'E' ] SORT "
        "d.same, d.same DESC LIMIT 10, 10 RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(
        vocbase, query, {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule}));

    arangodb::SmallVector<arangodb::aql::ExecutionNode*>::allocator_type::arena_type a;
    arangodb::SmallVector<arangodb::aql::ExecutionNode*> nodes{a};
    auto preparedQuery = arangodb::tests::prepareQuery(vocbase, query);
    auto plan = preparedQuery->plan();
    ASSERT_TRUE(plan);
    plan->findVarUsage();

    // ensure sort node is optimized out
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::SORT, true);
    EXPECT_TRUE(nodes.empty());

    // check sort is set
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, true);
    EXPECT_TRUE(1 == nodes.size());
    auto* viewNode =
        arangodb::aql::ExecutionNode::castTo<arangodb::iresearch::IResearchViewNode*>(
            nodes.front());
    ASSERT_TRUE(viewNode);
    EXPECT_TRUE(viewNode->sort().first);
    EXPECT_EQ(2, viewNode->sort().second);

    // check query execution
    std::vector<arangodb::ManagedDocumentResult const*> expectedDocs{};

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.begin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice((*expectedDoc)->vpack()),
                           resolved, true));
      ++expectedDoc;
    }
    EXPECT_TRUE(expectedDoc == expectedDocs.end());
  }
}

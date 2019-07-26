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

#include "Aql/IResearchViewNode.h"

#include "../Mocks/StorageEngineMock.h"

#if USE_ENTERPRISE
#include "Enterprise/Ldap/LdapFeature.h"
#endif

#include "3rdParty/iresearch/tests/tests_config.hpp"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/Ast.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/IResearchViewExecutor.h"
#include "Aql/IResearchViewNode.h"
#include "Aql/NoResultsExecutor.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Aql/Query.h"
#include "Basics/VelocyPackHelper.h"
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
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "VocBase/ManagedDocumentResult.h"

#include "IResearch/VelocyPackHelper.h"
#include "analysis/analyzers.hpp"
#include "analysis/token_attributes.hpp"
#include "utils/utf8_path.hpp"

#include "velocypack/Iterator.h"

namespace {

class IResearchViewNodeTest : public ::testing::Test {
 protected:
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::unique_ptr<TRI_vocbase_t> system;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  IResearchViewNodeTest() : engine(server), server(nullptr, nullptr) {
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
                                                     0, TRI_VOC_SYSTEM_DATABASE);
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

  ~IResearchViewNodeTest() {
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
};  // IResearchViewNodeSetup

}  // namespace

TEST_F(IResearchViewNodeTest, constructSortedView) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                        "testVocbase");
  // create view
  auto createJson = arangodb::velocypack::Parser::fromJson(
      "{ "
      "  \"name\": \"testView\", "
      "  \"type\": \"arangosearch\", "
      "  \"primarySort\": [ "
      "    { \"field\": \"my.nested.Fields\", \"asc\": false }, "
      "    { \"field\": \"another.field\", \"asc\": true } ] "
      "}");
  auto logicalView = vocbase.createView(createJson->slice());
  ASSERT_TRUE(logicalView);

  // dummy query
  arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString("RETURN 1"),
                             nullptr, arangodb::velocypack::Parser::fromJson("{}"),
                             arangodb::aql::PART_MAIN);
  query.prepare(arangodb::QueryRegistryFeature::registry());
  arangodb::aql::Variable const outVariable("variable", 0);

  {
    auto json = arangodb::velocypack::Parser::fromJson(
          "{ \"id\":42, \"depth\":0, \"totalNrRegs\":0, \"varInfoList\":[], "
          "\"nrRegs\":[], \"nrRegsHere\":[], \"regsToClear\":[], "
          "\"varsUsedLater\":[], \"varsValid\":[], \"outVariable\": { "
          "\"name\":\"variable\", \"id\":0 }, \"options\": { \"waitForSync\" : "
          "true, \"collections\":[42] }, \"viewId\": \"" + std::to_string(logicalView->id()) + "\", "
          "\"primarySort\": [ { \"field\": \"my.nested.Fields\", \"asc\": false},  { \"field\": \"another.field\", \"asc\":true } ] }");

    arangodb::iresearch::IResearchViewNode node(*query.plan(),  // plan
                                                json->slice());

    EXPECT_TRUE(node.empty());                // view has no links
    EXPECT_TRUE(node.collections().empty());  // view has no links
    EXPECT_TRUE(node.shards().empty());
    EXPECT_TRUE(node.sort().first);  // primary sort is set
    EXPECT_EQ(2, node.sort().second);

    EXPECT_TRUE(arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW == node.getType());
    EXPECT_TRUE(outVariable.id == node.outVariable().id);
    EXPECT_TRUE(outVariable.name == node.outVariable().name);
    EXPECT_TRUE(query.plan() == node.plan());
    EXPECT_TRUE(42 == node.id());
    EXPECT_TRUE(logicalView == node.view());
    EXPECT_TRUE(node.scorers().empty());
    EXPECT_TRUE(!node.volatility().first);   // filter volatility
    EXPECT_TRUE(!node.volatility().second);  // sort volatility
    arangodb::HashSet<arangodb::aql::Variable const*> usedHere;
    node.getVariablesUsedHere(usedHere);
    EXPECT_TRUE(usedHere.empty());
    auto const setHere = node.getVariablesSetHere();
    EXPECT_TRUE(1 == setHere.size());
    EXPECT_TRUE(outVariable.id == setHere[0]->id);
    EXPECT_TRUE(outVariable.name == setHere[0]->name);
    EXPECT_TRUE(true == node.options().forceSync);
    EXPECT_TRUE(true == node.options().restrictSources);
    EXPECT_TRUE(1 == node.options().sources.size());
    EXPECT_TRUE(42 == *node.options().sources.begin());

    EXPECT_TRUE(0. == node.getCost().estimatedCost);    // no dependencies
    EXPECT_TRUE(0 == node.getCost().estimatedNrItems);  // no dependencies
  }

  {
    auto json = arangodb::velocypack::Parser::fromJson(
          "{ \"id\":42, \"depth\":0, \"totalNrRegs\":0, \"varInfoList\":[], "
          "\"nrRegs\":[], \"nrRegsHere\":[], \"regsToClear\":[], "
          "\"varsUsedLater\":[], \"varsValid\":[], \"outVariable\": { "
          "\"name\":\"variable\", \"id\":0 }, \"options\": { \"waitForSync\" : "
          "true, \"collections\":[42] }, \"viewId\": \"" + std::to_string(logicalView->id()) + "\", "
          "\"primarySort\": [ { \"field\": \"my.nested.Fields\", \"asc\": false},  { \"field\": \"another.field\", \"asc\":true } ], \"primarySortBuckets\": 1 }");

    arangodb::iresearch::IResearchViewNode node(*query.plan(),  // plan
                                                json->slice());

    EXPECT_TRUE(node.empty());                // view has no links
    EXPECT_TRUE(node.collections().empty());  // view has no links
    EXPECT_TRUE(node.shards().empty());
    EXPECT_TRUE(node.sort().first);  // primary sort is set
    EXPECT_EQ(1, node.sort().second);

    EXPECT_TRUE(arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW == node.getType());
    EXPECT_TRUE(outVariable.id == node.outVariable().id);
    EXPECT_TRUE(outVariable.name == node.outVariable().name);
    EXPECT_TRUE(query.plan() == node.plan());
    EXPECT_TRUE(42 == node.id());
    EXPECT_TRUE(logicalView == node.view());
    EXPECT_TRUE(node.scorers().empty());
    EXPECT_TRUE(!node.volatility().first);   // filter volatility
    EXPECT_TRUE(!node.volatility().second);  // sort volatility
    arangodb::HashSet<arangodb::aql::Variable const*> usedHere;
    node.getVariablesUsedHere(usedHere);
    EXPECT_TRUE(usedHere.empty());
    auto const setHere = node.getVariablesSetHere();
    EXPECT_TRUE(1 == setHere.size());
    EXPECT_TRUE(outVariable.id == setHere[0]->id);
    EXPECT_TRUE(outVariable.name == setHere[0]->name);
    EXPECT_TRUE(true == node.options().forceSync);
    EXPECT_TRUE(true == node.options().restrictSources);
    EXPECT_TRUE(1 == node.options().sources.size());
    EXPECT_TRUE(42 == *node.options().sources.begin());

    EXPECT_TRUE(0. == node.getCost().estimatedCost);    // no dependencies
    EXPECT_TRUE(0 == node.getCost().estimatedNrItems);  // no dependencies
  }

  // invalid 'primarySortBuckets' specified
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"id\":42, \"depth\":0, \"totalNrRegs\":0, \"varInfoList\":[], "
        "\"nrRegs\":[], \"nrRegsHere\":[], \"regsToClear\":[], "
        "\"varsUsedLater\":[], \"varsValid\":[], \"outVariable\": { "
        "\"name\":\"variable\", \"id\":0 }, \"viewId\": \"" + std::to_string(logicalView->id()) + "\", "
        "\"primarySort\": [ { \"field\": \"my.nested.Fields\", \"asc\": false},  { \"field\": \"another.field\", \"asc\":true } ], \"primarySortBuckets\": false }");

    try {
      arangodb::iresearch::IResearchViewNode node(*query.plan(),  // plan
                                                  json->slice());
      EXPECT_TRUE(false);
    } catch (arangodb::basics::Exception const& ex) {
      EXPECT_TRUE(TRI_ERROR_BAD_PARAMETER == ex.code());
    }
  }

  // invalid 'primarySortBuckets' specified
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"id\":42, \"depth\":0, \"totalNrRegs\":0, \"varInfoList\":[], "
        "\"nrRegs\":[], \"nrRegsHere\":[], \"regsToClear\":[], "
        "\"varsUsedLater\":[], \"varsValid\":[], \"outVariable\": { "
        "\"name\":\"variable\", \"id\":0 }, \"viewId\": \"" + std::to_string(logicalView->id()) + "\", "
        "\"primarySort\": [ { \"field\": \"my.nested.Fields\", \"asc\": false},  { \"field\": \"another.field\", \"asc\":true } ], \"primarySortBuckets\": 3 }");

    try {
      arangodb::iresearch::IResearchViewNode node(*query.plan(),  // plan
                                                  json->slice());
      EXPECT_TRUE(false);
    } catch (arangodb::basics::Exception const& ex) {
      EXPECT_TRUE(TRI_ERROR_BAD_PARAMETER == ex.code());
    }
  }
}

TEST_F(IResearchViewNodeTest, construct) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                        "testVocbase");
  // create view
  auto createJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
  auto logicalView = vocbase.createView(createJson->slice());
  ASSERT_TRUE((false == !logicalView));

  // dummy query
  arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString("RETURN 1"),
                             nullptr, arangodb::velocypack::Parser::fromJson("{}"),
                             arangodb::aql::PART_MAIN);
  query.prepare(arangodb::QueryRegistryFeature::registry());
  arangodb::aql::Variable const outVariable("variable", 0);

  // no options
  {
    arangodb::aql::SingletonNode singleton(query.plan(), 0);
    arangodb::iresearch::IResearchViewNode node(*query.plan(),  // plan
                                                42,             // id
                                                vocbase,        // database
                                                logicalView,    // view
                                                outVariable,    // out variable
                                                nullptr,  // no filter condition
                                                nullptr,  // no options
                                                {}        // no sort condition
    );
    node.addDependency(&singleton);

    EXPECT_TRUE(node.empty());                // view has no links
    EXPECT_TRUE(node.collections().empty());  // view has no links
    EXPECT_TRUE(node.shards().empty());
    EXPECT_TRUE(!node.sort().first);  // primary sort is not set by default
    EXPECT_EQ(0, node.sort().second);  // primary sort is not set by default

    EXPECT_TRUE(arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW == node.getType());
    EXPECT_TRUE(&outVariable == &node.outVariable());
    EXPECT_TRUE(query.plan() == node.plan());
    EXPECT_TRUE(42 == node.id());
    EXPECT_TRUE(logicalView == node.view());
    EXPECT_TRUE(node.scorers().empty());
    EXPECT_TRUE(!node.volatility().first);   // filter volatility
    EXPECT_TRUE(!node.volatility().second);  // sort volatility
    arangodb::HashSet<arangodb::aql::Variable const*> usedHere;
    node.getVariablesUsedHere(usedHere);
    EXPECT_TRUE(usedHere.empty());
    auto const setHere = node.getVariablesSetHere();
    EXPECT_TRUE(1 == setHere.size());
    EXPECT_TRUE(&outVariable == setHere[0]);
    EXPECT_TRUE(false == node.options().forceSync);

    EXPECT_TRUE(2. == node.getCost().estimatedCost);  // dependency is a singleton
    EXPECT_TRUE(1 == node.getCost().estimatedNrItems);  // dependency is a singleton
  }

  // with options
  {
    // build options node
    arangodb::aql::AstNode attributeValue(arangodb::aql::NODE_TYPE_VALUE);
    attributeValue.setValueType(arangodb::aql::VALUE_TYPE_BOOL);
    attributeValue.setBoolValue(true);
    arangodb::aql::AstNode attributeName(arangodb::aql::NODE_TYPE_OBJECT_ELEMENT);
    attributeName.addMember(&attributeValue);
    attributeName.setStringValue("waitForSync", strlen("waitForSync"));
    arangodb::aql::AstNode options(arangodb::aql::NODE_TYPE_OBJECT);
    options.addMember(&attributeName);

    arangodb::iresearch::IResearchViewNode node(*query.plan(),  // plan
                                                42,             // id
                                                vocbase,        // database
                                                logicalView,    // view
                                                outVariable,    // out variable
                                                nullptr,  // no filter condition
                                                &options, {}  // no sort condition
    );

    EXPECT_TRUE(node.empty());                // view has no links
    EXPECT_TRUE(node.collections().empty());  // view has no links
    EXPECT_TRUE(node.shards().empty());

    EXPECT_TRUE(arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW == node.getType());
    EXPECT_TRUE(&outVariable == &node.outVariable());
    EXPECT_TRUE(query.plan() == node.plan());
    EXPECT_TRUE(42 == node.id());
    EXPECT_TRUE(logicalView == node.view());
    EXPECT_TRUE(node.scorers().empty());
    EXPECT_TRUE(!node.volatility().first);   // filter volatility
    EXPECT_TRUE(!node.volatility().second);  // sort volatility
    arangodb::HashSet<arangodb::aql::Variable const*> usedHere;
    node.getVariablesUsedHere(usedHere);
    EXPECT_TRUE(usedHere.empty());
    auto const setHere = node.getVariablesSetHere();
    EXPECT_TRUE(1 == setHere.size());
    EXPECT_TRUE(&outVariable == setHere[0]);
    EXPECT_TRUE(true == node.options().forceSync);

    EXPECT_TRUE(0. == node.getCost().estimatedCost);    // no dependencies
    EXPECT_TRUE(0 == node.getCost().estimatedNrItems);  // no dependencies
  }

  // invalid options
  {
    // build options node
    arangodb::aql::AstNode attributeValue(arangodb::aql::NODE_TYPE_VALUE);
    attributeValue.setValueType(arangodb::aql::VALUE_TYPE_STRING);
    attributeValue.setBoolValue(true);
    arangodb::aql::AstNode attributeName(arangodb::aql::NODE_TYPE_OBJECT_ELEMENT);
    attributeName.addMember(&attributeValue);
    attributeName.setStringValue("waitForSync", strlen("waitForSync"));
    attributeName.addMember(&attributeName);
    arangodb::aql::AstNode options(arangodb::aql::NODE_TYPE_OBJECT);
    options.addMember(&attributeName);

    EXPECT_ANY_THROW(
        arangodb::iresearch::IResearchViewNode(*query.plan(),  // plan
                                               42,             // id
                                               vocbase,        // database
                                               logicalView,    // view
                                               outVariable,    // out variable
                                               nullptr,  // no filter condition
                                               &options, {}  // no sort condition
                                               ));
  }

  // invalid options
  {
    // build options node
    arangodb::aql::AstNode attributeValue(arangodb::aql::NODE_TYPE_VALUE);
    attributeValue.setValueType(arangodb::aql::VALUE_TYPE_BOOL);
    attributeValue.setBoolValue(true);
    arangodb::aql::AstNode attributeName(arangodb::aql::NODE_TYPE_OBJECT_ELEMENT);
    attributeName.setStringValue("waitForSync1", strlen("waitForSync1"));
    attributeName.addMember(&attributeValue);
    attributeName.addMember(&attributeName);
    arangodb::aql::AstNode options(arangodb::aql::NODE_TYPE_OBJECT);
    options.addMember(&attributeName);

    arangodb::iresearch::IResearchViewNode node(*query.plan(),  // plan
                                                42,             // id
                                                vocbase,        // database
                                                logicalView,    // view
                                                outVariable,    // out variable
                                                nullptr,  // no filter condition
                                                &options, {}  // no sort condition
    );

    EXPECT_TRUE(node.empty());                // view has no links
    EXPECT_TRUE(node.collections().empty());  // view has no links
    EXPECT_TRUE(node.shards().empty());

    EXPECT_TRUE(arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW == node.getType());
    EXPECT_TRUE(&outVariable == &node.outVariable());
    EXPECT_TRUE(query.plan() == node.plan());
    EXPECT_TRUE(42 == node.id());
    EXPECT_TRUE(logicalView == node.view());
    EXPECT_TRUE(node.scorers().empty());
    EXPECT_TRUE(!node.volatility().first);   // filter volatility
    EXPECT_TRUE(!node.volatility().second);  // sort volatility
    arangodb::HashSet<arangodb::aql::Variable const*> usedHere;
    node.getVariablesUsedHere(usedHere);
    EXPECT_TRUE(usedHere.empty());
    auto const setHere = node.getVariablesSetHere();
    EXPECT_TRUE(1 == setHere.size());
    EXPECT_TRUE(&outVariable == setHere[0]);
    EXPECT_TRUE(false == node.options().forceSync);

    EXPECT_TRUE(0. == node.getCost().estimatedCost);    // no dependencies
    EXPECT_TRUE(0 == node.getCost().estimatedNrItems);  // no dependencies
  }
}

TEST_F(IResearchViewNodeTest, constructFromVPackSingleServer) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                        "testVocbase");
  // create view
  auto createJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
  auto logicalView = vocbase.createView(createJson->slice());
  ASSERT_TRUE((false == !logicalView));

  // dummy query
  arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString("RETURN 1"),
                             nullptr, arangodb::velocypack::Parser::fromJson("{}"),
                             arangodb::aql::PART_MAIN);
  query.prepare(arangodb::QueryRegistryFeature::registry());
  arangodb::aql::Variable const outVariable("variable", 0);

  // missing 'viewId'
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"id\":42, \"depth\":0, \"totalNrRegs\":0, \"varInfoList\":[], "
        "\"nrRegs\":[], \"nrRegsHere\":[], \"regsToClear\":[], "
        "\"varsUsedLater\":[], \"varsValid\":[], \"outVariable\": { "
        "\"name\":\"variable\", \"id\":0 } }");

    try {
      arangodb::iresearch::IResearchViewNode node(*query.plan(),  // plan
                                                  json->slice());
      EXPECT_TRUE(false);
    } catch (arangodb::basics::Exception const& ex) {
      EXPECT_TRUE(TRI_ERROR_BAD_PARAMETER == ex.code());
    }
  }

  // invalid 'viewId' type (string expected)
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"id\":42, \"depth\":0, \"totalNrRegs\":0, \"varInfoList\":[], "
        "\"nrRegs\":[], \"nrRegsHere\":[], \"regsToClear\":[], "
        "\"varsUsedLater\":[], \"varsValid\":[], \"outVariable\": { "
        "\"name\":\"variable\", \"id\":0 }, \"viewId\":123 }");

    try {
      arangodb::iresearch::IResearchViewNode node(*query.plan(),  // plan
                                                  json->slice());
      EXPECT_TRUE(false);
    } catch (arangodb::basics::Exception const& ex) {
      EXPECT_TRUE(TRI_ERROR_BAD_PARAMETER == ex.code());
    }
  }

  // invalid 'viewId' specified
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"id\":42, \"depth\":0, \"totalNrRegs\":0, \"varInfoList\":[], "
        "\"nrRegs\":[], \"nrRegsHere\":[], \"regsToClear\":[], "
        "\"varsUsedLater\":[], \"varsValid\":[], \"outVariable\": { "
        "\"name\":\"variable\", \"id\":0 }, \"viewId\": \"foo\"}");

    try {
      arangodb::iresearch::IResearchViewNode node(*query.plan(),  // plan
                                                  json->slice());
      EXPECT_TRUE(false);
    } catch (arangodb::basics::Exception const& ex) {
      EXPECT_TRUE(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND == ex.code());
    }
  }

  // invalid 'primarySort' specified
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"id\":42, \"depth\":0, \"totalNrRegs\":0, \"varInfoList\":[], "
        "\"nrRegs\":[], \"nrRegsHere\":[], \"regsToClear\":[], "
        "\"varsUsedLater\":[], \"varsValid\":[], \"outVariable\": { "
        "\"name\":\"variable\", \"id\":0 }, \"viewId\": \"" +
        std::to_string(logicalView->id()) + "\", \"primarySort\": false }");

    try {
      arangodb::iresearch::IResearchViewNode node(*query.plan(),  // plan
                                                  json->slice());
      EXPECT_TRUE(false);
    } catch (arangodb::basics::Exception const& ex) {
      EXPECT_TRUE(TRI_ERROR_BAD_PARAMETER == ex.code());
    }
  }

  // no options
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"id\":42, \"depth\":0, \"totalNrRegs\":0, \"varInfoList\":[], "
        "\"nrRegs\":[], \"nrRegsHere\":[], \"regsToClear\":[], "
        "\"varsUsedLater\":[], \"varsValid\":[], \"outVariable\": { "
        "\"name\":\"variable\", \"id\":0 }, \"viewId\": \"" +
        std::to_string(logicalView->id()) + "\", \"primarySort\": [] }");

    arangodb::iresearch::IResearchViewNode node(*query.plan(),  // plan
                                                json->slice());

    EXPECT_TRUE(node.empty());                // view has no links
    EXPECT_TRUE(node.collections().empty());  // view has no links
    EXPECT_TRUE(node.shards().empty());
    EXPECT_TRUE(!node.sort().first);  // primary sort is not set by default
    EXPECT_EQ(0, node.sort().second);  // primary sort is not set by default

    EXPECT_TRUE(arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW == node.getType());
    EXPECT_TRUE(outVariable.id == node.outVariable().id);
    EXPECT_TRUE(outVariable.name == node.outVariable().name);
    EXPECT_TRUE(query.plan() == node.plan());
    EXPECT_TRUE(42 == node.id());
    EXPECT_TRUE(logicalView == node.view());
    EXPECT_TRUE(node.scorers().empty());
    EXPECT_TRUE(!node.volatility().first);   // filter volatility
    EXPECT_TRUE(!node.volatility().second);  // sort volatility
    arangodb::HashSet<arangodb::aql::Variable const*> usedHere;
    node.getVariablesUsedHere(usedHere);
    EXPECT_TRUE(usedHere.empty());
    auto const setHere = node.getVariablesSetHere();
    EXPECT_TRUE(1 == setHere.size());
    EXPECT_TRUE(outVariable.id == setHere[0]->id);
    EXPECT_TRUE(outVariable.name == setHere[0]->name);
    EXPECT_TRUE(false == node.options().forceSync);

    EXPECT_TRUE(0. == node.getCost().estimatedCost);    // no dependencies
    EXPECT_TRUE(0 == node.getCost().estimatedNrItems);  // no dependencies
  }

  // no options, ignore 'primarySortBuckets'
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"id\":42, \"depth\":0, \"totalNrRegs\":0, \"varInfoList\":[], "
        "\"nrRegs\":[], \"nrRegsHere\":[], \"regsToClear\":[], "
        "\"varsUsedLater\":[], \"varsValid\":[], \"outVariable\": { "
        "\"name\":\"variable\", \"id\":0 }, \"viewId\": \"" +
        std::to_string(logicalView->id()) + "\", \"primarySort\": [], \"primarySortBuckets\": false }");

    arangodb::iresearch::IResearchViewNode node(*query.plan(),  // plan
                                                json->slice());

    EXPECT_TRUE(node.empty());                // view has no links
    EXPECT_TRUE(node.collections().empty());  // view has no links
    EXPECT_TRUE(node.shards().empty());
    EXPECT_TRUE(!node.sort().first);  // primary sort is not set by default
    EXPECT_EQ(0, node.sort().second);  // primary sort is not set by default

    EXPECT_TRUE(arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW == node.getType());
    EXPECT_TRUE(outVariable.id == node.outVariable().id);
    EXPECT_TRUE(outVariable.name == node.outVariable().name);
    EXPECT_TRUE(query.plan() == node.plan());
    EXPECT_TRUE(42 == node.id());
    EXPECT_TRUE(logicalView == node.view());
    EXPECT_TRUE(node.scorers().empty());
    EXPECT_TRUE(!node.volatility().first);   // filter volatility
    EXPECT_TRUE(!node.volatility().second);  // sort volatility
    arangodb::HashSet<arangodb::aql::Variable const*> usedHere;
    node.getVariablesUsedHere(usedHere);
    EXPECT_TRUE(usedHere.empty());
    auto const setHere = node.getVariablesSetHere();
    EXPECT_TRUE(1 == setHere.size());
    EXPECT_TRUE(outVariable.id == setHere[0]->id);
    EXPECT_TRUE(outVariable.name == setHere[0]->name);
    EXPECT_TRUE(false == node.options().forceSync);

    EXPECT_TRUE(0. == node.getCost().estimatedCost);    // no dependencies
    EXPECT_TRUE(0 == node.getCost().estimatedNrItems);  // no dependencies
  }

  // no options
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"id\":42, \"depth\":0, \"totalNrRegs\":0, \"varInfoList\":[], "
        "\"nrRegs\":[], \"nrRegsHere\":[], \"regsToClear\":[], "
        "\"varsUsedLater\":[], \"varsValid\":[], \"outVariable\": { "
        "\"name\":\"variable\", \"id\":0 }, \"viewId\": \"" +
        std::to_string(logicalView->id()) + "\", \"primarySort\": [], \"primarySortBuckets\": 42 }");

    arangodb::iresearch::IResearchViewNode node(*query.plan(),  // plan
                                                json->slice());

    EXPECT_TRUE(node.empty());                // view has no links
    EXPECT_TRUE(node.collections().empty());  // view has no links
    EXPECT_TRUE(node.shards().empty());
    EXPECT_TRUE(!node.sort().first);  // primary sort is not set by default
    EXPECT_EQ(0, node.sort().second);  // primary sort is not set by default

    EXPECT_TRUE(arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW == node.getType());
    EXPECT_TRUE(outVariable.id == node.outVariable().id);
    EXPECT_TRUE(outVariable.name == node.outVariable().name);
    EXPECT_TRUE(query.plan() == node.plan());
    EXPECT_TRUE(42 == node.id());
    EXPECT_TRUE(logicalView == node.view());
    EXPECT_TRUE(node.scorers().empty());
    EXPECT_TRUE(!node.volatility().first);   // filter volatility
    EXPECT_TRUE(!node.volatility().second);  // sort volatility
    arangodb::HashSet<arangodb::aql::Variable const*> usedHere;
    node.getVariablesUsedHere(usedHere);
    EXPECT_TRUE(usedHere.empty());
    auto const setHere = node.getVariablesSetHere();
    EXPECT_TRUE(1 == setHere.size());
    EXPECT_TRUE(outVariable.id == setHere[0]->id);
    EXPECT_TRUE(outVariable.name == setHere[0]->name);
    EXPECT_TRUE(false == node.options().forceSync);

    EXPECT_TRUE(0. == node.getCost().estimatedCost);    // no dependencies
    EXPECT_TRUE(0 == node.getCost().estimatedNrItems);  // no dependencies
  }

  // with options
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"id\":42, \"depth\":0, \"totalNrRegs\":0, \"varInfoList\":[], "
        "\"nrRegs\":[], \"nrRegsHere\":[], \"regsToClear\":[], "
        "\"varsUsedLater\":[], \"varsValid\":[], \"outVariable\": { "
        "\"name\":\"variable\", \"id\":0 }, \"options\": { \"waitForSync\" : "
        "true, \"collections\":[42] }, \"viewId\": \"" +
        std::to_string(logicalView->id()) + "\", \"primarySort\": [] }");

    arangodb::iresearch::IResearchViewNode node(*query.plan(),  // plan
                                                json->slice());

    EXPECT_TRUE(node.empty());                // view has no links
    EXPECT_TRUE(node.collections().empty());  // view has no links
    EXPECT_TRUE(node.shards().empty());
    EXPECT_TRUE(!node.sort().first);  // primary sort is not set by default
    EXPECT_EQ(0, node.sort().second);  // primary sort is not set by default

    EXPECT_TRUE(arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW == node.getType());
    EXPECT_TRUE(outVariable.id == node.outVariable().id);
    EXPECT_TRUE(outVariable.name == node.outVariable().name);
    EXPECT_TRUE(query.plan() == node.plan());
    EXPECT_TRUE(42 == node.id());
    EXPECT_TRUE(logicalView == node.view());
    EXPECT_TRUE(node.scorers().empty());
    EXPECT_TRUE(!node.volatility().first);   // filter volatility
    EXPECT_TRUE(!node.volatility().second);  // sort volatility
    arangodb::HashSet<arangodb::aql::Variable const*> usedHere;
    node.getVariablesUsedHere(usedHere);
    EXPECT_TRUE(usedHere.empty());
    auto const setHere = node.getVariablesSetHere();
    EXPECT_TRUE(1 == setHere.size());
    EXPECT_TRUE(outVariable.id == setHere[0]->id);
    EXPECT_TRUE(outVariable.name == setHere[0]->name);
    EXPECT_TRUE(true == node.options().forceSync);
    EXPECT_TRUE(true == node.options().restrictSources);
    EXPECT_TRUE(1 == node.options().sources.size());
    EXPECT_TRUE(42 == *node.options().sources.begin());

    EXPECT_TRUE(0. == node.getCost().estimatedCost);    // no dependencies
    EXPECT_TRUE(0 == node.getCost().estimatedNrItems);  // no dependencies
  }

  // with options
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"id\":42, \"depth\":0, \"totalNrRegs\":0, \"varInfoList\":[], "
        "\"nrRegs\":[], \"nrRegsHere\":[], \"regsToClear\":[], "
        "\"varsUsedLater\":[], \"varsValid\":[], \"outVariable\": { "
        "\"name\":\"variable\", \"id\":0 }, \"options\": { \"waitForSync\" : "
        "true, \"collections\":[] }, \"viewId\": \"" +
        std::to_string(logicalView->id()) + "\" }");

    arangodb::iresearch::IResearchViewNode node(*query.plan(),  // plan
                                                json->slice());

    EXPECT_TRUE(node.empty());                // view has no links
    EXPECT_TRUE(node.collections().empty());  // view has no links
    EXPECT_TRUE(node.shards().empty());
    EXPECT_TRUE(!node.sort().first);  // primary sort is not set by default
    EXPECT_EQ(0, node.sort().second);  // primary sort is not set by default

    EXPECT_TRUE(arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW == node.getType());
    EXPECT_TRUE(outVariable.id == node.outVariable().id);
    EXPECT_TRUE(outVariable.name == node.outVariable().name);
    EXPECT_TRUE(query.plan() == node.plan());
    EXPECT_TRUE(42 == node.id());
    EXPECT_TRUE(logicalView == node.view());
    EXPECT_TRUE(node.scorers().empty());
    EXPECT_TRUE(!node.volatility().first);   // filter volatility
    EXPECT_TRUE(!node.volatility().second);  // sort volatility
    arangodb::HashSet<arangodb::aql::Variable const*> usedHere;
    node.getVariablesUsedHere(usedHere);
    EXPECT_TRUE(usedHere.empty());
    auto const setHere = node.getVariablesSetHere();
    EXPECT_TRUE(1 == setHere.size());
    EXPECT_TRUE(outVariable.id == setHere[0]->id);
    EXPECT_TRUE(outVariable.name == setHere[0]->name);
    EXPECT_TRUE(true == node.options().forceSync);
    EXPECT_TRUE(true == node.options().restrictSources);
    EXPECT_TRUE(0 == node.options().sources.size());

    EXPECT_TRUE(0. == node.getCost().estimatedCost);    // no dependencies
    EXPECT_TRUE(0 == node.getCost().estimatedNrItems);  // no dependencies
  }

  // with options
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"id\":42, \"depth\":0, \"totalNrRegs\":0, \"varInfoList\":[], "
        "\"nrRegs\":[], \"nrRegsHere\":[], \"regsToClear\":[], "
        "\"varsUsedLater\":[], \"varsValid\":[], \"outVariable\": { "
        "\"name\":\"variable\", \"id\":0 }, \"options\": { \"waitForSync\" : "
        "true, \"collection\":null }, \"viewId\": \"" +
        std::to_string(logicalView->id()) + "\" }");

    arangodb::iresearch::IResearchViewNode node(*query.plan(),  // plan
                                                json->slice());

    EXPECT_TRUE(node.empty());                // view has no links
    EXPECT_TRUE(node.collections().empty());  // view has no links
    EXPECT_TRUE(node.shards().empty());
    EXPECT_TRUE(!node.sort().first);  // primary sort is not set by default
    EXPECT_EQ(0, node.sort().second);  // primary sort is not set by default

    EXPECT_TRUE(arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW == node.getType());
    EXPECT_TRUE(outVariable.id == node.outVariable().id);
    EXPECT_TRUE(outVariable.name == node.outVariable().name);
    EXPECT_TRUE(query.plan() == node.plan());
    EXPECT_TRUE(42 == node.id());
    EXPECT_TRUE(logicalView == node.view());
    EXPECT_TRUE(node.scorers().empty());
    EXPECT_TRUE(!node.volatility().first);   // filter volatility
    EXPECT_TRUE(!node.volatility().second);  // sort volatility
    arangodb::HashSet<arangodb::aql::Variable const*> usedHere;
    node.getVariablesUsedHere(usedHere);
    EXPECT_TRUE(usedHere.empty());
    auto const setHere = node.getVariablesSetHere();
    EXPECT_TRUE(1 == setHere.size());
    EXPECT_TRUE(outVariable.id == setHere[0]->id);
    EXPECT_TRUE(outVariable.name == setHere[0]->name);
    EXPECT_TRUE(true == node.options().forceSync);
    EXPECT_TRUE(false == node.options().restrictSources);
    EXPECT_TRUE(0 == node.options().sources.size());

    EXPECT_TRUE(0. == node.getCost().estimatedCost);    // no dependencies
    EXPECT_TRUE(0 == node.getCost().estimatedNrItems);  // no dependencies
  }

  // invalid option 'waitForSync'
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"id\":42, \"depth\":0, \"totalNrRegs\":0, \"varInfoList\":[], "
        "\"nrRegs\":[], \"nrRegsHere\":[], \"regsToClear\":[], "
        "\"varsUsedLater\":[], \"varsValid\":[], \"outVariable\": { "
        "\"name\":\"variable\", \"id\":0 }, \"options\": { \"waitForSync\" : "
        "\"false\"}, \"viewId\": \"" +
        std::to_string(logicalView->id()) + "\" }");

    try {
      arangodb::iresearch::IResearchViewNode node(*query.plan(),  // plan
                                                  json->slice());
      EXPECT_TRUE(false);
    } catch (arangodb::basics::Exception const& e) {
      EXPECT_TRUE(TRI_ERROR_BAD_PARAMETER == e.code());
    } catch (...) {
      EXPECT_TRUE(false);
    }
  }

  // invalid option 'collections'
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"id\":42, \"depth\":0, \"totalNrRegs\":0, \"varInfoList\":[], "
        "\"nrRegs\":[], \"nrRegsHere\":[], \"regsToClear\":[], "
        "\"varsUsedLater\":[], \"varsValid\":[], \"outVariable\": { "
        "\"name\":\"variable\", \"id\":0 }, \"options\": { \"collections\" : "
        "\"false\"}, \"viewId\": \"" +
        std::to_string(logicalView->id()) + "\" }");

    try {
      arangodb::iresearch::IResearchViewNode node(*query.plan(),  // plan
                                                  json->slice());
      EXPECT_TRUE(false);
    } catch (arangodb::basics::Exception const& e) {
      EXPECT_TRUE(TRI_ERROR_BAD_PARAMETER == e.code());
    } catch (...) {
      EXPECT_TRUE(false);
    }
  }

  // invalid option 'collections'
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"id\":42, \"depth\":0, \"totalNrRegs\":0, \"varInfoList\":[], "
        "\"nrRegs\":[], \"nrRegsHere\":[], \"regsToClear\":[], "
        "\"varsUsedLater\":[], \"varsValid\":[], \"outVariable\": { "
        "\"name\":\"variable\", \"id\":0 }, \"options\": { \"collections\" : "
        "{}}, \"viewId\": \"" +
        std::to_string(logicalView->id()) + "\" }");

    try {
      arangodb::iresearch::IResearchViewNode node(*query.plan(),  // plan
                                                  json->slice());
      EXPECT_TRUE(false);
    } catch (arangodb::basics::Exception const& e) {
      EXPECT_TRUE(TRI_ERROR_BAD_PARAMETER == e.code());
    } catch (...) {
      EXPECT_TRUE(false);
    }
  }

  // invalid option 'primarySort'
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"id\":42, \"depth\":0, \"totalNrRegs\":0, \"varInfoList\":[], "
        "\"nrRegs\":[], \"nrRegsHere\":[], \"regsToClear\":[], "
        "\"varsUsedLater\":[], \"varsValid\":[], \"outVariable\": { "
        "\"name\":\"variable\", \"id\":0 }, \"options\": { \"collections\" : "
        "{}}, \"viewId\": \"" +
        std::to_string(logicalView->id()) +
        "\", "
        "\"primarySort\": true }");

    try {
      arangodb::iresearch::IResearchViewNode node(*query.plan(),  // plan
                                                  json->slice());
      EXPECT_TRUE(false);
    } catch (arangodb::basics::Exception const& e) {
      EXPECT_TRUE(TRI_ERROR_BAD_PARAMETER == e.code());
    } catch (...) {
      EXPECT_TRUE(false);
    }
  }
}

// FIXME TODO
// TEST_F(IResearchViewNodeTest, constructFromVPackCluster) {
//}

TEST_F(IResearchViewNodeTest, clone) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                        "testVocbase");
  // create view
  auto createJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
  auto logicalView = vocbase.createView(createJson->slice());
  ASSERT_TRUE((false == !logicalView));

  // dummy query
  arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString("RETURN 1"),
                             nullptr, arangodb::velocypack::Parser::fromJson("{}"),
                             arangodb::aql::PART_MAIN);
  query.prepare(arangodb::QueryRegistryFeature::registry());
  arangodb::aql::Variable const outVariable("variable", 0);

  // no filter condition, no sort condition, no shards, no options
  {
    arangodb::iresearch::IResearchViewNode node(*query.plan(),
                                                42,           // id
                                                vocbase,      // database
                                                logicalView,  // view
                                                outVariable,
                                                nullptr,  // no filter condition
                                                nullptr,  // no options
                                                {}        // no sort condition
    );

    EXPECT_TRUE(node.empty());                // view has no links
    EXPECT_TRUE(node.collections().empty());  // view has no links
    EXPECT_TRUE(node.shards().empty());

    // clone without properties into the same plan
    {
      auto const nextId = node.plan()->nextId();
      auto& cloned = dynamic_cast<arangodb::iresearch::IResearchViewNode&>(
          *node.clone(query.plan(), true, false));
      EXPECT_TRUE(node.getType() == cloned.getType());
      EXPECT_TRUE(&node.outVariable() == &cloned.outVariable());  // same objects
      EXPECT_TRUE(node.plan() == cloned.plan());
      EXPECT_TRUE(nextId + 1 == cloned.id());
      EXPECT_TRUE(&node.vocbase() == &cloned.vocbase());
      EXPECT_TRUE(node.view() == cloned.view());
      EXPECT_TRUE(&node.filterCondition() == &cloned.filterCondition());
      EXPECT_TRUE(node.scorers() == cloned.scorers());
      EXPECT_TRUE(node.volatility() == cloned.volatility());
      EXPECT_TRUE(node.sort() == cloned.sort());

      EXPECT_TRUE(node.getCost() == cloned.getCost());
    }

    // clone with properties into another plan
    {
      // another dummy query
      arangodb::aql::Query otherQuery(false, vocbase, arangodb::aql::QueryString("RETURN 1"),
                                      nullptr, arangodb::velocypack::Parser::fromJson("{}"),
                                      arangodb::aql::PART_MAIN);
      otherQuery.prepare(arangodb::QueryRegistryFeature::registry());

      auto& cloned = dynamic_cast<arangodb::iresearch::IResearchViewNode&>(
          *node.clone(otherQuery.plan(), true, true));
      EXPECT_TRUE(node.getType() == cloned.getType());
      EXPECT_TRUE(&node.outVariable() != &cloned.outVariable());  // different objects
      EXPECT_TRUE(node.outVariable().id == cloned.outVariable().id);
      EXPECT_TRUE(node.outVariable().name == cloned.outVariable().name);
      EXPECT_TRUE(otherQuery.plan() == cloned.plan());
      EXPECT_TRUE(node.id() == cloned.id());
      EXPECT_TRUE(&node.vocbase() == &cloned.vocbase());
      EXPECT_TRUE(node.view() == cloned.view());
      EXPECT_TRUE(&node.filterCondition() == &cloned.filterCondition());
      EXPECT_TRUE(node.scorers() == cloned.scorers());
      EXPECT_TRUE(node.volatility() == cloned.volatility());
      EXPECT_TRUE(node.options().forceSync == cloned.options().forceSync);
      EXPECT_TRUE(node.sort() == cloned.sort());

      EXPECT_TRUE(node.getCost() == cloned.getCost());
    }

    // clone without properties into another plan
    {
      // another dummy query
      arangodb::aql::Query otherQuery(false, vocbase, arangodb::aql::QueryString("RETURN 1"),
                                      nullptr, arangodb::velocypack::Parser::fromJson("{}"),
                                      arangodb::aql::PART_MAIN);
      otherQuery.prepare(arangodb::QueryRegistryFeature::registry());

      node.plan()->nextId();
      auto& cloned = dynamic_cast<arangodb::iresearch::IResearchViewNode&>(
          *node.clone(otherQuery.plan(), true, false));
      EXPECT_TRUE(node.getType() == cloned.getType());
      EXPECT_TRUE(&node.outVariable() == &cloned.outVariable());  // same objects
      EXPECT_TRUE(otherQuery.plan() == cloned.plan());
      EXPECT_TRUE(node.id() == cloned.id());
      EXPECT_TRUE(&node.vocbase() == &cloned.vocbase());
      EXPECT_TRUE(node.view() == cloned.view());
      EXPECT_TRUE(&node.filterCondition() == &cloned.filterCondition());
      EXPECT_TRUE(node.scorers() == cloned.scorers());
      EXPECT_TRUE(node.volatility() == cloned.volatility());
      EXPECT_TRUE(node.options().forceSync == cloned.options().forceSync);
      EXPECT_TRUE(node.sort() == cloned.sort());

      EXPECT_TRUE(node.getCost() == cloned.getCost());
    }
  }

  // no filter condition, no sort condition, no shards, options
  {
    // build options node
    arangodb::aql::AstNode attributeValue(arangodb::aql::NODE_TYPE_VALUE);
    attributeValue.setValueType(arangodb::aql::VALUE_TYPE_BOOL);
    attributeValue.setBoolValue(true);
    arangodb::aql::AstNode attributeName(arangodb::aql::NODE_TYPE_OBJECT_ELEMENT);
    attributeName.addMember(&attributeValue);
    attributeName.setStringValue("waitForSync", strlen("waitForSync"));
    arangodb::aql::AstNode options(arangodb::aql::NODE_TYPE_OBJECT);
    options.addMember(&attributeName);

    arangodb::iresearch::IResearchViewNode node(*query.plan(),
                                                42,           // id
                                                vocbase,      // database
                                                logicalView,  // view
                                                outVariable,
                                                nullptr,  // no filter condition
                                                &options, {}  // no sort condition
    );

    EXPECT_TRUE(node.empty());                // view has no links
    EXPECT_TRUE(node.collections().empty());  // view has no links
    EXPECT_TRUE(node.shards().empty());
    EXPECT_TRUE(true == node.options().forceSync);

    // clone without properties into the same plan
    {
      auto const nextId = node.plan()->nextId();
      auto& cloned = dynamic_cast<arangodb::iresearch::IResearchViewNode&>(
          *node.clone(query.plan(), true, false));
      EXPECT_TRUE(node.getType() == cloned.getType());
      EXPECT_TRUE(&node.outVariable() == &cloned.outVariable());  // same objects
      EXPECT_TRUE(node.plan() == cloned.plan());
      EXPECT_TRUE(nextId + 1 == cloned.id());
      EXPECT_TRUE(&node.vocbase() == &cloned.vocbase());
      EXPECT_TRUE(node.view() == cloned.view());
      EXPECT_TRUE(&node.filterCondition() == &cloned.filterCondition());
      EXPECT_TRUE(node.scorers() == cloned.scorers());
      EXPECT_TRUE(node.volatility() == cloned.volatility());
      EXPECT_TRUE(node.sort() == cloned.sort());

      EXPECT_TRUE(node.getCost() == cloned.getCost());
    }

    // clone with properties into another plan
    {
      // another dummy query
      arangodb::aql::Query otherQuery(false, vocbase, arangodb::aql::QueryString("RETURN 1"),
                                      nullptr, arangodb::velocypack::Parser::fromJson("{}"),
                                      arangodb::aql::PART_MAIN);
      otherQuery.prepare(arangodb::QueryRegistryFeature::registry());

      auto& cloned = dynamic_cast<arangodb::iresearch::IResearchViewNode&>(
          *node.clone(otherQuery.plan(), true, true));
      EXPECT_TRUE(node.getType() == cloned.getType());
      EXPECT_TRUE(&node.outVariable() != &cloned.outVariable());  // different objects
      EXPECT_TRUE(node.outVariable().id == cloned.outVariable().id);
      EXPECT_TRUE(node.outVariable().name == cloned.outVariable().name);
      EXPECT_TRUE(otherQuery.plan() == cloned.plan());
      EXPECT_TRUE(node.id() == cloned.id());
      EXPECT_TRUE(&node.vocbase() == &cloned.vocbase());
      EXPECT_TRUE(node.view() == cloned.view());
      EXPECT_TRUE(&node.filterCondition() == &cloned.filterCondition());
      EXPECT_TRUE(node.scorers() == cloned.scorers());
      EXPECT_TRUE(node.volatility() == cloned.volatility());
      EXPECT_TRUE(node.options().forceSync == cloned.options().forceSync);
      EXPECT_TRUE(node.sort() == cloned.sort());

      EXPECT_TRUE(node.getCost() == cloned.getCost());
    }

    // clone without properties into another plan
    {
      // another dummy query
      arangodb::aql::Query otherQuery(false, vocbase, arangodb::aql::QueryString("RETURN 1"),
                                      nullptr, arangodb::velocypack::Parser::fromJson("{}"),
                                      arangodb::aql::PART_MAIN);
      otherQuery.prepare(arangodb::QueryRegistryFeature::registry());

      node.plan()->nextId();
      auto& cloned = dynamic_cast<arangodb::iresearch::IResearchViewNode&>(
          *node.clone(otherQuery.plan(), true, false));
      EXPECT_TRUE(node.getType() == cloned.getType());
      EXPECT_TRUE(&node.outVariable() == &cloned.outVariable());  // same objects
      EXPECT_TRUE(otherQuery.plan() == cloned.plan());
      EXPECT_TRUE(node.id() == cloned.id());
      EXPECT_TRUE(&node.vocbase() == &cloned.vocbase());
      EXPECT_TRUE(node.view() == cloned.view());
      EXPECT_TRUE(&node.filterCondition() == &cloned.filterCondition());
      EXPECT_TRUE(node.scorers() == cloned.scorers());
      EXPECT_TRUE(node.volatility() == cloned.volatility());
      EXPECT_TRUE(node.options().forceSync == cloned.options().forceSync);
      EXPECT_TRUE(node.sort() == cloned.sort());

      EXPECT_TRUE(node.getCost() == cloned.getCost());
    }
  }

  // no filter condition, no sort condition, with shards, no options
  {
    arangodb::iresearch::IResearchViewNode node(*query.plan(),
                                                42,           // id
                                                vocbase,      // database
                                                logicalView,  // view
                                                outVariable,
                                                nullptr,  // no filter condition
                                                nullptr,  // no options
                                                {}        // no sort condition
    );

    EXPECT_TRUE(node.empty());                // view has no links
    EXPECT_TRUE(node.collections().empty());  // view has no links
    EXPECT_TRUE(node.shards().empty());

    node.shards().emplace_back("abc");
    node.shards().emplace_back("def");

    // clone without properties into the same plan
    {
      auto const nextId = node.plan()->nextId();
      auto& cloned = dynamic_cast<arangodb::iresearch::IResearchViewNode&>(
          *node.clone(query.plan(), true, false));
      EXPECT_TRUE(cloned.collections().empty());
      EXPECT_TRUE(node.empty() == cloned.empty());
      EXPECT_TRUE(node.shards() == cloned.shards());
      EXPECT_TRUE(node.getType() == cloned.getType());
      EXPECT_TRUE(&node.outVariable() == &cloned.outVariable());  // same objects
      EXPECT_TRUE(node.plan() == cloned.plan());
      EXPECT_TRUE(nextId + 1 == cloned.id());
      EXPECT_TRUE(&node.vocbase() == &cloned.vocbase());
      EXPECT_TRUE(node.view() == cloned.view());
      EXPECT_TRUE(&node.filterCondition() == &cloned.filterCondition());
      EXPECT_TRUE(node.scorers() == cloned.scorers());
      EXPECT_TRUE(node.volatility() == cloned.volatility());
      EXPECT_TRUE(node.options().forceSync == cloned.options().forceSync);
      EXPECT_TRUE(node.sort() == cloned.sort());

      EXPECT_TRUE(node.getCost() == cloned.getCost());
    }

    // clone with properties into another plan
    {
      // another dummy query
      arangodb::aql::Query otherQuery(false, vocbase, arangodb::aql::QueryString("RETURN 1"),
                                      nullptr, arangodb::velocypack::Parser::fromJson("{}"),
                                      arangodb::aql::PART_MAIN);
      otherQuery.prepare(arangodb::QueryRegistryFeature::registry());

      auto& cloned = dynamic_cast<arangodb::iresearch::IResearchViewNode&>(
          *node.clone(otherQuery.plan(), true, true));
      EXPECT_TRUE(cloned.collections().empty());
      EXPECT_TRUE(node.empty() == cloned.empty());
      EXPECT_TRUE(node.shards() == cloned.shards());
      EXPECT_TRUE(node.getType() == cloned.getType());
      EXPECT_TRUE(&node.outVariable() != &cloned.outVariable());  // different objects
      EXPECT_TRUE(node.outVariable().id == cloned.outVariable().id);
      EXPECT_TRUE(node.outVariable().name == cloned.outVariable().name);
      EXPECT_TRUE(otherQuery.plan() == cloned.plan());
      EXPECT_TRUE(node.id() == cloned.id());
      EXPECT_TRUE(&node.vocbase() == &cloned.vocbase());
      EXPECT_TRUE(node.view() == cloned.view());
      EXPECT_TRUE(&node.filterCondition() == &cloned.filterCondition());
      EXPECT_TRUE(node.scorers() == cloned.scorers());
      EXPECT_TRUE(node.volatility() == cloned.volatility());
      EXPECT_TRUE(node.options().forceSync == cloned.options().forceSync);
      EXPECT_TRUE(node.sort() == cloned.sort());

      EXPECT_TRUE(node.getCost() == cloned.getCost());
    }

    // clone without properties into another plan
    {
      // another dummy query
      arangodb::aql::Query otherQuery(false, vocbase, arangodb::aql::QueryString("RETURN 1"),
                                      nullptr, arangodb::velocypack::Parser::fromJson("{}"),
                                      arangodb::aql::PART_MAIN);
      otherQuery.prepare(arangodb::QueryRegistryFeature::registry());

      node.plan()->nextId();
      auto& cloned = dynamic_cast<arangodb::iresearch::IResearchViewNode&>(
          *node.clone(otherQuery.plan(), true, false));
      EXPECT_TRUE(cloned.collections().empty());
      EXPECT_TRUE(node.empty() == cloned.empty());
      EXPECT_TRUE(node.shards() == cloned.shards());
      EXPECT_TRUE(node.getType() == cloned.getType());
      EXPECT_TRUE(&node.outVariable() == &cloned.outVariable());  // same objects
      EXPECT_TRUE(otherQuery.plan() == cloned.plan());
      EXPECT_TRUE(node.id() == cloned.id());
      EXPECT_TRUE(&node.vocbase() == &cloned.vocbase());
      EXPECT_TRUE(node.view() == cloned.view());
      EXPECT_TRUE(&node.filterCondition() == &cloned.filterCondition());
      EXPECT_TRUE(node.scorers() == cloned.scorers());
      EXPECT_TRUE(node.volatility() == cloned.volatility());
      EXPECT_TRUE(node.options().forceSync == cloned.options().forceSync);
      EXPECT_TRUE(node.sort() == cloned.sort());

      EXPECT_TRUE(node.getCost() == cloned.getCost());
    }
  }

  // no filter condition, sort condition, with shards, no options
  {
    arangodb::iresearch::IResearchViewSort sort;
    arangodb::iresearch::IResearchViewNode node(*query.plan(),
                                                42,           // id
                                                vocbase,      // database
                                                logicalView,  // view
                                                outVariable,
                                                nullptr,  // no filter condition
                                                nullptr,  // no options
                                                {}        // no scorers
    );
    node.sort(&sort, 0);

    EXPECT_TRUE(node.empty());                // view has no links
    EXPECT_TRUE(node.collections().empty());  // view has no links
    EXPECT_TRUE(node.shards().empty());

    node.shards().emplace_back("abc");
    node.shards().emplace_back("def");

    // clone without properties into the same plan
    {
      auto const nextId = node.plan()->nextId();
      auto& cloned = dynamic_cast<arangodb::iresearch::IResearchViewNode&>(
          *node.clone(query.plan(), true, false));
      EXPECT_TRUE(cloned.collections().empty());
      EXPECT_TRUE(node.empty() == cloned.empty());
      EXPECT_TRUE(node.shards() == cloned.shards());
      EXPECT_TRUE(node.getType() == cloned.getType());
      EXPECT_TRUE(&node.outVariable() == &cloned.outVariable());  // same objects
      EXPECT_TRUE(node.plan() == cloned.plan());
      EXPECT_TRUE(nextId + 1 == cloned.id());
      EXPECT_TRUE(&node.vocbase() == &cloned.vocbase());
      EXPECT_TRUE(node.view() == cloned.view());
      EXPECT_TRUE(&node.filterCondition() == &cloned.filterCondition());
      EXPECT_TRUE(node.scorers() == cloned.scorers());
      EXPECT_TRUE(node.volatility() == cloned.volatility());
      EXPECT_TRUE(node.options().forceSync == cloned.options().forceSync);
      EXPECT_TRUE(node.sort() == cloned.sort());

      EXPECT_TRUE(node.getCost() == cloned.getCost());
    }

    // clone with properties into another plan
    {
      // another dummy query
      arangodb::aql::Query otherQuery(false, vocbase, arangodb::aql::QueryString("RETURN 1"),
                                      nullptr, arangodb::velocypack::Parser::fromJson("{}"),
                                      arangodb::aql::PART_MAIN);
      otherQuery.prepare(arangodb::QueryRegistryFeature::registry());

      auto& cloned = dynamic_cast<arangodb::iresearch::IResearchViewNode&>(
          *node.clone(otherQuery.plan(), true, true));
      EXPECT_TRUE(cloned.collections().empty());
      EXPECT_TRUE(node.empty() == cloned.empty());
      EXPECT_TRUE(node.shards() == cloned.shards());
      EXPECT_TRUE(node.getType() == cloned.getType());
      EXPECT_TRUE(&node.outVariable() != &cloned.outVariable());  // different objects
      EXPECT_TRUE(node.outVariable().id == cloned.outVariable().id);
      EXPECT_TRUE(node.outVariable().name == cloned.outVariable().name);
      EXPECT_TRUE(otherQuery.plan() == cloned.plan());
      EXPECT_TRUE(node.id() == cloned.id());
      EXPECT_TRUE(&node.vocbase() == &cloned.vocbase());
      EXPECT_TRUE(node.view() == cloned.view());
      EXPECT_TRUE(&node.filterCondition() == &cloned.filterCondition());
      EXPECT_TRUE(node.scorers() == cloned.scorers());
      EXPECT_TRUE(node.volatility() == cloned.volatility());
      EXPECT_TRUE(node.options().forceSync == cloned.options().forceSync);
      EXPECT_TRUE(node.sort() == cloned.sort());

      EXPECT_TRUE(node.getCost() == cloned.getCost());
    }

    // clone without properties into another plan
    {
      // another dummy query
      arangodb::aql::Query otherQuery(false, vocbase, arangodb::aql::QueryString("RETURN 1"),
                                      nullptr, arangodb::velocypack::Parser::fromJson("{}"),
                                      arangodb::aql::PART_MAIN);
      otherQuery.prepare(arangodb::QueryRegistryFeature::registry());

      node.plan()->nextId();
      auto& cloned = dynamic_cast<arangodb::iresearch::IResearchViewNode&>(
          *node.clone(otherQuery.plan(), true, false));
      EXPECT_TRUE(cloned.collections().empty());
      EXPECT_TRUE(node.empty() == cloned.empty());
      EXPECT_TRUE(node.shards() == cloned.shards());
      EXPECT_TRUE(node.getType() == cloned.getType());
      EXPECT_TRUE(&node.outVariable() == &cloned.outVariable());  // same objects
      EXPECT_TRUE(otherQuery.plan() == cloned.plan());
      EXPECT_TRUE(node.id() == cloned.id());
      EXPECT_TRUE(&node.vocbase() == &cloned.vocbase());
      EXPECT_TRUE(node.view() == cloned.view());
      EXPECT_TRUE(&node.filterCondition() == &cloned.filterCondition());
      EXPECT_TRUE(node.scorers() == cloned.scorers());
      EXPECT_TRUE(node.volatility() == cloned.volatility());
      EXPECT_TRUE(node.options().forceSync == cloned.options().forceSync);
      EXPECT_TRUE(node.sort() == cloned.sort());

      EXPECT_TRUE(node.getCost() == cloned.getCost());
    }
  }
}

TEST_F(IResearchViewNodeTest, serialize) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                        "testVocbase");
  // create view
  auto createJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
  auto logicalView = vocbase.createView(createJson->slice());
  ASSERT_TRUE((false == !logicalView));

  // dummy query
  arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString("RETURN 1"),
                             nullptr, arangodb::velocypack::Parser::fromJson("{}"),
                             arangodb::aql::PART_MAIN);
  query.prepare(arangodb::QueryRegistryFeature::registry());

  arangodb::aql::Variable const outVariable("variable", 0);

  // no filter condition, no sort condition
  {
    arangodb::iresearch::IResearchViewNode node(*query.plan(),
                                                42,           // id
                                                vocbase,      // database
                                                logicalView,  // view
                                                outVariable,
                                                nullptr,  // no filter condition
                                                nullptr,  // no options
                                                {}        // no sort condition
    );

    EXPECT_TRUE(node.empty());                // view has no links
    EXPECT_TRUE(node.collections().empty());  // view has no links
    EXPECT_TRUE(node.shards().empty());

    arangodb::velocypack::Builder builder;
    unsigned flags = arangodb::aql::ExecutionNode::SERIALIZE_DETAILS;
    node.toVelocyPack(builder, flags, false);  // object with array of objects

    auto const slice = builder.slice();
    EXPECT_TRUE(slice.isObject());
    auto const nodesSlice = slice.get("nodes");
    EXPECT_TRUE(nodesSlice.isArray());
    arangodb::velocypack::ArrayIterator it(nodesSlice);
    EXPECT_TRUE(1 == it.size());
    auto nodeSlice = it.value();

    // constructor
    {
      arangodb::iresearch::IResearchViewNode const deserialized(*query.plan(), nodeSlice);
      EXPECT_TRUE(node.empty() == deserialized.empty());
      EXPECT_TRUE(node.shards() == deserialized.shards());
      EXPECT_TRUE(deserialized.collections().empty());
      EXPECT_TRUE(node.getType() == deserialized.getType());
      EXPECT_TRUE(node.outVariable().id == deserialized.outVariable().id);
      EXPECT_TRUE(node.outVariable().name == deserialized.outVariable().name);
      EXPECT_TRUE(node.plan() == deserialized.plan());
      EXPECT_TRUE(node.id() == deserialized.id());
      EXPECT_TRUE(&node.vocbase() == &deserialized.vocbase());
      EXPECT_TRUE(node.view() == deserialized.view());
      EXPECT_TRUE(&node.filterCondition() == &deserialized.filterCondition());
      EXPECT_TRUE(node.scorers() == deserialized.scorers());
      EXPECT_TRUE(node.volatility() == deserialized.volatility());
      EXPECT_TRUE(node.options().forceSync == deserialized.options().forceSync);
      EXPECT_TRUE(node.sort() == deserialized.sort());

      EXPECT_TRUE(node.getCost() == deserialized.getCost());
    }

    // factory method
    {
      std::unique_ptr<arangodb::aql::ExecutionNode> deserializedNode(
          arangodb::aql::ExecutionNode::fromVPackFactory(query.plan(), nodeSlice));
      auto& deserialized =
          dynamic_cast<arangodb::iresearch::IResearchViewNode&>(*deserializedNode);
      EXPECT_TRUE(node.empty() == deserialized.empty());
      EXPECT_TRUE(node.shards() == deserialized.shards());
      EXPECT_TRUE(deserialized.collections().empty());
      EXPECT_TRUE(node.getType() == deserialized.getType());
      EXPECT_TRUE(node.outVariable().id == deserialized.outVariable().id);
      EXPECT_TRUE(node.outVariable().name == deserialized.outVariable().name);
      EXPECT_TRUE(node.plan() == deserialized.plan());
      EXPECT_TRUE(node.id() == deserialized.id());
      EXPECT_TRUE(&node.vocbase() == &deserialized.vocbase());
      EXPECT_TRUE(node.view() == deserialized.view());
      EXPECT_TRUE(&node.filterCondition() == &deserialized.filterCondition());
      EXPECT_TRUE(node.scorers() == deserialized.scorers());
      EXPECT_TRUE(node.volatility() == deserialized.volatility());
      EXPECT_TRUE(node.options().forceSync == deserialized.options().forceSync);
      EXPECT_TRUE(node.sort() == deserialized.sort());

      EXPECT_TRUE(node.getCost() == deserialized.getCost());
    }
  }

  // no filter condition, no sort condition, options
  {
    // build options node
    arangodb::aql::AstNode attributeValue(arangodb::aql::NODE_TYPE_VALUE);
    attributeValue.setValueType(arangodb::aql::VALUE_TYPE_BOOL);
    attributeValue.setBoolValue(true);
    arangodb::aql::AstNode attributeName(arangodb::aql::NODE_TYPE_OBJECT_ELEMENT);
    attributeName.addMember(&attributeValue);
    attributeName.setStringValue("waitForSync", strlen("waitForSync"));
    arangodb::aql::AstNode options(arangodb::aql::NODE_TYPE_OBJECT);
    options.addMember(&attributeName);

    arangodb::iresearch::IResearchViewNode node(*query.plan(),
                                                42,           // id
                                                vocbase,      // database
                                                logicalView,  // view
                                                outVariable,
                                                nullptr,  // no filter condition
                                                &options, {}  // no sort condition
    );

    EXPECT_TRUE(node.empty());                // view has no links
    EXPECT_TRUE(node.collections().empty());  // view has no links
    EXPECT_TRUE(node.shards().empty());
    EXPECT_TRUE(true == node.options().forceSync);

    arangodb::velocypack::Builder builder;
    unsigned flags = arangodb::aql::ExecutionNode::SERIALIZE_DETAILS;
    node.toVelocyPack(builder, flags, false);  // object with array of objects

    auto const slice = builder.slice();
    EXPECT_TRUE(slice.isObject());
    auto const nodesSlice = slice.get("nodes");
    EXPECT_TRUE(nodesSlice.isArray());
    arangodb::velocypack::ArrayIterator it(nodesSlice);
    EXPECT_TRUE(1 == it.size());
    auto nodeSlice = it.value();

    // constructor
    {
      arangodb::iresearch::IResearchViewNode const deserialized(*query.plan(), nodeSlice);
      EXPECT_TRUE(node.empty() == deserialized.empty());
      EXPECT_TRUE(node.shards() == deserialized.shards());
      EXPECT_TRUE(deserialized.collections().empty());
      EXPECT_TRUE(node.getType() == deserialized.getType());
      EXPECT_TRUE(node.outVariable().id == deserialized.outVariable().id);
      EXPECT_TRUE(node.outVariable().name == deserialized.outVariable().name);
      EXPECT_TRUE(node.plan() == deserialized.plan());
      EXPECT_TRUE(node.id() == deserialized.id());
      EXPECT_TRUE(&node.vocbase() == &deserialized.vocbase());
      EXPECT_TRUE(node.view() == deserialized.view());
      EXPECT_TRUE(&node.filterCondition() == &deserialized.filterCondition());
      EXPECT_TRUE(node.scorers() == deserialized.scorers());
      EXPECT_TRUE(node.volatility() == deserialized.volatility());
      EXPECT_TRUE(node.options().forceSync == deserialized.options().forceSync);
      EXPECT_TRUE(node.sort() == deserialized.sort());

      EXPECT_TRUE(node.getCost() == deserialized.getCost());
    }

    // factory method
    {
      std::unique_ptr<arangodb::aql::ExecutionNode> deserializedNode(
          arangodb::aql::ExecutionNode::fromVPackFactory(query.plan(), nodeSlice));
      auto& deserialized =
          dynamic_cast<arangodb::iresearch::IResearchViewNode&>(*deserializedNode);
      EXPECT_TRUE(node.empty() == deserialized.empty());
      EXPECT_TRUE(node.shards() == deserialized.shards());
      EXPECT_TRUE(deserialized.collections().empty());
      EXPECT_TRUE(node.getType() == deserialized.getType());
      EXPECT_TRUE(node.outVariable().id == deserialized.outVariable().id);
      EXPECT_TRUE(node.outVariable().name == deserialized.outVariable().name);
      EXPECT_TRUE(node.plan() == deserialized.plan());
      EXPECT_TRUE(node.id() == deserialized.id());
      EXPECT_TRUE(&node.vocbase() == &deserialized.vocbase());
      EXPECT_TRUE(node.view() == deserialized.view());
      EXPECT_TRUE(&node.filterCondition() == &deserialized.filterCondition());
      EXPECT_TRUE(node.scorers() == deserialized.scorers());
      EXPECT_TRUE(node.volatility() == deserialized.volatility());
      EXPECT_TRUE(node.options().forceSync == deserialized.options().forceSync);
      EXPECT_TRUE(node.sort() == deserialized.sort());

      EXPECT_TRUE(node.getCost() == deserialized.getCost());
    }
  }
}

TEST_F(IResearchViewNodeTest, serializeSortedView) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                        "testVocbase");
  // create view
  auto createJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", \"type\": \"arangosearch\", \"primarySort\" : "
      "[ { \"field\":\"_key\", \"direction\":\"desc\"} ] }");
  auto logicalView = vocbase.createView(createJson->slice());
  ASSERT_TRUE((false == !logicalView));
  auto& viewImpl =
      arangodb::LogicalView::cast<arangodb::iresearch::IResearchView>(*logicalView);
  EXPECT_TRUE(!viewImpl.primarySort().empty());

  // dummy query
  arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString("RETURN 1"),
                             nullptr, arangodb::velocypack::Parser::fromJson("{}"),
                             arangodb::aql::PART_MAIN);
  query.prepare(arangodb::QueryRegistryFeature::registry());

  arangodb::aql::Variable const outVariable("variable", 0);

  // no filter condition, no sort condition
  {
    arangodb::iresearch::IResearchViewNode node(*query.plan(),
                                                42,           // id
                                                vocbase,      // database
                                                logicalView,  // view
                                                outVariable,
                                                nullptr,  // no filter condition
                                                nullptr,  // no options
                                                {}        // no sort condition
    );
    node.sort(&viewImpl.primarySort(), 1);

    EXPECT_TRUE(node.empty());                // view has no links
    EXPECT_TRUE(node.collections().empty());  // view has no links
    EXPECT_TRUE(node.shards().empty());

    arangodb::velocypack::Builder builder;
    unsigned flags = arangodb::aql::ExecutionNode::SERIALIZE_DETAILS;
    node.toVelocyPack(builder, flags, false);  // object with array of objects

    auto const slice = builder.slice();
    EXPECT_TRUE(slice.isObject());
    auto const nodesSlice = slice.get("nodes");
    EXPECT_TRUE(nodesSlice.isArray());
    arangodb::velocypack::ArrayIterator it(nodesSlice);
    EXPECT_TRUE(1 == it.size());
    auto nodeSlice = it.value();

    // constructor
    {
      arangodb::iresearch::IResearchViewNode const deserialized(*query.plan(), nodeSlice);
      EXPECT_TRUE(node.empty() == deserialized.empty());
      EXPECT_TRUE(node.shards() == deserialized.shards());
      EXPECT_TRUE(deserialized.collections().empty());
      EXPECT_TRUE(node.getType() == deserialized.getType());
      EXPECT_TRUE(node.outVariable().id == deserialized.outVariable().id);
      EXPECT_TRUE(node.outVariable().name == deserialized.outVariable().name);
      EXPECT_TRUE(node.plan() == deserialized.plan());
      EXPECT_TRUE(node.id() == deserialized.id());
      EXPECT_TRUE(&node.vocbase() == &deserialized.vocbase());
      EXPECT_TRUE(node.view() == deserialized.view());
      EXPECT_TRUE(&node.filterCondition() == &deserialized.filterCondition());
      EXPECT_TRUE(node.scorers() == deserialized.scorers());
      EXPECT_TRUE(node.volatility() == deserialized.volatility());
      EXPECT_TRUE(node.options().forceSync == deserialized.options().forceSync);
      EXPECT_TRUE(node.sort() == deserialized.sort());
      EXPECT_EQ(1, node.sort().second);

      EXPECT_TRUE(node.getCost() == deserialized.getCost());
    }

    // factory method
    {
      std::unique_ptr<arangodb::aql::ExecutionNode> deserializedNode(
          arangodb::aql::ExecutionNode::fromVPackFactory(query.plan(), nodeSlice));
      auto& deserialized =
          dynamic_cast<arangodb::iresearch::IResearchViewNode&>(*deserializedNode);
      EXPECT_TRUE(node.empty() == deserialized.empty());
      EXPECT_TRUE(node.shards() == deserialized.shards());
      EXPECT_TRUE(deserialized.collections().empty());
      EXPECT_TRUE(node.getType() == deserialized.getType());
      EXPECT_TRUE(node.outVariable().id == deserialized.outVariable().id);
      EXPECT_TRUE(node.outVariable().name == deserialized.outVariable().name);
      EXPECT_TRUE(node.plan() == deserialized.plan());
      EXPECT_TRUE(node.id() == deserialized.id());
      EXPECT_TRUE(&node.vocbase() == &deserialized.vocbase());
      EXPECT_TRUE(node.view() == deserialized.view());
      EXPECT_TRUE(&node.filterCondition() == &deserialized.filterCondition());
      EXPECT_TRUE(node.scorers() == deserialized.scorers());
      EXPECT_TRUE(node.volatility() == deserialized.volatility());
      EXPECT_TRUE(node.options().forceSync == deserialized.options().forceSync);
      EXPECT_TRUE(node.sort() == deserialized.sort());
      EXPECT_EQ(1, node.sort().second);

      EXPECT_TRUE(node.getCost() == deserialized.getCost());
    }
  }

  // no filter condition, no sort condition, options
  {
    // build options node
    arangodb::aql::AstNode attributeValue(arangodb::aql::NODE_TYPE_VALUE);
    attributeValue.setValueType(arangodb::aql::VALUE_TYPE_BOOL);
    attributeValue.setBoolValue(true);
    arangodb::aql::AstNode attributeName(arangodb::aql::NODE_TYPE_OBJECT_ELEMENT);
    attributeName.addMember(&attributeValue);
    attributeName.setStringValue("waitForSync", strlen("waitForSync"));
    arangodb::aql::AstNode options(arangodb::aql::NODE_TYPE_OBJECT);
    options.addMember(&attributeName);

    arangodb::iresearch::IResearchViewNode node(*query.plan(),
                                                42,           // id
                                                vocbase,      // database
                                                logicalView,  // view
                                                outVariable,
                                                nullptr,  // no filter condition
                                                &options, {}  // no sort condition
    );

    EXPECT_TRUE(node.empty());                // view has no links
    EXPECT_TRUE(node.collections().empty());  // view has no links
    EXPECT_TRUE(node.shards().empty());
    EXPECT_TRUE(true == node.options().forceSync);

    arangodb::velocypack::Builder builder;
    unsigned flags = arangodb::aql::ExecutionNode::SERIALIZE_DETAILS;
    node.toVelocyPack(builder, flags, false);  // object with array of objects

    auto const slice = builder.slice();
    EXPECT_TRUE(slice.isObject());
    auto const nodesSlice = slice.get("nodes");
    EXPECT_TRUE(nodesSlice.isArray());
    arangodb::velocypack::ArrayIterator it(nodesSlice);
    EXPECT_TRUE(1 == it.size());
    auto nodeSlice = it.value();

    // constructor
    {
      arangodb::iresearch::IResearchViewNode const deserialized(*query.plan(), nodeSlice);
      EXPECT_TRUE(node.empty() == deserialized.empty());
      EXPECT_TRUE(node.shards() == deserialized.shards());
      EXPECT_TRUE(deserialized.collections().empty());
      EXPECT_TRUE(node.getType() == deserialized.getType());
      EXPECT_TRUE(node.outVariable().id == deserialized.outVariable().id);
      EXPECT_TRUE(node.outVariable().name == deserialized.outVariable().name);
      EXPECT_TRUE(node.plan() == deserialized.plan());
      EXPECT_TRUE(node.id() == deserialized.id());
      EXPECT_TRUE(&node.vocbase() == &deserialized.vocbase());
      EXPECT_TRUE(node.view() == deserialized.view());
      EXPECT_TRUE(&node.filterCondition() == &deserialized.filterCondition());
      EXPECT_TRUE(node.scorers() == deserialized.scorers());
      EXPECT_TRUE(node.volatility() == deserialized.volatility());
      EXPECT_TRUE(node.options().forceSync == deserialized.options().forceSync);
      EXPECT_TRUE(node.sort() == deserialized.sort());
      EXPECT_EQ(0, node.sort().second);

      EXPECT_TRUE(node.getCost() == deserialized.getCost());
    }

    // factory method
    {
      std::unique_ptr<arangodb::aql::ExecutionNode> deserializedNode(
          arangodb::aql::ExecutionNode::fromVPackFactory(query.plan(), nodeSlice));
      auto& deserialized =
          dynamic_cast<arangodb::iresearch::IResearchViewNode&>(*deserializedNode);
      EXPECT_TRUE(node.empty() == deserialized.empty());
      EXPECT_TRUE(node.shards() == deserialized.shards());
      EXPECT_TRUE(deserialized.collections().empty());
      EXPECT_TRUE(node.getType() == deserialized.getType());
      EXPECT_TRUE(node.outVariable().id == deserialized.outVariable().id);
      EXPECT_TRUE(node.outVariable().name == deserialized.outVariable().name);
      EXPECT_TRUE(node.plan() == deserialized.plan());
      EXPECT_TRUE(node.id() == deserialized.id());
      EXPECT_TRUE(&node.vocbase() == &deserialized.vocbase());
      EXPECT_TRUE(node.view() == deserialized.view());
      EXPECT_TRUE(&node.filterCondition() == &deserialized.filterCondition());
      EXPECT_TRUE(node.scorers() == deserialized.scorers());
      EXPECT_TRUE(node.volatility() == deserialized.volatility());
      EXPECT_TRUE(node.options().forceSync == deserialized.options().forceSync);
      EXPECT_TRUE(node.sort() == deserialized.sort());
      EXPECT_EQ(0, node.sort().second);

      EXPECT_TRUE(node.getCost() == deserialized.getCost());
    }
  }
}

TEST_F(IResearchViewNodeTest, collections) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                        "testVocbase");

  std::shared_ptr<arangodb::LogicalCollection> collection0;
  std::shared_ptr<arangodb::LogicalCollection> collection1;

  // create collection0
  {
    auto createJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection0\", \"id\" : \"42\" }");
    collection0 = vocbase.createCollection(createJson->slice());
    ASSERT_TRUE((nullptr != collection0));
  }

  // create collection1
  {
    auto createJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection1\", \"id\" : \"4242\"  }");
    collection1 = vocbase.createCollection(createJson->slice());
    ASSERT_TRUE((nullptr != collection1));
  }

  // create collection2
  {
    auto createJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection2\" , \"id\" : \"424242\" }");
    ASSERT_TRUE(nullptr != vocbase.createCollection(createJson->slice()));
  }

  // create view
  auto createJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
  auto logicalView = vocbase.createView(createJson->slice());
  ASSERT_TRUE((false == !logicalView));

  // link collections
  auto updateJson = arangodb::velocypack::Parser::fromJson(
      "{ \"links\": {"
      "\"testCollection0\": { \"includeAllFields\": true, "
      "\"trackListPositions\": true },"
      "\"testCollection1\": { \"includeAllFields\": true },"
      "\"testCollection2\": { \"includeAllFields\": true }"
      "}}");
  EXPECT_TRUE((logicalView->properties(updateJson->slice(), true).ok()));

  // dummy query
  arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString("RETURN 1"),
                             nullptr, arangodb::velocypack::Parser::fromJson("{}"),
                             arangodb::aql::PART_MAIN);

  // register collections with the query
  query.addCollection(std::to_string(collection0->id()), arangodb::AccessMode::Type::READ);
  query.addCollection(std::to_string(collection1->id()), arangodb::AccessMode::Type::READ);

  // prepare query
  query.prepare(arangodb::QueryRegistryFeature::registry());

  arangodb::aql::Variable const outVariable("variable", 0);

  // no filter condition, no sort condition
  arangodb::iresearch::IResearchViewNode node(*query.plan(),
                                              42,           // id
                                              vocbase,      // database
                                              logicalView,  // view
                                              outVariable,
                                              nullptr,  // no filter condition
                                              nullptr,  // no options
                                              {}        // no sort condition
  );

  EXPECT_TRUE(node.shards().empty());
  EXPECT_TRUE(!node.empty());  // view has no links
  auto collections = node.collections();
  EXPECT_TRUE(!collections.empty());  // view has no links
  EXPECT_TRUE(2 == collections.size());

  // we expect only collections 'collection0', 'collection1' to be
  // present since 'collection2' is not registered with the query
  std::unordered_set<std::string> expectedCollections{std::to_string(collection0->id()),
                                                      std::to_string(collection1->id())};

  for (arangodb::aql::Collection const& collection : collections) {
    EXPECT_TRUE(1 == expectedCollections.erase(collection.name()));
  }
  EXPECT_TRUE(expectedCollections.empty());
}

TEST_F(IResearchViewNodeTest, createBlockSingleServer) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                        "testVocbase");
  auto createJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
  auto logicalView = vocbase.createView(createJson->slice());
  ASSERT_TRUE((false == !logicalView));

  // create collection0
  std::shared_ptr<arangodb::LogicalCollection> collection0;
  {
    auto createJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection0\", \"id\" : \"42\" }");
    collection0 = vocbase.createCollection(createJson->slice());
    ASSERT_TRUE((nullptr != collection0));
  }

  // link collections
  auto updateJson = arangodb::velocypack::Parser::fromJson(
      "{ \"links\": {"
      "\"testCollection0\": { \"includeAllFields\": true, "
      "\"trackListPositions\": true }"
      "}}");
  EXPECT_TRUE((logicalView->properties(updateJson->slice(), true).ok()));

  // insert into collection
  {
    std::vector<std::string> const EMPTY;

    arangodb::OperationOptions opt;
    arangodb::ManagedDocumentResult mmdoc;

    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                       EMPTY, EMPTY, EMPTY,
                                       arangodb::transaction::Options());
    EXPECT_TRUE((trx.begin().ok()));

    auto json = arangodb::velocypack::Parser::fromJson("{}");
    auto const res = collection0->insert(&trx, json->slice(), mmdoc, opt, false);
    EXPECT_TRUE(res.ok());

    EXPECT_TRUE((trx.commit().ok()));
    EXPECT_TRUE(
        (arangodb::tests::executeQuery(vocbase,
                                       "FOR d IN testView SEARCH 1 ==1 OPTIONS "
                                       "{ waitForSync: true } RETURN d")
             .result.ok()));  // commit
  }

  // dummy query
  arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString("RETURN 1"),
                             nullptr, arangodb::velocypack::Parser::fromJson("{}"),
                             arangodb::aql::PART_MAIN);
  query.prepare(arangodb::QueryRegistryFeature::registry());

  // dummy engine
  arangodb::aql::ExecutionEngine engine(&query);

  arangodb::aql::Variable const outVariable("variable", 0);

  // no filter condition, no sort condition
  {
    arangodb::aql::SingletonNode singleton(query.plan(), 0);
    arangodb::iresearch::IResearchViewNode node(*query.plan(),
                                                42,           // id
                                                vocbase,      // database
                                                logicalView,  // view
                                                outVariable,
                                                nullptr,  // no filter condition
                                                nullptr,  // no options
                                                {}        // no sort condition
    );
    node.addDependency(&singleton);

    // "Trust me, I'm an IT professional"
    singleton.setVarUsageValid();
    node.setVarUsageValid();

    node.planRegisters(nullptr);

    std::unordered_map<arangodb::aql::ExecutionNode*, arangodb::aql::ExecutionBlock*> EMPTY;

    // before transaction has started (no snapshot)
    try {
      auto block = node.createBlock(engine, EMPTY);
      EXPECT_TRUE(false);
    } catch (arangodb::basics::Exception const& e) {
      EXPECT_TRUE(TRI_ERROR_INTERNAL == e.code());
    }

    // start transaction (put snapshot into)
    ASSERT_TRUE(query.trx()->state());
    EXPECT_TRUE(nullptr ==
                arangodb::LogicalView::cast<arangodb::iresearch::IResearchView>(*logicalView)
                    .snapshot(*query.trx(),
                              arangodb::iresearch::IResearchView::SnapshotMode::Find));
    auto* snapshot =
        arangodb::LogicalView::cast<arangodb::iresearch::IResearchView>(*logicalView)
            .snapshot(*query.trx(), arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate);
    EXPECT_TRUE(snapshot ==
                arangodb::LogicalView::cast<arangodb::iresearch::IResearchView>(*logicalView)
                    .snapshot(*query.trx(),
                              arangodb::iresearch::IResearchView::SnapshotMode::Find));
    EXPECT_TRUE(snapshot ==
                arangodb::LogicalView::cast<arangodb::iresearch::IResearchView>(*logicalView)
                    .snapshot(*query.trx(),
                              arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate));
    EXPECT_TRUE(snapshot ==
                arangodb::LogicalView::cast<arangodb::iresearch::IResearchView>(*logicalView)
                    .snapshot(*query.trx(),
                              arangodb::iresearch::IResearchView::SnapshotMode::SyncAndReplace));

    // after transaction has started
    {
      auto block = node.createBlock(engine, EMPTY);
      EXPECT_TRUE(nullptr != block);
      EXPECT_TRUE(nullptr !=
                  dynamic_cast<arangodb::aql::ExecutionBlockImpl<arangodb::aql::IResearchViewExecutor<false>>*>(
                      block.get()));
    }
  }
}

// FIXME TODO
// TEST_F(IResearchViewNodeTest, createBlockDBServer) {
//}

TEST_F(IResearchViewNodeTest, createBlockCoordinator) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                        "testVocbase");
  auto createJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
  auto logicalView = vocbase.createView(createJson->slice());
  ASSERT_TRUE((false == !logicalView));

  // dummy query
  arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString("RETURN 1"),
                             nullptr, arangodb::velocypack::Parser::fromJson("{}"),
                             arangodb::aql::PART_MAIN);
  query.prepare(arangodb::QueryRegistryFeature::registry());

  // dummy engine
  arangodb::aql::ExecutionEngine engine(&query);
  arangodb::aql::SingletonNode singleton(query.plan(), 0);
  arangodb::aql::Variable const outVariable("variable", 0);

  // no filter condition, no sort condition
  arangodb::iresearch::IResearchViewNode node(*query.plan(),
                                              42,           // id
                                              vocbase,      // database
                                              logicalView,  // view
                                              outVariable,
                                              nullptr,  // no filter condition
                                              nullptr,  // no options
                                              {}        // no sort condition
  );
  node.addDependency(&singleton);

  std::unordered_map<arangodb::aql::ExecutionNode*, arangodb::aql::ExecutionBlock*> EMPTY;
  singleton.setVarUsageValid();
  node.setVarUsageValid();
  singleton.planRegisters(nullptr);
  node.planRegisters(nullptr);
  arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_COORDINATOR);
  auto emptyBlock = node.createBlock(engine, EMPTY);
  arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_SINGLE);
  EXPECT_TRUE(nullptr != emptyBlock);
  EXPECT_TRUE(nullptr !=
              dynamic_cast<arangodb::aql::ExecutionBlockImpl<arangodb::aql::NoResultsExecutor>*>(
                  emptyBlock.get()));
}

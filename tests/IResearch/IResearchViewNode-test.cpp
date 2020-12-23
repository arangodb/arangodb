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

#include "gtest/gtest.h"

#include "analysis/analyzers.hpp"
#include "analysis/token_attributes.hpp"

#include "velocypack/Iterator.h"

#include "IResearch/common.h"
#include "Mocks/IResearchLinkMock.h"
#include "Mocks/LogLevels.h"
#include "Mocks/Servers.h"
#include "Mocks/StorageEngineMock.h"

#include "Aql/AqlFunctionFeature.h"
#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/IResearchViewExecutor.h"
#include "Aql/IResearchViewNode.h"
#include "Aql/NoResultsExecutor.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Aql/Query.h"
#include "Aql/RegisterPlan.h"
#include "Aql/SingleRowFetcher.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterFeature.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/ApplicationServerHelper.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchLinkMeta.h"
#include "IResearch/IResearchView.h"
#include "Logger/LogTopic.h"
#include "Logger/Logger.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/FlushFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "Sharding/ShardingFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "VocBase/ManagedDocumentResult.h"

#if USE_ENTERPRISE
#include "Enterprise/Ldap/LdapFeature.h"
#endif

namespace {

class IResearchViewNodeTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION, arangodb::LogLevel::ERR> {
 protected:
  arangodb::tests::mocks::MockAqlServer server;

  IResearchViewNodeTest() : server(false) {
    arangodb::tests::init(true);

    server.addFeature<arangodb::FlushFeature>(false);
    server.startFeatures();

    auto& dbPathFeature = server.getFeature<arangodb::DatabasePathFeature>();
    arangodb::tests::setDatabasePath(dbPathFeature);  // ensure test data is stored in a unique directory
  }
};  // IResearchViewNodeSetup


struct MockQuery final : arangodb::aql::Query {
  MockQuery(std::shared_ptr<arangodb::transaction::Context> const& ctx,
        arangodb::aql::QueryString const& queryString,
        std::shared_ptr<arangodb::velocypack::Builder> const& bindParameters,
        std::shared_ptr<arangodb::velocypack::Builder> const& options)
    : arangodb::aql::Query(ctx, queryString, bindParameters, options) {
  }

  arangodb::transaction::Methods& trxForOptimization() override {
    // original version contains an assertion
    return *_trx;
  }
};

}  // namespace

TEST_F(IResearchViewNodeTest, constructSortedView) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
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
  MockQuery query(arangodb::transaction::StandaloneContext::Create(vocbase),
                  arangodb::aql::QueryString("RETURN 1"),
                  nullptr, arangodb::velocypack::Parser::fromJson("{}"));
  query.prepareQuery(arangodb::aql::SerializationFormat::SHADOWROWS);
  arangodb::aql::Variable const outVariable("variable", 0, false);

  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"id\":42, \"depth\":0, \"totalNrRegs\":0, \"varInfoList\":[], "
        "\"nrRegs\":[], \"nrRegsHere\":[], \"regsToClear\":[], "
        "\"varsUsedLater\":[], \"varsValid\":[], \"outVariable\": { "
        "\"name\":\"variable\", \"id\":0 }, \"options\": { \"waitForSync\" : "
        "true, \"collections\":[42] }, \"viewId\": \"" +
        std::to_string(logicalView->id()) +
        "\", "
        "\"primarySort\": [ { \"field\": \"my.nested.Fields\", \"asc\": "
        "false},  { \"field\": \"another.field\", \"asc\":true } ] }");

    arangodb::iresearch::IResearchViewNode node(*query.plan(),  // plan
                                                json->slice());

    EXPECT_TRUE(node.empty());                // view has no links
    EXPECT_TRUE(node.collections().empty());  // view has no links
    EXPECT_TRUE(node.shards().empty());
    EXPECT_TRUE(node.sort().first);  // primary sort is set
    EXPECT_EQ(2, node.sort().second);

    EXPECT_EQ(arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, node.getType());
    EXPECT_EQ(outVariable.id, node.outVariable().id);
    EXPECT_EQ(outVariable.name, node.outVariable().name);
    EXPECT_EQ(query.plan(), node.plan());
    EXPECT_EQ(arangodb::aql::ExecutionNodeId{42}, node.id());
    EXPECT_EQ(logicalView, node.view());
    EXPECT_TRUE(node.scorers().empty());
    EXPECT_FALSE(node.volatility().first);   // filter volatility
    EXPECT_FALSE(node.volatility().second);  // sort volatility
    arangodb::aql::VarSet usedHere;
    node.getVariablesUsedHere(usedHere);
    EXPECT_TRUE(usedHere.empty());
    auto const setHere = node.getVariablesSetHere();
    EXPECT_EQ(1, setHere.size());
    EXPECT_EQ(outVariable.id, setHere[0]->id);
    EXPECT_EQ(outVariable.name, setHere[0]->name);
    EXPECT_TRUE(node.options().forceSync);
    EXPECT_TRUE(node.options().restrictSources);
    EXPECT_EQ(1, node.options().sources.size());
    EXPECT_EQ(42, *node.options().sources.begin());
    EXPECT_EQ(0., node.getCost().estimatedCost);    // no dependencies
    EXPECT_EQ(0, node.getCost().estimatedNrItems);  // no dependencies
  }

  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"id\":42, \"depth\":0, \"totalNrRegs\":0, \"varInfoList\":[], "
        "\"nrRegs\":[], \"nrRegsHere\":[], \"regsToClear\":[], "
        "\"varsUsedLater\":[], \"varsValid\":[], \"outVariable\": { "
        "\"name\":\"variable\", \"id\":0 }, \"options\": { \"waitForSync\" : "
        "true, \"collections\":[42] }, \"viewId\": \"" +
        std::to_string(logicalView->id()) +
        "\", "
        "\"primarySort\": [ { \"field\": \"my.nested.Fields\", \"asc\": "
        "false},  { \"field\": \"another.field\", \"asc\":true } ], "
        "\"primarySortBuckets\": 1 }");

    arangodb::iresearch::IResearchViewNode node(*query.plan(),  // plan
                                                json->slice());

    EXPECT_TRUE(node.empty());                // view has no links
    EXPECT_TRUE(node.collections().empty());  // view has no links
    EXPECT_TRUE(node.shards().empty());
    EXPECT_TRUE(node.sort().first);  // primary sort is set
    EXPECT_EQ(1, node.sort().second);

    EXPECT_EQ(arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, node.getType());
    EXPECT_EQ(outVariable.id, node.outVariable().id);
    EXPECT_EQ(outVariable.name, node.outVariable().name);
    EXPECT_EQ(query.plan(), node.plan());
    EXPECT_EQ(arangodb::aql::ExecutionNodeId{42}, node.id());
    EXPECT_EQ(logicalView, node.view());
    EXPECT_TRUE(node.scorers().empty());
    EXPECT_FALSE(node.volatility().first);   // filter volatility
    EXPECT_FALSE(node.volatility().second);  // sort volatility
    arangodb::aql::VarSet usedHere;
    node.getVariablesUsedHere(usedHere);
    EXPECT_TRUE(usedHere.empty());
    auto const setHere = node.getVariablesSetHere();
    EXPECT_EQ(1, setHere.size());
    EXPECT_EQ(outVariable.id, setHere[0]->id);
    EXPECT_EQ(outVariable.name, setHere[0]->name);
    EXPECT_TRUE(node.options().forceSync);
    EXPECT_TRUE(node.options().restrictSources);
    EXPECT_EQ(1, node.options().sources.size());
    EXPECT_EQ(42, *node.options().sources.begin());
    EXPECT_EQ(0., node.getCost().estimatedCost);    // no dependencies
    EXPECT_EQ(0, node.getCost().estimatedNrItems);  // no dependencies
  }

  // invalid 'primarySortBuckets' specified
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"id\":42, \"depth\":0, \"totalNrRegs\":0, \"varInfoList\":[], "
        "\"nrRegs\":[], \"nrRegsHere\":[], \"regsToClear\":[], "
        "\"varsUsedLater\":[], \"varsValid\":[], \"outVariable\": { "
        "\"name\":\"variable\", \"id\":0 }, \"viewId\": \"" +
        std::to_string(logicalView->id()) +
        "\", "
        "\"primarySort\": [ { \"field\": \"my.nested.Fields\", \"asc\": "
        "false},  { \"field\": \"another.field\", \"asc\":true } ], "
        "\"primarySortBuckets\": false }");

    try {
      arangodb::iresearch::IResearchViewNode node(*query.plan(),  // plan
                                                  json->slice());
      EXPECT_TRUE(false);
    } catch (arangodb::basics::Exception const& ex) {
      EXPECT_EQ(TRI_ERROR_BAD_PARAMETER, ex.code());
    }
  }

  // invalid 'primarySortBuckets' specified
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"id\":42, \"depth\":0, \"totalNrRegs\":0, \"varInfoList\":[], "
        "\"nrRegs\":[], \"nrRegsHere\":[], \"regsToClear\":[], "
        "\"varsUsedLater\":[], \"varsValid\":[], \"outVariable\": { "
        "\"name\":\"variable\", \"id\":0 }, \"viewId\": \"" +
        std::to_string(logicalView->id()) +
        "\", "
        "\"primarySort\": [ { \"field\": \"my.nested.Fields\", \"asc\": "
        "false},  { \"field\": \"another.field\", \"asc\":true } ], "
        "\"primarySortBuckets\": 3 }");

    try {
      arangodb::iresearch::IResearchViewNode node(*query.plan(),  // plan
                                                  json->slice());
      EXPECT_TRUE(false);
    } catch (arangodb::basics::Exception const& ex) {
      EXPECT_EQ(TRI_ERROR_BAD_PARAMETER, ex.code());
    }
  }
}

TEST_F(IResearchViewNodeTest, construct) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
  // create view
  auto createJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
  auto logicalView = vocbase.createView(createJson->slice());
  ASSERT_FALSE(!logicalView);

  // dummy query
  MockQuery query(arangodb::transaction::StandaloneContext::Create(vocbase),
                  arangodb::aql::QueryString("RETURN 1"),
                  nullptr, arangodb::velocypack::Parser::fromJson("{}"));
  query.prepareQuery(arangodb::aql::SerializationFormat::SHADOWROWS);
  arangodb::aql::Variable const outVariable("variable", 0, false);

  // no options
  {
    arangodb::aql::SingletonNode singleton(query.plan(), arangodb::aql::ExecutionNodeId{0});
    arangodb::iresearch::IResearchViewNode node(*query.plan(),  // plan
                                                arangodb::aql::ExecutionNodeId{42},
                                                vocbase,      // database
                                                logicalView,  // view
                                                outVariable,  // out variable
                                                nullptr,  // no filter condition
                                                nullptr,  // no options
                                                {});      // no sort condition
    node.addDependency(&singleton);

    EXPECT_TRUE(node.empty());                // view has no links
    EXPECT_TRUE(node.collections().empty());  // view has no links
    EXPECT_TRUE(node.shards().empty());
    EXPECT_FALSE(node.sort().first);   // primary sort is not set by default
    EXPECT_EQ(0, node.sort().second);  // primary sort is not set by default

    EXPECT_EQ(arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, node.getType());
    EXPECT_EQ(&outVariable, &node.outVariable());
    EXPECT_EQ(query.plan(), node.plan());
    EXPECT_EQ(arangodb::aql::ExecutionNodeId{42}, node.id());
    EXPECT_EQ(logicalView, node.view());
    EXPECT_TRUE(node.scorers().empty());
    EXPECT_FALSE(node.volatility().first);   // filter volatility
    EXPECT_FALSE(node.volatility().second);  // sort volatility
    arangodb::aql::VarSet usedHere;
    node.getVariablesUsedHere(usedHere);
    EXPECT_TRUE(usedHere.empty());
    auto const setHere = node.getVariablesSetHere();
    EXPECT_EQ(1, setHere.size());
    EXPECT_EQ(&outVariable, setHere[0]);
    EXPECT_FALSE(node.options().forceSync);
    EXPECT_EQ(node.options().countApproximate,
              arangodb::iresearch::CountApproximate::Exact);
    EXPECT_EQ(1., node.getCost().estimatedCost);
    EXPECT_EQ(0, node.getCost().estimatedNrItems);
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
                                                arangodb::aql::ExecutionNodeId{42},
                                                vocbase,      // database
                                                logicalView,  // view
                                                outVariable,  // out variable
                                                nullptr,  // no filter condition
                                                &options, {});  // no sort condition

    EXPECT_TRUE(node.empty());                // view has no links
    EXPECT_TRUE(node.collections().empty());  // view has no links
    EXPECT_TRUE(node.shards().empty());

    EXPECT_EQ(arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, node.getType());
    EXPECT_EQ(&outVariable, &node.outVariable());
    EXPECT_EQ(query.plan(), node.plan());
    EXPECT_EQ(arangodb::aql::ExecutionNodeId{42}, node.id());
    EXPECT_EQ(logicalView, node.view());
    EXPECT_TRUE(node.scorers().empty());
    EXPECT_FALSE(node.volatility().first);   // filter volatility
    EXPECT_FALSE(node.volatility().second);  // sort volatility
    arangodb::aql::VarSet usedHere;
    node.getVariablesUsedHere(usedHere);
    EXPECT_TRUE(usedHere.empty());
    auto const setHere = node.getVariablesSetHere();
    EXPECT_EQ(1, setHere.size());
    EXPECT_EQ(&outVariable, setHere[0]);
    EXPECT_TRUE(node.options().forceSync);
    EXPECT_EQ(arangodb::aql::ConditionOptimization::Auto, node.options().conditionOptimization);
    EXPECT_EQ(node.options().countApproximate,
              arangodb::iresearch::CountApproximate::Exact);
    EXPECT_EQ(0., node.getCost().estimatedCost);    // no dependencies
    EXPECT_EQ(0, node.getCost().estimatedNrItems);  // no dependencies
  }

  // with options default optimization
  {
    // build options node
    std::string value{ "auto" };
    arangodb::aql::AstNode attributeValue(arangodb::aql::NODE_TYPE_VALUE);
    attributeValue.setValueType(arangodb::aql::VALUE_TYPE_STRING);
    attributeValue.setStringValue(value.c_str(), value.size());
    arangodb::aql::AstNode attributeName(arangodb::aql::NODE_TYPE_OBJECT_ELEMENT);
    attributeName.addMember(&attributeValue);
    std::string name{ "conditionOptimization" };
    attributeName.setStringValue(name.c_str(), name.size());
    arangodb::aql::AstNode options(arangodb::aql::NODE_TYPE_OBJECT);
    options.addMember(&attributeName);

    arangodb::iresearch::IResearchViewNode node(*query.plan(),  // plan
                                                arangodb::aql::ExecutionNodeId{42},
                                                vocbase,      // database
                                                logicalView,  // view
                                                outVariable,  // out variable
                                                nullptr,  // no filter condition
                                                &options, {});  // no sort condition

    EXPECT_TRUE(node.empty());                // view has no links
    EXPECT_TRUE(node.collections().empty());  // view has no links
    EXPECT_TRUE(node.shards().empty());

    EXPECT_EQ(arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, node.getType());
    EXPECT_EQ(&outVariable, &node.outVariable());
    EXPECT_EQ(query.plan(), node.plan());
    EXPECT_EQ(arangodb::aql::ExecutionNodeId{42}, node.id());
    EXPECT_EQ(logicalView, node.view());
    EXPECT_TRUE(node.scorers().empty());
    EXPECT_FALSE(node.volatility().first);   // filter volatility
    EXPECT_FALSE(node.volatility().second);  // sort volatility
    arangodb::aql::VarSet usedHere;
    node.getVariablesUsedHere(usedHere);
    EXPECT_TRUE(usedHere.empty());
    auto const setHere = node.getVariablesSetHere();
    EXPECT_EQ(1, setHere.size());
    EXPECT_EQ(&outVariable, setHere[0]);
    EXPECT_FALSE(node.options().forceSync);
    EXPECT_EQ(arangodb::aql::ConditionOptimization::Auto, node.options().conditionOptimization);
    EXPECT_EQ(node.options().countApproximate,
              arangodb::iresearch::CountApproximate::Exact);
    EXPECT_EQ(0., node.getCost().estimatedCost);    // no dependencies
    EXPECT_EQ(0, node.getCost().estimatedNrItems);  // no dependencies
  }
  // with options none optimization
  {
    // build options node
    std::string value{ "none" };
    arangodb::aql::AstNode attributeValue(arangodb::aql::NODE_TYPE_VALUE);
    attributeValue.setValueType(arangodb::aql::VALUE_TYPE_STRING);
    attributeValue.setStringValue(value.c_str(), value.size());
    arangodb::aql::AstNode attributeName(arangodb::aql::NODE_TYPE_OBJECT_ELEMENT);
    attributeName.addMember(&attributeValue);
    std::string name{ "conditionOptimization" };
    attributeName.setStringValue(name.c_str(), name.size());
    arangodb::aql::AstNode options(arangodb::aql::NODE_TYPE_OBJECT);
    options.addMember(&attributeName);

    arangodb::iresearch::IResearchViewNode node(*query.plan(),  // plan
                                                arangodb::aql::ExecutionNodeId{42},
                                                vocbase,      // database
                                                logicalView,  // view
                                                outVariable,  // out variable
                                                nullptr,  // no filter condition
                                                &options, {});  // no sort condition

    EXPECT_TRUE(node.empty());                // view has no links
    EXPECT_TRUE(node.collections().empty());  // view has no links
    EXPECT_TRUE(node.shards().empty());

    EXPECT_EQ(arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, node.getType());
    EXPECT_EQ(&outVariable, &node.outVariable());
    EXPECT_EQ(query.plan(), node.plan());
    EXPECT_EQ(arangodb::aql::ExecutionNodeId{42}, node.id());
    EXPECT_EQ(logicalView, node.view());
    EXPECT_TRUE(node.scorers().empty());
    EXPECT_FALSE(node.volatility().first);   // filter volatility
    EXPECT_FALSE(node.volatility().second);  // sort volatility
    arangodb::aql::VarSet usedHere;
    node.getVariablesUsedHere(usedHere);
    EXPECT_TRUE(usedHere.empty());
    auto const setHere = node.getVariablesSetHere();
    EXPECT_EQ(1, setHere.size());
    EXPECT_EQ(&outVariable, setHere[0]);
    EXPECT_FALSE(node.options().forceSync);
    EXPECT_EQ(arangodb::aql::ConditionOptimization::None, node.options().conditionOptimization);
    EXPECT_EQ(node.options().countApproximate,
              arangodb::iresearch::CountApproximate::Exact);
    EXPECT_EQ(0., node.getCost().estimatedCost);    // no dependencies
    EXPECT_EQ(0, node.getCost().estimatedNrItems);  // no dependencies
  }
  // with options noneg optimization
  {
    // build options node
    std::string value{ "noneg" };
    arangodb::aql::AstNode attributeValue(arangodb::aql::NODE_TYPE_VALUE);
    attributeValue.setValueType(arangodb::aql::VALUE_TYPE_STRING);
    attributeValue.setStringValue(value.c_str(), value.size());
    arangodb::aql::AstNode attributeName(arangodb::aql::NODE_TYPE_OBJECT_ELEMENT);
    attributeName.addMember(&attributeValue);
    std::string name{ "conditionOptimization" };
    attributeName.setStringValue(name.c_str(), name.size());
    arangodb::aql::AstNode options(arangodb::aql::NODE_TYPE_OBJECT);
    options.addMember(&attributeName);

    arangodb::iresearch::IResearchViewNode node(*query.plan(),  // plan
                                                arangodb::aql::ExecutionNodeId{42},
                                                vocbase,      // database
                                                logicalView,  // view
                                                outVariable,  // out variable
                                                nullptr,  // no filter condition
                                                &options, {});  // no sort condition

    EXPECT_TRUE(node.empty());                // view has no links
    EXPECT_TRUE(node.collections().empty());  // view has no links
    EXPECT_TRUE(node.shards().empty());

    EXPECT_EQ(arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, node.getType());
    EXPECT_EQ(&outVariable, &node.outVariable());
    EXPECT_EQ(query.plan(), node.plan());
    EXPECT_EQ(arangodb::aql::ExecutionNodeId{42}, node.id());
    EXPECT_EQ(logicalView, node.view());
    EXPECT_TRUE(node.scorers().empty());
    EXPECT_FALSE(node.volatility().first);   // filter volatility
    EXPECT_FALSE(node.volatility().second);  // sort volatility
    arangodb::aql::VarSet usedHere;
    node.getVariablesUsedHere(usedHere);
    EXPECT_TRUE(usedHere.empty());
    auto const setHere = node.getVariablesSetHere();
    EXPECT_EQ(1, setHere.size());
    EXPECT_EQ(&outVariable, setHere[0]);
    EXPECT_FALSE(node.options().forceSync);
    EXPECT_EQ(arangodb::aql::ConditionOptimization::NoNegation, node.options().conditionOptimization);
    EXPECT_EQ(node.options().countApproximate,
              arangodb::iresearch::CountApproximate::Exact);
    EXPECT_EQ(0., node.getCost().estimatedCost);    // no dependencies
    EXPECT_EQ(0, node.getCost().estimatedNrItems);  // no dependencies
  }
  // with options nodnf optimization
  {
    // build options node
    std::string value{ "nodnf" };
    arangodb::aql::AstNode attributeValue(arangodb::aql::NODE_TYPE_VALUE);
    attributeValue.setValueType(arangodb::aql::VALUE_TYPE_STRING);
    attributeValue.setStringValue(value.c_str(), value.size());
    arangodb::aql::AstNode attributeName(arangodb::aql::NODE_TYPE_OBJECT_ELEMENT);
    attributeName.addMember(&attributeValue);
    std::string name{ "conditionOptimization" };
    attributeName.setStringValue(name.c_str(), name.size());
    arangodb::aql::AstNode options(arangodb::aql::NODE_TYPE_OBJECT);
    options.addMember(&attributeName);

    arangodb::iresearch::IResearchViewNode node(*query.plan(),  // plan
                                                arangodb::aql::ExecutionNodeId{42},
                                                vocbase,      // database
                                                logicalView,  // view
                                                outVariable,  // out variable
                                                nullptr,  // no filter condition
                                                &options, {});  // no sort condition

    EXPECT_TRUE(node.empty());                // view has no links
    EXPECT_TRUE(node.collections().empty());  // view has no links
    EXPECT_TRUE(node.shards().empty());

    EXPECT_EQ(arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, node.getType());
    EXPECT_EQ(&outVariable, &node.outVariable());
    EXPECT_EQ(query.plan(), node.plan());
    EXPECT_EQ(arangodb::aql::ExecutionNodeId{42}, node.id());
    EXPECT_EQ(logicalView, node.view());
    EXPECT_TRUE(node.scorers().empty());
    EXPECT_FALSE(node.volatility().first);   // filter volatility
    EXPECT_FALSE(node.volatility().second);  // sort volatility
    arangodb::aql::VarSet usedHere;
    node.getVariablesUsedHere(usedHere);
    EXPECT_TRUE(usedHere.empty());
    auto const setHere = node.getVariablesSetHere();
    EXPECT_EQ(1, setHere.size());
    EXPECT_EQ(&outVariable, setHere[0]);
    EXPECT_FALSE(node.options().forceSync);
    EXPECT_EQ(arangodb::aql::ConditionOptimization::NoDNF, node.options().conditionOptimization);
    EXPECT_EQ(node.options().countApproximate,
              arangodb::iresearch::CountApproximate::Exact);
    EXPECT_EQ(0., node.getCost().estimatedCost);    // no dependencies
    EXPECT_EQ(0, node.getCost().estimatedNrItems);  // no dependencies
  }
  // with options exact countApproximate
  {
    // build options node
    std::string value{ "exact" };
    arangodb::aql::AstNode attributeValue(arangodb::aql::NODE_TYPE_VALUE);
    attributeValue.setValueType(arangodb::aql::VALUE_TYPE_STRING);
    attributeValue.setStringValue(value.c_str(), value.size());
    arangodb::aql::AstNode attributeName(arangodb::aql::NODE_TYPE_OBJECT_ELEMENT);
    attributeName.addMember(&attributeValue);
    std::string name{ "countApproximate" };
    attributeName.setStringValue(name.c_str(), name.size());
    arangodb::aql::AstNode options(arangodb::aql::NODE_TYPE_OBJECT);
    options.addMember(&attributeName);

    arangodb::iresearch::IResearchViewNode node(*query.plan(),  // plan
                                                arangodb::aql::ExecutionNodeId{42},
                                                vocbase,      // database
                                                logicalView,  // view
                                                outVariable,  // out variable
                                                nullptr,  // no filter condition
                                                &options, {});  // no sort condition

    EXPECT_TRUE(node.empty());                // view has no links
    EXPECT_TRUE(node.collections().empty());  // view has no links
    EXPECT_TRUE(node.shards().empty());

    EXPECT_EQ(arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, node.getType());
    EXPECT_EQ(&outVariable, &node.outVariable());
    EXPECT_EQ(query.plan(), node.plan());
    EXPECT_EQ(arangodb::aql::ExecutionNodeId{42}, node.id());
    EXPECT_EQ(logicalView, node.view());
    EXPECT_TRUE(node.scorers().empty());
    EXPECT_FALSE(node.volatility().first);   // filter volatility
    EXPECT_FALSE(node.volatility().second);  // sort volatility
    arangodb::aql::VarSet usedHere;
    node.getVariablesUsedHere(usedHere);
    EXPECT_TRUE(usedHere.empty());
    auto const setHere = node.getVariablesSetHere();
    EXPECT_EQ(1, setHere.size());
    EXPECT_EQ(&outVariable, setHere[0]);
    EXPECT_FALSE(node.options().forceSync);
    EXPECT_EQ(arangodb::aql::ConditionOptimization::Auto, node.options().conditionOptimization);
    EXPECT_EQ(node.options().countApproximate,
              arangodb::iresearch::CountApproximate::Exact);
    EXPECT_EQ(0., node.getCost().estimatedCost);    // no dependencies
    EXPECT_EQ(0, node.getCost().estimatedNrItems);  // no dependencies
  }
  // with optionscost countApproximate
  {
    // build options node
    std::string value{ "cost" };
    arangodb::aql::AstNode attributeValue(arangodb::aql::NODE_TYPE_VALUE);
    attributeValue.setValueType(arangodb::aql::VALUE_TYPE_STRING);
    attributeValue.setStringValue(value.c_str(), value.size());
    arangodb::aql::AstNode attributeName(arangodb::aql::NODE_TYPE_OBJECT_ELEMENT);
    attributeName.addMember(&attributeValue);
    std::string name{ "countApproximate" };
    attributeName.setStringValue(name.c_str(), name.size());
    arangodb::aql::AstNode options(arangodb::aql::NODE_TYPE_OBJECT);
    options.addMember(&attributeName);

    arangodb::iresearch::IResearchViewNode node(*query.plan(),  // plan
                                                arangodb::aql::ExecutionNodeId{42},
                                                vocbase,      // database
                                                logicalView,  // view
                                                outVariable,  // out variable
                                                nullptr,  // no filter condition
                                                &options, {});  // no sort condition

    EXPECT_TRUE(node.empty());                // view has no links
    EXPECT_TRUE(node.collections().empty());  // view has no links
    EXPECT_TRUE(node.shards().empty());

    EXPECT_EQ(arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, node.getType());
    EXPECT_EQ(&outVariable, &node.outVariable());
    EXPECT_EQ(query.plan(), node.plan());
    EXPECT_EQ(arangodb::aql::ExecutionNodeId{42}, node.id());
    EXPECT_EQ(logicalView, node.view());
    EXPECT_TRUE(node.scorers().empty());
    EXPECT_FALSE(node.volatility().first);   // filter volatility
    EXPECT_FALSE(node.volatility().second);  // sort volatility
    arangodb::aql::VarSet usedHere;
    node.getVariablesUsedHere(usedHere);
    EXPECT_TRUE(usedHere.empty());
    auto const setHere = node.getVariablesSetHere();
    EXPECT_EQ(1, setHere.size());
    EXPECT_EQ(&outVariable, setHere[0]);
    EXPECT_FALSE(node.options().forceSync);
    EXPECT_EQ(arangodb::aql::ConditionOptimization::Auto, node.options().conditionOptimization);
    EXPECT_EQ(node.options().countApproximate,
              arangodb::iresearch::CountApproximate::Cost);
    EXPECT_EQ(0., node.getCost().estimatedCost);    // no dependencies
    EXPECT_EQ(0, node.getCost().estimatedNrItems);  // no dependencies
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
                                               arangodb::aql::ExecutionNodeId{42},
                                               vocbase,      // database
                                               logicalView,  // view
                                               outVariable,  // out variable
                                               nullptr,  // no filter condition
                                               &options, {}));  // no sort condition
  }
  // invalid option conditionOptimization
  {
    // build options node
    std::string value{ "none2" };
    arangodb::aql::AstNode attributeValue(arangodb::aql::NODE_TYPE_VALUE);
    attributeValue.setValueType(arangodb::aql::VALUE_TYPE_STRING);
    attributeValue.setStringValue(value.c_str(), value.size());
    arangodb::aql::AstNode attributeName(arangodb::aql::NODE_TYPE_OBJECT_ELEMENT);
    attributeName.addMember(&attributeValue);
    std::string name{ "conditionOptimization" };
    attributeName.setStringValue(name.c_str(), name.size());
    arangodb::aql::AstNode options(arangodb::aql::NODE_TYPE_OBJECT);
    options.addMember(&attributeName);

    EXPECT_ANY_THROW(
        arangodb::iresearch::IResearchViewNode(*query.plan(),  // plan
                                               arangodb::aql::ExecutionNodeId{42},
                                               vocbase,      // database
                                               logicalView,  // view
                                               outVariable,  // out variable
                                               nullptr,  // no filter condition
                                               &options, {}));  // no sort condition
  }

  // invalid option conditionOptimization non-string
  {
    // build options node
    arangodb::aql::AstNode attributeValue(arangodb::aql::NODE_TYPE_VALUE);
    attributeValue.setValueType(arangodb::aql::VALUE_TYPE_BOOL);
    attributeValue.setBoolValue(false);
    arangodb::aql::AstNode attributeName(arangodb::aql::NODE_TYPE_OBJECT_ELEMENT);
    attributeName.addMember(&attributeValue);
    std::string name{ "conditionOptimization" };
    attributeName.setStringValue(name.c_str(), name.size());
    arangodb::aql::AstNode options(arangodb::aql::NODE_TYPE_OBJECT);
    options.addMember(&attributeName);

    EXPECT_ANY_THROW(
        arangodb::iresearch::IResearchViewNode(*query.plan(),  // plan
                                               arangodb::aql::ExecutionNodeId{42},
                                               vocbase,      // database
                                               logicalView,  // view
                                               outVariable,  // out variable
                                               nullptr,  // no filter condition
                                               &options, {}));  // no sort condition
  }
  // invalid option countApproximate non-string
  {
    // build options node
    arangodb::aql::AstNode attributeValue(arangodb::aql::NODE_TYPE_VALUE);
    attributeValue.setValueType(arangodb::aql::VALUE_TYPE_BOOL);
    attributeValue.setBoolValue(false);
    arangodb::aql::AstNode attributeName(arangodb::aql::NODE_TYPE_OBJECT_ELEMENT);
    attributeName.addMember(&attributeValue);
    std::string name{ "countApproximate" };
    attributeName.setStringValue(name.c_str(), name.size());
    arangodb::aql::AstNode options(arangodb::aql::NODE_TYPE_OBJECT);
    options.addMember(&attributeName);

    EXPECT_ANY_THROW(
        arangodb::iresearch::IResearchViewNode(*query.plan(),  // plan
                                               arangodb::aql::ExecutionNodeId{42},
                                               vocbase,      // database
                                               logicalView,  // view
                                               outVariable,  // out variable
                                               nullptr,  // no filter condition
                                               &options, {}));  // no sort condition
  }
  // invalid option countApproximate invalid string
  {
    // build options node
    std::string value{ "unknown_count_approximate" };
    arangodb::aql::AstNode attributeValue(arangodb::aql::NODE_TYPE_VALUE);
    attributeValue.setValueType(arangodb::aql::VALUE_TYPE_STRING);
    attributeValue.setStringValue(value.c_str(), value.size());
    arangodb::aql::AstNode attributeName(arangodb::aql::NODE_TYPE_OBJECT_ELEMENT);
    attributeName.addMember(&attributeValue);
    std::string name{ "countApproximate" };
    attributeName.setStringValue(name.c_str(), name.size());
    arangodb::aql::AstNode options(arangodb::aql::NODE_TYPE_OBJECT);
    options.addMember(&attributeName);

    EXPECT_ANY_THROW(
        arangodb::iresearch::IResearchViewNode(*query.plan(),  // plan
                                               arangodb::aql::ExecutionNodeId{42},
                                               vocbase,      // database
                                               logicalView,  // view
                                               outVariable,  // out variable
                                               nullptr,  // no filter condition
                                               &options, {}));  // no sort condition
  }
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
                                                arangodb::aql::ExecutionNodeId{42},
                                                vocbase,      // database
                                                logicalView,  // view
                                                outVariable,  // out variable
                                                nullptr,  // no filter condition
                                                &options, {});  // no sort condition

    EXPECT_TRUE(node.empty());                // view has no links
    EXPECT_TRUE(node.collections().empty());  // view has no links
    EXPECT_TRUE(node.shards().empty());

    EXPECT_EQ(arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, node.getType());
    EXPECT_EQ(&outVariable, &node.outVariable());
    EXPECT_EQ(query.plan(), node.plan());
    EXPECT_EQ(arangodb::aql::ExecutionNodeId{42}, node.id());
    EXPECT_EQ(logicalView, node.view());
    EXPECT_TRUE(node.scorers().empty());
    EXPECT_FALSE(node.volatility().first);   // filter volatility
    EXPECT_FALSE(node.volatility().second);  // sort volatility
    arangodb::aql::VarSet usedHere;
    node.getVariablesUsedHere(usedHere);
    EXPECT_TRUE(usedHere.empty());
    auto const setHere = node.getVariablesSetHere();
    EXPECT_EQ(1, setHere.size());
    EXPECT_EQ(&outVariable, setHere[0]);
    EXPECT_FALSE(node.options().forceSync);

    EXPECT_EQ(0., node.getCost().estimatedCost);    // no dependencies
    EXPECT_EQ(0, node.getCost().estimatedNrItems);  // no dependencies
  }
}

TEST_F(IResearchViewNodeTest, constructFromVPackSingleServer) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
  // create view
  auto createJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
  auto logicalView = vocbase.createView(createJson->slice());
  ASSERT_FALSE(!logicalView);

  // dummy query
  MockQuery query(arangodb::transaction::StandaloneContext::Create(vocbase),
                  arangodb::aql::QueryString("RETURN 1"),
                  nullptr, arangodb::velocypack::Parser::fromJson("{}"));
  query.prepareQuery(arangodb::aql::SerializationFormat::SHADOWROWS);
  arangodb::aql::Variable const outVariable("variable", 0, false);

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
      EXPECT_EQ(TRI_ERROR_BAD_PARAMETER, ex.code());
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
      EXPECT_EQ(TRI_ERROR_BAD_PARAMETER, ex.code());
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
      EXPECT_EQ(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, ex.code());
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
      EXPECT_EQ(TRI_ERROR_BAD_PARAMETER, ex.code());
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
    EXPECT_FALSE(node.sort().first);   // primary sort is not set by default
    EXPECT_EQ(0, node.sort().second);  // primary sort is not set by default

    EXPECT_EQ(arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, node.getType());
    EXPECT_EQ(outVariable.id, node.outVariable().id);
    EXPECT_EQ(outVariable.name, node.outVariable().name);
    EXPECT_EQ(query.plan(), node.plan());
    EXPECT_EQ(arangodb::aql::ExecutionNodeId{42}, node.id());
    EXPECT_EQ(logicalView, node.view());
    EXPECT_TRUE(node.scorers().empty());
    EXPECT_FALSE(node.volatility().first);   // filter volatility
    EXPECT_FALSE(node.volatility().second);  // sort volatility
    arangodb::aql::VarSet usedHere;
    node.getVariablesUsedHere(usedHere);
    EXPECT_TRUE(usedHere.empty());
    auto const setHere = node.getVariablesSetHere();
    EXPECT_EQ(1, setHere.size());
    EXPECT_EQ(outVariable.id, setHere[0]->id);
    EXPECT_EQ(outVariable.name, setHere[0]->name);
    EXPECT_FALSE(node.options().forceSync);

    EXPECT_EQ(0., node.getCost().estimatedCost);    // no dependencies
    EXPECT_EQ(0, node.getCost().estimatedNrItems);  // no dependencies
    EXPECT_EQ(node.options().countApproximate,
              arangodb::iresearch::CountApproximate::Exact);
  }

  // no options, ignore 'primarySortBuckets'
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"id\":42, \"depth\":0, \"totalNrRegs\":0, \"varInfoList\":[], "
        "\"nrRegs\":[], \"nrRegsHere\":[], \"regsToClear\":[], "
        "\"varsUsedLater\":[], \"varsValid\":[], \"outVariable\": { "
        "\"name\":\"variable\", \"id\":0 }, \"viewId\": \"" +
        std::to_string(logicalView->id()) +
        "\", \"primarySort\": [], \"primarySortBuckets\": false }");

    arangodb::iresearch::IResearchViewNode node(*query.plan(),  // plan
                                                json->slice());

    EXPECT_TRUE(node.empty());                // view has no links
    EXPECT_TRUE(node.collections().empty());  // view has no links
    EXPECT_TRUE(node.shards().empty());
    EXPECT_FALSE(node.sort().first);   // primary sort is not set by default
    EXPECT_EQ(0, node.sort().second);  // primary sort is not set by default

    EXPECT_EQ(arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, node.getType());
    EXPECT_EQ(outVariable.id, node.outVariable().id);
    EXPECT_EQ(outVariable.name, node.outVariable().name);
    EXPECT_EQ(query.plan(), node.plan());
    EXPECT_EQ(arangodb::aql::ExecutionNodeId{42}, node.id());
    EXPECT_EQ(logicalView, node.view());
    EXPECT_TRUE(node.scorers().empty());
    EXPECT_FALSE(node.volatility().first);   // filter volatility
    EXPECT_FALSE(node.volatility().second);  // sort volatility
    arangodb::aql::VarSet usedHere;
    node.getVariablesUsedHere(usedHere);
    EXPECT_TRUE(usedHere.empty());
    auto const setHere = node.getVariablesSetHere();
    EXPECT_EQ(1, setHere.size());
    EXPECT_EQ(outVariable.id, setHere[0]->id);
    EXPECT_EQ(outVariable.name, setHere[0]->name);
    EXPECT_FALSE(node.options().forceSync);

    EXPECT_EQ(0., node.getCost().estimatedCost);    // no dependencies
    EXPECT_EQ(0, node.getCost().estimatedNrItems);  // no dependencies
    EXPECT_EQ(node.options().countApproximate,
              arangodb::iresearch::CountApproximate::Exact);
  }

  // no options
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"id\":42, \"depth\":0, \"totalNrRegs\":0, \"varInfoList\":[], "
        "\"nrRegs\":[], \"nrRegsHere\":[], \"regsToClear\":[], "
        "\"varsUsedLater\":[], \"varsValid\":[], \"outVariable\": { "
        "\"name\":\"variable\", \"id\":0 }, \"viewId\": \"" +
        std::to_string(logicalView->id()) +
        "\", \"primarySort\": [], \"primarySortBuckets\": 42 }");

    arangodb::iresearch::IResearchViewNode node(*query.plan(),  // plan
                                                json->slice());

    EXPECT_TRUE(node.empty());                // view has no links
    EXPECT_TRUE(node.collections().empty());  // view has no links
    EXPECT_TRUE(node.shards().empty());
    EXPECT_FALSE(node.sort().first);   // primary sort is not set by default
    EXPECT_EQ(0, node.sort().second);  // primary sort is not set by default

    EXPECT_EQ(arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, node.getType());
    EXPECT_EQ(outVariable.id, node.outVariable().id);
    EXPECT_EQ(outVariable.name, node.outVariable().name);
    EXPECT_EQ(query.plan(), node.plan());
    EXPECT_EQ(arangodb::aql::ExecutionNodeId{42}, node.id());
    EXPECT_EQ(logicalView, node.view());
    EXPECT_TRUE(node.scorers().empty());
    EXPECT_FALSE(node.volatility().first);   // filter volatility
    EXPECT_FALSE(node.volatility().second);  // sort volatility
    arangodb::aql::VarSet usedHere;
    node.getVariablesUsedHere(usedHere);
    EXPECT_TRUE(usedHere.empty());
    auto const setHere = node.getVariablesSetHere();
    EXPECT_EQ(1, setHere.size());
    EXPECT_EQ(outVariable.id, setHere[0]->id);
    EXPECT_EQ(outVariable.name, setHere[0]->name);
    EXPECT_FALSE(node.options().forceSync);

    EXPECT_EQ(0., node.getCost().estimatedCost);    // no dependencies
    EXPECT_EQ(0, node.getCost().estimatedNrItems);  // no dependencies
    EXPECT_EQ(node.options().countApproximate,
              arangodb::iresearch::CountApproximate::Exact);
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
    EXPECT_FALSE(node.sort().first);   // primary sort is not set by default
    EXPECT_EQ(0, node.sort().second);  // primary sort is not set by default

    EXPECT_EQ(arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, node.getType());
    EXPECT_EQ(outVariable.id, node.outVariable().id);
    EXPECT_EQ(outVariable.name, node.outVariable().name);
    EXPECT_EQ(query.plan(), node.plan());
    EXPECT_EQ(arangodb::aql::ExecutionNodeId{42}, node.id());
    EXPECT_EQ(logicalView, node.view());
    EXPECT_TRUE(node.scorers().empty());
    EXPECT_FALSE(node.volatility().first);   // filter volatility
    EXPECT_FALSE(node.volatility().second);  // sort volatility
    arangodb::aql::VarSet usedHere;
    node.getVariablesUsedHere(usedHere);
    EXPECT_TRUE(usedHere.empty());
    auto const setHere = node.getVariablesSetHere();
    EXPECT_EQ(1, setHere.size());
    EXPECT_EQ(outVariable.id, setHere[0]->id);
    EXPECT_EQ(outVariable.name, setHere[0]->name);
    EXPECT_TRUE(node.options().forceSync);
    EXPECT_TRUE(node.options().restrictSources);
    EXPECT_EQ(1, node.options().sources.size());
    EXPECT_EQ(42, *node.options().sources.begin());
    EXPECT_EQ(0., node.getCost().estimatedCost);    // no dependencies
    EXPECT_EQ(0, node.getCost().estimatedNrItems);  // no dependencies
    EXPECT_EQ(node.options().countApproximate,
              arangodb::iresearch::CountApproximate::Exact);
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
    EXPECT_FALSE(node.sort().first);   // primary sort is not set by default
    EXPECT_EQ(0, node.sort().second);  // primary sort is not set by default

    EXPECT_EQ(arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, node.getType());
    EXPECT_EQ(outVariable.id, node.outVariable().id);
    EXPECT_EQ(outVariable.name, node.outVariable().name);
    EXPECT_EQ(query.plan(), node.plan());
    EXPECT_EQ(arangodb::aql::ExecutionNodeId{42}, node.id());
    EXPECT_EQ(logicalView, node.view());
    EXPECT_TRUE(node.scorers().empty());
    EXPECT_FALSE(node.volatility().first);   // filter volatility
    EXPECT_FALSE(node.volatility().second);  // sort volatility
    arangodb::aql::VarSet usedHere;
    node.getVariablesUsedHere(usedHere);
    EXPECT_TRUE(usedHere.empty());
    auto const setHere = node.getVariablesSetHere();
    EXPECT_EQ(1, setHere.size());
    EXPECT_EQ(outVariable.id, setHere[0]->id);
    EXPECT_EQ(outVariable.name, setHere[0]->name);
    EXPECT_TRUE(node.options().forceSync);
    EXPECT_TRUE(node.options().restrictSources);
    EXPECT_EQ(arangodb::aql::ConditionOptimization::Auto, node.options().conditionOptimization); // default value
    EXPECT_EQ(0, node.options().sources.size());
    EXPECT_EQ(0., node.getCost().estimatedCost);    // no dependencies
    EXPECT_EQ(0, node.getCost().estimatedNrItems);  // no dependencies
    EXPECT_EQ(node.options().countApproximate,
              arangodb::iresearch::CountApproximate::Exact);
  }

  // with options none condition optimization
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"id\":42, \"depth\":0, \"totalNrRegs\":0, \"varInfoList\":[], "
        "\"nrRegs\":[], \"nrRegsHere\":[], \"regsToClear\":[], "
        "\"varsUsedLater\":[], \"varsValid\":[], \"outVariable\": { "
        "\"name\":\"variable\", \"id\":0 }, \"options\": { \"waitForSync\" : "
        "true, \"collection\":null, \"conditionOptimization\": \"none\" }, \"viewId\": \"" +
        std::to_string(logicalView->id()) + "\" }");

    arangodb::iresearch::IResearchViewNode node(*query.plan(),  // plan
                                                json->slice());

    EXPECT_TRUE(node.empty());                // view has no links
    EXPECT_TRUE(node.collections().empty());  // view has no links
    EXPECT_TRUE(node.shards().empty());
    EXPECT_FALSE(node.sort().first);   // primary sort is not set by default
    EXPECT_EQ(0, node.sort().second);  // primary sort is not set by default

    EXPECT_EQ(arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, node.getType());
    EXPECT_EQ(outVariable.id, node.outVariable().id);
    EXPECT_EQ(outVariable.name, node.outVariable().name);
    EXPECT_EQ(query.plan(), node.plan());
    EXPECT_EQ(arangodb::aql::ExecutionNodeId{42}, node.id());
    EXPECT_EQ(logicalView, node.view());
    EXPECT_TRUE(node.scorers().empty());
    EXPECT_FALSE(node.volatility().first);   // filter volatility
    EXPECT_FALSE(node.volatility().second);  // sort volatility
    arangodb::aql::VarSet usedHere;
    node.getVariablesUsedHere(usedHere);
    EXPECT_TRUE(usedHere.empty());
    auto const setHere = node.getVariablesSetHere();
    EXPECT_EQ(1, setHere.size());
    EXPECT_EQ(outVariable.id, setHere[0]->id);
    EXPECT_EQ(outVariable.name, setHere[0]->name);
    EXPECT_TRUE(node.options().forceSync);
    EXPECT_FALSE(node.options().restrictSources);
    EXPECT_EQ(0, node.options().sources.size());
    EXPECT_EQ(node.options().countApproximate,
              arangodb::iresearch::CountApproximate::Exact);
    EXPECT_EQ(arangodb::aql::ConditionOptimization::None, node.options().conditionOptimization);

    EXPECT_EQ(0., node.getCost().estimatedCost);    // no dependencies
    EXPECT_EQ(0, node.getCost().estimatedNrItems);  // no dependencies
  }
  // with options noneg conditionOptimization
  {
    auto json = arangodb::velocypack::Parser::fromJson(
      "{ \"id\":42, \"depth\":0, \"totalNrRegs\":0, \"varInfoList\":[], "
      "\"nrRegs\":[], \"nrRegsHere\":[], \"regsToClear\":[], "
      "\"varsUsedLater\":[], \"varsValid\":[], \"outVariable\": { "
      "\"name\":\"variable\", \"id\":0 }, \"options\": { \"waitForSync\" : "
      "true, \"collection\":null, \"conditionOptimization\": \"noneg\" }, \"viewId\": \"" +
      std::to_string(logicalView->id()) + "\" }");

    arangodb::iresearch::IResearchViewNode node(*query.plan(),  // plan
      json->slice());

    EXPECT_TRUE(node.empty());                // view has no links
    EXPECT_TRUE(node.collections().empty());  // view has no links
    EXPECT_TRUE(node.shards().empty());
    EXPECT_FALSE(node.sort().first);   // primary sort is not set by default
    EXPECT_EQ(0, node.sort().second);  // primary sort is not set by default

    EXPECT_EQ(arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, node.getType());
    EXPECT_EQ(outVariable.id, node.outVariable().id);
    EXPECT_EQ(outVariable.name, node.outVariable().name);
    EXPECT_EQ(query.plan(), node.plan());
    EXPECT_EQ(arangodb::aql::ExecutionNodeId{42}, node.id());
    EXPECT_EQ(logicalView, node.view());
    EXPECT_TRUE(node.scorers().empty());
    EXPECT_FALSE(node.volatility().first);   // filter volatility
    EXPECT_FALSE(node.volatility().second);  // sort volatility
    arangodb::aql::VarSet usedHere;
    node.getVariablesUsedHere(usedHere);
    EXPECT_TRUE(usedHere.empty());
    auto const setHere = node.getVariablesSetHere();
    EXPECT_EQ(1, setHere.size());
    EXPECT_EQ(outVariable.id, setHere[0]->id);
    EXPECT_EQ(outVariable.name, setHere[0]->name);
    EXPECT_TRUE(node.options().forceSync);
    EXPECT_FALSE(node.options().restrictSources);
    EXPECT_EQ(0, node.options().sources.size());
    EXPECT_EQ(node.options().countApproximate,
              arangodb::iresearch::CountApproximate::Exact);
    EXPECT_EQ(arangodb::aql::ConditionOptimization::NoNegation, node.options().conditionOptimization);

    EXPECT_EQ(0., node.getCost().estimatedCost);    // no dependencies
    EXPECT_EQ(0, node.getCost().estimatedNrItems);  // no dependencies
  }
  // with options nodnf conditionOptimization
  {
    auto json = arangodb::velocypack::Parser::fromJson(
      "{ \"id\":42, \"depth\":0, \"totalNrRegs\":0, \"varInfoList\":[], "
      "\"nrRegs\":[], \"nrRegsHere\":[], \"regsToClear\":[], "
      "\"varsUsedLater\":[], \"varsValid\":[], \"outVariable\": { "
      "\"name\":\"variable\", \"id\":0 }, \"options\": { \"waitForSync\" : "
      "true, \"collection\":null, \"conditionOptimization\": \"nodnf\" }, \"viewId\": \"" +
      std::to_string(logicalView->id()) + "\" }");

    arangodb::iresearch::IResearchViewNode node(*query.plan(),  // plan
      json->slice());

    EXPECT_TRUE(node.empty());                // view has no links
    EXPECT_TRUE(node.collections().empty());  // view has no links
    EXPECT_TRUE(node.shards().empty());
    EXPECT_FALSE(node.sort().first);   // primary sort is not set by default
    EXPECT_EQ(0, node.sort().second);  // primary sort is not set by default

    EXPECT_EQ(arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, node.getType());
    EXPECT_EQ(outVariable.id, node.outVariable().id);
    EXPECT_EQ(outVariable.name, node.outVariable().name);
    EXPECT_EQ(query.plan(), node.plan());
    EXPECT_EQ(arangodb::aql::ExecutionNodeId{42}, node.id());
    EXPECT_EQ(logicalView, node.view());
    EXPECT_TRUE(node.scorers().empty());
    EXPECT_FALSE(node.volatility().first);   // filter volatility
    EXPECT_FALSE(node.volatility().second);  // sort volatility
    arangodb::aql::VarSet usedHere;
    node.getVariablesUsedHere(usedHere);
    EXPECT_TRUE(usedHere.empty());
    auto const setHere = node.getVariablesSetHere();
    EXPECT_EQ(1, setHere.size());
    EXPECT_EQ(outVariable.id, setHere[0]->id);
    EXPECT_EQ(outVariable.name, setHere[0]->name);
    EXPECT_TRUE(node.options().forceSync);
    EXPECT_FALSE(node.options().restrictSources);
    EXPECT_EQ(0, node.options().sources.size());
    EXPECT_EQ(node.options().countApproximate,
              arangodb::iresearch::CountApproximate::Exact);
    EXPECT_EQ(arangodb::aql::ConditionOptimization::NoDNF, node.options().conditionOptimization);

    EXPECT_EQ(0., node.getCost().estimatedCost);    // no dependencies
    EXPECT_EQ(0, node.getCost().estimatedNrItems);  // no dependencies
  }

  // with cost countApproximate
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"id\":42, \"depth\":0, \"totalNrRegs\":0, \"varInfoList\":[], "
        "\"nrRegs\":[], \"nrRegsHere\":[], \"regsToClear\":[], "
        "\"varsUsedLater\":[], \"varsValid\":[], \"outVariable\": { "
        "\"name\":\"variable\", \"id\":0 }, \"options\": { \"waitForSync\" : "
        "true, \"collection\":null, \"conditionOptimization\": \"nodnf\","
        "\"countApproximate\":\"cost\" }, "
        "\"viewId\": \"" +
        std::to_string(logicalView->id()) + "\" }");

    arangodb::iresearch::IResearchViewNode node(*query.plan(),  // plan
      json->slice());

    EXPECT_TRUE(node.empty());                // view has no links
    EXPECT_TRUE(node.collections().empty());  // view has no links
    EXPECT_TRUE(node.shards().empty());
    EXPECT_FALSE(node.sort().first);   // primary sort is not set by default
    EXPECT_EQ(0, node.sort().second);  // primary sort is not set by default

    EXPECT_EQ(arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, node.getType());
    EXPECT_EQ(outVariable.id, node.outVariable().id);
    EXPECT_EQ(outVariable.name, node.outVariable().name);
    EXPECT_EQ(query.plan(), node.plan());
    EXPECT_EQ(arangodb::aql::ExecutionNodeId{42}, node.id());
    EXPECT_EQ(logicalView, node.view());
    EXPECT_TRUE(node.scorers().empty());
    EXPECT_FALSE(node.volatility().first);   // filter volatility
    EXPECT_FALSE(node.volatility().second);  // sort volatility
    arangodb::aql::VarSet usedHere;
    node.getVariablesUsedHere(usedHere);
    EXPECT_TRUE(usedHere.empty());
    auto const setHere = node.getVariablesSetHere();
    EXPECT_EQ(1, setHere.size());
    EXPECT_EQ(outVariable.id, setHere[0]->id);
    EXPECT_EQ(outVariable.name, setHere[0]->name);
    EXPECT_TRUE(node.options().forceSync);
    EXPECT_FALSE(node.options().restrictSources);
    EXPECT_EQ(0, node.options().sources.size());
    EXPECT_EQ(arangodb::aql::ConditionOptimization::NoDNF, node.options().conditionOptimization);
    EXPECT_EQ(node.options().countApproximate,
              arangodb::iresearch::CountApproximate::Cost);

    EXPECT_EQ(0., node.getCost().estimatedCost);    // no dependencies
    EXPECT_EQ(0, node.getCost().estimatedNrItems);  // no dependencies
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
      EXPECT_EQ(TRI_ERROR_BAD_PARAMETER, e.code());
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
      EXPECT_EQ(TRI_ERROR_BAD_PARAMETER, e.code());
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
      EXPECT_EQ(TRI_ERROR_BAD_PARAMETER, e.code());
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
  // invalid option 'conditionOptimization'
  {
    auto json = arangodb::velocypack::Parser::fromJson(
      "{ \"id\":42, \"depth\":0, \"totalNrRegs\":0, \"varInfoList\":[], "
      "\"nrRegs\":[], \"nrRegsHere\":[], \"regsToClear\":[], "
      "\"varsUsedLater\":[], \"varsValid\":[], \"outVariable\": { "
      "\"name\":\"variable\", \"id\":0 }, \"options\": { \"conditionOptimization\" : "
      "\"invalid\"}, \"viewId\": \"" +
      std::to_string(logicalView->id()) +
      "\", "
      "\"primarySort\": true }");

    try {
      arangodb::iresearch::IResearchViewNode node(*query.plan(),  // plan
        json->slice());
      EXPECT_TRUE(false);
    }
    catch (arangodb::basics::Exception const& e) {
      EXPECT_TRUE(TRI_ERROR_BAD_PARAMETER == e.code());
    }
    catch (...) {
      EXPECT_TRUE(false);
    }
  }
  // with invalid late materialization (no collection variable)
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"id\":42, \"depth\":0, \"totalNrRegs\":0, \"varInfoList\":[], "
        "\"nrRegs\":[], \"nrRegsHere\":[], \"regsToClear\":[], "
        "\"varsUsedLater\":[], \"varsValid\":[], \"outVariable\": { "
        "\"name\":\"variable\", \"id\":0 }, \"outNmDocId\": { \"name\":\"variable100\", \"id\":100 },"
        "\"options\": { \"waitForSync\" : "
        "true, \"collection\":null }, \"viewId\": \"" +
        std::to_string(logicalView->id()) + "\" }");

    try {
      arangodb::iresearch::IResearchViewNode node(*query.plan(),  // plan
                                                  json->slice());
      EXPECT_TRUE(false);
    } catch (arangodb::basics::Exception const& e) {
      EXPECT_EQ(TRI_ERROR_BAD_PARAMETER, e.code());
    } catch (...) {
      EXPECT_TRUE(false);
    }
  }
  // with invalid late materialization (no local document id variable)
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"id\":42, \"depth\":0, \"totalNrRegs\":0, \"varInfoList\":[], "
        "\"nrRegs\":[], \"nrRegsHere\":[], \"regsToClear\":[], "
        "\"varsUsedLater\":[], \"varsValid\":[], \"outVariable\": { "
        "\"name\":\"variable\", \"id\":0 }, \"outNmColPtr\": { \"name\":\"variable100\", \"id\":100 },"
        "\"options\": { \"waitForSync\" : "
        "true, \"collection\":null }, \"viewId\": \"" +
        std::to_string(logicalView->id()) + "\" }");

    try {
      arangodb::iresearch::IResearchViewNode node(*query.plan(),  // plan
                                                  json->slice());
      EXPECT_TRUE(false);
    } catch (arangodb::basics::Exception const& e) {
      EXPECT_EQ(TRI_ERROR_BAD_PARAMETER, e.code());
    } catch (...) {
      EXPECT_TRUE(false);
    }
  }
  // with invalid late materialization (invalid view values vars)
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"id\":42, \"depth\":0, \"totalNrRegs\":0, \"varInfoList\":[], "
        "\"nrRegs\":[], \"nrRegsHere\":[], \"regsToClear\":[], "
        "\"varsUsedLater\":[], \"varsValid\":[], \"outVariable\": { "
        "\"name\":\"variable\", \"id\":0 }, \"outNmColPtr\": { \"name\":\"variable100\", \"id\":100 }, "
        "\"outNmDocId\": { \"name\":\"variable100\", \"id\":100 }, "
        "\"viewValuesVars\":{\"fieldNumber\":0, \"id\":101}, "
        "\"noMaterialization\":false, "
        "\"options\": { \"waitForSync\" : "
        "true, \"collection\":null }, \"viewId\": \"" +
        std::to_string(logicalView->id()) + "\" }");

    try {
      arangodb::iresearch::IResearchViewNode node(*query.plan(),  // plan
                                                  json->slice());
      EXPECT_TRUE(false);
    } catch (arangodb::basics::Exception const& e) {
      EXPECT_EQ(TRI_ERROR_BAD_PARAMETER, e.code());
    } catch (...) {
      EXPECT_TRUE(false);
    }
  }
  // with invalid late materialization (invalid field number)
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"id\":42, \"depth\":0, \"totalNrRegs\":0, \"varInfoList\":[], "
        "\"nrRegs\":[], \"nrRegsHere\":[], \"regsToClear\":[], "
        "\"varsUsedLater\":[], \"varsValid\":[], \"outVariable\": { "
        "\"name\":\"variable\", \"id\":0 }, \"outNmColPtr\": { \"name\":\"variable100\", \"id\":100 }, "
        "\"outNmDocId\": { \"name\":\"variable100\", \"id\":100 }, "
        "\"viewValuesVars\":[{\"fieldNumber\":\"0\", \"id\":101}], "
        "\"noMaterialization\":false, "
        "\"options\": { \"waitForSync\" : "
        "true, \"collection\":null }, \"viewId\": \"" +
        std::to_string(logicalView->id()) + "\" }");

    try {
      arangodb::iresearch::IResearchViewNode node(*query.plan(),  // plan
                                                  json->slice());
      EXPECT_TRUE(false);
    } catch (arangodb::basics::Exception const& e) {
      EXPECT_EQ(TRI_ERROR_BAD_PARAMETER, e.code());
    } catch (...) {
      EXPECT_TRUE(false);
    }
  }
  // with invalid late materialization (invalid variable id)
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"id\":42, \"depth\":0, \"totalNrRegs\":0, \"varInfoList\":[], "
        "\"nrRegs\":[], \"nrRegsHere\":[], \"regsToClear\":[], "
        "\"varsUsedLater\":[], \"varsValid\":[], \"outVariable\": { "
        "\"name\":\"variable\", \"id\":0 }, \"outNmColPtr\": { \"name\":\"variable100\", \"id\":100 }, "
        "\"outNmDocId\": { \"name\":\"variable100\", \"id\":100 }, "
        "\"viewValuesVars\":[{\"fieldNumber\":0, \"id\":\"101\"}], "
        "\"noMaterialization\":false, "
        "\"options\": { \"waitForSync\" : "
        "true, \"collection\":null }, \"viewId\": \"" +
        std::to_string(logicalView->id()) + "\" }");

    try {
      arangodb::iresearch::IResearchViewNode node(*query.plan(),  // plan
                                                  json->slice());
      EXPECT_TRUE(false);
    } catch (arangodb::basics::Exception const& e) {
      EXPECT_EQ(TRI_ERROR_BAD_PARAMETER, e.code());
    } catch (...) {
      EXPECT_TRUE(false);
    }
  }
  // with invalid no materialization (invalid noMaterialization)
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"id\":42, \"depth\":0, \"totalNrRegs\":0, \"varInfoList\":[], "
        "\"nrRegs\":[], \"nrRegsHere\":[], \"regsToClear\":[], "
        "\"varsUsedLater\":[], \"varsValid\":[], \"outVariable\": { "
        "\"name\":\"variable\", \"id\":0 }, \"outNmColPtr\": { \"name\":\"variable100\", \"id\":100 }, "
        "\"outNmDocId\": { \"name\":\"variable100\", \"id\":100 }, "
        "\"viewValuesVars\":[], "
        "\"noMaterialization\":1, "
        "\"options\": { \"waitForSync\" : "
        "true, \"collection\":null }, \"viewId\": \"" +
        std::to_string(logicalView->id()) + "\" }");

    try {
      arangodb::iresearch::IResearchViewNode node(*query.plan(),  // plan
                                                  json->slice());
      EXPECT_TRUE(false);
    } catch (arangodb::basics::Exception const& e) {
      EXPECT_EQ(TRI_ERROR_BAD_PARAMETER, e.code());
    } catch (...) {
      EXPECT_TRUE(false);
    }
  }
  // with invalid costApproximate (invalid type)
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"id\":42, \"depth\":0, \"totalNrRegs\":0, \"varInfoList\":[], "
        "\"nrRegs\":[], \"nrRegsHere\":[], \"regsToClear\":[], "
        "\"varsUsedLater\":[], \"varsValid\":[], \"outVariable\": { "
        "\"name\":\"variable\", \"id\":0 }, \"outNmColPtr\": { "
        "\"name\":\"variable100\", \"id\":100 }, "
        "\"outNmDocId\": { \"name\":\"variable100\", \"id\":100 }, "
        "\"viewValuesVars\":[], "
        "\"noMaterialization\":false, "
        "\"options\": { \"waitForSync\" : "
        "true, \"collection\":null, \"countApproximate\":1 }, \"viewId\": \"" +
        std::to_string(logicalView->id()) + "\" }");

    try {
      arangodb::iresearch::IResearchViewNode node(*query.plan(),  // plan
                                                  json->slice());
      EXPECT_TRUE(false);
    } catch (arangodb::basics::Exception const& e) {
      EXPECT_EQ(TRI_ERROR_BAD_PARAMETER, e.code());
    } catch (...) {
      EXPECT_TRUE(false);
    }
  }
  // with invalid costApproximate (invalid value)
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"id\":42, \"depth\":0, \"totalNrRegs\":0, \"varInfoList\":[], "
        "\"nrRegs\":[], \"nrRegsHere\":[], \"regsToClear\":[], "
        "\"varsUsedLater\":[], \"varsValid\":[], \"outVariable\": { "
        "\"name\":\"variable\", \"id\":0 }, \"outNmColPtr\": { "
        "\"name\":\"variable100\", \"id\":100 }, "
        "\"outNmDocId\": { \"name\":\"variable100\", \"id\":100 }, "
        "\"viewValuesVars\":[], "
        "\"noMaterialization\":\"unknown_approximate_type\", "
        "\"options\": { \"waitForSync\" : "
        "true, \"collection\":null, \"countApproximate\":1 }, \"viewId\": \"" +
        std::to_string(logicalView->id()) + "\" }");

    try {
      arangodb::iresearch::IResearchViewNode node(*query.plan(),  // plan
                                                  json->slice());
      EXPECT_TRUE(false);
    } catch (arangodb::basics::Exception const& e) {
      EXPECT_EQ(TRI_ERROR_BAD_PARAMETER, e.code());
    } catch (...) {
      EXPECT_TRUE(false);
    }
  }
}

// FIXME TODO
// TEST_F(IResearchViewNodeTest, constructFromVPackCluster) {
//}

TEST_F(IResearchViewNodeTest, clone) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
  // create view
  auto createJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
  auto logicalView = vocbase.createView(createJson->slice());
  ASSERT_FALSE(!logicalView);

  // dummy query
  MockQuery query(arangodb::transaction::StandaloneContext::Create(vocbase),
                  arangodb::aql::QueryString("RETURN 1"),
                  nullptr, arangodb::velocypack::Parser::fromJson("{}"));
  query.prepareQuery(arangodb::aql::SerializationFormat::SHADOWROWS);
  arangodb::aql::Variable const outVariable("variable", 0, false);

  // no filter condition, no sort condition, no shards, no options
  {
    arangodb::iresearch::IResearchViewNode node(*query.plan(),
                                                arangodb::aql::ExecutionNodeId{42},
                                                vocbase,      // database
                                                logicalView,  // view
                                                outVariable,
                                                nullptr,  // no filter condition
                                                nullptr,  // no options
                                                {});        // no sort condition
    EXPECT_TRUE(node.empty());                // view has no links
    EXPECT_TRUE(node.collections().empty());  // view has no links
    EXPECT_TRUE(node.shards().empty());

    // clone without properties into the same plan
    {
      auto const nextId = node.plan()->nextId();
      auto& cloned = dynamic_cast<arangodb::iresearch::IResearchViewNode&>(
          *node.clone(query.plan(), true, false));
      EXPECT_EQ(node.getType(), cloned.getType());
      EXPECT_EQ(&node.outVariable(), &cloned.outVariable());  // same objects
      EXPECT_EQ(node.plan(), cloned.plan());
      EXPECT_EQ(arangodb::aql::ExecutionNodeId{nextId.id() + 1}, cloned.id());
      EXPECT_EQ(&node.vocbase(), &cloned.vocbase());
      EXPECT_EQ(node.view(), cloned.view());
      EXPECT_EQ(&node.filterCondition(), &cloned.filterCondition());
      EXPECT_EQ(node.scorers(), cloned.scorers());
      EXPECT_EQ(node.volatility(), cloned.volatility());
      EXPECT_EQ(node.sort(), cloned.sort());

      EXPECT_EQ(node.getCost(), cloned.getCost());
    }

    // clone with properties into another plan
    {
      // another dummy query
      MockQuery otherQuery(arangodb::transaction::StandaloneContext::Create(vocbase),
                           arangodb::aql::QueryString("RETURN 1"),
                           nullptr, arangodb::velocypack::Parser::fromJson("{}"));
      otherQuery.prepareQuery(arangodb::aql::SerializationFormat::SHADOWROWS);

      auto& cloned = dynamic_cast<arangodb::iresearch::IResearchViewNode&>(
          *node.clone(otherQuery.plan(), true, true));
      EXPECT_EQ(node.getType(), cloned.getType());
      EXPECT_NE(&node.outVariable(), &cloned.outVariable());  // different objects
      EXPECT_EQ(node.outVariable().id, cloned.outVariable().id);
      EXPECT_EQ(node.outVariable().name, cloned.outVariable().name);
      EXPECT_EQ(otherQuery.plan(), cloned.plan());
      EXPECT_EQ(node.id(), cloned.id());
      EXPECT_EQ(&node.vocbase(), &cloned.vocbase());
      EXPECT_EQ(node.view(), cloned.view());
      EXPECT_EQ(&node.filterCondition(), &cloned.filterCondition());
      EXPECT_EQ(node.scorers(), cloned.scorers());
      EXPECT_EQ(node.volatility(), cloned.volatility());
      EXPECT_EQ(node.options().forceSync, cloned.options().forceSync);
      EXPECT_EQ(node.sort(), cloned.sort());

      EXPECT_EQ(node.getCost(), cloned.getCost());
    }

    // clone without properties into another plan
    {
      // another dummy query
      MockQuery otherQuery(arangodb::transaction::StandaloneContext::Create(vocbase),
                           arangodb::aql::QueryString("RETURN 1"),
                           nullptr, arangodb::velocypack::Parser::fromJson("{}"));
      otherQuery.prepareQuery(arangodb::aql::SerializationFormat::SHADOWROWS);

      node.plan()->nextId();
      auto& cloned = dynamic_cast<arangodb::iresearch::IResearchViewNode&>(
          *node.clone(otherQuery.plan(), true, false));
      EXPECT_EQ(node.getType(), cloned.getType());
      EXPECT_EQ(&node.outVariable(), &cloned.outVariable());  // same objects
      EXPECT_EQ(otherQuery.plan(), cloned.plan());
      EXPECT_EQ(node.id(), cloned.id());
      EXPECT_EQ(&node.vocbase(), &cloned.vocbase());
      EXPECT_EQ(node.view(), cloned.view());
      EXPECT_EQ(&node.filterCondition(), &cloned.filterCondition());
      EXPECT_EQ(node.scorers(), cloned.scorers());
      EXPECT_EQ(node.volatility(), cloned.volatility());
      EXPECT_EQ(node.options().forceSync, cloned.options().forceSync);
      EXPECT_EQ(node.sort(), cloned.sort());

      EXPECT_EQ(node.getCost(), cloned.getCost());
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
                                                arangodb::aql::ExecutionNodeId{42},
                                                vocbase,      // database
                                                logicalView,  // view
                                                outVariable,
                                                nullptr,  // no filter condition
                                                &options, {});  // no sort condition
    EXPECT_TRUE(node.empty());                // view has no links
    EXPECT_TRUE(node.collections().empty());  // view has no links
    EXPECT_TRUE(node.shards().empty());
    EXPECT_TRUE(node.options().forceSync);

    // clone without properties into the same plan
    {
      auto const nextId = node.plan()->nextId();
      auto& cloned = dynamic_cast<arangodb::iresearch::IResearchViewNode&>(
          *node.clone(query.plan(), true, false));
      EXPECT_EQ(node.getType(), cloned.getType());
      EXPECT_EQ(&node.outVariable(), &cloned.outVariable());  // same objects
      EXPECT_EQ(node.plan(), cloned.plan());
      EXPECT_EQ(arangodb::aql::ExecutionNodeId{nextId.id() + 1}, cloned.id());
      EXPECT_EQ(&node.vocbase(), &cloned.vocbase());
      EXPECT_EQ(node.view(), cloned.view());
      EXPECT_EQ(&node.filterCondition(), &cloned.filterCondition());
      EXPECT_EQ(node.scorers(), cloned.scorers());
      EXPECT_EQ(node.volatility(), cloned.volatility());
      EXPECT_EQ(node.sort(), cloned.sort());

      EXPECT_EQ(node.getCost(), cloned.getCost());
    }

    // clone with properties into another plan
    {
      // another dummy query
      MockQuery otherQuery(arangodb::transaction::StandaloneContext::Create(vocbase),
                           arangodb::aql::QueryString("RETURN 1"),
                           nullptr, arangodb::velocypack::Parser::fromJson("{}"));
      otherQuery.prepareQuery(arangodb::aql::SerializationFormat::SHADOWROWS);

      auto& cloned = dynamic_cast<arangodb::iresearch::IResearchViewNode&>(
          *node.clone(otherQuery.plan(), true, true));
      EXPECT_EQ(node.getType(), cloned.getType());
      EXPECT_NE(&node.outVariable(), &cloned.outVariable());  // different objects
      EXPECT_EQ(node.outVariable().id, cloned.outVariable().id);
      EXPECT_EQ(node.outVariable().name, cloned.outVariable().name);
      EXPECT_EQ(otherQuery.plan(), cloned.plan());
      EXPECT_EQ(node.id(), cloned.id());
      EXPECT_EQ(&node.vocbase(), &cloned.vocbase());
      EXPECT_EQ(node.view(), cloned.view());
      EXPECT_EQ(&node.filterCondition(), &cloned.filterCondition());
      EXPECT_EQ(node.scorers(), cloned.scorers());
      EXPECT_EQ(node.volatility(), cloned.volatility());
      EXPECT_EQ(node.options().forceSync, cloned.options().forceSync);
      EXPECT_EQ(node.sort(), cloned.sort());

      EXPECT_EQ(node.getCost(), cloned.getCost());
    }

    // clone without properties into another plan
    {
      // another dummy query
      MockQuery otherQuery(arangodb::transaction::StandaloneContext::Create(vocbase),
                           arangodb::aql::QueryString("RETURN 1"),
                           nullptr, arangodb::velocypack::Parser::fromJson("{}"));
      otherQuery.prepareQuery(arangodb::aql::SerializationFormat::SHADOWROWS);

      node.plan()->nextId();
      auto& cloned = dynamic_cast<arangodb::iresearch::IResearchViewNode&>(
          *node.clone(otherQuery.plan(), true, false));
      EXPECT_EQ(node.getType(), cloned.getType());
      EXPECT_EQ(&node.outVariable(), &cloned.outVariable());  // same objects
      EXPECT_EQ(otherQuery.plan(), cloned.plan());
      EXPECT_EQ(node.id(), cloned.id());
      EXPECT_EQ(&node.vocbase(), &cloned.vocbase());
      EXPECT_EQ(node.view(), cloned.view());
      EXPECT_EQ(&node.filterCondition(), &cloned.filterCondition());
      EXPECT_EQ(node.scorers(), cloned.scorers());
      EXPECT_EQ(node.volatility(), cloned.volatility());
      EXPECT_EQ(node.options().forceSync, cloned.options().forceSync);
      EXPECT_EQ(node.sort(), cloned.sort());

      EXPECT_EQ(node.getCost(), cloned.getCost());
    }
  }

  // no filter condition, no sort condition, with shards, no options
  {
    arangodb::iresearch::IResearchViewNode node(*query.plan(),
                                                arangodb::aql::ExecutionNodeId{42},
                                                vocbase,      // database
                                                logicalView,  // view
                                                outVariable,
                                                nullptr,  // no filter condition
                                                nullptr,  // no options
                                                {});        // no sort condition

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
      EXPECT_EQ(node.empty(), cloned.empty());
      EXPECT_EQ(node.shards(), cloned.shards());
      EXPECT_EQ(node.getType(), cloned.getType());
      EXPECT_EQ(&node.outVariable(), &cloned.outVariable());  // same objects
      EXPECT_EQ(node.plan(), cloned.plan());
      EXPECT_EQ(arangodb::aql::ExecutionNodeId{nextId.id() + 1}, cloned.id());
      EXPECT_EQ(&node.vocbase(), &cloned.vocbase());
      EXPECT_EQ(node.view(), cloned.view());
      EXPECT_EQ(&node.filterCondition(), &cloned.filterCondition());
      EXPECT_EQ(node.scorers(), cloned.scorers());
      EXPECT_EQ(node.volatility(), cloned.volatility());
      EXPECT_EQ(node.options().forceSync, cloned.options().forceSync);
      EXPECT_EQ(node.sort(), cloned.sort());

      EXPECT_EQ(node.getCost(), cloned.getCost());
    }

    // clone with properties into another plan
    {
      // another dummy query
      MockQuery otherQuery(arangodb::transaction::StandaloneContext::Create(vocbase),
                           arangodb::aql::QueryString("RETURN 1"),
                           nullptr, arangodb::velocypack::Parser::fromJson("{}"));
      otherQuery.prepareQuery(arangodb::aql::SerializationFormat::SHADOWROWS);

      auto& cloned = dynamic_cast<arangodb::iresearch::IResearchViewNode&>(
          *node.clone(otherQuery.plan(), true, true));
      EXPECT_TRUE(cloned.collections().empty());
      EXPECT_EQ(node.empty(), cloned.empty());
      EXPECT_EQ(node.shards(), cloned.shards());
      EXPECT_EQ(node.getType(), cloned.getType());
      EXPECT_NE(&node.outVariable(), &cloned.outVariable());  // different objects
      EXPECT_EQ(node.outVariable().id, cloned.outVariable().id);
      EXPECT_EQ(node.outVariable().name, cloned.outVariable().name);
      EXPECT_EQ(otherQuery.plan(), cloned.plan());
      EXPECT_EQ(node.id(), cloned.id());
      EXPECT_EQ(&node.vocbase(), &cloned.vocbase());
      EXPECT_EQ(node.view(), cloned.view());
      EXPECT_EQ(&node.filterCondition(), &cloned.filterCondition());
      EXPECT_EQ(node.scorers(), cloned.scorers());
      EXPECT_EQ(node.volatility(), cloned.volatility());
      EXPECT_EQ(node.options().forceSync, cloned.options().forceSync);
      EXPECT_EQ(node.sort(), cloned.sort());

      EXPECT_EQ(node.getCost(), cloned.getCost());
    }

    // clone without properties into another plan
    {
      // another dummy query
      MockQuery otherQuery(arangodb::transaction::StandaloneContext::Create(vocbase),
                           arangodb::aql::QueryString("RETURN 1"),
                           nullptr, arangodb::velocypack::Parser::fromJson("{}"));
      otherQuery.prepareQuery(arangodb::aql::SerializationFormat::SHADOWROWS);

      node.plan()->nextId();
      auto& cloned = dynamic_cast<arangodb::iresearch::IResearchViewNode&>(
          *node.clone(otherQuery.plan(), true, false));
      EXPECT_TRUE(cloned.collections().empty());
      EXPECT_EQ(node.empty(), cloned.empty());
      EXPECT_EQ(node.shards(), cloned.shards());
      EXPECT_EQ(node.getType(), cloned.getType());
      EXPECT_EQ(&node.outVariable(), &cloned.outVariable());  // same objects
      EXPECT_EQ(otherQuery.plan(), cloned.plan());
      EXPECT_EQ(node.id(), cloned.id());
      EXPECT_EQ(&node.vocbase(), &cloned.vocbase());
      EXPECT_EQ(node.view(), cloned.view());
      EXPECT_EQ(&node.filterCondition(), &cloned.filterCondition());
      EXPECT_EQ(node.scorers(), cloned.scorers());
      EXPECT_EQ(node.volatility(), cloned.volatility());
      EXPECT_EQ(node.options().forceSync, cloned.options().forceSync);
      EXPECT_EQ(node.sort(), cloned.sort());

      EXPECT_EQ(node.getCost(), cloned.getCost());
    }
  }

  // no filter condition, sort condition, with shards, no options
  {
    arangodb::iresearch::IResearchViewSort sort;
    arangodb::iresearch::IResearchViewNode node(*query.plan(),
                                                arangodb::aql::ExecutionNodeId{42},
                                                vocbase,      // database
                                                logicalView,  // view
                                                outVariable,
                                                nullptr,  // no filter condition
                                                nullptr,  // no options
                                                {});        // no scorers
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
      EXPECT_EQ(node.empty(), cloned.empty());
      EXPECT_EQ(node.shards(), cloned.shards());
      EXPECT_EQ(node.getType(), cloned.getType());
      EXPECT_EQ(&node.outVariable(), &cloned.outVariable());  // same objects
      EXPECT_EQ(node.plan(), cloned.plan());
      EXPECT_EQ(arangodb::aql::ExecutionNodeId{nextId.id() + 1}, cloned.id());
      EXPECT_EQ(&node.vocbase(), &cloned.vocbase());
      EXPECT_EQ(node.view(), cloned.view());
      EXPECT_EQ(&node.filterCondition(), &cloned.filterCondition());
      EXPECT_EQ(node.scorers(), cloned.scorers());
      EXPECT_EQ(node.volatility(), cloned.volatility());
      EXPECT_EQ(node.options().forceSync, cloned.options().forceSync);
      EXPECT_EQ(node.sort(), cloned.sort());

      EXPECT_EQ(node.getCost(), cloned.getCost());
    }

    // clone with properties into another plan
    {
      // another dummy query
      MockQuery otherQuery(arangodb::transaction::StandaloneContext::Create(vocbase),
                           arangodb::aql::QueryString("RETURN 1"),
                           nullptr, arangodb::velocypack::Parser::fromJson("{}"));
      otherQuery.prepareQuery(arangodb::aql::SerializationFormat::SHADOWROWS);

      auto& cloned = dynamic_cast<arangodb::iresearch::IResearchViewNode&>(
          *node.clone(otherQuery.plan(), true, true));
      EXPECT_TRUE(cloned.collections().empty());
      EXPECT_EQ(node.empty(), cloned.empty());
      EXPECT_EQ(node.shards(), cloned.shards());
      EXPECT_EQ(node.getType(), cloned.getType());
      EXPECT_NE(&node.outVariable(), &cloned.outVariable());  // different objects
      EXPECT_EQ(node.outVariable().id, cloned.outVariable().id);
      EXPECT_EQ(node.outVariable().name, cloned.outVariable().name);
      EXPECT_EQ(otherQuery.plan(), cloned.plan());
      EXPECT_EQ(node.id(), cloned.id());
      EXPECT_EQ(&node.vocbase(), &cloned.vocbase());
      EXPECT_EQ(node.view(), cloned.view());
      EXPECT_EQ(&node.filterCondition(), &cloned.filterCondition());
      EXPECT_EQ(node.scorers(), cloned.scorers());
      EXPECT_EQ(node.volatility(), cloned.volatility());
      EXPECT_EQ(node.options().forceSync, cloned.options().forceSync);
      EXPECT_EQ(node.sort(), cloned.sort());

      EXPECT_EQ(node.getCost(), cloned.getCost());
    }

    // clone without properties into another plan
    {
      // another dummy query
      MockQuery otherQuery(arangodb::transaction::StandaloneContext::Create(vocbase),
                           arangodb::aql::QueryString("RETURN 1"),
                           nullptr, arangodb::velocypack::Parser::fromJson("{}"));
      otherQuery.prepareQuery(arangodb::aql::SerializationFormat::SHADOWROWS);

      node.plan()->nextId();
      auto& cloned = dynamic_cast<arangodb::iresearch::IResearchViewNode&>(
          *node.clone(otherQuery.plan(), true, false));
      EXPECT_TRUE(cloned.collections().empty());
      EXPECT_EQ(node.empty(), cloned.empty());
      EXPECT_EQ(node.shards(), cloned.shards());
      EXPECT_EQ(node.getType(), cloned.getType());
      EXPECT_EQ(&node.outVariable(), &cloned.outVariable());  // same objects
      EXPECT_EQ(otherQuery.plan(), cloned.plan());
      EXPECT_EQ(node.id(), cloned.id());
      EXPECT_EQ(&node.vocbase(), &cloned.vocbase());
      EXPECT_EQ(node.view(), cloned.view());
      EXPECT_EQ(&node.filterCondition(), &cloned.filterCondition());
      EXPECT_EQ(node.scorers(), cloned.scorers());
      EXPECT_EQ(node.volatility(), cloned.volatility());
      EXPECT_EQ(node.options().forceSync, cloned.options().forceSync);
      EXPECT_EQ(node.sort(), cloned.sort());

      EXPECT_EQ(node.getCost(), cloned.getCost());
    }
  }

  // with late materialization
  {
    arangodb::iresearch::IResearchViewNode node(*query.plan(),
                                                arangodb::aql::ExecutionNodeId{42},
                                                vocbase,      // database
                                                logicalView,  // view
                                                outVariable,
                                                nullptr,  // no filter condition
                                                nullptr,  // no options
                                                {});        // no sort condition
    arangodb::aql::Variable const outNmColPtr("variable100", 100, false);
    arangodb::aql::Variable const outNmDocId("variable101", 101, false);
    node.setLateMaterialized(outNmColPtr, outNmDocId);
    ASSERT_TRUE(node.isLateMaterialized());
    auto varsSetOriginal = node.getVariablesSetHere();
    ASSERT_EQ(2, varsSetOriginal.size());
    // clone without properties into the same plan
    {
      auto const nextId = node.plan()->nextId();
      auto& cloned = dynamic_cast<arangodb::iresearch::IResearchViewNode&>(
          *node.clone(query.plan(), true, false));
      auto varsSetCloned = cloned.getVariablesSetHere();
      EXPECT_TRUE(cloned.collections().empty());
      EXPECT_EQ(node.empty(), cloned.empty());
      EXPECT_EQ(node.shards(), cloned.shards());
      EXPECT_EQ(node.getType(), cloned.getType());
      EXPECT_EQ(&node.outVariable(), &cloned.outVariable());  // same objects
      EXPECT_EQ(node.plan(), cloned.plan());
      EXPECT_EQ(arangodb::aql::ExecutionNodeId{nextId.id() + 1}, cloned.id());
      EXPECT_EQ(&node.vocbase(), &cloned.vocbase());
      EXPECT_EQ(node.view(), cloned.view());
      EXPECT_EQ(&node.filterCondition(), &cloned.filterCondition());
      EXPECT_EQ(node.scorers(), cloned.scorers());
      EXPECT_EQ(node.volatility(), cloned.volatility());
      EXPECT_EQ(node.options().forceSync, cloned.options().forceSync);
      EXPECT_EQ(node.sort(), cloned.sort());
      EXPECT_EQ(node.getCost(), cloned.getCost());
      EXPECT_EQ(node.isLateMaterialized(), cloned.isLateMaterialized());
      ASSERT_EQ(varsSetOriginal.size(), varsSetCloned.size());
      EXPECT_EQ(varsSetOriginal[0], varsSetCloned[0]);
      EXPECT_EQ(varsSetOriginal[1], varsSetCloned[1]);
    }

    // clone with properties into another plan
    {
      // another dummy query
      MockQuery otherQuery(arangodb::transaction::StandaloneContext::Create(vocbase),
                           arangodb::aql::QueryString("RETURN 1"),
                           nullptr, arangodb::velocypack::Parser::fromJson("{}"));
      otherQuery.prepareQuery(arangodb::aql::SerializationFormat::SHADOWROWS);

      auto& cloned = dynamic_cast<arangodb::iresearch::IResearchViewNode&>(
          *node.clone(otherQuery.plan(), true, true));
      auto varsSetCloned = cloned.getVariablesSetHere();
      EXPECT_TRUE(cloned.collections().empty());
      EXPECT_EQ(node.empty(), cloned.empty());
      EXPECT_EQ(node.shards(), cloned.shards());
      EXPECT_EQ(node.getType(), cloned.getType());
      EXPECT_NE(&node.outVariable(), &cloned.outVariable());  // different objects
      EXPECT_EQ(node.outVariable().id, cloned.outVariable().id);
      EXPECT_EQ(node.outVariable().name, cloned.outVariable().name);
      EXPECT_EQ(otherQuery.plan(), cloned.plan());
      EXPECT_EQ(node.id(), cloned.id());
      EXPECT_EQ(&node.vocbase(), &cloned.vocbase());
      EXPECT_EQ(node.view(), cloned.view());
      EXPECT_EQ(&node.filterCondition(), &cloned.filterCondition());
      EXPECT_EQ(node.scorers(), cloned.scorers());
      EXPECT_EQ(node.volatility(), cloned.volatility());
      EXPECT_EQ(node.options().forceSync, cloned.options().forceSync);
      EXPECT_EQ(node.sort(), cloned.sort());
      EXPECT_EQ(node.getCost(), cloned.getCost());
      EXPECT_EQ(node.isLateMaterialized(), cloned.isLateMaterialized());
      ASSERT_EQ(varsSetOriginal.size(), varsSetCloned.size());
      EXPECT_NE(varsSetOriginal[0], varsSetCloned[0]);
      EXPECT_NE(varsSetOriginal[1], varsSetCloned[1]);
    }
    // clone without properties into another plan
    {
      // another dummy query
      MockQuery otherQuery(arangodb::transaction::StandaloneContext::Create(vocbase),
                           arangodb::aql::QueryString("RETURN 1"),
                           nullptr, arangodb::velocypack::Parser::fromJson("{}"));
      otherQuery.prepareQuery(arangodb::aql::SerializationFormat::SHADOWROWS);

      node.plan()->nextId();
      auto& cloned = dynamic_cast<arangodb::iresearch::IResearchViewNode&>(
        *node.clone(otherQuery.plan(), true, false));
      auto varsSetCloned = cloned.getVariablesSetHere();
      EXPECT_TRUE(cloned.collections().empty());
      EXPECT_EQ(node.empty(), cloned.empty());
      EXPECT_EQ(node.shards(), cloned.shards());
      EXPECT_EQ(node.getType(), cloned.getType());
      EXPECT_EQ(&node.outVariable(), &cloned.outVariable());  // same objects
      EXPECT_EQ(otherQuery.plan(), cloned.plan());
      EXPECT_EQ(node.id(), cloned.id());
      EXPECT_EQ(&node.vocbase(), &cloned.vocbase());
      EXPECT_EQ(node.view(), cloned.view());
      EXPECT_EQ(&node.filterCondition(), &cloned.filterCondition());
      EXPECT_EQ(node.scorers(), cloned.scorers());
      EXPECT_EQ(node.volatility(), cloned.volatility());
      EXPECT_EQ(node.options().forceSync, cloned.options().forceSync);
      EXPECT_EQ(node.sort(), cloned.sort());
      EXPECT_TRUE(node.getCost() == cloned.getCost());
      EXPECT_EQ(node.isLateMaterialized(), cloned.isLateMaterialized());
      ASSERT_EQ(varsSetOriginal.size(), varsSetCloned.size());
      EXPECT_EQ(varsSetOriginal[0], varsSetCloned[0]);
      EXPECT_EQ(varsSetOriginal[1], varsSetCloned[1]);
    }
  }
}

TEST_F(IResearchViewNodeTest, serialize) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
  // create view
  auto createJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
  auto logicalView = vocbase.createView(createJson->slice());
  ASSERT_FALSE(!logicalView);

  // dummy query
  MockQuery query(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      arangodb::aql::QueryString(
          "let variable = 1 let variable100 = 3 let variable101 = 2 RETURN 1"),
      nullptr, arangodb::velocypack::Parser::fromJson("{}"));
  query.prepareQuery(arangodb::aql::SerializationFormat::SHADOWROWS);

  arangodb::aql::Variable const outVariable("variable", 0, false);

  // no filter condition, no sort condition
  {
    arangodb::iresearch::IResearchViewNode node(*query.plan(),
                                                arangodb::aql::ExecutionNodeId{42},
                                                vocbase,      // database
                                                logicalView,  // view
                                                outVariable,
                                                nullptr,  // no filter condition
                                                nullptr,  // no options
                                                {});        // no sort condition
    EXPECT_TRUE(node.empty());                // view has no links
    EXPECT_TRUE(node.collections().empty());  // view has no links
    EXPECT_TRUE(node.shards().empty());
    node.setVarsUsedLater({arangodb::aql::VarSet{&outVariable}});
    node.setVarsValid({{}});
    node.setRegsToKeep({{}});
    node.setVarUsageValid();

    arangodb::velocypack::Builder builder;
    unsigned flags = arangodb::aql::ExecutionNode::SERIALIZE_DETAILS;
    node.toVelocyPack(builder, flags, false);  // object with array of objects

    auto const slice = builder.slice();
    EXPECT_TRUE(slice.isObject());
    auto const nodesSlice = slice.get("nodes");
    EXPECT_TRUE(nodesSlice.isArray());
    arangodb::velocypack::ArrayIterator it(nodesSlice);
    EXPECT_EQ(1, it.size());
    auto nodeSlice = it.value();

    // constructor
    {
      arangodb::iresearch::IResearchViewNode const deserialized(*query.plan(), nodeSlice);
      EXPECT_EQ(node.empty(), deserialized.empty());
      EXPECT_EQ(node.shards(), deserialized.shards());
      EXPECT_TRUE(deserialized.collections().empty());
      EXPECT_EQ(node.getType(), deserialized.getType());
      EXPECT_EQ(node.outVariable().id, deserialized.outVariable().id);
      EXPECT_EQ(node.outVariable().name, deserialized.outVariable().name);
      EXPECT_EQ(node.plan(), deserialized.plan());
      EXPECT_EQ(node.id(), deserialized.id());
      EXPECT_EQ(&node.vocbase(), &deserialized.vocbase());
      EXPECT_EQ(node.view(), deserialized.view());
      EXPECT_EQ(&node.filterCondition(), &deserialized.filterCondition());
      EXPECT_EQ(node.scorers(), deserialized.scorers());
      EXPECT_EQ(node.volatility(), deserialized.volatility());
      EXPECT_EQ(node.options().forceSync, deserialized.options().forceSync);
      EXPECT_EQ(node.sort(), deserialized.sort());
      EXPECT_EQ(node.getCost(), deserialized.getCost());
      EXPECT_EQ(node.options().countApproximate, arangodb::iresearch::CountApproximate::Exact);
    }

    // factory method
    {
      std::unique_ptr<arangodb::aql::ExecutionNode> deserializedNode(
          arangodb::aql::ExecutionNode::fromVPackFactory(query.plan(), nodeSlice));
      auto& deserialized =
          dynamic_cast<arangodb::iresearch::IResearchViewNode&>(*deserializedNode);
      EXPECT_EQ(node.empty(), deserialized.empty());
      EXPECT_EQ(node.shards(), deserialized.shards());
      EXPECT_TRUE(deserialized.collections().empty());
      EXPECT_EQ(node.getType(), deserialized.getType());
      EXPECT_EQ(node.outVariable().id, deserialized.outVariable().id);
      EXPECT_EQ(node.outVariable().name, deserialized.outVariable().name);
      EXPECT_EQ(node.plan(), deserialized.plan());
      EXPECT_EQ(node.id(), deserialized.id());
      EXPECT_EQ(&node.vocbase(), &deserialized.vocbase());
      EXPECT_EQ(node.view(), deserialized.view());
      EXPECT_EQ(&node.filterCondition(), &deserialized.filterCondition());
      EXPECT_EQ(node.scorers(), deserialized.scorers());
      EXPECT_EQ(node.volatility(), deserialized.volatility());
      EXPECT_EQ(node.options().forceSync, deserialized.options().forceSync);
      EXPECT_EQ(node.sort(), deserialized.sort());
      EXPECT_EQ(node.getCost(), deserialized.getCost());
      EXPECT_EQ(node.options().countApproximate, arangodb::iresearch::CountApproximate::Exact);
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
                                                arangodb::aql::ExecutionNodeId{42},
                                                vocbase,      // database
                                                logicalView,  // view
                                                outVariable,
                                                nullptr,  // no filter condition
                                                &options, {});  // no sort condition
    EXPECT_TRUE(node.empty());                // view has no links
    EXPECT_TRUE(node.collections().empty());  // view has no links
    EXPECT_TRUE(node.shards().empty());
    EXPECT_TRUE(node.options().forceSync);
    EXPECT_EQ(node.options().countApproximate, arangodb::iresearch::CountApproximate::Exact);

    node.setVarsUsedLater({arangodb::aql::VarSet{&outVariable}});
    node.setVarsValid({{}});
    node.setRegsToKeep({{}});
    node.setVarUsageValid();

    arangodb::velocypack::Builder builder;
    unsigned flags = arangodb::aql::ExecutionNode::SERIALIZE_DETAILS;
    node.toVelocyPack(builder, flags, false);  // object with array of objects

    auto const slice = builder.slice();
    EXPECT_TRUE(slice.isObject());
    auto const nodesSlice = slice.get("nodes");
    EXPECT_TRUE(nodesSlice.isArray());
    arangodb::velocypack::ArrayIterator it(nodesSlice);
    ASSERT_EQ(1, it.size());
    auto nodeSlice = it.value();

    // constructor
    {
      arangodb::iresearch::IResearchViewNode const deserialized(*query.plan(), nodeSlice);
      EXPECT_EQ(node.empty(), deserialized.empty());
      EXPECT_EQ(node.shards(), deserialized.shards());
      EXPECT_TRUE(deserialized.collections().empty());
      EXPECT_EQ(node.getType(), deserialized.getType());
      EXPECT_EQ(node.outVariable().id, deserialized.outVariable().id);
      EXPECT_EQ(node.outVariable().name, deserialized.outVariable().name);
      EXPECT_EQ(node.plan(), deserialized.plan());
      EXPECT_EQ(node.id(), deserialized.id());
      EXPECT_EQ(&node.vocbase(), &deserialized.vocbase());
      EXPECT_EQ(node.view(), deserialized.view());
      EXPECT_EQ(&node.filterCondition(), &deserialized.filterCondition());
      EXPECT_EQ(node.scorers(), deserialized.scorers());
      EXPECT_EQ(node.volatility(), deserialized.volatility());
      EXPECT_EQ(node.options().forceSync, deserialized.options().forceSync);
      EXPECT_EQ(node.sort(), deserialized.sort());
      EXPECT_EQ(node.getCost(), deserialized.getCost());
      EXPECT_EQ(node.options().countApproximate, arangodb::iresearch::CountApproximate::Exact);
    }

    // factory method
    {
      std::unique_ptr<arangodb::aql::ExecutionNode> deserializedNode(
          arangodb::aql::ExecutionNode::fromVPackFactory(query.plan(), nodeSlice));
      auto& deserialized =
          dynamic_cast<arangodb::iresearch::IResearchViewNode&>(*deserializedNode);
      EXPECT_EQ(node.empty(), deserialized.empty());
      EXPECT_EQ(node.shards(), deserialized.shards());
      EXPECT_TRUE(deserialized.collections().empty());
      EXPECT_EQ(node.getType(), deserialized.getType());
      EXPECT_EQ(node.outVariable().id, deserialized.outVariable().id);
      EXPECT_EQ(node.outVariable().name, deserialized.outVariable().name);
      EXPECT_EQ(node.plan(), deserialized.plan());
      EXPECT_EQ(node.id(), deserialized.id());
      EXPECT_EQ(&node.vocbase(), &deserialized.vocbase());
      EXPECT_EQ(node.view(), deserialized.view());
      EXPECT_EQ(&node.filterCondition(), &deserialized.filterCondition());
      EXPECT_EQ(node.scorers(), deserialized.scorers());
      EXPECT_EQ(node.volatility(), deserialized.volatility());
      EXPECT_EQ(node.options().forceSync, deserialized.options().forceSync);
      EXPECT_EQ(node.sort(), deserialized.sort());
      EXPECT_EQ(node.getCost(), deserialized.getCost());
      EXPECT_EQ(node.options().countApproximate, arangodb::iresearch::CountApproximate::Exact);
    }
  }
  // with late materialization
  {
    arangodb::iresearch::IResearchViewNode node(*query.plan(),
                                                arangodb::aql::ExecutionNodeId{42},
                                                vocbase,      // database
                                                logicalView,  // view
                                                outVariable,
                                                nullptr,  // no filter condition
                                                nullptr,  // no options
                                                {});        // no sort condition
    arangodb::aql::Variable const outNmColPtr("variable100", 1, false);
    arangodb::aql::Variable const outNmDocId("variable101", 2, false);
    node.setLateMaterialized(outNmColPtr, outNmDocId);

    node.setVarsUsedLater({arangodb::aql::VarSet{&outNmColPtr, &outNmDocId}});
    node.setVarsValid({{}});
    node.setRegsToKeep({{}});
    node.setVarUsageValid();

    arangodb::velocypack::Builder builder;
    unsigned flags = arangodb::aql::ExecutionNode::SERIALIZE_DETAILS;
    node.toVelocyPack(builder, flags, false);  // object with array of objects

    auto const slice = builder.slice();
    EXPECT_TRUE(slice.isObject());
    auto const nodesSlice = slice.get("nodes");
    EXPECT_TRUE(nodesSlice.isArray());
    arangodb::velocypack::ArrayIterator it(nodesSlice);
    ASSERT_EQ(1, it.size());
    auto nodeSlice = it.value();

    // constructor
    {
      arangodb::iresearch::IResearchViewNode const deserialized(*query.plan(), nodeSlice);
      EXPECT_EQ(node.empty(), deserialized.empty());
      EXPECT_EQ(node.shards(), deserialized.shards());
      EXPECT_TRUE(deserialized.collections().empty());
      EXPECT_EQ(node.getType(), deserialized.getType());
      EXPECT_EQ(node.outVariable().id, deserialized.outVariable().id);
      EXPECT_EQ(node.outVariable().name, deserialized.outVariable().name);
      EXPECT_EQ(node.plan(), deserialized.plan());
      EXPECT_EQ(node.id(), deserialized.id());
      EXPECT_EQ(&node.vocbase(), &deserialized.vocbase());
      EXPECT_EQ(node.view(), deserialized.view());
      EXPECT_EQ(&node.filterCondition(), &deserialized.filterCondition());
      EXPECT_EQ(node.scorers(), deserialized.scorers());
      EXPECT_EQ(node.volatility(), deserialized.volatility());
      EXPECT_EQ(node.options().forceSync, deserialized.options().forceSync);
      EXPECT_EQ(node.sort(), deserialized.sort());
      EXPECT_EQ(node.getCost(), deserialized.getCost());
      EXPECT_EQ(node.isLateMaterialized(), deserialized.isLateMaterialized());
      auto varsSetHere = deserialized.getVariablesSetHere();
      ASSERT_EQ(2, varsSetHere.size());
      EXPECT_EQ(outNmColPtr.id, varsSetHere[0]->id);
      EXPECT_EQ(outNmColPtr.name, varsSetHere[0]->name);
      EXPECT_EQ(outNmDocId.id, varsSetHere[1]->id);
      EXPECT_EQ(outNmDocId.name, varsSetHere[1]->name);
      EXPECT_EQ(node.options().countApproximate, arangodb::iresearch::CountApproximate::Exact);
    }

    // factory method
    {
      std::unique_ptr<arangodb::aql::ExecutionNode> deserializedNode(
          arangodb::aql::ExecutionNode::fromVPackFactory(query.plan(), nodeSlice));
      auto& deserialized =
          dynamic_cast<arangodb::iresearch::IResearchViewNode&>(*deserializedNode);
      EXPECT_EQ(node.empty(), deserialized.empty());
      EXPECT_EQ(node.shards(), deserialized.shards());
      EXPECT_TRUE(deserialized.collections().empty());
      EXPECT_EQ(node.getType(), deserialized.getType());
      EXPECT_EQ(node.outVariable().id, deserialized.outVariable().id);
      EXPECT_EQ(node.outVariable().name, deserialized.outVariable().name);
      EXPECT_EQ(node.plan(), deserialized.plan());
      EXPECT_EQ(node.id(), deserialized.id());
      EXPECT_EQ(&node.vocbase(), &deserialized.vocbase());
      EXPECT_EQ(node.view(), deserialized.view());
      EXPECT_EQ(&node.filterCondition(), &deserialized.filterCondition());
      EXPECT_EQ(node.scorers(), deserialized.scorers());
      EXPECT_EQ(node.volatility(), deserialized.volatility());
      EXPECT_EQ(node.options().forceSync, deserialized.options().forceSync);
      EXPECT_EQ(node.sort(), deserialized.sort());
      EXPECT_EQ(node.getCost(), deserialized.getCost());
      EXPECT_EQ(node.isLateMaterialized(), deserialized.isLateMaterialized());
      auto varsSetHere = deserialized.getVariablesSetHere();
      ASSERT_EQ(2, varsSetHere.size());
      EXPECT_EQ(outNmColPtr.id, varsSetHere[0]->id);
      EXPECT_EQ(outNmColPtr.name, varsSetHere[0]->name);
      EXPECT_EQ(outNmDocId.id, varsSetHere[1]->id);
      EXPECT_EQ(outNmDocId.name, varsSetHere[1]->name);
      EXPECT_EQ(node.options().countApproximate, arangodb::iresearch::CountApproximate::Exact);
    }
  }
  // with countApproximate cost
  {
    std::string value{ "cost" };
    arangodb::aql::AstNode attributeValue(arangodb::aql::NODE_TYPE_VALUE);
    attributeValue.setValueType(arangodb::aql::VALUE_TYPE_STRING);
    attributeValue.setStringValue(value.c_str(), value.size());
    arangodb::aql::AstNode attributeName(arangodb::aql::NODE_TYPE_OBJECT_ELEMENT);
    attributeName.addMember(&attributeValue);
    std::string name{ "countApproximate" };
    attributeName.setStringValue(name.c_str(), name.size());
    arangodb::aql::AstNode options(arangodb::aql::NODE_TYPE_OBJECT);
    options.addMember(&attributeName);

    arangodb::iresearch::IResearchViewNode node(*query.plan(),
                                                arangodb::aql::ExecutionNodeId{42},
                                                vocbase,      // database
                                                logicalView,  // view
                                                outVariable,
                                                nullptr,  // no filter condition
                                                &options,  // no options
                                                {});        // no sort condition
   

    node.setVarsUsedLater({arangodb::aql::VarSet{&outVariable}});
    node.setVarsValid({{}});
    node.setRegsToKeep({{}});
    node.setVarUsageValid();

    arangodb::velocypack::Builder builder;
    unsigned flags = arangodb::aql::ExecutionNode::SERIALIZE_DETAILS;
    node.toVelocyPack(builder, flags, false);  // object with array of objects

    auto const slice = builder.slice();
    EXPECT_TRUE(slice.isObject());
    auto const nodesSlice = slice.get("nodes");
    EXPECT_TRUE(nodesSlice.isArray());
    arangodb::velocypack::ArrayIterator it(nodesSlice);
    ASSERT_EQ(1, it.size());
    auto nodeSlice = it.value();

    // constructor
    {
      arangodb::iresearch::IResearchViewNode const deserialized(*query.plan(), nodeSlice);
      EXPECT_EQ(node.empty(), deserialized.empty());
      EXPECT_EQ(deserialized.options().countApproximate, arangodb::iresearch::CountApproximate::Cost);
    }

    // factory method
    {
      std::unique_ptr<arangodb::aql::ExecutionNode> deserializedNode(
          arangodb::aql::ExecutionNode::fromVPackFactory(query.plan(), nodeSlice));
      auto& deserialized =
          dynamic_cast<arangodb::iresearch::IResearchViewNode&>(*deserializedNode);
      EXPECT_EQ(deserialized.options().countApproximate, arangodb::iresearch::CountApproximate::Cost);
    }
  }
    // with countApproximate exact
  {
    std::string value{ "exact" };
    arangodb::aql::AstNode attributeValue(arangodb::aql::NODE_TYPE_VALUE);
    attributeValue.setValueType(arangodb::aql::VALUE_TYPE_STRING);
    attributeValue.setStringValue(value.c_str(), value.size());
    arangodb::aql::AstNode attributeName(arangodb::aql::NODE_TYPE_OBJECT_ELEMENT);
    attributeName.addMember(&attributeValue);
    std::string name{ "countApproximate" };
    attributeName.setStringValue(name.c_str(), name.size());
    arangodb::aql::AstNode options(arangodb::aql::NODE_TYPE_OBJECT);
    options.addMember(&attributeName);

    arangodb::iresearch::IResearchViewNode node(*query.plan(),
                                                arangodb::aql::ExecutionNodeId{42},
                                                vocbase,      // database
                                                logicalView,  // view
                                                outVariable,
                                                nullptr,  // no filter condition
                                                &options,  // no options
                                                {});        // no sort condition
   

    node.setVarsUsedLater({arangodb::aql::VarSet{&outVariable}});
    node.setVarsValid({{}});
    node.setRegsToKeep({{}});
    node.setVarUsageValid();

    arangodb::velocypack::Builder builder;
    unsigned flags = arangodb::aql::ExecutionNode::SERIALIZE_DETAILS;
    node.toVelocyPack(builder, flags, false);  // object with array of objects

    auto const slice = builder.slice();
    EXPECT_TRUE(slice.isObject());
    auto const nodesSlice = slice.get("nodes");
    EXPECT_TRUE(nodesSlice.isArray());
    arangodb::velocypack::ArrayIterator it(nodesSlice);
    ASSERT_EQ(1, it.size());
    auto nodeSlice = it.value();

    // constructor
    {
      arangodb::iresearch::IResearchViewNode const deserialized(*query.plan(), nodeSlice);
      EXPECT_EQ(node.empty(), deserialized.empty());
      EXPECT_EQ(node.options().countApproximate, arangodb::iresearch::CountApproximate::Exact);
    }

    // factory method
    {
      std::unique_ptr<arangodb::aql::ExecutionNode> deserializedNode(
          arangodb::aql::ExecutionNode::fromVPackFactory(query.plan(), nodeSlice));
      auto& deserialized =
          dynamic_cast<arangodb::iresearch::IResearchViewNode&>(*deserializedNode);
      EXPECT_EQ(deserialized.options().countApproximate, arangodb::iresearch::CountApproximate::Exact);
    }
  }
}

TEST_F(IResearchViewNodeTest, serializeSortedView) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
  // create view
  auto createJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", \"type\": \"arangosearch\", \"primarySort\" : "
      "[ { \"field\":\"_key\", \"direction\":\"desc\"} ] }");
  auto logicalView = vocbase.createView(createJson->slice());
  ASSERT_FALSE(!logicalView);
  auto& viewImpl =
      arangodb::LogicalView::cast<arangodb::iresearch::IResearchView>(*logicalView);
  EXPECT_FALSE(viewImpl.primarySort().empty());

  // dummy query
  MockQuery query(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      arangodb::aql::QueryString(
          "let variable = 1 let variable100 = 3 let variable101 = 2 RETURN 1"),
      nullptr, arangodb::velocypack::Parser::fromJson("{}"));
  query.prepareQuery(arangodb::aql::SerializationFormat::SHADOWROWS);

  arangodb::aql::Variable const outVariable("variable", 0, false);

  // no filter condition, no sort condition
  {
    arangodb::iresearch::IResearchViewNode node(*query.plan(),
                                                arangodb::aql::ExecutionNodeId{42},
                                                vocbase,      // database
                                                logicalView,  // view
                                                outVariable,
                                                nullptr,  // no filter condition
                                                nullptr,  // no options
                                                {});        // no sort condition
    node.sort(&viewImpl.primarySort(), 1);

    EXPECT_TRUE(node.empty());                // view has no links
    EXPECT_TRUE(node.collections().empty());  // view has no links
    EXPECT_TRUE(node.shards().empty());

    node.setVarsUsedLater({arangodb::aql::VarSet{&outVariable}});
    node.setVarsValid({{}});
    node.setRegsToKeep({{}});
    node.setVarUsageValid();

    arangodb::velocypack::Builder builder;
    unsigned flags = arangodb::aql::ExecutionNode::SERIALIZE_DETAILS;
    node.toVelocyPack(builder, flags, false);  // object with array of objects

    auto const slice = builder.slice();
    EXPECT_TRUE(slice.isObject());
    auto const nodesSlice = slice.get("nodes");
    EXPECT_TRUE(nodesSlice.isArray());
    arangodb::velocypack::ArrayIterator it(nodesSlice);
    EXPECT_EQ(1, it.size());
    auto nodeSlice = it.value();

    // constructor
    {
      arangodb::iresearch::IResearchViewNode const deserialized(*query.plan(), nodeSlice);
      EXPECT_EQ(node.empty(), deserialized.empty());
      EXPECT_EQ(node.shards(), deserialized.shards());
      EXPECT_TRUE(deserialized.collections().empty());
      EXPECT_EQ(node.getType(), deserialized.getType());
      EXPECT_EQ(node.outVariable().id, deserialized.outVariable().id);
      EXPECT_EQ(node.outVariable().name, deserialized.outVariable().name);
      EXPECT_EQ(node.plan(), deserialized.plan());
      EXPECT_EQ(node.id(), deserialized.id());
      EXPECT_EQ(&node.vocbase(), &deserialized.vocbase());
      EXPECT_EQ(node.view(), deserialized.view());
      EXPECT_EQ(&node.filterCondition(), &deserialized.filterCondition());
      EXPECT_EQ(node.scorers(), deserialized.scorers());
      EXPECT_EQ(node.volatility(), deserialized.volatility());
      EXPECT_EQ(node.options().forceSync, deserialized.options().forceSync);
      EXPECT_EQ(node.sort(), deserialized.sort());
      EXPECT_EQ(1, node.sort().second);
      EXPECT_EQ(node.getCost(), deserialized.getCost());
    }

    // factory method
    {
      std::unique_ptr<arangodb::aql::ExecutionNode> deserializedNode(
          arangodb::aql::ExecutionNode::fromVPackFactory(query.plan(), nodeSlice));
      auto& deserialized =
          dynamic_cast<arangodb::iresearch::IResearchViewNode&>(*deserializedNode);
      EXPECT_EQ(node.empty(), deserialized.empty());
      EXPECT_EQ(node.shards(), deserialized.shards());
      EXPECT_TRUE(deserialized.collections().empty());
      EXPECT_EQ(node.getType(), deserialized.getType());
      EXPECT_EQ(node.outVariable().id, deserialized.outVariable().id);
      EXPECT_EQ(node.outVariable().name, deserialized.outVariable().name);
      EXPECT_EQ(node.plan(), deserialized.plan());
      EXPECT_EQ(node.id(), deserialized.id());
      EXPECT_EQ(&node.vocbase(), &deserialized.vocbase());
      EXPECT_EQ(node.view(), deserialized.view());
      EXPECT_EQ(&node.filterCondition(), &deserialized.filterCondition());
      EXPECT_EQ(node.scorers(), deserialized.scorers());
      EXPECT_EQ(node.volatility(), deserialized.volatility());
      EXPECT_EQ(node.options().forceSync, deserialized.options().forceSync);
      EXPECT_EQ(node.sort(), deserialized.sort());
      EXPECT_EQ(1, node.sort().second);
      EXPECT_EQ(node.getCost(), deserialized.getCost());
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
                                                arangodb::aql::ExecutionNodeId{42},
                                                vocbase,      // database
                                                logicalView,  // view
                                                outVariable,
                                                nullptr,  // no filter condition
                                                &options, {});  // no sort condition
    EXPECT_TRUE(node.empty());                // view has no links
    EXPECT_TRUE(node.collections().empty());  // view has no links
    EXPECT_TRUE(node.shards().empty());
    EXPECT_TRUE(node.options().forceSync);

    node.setVarsUsedLater({arangodb::aql::VarSet{&outVariable}});
    node.setVarsValid({{}});
    node.setRegsToKeep({{}});
    node.setVarUsageValid();

    arangodb::velocypack::Builder builder;
    unsigned flags = arangodb::aql::ExecutionNode::SERIALIZE_DETAILS;
    node.toVelocyPack(builder, flags, false);  // object with array of objects

    auto const slice = builder.slice();
    EXPECT_TRUE(slice.isObject());
    auto const nodesSlice = slice.get("nodes");
    EXPECT_TRUE(nodesSlice.isArray());
    arangodb::velocypack::ArrayIterator it(nodesSlice);
    EXPECT_EQ(1, it.size());
    auto nodeSlice = it.value();

    // constructor
    {
      arangodb::iresearch::IResearchViewNode const deserialized(*query.plan(), nodeSlice);
      EXPECT_EQ(node.empty(), deserialized.empty());
      EXPECT_EQ(node.shards(), deserialized.shards());
      EXPECT_TRUE(deserialized.collections().empty());
      EXPECT_EQ(node.getType(), deserialized.getType());
      EXPECT_EQ(node.outVariable().id, deserialized.outVariable().id);
      EXPECT_EQ(node.outVariable().name, deserialized.outVariable().name);
      EXPECT_EQ(node.plan(), deserialized.plan());
      EXPECT_EQ(node.id(), deserialized.id());
      EXPECT_EQ(&node.vocbase(), &deserialized.vocbase());
      EXPECT_EQ(node.view(), deserialized.view());
      EXPECT_EQ(&node.filterCondition(), &deserialized.filterCondition());
      EXPECT_EQ(node.scorers(), deserialized.scorers());
      EXPECT_EQ(node.volatility(), deserialized.volatility());
      EXPECT_EQ(node.options().forceSync, deserialized.options().forceSync);
      EXPECT_EQ(node.sort(), deserialized.sort());
      EXPECT_EQ(0, node.sort().second);
      EXPECT_EQ(node.getCost(), deserialized.getCost());
    }

    // factory method
    {
      std::unique_ptr<arangodb::aql::ExecutionNode> deserializedNode(
          arangodb::aql::ExecutionNode::fromVPackFactory(query.plan(), nodeSlice));
      auto& deserialized =
          dynamic_cast<arangodb::iresearch::IResearchViewNode&>(*deserializedNode);
      EXPECT_EQ(node.empty(), deserialized.empty());
      EXPECT_EQ(node.shards(), deserialized.shards());
      EXPECT_TRUE(deserialized.collections().empty());
      EXPECT_EQ(node.getType(), deserialized.getType());
      EXPECT_EQ(node.outVariable().id, deserialized.outVariable().id);
      EXPECT_EQ(node.outVariable().name, deserialized.outVariable().name);
      EXPECT_EQ(node.plan(), deserialized.plan());
      EXPECT_EQ(node.id(), deserialized.id());
      EXPECT_EQ(&node.vocbase(), &deserialized.vocbase());
      EXPECT_EQ(node.view(), deserialized.view());
      EXPECT_EQ(&node.filterCondition(), &deserialized.filterCondition());
      EXPECT_EQ(node.scorers(), deserialized.scorers());
      EXPECT_EQ(node.volatility(), deserialized.volatility());
      EXPECT_EQ(node.options().forceSync, deserialized.options().forceSync);
      EXPECT_EQ(node.sort(), deserialized.sort());
      EXPECT_EQ(0, node.sort().second);
      EXPECT_EQ(node.getCost(), deserialized.getCost());
    }
  }

  // with late materialization
  {
    arangodb::iresearch::IResearchViewNode node(*query.plan(),
                                                arangodb::aql::ExecutionNodeId{42},
                                                vocbase,      // database
                                                logicalView,  // view
                                                outVariable,
                                                nullptr,  // no filter condition
                                                nullptr,  // no options
                                                {});        // no sort condition
    arangodb::aql::Variable const outNmColPtr("variable100", 1, false);
    arangodb::aql::Variable const outNmDocId("variable101", 2, false);
    node.sort(&viewImpl.primarySort(), 1);
    node.setLateMaterialized(outNmColPtr, outNmDocId);

    node.setVarsUsedLater({arangodb::aql::VarSet{&outNmColPtr, &outNmDocId}});
    node.setVarsValid({{}});
    node.setRegsToKeep({{}});
    node.setVarUsageValid();

    arangodb::velocypack::Builder builder;
    unsigned flags = arangodb::aql::ExecutionNode::SERIALIZE_DETAILS;
    node.toVelocyPack(builder, flags, false);  // object with array of objects

    auto const slice = builder.slice();
    EXPECT_TRUE(slice.isObject());
    auto const nodesSlice = slice.get("nodes");
    EXPECT_TRUE(nodesSlice.isArray());
    arangodb::velocypack::ArrayIterator it(nodesSlice);
    ASSERT_EQ(1, it.size());
    auto nodeSlice = it.value();

    // constructor
    {
      arangodb::iresearch::IResearchViewNode const deserialized(*query.plan(), nodeSlice);
      EXPECT_EQ(node.empty(), deserialized.empty());
      EXPECT_EQ(node.shards(), deserialized.shards());
      EXPECT_TRUE(deserialized.collections().empty());
      EXPECT_EQ(node.getType(), deserialized.getType());
      EXPECT_EQ(node.outVariable().id, deserialized.outVariable().id);
      EXPECT_EQ(node.outVariable().name, deserialized.outVariable().name);
      EXPECT_EQ(node.plan(), deserialized.plan());
      EXPECT_EQ(node.id(), deserialized.id());
      EXPECT_EQ(&node.vocbase(), &deserialized.vocbase());
      EXPECT_EQ(node.view(), deserialized.view());
      EXPECT_EQ(&node.filterCondition(), &deserialized.filterCondition());
      EXPECT_EQ(node.scorers(), deserialized.scorers());
      EXPECT_EQ(node.volatility(), deserialized.volatility());
      EXPECT_EQ(node.options().forceSync, deserialized.options().forceSync);
      EXPECT_EQ(node.sort(), deserialized.sort());
      EXPECT_EQ(1, node.sort().second);
      EXPECT_EQ(node.getCost(), deserialized.getCost());
      EXPECT_EQ(node.isLateMaterialized(), deserialized.isLateMaterialized());
      auto varsSetHere = deserialized.getVariablesSetHere();
      ASSERT_EQ(2, varsSetHere.size());
      EXPECT_EQ(outNmColPtr.id, varsSetHere[0]->id);
      EXPECT_EQ(outNmColPtr.name, varsSetHere[0]->name);
      EXPECT_EQ(outNmDocId.id, varsSetHere[1]->id);
      EXPECT_EQ(outNmDocId.name, varsSetHere[1]->name);
    }

    // factory method
    {
      std::unique_ptr<arangodb::aql::ExecutionNode> deserializedNode(
          arangodb::aql::ExecutionNode::fromVPackFactory(query.plan(), nodeSlice));
      auto& deserialized =
          dynamic_cast<arangodb::iresearch::IResearchViewNode&>(*deserializedNode);
      EXPECT_EQ(node.empty(), deserialized.empty());
      EXPECT_EQ(node.shards(), deserialized.shards());
      EXPECT_TRUE(deserialized.collections().empty());
      EXPECT_EQ(node.getType(), deserialized.getType());
      EXPECT_EQ(node.outVariable().id, deserialized.outVariable().id);
      EXPECT_EQ(node.outVariable().name, deserialized.outVariable().name);
      EXPECT_EQ(node.plan(), deserialized.plan());
      EXPECT_EQ(node.id(), deserialized.id());
      EXPECT_EQ(&node.vocbase(), &deserialized.vocbase());
      EXPECT_EQ(node.view(), deserialized.view());
      EXPECT_EQ(&node.filterCondition(), &deserialized.filterCondition());
      EXPECT_EQ(node.scorers(), deserialized.scorers());
      EXPECT_EQ(node.volatility(), deserialized.volatility());
      EXPECT_EQ(node.options().forceSync, deserialized.options().forceSync);
      EXPECT_EQ(node.sort(), deserialized.sort());
      EXPECT_EQ(1, node.sort().second);
      EXPECT_EQ(node.getCost(), deserialized.getCost());
      EXPECT_EQ(node.isLateMaterialized(), deserialized.isLateMaterialized());
      auto varsSetHere = deserialized.getVariablesSetHere();
      ASSERT_EQ(2, varsSetHere.size());
      EXPECT_EQ(outNmColPtr.id, varsSetHere[0]->id);
      EXPECT_EQ(outNmColPtr.name, varsSetHere[0]->name);
      EXPECT_EQ(outNmDocId.id, varsSetHere[1]->id);
      EXPECT_EQ(outNmDocId.name, varsSetHere[1]->name);
    }
  }
}

TEST_F(IResearchViewNodeTest, collections) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));

  std::shared_ptr<arangodb::LogicalCollection> collection0;
  std::shared_ptr<arangodb::LogicalCollection> collection1;

  // create collection0
  {
    auto createJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection0\", \"id\" : \"42\" }");
    collection0 = vocbase.createCollection(createJson->slice());
    ASSERT_NE(nullptr, collection0);
  }

  // create collection1
  {
    auto createJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection1\", \"id\" : \"4242\"  }");
    collection1 = vocbase.createCollection(createJson->slice());
    ASSERT_NE(nullptr, collection1);
  }

  // create collection2
  {
    auto createJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection2\" , \"id\" : \"424242\" }");
    ASSERT_NE(nullptr, vocbase.createCollection(createJson->slice()));
  }

  // create view
  auto createJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
  auto logicalView = vocbase.createView(createJson->slice());
  ASSERT_FALSE(!logicalView);

  // link collections
  auto updateJson = arangodb::velocypack::Parser::fromJson(
      "{ \"links\": {"
      "\"testCollection0\": { \"includeAllFields\": true, "
      "\"trackListPositions\": true },"
      "\"testCollection1\": { \"includeAllFields\": true },"
      "\"testCollection2\": { \"includeAllFields\": true }"
      "}}");
  EXPECT_TRUE(logicalView->properties(updateJson->slice(), true).ok());

  // dummy query
  MockQuery query(arangodb::transaction::StandaloneContext::Create(vocbase),
                  arangodb::aql::QueryString("RETURN 1"),
                  nullptr, arangodb::velocypack::Parser::fromJson("{}"));

  // register collections with the query
  query.collections().add(std::to_string(collection0->id()), arangodb::AccessMode::Type::READ, arangodb::aql::Collection::Hint::None);
  query.collections().add(std::to_string(collection1->id()), arangodb::AccessMode::Type::READ, arangodb::aql::Collection::Hint::None);

  // prepare query
  query.prepareQuery(arangodb::aql::SerializationFormat::SHADOWROWS);

  arangodb::aql::Variable const outVariable("variable", 0, false);

  // no filter condition, no sort condition
  arangodb::iresearch::IResearchViewNode node(*query.plan(),
                                              arangodb::aql::ExecutionNodeId{42},
                                              vocbase,      // database
                                              logicalView,  // view
                                              outVariable,
                                              nullptr,  // no filter condition
                                              nullptr,  // no options
                                              {});        // no sort condition
  EXPECT_TRUE(node.shards().empty());
  EXPECT_FALSE(node.empty());  // view has no links
  auto collections = node.collections();
  EXPECT_FALSE(collections.empty());  // view has no links
  EXPECT_EQ(2, collections.size());

  // we expect only collections 'collection0', 'collection1' to be
  // present since 'collection2' is not registered with the query
  std::unordered_set<std::string> expectedCollections{std::to_string(collection0->id()),
                                                      std::to_string(collection1->id())};

  for (arangodb::aql::Collection const& collection : collections) {
    EXPECT_EQ(1, expectedCollections.erase(collection.name()));
  }
  EXPECT_TRUE(expectedCollections.empty());
}

TEST_F(IResearchViewNodeTest, createBlockSingleServer) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
  auto createJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
  auto logicalView = vocbase.createView(createJson->slice());
  ASSERT_FALSE(!logicalView);

  // create collection0
  std::shared_ptr<arangodb::LogicalCollection> collection0;
  {
    auto createJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection0\", \"id\" : \"42\" }");
    collection0 = vocbase.createCollection(createJson->slice());
    ASSERT_NE(nullptr, collection0);
  }

  // link collections
  auto updateJson = arangodb::velocypack::Parser::fromJson(
      "{ \"links\": {"
      "\"testCollection0\": { \"includeAllFields\": true, "
      "\"trackListPositions\": true }"
      "}}");
  EXPECT_TRUE(logicalView->properties(updateJson->slice(), true).ok());

  // insert into collection
  {
    std::vector<std::string> const EMPTY;

    arangodb::OperationOptions opt;
    arangodb::ManagedDocumentResult mmdoc;

    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                       EMPTY, EMPTY, EMPTY,
                                       arangodb::transaction::Options());
    EXPECT_TRUE(trx.begin().ok());

    auto json = arangodb::velocypack::Parser::fromJson("{}");
    auto const res = collection0->insert(&trx, json->slice(), mmdoc, opt);
    EXPECT_TRUE(res.ok());

    EXPECT_TRUE(trx.commit().ok());
    EXPECT_TRUE(
        (arangodb::tests::executeQuery(vocbase,
                                       "FOR d IN testView SEARCH 1 ==1 OPTIONS "
                                       "{ waitForSync: true } RETURN d")
             .result.ok()));  // commit
  }

  // dummy query
  auto ctx = arangodb::transaction::StandaloneContext::Create(vocbase);
  MockQuery query(ctx, arangodb::aql::QueryString("RETURN 1"),
                  nullptr, arangodb::velocypack::Parser::fromJson("{}"));
  query.initForTests();

  // dummy engine
  arangodb::aql::ExecutionEngine engine(query, query.itemBlockManager(), arangodb::aql::SerializationFormat::SHADOWROWS);
  arangodb::aql::ExecutionPlan plan(query.ast(), false);

  arangodb::aql::Variable const outVariable("variable", 0, false);

  // no filter condition, no sort condition
  {
    arangodb::aql::SingletonNode singleton(&plan, arangodb::aql::ExecutionNodeId{0});
    arangodb::iresearch::IResearchViewNode node(plan,
                                                arangodb::aql::ExecutionNodeId{42},
                                                vocbase,      // database
                                                logicalView,  // view
                                                outVariable,
                                                nullptr,  // no filter condition
                                                nullptr,  // no options
                                                {});        // no sort condition
    node.addDependency(&singleton);

    // "Trust me, I'm an IT professional"
    singleton.setVarsUsedLater({arangodb::aql::VarSet{&outVariable}});
    singleton.setVarsValid({{}});
    singleton.setRegsToKeep({{}});
    node.setVarsUsedLater({{}});
    node.setVarsValid({arangodb::aql::VarSet{&outVariable}});
    node.setRegsToKeep({{}});
    node.setVarUsageValid();
    singleton.setVarUsageValid();

    node.planRegisters(nullptr);

    std::unordered_map<arangodb::aql::ExecutionNode*, arangodb::aql::ExecutionBlock*> EMPTY;

    // before transaction has started (no snapshot)
    try {
      auto block = node.createBlock(engine, EMPTY);
      EXPECT_TRUE(false);
    } catch (arangodb::basics::Exception const& e) {
      EXPECT_EQ(TRI_ERROR_INTERNAL, e.code());
    }

    arangodb::transaction::Methods trx(ctx);
    ASSERT_TRUE(trx.state());

    // start transaction (put snapshot into)
    EXPECT_TRUE(nullptr ==
                arangodb::LogicalView::cast<arangodb::iresearch::IResearchView>(*logicalView)
                    .snapshot(trx,
                              arangodb::iresearch::IResearchView::SnapshotMode::Find));
    auto* snapshot =
        arangodb::LogicalView::cast<arangodb::iresearch::IResearchView>(*logicalView)
            .snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate);
    EXPECT_TRUE(snapshot ==
                arangodb::LogicalView::cast<arangodb::iresearch::IResearchView>(*logicalView)
                    .snapshot(trx,
                              arangodb::iresearch::IResearchView::SnapshotMode::Find));
    EXPECT_TRUE(snapshot ==
                arangodb::LogicalView::cast<arangodb::iresearch::IResearchView>(*logicalView)
                    .snapshot(trx,
                              arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate));
    EXPECT_TRUE(snapshot ==
                arangodb::LogicalView::cast<arangodb::iresearch::IResearchView>(*logicalView)
                    .snapshot(trx,
                              arangodb::iresearch::IResearchView::SnapshotMode::SyncAndReplace));

    // after transaction has started
    {
      auto block = node.createBlock(engine, EMPTY);
      EXPECT_NE(nullptr, block);
      EXPECT_NE(nullptr,
                  (dynamic_cast<arangodb::aql::ExecutionBlockImpl<arangodb::aql::IResearchViewExecutor<false, arangodb::iresearch::MaterializeType::Materialize>>*>(
                      block.get())));
    }
  }
}

// FIXME TODO
// TEST_F(IResearchViewNodeTest, createBlockDBServer) {
//}

TEST_F(IResearchViewNodeTest, createBlockCoordinator) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
  auto createJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
  auto logicalView = vocbase.createView(createJson->slice());
  ASSERT_FALSE(!logicalView);

  // dummy query
  MockQuery query(arangodb::transaction::StandaloneContext::Create(vocbase),
                  arangodb::aql::QueryString("RETURN 1"),
                  nullptr, arangodb::velocypack::Parser::fromJson("{}"));
    query.initForTests();

  // dummy engine
  arangodb::aql::ExecutionEngine engine(query, query.itemBlockManager(), arangodb::aql::SerializationFormat::SHADOWROWS);
  arangodb::aql::ExecutionPlan plan(query.ast(), false);

  // dummy engine
  arangodb::aql::SingletonNode singleton(&plan, arangodb::aql::ExecutionNodeId{0});
  arangodb::aql::Variable const outVariable("variable", 0, false);

  // no filter condition, no sort condition
  arangodb::iresearch::IResearchViewNode node(plan,
                                              arangodb::aql::ExecutionNodeId{42},
                                              vocbase,      // database
                                              logicalView,  // view
                                              outVariable,
                                              nullptr,  // no filter condition
                                              nullptr,  // no options
                                              {});        // no sort condition
  node.addDependency(&singleton);

  std::unordered_map<arangodb::aql::ExecutionNode*, arangodb::aql::ExecutionBlock*> EMPTY;
  singleton.setVarsUsedLater({arangodb::aql::VarSet{&outVariable}});
  singleton.setVarsValid({{}});
  node.setVarsUsedLater({{}});
  node.setVarsValid({arangodb::aql::VarSet{&outVariable}});
  singleton.setVarUsageValid();
  node.setVarUsageValid();
  singleton.planRegisters(nullptr);
  node.planRegisters(nullptr);
  arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_COORDINATOR);
  auto emptyBlock = node.createBlock(engine, EMPTY);
  arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_SINGLE);
  EXPECT_NE(nullptr, emptyBlock);
  EXPECT_TRUE(nullptr !=
              dynamic_cast<arangodb::aql::ExecutionBlockImpl<arangodb::aql::NoResultsExecutor>*>(
                  emptyBlock.get()));
}

TEST_F(IResearchViewNodeTest, createBlockCoordinatorLateMaterialize) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server(),
                        "testVocbase", 1));
  auto createJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
  auto logicalView = vocbase.createView(createJson->slice());
  ASSERT_TRUE((false == !logicalView));

  // dummy query
  MockQuery query(arangodb::transaction::StandaloneContext::Create(vocbase),
                  arangodb::aql::QueryString("RETURN 1"),
                  nullptr, arangodb::velocypack::Parser::fromJson("{}"));
  query.prepareQuery(arangodb::aql::SerializationFormat::SHADOWROWS);

  // dummy engine
  arangodb::aql::ExecutionEngine engine(query, query.itemBlockManager(), arangodb::aql::SerializationFormat::SHADOWROWS);
  arangodb::aql::SingletonNode singleton(query.plan(), arangodb::aql::ExecutionNodeId{0});
  arangodb::aql::Variable const outVariable("variable", 0, false);
  arangodb::aql::Variable const outNmColPtr("variable100", 100, false);
  arangodb::aql::Variable const outNmDocId("variable101", 101, false);

  // no filter condition, no sort condition
  arangodb::iresearch::IResearchViewNode node(*query.plan(),
                                              arangodb::aql::ExecutionNodeId{42},
                                              vocbase,      // database
                                              logicalView,  // view
                                              outVariable,
                                              nullptr,  // no filter condition
                                              nullptr,  // no options
                                              {});        // no sort condition
  node.addDependency(&singleton);
  node.setLateMaterialized(outNmColPtr, outNmDocId);
  std::unordered_map<arangodb::aql::ExecutionNode*, arangodb::aql::ExecutionBlock*> EMPTY;
  singleton.setVarsUsedLater({arangodb::aql::VarSet{&outNmColPtr, &outNmDocId}});
  singleton.setVarsValid({{}});
  node.setVarsUsedLater({{}});
  node.setVarsValid({arangodb::aql::VarSet{&outNmColPtr, &outNmDocId}});
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


class IResearchViewVolatitlityTest
  : public ::testing::Test,
  public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION, arangodb::LogLevel::ERR> {
protected:
  arangodb::tests::mocks::MockAqlServer server;
  TRI_vocbase_t* vocbase{nullptr};

  IResearchViewVolatitlityTest() : server(false) {
    arangodb::tests::init(true);

    server.addFeature<arangodb::FlushFeature>(false);
    server.startFeatures();

    auto& dbPathFeature = server.getFeature<arangodb::DatabasePathFeature>();
    arangodb::tests::setDatabasePath(dbPathFeature);  // ensure test data is stored in a unique directory

    auto& dbFeature = server.getFeature<arangodb::DatabaseFeature>();
    vocbase = dbFeature.useDatabase(arangodb::StaticStrings::SystemDatabase);
    EXPECT_NE(nullptr, vocbase);
    std::shared_ptr<arangodb::LogicalCollection> collection0;
    {
      auto createJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection0\", \"id\" : \"42\" }");
      collection0 = vocbase->createCollection(createJson->slice());
      EXPECT_NE(nullptr, collection0);
    }
    std::shared_ptr<arangodb::LogicalCollection> collection1;
    {
      auto createJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection1\", \"id\" : \"43\" }");
      collection1 = vocbase->createCollection(createJson->slice());
      EXPECT_NE(nullptr, collection1);
    }
    arangodb::LogicalView::ptr logicalView0;
    {
      auto createJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView0\", \"type\": \"arangosearch\" }");
      logicalView0 = vocbase->createView(createJson->slice());
      EXPECT_NE(nullptr, logicalView0);
      auto updateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": {"
        "\"testCollection0\": { \"includeAllFields\": true, "
        "\"trackListPositions\": true }"
        "}}");
      EXPECT_TRUE(logicalView0->properties(updateJson->slice(), true).ok());
    }
    arangodb::LogicalView::ptr logicalView1;
    {
      auto createJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView1\", \"type\": \"arangosearch\" }");
      logicalView1 = vocbase->createView(createJson->slice());
      EXPECT_NE(nullptr, logicalView1);
      auto updateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": {"
        "\"testCollection1\": { \"includeAllFields\": true, "
        "\"trackListPositions\": true }"
        "}}");
      EXPECT_TRUE(logicalView1->properties(updateJson->slice(), true).ok());
    }

    std::vector<std::string> EMPTY_VECTOR;
    auto trx = std::make_shared<arangodb::transaction::Methods>(
      arangodb::transaction::StandaloneContext::Create(*vocbase), EMPTY_VECTOR,
      EMPTY_VECTOR, EMPTY_VECTOR, arangodb::transaction::Options());

    EXPECT_TRUE(trx->begin().ok());
    // in collection only one alive doc
    {
      auto aliveDoc0 = arangodb::velocypack::Parser::fromJson("{ \"key\": 1 }");
      arangodb::ManagedDocumentResult insertResult;
      arangodb::OperationOptions options;
      EXPECT_TRUE(collection0
        ->insert(trx.get(), aliveDoc0->slice(), insertResult, options)
        .ok());

      auto aliveDoc1 = arangodb::velocypack::Parser::fromJson("{ \"key\": 2 }");
      arangodb::ManagedDocumentResult insertResult1;
      arangodb::OperationOptions options1;
      EXPECT_TRUE(collection0
        ->insert(trx.get(), aliveDoc1->slice(), insertResult1, options1)
        .ok());
    }
    {
      auto aliveDoc1 = arangodb::velocypack::Parser::fromJson("{ \"key\": 1 }");
      arangodb::ManagedDocumentResult insertResult;
      arangodb::OperationOptions options;
      EXPECT_TRUE(collection1
        ->insert(trx.get(), aliveDoc1->slice(), insertResult, options)
        .ok());
      arangodb::iresearch::IResearchLinkMeta meta;
    }
    EXPECT_TRUE(trx->commit().ok());
    // force views sync
    auto res = arangodb::tests::executeQuery(
        *vocbase, 
        "FOR s IN testView0 OPTIONS { waitForSync: true } LET kk = s.key "
        "FOR d IN testView1 SEARCH d.key == kk "
        "OPTIONS { waitForSync: true } RETURN d ");
    EXPECT_TRUE(res.ok());
    EXPECT_TRUE(res.data->slice().isArray());
    EXPECT_NE(0, res.data->slice().length());
  }
};

TEST_F(IResearchViewVolatitlityTest, volatilityFilterSubqueryWithVar) {
  std::string const queryString = "FOR s IN testView0 LET kk = s.key "
                                  "FOR d IN testView1 SEARCH d.key == kk RETURN d";
  auto prepared = arangodb::tests::prepareQuery(*vocbase, queryString);
  auto plan = prepared->plan();
  ASSERT_NE(nullptr, plan);
  ::arangodb::containers::SmallVector<arangodb::aql::ExecutionNode*>::allocator_type::arena_type a;
  ::arangodb::containers::SmallVector<arangodb::aql::ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, { arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW }, true);
  ASSERT_EQ(2, nodes.size());
  for (auto const* n : nodes) {
    arangodb::iresearch::IResearchViewNode const* view =
      static_cast<arangodb::iresearch::IResearchViewNode const*>(n);
    if (view->view()->name() == "testView1") {
      ASSERT_TRUE(view->volatility().first);
      ASSERT_FALSE(view->volatility().second);
    } else if (view->view()->name() == "testView0") {
      ASSERT_FALSE(view->volatility().first);
      ASSERT_FALSE(view->volatility().second);
    } else {
      ASSERT_TRUE(false); // unexpected node
    }
  }
  // check volatility affects results
  auto res = arangodb::tests::executeQuery(*vocbase, queryString);

  EXPECT_TRUE(res.ok());
  EXPECT_TRUE(res.data->slice().isArray());
  EXPECT_EQ(1, res.data->slice().length());
}

TEST_F(IResearchViewVolatitlityTest, volatilityFilterSubquery) {
  std::string const queryString = "FOR s IN testView0 "
                                  "FOR d IN testView1 SEARCH d.key == s.key RETURN d";
  auto prepared = arangodb::tests::prepareQuery(*vocbase, queryString);
  auto plan = prepared->plan();
  ASSERT_NE(nullptr, plan);
  ::arangodb::containers::SmallVector<arangodb::aql::ExecutionNode*>::allocator_type::arena_type a;
  ::arangodb::containers::SmallVector<arangodb::aql::ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, { arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW }, true);
  ASSERT_EQ(2, nodes.size());
  for (auto const* n : nodes) {
    arangodb::iresearch::IResearchViewNode const* view =
      static_cast<arangodb::iresearch::IResearchViewNode const*>(n);
    if (view->view()->name() == "testView1") {
      ASSERT_TRUE(view->volatility().first);
      ASSERT_FALSE(view->volatility().second);
    } else if (view->view()->name() == "testView0") {
      ASSERT_FALSE(view->volatility().first);
      ASSERT_FALSE(view->volatility().second);
    } else {
      ASSERT_TRUE(false); // unexpected node
    }
  }
  // check volatility affects results
  auto res = arangodb::tests::executeQuery(*vocbase, queryString);

  EXPECT_TRUE(res.ok());
  EXPECT_TRUE(res.data->slice().isArray());
  EXPECT_EQ(1, res.data->slice().length());
}

TEST_F(IResearchViewVolatitlityTest, volatilityFilterNonDetVar) {
  std::string const queryString = "FOR s IN testView0 LET kk = NOOPT(s.key) "
                                  "FOR d IN testView1 SEARCH d.key == kk RETURN d";
  auto prepared = arangodb::tests::prepareQuery(*vocbase, queryString);
  auto plan = prepared->plan();
  ASSERT_NE(nullptr, plan);
  ::arangodb::containers::SmallVector<arangodb::aql::ExecutionNode*>::allocator_type::arena_type a;
  ::arangodb::containers::SmallVector<arangodb::aql::ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, { arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW }, true);
  ASSERT_EQ(2, nodes.size());
  for (auto const* n : nodes) {
    arangodb::iresearch::IResearchViewNode const* view =
      static_cast<arangodb::iresearch::IResearchViewNode const*>(n);
    if (view->view()->name() == "testView1") {
      ASSERT_TRUE(view->volatility().first);
      ASSERT_FALSE(view->volatility().second);
    } else if (view->view()->name() == "testView0") {
      ASSERT_FALSE(view->volatility().first);
      ASSERT_FALSE(view->volatility().second);
    } else {
      ASSERT_TRUE(false); // unexpected node
    }
  }
  // check volatility affects results
  auto res = arangodb::tests::executeQuery(*vocbase, queryString);

  EXPECT_TRUE(res.ok());
  EXPECT_TRUE(res.data->slice().isArray());
  EXPECT_EQ(1, res.data->slice().length());
}

TEST_F(IResearchViewVolatitlityTest, volatilityFilterListWithVar) {
  std::string const queryString = "FOR s IN 1..2 LET kk = s "
                                  "FOR d IN testView1 SEARCH d.key == kk RETURN d";
  auto prepared = arangodb::tests::prepareQuery(*vocbase, queryString);
  auto plan = prepared->plan();
  ASSERT_NE(nullptr, plan);
  ::arangodb::containers::SmallVector<arangodb::aql::ExecutionNode*>::allocator_type::arena_type a;
  ::arangodb::containers::SmallVector<arangodb::aql::ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, { arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW }, true);
  ASSERT_EQ(1, nodes.size());
  for (auto const* n : nodes) {
    arangodb::iresearch::IResearchViewNode const* view =
      static_cast<arangodb::iresearch::IResearchViewNode const*>(n);
    if (view->view()->name() == "testView1") {
      ASSERT_TRUE(view->volatility().first);
      ASSERT_FALSE(view->volatility().second);
    } else {
      ASSERT_TRUE(false); // unexpected node
    }
  }
  // check volatility affects results
  auto res = arangodb::tests::executeQuery(*vocbase, queryString);

  EXPECT_TRUE(res.ok());
  EXPECT_TRUE(res.data->slice().isArray());
  EXPECT_EQ(1, res.data->slice().length());
}

TEST_F(IResearchViewVolatitlityTest, volatilityFilterList) {
  std::string const queryString = "FOR s IN 1..2 "
                                  "FOR d IN testView1 SEARCH d.key == s RETURN d";
  auto prepared = arangodb::tests::prepareQuery(*vocbase, queryString);
  auto plan = prepared->plan();
  ASSERT_NE(nullptr, plan);
  ::arangodb::containers::SmallVector<arangodb::aql::ExecutionNode*>::allocator_type::arena_type a;
  ::arangodb::containers::SmallVector<arangodb::aql::ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, { arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW }, true);
  ASSERT_EQ(1, nodes.size());
  for (auto const* n : nodes) {
    arangodb::iresearch::IResearchViewNode const* view =
      static_cast<arangodb::iresearch::IResearchViewNode const*>(n);
    if (view->view()->name() == "testView1") {
      ASSERT_TRUE(view->volatility().first);
      ASSERT_FALSE(view->volatility().second);
    } else {
      ASSERT_TRUE(false); // unexpected node
    }
  }
  // check volatility affects results
  auto res = arangodb::tests::executeQuery(*vocbase, queryString);

  EXPECT_TRUE(res.ok());
  EXPECT_TRUE(res.data->slice().isArray());
  EXPECT_EQ(1, res.data->slice().length());
}

TEST_F(IResearchViewVolatitlityTest, volatilityFilterListNonVolatile) {
  std::string const queryString = "FOR s IN 1..20 LET kk = NOEVAL(1) "
                                  "FOR d IN testView1 SEARCH d.key == kk RETURN d";
  auto prepared = arangodb::tests::prepareQuery(*vocbase, queryString);
  auto plan = prepared->plan();
  ASSERT_NE(nullptr, plan);
  ::arangodb::containers::SmallVector<arangodb::aql::ExecutionNode*>::allocator_type::arena_type a;
  ::arangodb::containers::SmallVector<arangodb::aql::ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, { arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW }, true);
  ASSERT_EQ(1, nodes.size());
  for (auto const* n : nodes) {
    arangodb::iresearch::IResearchViewNode const* view =
      static_cast<arangodb::iresearch::IResearchViewNode const*>(n);
    if (view->view()->name() == "testView1") {
      ASSERT_FALSE(view->volatility().first);
      ASSERT_FALSE(view->volatility().second);
    } else {
      ASSERT_TRUE(false); // unexpected node
    }
  }
  // check volatility affects results
  auto res = arangodb::tests::executeQuery(*vocbase, queryString);

  EXPECT_TRUE(res.ok());
  EXPECT_TRUE(res.data->slice().isArray());
  EXPECT_EQ(20, res.data->slice().length());
}

TEST_F(IResearchViewVolatitlityTest, volatilityFilterQueryNonVolatile) {
  std::string const queryString = "FOR s IN testView0 COLLECT WITH COUNT INTO c LET kk = NOEVAL(c) "
                                  "FOR d IN testView1 SEARCH d.key == kk RETURN d";
  auto prepared = arangodb::tests::prepareQuery(*vocbase, queryString);
  auto plan = prepared->plan();
  ASSERT_NE(nullptr, plan);
  ::arangodb::containers::SmallVector<arangodb::aql::ExecutionNode*>::allocator_type::arena_type a;
  ::arangodb::containers::SmallVector<arangodb::aql::ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, { arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW }, true);
  ASSERT_EQ(2, nodes.size());
  for (auto const* n : nodes) {
    arangodb::iresearch::IResearchViewNode const* view =
      static_cast<arangodb::iresearch::IResearchViewNode const*>(n);
    if (view->view()->name() == "testView1") {
      ASSERT_TRUE(view->volatility().first);
      ASSERT_FALSE(view->volatility().second);
    } else if (view->view()->name() == "testView0") {
      ASSERT_FALSE(view->volatility().first);
      ASSERT_FALSE(view->volatility().second);
    } else {
      ASSERT_TRUE(false); // unexpected node
    }
  }
  // check volatility affects results
  auto res = arangodb::tests::executeQuery(*vocbase, queryString);

  EXPECT_TRUE(res.ok());
  EXPECT_TRUE(res.data->slice().isArray());
  EXPECT_EQ(0, res.data->slice().length());
}

TEST_F(IResearchViewVolatitlityTest, volatilityFilterListSubquery) {
  std::string const queryString = "FOR s IN 1..20 LET kk = (FOR v IN testView0 SEARCH v.key == 1 RETURN v.key)[0] "
                                  "FOR d IN testView1 SEARCH d.key == kk RETURN d";
  auto prepared = arangodb::tests::prepareQuery(*vocbase, queryString);
  auto plan = prepared->plan();
  ASSERT_NE(nullptr, plan);
  ::arangodb::containers::SmallVector<arangodb::aql::ExecutionNode*>::allocator_type::arena_type a;
  ::arangodb::containers::SmallVector<arangodb::aql::ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, { arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW }, true);
  ASSERT_EQ(2, nodes.size());
  for (auto const* n : nodes) {
    arangodb::iresearch::IResearchViewNode const* view =
      static_cast<arangodb::iresearch::IResearchViewNode const*>(n);
    if (view->view()->name() == "testView1") {
      ASSERT_TRUE(view->volatility().first);
      ASSERT_FALSE(view->volatility().second);
    } else if (view->view()->name() == "testView0") {
      ASSERT_FALSE(view->volatility().first);
      ASSERT_FALSE(view->volatility().second);
    } else {
      ASSERT_TRUE(false); // unexpected node
    }
  }
  // check volatility affects results
  auto res = arangodb::tests::executeQuery(*vocbase, queryString);

  EXPECT_TRUE(res.ok());
  EXPECT_TRUE(res.data->slice().isArray());
  EXPECT_EQ(20, res.data->slice().length());
}

TEST_F(IResearchViewVolatitlityTest, volatilitySortFilterListSubquery) {
  std::string const queryString = "FOR s IN 1..20 LET kk = (FOR v IN testView0 SEARCH v.key == 1 RETURN v.key)[0] "
                                  "FOR d IN testView1 SEARCH d.key == kk SORT BM25(d, kk) RETURN d";
  auto prepared = arangodb::tests::prepareQuery(*vocbase, queryString);
  auto plan = prepared->plan();
  ASSERT_NE(nullptr, plan);
  ::arangodb::containers::SmallVector<arangodb::aql::ExecutionNode*>::allocator_type::arena_type a;
  ::arangodb::containers::SmallVector<arangodb::aql::ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, { arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW }, true);
  ASSERT_EQ(2, nodes.size());
  for (auto const* n : nodes) {
    arangodb::iresearch::IResearchViewNode const* view =
      static_cast<arangodb::iresearch::IResearchViewNode const*>(n);
    if (view->view()->name() == "testView1") {
      ASSERT_TRUE(view->volatility().first);
      ASSERT_TRUE(view->volatility().second);
    } else if (view->view()->name() == "testView0") {
      ASSERT_FALSE(view->volatility().first);
      ASSERT_FALSE(view->volatility().second);
    } else {
      ASSERT_TRUE(false); // unexpected node
    }
  }
  // check volatility affects results
  auto res = arangodb::tests::executeQuery(*vocbase, queryString);

  EXPECT_TRUE(res.ok());
  EXPECT_TRUE(res.data->slice().isArray());
  EXPECT_EQ(20, res.data->slice().length());
}

TEST_F(IResearchViewVolatitlityTest, volatilitySortNonVolatileFilter) {
  std::string const queryString = "FOR s IN 1..20 LET kk = (FOR v IN testView0 SEARCH v.key == 1 RETURN v.key)[0] "
                                  "FOR d IN testView1 SEARCH d.key == 1 SORT BM25(d, kk) RETURN d";
  auto prepared = arangodb::tests::prepareQuery(*vocbase, queryString);
  auto plan = prepared->plan();
  ASSERT_NE(nullptr, plan);
  ::arangodb::containers::SmallVector<arangodb::aql::ExecutionNode*>::allocator_type::arena_type a;
  ::arangodb::containers::SmallVector<arangodb::aql::ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, { arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW }, true);
  ASSERT_EQ(2, nodes.size());
  for (auto const* n : nodes) {
    arangodb::iresearch::IResearchViewNode const* view =
      static_cast<arangodb::iresearch::IResearchViewNode const*>(n);
    if (view->view()->name() == "testView1") {
      ASSERT_FALSE(view->volatility().first);
      ASSERT_TRUE(view->volatility().second);
    } else if (view->view()->name() == "testView0") {
      ASSERT_FALSE(view->volatility().first);
      ASSERT_FALSE(view->volatility().second);
    } else {
      ASSERT_TRUE(false); // unexpected node
    }
  }
  // check volatility affects results
  auto res = arangodb::tests::executeQuery(*vocbase, queryString);

  EXPECT_TRUE(res.ok());
  EXPECT_TRUE(res.data->slice().isArray());
  EXPECT_EQ(20, res.data->slice().length());
}

class IResearchViewBlockTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION, arangodb::LogLevel::ERR> {
 protected:
  arangodb::tests::mocks::MockAqlServer server;

  IResearchViewBlockTest() : server(false) {
    arangodb::tests::init(true);

    server.addFeature<arangodb::FlushFeature>(false);
    server.startFeatures();

    auto& dbPathFeature = server.getFeature<arangodb::DatabasePathFeature>();
    arangodb::tests::setDatabasePath(dbPathFeature);  // ensure test data is stored in a unique directory

    auto& dbFeature = server.getFeature<arangodb::DatabaseFeature>();
    auto vocbase = dbFeature.useDatabase(arangodb::StaticStrings::SystemDatabase);
    std::shared_ptr<arangodb::LogicalCollection> collection0;
    {
      auto createJson = arangodb::velocypack::Parser::fromJson(
          "{ \"name\": \"testCollection0\", \"id\" : \"42\" }");
      collection0 = vocbase->createCollection(createJson->slice());
      EXPECT_NE(nullptr, collection0);
    }
    auto createJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto logicalView = vocbase->createView(createJson->slice());
    EXPECT_NE(nullptr, logicalView);
    auto updateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": {"
        "\"testCollection0\": { \"includeAllFields\": true, "
        "\"trackListPositions\": true }"
        "}}");
    EXPECT_TRUE(logicalView->properties(updateJson->slice(), true).ok());
    std::vector<std::string> EMPTY_VECTOR;
    auto trx = std::make_shared<arangodb::transaction::Methods>(
        arangodb::transaction::StandaloneContext::Create(*vocbase), EMPTY_VECTOR,
        EMPTY_VECTOR, EMPTY_VECTOR, arangodb::transaction::Options());

    EXPECT_TRUE(trx->begin().ok());
    // Fill dummy data in index only (to simulate some documents where already removed from collection)
    arangodb::iresearch::IResearchLinkMeta meta;
    meta._includeAllFields = true;
    {
      auto doc = arangodb::velocypack::Parser::fromJson("{ \"key\": 1 }");
      auto indexes = collection0->getIndexes();
      EXPECT_FALSE(indexes.empty());
      auto* l =
          static_cast<arangodb::iresearch::IResearchLinkMock*>(indexes[0].get());
      for (size_t i = 2; i < 10; ++i) {
        l->insert(*trx, arangodb::LocalDocumentId(i), doc->slice(), arangodb::Index::normal);
      }
    }
    // in collection only one alive doc
    auto aliveDoc = arangodb::velocypack::Parser::fromJson("{ \"key\": 1 }");
    arangodb::ManagedDocumentResult insertResult;
    arangodb::OperationOptions options;
    EXPECT_TRUE(collection0
                    ->insert(trx.get(), aliveDoc->slice(), insertResult, options)
                    .ok());
    EXPECT_TRUE(trx->commit().ok());
  }
};

TEST_F(IResearchViewBlockTest, retrieveWithMissingInCollectionUnordered) {
  auto& dbFeature = server.getFeature<arangodb::DatabaseFeature>();
  auto vocbase = dbFeature.useDatabase(arangodb::StaticStrings::SystemDatabase);
  auto queryResult =
    arangodb::tests::executeQuery(*vocbase,
                                  "FOR d IN testView OPTIONS { waitForSync: true } RETURN d");
  ASSERT_TRUE(queryResult.result.ok());
  auto result = queryResult.data->slice();
  EXPECT_TRUE(result.isArray());
  arangodb::velocypack::ArrayIterator resultIt(result);
  ASSERT_EQ(1, resultIt.size());
}

TEST_F(IResearchViewBlockTest, retrieveWithMissingInCollection) {
  auto& dbFeature = server.getFeature<arangodb::DatabaseFeature>();
  auto vocbase = dbFeature.useDatabase(arangodb::StaticStrings::SystemDatabase);
  auto queryResult =
    arangodb::tests::executeQuery(*vocbase,
                                  "FOR d IN testView  OPTIONS { waitForSync: true } SORT BM25(d) RETURN d");
  ASSERT_TRUE(queryResult.result.ok());
  auto result = queryResult.data->slice();
  EXPECT_TRUE(result.isArray());
  arangodb::velocypack::ArrayIterator resultIt(result);
  ASSERT_EQ(1, resultIt.size());
}

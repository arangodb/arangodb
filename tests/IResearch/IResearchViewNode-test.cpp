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

#include "catch.hpp"
#include "common.h"

#include "IResearch/IResearchViewNode.h"

#include "StorageEngineMock.h"

#if USE_ENTERPRISE
  #include "Enterprise/Ldap/LdapFeature.h"
#endif

#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/BasicBlocks.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/OptimizerRulesFeature.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/ApplicationServerHelper.h"
#include "IResearch/IResearchFilterFactory.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchView.h"
#include "IResearch/IResearchViewNode.h"
#include "IResearch/IResearchViewBlock.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "Logger/Logger.h"
#include "Logger/LogTopic.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/TraverserEngineRegistryFeature.h"
#include "Sharding/ShardingFeature.h"
#include "Basics/VelocyPackHelper.h"
#include "Aql/Ast.h"
#include "Aql/Query.h"
#include "Aql/ExecutionPlan.h"
#include "3rdParty/iresearch/tests/tests_config.hpp"

#include "IResearch/VelocyPackHelper.h"
#include "analysis/analyzers.hpp"
#include "analysis/token_attributes.hpp"
#include "utils/utf8_path.hpp"

#include "velocypack/Iterator.h"

namespace {

struct IResearchViewNodeSetup {
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::unique_ptr<TRI_vocbase_t> system;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  IResearchViewNodeSetup(): engine(server), server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

    arangodb::tests::init(true);

    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::WARN);

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(), arangodb::LogLevel::ERR); // suppress WARNING DefaultCustomTypeHandler called
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::FATAL);
    irs::logger::output_le(iresearch::logger::IRL_FATAL, stderr);

    // setup required application features
    features.emplace_back(new arangodb::ViewTypesFeature(server), true);
    features.emplace_back(new arangodb::AuthenticationFeature(server), true);
    features.emplace_back(new arangodb::DatabasePathFeature(server), false);
    features.emplace_back(new arangodb::DatabaseFeature(server), false);
    features.emplace_back(new arangodb::ShardingFeature(server), false);
    features.emplace_back(new arangodb::QueryRegistryFeature(server), false); // must be first
    arangodb::application_features::ApplicationServer::server->addFeature(features.back().first); // need QueryRegistryFeature feature to be added now in order to create the system database
    system = irs::memory::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 0, TRI_VOC_SYSTEM_DATABASE);
    features.emplace_back(new arangodb::SystemDatabaseFeature(server, system.get()), false); // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::TraverserEngineRegistryFeature(server), false); // must be before AqlFeature
    features.emplace_back(new arangodb::AqlFeature(server), true);
    features.emplace_back(new arangodb::aql::OptimizerRulesFeature(server), true);
    features.emplace_back(new arangodb::aql::AqlFunctionFeature(server), true); // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::iresearch::IResearchAnalyzerFeature(server), true);
    features.emplace_back(new arangodb::iresearch::IResearchFeature(server), true);

    #if USE_ENTERPRISE
      features.emplace_back(new arangodb::LdapFeature(server), false); // required for AuthenticationFeature with USE_ENTERPRISE
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

    auto* dbPathFeature = arangodb::application_features::ApplicationServer::getFeature<arangodb::DatabasePathFeature>("DatabasePath");
    arangodb::tests::setDatabasePath(*dbPathFeature); // ensure test data is stored in a unique directory
  }

  ~IResearchViewNodeSetup() {
    system.reset(); // destroy before reseting the 'ENGINE'
    arangodb::AqlFeature(server).stop(); // unset singleton instance
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(), arangodb::LogLevel::DEFAULT);
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

    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::DEFAULT);
  }
}; // IResearchViewNodeSetup

}

TEST_CASE("IResearchViewNodeTest", "[iresearch][iresearch-view-node]") {
  IResearchViewNodeSetup s;
  UNUSED(s);

SECTION("construct") {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  // create view
  auto createJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
  auto logicalView = vocbase.createView(createJson->slice());
  REQUIRE((false == !logicalView));

  // dummy query
  arangodb::aql::Query query(
    false, vocbase, arangodb::aql::QueryString("RETURN 1"),
    nullptr, arangodb::velocypack::Parser::fromJson("{}"),
    arangodb::aql::PART_MAIN
  );
  query.prepare(arangodb::QueryRegistryFeature::registry());
  arangodb::aql::Variable const outVariable("variable", 0);

  // no options
  {
    arangodb::iresearch::IResearchViewNode node(
      *query.plan(), // plan
      42, // id
      vocbase, // database
      logicalView, // view
      outVariable, // out variable
      nullptr, // no filter condition
      nullptr, // no options
      {} // no sort condition
    );

    CHECK(node.empty()); // view has no links
    CHECK(node.collections().empty()); // view has no links
    CHECK(node.shards().empty());

    CHECK(arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW == node.getType());
    CHECK(&outVariable == &node.outVariable());
    CHECK(query.plan() == node.plan());
    CHECK(42 == node.id());
    CHECK(logicalView == node.view());
    CHECK(node.sortCondition().empty());
    CHECK(!node.volatility().first); // filter volatility
    CHECK(!node.volatility().second); // sort volatility
    CHECK(node.getVariablesUsedHere().empty());
    auto const setHere = node.getVariablesSetHere();
    CHECK(1 == setHere.size());
    CHECK(&outVariable == setHere[0]);
    CHECK(false == node.options().forceSync);

    CHECK(0. == node.getCost().estimatedCost); // no dependencies
    CHECK(0 == node.getCost().estimatedNrItems); // no dependencies
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

    arangodb::iresearch::IResearchViewNode node(
      *query.plan(), // plan
      42, // id
      vocbase, // database
      logicalView, // view
      outVariable, // out variable
      nullptr, // no filter condition
      &options,
      {} // no sort condition
    );

    CHECK(node.empty()); // view has no links
    CHECK(node.collections().empty()); // view has no links
    CHECK(node.shards().empty());

    CHECK(arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW == node.getType());
    CHECK(&outVariable == &node.outVariable());
    CHECK(query.plan() == node.plan());
    CHECK(42 == node.id());
    CHECK(logicalView == node.view());
    CHECK(node.sortCondition().empty());
    CHECK(!node.volatility().first); // filter volatility
    CHECK(!node.volatility().second); // sort volatility
    CHECK(node.getVariablesUsedHere().empty());
    auto const setHere = node.getVariablesSetHere();
    CHECK(1 == setHere.size());
    CHECK(&outVariable == setHere[0]);
    CHECK(true == node.options().forceSync);

    CHECK(0. == node.getCost().estimatedCost); // no dependencies
    CHECK(0 == node.getCost().estimatedNrItems); // no dependencies
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

    CHECK_THROWS(arangodb::iresearch::IResearchViewNode(
      *query.plan(), // plan
      42, // id
      vocbase, // database
      logicalView, // view
      outVariable, // out variable
      nullptr, // no filter condition
      &options,
      {} // no sort condition
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

    arangodb::iresearch::IResearchViewNode node(
      *query.plan(), // plan
      42, // id
      vocbase, // database
      logicalView, // view
      outVariable, // out variable
      nullptr, // no filter condition
      &options,
      {} // no sort condition
    );

    CHECK(node.empty()); // view has no links
    CHECK(node.collections().empty()); // view has no links
    CHECK(node.shards().empty());

    CHECK(arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW == node.getType());
    CHECK(&outVariable == &node.outVariable());
    CHECK(query.plan() == node.plan());
    CHECK(42 == node.id());
    CHECK(logicalView == node.view());
    CHECK(node.sortCondition().empty());
    CHECK(!node.volatility().first); // filter volatility
    CHECK(!node.volatility().second); // sort volatility
    CHECK(node.getVariablesUsedHere().empty());
    auto const setHere = node.getVariablesSetHere();
    CHECK(1 == setHere.size());
    CHECK(&outVariable == setHere[0]);
    CHECK(false == node.options().forceSync);

    CHECK(0. == node.getCost().estimatedCost); // no dependencies
    CHECK(0 == node.getCost().estimatedNrItems); // no dependencies
  }
}

SECTION("constructFromVPackSingleServer") {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  // create view
  auto createJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
  auto logicalView = vocbase.createView(createJson->slice());
  REQUIRE((false == !logicalView));

  // dummy query
  arangodb::aql::Query query(
    false, vocbase, arangodb::aql::QueryString("RETURN 1"),
    nullptr, arangodb::velocypack::Parser::fromJson("{}"),
    arangodb::aql::PART_MAIN
  );
  query.prepare(arangodb::QueryRegistryFeature::registry());
  arangodb::aql::Variable const outVariable("variable", 0);

  // missing 'viewId'
  {
    auto json = arangodb::velocypack::Parser::fromJson(
      "{ \"id\":42, \"depth\":0, \"totalNrRegs\":0, \"varInfoList\":[], \"nrRegs\":[], \"nrRegsHere\":[], \"regsToClear\":[], \"varsUsedLater\":[], \"varsValid\":[], \"outVariable\": { \"name\":\"variable\", \"id\":0 } }"
    );

    try {
      arangodb::iresearch::IResearchViewNode node(
        *query.plan(), // plan
        json->slice()
      );
      CHECK(false);
    } catch (arangodb::basics::Exception const& ex) {
      CHECK(TRI_ERROR_BAD_PARAMETER == ex.code());
    }
  }

  // invalid 'viewId' type (string expected)
  {
    auto json = arangodb::velocypack::Parser::fromJson(
      "{ \"id\":42, \"depth\":0, \"totalNrRegs\":0, \"varInfoList\":[], \"nrRegs\":[], \"nrRegsHere\":[], \"regsToClear\":[], \"varsUsedLater\":[], \"varsValid\":[], \"outVariable\": { \"name\":\"variable\", \"id\":0 }, \"viewId\":123 }"
    );

    try {
      arangodb::iresearch::IResearchViewNode node(
        *query.plan(), // plan
        json->slice()
      );
      CHECK(false);
    } catch (arangodb::basics::Exception const& ex) {
      CHECK(TRI_ERROR_BAD_PARAMETER == ex.code());
    }
  }

  // invalid 'viewId' specified
  {
    auto json = arangodb::velocypack::Parser::fromJson(
      "{ \"id\":42, \"depth\":0, \"totalNrRegs\":0, \"varInfoList\":[], \"nrRegs\":[], \"nrRegsHere\":[], \"regsToClear\":[], \"varsUsedLater\":[], \"varsValid\":[], \"outVariable\": { \"name\":\"variable\", \"id\":0 }, \"viewId\": \"foo\"}"
    );

    try {
      arangodb::iresearch::IResearchViewNode node(
        *query.plan(), // plan
        json->slice()
      );
      CHECK(false);
    } catch (arangodb::basics::Exception const& ex) {
      CHECK(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND == ex.code());
    }
  }

  // no options
  {
    auto json = arangodb::velocypack::Parser::fromJson(
      "{ \"id\":42, \"depth\":0, \"totalNrRegs\":0, \"varInfoList\":[], \"nrRegs\":[], \"nrRegsHere\":[], \"regsToClear\":[], \"varsUsedLater\":[], \"varsValid\":[], \"outVariable\": { \"name\":\"variable\", \"id\":0 }, \"viewId\": \"" + std::to_string(logicalView->id()) + "\" }"
    );

    arangodb::iresearch::IResearchViewNode node(
      *query.plan(), // plan
      json->slice()
    );

    CHECK(node.empty()); // view has no links
    CHECK(node.collections().empty()); // view has no links
    CHECK(node.shards().empty());

    CHECK(arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW == node.getType());
    CHECK(outVariable.id == node.outVariable().id);
    CHECK(outVariable.name == node.outVariable().name);
    CHECK(query.plan() == node.plan());
    CHECK(42 == node.id());
    CHECK(logicalView == node.view());
    CHECK(node.sortCondition().empty());
    CHECK(!node.volatility().first); // filter volatility
    CHECK(!node.volatility().second); // sort volatility
    CHECK(node.getVariablesUsedHere().empty());
    auto const setHere = node.getVariablesSetHere();
    CHECK(1 == setHere.size());
    CHECK(outVariable.id == setHere[0]->id);
    CHECK(outVariable.name == setHere[0]->name);
    CHECK(false == node.options().forceSync);

    CHECK(0. == node.getCost().estimatedCost); // no dependencies
    CHECK(0 == node.getCost().estimatedNrItems); // no dependencies
  }

  // with options
  {
    auto json = arangodb::velocypack::Parser::fromJson(
      "{ \"id\":42, \"depth\":0, \"totalNrRegs\":0, \"varInfoList\":[], \"nrRegs\":[], \"nrRegsHere\":[], \"regsToClear\":[], \"varsUsedLater\":[], \"varsValid\":[], \"outVariable\": { \"name\":\"variable\", \"id\":0 }, \"options\": { \"waitForSync\" : true }, \"viewId\": \"" + std::to_string(logicalView->id()) + "\" }"
    );

    arangodb::iresearch::IResearchViewNode node(
      *query.plan(), // plan
      json->slice()
    );

    CHECK(node.empty()); // view has no links
    CHECK(node.collections().empty()); // view has no links
    CHECK(node.shards().empty());

    CHECK(arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW == node.getType());
    CHECK(outVariable.id == node.outVariable().id);
    CHECK(outVariable.name == node.outVariable().name);
    CHECK(query.plan() == node.plan());
    CHECK(42 == node.id());
    CHECK(logicalView == node.view());
    CHECK(node.sortCondition().empty());
    CHECK(!node.volatility().first); // filter volatility
    CHECK(!node.volatility().second); // sort volatility
    CHECK(node.getVariablesUsedHere().empty());
    auto const setHere = node.getVariablesSetHere();
    CHECK(1 == setHere.size());
    CHECK(outVariable.id == setHere[0]->id);
    CHECK(outVariable.name == setHere[0]->name);
    CHECK(true == node.options().forceSync);

    CHECK(0. == node.getCost().estimatedCost); // no dependencies
    CHECK(0 == node.getCost().estimatedNrItems); // no dependencies
  }

  // with invalid options
  {
    auto json = arangodb::velocypack::Parser::fromJson(
      "{ \"id\":42, \"depth\":0, \"totalNrRegs\":0, \"varInfoList\":[], \"nrRegs\":[], \"nrRegsHere\":[], \"regsToClear\":[], \"varsUsedLater\":[], \"varsValid\":[], \"outVariable\": { \"name\":\"variable\", \"id\":0 }, \"options\": { \"waitForSync\" : \"false\"}, \"viewId\": \"" + std::to_string(logicalView->id()) + "\" }"
    );

    arangodb::iresearch::IResearchViewNode node(
      *query.plan(), // plan
      json->slice()
    );

    CHECK(node.empty()); // view has no links
    CHECK(node.collections().empty()); // view has no links
    CHECK(node.shards().empty());

    CHECK(arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW == node.getType());
    CHECK(outVariable.id == node.outVariable().id);
    CHECK(outVariable.name == node.outVariable().name);
    CHECK(query.plan() == node.plan());
    CHECK(42 == node.id());
    CHECK(logicalView == node.view());
    CHECK(node.sortCondition().empty());
    CHECK(!node.volatility().first); // filter volatility
    CHECK(!node.volatility().second); // sort volatility
    CHECK(node.getVariablesUsedHere().empty());
    auto const setHere = node.getVariablesSetHere();
    CHECK(1 == setHere.size());
    CHECK(outVariable.id == setHere[0]->id);
    CHECK(outVariable.name == setHere[0]->name);
    CHECK(false == node.options().forceSync);

    CHECK(0. == node.getCost().estimatedCost); // no dependencies
    CHECK(0 == node.getCost().estimatedNrItems); // no dependencies
  }
}

// FIXME TODO
//SECTION("constructFromVPackCluster") {
//}

SECTION("clone") {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  // create view
  auto createJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
  auto logicalView = vocbase.createView(createJson->slice());
  REQUIRE((false == !logicalView));

  // dummy query
  arangodb::aql::Query query(
    false, vocbase, arangodb::aql::QueryString("RETURN 1"),
    nullptr, arangodb::velocypack::Parser::fromJson("{}"),
    arangodb::aql::PART_MAIN
  );
  query.prepare(arangodb::QueryRegistryFeature::registry());
  arangodb::aql::Variable const outVariable("variable", 0);

  // no filter condition, no sort condition, no shards, no options
  {
    arangodb::iresearch::IResearchViewNode node(
      *query.plan(),
      42, // id
      vocbase, // database
      logicalView, // view
      outVariable,
      nullptr, // no filter condition
      nullptr, // no options
      {} // no sort condition
    );

    CHECK(node.empty()); // view has no links
    CHECK(node.collections().empty()); // view has no links
    CHECK(node.shards().empty());

    // clone without properties into the same plan
    {
      auto const nextId = node.plan()->nextId();
      auto& cloned = dynamic_cast<arangodb::iresearch::IResearchViewNode&>(
        *node.clone(query.plan(), true, false)
      );
      CHECK(node.getType() == cloned.getType());
      CHECK(&node.outVariable() == &cloned.outVariable()); // same objects
      CHECK(node.plan() == cloned.plan());
      CHECK(nextId + 1 == cloned.id());
      CHECK(&node.vocbase() == &cloned.vocbase());
      CHECK(node.view() == cloned.view());
      CHECK(&node.filterCondition() == &cloned.filterCondition());
      CHECK(node.sortCondition() == cloned.sortCondition());
      CHECK(node.volatility() == cloned.volatility());

      CHECK(node.getCost() == cloned.getCost());
    }

    // clone with properties into another plan
    {
      // another dummy query
      arangodb::aql::Query otherQuery(
        false, vocbase, arangodb::aql::QueryString("RETURN 1"),
        nullptr, arangodb::velocypack::Parser::fromJson("{}"),
        arangodb::aql::PART_MAIN
      );
      otherQuery.prepare(arangodb::QueryRegistryFeature::registry());

      auto& cloned = dynamic_cast<arangodb::iresearch::IResearchViewNode&>(
        *node.clone(otherQuery.plan(), true, true)
      );
      CHECK(node.getType() == cloned.getType());
      CHECK(&node.outVariable() != &cloned.outVariable()); // different objects
      CHECK(node.outVariable().id == cloned.outVariable().id);
      CHECK(node.outVariable().name == cloned.outVariable().name);
      CHECK(otherQuery.plan() == cloned.plan());
      CHECK(node.id() == cloned.id());
      CHECK(&node.vocbase() == &cloned.vocbase());
      CHECK(node.view() == cloned.view());
      CHECK(&node.filterCondition() == &cloned.filterCondition());
      CHECK(node.sortCondition() == cloned.sortCondition());
      CHECK(node.volatility() == cloned.volatility());
      CHECK(node.options().forceSync == cloned.options().forceSync);

      CHECK(node.getCost() == cloned.getCost());
    }

    // clone without properties into another plan
    {
      // another dummy query
      arangodb::aql::Query otherQuery(
        false, vocbase, arangodb::aql::QueryString("RETURN 1"),
        nullptr, arangodb::velocypack::Parser::fromJson("{}"),
        arangodb::aql::PART_MAIN
      );
      otherQuery.prepare(arangodb::QueryRegistryFeature::registry());

      node.plan()->nextId();
      auto& cloned = dynamic_cast<arangodb::iresearch::IResearchViewNode&>(
        *node.clone(otherQuery.plan(), true, false)
      );
      CHECK(node.getType() == cloned.getType());
      CHECK(&node.outVariable() == &cloned.outVariable()); // same objects
      CHECK(otherQuery.plan() == cloned.plan());
      CHECK(node.id() == cloned.id());
      CHECK(&node.vocbase() == &cloned.vocbase());
      CHECK(node.view() == cloned.view());
      CHECK(&node.filterCondition() == &cloned.filterCondition());
      CHECK(node.sortCondition() == cloned.sortCondition());
      CHECK(node.volatility() == cloned.volatility());
      CHECK(node.options().forceSync == cloned.options().forceSync);

      CHECK(node.getCost() == cloned.getCost());
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

    arangodb::iresearch::IResearchViewNode node(
      *query.plan(),
      42, // id
      vocbase, // database
      logicalView, // view
      outVariable,
      nullptr, // no filter condition
      &options,
      {} // no sort condition
    );

    CHECK(node.empty()); // view has no links
    CHECK(node.collections().empty()); // view has no links
    CHECK(node.shards().empty());
    CHECK(true == node.options().forceSync);

    // clone without properties into the same plan
    {
      auto const nextId = node.plan()->nextId();
      auto& cloned = dynamic_cast<arangodb::iresearch::IResearchViewNode&>(
        *node.clone(query.plan(), true, false)
      );
      CHECK(node.getType() == cloned.getType());
      CHECK(&node.outVariable() == &cloned.outVariable()); // same objects
      CHECK(node.plan() == cloned.plan());
      CHECK(nextId + 1 == cloned.id());
      CHECK(&node.vocbase() == &cloned.vocbase());
      CHECK(node.view() == cloned.view());
      CHECK(&node.filterCondition() == &cloned.filterCondition());
      CHECK(node.sortCondition() == cloned.sortCondition());
      CHECK(node.volatility() == cloned.volatility());

      CHECK(node.getCost() == cloned.getCost());
    }

    // clone with properties into another plan
    {
      // another dummy query
      arangodb::aql::Query otherQuery(
        false, vocbase, arangodb::aql::QueryString("RETURN 1"),
        nullptr, arangodb::velocypack::Parser::fromJson("{}"),
        arangodb::aql::PART_MAIN
      );
      otherQuery.prepare(arangodb::QueryRegistryFeature::registry());

      auto& cloned = dynamic_cast<arangodb::iresearch::IResearchViewNode&>(
        *node.clone(otherQuery.plan(), true, true)
      );
      CHECK(node.getType() == cloned.getType());
      CHECK(&node.outVariable() != &cloned.outVariable()); // different objects
      CHECK(node.outVariable().id == cloned.outVariable().id);
      CHECK(node.outVariable().name == cloned.outVariable().name);
      CHECK(otherQuery.plan() == cloned.plan());
      CHECK(node.id() == cloned.id());
      CHECK(&node.vocbase() == &cloned.vocbase());
      CHECK(node.view() == cloned.view());
      CHECK(&node.filterCondition() == &cloned.filterCondition());
      CHECK(node.sortCondition() == cloned.sortCondition());
      CHECK(node.volatility() == cloned.volatility());
      CHECK(node.options().forceSync == cloned.options().forceSync);

      CHECK(node.getCost() == cloned.getCost());
    }

    // clone without properties into another plan
    {
      // another dummy query
      arangodb::aql::Query otherQuery(
        false, vocbase, arangodb::aql::QueryString("RETURN 1"),
        nullptr, arangodb::velocypack::Parser::fromJson("{}"),
        arangodb::aql::PART_MAIN
      );
      otherQuery.prepare(arangodb::QueryRegistryFeature::registry());

      node.plan()->nextId();
      auto& cloned = dynamic_cast<arangodb::iresearch::IResearchViewNode&>(
        *node.clone(otherQuery.plan(), true, false)
      );
      CHECK(node.getType() == cloned.getType());
      CHECK(&node.outVariable() == &cloned.outVariable()); // same objects
      CHECK(otherQuery.plan() == cloned.plan());
      CHECK(node.id() == cloned.id());
      CHECK(&node.vocbase() == &cloned.vocbase());
      CHECK(node.view() == cloned.view());
      CHECK(&node.filterCondition() == &cloned.filterCondition());
      CHECK(node.sortCondition() == cloned.sortCondition());
      CHECK(node.volatility() == cloned.volatility());
      CHECK(node.options().forceSync == cloned.options().forceSync);

      CHECK(node.getCost() == cloned.getCost());
    }
  }

  // no filter condition, no sort condition, with shards, no options
  {
    arangodb::iresearch::IResearchViewNode node(
      *query.plan(),
      42, // id
      vocbase, // database
      logicalView, // view
      outVariable,
      nullptr, // no filter condition
      nullptr, // no options
      {} // no sort condition
    );

    CHECK(node.empty()); // view has no links
    CHECK(node.collections().empty()); // view has no links
    CHECK(node.shards().empty());

    node.shards().emplace_back("abc");
    node.shards().emplace_back("def");

    // clone without properties into the same plan
    {
      auto const nextId = node.plan()->nextId();
      auto& cloned = dynamic_cast<arangodb::iresearch::IResearchViewNode&>(
        *node.clone(query.plan(), true, false)
      );
      CHECK(cloned.collections().empty());
      CHECK(node.empty() == cloned.empty());
      CHECK(node.shards() == cloned.shards());
      CHECK(node.getType() == cloned.getType());
      CHECK(&node.outVariable() == &cloned.outVariable()); // same objects
      CHECK(node.plan() == cloned.plan());
      CHECK(nextId + 1 == cloned.id());
      CHECK(&node.vocbase() == &cloned.vocbase());
      CHECK(node.view() == cloned.view());
      CHECK(&node.filterCondition() == &cloned.filterCondition());
      CHECK(node.sortCondition() == cloned.sortCondition());
      CHECK(node.volatility() == cloned.volatility());
      CHECK(node.options().forceSync == cloned.options().forceSync);

      CHECK(node.getCost() == cloned.getCost());
    }

    // clone with properties into another plan
    {
      // another dummy query
      arangodb::aql::Query otherQuery(
        false, vocbase, arangodb::aql::QueryString("RETURN 1"),
        nullptr, arangodb::velocypack::Parser::fromJson("{}"),
        arangodb::aql::PART_MAIN
      );
      otherQuery.prepare(arangodb::QueryRegistryFeature::registry());

      auto& cloned = dynamic_cast<arangodb::iresearch::IResearchViewNode&>(
        *node.clone(otherQuery.plan(), true, true)
      );
      CHECK(cloned.collections().empty());
      CHECK(node.empty() == cloned.empty());
      CHECK(node.shards() == cloned.shards());
      CHECK(node.getType() == cloned.getType());
      CHECK(&node.outVariable() != &cloned.outVariable()); // different objects
      CHECK(node.outVariable().id == cloned.outVariable().id);
      CHECK(node.outVariable().name == cloned.outVariable().name);
      CHECK(otherQuery.plan() == cloned.plan());
      CHECK(node.id() == cloned.id());
      CHECK(&node.vocbase() == &cloned.vocbase());
      CHECK(node.view() == cloned.view());
      CHECK(&node.filterCondition() == &cloned.filterCondition());
      CHECK(node.sortCondition() == cloned.sortCondition());
      CHECK(node.volatility() == cloned.volatility());
      CHECK(node.options().forceSync == cloned.options().forceSync);

      CHECK(node.getCost() == cloned.getCost());
    }

    // clone without properties into another plan
    {
      // another dummy query
      arangodb::aql::Query otherQuery(
        false, vocbase, arangodb::aql::QueryString("RETURN 1"),
        nullptr, arangodb::velocypack::Parser::fromJson("{}"),
        arangodb::aql::PART_MAIN
      );
      otherQuery.prepare(arangodb::QueryRegistryFeature::registry());

      node.plan()->nextId();
      auto& cloned = dynamic_cast<arangodb::iresearch::IResearchViewNode&>(
        *node.clone(otherQuery.plan(), true, false)
      );
      CHECK(cloned.collections().empty());
      CHECK(node.empty() == cloned.empty());
      CHECK(node.shards() == cloned.shards());
      CHECK(node.getType() == cloned.getType());
      CHECK(&node.outVariable() == &cloned.outVariable()); // same objects
      CHECK(otherQuery.plan() == cloned.plan());
      CHECK(node.id() == cloned.id());
      CHECK(&node.vocbase() == &cloned.vocbase());
      CHECK(node.view() == cloned.view());
      CHECK(&node.filterCondition() == &cloned.filterCondition());
      CHECK(node.sortCondition() == cloned.sortCondition());
      CHECK(node.volatility() == cloned.volatility());
      CHECK(node.options().forceSync == cloned.options().forceSync);

      CHECK(node.getCost() == cloned.getCost());
    }
  }
}

SECTION("serialize") {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  // create view
  auto createJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
  auto logicalView = vocbase.createView(createJson->slice());
  REQUIRE((false == !logicalView));

  // dummy query
  arangodb::aql::Query query(
    false, vocbase, arangodb::aql::QueryString("RETURN 1"),
    nullptr, arangodb::velocypack::Parser::fromJson("{}"),
    arangodb::aql::PART_MAIN
  );
  query.prepare(arangodb::QueryRegistryFeature::registry());

  arangodb::aql::Variable const outVariable("variable", 0);

  // no filter condition, no sort condition
  {
    arangodb::iresearch::IResearchViewNode node(
      *query.plan(),
      42, // id
      vocbase, // database
      logicalView, // view
      outVariable,
      nullptr, // no filter condition
      nullptr, // no options
      {} // no sort condition
    );

    CHECK(node.empty()); // view has no links
    CHECK(node.collections().empty()); // view has no links
    CHECK(node.shards().empty());

    arangodb::velocypack::Builder builder;
    unsigned flags = arangodb::aql::ExecutionNode::SERIALIZE_DETAILS;
    node.toVelocyPack(builder, flags, false); // object with array of objects

    auto const slice = builder.slice();
    CHECK(slice.isObject());
    auto const nodesSlice = slice.get("nodes");
    CHECK(nodesSlice.isArray());
    arangodb::velocypack::ArrayIterator it(nodesSlice);
    CHECK(1 == it.size());
    auto nodeSlice = it.value();

    // constructor
    {
      arangodb::iresearch::IResearchViewNode const deserialized(
        *query.plan(), nodeSlice
      );
      CHECK(node.empty() == deserialized.empty());
      CHECK(node.shards() == deserialized.shards());
      CHECK(deserialized.collections().empty());
      CHECK(node.getType() == deserialized.getType());
      CHECK(node.outVariable().id == deserialized.outVariable().id);
      CHECK(node.outVariable().name == deserialized.outVariable().name);
      CHECK(node.plan() == deserialized.plan());
      CHECK(node.id() == deserialized.id());
      CHECK(&node.vocbase() == &deserialized.vocbase());
      CHECK(node.view() == deserialized.view());
      CHECK(&node.filterCondition() == &deserialized.filterCondition());
      CHECK(node.sortCondition() == deserialized.sortCondition());
      CHECK(node.volatility() == deserialized.volatility());
      CHECK(node.options().forceSync == deserialized.options().forceSync);

      CHECK(node.getCost() == deserialized.getCost());
    }

    // factory method
    {
      std::unique_ptr<arangodb::aql::ExecutionNode> deserializedNode(
        arangodb::aql::ExecutionNode::fromVPackFactory(query.plan(), nodeSlice)
      );
      auto& deserialized = dynamic_cast<arangodb::iresearch::IResearchViewNode&>(*deserializedNode);
      CHECK(node.empty() == deserialized.empty());
      CHECK(node.shards() == deserialized.shards());
      CHECK(deserialized.collections().empty());
      CHECK(node.getType() == deserialized.getType());
      CHECK(node.outVariable().id == deserialized.outVariable().id);
      CHECK(node.outVariable().name == deserialized.outVariable().name);
      CHECK(node.plan() == deserialized.plan());
      CHECK(node.id() == deserialized.id());
      CHECK(&node.vocbase() == &deserialized.vocbase());
      CHECK(node.view() == deserialized.view());
      CHECK(&node.filterCondition() == &deserialized.filterCondition());
      CHECK(node.sortCondition() == deserialized.sortCondition());
      CHECK(node.volatility() == deserialized.volatility());
      CHECK(node.options().forceSync == deserialized.options().forceSync);

      CHECK(node.getCost() == deserialized.getCost());
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

    arangodb::iresearch::IResearchViewNode node(
      *query.plan(),
      42, // id
      vocbase, // database
      logicalView, // view
      outVariable,
      nullptr, // no filter condition
      &options,
      {} // no sort condition
    );

    CHECK(node.empty()); // view has no links
    CHECK(node.collections().empty()); // view has no links
    CHECK(node.shards().empty());
    CHECK(true == node.options().forceSync);

    arangodb::velocypack::Builder builder;
    unsigned flags = arangodb::aql::ExecutionNode::SERIALIZE_DETAILS;
    node.toVelocyPack(builder, flags, false); // object with array of objects

    auto const slice = builder.slice();
    CHECK(slice.isObject());
    auto const nodesSlice = slice.get("nodes");
    CHECK(nodesSlice.isArray());
    arangodb::velocypack::ArrayIterator it(nodesSlice);
    CHECK(1 == it.size());
    auto nodeSlice = it.value();

    // constructor
    {
      arangodb::iresearch::IResearchViewNode const deserialized(
        *query.plan(), nodeSlice
      );
      CHECK(node.empty() == deserialized.empty());
      CHECK(node.shards() == deserialized.shards());
      CHECK(deserialized.collections().empty());
      CHECK(node.getType() == deserialized.getType());
      CHECK(node.outVariable().id == deserialized.outVariable().id);
      CHECK(node.outVariable().name == deserialized.outVariable().name);
      CHECK(node.plan() == deserialized.plan());
      CHECK(node.id() == deserialized.id());
      CHECK(&node.vocbase() == &deserialized.vocbase());
      CHECK(node.view() == deserialized.view());
      CHECK(&node.filterCondition() == &deserialized.filterCondition());
      CHECK(node.sortCondition() == deserialized.sortCondition());
      CHECK(node.volatility() == deserialized.volatility());
      CHECK(node.options().forceSync == deserialized.options().forceSync);

      CHECK(node.getCost() == deserialized.getCost());
    }

    // factory method
    {
      std::unique_ptr<arangodb::aql::ExecutionNode> deserializedNode(
        arangodb::aql::ExecutionNode::fromVPackFactory(query.plan(), nodeSlice)
      );
      auto& deserialized = dynamic_cast<arangodb::iresearch::IResearchViewNode&>(*deserializedNode);
      CHECK(node.empty() == deserialized.empty());
      CHECK(node.shards() == deserialized.shards());
      CHECK(deserialized.collections().empty());
      CHECK(node.getType() == deserialized.getType());
      CHECK(node.outVariable().id == deserialized.outVariable().id);
      CHECK(node.outVariable().name == deserialized.outVariable().name);
      CHECK(node.plan() == deserialized.plan());
      CHECK(node.id() == deserialized.id());
      CHECK(&node.vocbase() == &deserialized.vocbase());
      CHECK(node.view() == deserialized.view());
      CHECK(&node.filterCondition() == &deserialized.filterCondition());
      CHECK(node.sortCondition() == deserialized.sortCondition());
      CHECK(node.volatility() == deserialized.volatility());
      CHECK(node.options().forceSync == deserialized.options().forceSync);

      CHECK(node.getCost() == deserialized.getCost());
    }
  }
}

SECTION("collections") {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");

  std::shared_ptr<arangodb::LogicalCollection> collection0;
  std::shared_ptr<arangodb::LogicalCollection> collection1;

  // create collection0
  {
    auto createJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection0\", \"id\" : \"42\" }");
    collection0 = vocbase.createCollection(createJson->slice());
    REQUIRE((nullptr != collection0));
  }

  // create collection1
  {
    auto createJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection1\", \"id\" : \"4242\"  }");
    collection1 = vocbase.createCollection(createJson->slice());
    REQUIRE((nullptr != collection1));
  }

  // create collection2
  {
    auto createJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection2\" , \"id\" : \"424242\" }");
    REQUIRE(nullptr != vocbase.createCollection(createJson->slice()));
  }

  // create view
  auto createJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
  auto logicalView = vocbase.createView(createJson->slice());
  REQUIRE((false == !logicalView));

  // link collections
  auto updateJson = arangodb::velocypack::Parser::fromJson(
    "{ \"links\": {"
      "\"testCollection0\": { \"includeAllFields\": true, \"trackListPositions\": true },"
      "\"testCollection1\": { \"includeAllFields\": true },"
      "\"testCollection2\": { \"includeAllFields\": true }"
    "}}"
  );
  CHECK((logicalView->properties(updateJson->slice(), true).ok()));

  // dummy query
  arangodb::aql::Query query(
    false, vocbase, arangodb::aql::QueryString("RETURN 1"),
    nullptr, arangodb::velocypack::Parser::fromJson("{}"),
    arangodb::aql::PART_MAIN
  );

  // register collections with the query
  query.addCollection(std::to_string(collection0->id()), arangodb::AccessMode::Type::READ);
  query.addCollection(std::to_string(collection1->id()), arangodb::AccessMode::Type::READ);

  // prepare query
  query.prepare(arangodb::QueryRegistryFeature::registry());

  arangodb::aql::Variable const outVariable("variable", 0);

  // no filter condition, no sort condition
  arangodb::iresearch::IResearchViewNode node(
    *query.plan(),
    42, // id
    vocbase, // database
    logicalView, // view
    outVariable,
    nullptr, // no filter condition
    nullptr, // no options
    {} // no sort condition
  );

  CHECK(node.shards().empty());
  CHECK(!node.empty()); // view has no links
  auto collections = node.collections();
  CHECK(!collections.empty()); // view has no links
  CHECK(2 == collections.size());

  // we expect only collections 'collection0', 'collection1' to be
  // present since 'collection2' is not registered with the query
  std::unordered_set<std::string> expectedCollections {
    std::to_string(collection0->id()),
    std::to_string(collection1->id())
  };

  for (arangodb::aql::Collection const& collection : collections) {
    CHECK(1 == expectedCollections.erase(collection.name()));
  }
  CHECK(expectedCollections.empty());
}

SECTION("createBlockSingleServer") {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  auto createJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
  auto logicalView = vocbase.createView(createJson->slice());
  REQUIRE((false == !logicalView));

  // dummy query
  arangodb::aql::Query query(
    false, vocbase, arangodb::aql::QueryString("RETURN 1"),
    nullptr, arangodb::velocypack::Parser::fromJson("{}"),
    arangodb::aql::PART_MAIN
  );
  query.prepare(arangodb::QueryRegistryFeature::registry());

  // dummy engine
  arangodb::aql::ExecutionEngine engine(&query);

  arangodb::aql::Variable const outVariable("variable", 0);

  // prepare view snapshot

  // no filter condition, no sort condition
  {
    arangodb::iresearch::IResearchViewNode node(
      *query.plan(),
      42, // id
      vocbase, // database
      logicalView, // view
      outVariable,
      nullptr, // no filter condition
      nullptr, // no options
      {} // no sort condition
    );

    std::unordered_map<arangodb::aql::ExecutionNode*, arangodb::aql::ExecutionBlock*> EMPTY;

    // before transaction has started (no snapshot)
    try {
      auto block = node.createBlock(engine, EMPTY);
      CHECK(false);
    } catch (arangodb::basics::Exception const& e) {
      CHECK(TRI_ERROR_INTERNAL == e.code());
    }

    // start transaction (put snapshot into)
    REQUIRE(query.trx()->state());
    CHECK(nullptr == arangodb::LogicalView::cast<arangodb::iresearch::IResearchView>(*logicalView).snapshot(
      *query.trx(), arangodb::iresearch::IResearchView::Snapshot::Find
    ));
    auto* snapshot = arangodb::LogicalView::cast<arangodb::iresearch::IResearchView>(*logicalView).snapshot(
      *query.trx(), arangodb::iresearch::IResearchView::Snapshot::FindOrCreate
    );
    CHECK(snapshot == arangodb::LogicalView::cast<arangodb::iresearch::IResearchView>(*logicalView).snapshot(
      *query.trx(), arangodb::iresearch::IResearchView::Snapshot::Find
    ));
    CHECK(snapshot == arangodb::LogicalView::cast<arangodb::iresearch::IResearchView>(*logicalView).snapshot(
      *query.trx(), arangodb::iresearch::IResearchView::Snapshot::FindOrCreate
    ));
    CHECK(snapshot == arangodb::LogicalView::cast<arangodb::iresearch::IResearchView>(*logicalView).snapshot(
      *query.trx(), arangodb::iresearch::IResearchView::Snapshot::SyncAndReplace
    ));

    // after transaction has started
    {
      auto block = node.createBlock(engine, EMPTY);
      CHECK(nullptr != block);
      CHECK(nullptr != dynamic_cast<arangodb::iresearch::IResearchViewUnorderedBlock*>(block.get()));
    }
  }
}

// FIXME TODO
//SECTION("createBlockDBServer") {
//}

SECTION("createBlockCoordinator") {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  auto createJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
  auto logicalView = vocbase.createView(createJson->slice());
  REQUIRE((false == !logicalView));

  // dummy query
  arangodb::aql::Query query(
    false, vocbase, arangodb::aql::QueryString("RETURN 1"),
    nullptr, arangodb::velocypack::Parser::fromJson("{}"),
    arangodb::aql::PART_MAIN
  );
  query.prepare(arangodb::QueryRegistryFeature::registry());

  // dummy engine
  arangodb::aql::ExecutionEngine engine(&query);

  arangodb::aql::Variable const outVariable("variable", 0);

  // no filter condition, no sort condition
  arangodb::iresearch::IResearchViewNode node(
    *query.plan(),
    42, // id
    vocbase, // database
    logicalView, // view
    outVariable,
    nullptr, // no filter condition
    nullptr, // no options
    {} // no sort condition
  );

  std::unordered_map<arangodb::aql::ExecutionNode*, arangodb::aql::ExecutionBlock*> EMPTY;

  arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_COORDINATOR);
  auto emptyBlock = node.createBlock(engine, EMPTY);
  arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_SINGLE);
  CHECK(nullptr != emptyBlock);
  CHECK(nullptr != dynamic_cast<arangodb::aql::NoResultsBlock*>(emptyBlock.get()));
}

}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

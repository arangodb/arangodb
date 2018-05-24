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

#include "VocBase/LogicalView.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/OptimizerRulesFeature.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/ApplicationServerHelper.h"
#include "IResearch/IResearchFilterFactory.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchView.h"
#include "IResearch/IResearchViewNode.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/SystemDatabaseFeature.h"
#include "Logger/Logger.h"
#include "Logger/LogTopic.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/TraverserEngineRegistryFeature.h"
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

  IResearchViewNodeSetup(): server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

    arangodb::tests::init(true);

    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::WARN);

    // setup required application features
    features.emplace_back(new arangodb::ViewTypesFeature(&server), true);
    features.emplace_back(new arangodb::AuthenticationFeature(&server), true);
    features.emplace_back(new arangodb::DatabasePathFeature(&server), false);
    features.emplace_back(new arangodb::DatabaseFeature(&server), false);
    features.emplace_back(new arangodb::QueryRegistryFeature(&server), false); // must be first
    arangodb::application_features::ApplicationServer::server->addFeature(features.back().first);
    system = irs::memory::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 0, TRI_VOC_SYSTEM_DATABASE);
    features.emplace_back(new arangodb::TraverserEngineRegistryFeature(&server), false); // must be before AqlFeature
    features.emplace_back(new arangodb::AqlFeature(&server), true);
    features.emplace_back(new arangodb::aql::OptimizerRulesFeature(&server), true);
    features.emplace_back(new arangodb::aql::AqlFunctionFeature(&server), true); // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::iresearch::IResearchAnalyzerFeature(&server), true);
    features.emplace_back(new arangodb::iresearch::IResearchFeature(&server), true);
    features.emplace_back(new arangodb::iresearch::SystemDatabaseFeature(&server, system.get()), false); // required for IResearchAnalyzerFeature

    #if USE_ENTERPRISE
      features.emplace_back(new arangodb::LdapFeature(&server), false); // required for AuthenticationFeature with USE_ENTERPRISE
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

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(), arangodb::LogLevel::ERR); // suppress WARNING DefaultCustomTypeHandler called
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::FATAL);
    irs::logger::output_le(iresearch::logger::IRL_FATAL, stderr);
  }

  ~IResearchViewNodeSetup() {
    system.reset(); // destroy before reseting the 'ENGINE'
    arangodb::AqlFeature(&server).stop(); // unset singleton instance
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
  query.prepare(arangodb::QueryRegistryFeature::QUERY_REGISTRY, 42);
  arangodb::aql::Variable const outVariable("variable", 0);

  arangodb::iresearch::IResearchViewNode node(
    *query.plan(), // plan
    42, // id
    vocbase, // database
    *logicalView, // view
    outVariable, // out variable
    nullptr, // no filter condition
    {} // no sort condition
  );

  CHECK(arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW == node.getType());
  CHECK(&outVariable == &node.outVariable());
  CHECK(query.plan() == node.plan());
  CHECK(42 == node.id());
  CHECK(logicalView.get() == &node.view());
  CHECK(node.sortCondition().empty());
  CHECK(!node.volatile_filter());
  CHECK(!node.volatile_sort());
  CHECK(node.getVariablesUsedHere().empty());

  size_t nrItems{};
  CHECK(0. == node.estimateCost(nrItems));
  CHECK(0 == nrItems);
}

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
  query.prepare(arangodb::QueryRegistryFeature::QUERY_REGISTRY, 42);

  arangodb::aql::Variable const outVariable("variable", 0);

  // no filter condition, no sort condition
  {
    arangodb::iresearch::IResearchViewNode node(
      *query.plan(),
      42, // id
      vocbase, // database
      *logicalView, // view
      outVariable,
      nullptr, // no filter condition
      {} // no sort condition
    );

    // clone with properties into the same plan
    {
      auto const nextId = node.plan()->nextId();
      auto& cloned = dynamic_cast<arangodb::iresearch::IResearchViewNode&>(
        *node.clone(query.plan(), true, true)
      );
      CHECK(node.getType() == cloned.getType());
      CHECK(&node.outVariable() != &cloned.outVariable()); // different objects
      CHECK(node.outVariable().id == cloned.outVariable().id);
      CHECK(node.outVariable().name == cloned.outVariable().name);
      CHECK(node.plan() == cloned.plan());
      CHECK(nextId + 1 == cloned.id());
      CHECK(&node.vocbase() == &cloned.vocbase());
      CHECK(&node.view() == &cloned.view());
      CHECK(&node.filterCondition() == &cloned.filterCondition());
      CHECK(node.sortCondition() == cloned.sortCondition());
      CHECK(node.volatile_filter() == cloned.volatile_filter());
      CHECK(node.volatile_sort() == cloned.volatile_sort());

      size_t lhsNrItems{}, rhsNrItems{};
      CHECK(node.estimateCost(lhsNrItems) == cloned.estimateCost(rhsNrItems));
      CHECK(lhsNrItems == rhsNrItems);
    }

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
      CHECK(&node.view() == &cloned.view());
      CHECK(&node.filterCondition() == &cloned.filterCondition());
      CHECK(node.sortCondition() == cloned.sortCondition());
      CHECK(node.volatile_filter() == cloned.volatile_filter());
      CHECK(node.volatile_sort() == cloned.volatile_sort());

      size_t lhsNrItems{}, rhsNrItems{};
      CHECK(node.estimateCost(lhsNrItems) == cloned.estimateCost(rhsNrItems));
      CHECK(lhsNrItems == rhsNrItems);
    }

    // clone with properties into another plan
    {
      // another dummy query
      arangodb::aql::Query otherQuery(
        false, vocbase, arangodb::aql::QueryString("RETURN 1"),
        nullptr, arangodb::velocypack::Parser::fromJson("{}"),
        arangodb::aql::PART_MAIN
      );
      otherQuery.prepare(arangodb::QueryRegistryFeature::QUERY_REGISTRY, 42);

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
      CHECK(&node.view() == &cloned.view());
      CHECK(&node.filterCondition() == &cloned.filterCondition());
      CHECK(node.sortCondition() == cloned.sortCondition());
      CHECK(node.volatile_filter() == cloned.volatile_filter());
      CHECK(node.volatile_sort() == cloned.volatile_sort());

      size_t lhsNrItems{}, rhsNrItems{};
      CHECK(node.estimateCost(lhsNrItems) == cloned.estimateCost(rhsNrItems));
      CHECK(lhsNrItems == rhsNrItems);
    }

    // clone without properties into another plan
    {
      // another dummy query
      arangodb::aql::Query otherQuery(
        false, vocbase, arangodb::aql::QueryString("RETURN 1"),
        nullptr, arangodb::velocypack::Parser::fromJson("{}"),
        arangodb::aql::PART_MAIN
      );
      otherQuery.prepare(arangodb::QueryRegistryFeature::QUERY_REGISTRY, 42);

      auto const nextId = node.plan()->nextId();
      auto& cloned = dynamic_cast<arangodb::iresearch::IResearchViewNode&>(
        *node.clone(otherQuery.plan(), true, false)
      );
      CHECK(node.getType() == cloned.getType());
      CHECK(&node.outVariable() == &cloned.outVariable()); // same objects
      CHECK(otherQuery.plan() == cloned.plan());
      CHECK(node.id() == cloned.id());
      CHECK(&node.vocbase() == &cloned.vocbase());
      CHECK(&node.view() == &cloned.view());
      CHECK(&node.filterCondition() == &cloned.filterCondition());
      CHECK(node.sortCondition() == cloned.sortCondition());
      CHECK(node.volatile_filter() == cloned.volatile_filter());
      CHECK(node.volatile_sort() == cloned.volatile_sort());

      size_t lhsNrItems{}, rhsNrItems{};
      CHECK(node.estimateCost(lhsNrItems) == cloned.estimateCost(rhsNrItems));
      CHECK(lhsNrItems == rhsNrItems);
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
  query.prepare(arangodb::QueryRegistryFeature::QUERY_REGISTRY, 42);

  arangodb::aql::Variable const outVariable("variable", 0);

  // no filter condition, no sort condition
  {
    arangodb::iresearch::IResearchViewNode node(
      *query.plan(),
      42, // id
      vocbase, // database
      *logicalView, // view
      outVariable,
      nullptr, // no filter condition
      {} // no sort condition
    );

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
      CHECK(node.getType() == deserialized.getType());
      CHECK(node.outVariable().id == deserialized.outVariable().id);
      CHECK(node.outVariable().name == deserialized.outVariable().name);
      CHECK(node.plan() == deserialized.plan());
      CHECK(node.id() == deserialized.id());
      CHECK(&node.vocbase() == &deserialized.vocbase());
      CHECK(&node.view() == &deserialized.view());
      CHECK(&node.filterCondition() == &deserialized.filterCondition());
      CHECK(node.sortCondition() == deserialized.sortCondition());
      CHECK(node.volatile_filter() == deserialized.volatile_filter());
      CHECK(node.volatile_sort() == deserialized.volatile_sort());

      size_t lhsNrItems{}, rhsNrItems{};
      CHECK(node.estimateCost(lhsNrItems) == deserialized.estimateCost(rhsNrItems));
      CHECK(lhsNrItems == rhsNrItems);
    }

    // factory method
    {
      std::unique_ptr<arangodb::aql::ExecutionNode> deserializedNode(
        arangodb::aql::ExecutionNode::fromVPackFactory(query.plan(), nodeSlice)
      );
      auto& deserialized = dynamic_cast<arangodb::iresearch::IResearchViewNode&>(*deserializedNode);
      CHECK(node.getType() == deserialized.getType());
      CHECK(node.outVariable().id == deserialized.outVariable().id);
      CHECK(node.outVariable().name == deserialized.outVariable().name);
      CHECK(node.plan() == deserialized.plan());
      CHECK(node.id() == deserialized.id());
      CHECK(&node.vocbase() == &deserialized.vocbase());
      CHECK(&node.view() == &deserialized.view());
      CHECK(&node.filterCondition() == &deserialized.filterCondition());
      CHECK(node.sortCondition() == deserialized.sortCondition());
      CHECK(node.volatile_filter() == deserialized.volatile_filter());
      CHECK(node.volatile_sort() == deserialized.volatile_sort());

      size_t lhsNrItems{}, rhsNrItems{};
      CHECK(node.estimateCost(lhsNrItems) == deserialized.estimateCost(rhsNrItems));
      CHECK(lhsNrItems == rhsNrItems);
    }
  }
}

}

TEST_CASE("IResearchViewScatterNodeTest", "[iresearch][iresearch-view-node]") {
  IResearchViewNodeSetup s;
  UNUSED(s);
  
SECTION("construct") {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  // create view
  auto createJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
  auto logicalView = vocbase.createView(createJson->slice());
  REQUIRE((false == !logicalView));

  arangodb::aql::Query query(
    false, vocbase, arangodb::aql::QueryString("RETURN 1"),
    nullptr, arangodb::velocypack::Parser::fromJson("{}"),
    arangodb::aql::PART_MAIN
  );
  query.prepare(arangodb::QueryRegistryFeature::QUERY_REGISTRY, 42);
  arangodb::aql::Variable const outVariable("variable", 0);

  arangodb::iresearch::IResearchViewScatterNode node(
    *query.plan(), // plan
    42, // id
    vocbase, // database
    *logicalView // view
  );

  CHECK(arangodb::aql::ExecutionNode::SCATTER_IRESEARCH_VIEW == node.getType());
  CHECK(query.plan() == node.plan());
  CHECK(42 == node.id());
  CHECK(logicalView.get() == &node.view());

  size_t nrItems{};
  CHECK(0. == node.estimateCost(nrItems));
  CHECK(0 == nrItems);
}

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
  query.prepare(arangodb::QueryRegistryFeature::QUERY_REGISTRY, 42);

  arangodb::aql::Variable const outVariable("variable", 0);

  // no filter condition, no sort condition
  {
    arangodb::iresearch::IResearchViewScatterNode node(
      *query.plan(),
      42, // id
      vocbase, // database
      *logicalView // view
    );

    // clone with properties into the same plan
    {
      auto const nextId = node.plan()->nextId();
      auto& cloned = dynamic_cast<arangodb::iresearch::IResearchViewScatterNode&>(
        *node.clone(query.plan(), true, true)
      );
      CHECK(node.getType() == cloned.getType());
      CHECK(node.plan() == cloned.plan());
      CHECK(nextId + 1 == cloned.id());
      CHECK(&node.vocbase() == &cloned.vocbase());
      CHECK(&node.view() == &cloned.view());

      size_t lhsNrItems{}, rhsNrItems{};
      CHECK(node.estimateCost(lhsNrItems) == cloned.estimateCost(rhsNrItems));
      CHECK(lhsNrItems == rhsNrItems);
    }

    // clone without properties into the same plan
    {
      auto const nextId = node.plan()->nextId();
      auto& cloned = dynamic_cast<arangodb::iresearch::IResearchViewScatterNode&>(
        *node.clone(query.plan(), true, false)
      );
      CHECK(node.getType() == cloned.getType());
      CHECK(node.plan() == cloned.plan());
      CHECK(nextId + 1 == cloned.id());
      CHECK(&node.vocbase() == &cloned.vocbase());
      CHECK(&node.view() == &cloned.view());

      size_t lhsNrItems{}, rhsNrItems{};
      CHECK(node.estimateCost(lhsNrItems) == cloned.estimateCost(rhsNrItems));
      CHECK(lhsNrItems == rhsNrItems);
    }

    // clone with properties into another plan
    {
      // another dummy query
      arangodb::aql::Query otherQuery(
        false, vocbase, arangodb::aql::QueryString("RETURN 1"),
        nullptr, arangodb::velocypack::Parser::fromJson("{}"),
        arangodb::aql::PART_MAIN
      );
      otherQuery.prepare(arangodb::QueryRegistryFeature::QUERY_REGISTRY, 42);

      auto& cloned = dynamic_cast<arangodb::iresearch::IResearchViewScatterNode&>(
        *node.clone(otherQuery.plan(), true, true)
      );
      CHECK(node.getType() == cloned.getType());
      CHECK(otherQuery.plan() == cloned.plan());
      CHECK(node.id() == cloned.id());
      CHECK(&node.vocbase() == &cloned.vocbase());
      CHECK(&node.view() == &cloned.view());

      size_t lhsNrItems{}, rhsNrItems{};
      CHECK(node.estimateCost(lhsNrItems) == cloned.estimateCost(rhsNrItems));
      CHECK(lhsNrItems == rhsNrItems);
    }

    // clone without properties into another plan
    {
      // another dummy query
      arangodb::aql::Query otherQuery(
        false, vocbase, arangodb::aql::QueryString("RETURN 1"),
        nullptr, arangodb::velocypack::Parser::fromJson("{}"),
        arangodb::aql::PART_MAIN
      );
      otherQuery.prepare(arangodb::QueryRegistryFeature::QUERY_REGISTRY, 42);

      auto const nextId = node.plan()->nextId();
      auto& cloned = dynamic_cast<arangodb::iresearch::IResearchViewScatterNode&>(
        *node.clone(otherQuery.plan(), true, false)
      );
      CHECK(node.getType() == cloned.getType());
      CHECK(otherQuery.plan() == cloned.plan());
      CHECK(node.id() == cloned.id());
      CHECK(&node.vocbase() == &cloned.vocbase());
      CHECK(&node.view() == &cloned.view());

      size_t lhsNrItems{}, rhsNrItems{};
      CHECK(node.estimateCost(lhsNrItems) == cloned.estimateCost(rhsNrItems));
      CHECK(lhsNrItems == rhsNrItems);
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
  query.prepare(arangodb::QueryRegistryFeature::QUERY_REGISTRY, 42);

  arangodb::aql::Variable const outVariable("variable", 0);

  // no filter condition, no sort condition
  {
    arangodb::iresearch::IResearchViewScatterNode node(
      *query.plan(),
      42, // id
      vocbase, // database
      *logicalView // view
    );

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
      arangodb::iresearch::IResearchViewScatterNode const deserialized(
        *query.plan(), nodeSlice
      );
      CHECK(node.getType() == deserialized.getType());
      CHECK(node.plan() == deserialized.plan());
      CHECK(node.id() == deserialized.id());
      CHECK(&node.vocbase() == &deserialized.vocbase());
      CHECK(&node.view() == &deserialized.view());

      size_t lhsNrItems{}, rhsNrItems{};
      CHECK(node.estimateCost(lhsNrItems) == deserialized.estimateCost(rhsNrItems));
      CHECK(lhsNrItems == rhsNrItems);
    }

    // factory method
    {
      std::unique_ptr<arangodb::aql::ExecutionNode> deserializedNode(
        arangodb::aql::ExecutionNode::fromVPackFactory(query.plan(), nodeSlice)
      );
      auto& deserialized = dynamic_cast<arangodb::iresearch::IResearchViewScatterNode&>(*deserializedNode);
      CHECK(node.getType() == deserialized.getType());
      CHECK(node.plan() == deserialized.plan());
      CHECK(node.id() == deserialized.id());
      CHECK(&node.vocbase() == &deserialized.vocbase());
      CHECK(&node.view() == &deserialized.view());

      size_t lhsNrItems{}, rhsNrItems{};
      CHECK(node.estimateCost(lhsNrItems) == deserialized.estimateCost(rhsNrItems));
      CHECK(lhsNrItems == rhsNrItems);
    }
  }
}

}

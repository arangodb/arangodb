////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////
#ifndef TESTS_GRAPH_TOOLS_H
#define TESTS_GRAPH_TOOLS_H

#include "gtest/gtest.h"

#include "IResearch/common.h"
#include "Mocks/LogLevels.h"
#include "Mocks/Servers.h"
#include "Mocks/StorageEngineMock.h"

#include "./MockGraph.h"

#include "Aql/AqlFunctionFeature.h"
#include "Aql/AqlItemBlockSerializationFormat.h"
#include "Aql/Ast.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Aql/Query.h"
#include "ClusterEngine/ClusterEngine.h"
#include "Graph/ConstantWeightShortestPathFinder.h"
#include "Graph/ShortestPathOptions.h"
#include "Graph/ShortestPathResult.h"
#include "Random/RandomGenerator.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/MetricsFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"

#include <optional>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::graph;
using namespace arangodb::velocypack;

namespace arangodb {
namespace tests {
namespace graph {

struct GraphTestSetup
    : public arangodb::tests::LogSuppressor<arangodb::Logger::FIXME, arangodb::LogLevel::ERR> {
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::unique_ptr<TRI_vocbase_t> system;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature&, bool>> features;

  GraphTestSetup() : engine(server), server(nullptr, nullptr) {
    arangodb::transaction::Methods::clearDataSourceRegistrationCallbacks();
    arangodb::ClusterEngine::Mocking = true;
    arangodb::RandomGenerator::initialize(arangodb::RandomGenerator::RandomType::MERSENNE);

    // setup required application features
    features.emplace_back(server.addFeature<arangodb::MetricsFeature>(), false);
    features.emplace_back(server.addFeature<arangodb::DatabasePathFeature>(), false);
    features.emplace_back(server.addFeature<arangodb::DatabaseFeature>(), false);
    features.emplace_back(server.addFeature<arangodb::EngineSelectorFeature>(), false);
    server.getFeature<EngineSelectorFeature>().setEngineTesting(&engine);
    features.emplace_back(server.addFeature<arangodb::QueryRegistryFeature>(), false);  // must be first
    system = std::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                                             systemDBInfo(server));
    features.emplace_back(server.addFeature<arangodb::SystemDatabaseFeature>(
                              system.get()),
                          false);  // required for IResearchAnalyzerFeature
    features.emplace_back(server.addFeature<arangodb::AqlFeature>(), true);
    features.emplace_back(server.addFeature<arangodb::aql::OptimizerRulesFeature>(), true);
    features.emplace_back(server.addFeature<arangodb::aql::AqlFunctionFeature>(),
                          true);  // required for IResearchAnalyzerFeature

    for (auto& f : features) {
      f.first.prepare();
    }

    for (auto& f : features) {
      if (f.second) {
        f.first.start();
      }
    }

    auto& dbPathFeature = server.getFeature<arangodb::DatabasePathFeature>();
    arangodb::tests::setDatabasePath(dbPathFeature);  // ensure test data is stored in a unique directory
  }

  ~GraphTestSetup() {
    system.reset();  // destroy before reseting the 'ENGINE'
    arangodb::AqlFeature(server).stop();  // unset singleton instance
    server.getFeature<EngineSelectorFeature>().setEngineTesting(nullptr);

    // destroy application features
    for (auto& f : features) {
      if (f.second) {
        f.first.stop();
      }
    }

    for (auto& f : features) {
      f.first.unprepare();
    }
  }
};  // Setup

struct MockGraphDatabase {
  TRI_vocbase_t vocbase;

  MockGraphDatabase(application_features::ApplicationServer& server, std::string name)
      : vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, createInfo(server, name, 1)) {}

  ~MockGraphDatabase() {}

  // Create a collection with name <name> and <n> vertices
  // with ids 0..n-1
  void addVertexCollection(std::string name, size_t n) {
    auto vertices = generateVertexCollection(name);

    std::vector<std::shared_ptr<arangodb::velocypack::Builder>> docs;
    for (size_t i = 0; i < n; i++) {
      docs.emplace_back(arangodb::velocypack::Parser::fromJson(
          "{ \"_key\": \"" + std::to_string(i) + "\"}"));
    };

    arangodb::OperationOptions options;
    options.returnNew = true;
    arangodb::SingleCollectionTransaction trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                              *vertices, arangodb::AccessMode::Type::WRITE);
    EXPECT_TRUE((trx.begin().ok()));

    std::vector<velocypack::Builder> insertedDocs;
    for (auto& entry : docs) {
      auto res = trx.insert(vertices->name(), entry->slice(), options);
      EXPECT_TRUE((res.ok()));
      insertedDocs.emplace_back(res.slice().get("new"));
    }

    EXPECT_TRUE((trx.commit().ok()));
    EXPECT_TRUE(insertedDocs.size() == n);
    EXPECT_TRUE(vertices->type());
  }

  struct EdgeDef {
    EdgeDef(size_t from, size_t to) : _from(from), _to(to){};
    EdgeDef(size_t from, size_t to, double weight)
        : _from(from), _to(to), _weight(weight){};
    size_t _from;
    size_t _to;
    std::optional<double> _weight;
  };

  // Create a collection with name <name> of edges given by <edges>
  void addEdgeCollection(std::string name, std::string vertexCollection,
                         std::vector<EdgeDef> edgedef) {
    auto edges = generateEdgeCollection(name);

    std::vector<std::shared_ptr<arangodb::velocypack::Builder>> docs;

    for (auto& p : edgedef) {
      //      std::cout << "edge: " << vertexCollection << " " << p.first << " -> "
      //          << p.second << std::endl;
      // This is moderately horrible
      auto docJson =
          p._weight.has_value()
              ? velocypack::Parser::fromJson(
                    "{ \"_from\": \"" + vertexCollection + "/" +
                    std::to_string(p._from) + "\"" + ", \"_to\": \"" +
                    vertexCollection + "/" + std::to_string(p._to) +
                    "\", \"cost\": " + std::to_string(p._weight.value()) + "}")
              : velocypack::Parser::fromJson(
                    "{ \"_from\": \"" + vertexCollection + "/" +
                    std::to_string(p._from) + "\"" + ", \"_to\": \"" +
                    vertexCollection + "/" + std::to_string(p._to) + "\" }");
      docs.emplace_back(docJson);
    }

    arangodb::OperationOptions options;
    options.returnNew = true;
    arangodb::SingleCollectionTransaction trx(arangodb::transaction::StandaloneContext::Create(vocbase),

                                              *edges, arangodb::AccessMode::Type::WRITE);
    EXPECT_TRUE((trx.begin().ok()));

    std::vector<velocypack::Builder> insertedDocs;
    for (auto& entry : docs) {
      auto res = trx.insert(edges->name(), entry->slice(), options);
      EXPECT_TRUE((res.ok()));
      insertedDocs.emplace_back(res.slice().get("new"));
    }

    EXPECT_TRUE((trx.commit().ok()));
    EXPECT_TRUE(insertedDocs.size() == edgedef.size());
  }

  void addGraph(MockGraph const& graph) {
    auto vertices = generateVertexCollection("v");
    auto edges = generateEdgeCollection("e");
    graph.storeData(vocbase, "v", "e");
  }

  auto generateEdgeCollection(std::string name)
      -> std::shared_ptr<arangodb::LogicalCollection> {
    auto createJson = velocypack::Parser::fromJson("{ \"name\": \"" + name +
                                                   "\", \"type\": 3 }");
    std::shared_ptr<arangodb::LogicalCollection> edges =
        vocbase.createCollection(createJson->slice());
    TRI_ASSERT(nullptr != edges);

    auto indexJson = velocypack::Parser::fromJson("{ \"type\": \"edge\" }");
    bool created = false;
    auto index = edges->createIndex(indexJson->slice(), created);
    TRI_ASSERT(index);
    TRI_ASSERT(created);
    return edges;
  }

  auto generateVertexCollection(std::string name)
      -> std::shared_ptr<arangodb::LogicalCollection> {
    auto createJson = velocypack::Parser::fromJson("{ \"name\": \"" + name +
                                                   "\", \"type\": 2 }");
    std::shared_ptr<arangodb::LogicalCollection> vertices =
        vocbase.createCollection(createJson->slice());
    TRI_ASSERT(nullptr != vertices);
    return vertices;
  }

  std::shared_ptr<Index> getEdgeIndexHandle(std::string name) {
    std::shared_ptr<arangodb::LogicalCollection> coll = vocbase.lookupCollection(name);
    TRI_ASSERT(coll != nullptr);    // no edge collection of this name
    TRI_ASSERT(coll->type() == 3);  // Is not an edge collection
    for (auto const& idx : coll->getIndexes()) {
      if (idx->type() == Index::TRI_IDX_TYPE_EDGE_INDEX) {
        return idx;
      }
    }
    TRI_ASSERT(false);  // Index not found
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  std::unique_ptr<arangodb::aql::Query> getQuery(std::string qry,
                                                 std::vector<std::string> collections) {
    auto queryString = arangodb::aql::QueryString(qry);

    auto ctx = std::make_shared<arangodb::transaction::StandaloneContext>(vocbase);
    auto query = std::make_unique<arangodb::aql::Query>(ctx, queryString, nullptr);
    for (auto const& c : collections) {
      query->collections().add(c, AccessMode::Type::READ,
                               arangodb::aql::Collection::Hint::Collection);
    }
    query->prepareQuery(SerializationFormat::SHADOWROWS);

    return query;
  }

  std::unique_ptr<arangodb::graph::ShortestPathOptions> getShortestPathOptions(
      arangodb::aql::Query* query) {
    auto plan = const_cast<arangodb::aql::ExecutionPlan*>(query->plan());
    auto ast = plan->getAst();

    auto _toCondition = ast->createNodeNaryOperator(NODE_TYPE_OPERATOR_NARY_AND);

    auto tmpVar = generateTempVar(query);

    auto _fromCondition = buildOutboundCondition(query, tmpVar);

    AstNode* tmpId1 = plan->getAst()->createNodeReference(tmpVar);
    AstNode* tmpId2 = plan->getAst()->createNodeValueString("", 0);

    {
      auto const* access =
          ast->createNodeAttributeAccess(tmpId1, StaticStrings::ToString.c_str(),
                                         StaticStrings::ToString.length());
      auto const* cond =
          ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_EQ, access, tmpId2);
      _toCondition->addMember(cond);
    }

    auto spo = std::make_unique<ShortestPathOptions>(*query);
    spo->setVariable(tmpVar);
    spo->addLookupInfo(plan, "e", StaticStrings::FromString, _fromCondition->clone(ast));
    spo->addReverseLookupInfo(plan, "e", StaticStrings::ToString, _toCondition->clone(ast));

    return spo;
  }

  arangodb::aql::AstNode* buildOutboundCondition(arangodb::aql::Query* query,
                                                 arangodb::aql::Variable const* tmpVar) {
    auto plan = const_cast<arangodb::aql::ExecutionPlan*>(query->plan());
    auto ast = plan->getAst();
    auto fromCondition = ast->createNodeNaryOperator(NODE_TYPE_OPERATOR_NARY_AND);
    AstNode* tmpId1 = plan->getAst()->createNodeReference(tmpVar);
    AstNode* tmpId2 = plan->getAst()->createNodeValueMutableString("", 0);

    auto const* access =
        ast->createNodeAttributeAccess(tmpId1, StaticStrings::FromString.c_str(),
                                       StaticStrings::FromString.length());
    auto const* cond =
        ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_EQ, access, tmpId2);
    fromCondition->addMember(cond);
    return fromCondition;
  }

  arangodb::aql::Variable* generateTempVar(arangodb::aql::Query* query) {
    auto plan = const_cast<arangodb::aql::ExecutionPlan*>(query->plan());
    return plan->getAst()->variables()->createTemporaryVariable();
  }

};  // namespace graph

bool checkPath(ShortestPathOptions* spo, ShortestPathResult result,
               std::vector<std::string> vertices,
               std::vector<std::pair<std::string, std::string>> edges, std::string& msgs);

}  // namespace graph
}  // namespace tests
}  // namespace arangodb
#endif

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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

#include "Aql/AqlFunctionFeature.h"
#include "Aql/Ast.h"
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
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/TraverserEngineRegistryFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"

#include "../Mocks/Servers.h"
#include "../Mocks/StorageEngineMock.h"
#include "IResearch/common.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::graph;
using namespace arangodb::velocypack;

namespace arangodb {
namespace tests {
namespace graph {

struct GraphTestSetup {
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::unique_ptr<TRI_vocbase_t> system;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  GraphTestSetup() : engine(server), server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;
    arangodb::transaction::Methods::clearDataSourceRegistrationCallbacks();
    arangodb::ClusterEngine::Mocking = true;
    arangodb::RandomGenerator::initialize(arangodb::RandomGenerator::RandomType::MERSENNE);

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(), arangodb::LogLevel::ERR);  // suppress WARNING DefaultCustomTypeHandler called

    // setup required application features
    features.emplace_back(new arangodb::DatabasePathFeature(server), false);
    features.emplace_back(new arangodb::DatabaseFeature(server), false);
    features.emplace_back(new arangodb::QueryRegistryFeature(server), false);  // must be first
    arangodb::application_features::ApplicationServer::server->addFeature(
        features.back().first);  // need QueryRegistryFeature feature to be added now in order to create the system database
    auto builder = dbArgsBuilder();
    system = std::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                                             0, builder.slice());
    features.emplace_back(new arangodb::SystemDatabaseFeature(server, system.get()),
                          false);  // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::TraverserEngineRegistryFeature(server), false);  // must be before AqlFeature
    features.emplace_back(new arangodb::AqlFeature(server), true);
    features.emplace_back(new arangodb::aql::OptimizerRulesFeature(server), true);
    features.emplace_back(new arangodb::aql::AqlFunctionFeature(server), true);  // required for IResearchAnalyzerFeature

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

  ~GraphTestSetup() {
    system.reset();  // destroy before reseting the 'ENGINE'
    arangodb::AqlFeature(server).stop();  // unset singleton instance
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
  }
};  // Setup

struct MockGraphDatabase {
  TRI_vocbase_t vocbase;
  std::vector<arangodb::aql::Query*> queries;
  std::vector<arangodb::graph::ShortestPathOptions*> spos;

  MockGraphDatabase(std::string name) :
    vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, dbArgsBuilder(name).slice())
  {}

  ~MockGraphDatabase() {
    for (auto& q : queries) {
      if (q->trx() != nullptr) {
        q->trx()->abort();
      }
      delete q;
    }
    for (auto& o : spos) {
      delete o;
    }
  }

  // Create a collection with name <name> and <n> vertices
  // with ids 0..n-1
  void addVertexCollection(std::string name, size_t n) {
    std::shared_ptr<arangodb::LogicalCollection> vertices;

    auto createJson = velocypack::Parser::fromJson("{ \"name\": \"" + name +
                                                   "\", \"type\": 2 }");
    vertices = vocbase.createCollection(createJson->slice());
    ASSERT_TRUE((nullptr != vertices));

    std::vector<velocypack::Builder> insertedDocs;
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

    for (auto& entry : docs) {
      auto res = trx.insert(vertices->name(), entry->slice(), options);
      EXPECT_TRUE((res.ok()));
      insertedDocs.emplace_back(res.slice().get("new"));
    }

    EXPECT_TRUE((trx.commit().ok()));
    EXPECT_TRUE(insertedDocs.size() == n);
    EXPECT_TRUE(vertices->type());
  }

  // Create a collection with name <name> of edges given by <edges>
  void addEdgeCollection(std::string name, std::string vertexCollection,
                         std::vector<std::pair<size_t, size_t>> edgedef) {
    std::shared_ptr<arangodb::LogicalCollection> edges;
    auto createJson = velocypack::Parser::fromJson("{ \"name\": \"" + name +
                                                   "\", \"type\": 3 }");
    edges = vocbase.createCollection(createJson->slice());
    ASSERT_TRUE((nullptr != edges));

    auto indexJson = velocypack::Parser::fromJson("{ \"type\": \"edge\" }");
    bool created = false;
    auto index = edges->createIndex(indexJson->slice(), created);
    EXPECT_TRUE(index);
    EXPECT_TRUE(created);

    std::vector<velocypack::Builder> insertedDocs;
    std::vector<std::shared_ptr<arangodb::velocypack::Builder>> docs;

    for (auto& p : edgedef) {
      //      std::cout << "edge: " << vertexCollection << " " << p.first << " -> "
      //          << p.second << std::endl;
      auto docJson = velocypack::Parser::fromJson(
          "{ \"_from\": \"" + vertexCollection + "/" + std::to_string(p.first) +
          "\"" + ", \"_to\": \"" + vertexCollection + "/" +
          std::to_string(p.second) + "\" }");
      docs.emplace_back(docJson);
    }

    arangodb::OperationOptions options;
    options.returnNew = true;
    arangodb::SingleCollectionTransaction trx(arangodb::transaction::StandaloneContext::Create(vocbase),

                                              *edges, arangodb::AccessMode::Type::WRITE);
    EXPECT_TRUE((trx.begin().ok()));

    for (auto& entry : docs) {
      auto res = trx.insert(edges->name(), entry->slice(), options);
      EXPECT_TRUE((res.ok()));
      insertedDocs.emplace_back(res.slice().get("new"));
    }

    EXPECT_TRUE((trx.commit().ok()));
    EXPECT_TRUE(insertedDocs.size() == edgedef.size());
  }

  arangodb::aql::Query* getQuery(std::string qry) {
    auto queryString = arangodb::aql::QueryString(qry);

    arangodb::aql::Query* query =
        new arangodb::aql::Query(false, vocbase, queryString, nullptr,
                                 arangodb::velocypack::Parser::fromJson("{}"),
                                 arangodb::aql::PART_MAIN);
    query->parse();
    query->prepare(arangodb::QueryRegistryFeature::registry());

    queries.emplace_back(query);

    return query;
  }

  arangodb::graph::ShortestPathOptions* getShortestPathOptions(arangodb::aql::Query* query) {
    arangodb::graph::ShortestPathOptions* spo;

    auto plan = query->plan();
    auto ast = plan->getAst();

    auto _toCondition = ast->createNodeNaryOperator(NODE_TYPE_OPERATOR_NARY_AND);
    auto _fromCondition = ast->createNodeNaryOperator(NODE_TYPE_OPERATOR_NARY_AND);

    auto tmpVar = plan->getAst()->variables()->createTemporaryVariable();

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

    {
      auto const* access =
          ast->createNodeAttributeAccess(tmpId1, StaticStrings::FromString.c_str(),
                                         StaticStrings::FromString.length());
      auto const* cond =
          ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_EQ, access, tmpId2);
      _fromCondition->addMember(cond);
    }
    spo = new ShortestPathOptions(query);
    spo->setVariable(tmpVar);
    spo->addLookupInfo(plan, "e", StaticStrings::FromString, _fromCondition->clone(ast));
    spo->addReverseLookupInfo(plan, "e", StaticStrings::ToString, _toCondition->clone(ast));

    spos.emplace_back(spo);
    return spo;
  }
};

bool checkPath(ShortestPathOptions* spo, ShortestPathResult result, std::vector<std::string> vertices,
               std::vector<std::pair<std::string, std::string>> edges, std::string& msgs);

}
}
}
#endif

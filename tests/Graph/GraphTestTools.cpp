////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

// test setup
#include "RestServer/arangod.h"
#include "../Mocks/Servers.h"
#include "../Mocks/StorageEngineMock.h"

#include "Aql/OptimizerRulesFeature.h"
#include "Cluster/ClusterFeature.h"
#include "ClusterEngine/ClusterEngine.h"
#include "IResearch/common.h"
#include "Metrics/ClusterMetricsFeature.h"
#include "Metrics/MetricsFeature.h"
#include "Random/RandomGenerator.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Statistics/StatisticsFeature.h"
#include "StorageEngine/PhysicalCollection.h"
#include "Transaction/ManagerFeature.h"
#include "GraphTestTools.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::graph;
using namespace arangodb::velocypack;

namespace arangodb {
namespace tests {
namespace graph {

GraphTestSetup::GraphTestSetup() : server(nullptr, nullptr), engine(server) {
  arangodb::transaction::Methods::clearDataSourceRegistrationCallbacks();
  arangodb::ClusterEngine::Mocking = true;
  arangodb::RandomGenerator::initialize(
      arangodb::RandomGenerator::RandomType::MERSENNE);

  // setup required application features
  features.emplace_back(
      server.addFeature<arangodb::metrics::MetricsFeature>(
          LazyApplicationFeatureReference<QueryRegistryFeature>(server),
          LazyApplicationFeatureReference<StatisticsFeature>(nullptr),
          LazyApplicationFeatureReference<EngineSelectorFeature>(server),
          LazyApplicationFeatureReference<metrics::ClusterMetricsFeature>(
              nullptr),
          LazyApplicationFeatureReference<ClusterFeature>(nullptr)),
      false);
  features.emplace_back(server.addFeature<arangodb::DatabasePathFeature>(),
                        false);
  features.emplace_back(
      server.addFeature<arangodb::transaction::ManagerFeature>(), false);
  features.emplace_back(server.addFeature<arangodb::DatabaseFeature>(), false);
  features.emplace_back(server.addFeature<arangodb::EngineSelectorFeature>(),
                        false);
  server.getFeature<EngineSelectorFeature>().setEngineTesting(&engine);
  features.emplace_back(
      server.addFeature<arangodb::QueryRegistryFeature>(
          server.template getFeature<arangodb::metrics::MetricsFeature>()),
      false);  // must be first
  system = std::make_unique<TRI_vocbase_t>(
      systemDBInfo(server),
      server.getFeature<DatabaseFeature>().versionTracker(), true);
  features.emplace_back(
      server.addFeature<arangodb::SystemDatabaseFeature>(system.get()),
      false);  // required for IResearchAnalyzerFeature
  features.emplace_back(server.addFeature<arangodb::AqlFeature>(), true);
  features.emplace_back(
      server.addFeature<arangodb::aql::OptimizerRulesFeature>(), true);
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
  arangodb::tests::setDatabasePath(
      dbPathFeature);  // ensure test data is stored in a unique directory
}

GraphTestSetup::~GraphTestSetup() {
  system.reset();                       // destroy before reseting the 'ENGINE'
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

std::shared_ptr<Index> MockIndexHelpers::getEdgeIndexHandle(
    TRI_vocbase_t& vocbase, std::string const& edgeCollectionName) {
  std::shared_ptr<arangodb::LogicalCollection> coll =
      vocbase.lookupCollection(edgeCollectionName);
  TRI_ASSERT(coll != nullptr);    // no edge collection of this name
  TRI_ASSERT(coll->type() == 3);  // Is not an edge collection
  for (auto const& idx : coll->getPhysical()->getAllIndexes()) {
    if (idx->type() == Index::TRI_IDX_TYPE_EDGE_INDEX) {
      return idx;
    }
  }
  TRI_ASSERT(false);  // Index not found
  THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
}

arangodb::aql::AstNode* MockIndexHelpers::buildCondition(
    aql::Query& query, aql::Variable const* tmpVar,
    TRI_edge_direction_e direction) {
  TRI_ASSERT(direction == TRI_EDGE_OUT || direction == TRI_EDGE_IN);
  auto plan = const_cast<arangodb::aql::ExecutionPlan*>(query.plan());
  auto ast = plan->getAst();
  auto condition = ast->createNodeNaryOperator(NODE_TYPE_OPERATOR_NARY_AND);
  AstNode* tmpId1 = ast->createNodeReference(tmpVar);
  AstNode* tmpId2 = ast->createNodeValueMutableString("", 0);

  auto const* access = ast->createNodeAttributeAccess(
      tmpId1, direction == TRI_EDGE_OUT ? StaticStrings::FromString
                                        : StaticStrings::ToString);
  auto const* cond = ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_EQ,
                                                   access, tmpId2);
  condition->addMember(cond);
  return condition;
}

}  // namespace graph
}  // namespace tests
}  // namespace arangodb

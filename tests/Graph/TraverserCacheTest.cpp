////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "fakeit.hpp"

#include "Aql/AqlValue.h"
#include "Aql/Query.h"
#include "Aql/QueryWarnings.h"
#include "Cluster/ServerState.h"
#include "Graph/Cache/RefactoredTraverserCache.h"
//#include "Graph/ClusterTraverserCache.h"
#include "Graph/GraphTestTools.h"
#include "Graph/TraverserOptions.h"
#include "Transaction/Methods.h"

#include <Aql/TraversalStats.h>
#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::graph;

namespace arangodb {
namespace tests {
namespace traverser_cache_test {

class TraverserCacheTest : public ::testing::Test {
 protected:
  ServerState::RoleEnum oldRole;

  graph::GraphTestSetup s{};
  graph::MockGraphDatabase gdb;
  arangodb::aql::TraversalStats stats{};
  std::unique_ptr<arangodb::aql::Query> query{nullptr};
  std::unique_ptr<RefactoredTraverserCache> traverserCache{nullptr};
  std::shared_ptr<transaction::Context> queryContext{nullptr};
  std::unique_ptr<arangodb::transaction::Methods> trx{nullptr};
  arangodb::ResourceMonitor* _monitor;

  TraverserCacheTest() : gdb(s.server, "testVocbase") {
    query = gdb.getQuery("RETURN 1", std::vector<std::string>{});
    queryContext = query.get()->newTrxContext();
    trx = std::make_unique<arangodb::transaction::Methods>(queryContext);
    _monitor = &query->resourceMonitor();
    traverserCache =
        std::make_unique<RefactoredTraverserCache>(trx.get(), query.get(),
                                                   query->resourceMonitor(), stats);
  }

  ~TraverserCacheTest() = default;
};

TEST_F(TraverserCacheTest, it_should_return_a_null_aqlvalue_if_vertex_is_not_available) {
  // prepare graph data - in this case, no data (no vertices and no edges, but collections v and e)
  graph::MockGraph graph{};
  gdb.addGraph(graph);

  std::string vertexId = "v/Vertex";
  std::string expectedMessage = "vertex '" + vertexId + "' not found";

  auto data = VPackParser::fromJson(R"({"_key":"Vertex", "_id": "v/Vertex"})");
  VPackSlice doc = data->slice();
  HashedStringRef id{doc.get("_id")};
  VPackBuilder builder;

  // NOTE: we do not have the data, so we get null for any vertex
  traverserCache->insertVertexIntoResult(arangodb::velocypack::HashedStringRef(id), builder);
  ASSERT_TRUE(builder.slice().isNull());
  auto all = query->warnings().all();
  ASSERT_EQ(all.size(), 1);
  ASSERT_TRUE(all[0].first == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
  ASSERT_TRUE(all[0].second == expectedMessage);
}

TEST_F(TraverserCacheTest, it_should_return_a_null_aqlvalue_if_edge_is_not_available) {
  // prepare graph data - in this case, no data (no vertices and no edges, but collections v and e)
  graph::MockGraph graph{};
  gdb.addGraph(graph);

  std::shared_ptr<arangodb::LogicalCollection> col = gdb.vocbase.lookupCollection("e");
  LocalDocumentId localDocumentId{1}; // invalid
  DataSourceId dataSourceId{col->id()}; // valid
  EdgeDocumentToken edt{dataSourceId, localDocumentId};
  VPackBuilder builder;

  // NOTE: we do not have the data, so we get null for any vertex
  traverserCache->insertEdgeIntoResult(edt, builder);
  ASSERT_TRUE(builder.slice().isNull());
}

TEST_F(TraverserCacheTest, it_should_increase_memory_usage_when_persisting_a_string) {
  auto memoryUsageBefore = _monitor->current();

  auto data = VPackParser::fromJson(R"({"_key":"123", "value":123})");
  VPackSlice doc = data->slice();
  HashedStringRef key{doc.get("_key")};
  traverserCache->persistString(key);

  EXPECT_LT(memoryUsageBefore, _monitor->current());
  // now clear the class, and check memory usage again
  traverserCache->clear();
  EXPECT_EQ(memoryUsageBefore, _monitor->current());
}

}  // namespace traverser_cache_test
}  // namespace tests
}  // namespace arangodb
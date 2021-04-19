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

#include "Aql/Query.h"
#include "Cluster/ServerState.h"
#include "Graph/Cache/RefactoredTraverserCache.h"
#include "Graph/GraphTestTools.h"
#include "Graph/TraverserOptions.h"
#include "Transaction/Methods.h"

#include <Aql/TraversalStats.h>
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
  std::map<std::string, std::string> collectionToShardMap{}; // can be empty, only used in standalone mode
  arangodb::ResourceMonitor* _monitor;

  TraverserCacheTest() : gdb(s.server, "testVocbase") {
    query = gdb.getQuery("RETURN 1", std::vector<std::string>{});
    queryContext = query.get()->newTrxContext();
    trx = std::make_unique<arangodb::transaction::Methods>(queryContext);
    _monitor = &query->resourceMonitor();
    traverserCache =
        std::make_unique<RefactoredTraverserCache>(trx.get(), query.get(),
                                                   query->resourceMonitor(), stats, collectionToShardMap);
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
  traverserCache->insertVertexIntoResult(stats, arangodb::velocypack::HashedStringRef(id), builder);
  ASSERT_TRUE(builder.slice().isNull());
  auto all = query->warnings().all();
  ASSERT_EQ(all.size(), 1);
  ASSERT_TRUE(all[0].first == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
  ASSERT_TRUE(all[0].second == expectedMessage);

  // check stats
  EXPECT_EQ(stats.getHttpRequests(), 0);
  EXPECT_EQ(stats.getFiltered(), 0);
  EXPECT_EQ(stats.getScannedIndex(), 0);
}

TEST_F(TraverserCacheTest, it_should_return_a_null_aqlvalue_if_edge_is_not_available) {
  // prepare graph data - in this case, no data (no vertices and no edges, but collections v and e)
  graph::MockGraph graph{};
  gdb.addGraph(graph);

  std::shared_ptr<arangodb::LogicalCollection> col =
      gdb.vocbase.lookupCollection("e");
  LocalDocumentId localDocumentId{1};    // invalid
  DataSourceId dataSourceId{col->id()};  // valid
  EdgeDocumentToken edt{dataSourceId, localDocumentId};
  VPackBuilder builder;

  // NOTE: we do not have the data, so we get null for any edge
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

TEST_F(TraverserCacheTest, it_should_not_increase_memory_usage_twice_when_persisting_two_equal_strings) {
  auto memoryUsageStart = _monitor->current();

  auto data = VPackParser::fromJson(R"({"_key":"123", "value":123})");
  VPackSlice doc = data->slice();
  HashedStringRef key{doc.get("_key")};

  traverserCache->persistString(key);
  EXPECT_LT(memoryUsageStart, _monitor->current());
  auto memoryUsageAfterFirstInsert = _monitor->current();
  traverserCache->persistString(key);
  EXPECT_EQ(memoryUsageAfterFirstInsert, _monitor->current());

  // now clear the class, and check memory usage again
  traverserCache->clear();
  EXPECT_EQ(memoryUsageStart, _monitor->current());
}

TEST_F(TraverserCacheTest, it_should_increase_memory_usage_twice_when_persisting_two_strings) {
  auto memoryUsageStart = _monitor->current();

  auto data = VPackParser::fromJson(R"({"_key":"123", "value":123})");
  VPackSlice doc = data->slice();
  HashedStringRef key{doc.get("_key")};
  auto data2 = VPackParser::fromJson(R"({"_key":"456", "value":456})");
  VPackSlice doc2 = data2->slice();
  HashedStringRef key2{doc2.get("_key")};

  traverserCache->persistString(key);
  EXPECT_LT(memoryUsageStart, _monitor->current());
  auto memoryUsageAfterFirstInsert = _monitor->current();
  traverserCache->persistString(key2);
  EXPECT_LT(memoryUsageAfterFirstInsert, _monitor->current());

  // now clear the class, and check memory usage again
  traverserCache->clear();
  EXPECT_EQ(memoryUsageStart, _monitor->current());
}

TEST_F(TraverserCacheTest, it_should_increase_memory_usage_twice_when_persisting_a_string_clear_persist_again) {
  auto memoryUsageBefore = _monitor->current();

  auto data = VPackParser::fromJson(R"({"_key":"123", "value":123})");
  VPackSlice doc = data->slice();
  HashedStringRef key{doc.get("_key")};

  traverserCache->persistString(key);
  EXPECT_LT(memoryUsageBefore, _monitor->current());

  // now clear the class, and check memory usage again
  traverserCache->clear();
  EXPECT_EQ(memoryUsageBefore, _monitor->current());

  traverserCache->persistString(key);
  EXPECT_LT(memoryUsageBefore, _monitor->current());

  // now clear the class, and check memory usage again
  traverserCache->clear();
  EXPECT_EQ(memoryUsageBefore, _monitor->current());
}

TEST_F(TraverserCacheTest, it_should_not_increase_memory_usage_when_persisting_duplicate_string) {
  auto memoryUsageBefore = _monitor->current();

  auto data = VPackParser::fromJson(R"({"_key":"123", "value":123})");
  VPackSlice doc = data->slice();
  HashedStringRef key{doc.get("_key")};
  traverserCache->persistString(key);
  auto memoryUsageAfterFirstInsert = _monitor->current();
  EXPECT_LT(memoryUsageBefore, memoryUsageAfterFirstInsert);
  auto returned = traverserCache->persistString(key);
  // not allowed to be increased here
  EXPECT_EQ(memoryUsageAfterFirstInsert, _monitor->current());
  EXPECT_TRUE(returned.equals(key));

  // now clear the class, and check memory usage again
  traverserCache->clear();
  EXPECT_EQ(memoryUsageBefore, _monitor->current());
}

TEST_F(TraverserCacheTest, it_should_insert_a_vertex_into_a_result_builder) {
  // prepare graph data - in this case, no data (no vertices and no edges, but collections v and e)
  graph::MockGraph graph{};
  graph.addEdge(0, 1);
  gdb.addGraph(graph);

  auto data = VPackParser::fromJson(R"({"_key":"0", "_id": "v/0"})");
  VPackSlice doc = data->slice();
  HashedStringRef id{doc.get("_id")};
  VPackBuilder builder;

  traverserCache->insertVertexIntoResult(stats, arangodb::velocypack::HashedStringRef(id), builder);
  EXPECT_TRUE(builder.slice().get("_key").isString());
  EXPECT_EQ(builder.slice().get("_key").toString(), "0");

  // check stats
  EXPECT_EQ(stats.getHttpRequests(), 0);
  EXPECT_EQ(stats.getFiltered(), 0);
  EXPECT_EQ(stats.getScannedIndex(), 1);
}

TEST_F(TraverserCacheTest, it_should_insert_an_edge_into_a_result_builder) {
  graph::MockGraph graph{};
  graph.addEdge(0, 1);
  gdb.addGraph(graph);

  std::string edgeKey = "0-1"; // (EdgeKey format: <from>-<to>)

  std::shared_ptr<arangodb::LogicalCollection> col =
      gdb.vocbase.lookupCollection("e");

  std::uint64_t fetchedDocumentId = 0;
  bool called = false;
  auto result =
      col->getPhysical()->read(trx.get(), arangodb::velocypack::StringRef{edgeKey},
                               [&fetchedDocumentId, &called, &edgeKey](LocalDocumentId const& ldid, VPackSlice edgeDocument) {
                                 fetchedDocumentId = ldid.id();
                                 called = true;
                                 EXPECT_TRUE(edgeDocument.isObject());
                                 EXPECT_TRUE(edgeDocument.get("_key").isString());
                                 EXPECT_EQ(edgeKey, edgeDocument.get("_key").copyString());
                                 return true;
                               });
  ASSERT_TRUE(called);
  ASSERT_TRUE(result.ok());
  ASSERT_NE(fetchedDocumentId, 0);

  DataSourceId dataSourceId{col->id()};  // valid
  LocalDocumentId localDocumentId{fetchedDocumentId}; // valid
  EdgeDocumentToken edt{dataSourceId, localDocumentId};
  VPackBuilder builder;

  traverserCache->insertEdgeIntoResult(edt, builder);
  EXPECT_TRUE(builder.slice().get("_key").isString());
  EXPECT_EQ(builder.slice().get("_key").toString(), "0-1");
  EXPECT_EQ(builder.slice().get("_from").toString(), "v/0");
  EXPECT_EQ(builder.slice().get("_to").toString(), "v/1");

  // check stats
  EXPECT_EQ(stats.getHttpRequests(), 0);
  EXPECT_EQ(stats.getFiltered(), 0);
  EXPECT_EQ(stats.getScannedIndex(), 0); // <- see note in method: appendEdge
}

}  // namespace traverser_cache_test
}  // namespace tests
}  // namespace arangodb

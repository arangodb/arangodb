////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "fakeit.hpp"

#include "Aql/AqlValue.h"
#include "Aql/Query.h"
#include "Aql/QueryWarnings.h"
#include "Cluster/ServerState.h"
#include "Graph/ClusterTraverserCache.h"
#include "Graph/GraphTestTools.h"
#include "Graph/TraverserOptions.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::graph;

namespace arangodb {
namespace tests {
namespace cluster_traverser_cache_test {

class ClusterTraverserCacheTest : public ::testing::Test {
 protected:
  ServerState::RoleEnum oldRole;
  
  graph::GraphTestSetup s;
  graph::MockGraphDatabase gdb;

  ClusterTraverserCacheTest()
      : gdb(s.server, "testVocbase")  {
  }

  ~ClusterTraverserCacheTest() { }
};

TEST_F(ClusterTraverserCacheTest, it_should_return_a_null_aqlvalue_if_vertex_not_cached) {
  std::unordered_map<ServerID, aql::EngineId> engines;
  std::string vertexId = "UnitTest/Vertex";
  std::string expectedMessage = "vertex '" + vertexId + "' not found";
  
  auto q = gdb.getQuery("RETURN 1", std::vector<std::string>{});

  traverser::TraverserOptions opts{*q};
  ClusterTraverserCache testee(*q, &engines, &opts);

  // NOTE: we do not put anything into the cache, so we get null for any vertex
  AqlValue val = testee.fetchVertexAqlResult(arangodb::velocypack::StringRef(vertexId));
  ASSERT_TRUE(val.isNull(false));
  auto all = q->warnings().all();
  ASSERT_TRUE(all.size() == 1);
  ASSERT_TRUE(all[0].first == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
  ASSERT_TRUE(all[0].second == expectedMessage);
}

TEST_F(ClusterTraverserCacheTest, it_should_insert_a_null_vpack_if_vertex_not_cached) {
  std::unordered_map<ServerID, aql::EngineId> engines;
  std::string vertexId = "UnitTest/Vertex";
  std::string expectedMessage = "vertex '" + vertexId + "' not found";

  auto q = gdb.getQuery("RETURN 1", std::vector<std::string>{});
  VPackBuilder result;
  traverser::TraverserOptions opts{*q};
  ClusterTraverserCache testee(*q, &engines, &opts);

  // NOTE: we do not put anything into the cache, so we get null for any vertex
  testee.insertVertexIntoResult(arangodb::velocypack::StringRef(vertexId), result);

  VPackSlice sl = result.slice();
  ASSERT_TRUE(sl.isNull());
  
  auto all = q->warnings().all();
  ASSERT_TRUE(all.size() == 1);
  ASSERT_TRUE(all[0].first == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
  ASSERT_TRUE(all[0].second == expectedMessage);
}

}  // namespace cluster_traverser_cache_test
}  // namespace tests
}  // namespace arangodb

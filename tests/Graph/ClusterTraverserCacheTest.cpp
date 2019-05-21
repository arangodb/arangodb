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
#include "Cluster/ServerState.h"
#include "Graph/ClusterTraverserCache.h"
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
  ServerState* ss;
  ServerState::RoleEnum oldRole;

  ClusterTraverserCacheTest()
      : ss(ServerState::instance()), oldRole(ss->getRole()) {
    ss->setRole(ServerState::ROLE_COORDINATOR);
  }

  ~ClusterTraverserCacheTest() { ss->setRole(oldRole); }
};

TEST_F(ClusterTraverserCacheTest, it_should_return_a_null_aqlvalue_if_vertex_not_cached) {
  std::unordered_map<ServerID, traverser::TraverserEngineID> engines;
  std::string vertexId = "UnitTest/Vertex";
  std::string expectedMessage = "vertex '" + vertexId + "' not found";

  fakeit::Mock<transaction::Methods> trxMock;
  transaction::Methods& trx = trxMock.get();

  fakeit::Mock<Query> queryMock;
  Query& query = queryMock.get();
  fakeit::When(Method(queryMock, trx)).AlwaysReturn(&trx);
  fakeit::When(OverloadedMethod(queryMock, registerWarning, void(int, const char*)))
      .Do([&](int code, char const* message) {
        ASSERT_TRUE(code == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
        ASSERT_TRUE(strcmp(message, expectedMessage.c_str()) == 0);
      });

  ClusterTraverserCache testee(&query, &engines);

  // NOTE: we do not put anything into the cache, so we get null for any vertex
  AqlValue val = testee.fetchVertexAqlResult(arangodb::velocypack::StringRef(vertexId));
  ASSERT_TRUE(val.isNull(false));
  fakeit::Verify(OverloadedMethod(queryMock, registerWarning, void(int, const char*)))
      .Exactly(1);
}

TEST_F(ClusterTraverserCacheTest, it_should_insert_a_null_vpack_if_vertex_not_cached) {
  std::unordered_map<ServerID, traverser::TraverserEngineID> engines;
  std::string vertexId = "UnitTest/Vertex";
  std::string expectedMessage = "vertex '" + vertexId + "' not found";

  fakeit::Mock<transaction::Methods> trxMock;
  transaction::Methods& trx = trxMock.get();

  fakeit::Mock<Query> queryMock;
  Query& query = queryMock.get();
  fakeit::When(Method(queryMock, trx)).AlwaysReturn(&trx);
  fakeit::When(OverloadedMethod(queryMock, registerWarning, void(int, const char*)))
      .Do([&](int code, char const* message) {
        ASSERT_TRUE(code == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
        ASSERT_TRUE(strcmp(message, expectedMessage.c_str()) == 0);
      });

  VPackBuilder result;

  ClusterTraverserCache testee(&query, &engines);

  // NOTE: we do not put anything into the cache, so we get null for any vertex
  testee.insertVertexIntoResult(arangodb::velocypack::StringRef(vertexId), result);

  VPackSlice sl = result.slice();
  ASSERT_TRUE(sl.isNull());

  fakeit::Verify(OverloadedMethod(queryMock, registerWarning, void(int, const char*)))
      .Exactly(1);
}

}  // namespace cluster_traverser_cache_test
}  // namespace tests
}  // namespace arangodb

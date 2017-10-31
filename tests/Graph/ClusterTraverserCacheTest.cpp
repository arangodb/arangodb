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

#include "catch.hpp"
#include "fakeit.hpp"

#include "Aql/AqlValue.h"
#include "Cluster/ServerState.h"
#include "Graph/ClusterTraverserCache.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::graph;
using namespace fakeit;

namespace arangodb {
namespace tests {
namespace cluster_traverser_cache_test {

TEST_CASE("ClusterTraverserCache", "[aql][cluster]") {

  auto ss = ServerState::instance();
  ss->setRole(ServerState::ROLE_COORDINATOR);

  SECTION("it should return a NULL AQLValue if vertex not cached") {
    std::unordered_map<ServerID, traverser::TraverserEngineID> engines;
    std::string vertexId = "UnitTest/Vertex";

    Mock<transaction::Methods> trxMock;
    transaction::Methods& trx = trxMock.get();

    ClusterTraverserCache testee(&trx, &engines);

    // NOTE: we do not put anything into the cache, so we get null for any vertex
    AqlValue val = testee.fetchVertexAqlResult(StringRef(vertexId));
    REQUIRE(val.isNull(false));
  }

  SECTION("it should insert a NULL VPack if vertex not cached") {
    std::unordered_map<ServerID, traverser::TraverserEngineID> engines;
    std::string vertexId = "UnitTest/Vertex";

    Mock<transaction::Methods> trxMock;
    transaction::Methods& trx = trxMock.get();
    VPackBuilder result;

    ClusterTraverserCache testee(&trx, &engines);

    // NOTE: we do not put anything into the cache, so we get null for any vertex
    testee.insertVertexIntoResult(StringRef(vertexId), result);

    VPackSlice sl = result.slice();
    REQUIRE(sl.isNull());
  }

}

} // cluster_traveser_cache_test
} // tests
} // arangodb

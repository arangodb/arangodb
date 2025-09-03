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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#include "Graph/GraphTestTools.h"
#include "Mocks/MockGraph.h"
#include "Mocks/MockGraphProvider.h"
#include "Mocks/PreparedResponseConnectionPool.h"
#include "Aql/QueryRegistry.h"
#include "IResearch/RestHandlerMock.h"

#include "Basics/GlobalResourceMonitor.h"
#include "VelocypackUtils/VelocyPackStringLiteral.h"
#include "Basics/ResourceUsage.h"
#include "Graph/Providers/SingleServerProvider.h"
#include "Graph/Steps/SingleServerProviderStep.h"
#include "InternalRestHandler/InternalRestTraverserHandler.h"
#include "Graph/TraverserOptions.h"

#include <unordered_set>
#include <gtest/gtest.h>

using namespace arangodb;
using namespace arangodb::tests;
using namespace arangodb::tests::graph;
using namespace arangodb::graph;

namespace arangodb::tests {

class InternalRestTraverserHandlerTest : public ::testing::Test {
 protected:
  arangodb::tests::mocks::MockDBServer server;
  std::shared_ptr<arangodb::transaction::StandaloneContext> ctx;
  arangodb::aql::QueryString queryString;
  std::shared_ptr<MockQuery> fakeQuery;
  aql::QueryRegistry queryRegistry;

  InternalRestTraverserHandlerTest()
      : server{"PRMR_0001", true, true},
        ctx{std::make_shared<arangodb::transaction::StandaloneContext>(
            server.getSystemDatabase(),
            transaction::OperationOriginTestCase{})},
        queryString{std::string_view("RETURN 1")},
        fakeQuery{std::make_shared<MockQuery>(ctx, queryString)},
        queryRegistry{120} {}
  ~InternalRestTraverserHandlerTest() {}

  auto createEngine(MockGraph const& graph) -> uint64_t {
    graph.prepareServer(server);

    try {
      fakeQuery->collections().add(graph.getEdgeShardNameServerPairs()[0].first,
                                   AccessMode::Type::READ,
                                   arangodb::aql::Collection::Hint::Shard);
    } catch (...) {
    }
    waitForAsync(fakeQuery->prepareQuery());
    auto ast = fakeQuery->ast();
    auto tmpVar = ast->variables()->createTemporaryVariable();
    auto tmpVarRef = ast->createNodeReference(tmpVar);
    auto tmpIdNode = ast->createNodeValueString("", 0);

    traverser::TraverserOptions opts{*fakeQuery};
    opts.setVariable(tmpVar);

    auto const* access =
        ast->createNodeAttributeAccess(tmpVarRef, StaticStrings::FromString);
    auto const* cond = ast->createNodeBinaryOperator(
        NODE_TYPE_OPERATOR_BINARY_EQ, access, tmpIdNode);
    auto fromCondition =
        ast->createNodeNaryOperator(NODE_TYPE_OPERATOR_NARY_AND);
    fromCondition->addMember(cond);
    opts.addLookupInfo(fakeQuery->plan(),
                       graph.getEdgeShardNameServerPairs()[0].first,
                       StaticStrings::FromString, fromCondition,
                       /*onlyEdgeIndexes*/ false, TRI_EDGE_OUT);

    return graph.createEngine(server, opts, queryRegistry);
  }
};

TEST_F(InternalRestTraverserHandlerTest, errors_for_negative_batch_size) {
  MockGraph g;
  auto engineId = createEngine(g);

  auto fakeRequest = GeneralRequestMock::generate(
      server.getSystemDatabase(), arangodb::rest::RequestType::PUT,
      {"edge", basics::StringUtils::itoa(engineId)},
      VPackBuilder(R"({"keys": ["v/0"], "depth": 1, "batchSize": -3})"_vpack));
  auto fakeResponse = std::make_unique<GeneralResponseMock>();
  auto handler =
      InternalRestTraverserHandler{server.server(), fakeRequest.release(),
                                   fakeResponse.release(), &queryRegistry};

  handler.executeAsync().waitAndGet();

  auto response =
      dynamic_cast<GeneralResponseMock*>(handler.stealResponse().release());
  EXPECT_EQ(response->responseCode(), ResponseCode::BAD);
}

}  // namespace arangodb::tests

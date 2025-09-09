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

class InternalRestTraverserHandlerEdgeTest : public ::testing::Test {
 protected:
  arangodb::tests::mocks::MockDBServer server;
  std::shared_ptr<arangodb::transaction::StandaloneContext> ctx;
  arangodb::aql::QueryString queryString;
  std::shared_ptr<MockQuery> fakeQuery;
  aql::QueryRegistry queryRegistry;

  InternalRestTraverserHandlerEdgeTest()
      : server{"PRMR_0001", true, true},
        ctx{std::make_shared<arangodb::transaction::StandaloneContext>(
            server.getSystemDatabase(),
            transaction::OperationOriginTestCase{})},
        queryString{std::string_view("RETURN 1")},
        fakeQuery{std::make_shared<MockQuery>(ctx, queryString)},
        queryRegistry{120} {}
  ~InternalRestTraverserHandlerEdgeTest() {}

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

  auto requestHandler(RequestType type, std::vector<std::string> suffixes,
                      VPackBuilder payload)
      -> std::unique_ptr<GeneralResponseMock> {
    auto fakeRequest =
        GeneralRequestMock::generate(server.getSystemDatabase(), type,
                                     std::move(suffixes), std::move(payload));
    auto fakeResponse = std::make_unique<GeneralResponseMock>();
    auto handler =
        InternalRestTraverserHandler{server.server(), fakeRequest.release(),
                                     fakeResponse.release(), &queryRegistry};

    handler.executeAsync().waitAndGet();

    auto response =
        dynamic_cast<GeneralResponseMock*>(handler.stealResponse().release());
    return std::unique_ptr<GeneralResponseMock>{response};
  }

  auto destroyEngine(uint64_t engineId) -> void {
    auto response =
        requestHandler(arangodb::rest::RequestType::DELETE_REQ,
                       {basics::StringUtils::itoa(engineId)}, VPackBuilder{});
    EXPECT_EQ(response->responseCode(), ResponseCode::OK);
  }
};

TEST_F(InternalRestTraverserHandlerEdgeTest, errors_for_negative_batch_size) {
  MockGraph g;
  auto engineId = createEngine(g);

  auto response = requestHandler(
      RequestType::PUT, {"edge", basics::StringUtils::itoa(engineId)},
      VPackBuilder(R"({"keys": ["v/0"], "depth": 1, "batchSize": -3})"_vpack));

  EXPECT_EQ(response->responseCode(), ResponseCode::BAD);
}

TEST_F(InternalRestTraverserHandlerEdgeTest, errors_for_zero_batch_size) {
  MockGraph g;
  auto engineId = createEngine(g);

  auto response = requestHandler(
      RequestType::PUT, {"edge", basics::StringUtils::itoa(engineId)},
      VPackBuilder(R"({"keys": ["v/0"], "depth": 1, "batchSize": 0})"_vpack));

  EXPECT_EQ(response->responseCode(), ResponseCode::BAD);
}

TEST_F(InternalRestTraverserHandlerEdgeTest,
       gives_all_neighbours_without_batchSize_input) {
  MockGraph g;
  g.addEdge(0, 1);
  g.addEdge(0, 2);
  g.addEdge(0, 0);
  g.addEdge(1, 2);
  auto engineId = createEngine(g);

  auto response =
      requestHandler(arangodb::rest::RequestType::PUT,
                     {"edge", basics::StringUtils::itoa(engineId)},
                     VPackBuilder(R"({"keys": ["v/0"], "depth": 1})"_vpack));

  EXPECT_EQ(response->responseCode(), ResponseCode::OK);
  auto edges = response->_payload.slice().get("edges");
  EXPECT_FALSE(edges.isNone());
  std::unordered_set<std::string> toVertices;
  for (VPackSlice edge : VPackArrayIterator(edges)) {
    EXPECT_EQ(edge.get("_from").copyString(), "v/0");
    toVertices.emplace(edge.get("_to").copyString());
  }
  EXPECT_EQ(toVertices, (std::unordered_set<std::string>{"v/0", "v/1", "v/2"}));

  destroyEngine(engineId);
}

TEST_F(InternalRestTraverserHandlerEdgeTest,
       gives_next_batch_of_neighbours_with_batchSize_input) {
  MockGraph g;
  g.addEdge(0, 1);
  g.addEdge(0, 2);
  g.addEdge(0, 0);
  g.addEdge(1, 2);
  auto engineId = createEngine(g);

  std::unordered_multiset<std::string> toVertices;
  {
    auto response = requestHandler(
        arangodb::rest::RequestType::PUT,
        {"edge", basics::StringUtils::itoa(engineId)},
        VPackBuilder(R"({"keys": ["v/0"], "depth": 1, "batchSize": 2})"_vpack));

    EXPECT_EQ(response->responseCode(), ResponseCode::OK);
    auto edges = response->_payload.slice().get("edges");
    EXPECT_FALSE(edges.isNone());
    for (VPackSlice edge : VPackArrayIterator(edges)) {
      EXPECT_EQ(edge.get("_from").copyString(), "v/0");
      toVertices.emplace(edge.get("_to").copyString());
    }
    EXPECT_EQ(toVertices.size(), 2);
  }

  {
    auto second_response =
        requestHandler(arangodb::rest::RequestType::PUT,
                       {"edge", basics::StringUtils::itoa(engineId)},
                       VPackBuilder(R"({"continue": true})"_vpack));

    EXPECT_EQ(second_response->responseCode(), ResponseCode::OK);
    auto edges = second_response->_payload.slice().get("edges");
    EXPECT_FALSE(edges.isNone());
    for (VPackSlice edge : VPackArrayIterator(edges)) {
      EXPECT_EQ(edge.get("_from").copyString(), "v/0");
      toVertices.emplace(edge.get("_to").copyString());
    }
    EXPECT_EQ(toVertices.size(), 3);
  }
  EXPECT_EQ(toVertices,
            (std::unordered_multiset<std::string>{"v/0", "v/1", "v/2"}));

  destroyEngine(engineId);
}

}  // namespace arangodb::tests

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
  QueryRegistry queryRegistry;

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
       errors_for_continue_request_without_an_existing_cursor) {
  MockGraph g;
  auto engineId = createEngine(g);

  auto response = requestHandler(
      RequestType::PUT, {"edge", basics::StringUtils::itoa(engineId)},
      VPackBuilder(R"({"cursorId": 0, "batchId": 0})"_vpack));

  EXPECT_EQ(response->responseCode(), ResponseCode::BAD);
}

TEST_F(InternalRestTraverserHandlerEdgeTest,
       gives_all_edges_without_batching_for_backwards_compatibility) {
  MockGraph g;
  g.addEdge(0, 1);
  g.addEdge(0, 2);
  g.addEdge(0, 0);
  g.addEdge(1, 2);
  auto engineId = createEngine(g);

  std::unordered_multiset<std::string> vertices;
  auto response =
      requestHandler(arangodb::rest::RequestType::PUT,
                     {"edge", basics::StringUtils::itoa(engineId)},
                     VPackBuilder(R"({"keys": ["v/0"], "depth": 1})"_vpack));

  EXPECT_EQ(response->responseCode(), ResponseCode::OK);
  auto edges = response->_payload.slice().get("edges");
  EXPECT_FALSE(edges.isNone());
  for (VPackSlice edge : VPackArrayIterator(edges)) {
    vertices.emplace(edge.get("_to").copyString());
  }
  EXPECT_EQ(vertices,
            (std::unordered_multiset<std::string>{"v/0", "v/1", "v/2"}));

  destroyEngine(engineId);
}

TEST_F(InternalRestTraverserHandlerEdgeTest,
       gives_no_edges_for_non_existing_vertices) {
  MockGraph g;
  g.addEdge(0, 1);
  g.addEdge(0, 2);
  g.addEdge(0, 0);
  g.addEdge(1, 2);
  auto engineId = createEngine(g);

  auto response = requestHandler(
      arangodb::rest::RequestType::PUT,
      {"edge", basics::StringUtils::itoa(engineId)},
      VPackBuilder(R"({"keys": ["v/5"], "depth": 1, "batchSize": 2})"_vpack));

  EXPECT_EQ(response->responseCode(), ResponseCode::OK);
  EXPECT_TRUE(response->_payload.slice().get("done").isTrue());
  auto edges = response->_payload.slice().get("edges");
  EXPECT_FALSE(edges.isNone());
  EXPECT_TRUE(edges.isEmptyArray());

  destroyEngine(engineId);
}

TEST_F(InternalRestTraverserHandlerEdgeTest,
       gives_no_edges_for_empty_vertices) {
  MockGraph g;
  g.addEdge(0, 1);
  g.addEdge(0, 2);
  g.addEdge(0, 0);
  g.addEdge(1, 2);
  auto engineId = createEngine(g);

  auto response = requestHandler(
      arangodb::rest::RequestType::PUT,
      {"edge", basics::StringUtils::itoa(engineId)},
      VPackBuilder(R"({"keys": [], "depth": 1, "batchSize": 2})"_vpack));

  EXPECT_EQ(response->responseCode(), ResponseCode::OK);
  EXPECT_TRUE(response->_payload.slice().get("done").isTrue());
  auto edges = response->_payload.slice().get("edges");
  EXPECT_FALSE(edges.isNone());
  EXPECT_TRUE(edges.isEmptyArray());

  destroyEngine(engineId);
}

TEST_F(InternalRestTraverserHandlerEdgeTest, continues_with_next_batch) {
  MockGraph g;
  g.addEdge(0, 1);
  g.addEdge(0, 2);
  g.addEdge(0, 0);
  g.addEdge(1, 2);
  auto engineId = createEngine(g);

  size_t batchId;
  size_t cursorId;
  std::unordered_multiset<std::string> verticesFirstBatch;
  {  // request neighbours of v/0
    auto response = requestHandler(
        arangodb::rest::RequestType::PUT,
        {"edge", basics::StringUtils::itoa(engineId)},
        VPackBuilder(R"({"keys": ["v/0"], "depth": 1, "batchSize": 2})"_vpack));

    EXPECT_EQ(response->responseCode(), ResponseCode::OK);
    EXPECT_FALSE(response->_payload.slice().get("done").isTrue());
    auto edges = response->_payload.slice().get("edges");
    EXPECT_FALSE(edges.isNone());
    for (VPackSlice edge : VPackArrayIterator(edges)) {
      verticesFirstBatch.emplace(edge.get("_to").copyString());
    }
    EXPECT_EQ(verticesFirstBatch.size(), 2);
    EXPECT_EQ(response->_payload.slice().get("cursorId").getInt(), 0);
    cursorId = 0;
    EXPECT_EQ(response->_payload.slice().get("batchId").getInt(), 0);
    batchId = 0;
  }

  {  // request the same batch again gives error
    VPackBuilder request;
    request.openObject();
    request.add("cursorId", VPackValue(cursorId));
    request.add("batchId", VPackValue(batchId));
    request.close();
    auto response = requestHandler(
        arangodb::rest::RequestType::PUT,
        {"edge", basics::StringUtils::itoa(engineId)}, std::move(request));

    EXPECT_EQ(response->responseCode(), ResponseCode::BAD);
  }

  {  // request previous (non-existent) batch
    VPackBuilder request;
    request.openObject();
    request.add("cursorId", VPackValue(cursorId));
    request.add("batchId", VPackValue((int)batchId - 1));
    request.close();
    auto response = requestHandler(
        arangodb::rest::RequestType::PUT,
        {"edge", basics::StringUtils::itoa(engineId)}, std::move(request));

    EXPECT_EQ(response->responseCode(), ResponseCode::BAD);
  }

  {  // request a batch from a different - currently non existent cursor - gives
     // error
    VPackBuilder request;
    request.openObject();
    request.add("cursorId", VPackValue(1));
    request.add("batchId", VPackValue(batchId + 1));
    request.close();
    auto response = requestHandler(
        arangodb::rest::RequestType::PUT,
        {"edge", basics::StringUtils::itoa(engineId)}, std::move(request));

    EXPECT_EQ(response->responseCode(), ResponseCode::BAD);
  }

  {  // request next batch is fine
    std::unordered_multiset<std::string> verticesSecondBatch;
    VPackBuilder request;
    request.openObject();
    request.add("cursorId", VPackValue(cursorId));
    request.add("batchId", VPackValue(batchId + 1));
    request.close();
    auto response = requestHandler(
        arangodb::rest::RequestType::PUT,
        {"edge", basics::StringUtils::itoa(engineId)}, std::move(request));

    EXPECT_EQ(response->responseCode(), ResponseCode::OK);
    EXPECT_TRUE(response->_payload.slice().get("done").isTrue());
    auto edges = response->_payload.slice().get("edges");
    EXPECT_FALSE(edges.isNone());
    for (VPackSlice edge : VPackArrayIterator(edges)) {
      verticesSecondBatch.emplace(edge.get("_to").copyString());
    }
    EXPECT_EQ(verticesSecondBatch.size(), 1);
    std::unordered_multiset<std::string> verticesBothBatches =
        verticesFirstBatch;
    verticesBothBatches.insert(verticesSecondBatch.begin(),
                               verticesSecondBatch.end());
    EXPECT_EQ(verticesBothBatches,
              (std::unordered_multiset<std::string>{"v/0", "v/1", "v/2"}));
    EXPECT_EQ(response->_payload.slice().get("cursorId").getInt(), 0);
    EXPECT_EQ(response->_payload.slice().get("batchId").getInt(), 1);
    batchId = 1;
  }

  {  // request previous batch gives an error
    std::unordered_multiset<std::string> verticesSecondBatch;
    VPackBuilder request;
    request.openObject();
    request.add("cursorId", VPackValue(cursorId));
    request.add("batchId", VPackValue(batchId - 1));
    request.close();
    auto response = requestHandler(
        arangodb::rest::RequestType::PUT,
        {"edge", basics::StringUtils::itoa(engineId)}, std::move(request));

    EXPECT_EQ(response->responseCode(), ResponseCode::BAD);
  }

  {  // request the same batch again gives error
    VPackBuilder request;
    request.openObject();
    request.add("cursorId", VPackValue(cursorId));
    request.add("batchId", VPackValue(batchId));
    request.close();
    auto response = requestHandler(
        arangodb::rest::RequestType::PUT,
        {"edge", basics::StringUtils::itoa(engineId)}, std::move(request));

    EXPECT_EQ(response->responseCode(), ResponseCode::BAD);
  }

  destroyEngine(engineId);
}

TEST_F(InternalRestTraverserHandlerEdgeTest,
       all_cursors_that_exist_can_be_queried) {
  MockGraph g;
  g.addEdge(0, 1);
  g.addEdge(0, 2);
  g.addEdge(0, 0);
  g.addEdge(1, 2);
  g.addEdge(1, 3);
  auto engineId = createEngine(g);

  std::unordered_multiset<std::string> v0_toVertices;
  std::unordered_multiset<std::string> v1_toVertices;

  {  // request two out of the three neighbours of v/0
    auto response = requestHandler(
        arangodb::rest::RequestType::PUT,
        {"edge", basics::StringUtils::itoa(engineId)},
        VPackBuilder(
            R"({"keys": ["v/0"], "depth": 1, "batchSize": 2, "creationId":
            0})"_vpack));

    EXPECT_EQ(response->responseCode(), ResponseCode::OK);
    EXPECT_FALSE(response->_payload.slice().get("done").isTrue());
    EXPECT_EQ(response->_payload.slice().get("cursorId").getInt(), 0);
    EXPECT_EQ(response->_payload.slice().get("batchId").getInt(), 0);
    auto edges = response->_payload.slice().get("edges");
    EXPECT_FALSE(edges.isNone());
    for (VPackSlice edge : VPackArrayIterator(edges)) {
      EXPECT_EQ(edge.get("_from").copyString(), "v/0");
      v0_toVertices.emplace(edge.get("_to").copyString());
    }
    EXPECT_EQ(v0_toVertices.size(), 2);
  }

  {  // requests one neighbour of v/1 (creating a new cursor)
    auto response = requestHandler(
        arangodb::rest::RequestType::PUT,
        {"edge", basics::StringUtils::itoa(engineId)},
        VPackBuilder(
            R"({"keys": ["v/1"], "depth": 1, "batchSize": 1, "creationId":
            1})"_vpack));

    EXPECT_EQ(response->responseCode(), ResponseCode::OK);
    EXPECT_FALSE(response->_payload.slice().get("done").isTrue());
    EXPECT_EQ(response->_payload.slice().get("cursorId").getInt(), 1);
    EXPECT_EQ(response->_payload.slice().get("batchId").getInt(), 0);
    auto edges = response->_payload.slice().get("edges");
    EXPECT_FALSE(edges.isNone());
    for (VPackSlice edge : VPackArrayIterator(edges)) {
      EXPECT_EQ(edge.get("_from").copyString(), "v/1");
      v1_toVertices.emplace(edge.get("_to").copyString());
    }
    EXPECT_EQ(v1_toVertices.size(), 1);
  }

  {  // continue with v/0 in between
    auto response =
        requestHandler(arangodb::rest::RequestType::PUT,
                       {"edge", basics::StringUtils::itoa(engineId)},
                       VPackBuilder(R"({"cursorId": 0, "batchId": 1})"_vpack));

    EXPECT_EQ(response->responseCode(), ResponseCode::OK);
    EXPECT_TRUE(response->_payload.slice().get("done").isTrue());
    EXPECT_EQ(response->_payload.slice().get("cursorId").getInt(), 0);
    EXPECT_EQ(response->_payload.slice().get("batchId").getInt(), 1);
    auto edges = response->_payload.slice().get("edges");
    EXPECT_FALSE(edges.isNone());
    for (VPackSlice edge : VPackArrayIterator(edges)) {
      EXPECT_EQ(edge.get("_from").copyString(), "v/0");
      v0_toVertices.emplace(edge.get("_to").copyString());
    }
  }

  {  // continue with v/1
    auto response =
        requestHandler(arangodb::rest::RequestType::PUT,
                       {"edge", basics::StringUtils::itoa(engineId)},
                       VPackBuilder(R"({"cursorId": 1, "batchId": 1})"_vpack));

    EXPECT_EQ(response->responseCode(), ResponseCode::OK);
    EXPECT_TRUE(response->_payload.slice().get("done").isTrue());
    EXPECT_EQ(response->_payload.slice().get("cursorId").getInt(), 1);
    EXPECT_EQ(response->_payload.slice().get("batchId").getInt(), 1);
    auto edges = response->_payload.slice().get("edges");
    EXPECT_FALSE(edges.isNone());
    for (VPackSlice edge : VPackArrayIterator(edges)) {
      EXPECT_EQ(edge.get("_from").copyString(), "v/1");
      v1_toVertices.emplace(edge.get("_to").copyString());
    }
  }

  EXPECT_EQ(v1_toVertices,
            (std::unordered_multiset<std::string>{"v/2", "v/3"}));
  EXPECT_EQ(v0_toVertices,
            (std::unordered_multiset<std::string>{"v/1", "v/2", "v/0"}));

  destroyEngine(engineId);
}

TEST_F(InternalRestTraverserHandlerEdgeTest,
       gives_edges_of_all_given_input_vertices) {
  MockGraph g;
  g.addEdge(0, 1);
  g.addEdge(0, 2);
  g.addEdge(1, 1);
  g.addEdge(1, 2);
  g.addEdge(1, 0);
  auto engineId = createEngine(g);

  std::unordered_multimap<std::string, std::string> fromAndToVertices;
  {  // request 2 neighbours of v/0 and v/1
    auto response = requestHandler(
        arangodb::rest::RequestType::PUT,
        {"edge", basics::StringUtils::itoa(engineId)},
        VPackBuilder(
            R"({"keys": ["v/0", "v/1"], "depth": 1, "batchSize": 2})"_vpack));

    EXPECT_EQ(response->responseCode(), ResponseCode::OK);
    EXPECT_FALSE(response->_payload.slice().get("done").isTrue());
    EXPECT_EQ(response->_payload.slice().get("cursorId").getInt(), 0);
    EXPECT_EQ(response->_payload.slice().get("batchId").getInt(), 0);
    auto edges = response->_payload.slice().get("edges");
    EXPECT_FALSE(edges.isNone());
    for (VPackSlice edge : VPackArrayIterator(edges)) {
      fromAndToVertices.emplace(edge.get("_from").copyString(),
                                edge.get("_to").copyString());
    }
    EXPECT_EQ(fromAndToVertices.size(), 2);
  }

  {  // continue with next two neighbours of v/0 and v/1
    auto response =
        requestHandler(arangodb::rest::RequestType::PUT,
                       {"edge", basics::StringUtils::itoa(engineId)},
                       VPackBuilder(R"({"cursorId": 0, "batchId": 1})"_vpack));

    EXPECT_EQ(response->responseCode(), ResponseCode::OK);
    EXPECT_FALSE(response->_payload.slice().get("done").isTrue());
    EXPECT_EQ(response->_payload.slice().get("cursorId").getInt(), 0);
    EXPECT_EQ(response->_payload.slice().get("batchId").getInt(), 1);
    auto edges = response->_payload.slice().get("edges");
    EXPECT_FALSE(edges.isNone());
    for (VPackSlice edge : VPackArrayIterator(edges)) {
      fromAndToVertices.emplace(edge.get("_from").copyString(),
                                edge.get("_to").copyString());
    }
    EXPECT_EQ(fromAndToVertices.size(), 4);
  }

  {  // continue next neighbours of v/0 and v/1 (last one)
    auto response =
        requestHandler(arangodb::rest::RequestType::PUT,
                       {"edge", basics::StringUtils::itoa(engineId)},
                       VPackBuilder(R"({"cursorId": 0, "batchId": 2})"_vpack));

    EXPECT_EQ(response->responseCode(), ResponseCode::OK);
    EXPECT_TRUE(response->_payload.slice().get("done").isTrue());
    EXPECT_EQ(response->_payload.slice().get("cursorId").getInt(), 0);
    EXPECT_EQ(response->_payload.slice().get("batchId").getInt(), 2);
    auto edges = response->_payload.slice().get("edges");
    EXPECT_FALSE(edges.isNone());
    for (VPackSlice edge : VPackArrayIterator(edges)) {
      fromAndToVertices.emplace(edge.get("_from").copyString(),
                                edge.get("_to").copyString());
    }
    EXPECT_EQ(fromAndToVertices.size(), 5);
  }

  EXPECT_EQ(
      fromAndToVertices,
      (std::unordered_multimap<std::string, std::string>{{"v/0", "v/1"},
                                                         {"v/0", "v/2"},
                                                         {"v/1", "v/0"},
                                                         {"v/1", "v/1"},
                                                         {"v/1", "v/2"}}));

  destroyEngine(engineId);
}

}  // namespace arangodb::tests

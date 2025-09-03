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

namespace {
aql::AstNode* InitializeReference(aql::Ast& ast, aql::Variable& var) {
  ast.scopes()->start(aql::ScopeType::AQL_SCOPE_MAIN);
  ast.scopes()->addVariable(&var);
  aql::AstNode* a = ast.createNodeReference(var.name);
  ast.scopes()->endCurrent();
  return a;
}
}  // namespace

namespace arangodb {
namespace tests {
namespace single_server_provider_test {

using Step = SingleServerProviderStep;

class InternalRestTraverserHandlerTest : public ::testing::Test {
 protected:
  // Only used to mock a singleServer
  std::unique_ptr<GraphTestSetup> s{nullptr};
  std::unique_ptr<MockGraphDatabase> singleServer{nullptr};
  std::shared_ptr<arangodb::aql::Query> query{nullptr};
  arangodb::GlobalResourceMonitor _global{};
  arangodb::ResourceMonitor _resourceMonitor{_global};
  arangodb::aql::AqlFunctionsInternalCache _functionsCache{};
  std::unique_ptr<arangodb::aql::FixedVarExpressionContext> _expressionContext;
  std::unique_ptr<arangodb::transaction::Methods> _trx{};

  // Expression Parts
  aql::Variable* _tmpVar{nullptr};
  aql::AstNode* _varNode{nullptr};
  aql::Projections _vertexProjections{};
  aql::Projections _edgeProjections{};
  ResourceUsageAllocator<MonitoredCollectionToShardMap, ResourceMonitor> alloc =
      {_resourceMonitor};
  MonitoredCollectionToShardMap _emptyShardMap{alloc};

  // can be used for further testing to generate a expression
  // std::string stringToMatch = "0-1";

  InternalRestTraverserHandlerTest() {}
  ~InternalRestTraverserHandlerTest() {}

  auto makeProvider(MockGraph const& graph)
      -> arangodb::graph::SingleServerProvider<SingleServerProviderStep> {
    // Setup code for each provider type
    s = std::make_unique<GraphTestSetup>();
    singleServer =
        std::make_unique<MockGraphDatabase>(s->server, "testVocbase");
    singleServer->addGraph(graph);

    // We now have collections "v" and "e"
    query = singleServer->getQuery("RETURN 1", {"v", "e"});
    _trx = std::make_unique<arangodb::transaction::Methods>(
        query->newTrxContext());

    auto edgeIndexHandle = singleServer->getEdgeIndexHandle("e");
    _tmpVar = singleServer->generateTempVar(query.get());

    auto indexCondition =
        singleServer->buildOutboundCondition(query.get(), _tmpVar);
    _varNode = ::InitializeReference(*query->ast(), *_tmpVar);

    std::vector<IndexAccessor> usedIndexes{};

    // can be used to create an expression, currently unused but may be helpful
    // for additional tests auto expr = conditionKeyMatches(stringToMatch);
    usedIndexes.emplace_back(IndexAccessor{edgeIndexHandle, indexCondition, 0,
                                           nullptr, std::nullopt, 0,
                                           TRI_EDGE_OUT});

    _expressionContext =
        std::make_unique<arangodb::aql::FixedVarExpressionContext>(
            *_trx, *query, _functionsCache);
    SingleServerBaseProviderOptions opts(
        _tmpVar,
        std::make_pair(
            std::move(usedIndexes),
            std::unordered_map<uint64_t, std::vector<IndexAccessor>>{}),
        *_expressionContext.get(), {}, _emptyShardMap, _vertexProjections,
        _edgeProjections, /*produceVertices*/ true, /*useCache*/ true);
    return {*query.get(), std::move(opts), _resourceMonitor};
  }

  /*
   * generates a condition #TMP._key == '<toMatch>'
   */
  std::unique_ptr<aql::Expression> conditionKeyMatches(
      std::string const& toMatch) {
    auto expectedKey =
        query->ast()->createNodeValueString(toMatch.c_str(), toMatch.length());
    auto keyAccess = query->ast()->createNodeAttributeAccess(
        _varNode, StaticStrings::KeyString);
    // This condition cannot be fulfilled
    auto condition = query->ast()->createNodeBinaryOperator(
        aql::AstNodeType::NODE_TYPE_OPERATOR_BINARY_EQ, keyAccess, expectedKey);
    return std::make_unique<aql::Expression>(query->ast(), condition);
  }
};

TEST_F(InternalRestTraverserHandlerTest, it_can_provide_edges) {
  MockGraph g;
  g.addEdge(0, 1);
  g.addEdge(0, 2);
  g.addEdge(1, 2);

  arangodb::tests::mocks::MockDBServer server{"PRMR_0001", true, true};
  g.prepareServer(server);
  auto ctx = std::make_shared<arangodb::transaction::StandaloneContext>(
      server.getSystemDatabase(), transaction::OperationOriginTestCase{});
  auto queryString = arangodb::aql::QueryString(std::string_view("RETURN 1"));
  auto fakeQuery = std::make_shared<MockQuery>(ctx, queryString);
  try {
    fakeQuery->collections().add("s9880", AccessMode::Type::READ,
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
  auto const* cond = ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_EQ,
                                                   access, tmpIdNode);
  auto fromCondition = ast->createNodeNaryOperator(NODE_TYPE_OPERATOR_NARY_AND);
  fromCondition->addMember(cond);
  opts.addLookupInfo(fakeQuery->plan(), "s9880", StaticStrings::FromString,
                     fromCondition,
                     /*onlyEdgeIndexes*/ false, TRI_EDGE_OUT);

  aql::QueryRegistry queryRegistry{120};
  auto engineId = g.createEngine(server, opts, queryRegistry);

  VPackBuilder leased;
  leased.openObject();
  leased.add("keys", VPackValue(VPackValueType::Array));
  leased.add(VPackValue("v/0"));
  leased.close();  // 'keys' Array
  leased.add("depth", VPackValue(1));
  leased.add("batchSize", VPackValue(3));
  leased.close();  // base object

  arangodb::tests::PreparedRequestResponse prep{server.getSystemDatabase()};
  prep.setRequestType(arangodb::rest::RequestType::PUT);
  prep.addRestSuffix("traverser");
  prep.addSuffix("edge");
  prep.addSuffix(basics::StringUtils::itoa(engineId));
  prep.addBody(leased.slice());
  auto fakeRequest = prep.generateRequest();
  auto fakeResponse = std::make_unique<GeneralResponseMock>();
  auto handler =
      InternalRestTraverserHandler{server.server(), fakeRequest.release(),
                                   fakeResponse.release(), &queryRegistry};

  handler.executeAsync().waitAndGet();

  auto response =
      dynamic_cast<GeneralResponseMock*>(handler.stealResponse().release());

  EXPECT_EQ(response->responseCode(), ResponseCode::OK);
  LOG_DEVEL << inspection::json(response->_payload);

  LOG_DEVEL << "test end";
  // auto testee = makeProvider(g);
  // auto startVertex = g.vertexToId(0);
  // HashedStringRef hashedStart{startVertex.c_str(),
  //                             static_cast<uint32_t>(startVertex.length())};
  // Step s = testee.startVertex(hashedStart);

  // std::vector<std::string> results = {};
  // VPackBuilder builder;

  // testee.expand(s, 0, [&](Step next) {
  //   results.push_back(next.getVertex().getID().toString());
  // });

  // // Order is not guaranteed
  // ASSERT_EQ(results.size(), 2U);
  // if (results.at(0) == "v/1") {
  //   ASSERT_EQ(results.at(1), "v/2");
  // } else {
  //   ASSERT_EQ(results.at(0), "v/2");
  //   ASSERT_EQ(results.at(1), "v/1");
  // }
}

}  // namespace single_server_provider_test
}  // namespace tests
}  // namespace arangodb

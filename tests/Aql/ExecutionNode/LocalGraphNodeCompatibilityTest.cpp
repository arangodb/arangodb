////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/TraversalNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Query.h"
#include "Basics/StaticStrings.h"
#include "Containers/SmallVector.h"
#include "Mocks/Servers.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>

using namespace arangodb::aql;
using namespace arangodb::tests::mocks;

namespace arangodb::tests::aql {

namespace {

std::shared_ptr<velocypack::Builder> asOldCoordinatorNode(
    velocypack::Slice node) {
  velocypack::Builder overlay;
  {
    velocypack::ObjectBuilder guard(&overlay);
    overlay.add(StaticStrings::IsLocalGraphNode, velocypack::Value(true));
  }

  auto merged = std::make_shared<velocypack::Builder>(
      velocypack::Collection::merge(node, overlay.slice(),
                                    /*mergeObjects*/ true,
                                    /*nullMeansRemove*/ false));
  if (!merged->slice().get(StaticStrings::ProtoCollection).isNone()) {
    auto stripped =
        std::make_shared<velocypack::Builder>(velocypack::Collection::remove(
            merged->slice(),
            std::vector<std::string>{StaticStrings::ProtoCollection}));
    return stripped;
  }
  return merged;
}

}  // namespace

class LocalGraphNodeCompatibilityTest : public ::testing::Test {
 protected:
  MockAqlServer _server{};

  LocalGraphNodeCompatibilityTest() {
    auto& vocbase = _server.getSystemDatabase();
    auto graphs = velocypack::Parser::fromJson(
        R"({"name": "_graphs", "id": 99, "isSystem": true})");
    auto verts =
        velocypack::Parser::fromJson(R"({"name": "verts", "id": 100})");
    auto edges = velocypack::Parser::fromJson(
        R"({"name": "edges", "id": 101, "type": 3})");
    EXPECT_TRUE(vocbase.createCollection(graphs->slice()) != nullptr);
    EXPECT_TRUE(vocbase.createCollection(verts->slice()) != nullptr);
    auto edgeColl = vocbase.createCollection(edges->slice());
    EXPECT_TRUE(edgeColl != nullptr);

    // Traversal planning needs an edge index on the edge collection.
    auto edgeIndex = velocypack::Parser::fromJson(R"({"type": "edge"})");
    bool created = false;
    EXPECT_TRUE(
        edgeColl->createIndex(edgeIndex->slice(), created).waitAndGet() !=
        nullptr);
  }

  std::shared_ptr<Query> prepareTraversalQuery() {
    auto query = _server.createFakeQuery(
        /*activateTracing*/ false,
        "FOR v, e IN 1..3 OUTBOUND 'verts/1' edges RETURN v");
    return query;
  }

  static TraversalNode* findTraversalNode(ExecutionPlan* plan) {
    containers::SmallVector<ExecutionNode*, 8> nodes;
    plan->findNodesOfType(nodes, ExecutionNode::TRAVERSAL,
                          /*enterSubqueries*/ true);
    if (nodes.empty()) {
      return nullptr;
    }
    return ExecutionNode::castTo<TraversalNode*>(nodes.front());
  }
};

// Rolling-upgrade regression: an old coordinator serializes a local graph node
// with "isLocalGraphNode" but no "protoCollection".
// A new DB-Server must deserialize it instead of throwing.
// Solves COR-616
TEST_F(LocalGraphNodeCompatibilityTest, GeneratedSlice_missingProtoCollection) {
  auto query = prepareTraversalQuery();
  auto* plan = query->plan();
  ASSERT_NE(plan, nullptr);

  auto* original = findTraversalNode(plan);
  ASSERT_NE(original, nullptr);

  velocypack::Builder builder;
  static_cast<ExecutionNode*>(original)->toVelocyPack(
      builder, ExecutionNode::SERIALIZE_DETAILS);

  auto oldNode = asOldCoordinatorNode(builder.slice());
  ASSERT_TRUE(oldNode->slice().get(StaticStrings::IsLocalGraphNode).isTrue());
  ASSERT_TRUE(oldNode->slice().get(StaticStrings::ProtoCollection).isNone());

  // unique_ptr: fromVPackFactory hands back an owning pointer.
  std::unique_ptr<ExecutionNode> deserialized;
  ASSERT_NO_THROW({
    deserialized.reset(ExecutionNode::fromVPackFactory(plan, oldNode->slice()));
  });
  ASSERT_NE(deserialized, nullptr);
  EXPECT_EQ(deserialized->getType(), ExecutionNode::TRAVERSAL);
  EXPECT_NE(dynamic_cast<TraversalNode*>(deserialized.get()), nullptr);
}

}  // namespace arangodb::tests::aql

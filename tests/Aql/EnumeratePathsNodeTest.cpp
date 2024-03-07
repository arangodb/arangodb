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
/// @author Michael Hackstein
/// @author Copyright 2021, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Aql/Ast.h"
#include "Aql/EnumeratePathsNode.h"
#include "Aql/Query.h"
#include "Graph/BaseOptions.h"
#include "Graph/ShortestPathOptions.h"
#include "Mocks/Servers.h"

#include <memory>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::graph;
using namespace arangodb::tests::mocks;

namespace arangodb {
namespace tests {
namespace aql {
class EnumeratePathsNodeTest : public ::testing::Test {
  MockAqlServer _server{};
  std::shared_ptr<Query> _query{_server.createFakeQuery()};
  std::shared_ptr<Query> _otherQuery{_server.createFakeQuery()};
  std::string _startNode{"v/123"};
  AstNode* _source{nullptr};
  AstNode* _target{nullptr};
  AstNode* _direction{nullptr};
  AstNode* _graph{nullptr};

 public:
  EnumeratePathsNodeTest() {
    auto ast = _query->ast();
    _source =
        ast->createNodeValueString(_startNode.c_str(), _startNode.length());
    _target =
        ast->createNodeValueString(_startNode.c_str(), _startNode.length());
    _direction = ast->createNodeDirection(0, 1);
    AstNode* edges = ast->createNodeArray(0);
    _graph = ast->createNodeCollectionList(edges, _query->resolver());
  }

  ExecutionPlan* plan() const { return _query->plan(); }
  ExecutionPlan* otherPlan(bool emptyQuery = false) {
    if (emptyQuery) {
      // Let us start a new blank query
      _otherQuery = _server.createFakeQuery();
    }
    return _otherQuery->plan();
  }

  EnumeratePathsNode createNode(
      ExecutionNodeId id, std::unique_ptr<ShortestPathOptions> opts) const {
    return EnumeratePathsNode{plan(),
                              id,
                              &_query->vocbase(),
                              PathType::Type::KShortestPaths,
                              _direction,
                              _source,
                              _target,
                              _graph,
                              std::move(opts)};
  };

  std::unique_ptr<ShortestPathOptions> makeOptions() const {
    return std::make_unique<ShortestPathOptions>(*_query);
  }
};

TEST_F(EnumeratePathsNodeTest, clone_should_preserve_isSmart) {
  ExecutionNodeId id(12);
  auto opts = makeOptions();
  EnumeratePathsNode original = createNode(id, std::move(opts));
  ASSERT_EQ(original.id(), id);
  for (bool value : std::vector<bool>{false, true}) {
    original.setIsSmart(value);
    auto clone = ExecutionNode::castTo<EnumeratePathsNode*>(
        original.clone(plan(), false));
    EXPECT_NE(clone->id(), original.id());
    EXPECT_EQ(original.isSmart(), value);
    EXPECT_EQ(clone->isSmart(), value);
  }
}

TEST_F(EnumeratePathsNodeTest, clone_should_preserve_isDisjoint) {
  ExecutionNodeId id(12);
  auto opts = makeOptions();
  EnumeratePathsNode original = createNode(id, std::move(opts));
  ASSERT_EQ(original.id(), id);
  for (bool value : std::vector<bool>{false, true}) {
    original.setIsDisjoint(value);
    auto clone = ExecutionNode::castTo<EnumeratePathsNode*>(
        original.clone(plan(), false));
    EXPECT_NE(clone->id(), original.id());
    EXPECT_EQ(original.isDisjoint(), value);
    EXPECT_EQ(clone->isDisjoint(), value);
  }
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2021, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Aql/Ast.h"
#include "Aql/Expression.h"
#include "Aql/Query.h"
#include "Aql/TraversalNode.h"
#include "Graph/BaseOptions.h"
#include "Graph/TraverserOptions.h"
#include "Mocks/Servers.h"

#include <memory>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::graph;
using namespace arangodb::tests::mocks;

namespace arangodb {
namespace tests {
namespace aql {
class TraversalNodeTest : public ::testing::Test {
  MockAqlServer _server{};
  std::shared_ptr<Query> _query{_server.createFakeQuery()};
  std::shared_ptr<Query> _otherQuery{_server.createFakeQuery()};
  std::string _startNode{"v/123"};
  AstNode* _start{nullptr};
  AstNode* _direction{nullptr};
  AstNode* _graph{nullptr};

 public:
  TraversalNodeTest() {
    auto ast = _query->ast();
    _start = ast->createNodeValueString(_startNode.c_str(), _startNode.length());
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

  TraversalNode createNode(ExecutionNodeId id,
                           std::unique_ptr<traverser::TraverserOptions> opts) const {
    std::unique_ptr<Expression> pruneExp = nullptr;
    return TraversalNode{plan(), id,     &_query->vocbase(),  _direction,
                         _start, _graph, std::move(pruneExp), std::move(opts)};
  };

  std::unique_ptr<traverser::TraverserOptions> makeOptions() const {
    return std::make_unique<traverser::TraverserOptions>(*_query);
  }
};

TEST_F(TraversalNodeTest, clone_should_preserve_isSmart) {
  ExecutionNodeId id(12);
  auto opts = makeOptions();
  TraversalNode original = createNode(id, std::move(opts));
  ASSERT_EQ(original.id(), id);
  for (bool keepPlan : std::vector<bool>{false, true}) {
    for (bool value : std::vector<bool>{false, true}) {
      auto p = keepPlan ? plan() : otherPlan(true);
      original.setIsSmart(value);
      auto clone =
          ExecutionNode::castTo<TraversalNode*>(original.clone(p, false, !keepPlan));
      if (keepPlan) {
        EXPECT_NE(clone->id(), original.id()) << "Clone did keep the id";
      } else {
        EXPECT_EQ(clone->id(), original.id()) << "Clone did not keep the id";
      }
      EXPECT_EQ(original.isSmart(), value);
      EXPECT_EQ(clone->isSmart(), value);
    }
  }
}

TEST_F(TraversalNodeTest, clone_should_preserve_isDisjoint) {
  ExecutionNodeId id(12);
  auto opts = makeOptions();
  TraversalNode original = createNode(id, std::move(opts));
  ASSERT_EQ(original.id(), id);
  for (bool keepPlan : std::vector<bool>{false, true}) {
    for (bool value : std::vector<bool>{false, true}) {
      auto p = keepPlan ? plan() : otherPlan(true);
      original.setIsDisjoint(value);
      auto clone =
          ExecutionNode::castTo<TraversalNode*>(original.clone(p, false, !keepPlan));
      if (keepPlan) {
        EXPECT_NE(clone->id(), original.id()) << "Clone did keep the id";
      } else {
        EXPECT_EQ(clone->id(), original.id()) << "Clone did not keep the id";
      }
      EXPECT_EQ(original.isDisjoint(), value);
      EXPECT_EQ(clone->isDisjoint(), value);
    }
  }
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb

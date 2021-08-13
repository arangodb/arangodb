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
 public:
  MockAqlServer _server{};
  std::unique_ptr<Query> _query{_server.createFakeQuery()};
  std::string _startNode{"v/123"};
  AstNode* _start{nullptr};
  AstNode* _direction{nullptr};
  AstNode* _graph{nullptr};

 public:
  TraversalNodeTest() {
    auto ast = _query->ast();
    _start = ast->createNodeValueString(_startNode.c_str(), _startNode.length());
    _direction = ast->createNodeDirection(0,1);
    AstNode* edges = ast->createNodeArray(0);
    _graph = ast->createNodeCollectionList(edges, _query->resolver());
  }

  
};

TEST_F(TraversalNodeTest, cloneHelper) {
  auto opts = std::make_unique<traverser::TraverserOptions>(*_query);
  ExecutionNodeId id(12);
  std::unique_ptr<Expression> pruneExp = nullptr;
  TraversalNode testee{
      _query->plan(), id,    &_query->vocbase(),  _direction,
      _start,          _graph, std::move(pruneExp), std::move(opts)};
  ASSERT_EQ(testee.id(), id);
}
}  // namespace aql
}  // namespace tests
}  // namespace arangodb

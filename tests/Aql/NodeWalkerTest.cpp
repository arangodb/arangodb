////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Heiko Kernbach
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "AqlExecutorTestCase.h"

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb::tests::aql {
struct TestWalker final
    : public WalkerWorker<ExecutionNode, WalkerUniqueness::NonUnique> {
  std::vector<std::pair<ExecutionNodeId, bool>> _visitedNode{};

  bool before(ExecutionNode* en) final {
    _visitedNode.emplace_back(std::make_pair(en->id(), false));
    return true;
  }

  void after(ExecutionNode* en) final {
    _visitedNode.emplace_back(std::make_pair(en->id(), true));
  }
};

class NodeWalkerTest : public AqlExecutorTestCase<false> {};

TEST_F(NodeWalkerTest, simple_query_walker) {
  auto first = generateNodeDummy(ExecutionNode::NodeType::SINGLETON);
  auto second = generateNodeDummy(ExecutionNode::NodeType::ENUMERATE_LIST);
  auto third = generateNodeDummy(ExecutionNode::NodeType::RETURN);

  second->addDependency(first);
  third->addDependency(second);
  TestWalker walker{};

  third->walk(walker);
  EXPECT_EQ(walker._visitedNode.at(0), std::make_pair(first->id(), false));
  EXPECT_EQ(walker._visitedNode.at(1), std::make_pair(second->id(), false));
  EXPECT_EQ(walker._visitedNode.at(2), std::make_pair(third->id(), false));

  EXPECT_EQ(walker._visitedNode.at(3), std::make_pair(third->id(), true));
  EXPECT_EQ(walker._visitedNode.at(4), std::make_pair(second->id(), true));
  EXPECT_EQ(walker._visitedNode.at(5), std::make_pair(first->id(), true));
}
}  // namespace arangodb::tests::aql

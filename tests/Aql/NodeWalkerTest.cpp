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
    return false;
  }

  void after(ExecutionNode* en) final {
    _visitedNode.emplace_back(std::make_pair(en->id(), true));
  }
};

class NodeWalkerTest : public AqlExecutorTestCase<false> {};

struct ExpectedVisits {
  std::vector<std::pair<ExecutionNodeId, bool>> _expectedVisitedNodesBefore{};
  std::vector<std::pair<ExecutionNodeId, bool>> _expectedVisitedNodesAfter{};

  auto addExpectedBefore(std::vector<ExecutionNodeId> before) -> void {
    for (auto const& node : before) {
      _expectedVisitedNodesBefore.emplace_back(std::make_pair(node, false));
    }
  }

  auto addExpectedAfter(std::vector<ExecutionNodeId> after) -> void {
    for (auto const& node : after) {
      _expectedVisitedNodesAfter.emplace_back(std::make_pair(node, true));
    }
  }

  auto verify(std::vector<std::pair<ExecutionNodeId, bool>> actualNodes)
      -> void {
    EXPECT_EQ(
        _expectedVisitedNodesBefore.size() + _expectedVisitedNodesAfter.size(),
        actualNodes.size());

    size_t position = 0;
    for (auto expectedBefore : _expectedVisitedNodesBefore) {
      EXPECT_EQ(actualNodes.at(position), expectedBefore);
      position++;
    }
    for (auto expectedAfter : _expectedVisitedNodesAfter) {
      EXPECT_EQ(actualNodes.at(position), expectedAfter);
      position++;
    }
    EXPECT_EQ(position, actualNodes.size());
  }
};

TEST_F(NodeWalkerTest, simple_query_walker) {
  auto singletonNode = generateNodeDummy(ExecutionNode::NodeType::SINGLETON);
  auto enumerateNode =
      generateNodeDummy(ExecutionNode::NodeType::ENUMERATE_LIST);
  auto returnNode = generateNodeDummy(ExecutionNode::NodeType::RETURN);

  enumerateNode->addDependency(singletonNode);
  returnNode->addDependency(enumerateNode);
  TestWalker walker{};

  returnNode->walk(walker);

  ExpectedVisits tester;
  tester.addExpectedBefore(
      {returnNode->id(), enumerateNode->id(), singletonNode->id()});

  tester.addExpectedAfter(
      {singletonNode->id(), enumerateNode->id(), returnNode->id()});

  tester.verify(walker._visitedNode);
}

TEST_F(NodeWalkerTest, simple_query_walker_flatten) {
  auto singleton = generateNodeDummy(ExecutionNode::NodeType::SINGLETON);
  auto scatter = generateNodeDummy(ExecutionNode::NodeType::SCATTER);

  auto remote1 = generateNodeDummy(ExecutionNode::NodeType::REMOTE);
  auto enumerate1 =
      generateNodeDummy(ExecutionNode::NodeType::ENUMERATE_COLLECTION);
  auto innerRemote1 = generateNodeDummy(ExecutionNode::NodeType::REMOTE);

  auto remote2 = generateNodeDummy(ExecutionNode::NodeType::REMOTE);
  auto enumerate2 =
      generateNodeDummy(ExecutionNode::NodeType::ENUMERATE_COLLECTION);
  auto innerRemote2 = generateNodeDummy(ExecutionNode::NodeType::REMOTE);

  auto gather = generateNodeDummy(ExecutionNode::NodeType::GATHER);
  auto returnNode = generateNodeDummy(ExecutionNode::NodeType::RETURN);

  scatter->addDependency(singleton);

  remote1->addDependency(scatter);
  enumerate1->addDependency(remote1);
  innerRemote1->addDependency(enumerate1);

  remote2->addDependency(scatter);
  enumerate2->addDependency(remote2);
  innerRemote2->addDependency(enumerate2);

  gather->addDependency(innerRemote1);
  gather->addDependency(innerRemote2);
  returnNode->addDependency(gather);

  TestWalker walker{};

  returnNode->walk(walker);

  ExpectedVisits tester;
  tester.addExpectedBefore({returnNode->id(), gather->id(), innerRemote1->id(),
                            enumerate1->id(), remote1->id(), scatter->id(),
                            innerRemote2->id(), enumerate2->id(), remote2->id(),
                            singleton->id()});

  tester.addExpectedAfter({returnNode->id()});

  tester.verify(walker._visitedNode);

  /*
  EXPECT_EQ(walker._visitedNode.at(0), std::make_pair(third->id(), false));
  EXPECT_EQ(walker._visitedNode.at(1), std::make_pair(second->id(), false));
  EXPECT_EQ(walker._visitedNode.at(2), std::make_pair(first->id(), false));

  EXPECT_EQ(walker._visitedNode.at(3), std::make_pair(first->id(), true));
  EXPECT_EQ(walker._visitedNode.at(4), std::make_pair(second->id(), true));
  EXPECT_EQ(walker._visitedNode.at(5), std::make_pair(third->id(), true));*/
}

}  // namespace arangodb::tests::aql

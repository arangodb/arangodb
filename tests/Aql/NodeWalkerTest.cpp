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
  std::vector<std::pair<ExecutionNodeId, bool>> _expectedVisitedNodes;

  auto addExpectedBackAndForth(std::vector<ExecutionNode const*> nodes)
      -> void {
    for (auto node : nodes) {
      _expectedVisitedNodes.emplace_back(std::make_pair(node->id(), false));
    }

    for (auto it = nodes.rbegin(); it != nodes.rend(); ++it) {
      _expectedVisitedNodes.emplace_back(std::make_pair((*it)->id(), false));
    }
  }

  auto addExpectedBefore(std::vector<ExecutionNode const*> before) -> void {
    for (auto node : before) {
      _expectedVisitedNodes.emplace_back(std::make_pair(node->id(), false));
    }
  }

  auto addExpectedAfter(std::vector<ExecutionNode const*> after) -> void {
    for (auto node : after) {
      _expectedVisitedNodes.emplace_back(std::make_pair(node->id(), true));
    }
  }

  auto verify(std::vector<std::pair<ExecutionNodeId, bool>> actualNodes)
      -> void {
    EXPECT_EQ(_expectedVisitedNodes.size(), actualNodes.size());

    size_t position = 0;
    for (auto expectedBefore : _expectedVisitedNodes) {
      EXPECT_EQ(actualNodes.at(position), expectedBefore);
      position++;
    }
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
  tester.addExpectedBefore({returnNode, enumerateNode, singletonNode});
  tester.addExpectedAfter({singletonNode, enumerateNode, returnNode});

  tester.verify(walker._visitedNode);
}

TEST_F(NodeWalkerTest, simple_query_walker_multiple_dependency) {
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
  tester.addExpectedBefore({returnNode, gather, innerRemote1, enumerate1,
                            remote1, scatter, singleton});

  tester.addExpectedAfter(
      {singleton, scatter, remote1, enumerate1, innerRemote1});

  tester.addExpectedBefore(
      {innerRemote2, enumerate2, remote2, scatter, singleton});

  tester.addExpectedAfter({singleton, scatter, remote2, enumerate2,
                           innerRemote2, gather, returnNode});

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

  returnNode->flatWalk(walker);

  ExpectedVisits tester;

  tester.addExpectedBackAndForth({returnNode, gather, innerRemote1, enumerate1,
                                  remote1, innerRemote2, enumerate2, remote2,
                                  scatter, singleton});

  tester.verify(walker._visitedNode);
}

}  // namespace arangodb::tests::aql

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
  std::vector<std::tuple<uint64_t, std::string, bool>> _visitedNode{};

  bool before(ExecutionNode* en) final {
    _visitedNode.emplace_back(
        std::make_tuple(en->id().id(), en->getTypeString(), false));
    return false;
  }

  void after(ExecutionNode* en) final {
    _visitedNode.emplace_back(
        std::make_tuple(en->id().id(), en->getTypeString(), true));
  }
};

class NodeWalkerTest : public AqlExecutorTestCase<false> {
 protected:
  // Builds a linear chain of Execution Nodes using the given types in order.
  // If parent is not a nullptr the beginning of the ExecutionNodeChain will be
  // added as dependency to parent. if dependency is not nullptr the end of the
  // ExecutionNodeChain will add it as a dependency.
  auto buildBranch(std::vector<ExecutionNode::NodeType> const& types,
                   ExecutionNode* parent, ExecutionNode* dependency)
      -> std::vector<ExecutionNode*> {
    std::vector<ExecutionNode*> result{};
    TRI_ASSERT(!types.empty());
    for (auto t : types) {
      auto node = generateNodeDummy(t);
      result.emplace_back(node);
      if (parent != nullptr) {
        parent->addDependency(node);
      }
      parent = node;
    }
    if (parent != nullptr && dependency != nullptr) {
      parent->addDependency(dependency);
    }
    return result;
  }
};

struct ExpectedVisits {
  std::vector<std::tuple<uint64_t, std::string, bool>> _expectedVisitedNodes;

  auto addExpectedBackAndForth(std::vector<ExecutionNode*> const& nodes)
      -> void {
    for (auto node : nodes) {
      _expectedVisitedNodes.emplace_back(
          std::make_tuple(node->id().id(), node->getTypeString(), false));
    }

    for (auto it = nodes.rbegin(); it != nodes.rend(); ++it) {
      _expectedVisitedNodes.emplace_back(
          std::make_tuple((*it)->id().id(), (*it)->getTypeString(), true));
    }
  }

  auto addExpectedBefore(std::vector<ExecutionNode*> const& before) -> void {
    for (auto node : before) {
      _expectedVisitedNodes.emplace_back(
          std::make_tuple(node->id().id(), node->getTypeString(), false));
    }
  }

  auto addExpectedAfter(std::vector<ExecutionNode*> const& after) -> void {
    for (auto node : after) {
      _expectedVisitedNodes.emplace_back(
          std::make_tuple(node->id().id(), node->getTypeString(), true));
    }
  }

  auto addExpectedAfterReverse(std::vector<ExecutionNode*> const& after)
      -> void {
    for (auto it = after.rbegin(); it != after.rend(); ++it) {
      _expectedVisitedNodes.emplace_back(
          std::make_tuple((*it)->id().id(), (*it)->getTypeString(), true));
    }
  }

  auto verify(std::vector<std::tuple<uint64_t, std::string, bool>> actualNodes)
      -> void {
    EXPECT_EQ(_expectedVisitedNodes.size(), actualNodes.size());

    size_t position = 0;
    for (auto const& expectedBefore : _expectedVisitedNodes) {
      EXPECT_EQ(actualNodes.at(position), expectedBefore)
          << "Position is " << position;
      position++;
    }
  }
};

TEST_F(NodeWalkerTest, simple_query_walker) {
  auto singleBranch = buildBranch(
      {ExecutionNode::NodeType::RETURN, ExecutionNode::NodeType::ENUMERATE_LIST,
       ExecutionNode::NodeType::SINGLETON},
      nullptr, nullptr);

  TestWalker walker{};

  singleBranch.front()->walk(walker);

  ExpectedVisits tester;
  tester.addExpectedBackAndForth(singleBranch);

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

TEST_F(NodeWalkerTest, simple_query_walker_flatten_cluster_all) {
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

  returnNode->flatWalk(walker, false);

  ExpectedVisits tester;

  tester.addExpectedBackAndForth({returnNode, gather, innerRemote1, enumerate1,
                                  remote1, innerRemote2, enumerate2, remote2,
                                  scatter, singleton});

  tester.verify(walker._visitedNode);
}

TEST_F(NodeWalkerTest, simple_query_walker_flatten_async_all) {
  auto singleton = generateNodeDummy(ExecutionNode::NodeType::SINGLETON);
  auto mutex = generateNodeDummy(ExecutionNode::NodeType::MUTEX);

  auto consumer1 =
      generateNodeDummy(ExecutionNode::NodeType::DISTRIBUTE_CONSUMER);
  auto enumerate1 =
      generateNodeDummy(ExecutionNode::NodeType::ENUMERATE_COLLECTION);
  auto async1 = generateNodeDummy(ExecutionNode::NodeType::ASYNC);

  auto consumer2 =
      generateNodeDummy(ExecutionNode::NodeType::DISTRIBUTE_CONSUMER);
  auto enumerate2 =
      generateNodeDummy(ExecutionNode::NodeType::ENUMERATE_COLLECTION);
  auto async2 = generateNodeDummy(ExecutionNode::NodeType::ASYNC);

  auto gather = generateNodeDummy(ExecutionNode::NodeType::GATHER);
  auto returnNode = generateNodeDummy(ExecutionNode::NodeType::RETURN);

  mutex->addDependency(singleton);

  consumer1->addDependency(mutex);
  enumerate1->addDependency(consumer1);
  async1->addDependency(enumerate1);

  consumer2->addDependency(mutex);
  enumerate2->addDependency(consumer2);
  async2->addDependency(enumerate2);

  gather->addDependency(async1);
  gather->addDependency(async2);
  returnNode->addDependency(gather);

  TestWalker walker{};

  returnNode->flatWalk(walker, false);

  ExpectedVisits tester;

  tester.addExpectedBackAndForth({returnNode, gather, async1, enumerate1,
                                  consumer1, async2, enumerate2, consumer2,
                                  mutex, singleton});

  tester.verify(walker._visitedNode);
}

TEST_F(NodeWalkerTest, simple_query_walker_flatten_cluster_async) {
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

  returnNode->flatWalk(walker, true);

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

TEST_F(NodeWalkerTest, simple_query_walker_flatten_async_async) {
  auto singleton = generateNodeDummy(ExecutionNode::NodeType::SINGLETON);
  auto mutex = generateNodeDummy(ExecutionNode::NodeType::MUTEX);

  auto consumer1 =
      generateNodeDummy(ExecutionNode::NodeType::DISTRIBUTE_CONSUMER);
  auto enumerate1 =
      generateNodeDummy(ExecutionNode::NodeType::ENUMERATE_COLLECTION);
  auto async1 = generateNodeDummy(ExecutionNode::NodeType::ASYNC);

  auto consumer2 =
      generateNodeDummy(ExecutionNode::NodeType::DISTRIBUTE_CONSUMER);
  auto enumerate2 =
      generateNodeDummy(ExecutionNode::NodeType::ENUMERATE_COLLECTION);
  auto async2 = generateNodeDummy(ExecutionNode::NodeType::ASYNC);

  auto gather = generateNodeDummy(ExecutionNode::NodeType::GATHER);
  auto returnNode = generateNodeDummy(ExecutionNode::NodeType::RETURN);

  mutex->addDependency(singleton);

  consumer1->addDependency(mutex);
  enumerate1->addDependency(consumer1);
  async1->addDependency(enumerate1);

  consumer2->addDependency(mutex);
  enumerate2->addDependency(consumer2);
  async2->addDependency(enumerate2);

  gather->addDependency(async1);
  gather->addDependency(async2);
  returnNode->addDependency(gather);

  TestWalker walker{};

  returnNode->flatWalk(walker, true);

  ExpectedVisits tester;

  tester.addExpectedBackAndForth({returnNode, gather, async1, enumerate1,
                                  consumer1, async2, enumerate2, consumer2,
                                  mutex, singleton});

  tester.verify(walker._visitedNode);
}

TEST_F(NodeWalkerTest, simple_query_walker_nested_flatten_all) {
  auto topCoordinatorPart = buildBranch(
      {ExecutionNode::NodeType::SCATTER, ExecutionNode::NodeType::SINGLETON},
      nullptr, nullptr);
  auto scatter = topCoordinatorPart.front();

  auto queryStart = buildBranch(
      {ExecutionNode::NodeType::RETURN, ExecutionNode::NodeType::GATHER},
      nullptr, nullptr);
  auto gather = queryStart.back();

  auto firstDBServerToCoordinator = buildBranch(
      {ExecutionNode::NodeType::MUTEX, ExecutionNode::NodeType::REMOTE},
      nullptr, scatter);
  auto secondDBServerToCoordinator = buildBranch(
      {ExecutionNode::NodeType::MUTEX, ExecutionNode::NodeType::REMOTE},
      nullptr, scatter);

  auto firstCoordinatorToDBServer = buildBranch(
      {ExecutionNode::NodeType::REMOTE, ExecutionNode::NodeType::GATHER},
      gather, nullptr);
  auto secondCoordinatorToDBServer = buildBranch(
      {ExecutionNode::NodeType::REMOTE, ExecutionNode::NodeType::GATHER},
      gather, nullptr);

  auto localMutex_1 = firstDBServerToCoordinator.front();
  auto localGather_1 = firstCoordinatorToDBServer.back();
  auto localBranch_1_1 =
      buildBranch({ExecutionNode::NodeType::ASYNC,
                   ExecutionNode::NodeType::ENUMERATE_COLLECTION,
                   ExecutionNode::NodeType::DISTRIBUTE_CONSUMER},
                  localGather_1, localMutex_1);
  auto localBranch_1_2 =
      buildBranch({ExecutionNode::NodeType::ASYNC,
                   ExecutionNode::NodeType::ENUMERATE_COLLECTION,
                   ExecutionNode::NodeType::DISTRIBUTE_CONSUMER},
                  localGather_1, localMutex_1);

  auto localMutex_2 = secondDBServerToCoordinator.front();
  auto localGather_2 = secondCoordinatorToDBServer.back();
  auto localBranch_2_1 =
      buildBranch({ExecutionNode::NodeType::ASYNC,
                   ExecutionNode::NodeType::ENUMERATE_COLLECTION,
                   ExecutionNode::NodeType::DISTRIBUTE_CONSUMER},
                  localGather_2, localMutex_2);
  auto localBranch_2_2 =
      buildBranch({ExecutionNode::NodeType::ASYNC,
                   ExecutionNode::NodeType::ENUMERATE_COLLECTION,
                   ExecutionNode::NodeType::DISTRIBUTE_CONSUMER},
                  localGather_2, localMutex_2);
  TestWalker walker{};

  queryStart.front()->flatWalk(walker, false);

  ExpectedVisits tester;
  // We will first visit all branches in before, and stack them
  tester.addExpectedBefore(queryStart);
  // Visit the first server first
  tester.addExpectedBefore(firstCoordinatorToDBServer);
  // Visit each branch before completing the server
  tester.addExpectedBefore(localBranch_1_1);
  tester.addExpectedBefore(localBranch_1_2);
  // Now add snippet to go back to coordinator
  tester.addExpectedBefore(firstDBServerToCoordinator);
  // Before doing the coordiantor we have to branch back to second dbserver
  tester.addExpectedBefore(secondCoordinatorToDBServer);
  // Visit each branch before completing the server
  tester.addExpectedBefore(localBranch_2_1);
  tester.addExpectedBefore(localBranch_2_2);
  // Now add snippet to go back to coordinator
  tester.addExpectedBefore(secondDBServerToCoordinator);
  // Finish the query
  tester.addExpectedBefore(topCoordinatorPart);

  // Just the same as above, just reversed ordering!

  // Finish the query
  tester.addExpectedAfterReverse(topCoordinatorPart);
  // Now add snippet to go back to coordinator
  tester.addExpectedAfterReverse(secondDBServerToCoordinator);
  // Visit each branch before completing the server
  tester.addExpectedAfterReverse(localBranch_2_2);
  tester.addExpectedAfterReverse(localBranch_2_1);
  // Before doing the coordinator we have to branch back to second dbserver
  tester.addExpectedAfterReverse(secondCoordinatorToDBServer);
  // Now add snippet to go back to coordinator
  tester.addExpectedAfterReverse(firstDBServerToCoordinator);
  // Visit each branch before completing the server
  tester.addExpectedAfterReverse(localBranch_1_2);
  tester.addExpectedAfterReverse(localBranch_1_1);
  // Visit the first server first
  tester.addExpectedAfterReverse(firstCoordinatorToDBServer);
  // We will first visit all branches in before, and stack them
  tester.addExpectedAfterReverse(queryStart);

  tester.verify(walker._visitedNode);
}

}  // namespace arangodb::tests::aql

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "../Mocks/Servers.h"
#include "Aql/Ast.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Query.h"
#include "Aql/SubqueryEndExecutionNode.h"
#include "Aql/SubqueryStartExecutionNode.h"
#include "Logger/LogMacros.h"
#include "gtest/gtest.h"
#include "velocypack/Builder.h"
#include "velocypack/velocypack-aliases.h"

using namespace arangodb::aql;

namespace arangodb::tests::aql {

class ExecutionNodeTest : public ::testing::Test {
 protected:
  mocks::MockAqlServer server;
  std::shared_ptr<arangodb::aql::Query> fakedQuery;
  Ast ast;
  ExecutionPlan plan;

 public:
  ExecutionNodeTest()
      : fakedQuery(server.createFakeQuery()),
        ast(*fakedQuery.get()),
        plan(&ast, false) {}
};

TEST_F(ExecutionNodeTest, allToVelocyPack_roundtrip) {
  auto initNode = [](ExecutionNode* node) {
    node->setVarsUsedLater({{}});
    node->setVarsValid({{}});
    node->setRegsToKeep({{}});
  };

  auto singletonNode =
      std::make_unique<SingletonNode>(&plan, ExecutionNodeId{1});
  initNode(singletonNode.get());

  auto returnNode = std::make_unique<ReturnNode>(
      &plan, ExecutionNodeId{0}, ast.variables()->createTemporaryVariable());
  returnNode->addDependency(singletonNode.get());
  initNode(returnNode.get());

  VPackBuilder builder;
  returnNode->allToVelocyPack(builder, ExecutionNode::SERIALIZE_DETAILS);

  auto slice = builder.slice();
  EXPECT_TRUE(slice.isArray());
  EXPECT_EQ(2, slice.length());

  auto singletonFromVPack = std::make_unique<SingletonNode>(&plan, slice[0]);
  auto returnFromVPack = std::make_unique<ReturnNode>(&plan, slice[1]);

  VPackSlice dependencies = slice[1]["dependencies"];
  ASSERT_TRUE(dependencies.isArray());
  ASSERT_EQ(1, dependencies.length());
  ASSERT_EQ(singletonFromVPack->id(),
            ExecutionNodeId{dependencies[0].getUInt()});

  returnFromVPack->addDependency(singletonFromVPack.get());
  ASSERT_TRUE(singletonNode->isEqualTo(*singletonFromVPack));
  ASSERT_TRUE(returnNode->isEqualTo(*returnFromVPack));
}

TEST_F(ExecutionNodeTest, start_node_velocypack_roundtrip) {
  VPackBuilder builder;

  std::unique_ptr<SubqueryStartNode> node, nodeFromVPack;

  node =
      std::make_unique<SubqueryStartNode>(&plan, ExecutionNodeId{0}, nullptr);
  node->setVarsUsedLater({{}});
  node->setVarsValid({{}});
  node->setRegsToKeep({{}});

  node->toVelocyPack(builder, ExecutionNode::SERIALIZE_DETAILS);

  nodeFromVPack = std::make_unique<SubqueryStartNode>(&plan, builder.slice());

  ASSERT_TRUE(node->isEqualTo(*nodeFromVPack));
}

TEST_F(ExecutionNodeTest, start_node_not_equal_different_id) {
  std::unique_ptr<SubqueryStartNode> node1, node2;

  node1 =
      std::make_unique<SubqueryStartNode>(&plan, ExecutionNodeId{0}, nullptr);
  node2 =
      std::make_unique<SubqueryStartNode>(&plan, ExecutionNodeId{1}, nullptr);

  ASSERT_FALSE(node1->isEqualTo(*node2));
}

TEST_F(ExecutionNodeTest, end_node_velocypack_roundtrip_no_invariable) {
  VPackBuilder builder;

  Variable outvar("name", 1, false);

  std::unique_ptr<SubqueryEndNode> node, nodeFromVPack;

  node = std::make_unique<SubqueryEndNode>(&plan, ExecutionNodeId{0}, nullptr,
                                           &outvar);
  node->setVarsUsedLater({{}});
  node->setVarsValid({{}});
  node->setRegsToKeep({{}});

  node->toVelocyPack(builder, ExecutionNode::SERIALIZE_DETAILS);

  nodeFromVPack = std::make_unique<SubqueryEndNode>(&plan, builder.slice());

  ASSERT_TRUE(node->isEqualTo(*nodeFromVPack));
}

TEST_F(ExecutionNodeTest, end_node_velocypack_roundtrip_invariable) {
  VPackBuilder builder;

  Variable outvar("name", 1, false);
  Variable invar("otherName", 2, false);

  std::unique_ptr<SubqueryEndNode> node, nodeFromVPack;

  node = std::make_unique<SubqueryEndNode>(&plan, ExecutionNodeId{0}, &invar,
                                           &outvar);
  node->setVarsUsedLater({{}});
  node->setVarsValid({{}});
  node->setRegsToKeep({{}});

  node->toVelocyPack(builder, ExecutionNode::SERIALIZE_DETAILS);

  nodeFromVPack = std::make_unique<SubqueryEndNode>(&plan, builder.slice());

  ASSERT_TRUE(node->isEqualTo(*nodeFromVPack));
}

TEST_F(ExecutionNodeTest, end_node_not_equal_different_id) {
  std::unique_ptr<SubqueryEndNode> node1, node2;

  Variable outvar("name", 1, false);

  node1 = std::make_unique<SubqueryEndNode>(&plan, ExecutionNodeId{0}, nullptr,
                                            &outvar);
  node2 = std::make_unique<SubqueryEndNode>(&plan, ExecutionNodeId{1}, nullptr,
                                            &outvar);

  ASSERT_FALSE(node1->isEqualTo(*node2));
}

TEST_F(ExecutionNodeTest, end_node_not_equal_invariable_null_vs_non_null) {
  std::unique_ptr<SubqueryEndNode> node1, node2;

  Variable outvar("name", 1, false);
  Variable invar("otherName", 2, false);

  node1 = std::make_unique<SubqueryEndNode>(&plan, ExecutionNodeId{0}, &invar,
                                            &outvar);
  node2 = std::make_unique<SubqueryEndNode>(&plan, ExecutionNodeId{1}, nullptr,
                                            &outvar);

  ASSERT_FALSE(node1->isEqualTo(*node2));
  // Bidirectional nullptr check
  ASSERT_FALSE(node2->isEqualTo(*node1));
}

TEST_F(ExecutionNodeTest, end_node_not_equal_invariable_differ) {
  std::unique_ptr<SubqueryEndNode> node1, node2;

  Variable outvar("name", 1, false);
  Variable invar("otherName", 2, false);
  Variable otherInvar("invalidName", 3, false);

  node1 = std::make_unique<SubqueryEndNode>(&plan, ExecutionNodeId{0}, &invar,
                                            &outvar);
  node2 = std::make_unique<SubqueryEndNode>(&plan, ExecutionNodeId{1},
                                            &otherInvar, &outvar);

  ASSERT_FALSE(node1->isEqualTo(*node2));
  // Bidirectional check
  ASSERT_FALSE(node2->isEqualTo(*node1));
}

TEST_F(ExecutionNodeTest, end_node_not_equal_outvariable_differ) {
  std::unique_ptr<SubqueryEndNode> node1, node2;

  Variable outvar("name", 1, false);
  Variable otherOutvar("otherName", 2, false);

  node1 = std::make_unique<SubqueryEndNode>(&plan, ExecutionNodeId{0}, nullptr,
                                            &outvar);
  node2 = std::make_unique<SubqueryEndNode>(&plan, ExecutionNodeId{1}, nullptr,
                                            &otherOutvar);

  ASSERT_FALSE(node1->isEqualTo(*node2));
  // Bidirectional check
  ASSERT_FALSE(node2->isEqualTo(*node1));
}

}  // namespace arangodb::tests::aql

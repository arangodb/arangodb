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
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/ModificationOptions.h"
#include "Aql/Query.h"
#include "Aql/ExecutionNode/EnumerateListNode.h"
#include "Aql/ExecutionNode/InsertNode.h"
#include "Aql/ExecutionNode/ReturnNode.h"
#include "Aql/ExecutionNode/SingletonNode.h"
#include "Aql/ExecutionNode/SortNode.h"
#include "Aql/ExecutionNode/SubqueryEndExecutionNode.h"
#include "Aql/ExecutionNode/SubqueryStartExecutionNode.h"
#include "Aql/ExecutionNode/UpdateNode.h"
#include "Aql/ExecutionNode/UpsertNode.h"
#include "Basics/GlobalResourceMonitor.h"
#include "Basics/ResourceUsage.h"

#include "velocypack/Builder.h"

#include "../Mocks/Servers.h"

using namespace arangodb::aql;

namespace arangodb::tests::aql {

class ExecutionNodeTest : public ::testing::Test {
 protected:
  mocks::MockAqlServer server;
  std::shared_ptr<arangodb::aql::Query> fakedQuery;
  Ast ast;
  ExecutionPlan plan;
  arangodb::GlobalResourceMonitor global{};
  arangodb::ResourceMonitor resourceMonitor{global};

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

  Variable outvar("name", 1, false, resourceMonitor);

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

  Variable outvar("name", 1, false, resourceMonitor);
  Variable invar("otherName", 2, false, resourceMonitor);

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

  Variable outvar("name", 1, false, resourceMonitor);

  node1 = std::make_unique<SubqueryEndNode>(&plan, ExecutionNodeId{0}, nullptr,
                                            &outvar);
  node2 = std::make_unique<SubqueryEndNode>(&plan, ExecutionNodeId{1}, nullptr,
                                            &outvar);

  ASSERT_FALSE(node1->isEqualTo(*node2));
}

TEST_F(ExecutionNodeTest, end_node_not_equal_invariable_null_vs_non_null) {
  std::unique_ptr<SubqueryEndNode> node1, node2;

  Variable outvar("name", 1, false, resourceMonitor);
  Variable invar("otherName", 2, false, resourceMonitor);

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

  Variable outvar("name", 1, false, resourceMonitor);
  Variable invar("otherName", 2, false, resourceMonitor);
  Variable otherInvar("invalidName", 3, false, resourceMonitor);

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

  Variable outvar("name", 1, false, resourceMonitor);
  Variable otherOutvar("otherName", 2, false, resourceMonitor);

  node1 = std::make_unique<SubqueryEndNode>(&plan, ExecutionNodeId{0}, nullptr,
                                            &outvar);
  node2 = std::make_unique<SubqueryEndNode>(&plan, ExecutionNodeId{1}, nullptr,
                                            &otherOutvar);

  ASSERT_FALSE(node1->isEqualTo(*node2));
  // Bidirectional check
  ASSERT_FALSE(node2->isEqualTo(*node1));
}

TEST_F(ExecutionNodeTest, clone_copies_execution_flags) {
  auto* node = plan.createNode<SingletonNode>(&plan, plan.nextId());
  node->enableCallstackSplit();
  node->setIsAsyncPrefetchEnabled(true);
  ASSERT_EQ(1, plan.asyncPrefetchNodes());

  auto* cloned = node->clone(&plan, false);
  EXPECT_NE(node->id(), cloned->id());
  EXPECT_TRUE(cloned->isCallstackSplitEnabled());
  EXPECT_TRUE(cloned->isAsyncPrefetchEnabled());
  EXPECT_EQ(2, plan.asyncPrefetchNodes());
}

TEST_F(ExecutionNodeTest, sort_node_clone_copies_limit_and_grouping) {
  auto* var = ast.variables()->createTemporaryVariable();
  SortElementVector elements;
  elements.emplace_back(SortElement::create(var, true));
  elements.emplace_back(SortElement::create(var, false));

  auto* node = plan.createNode<SortNode>(&plan, plan.nextId(), elements,
                                         /*stable*/ false);
  node->setLimit(10);
  node->setGroupedElements(1);
  node->dontReinsertInCluster();

  auto* cloned = ExecutionNode::castTo<SortNode*>(node->clone(&plan, false));
  EXPECT_EQ(10, cloned->limit());
  EXPECT_EQ(SortNode::SorterType::kConstrainedHeap, cloned->sorterType());
  EXPECT_FALSE(cloned->reinsertInCluster());

  // without a limit, the grouped sorter is used
  node->setLimit(0);
  cloned = ExecutionNode::castTo<SortNode*>(node->clone(&plan, false));
  EXPECT_EQ(SortNode::SorterType::kGrouped, cloned->sorterType());
}

TEST_F(ExecutionNodeTest, enumerate_list_node_clone_keeps_object_mode) {
  auto* inVar = ast.variables()->createTemporaryVariable();
  auto* outVar = ast.variables()->createTemporaryVariable();
  auto* keyVar = ast.variables()->createTemporaryVariable();
  auto* valueVar = ast.variables()->createTemporaryVariable();

  auto* node =
      plan.createNode<EnumerateListNode>(&plan, plan.nextId(), inVar, outVar);
  node->setEnumerateObject(keyVar, valueVar);

  auto* cloned =
      ExecutionNode::castTo<EnumerateListNode*>(node->clone(&plan, false));
  EXPECT_EQ(EnumerateListNode::kEnumerateObject, cloned->getMode());
  EXPECT_EQ(node->getVariablesSetHere(), cloned->getVariablesSetHere());
  EXPECT_EQ((std::vector<Variable const*>{keyVar, valueVar}),
            cloned->getVariablesSetHere());
}

TEST_F(ExecutionNodeTest,
       modification_nodes_replace_key_access_only_for_lookup_key) {
  Collection collection("test", &fakedQuery->vocbase(), AccessMode::Type::WRITE,
                        Collection::Hint::None);
  ModificationOptions options;
  auto* docVar = ast.variables()->createTemporaryVariable();
  auto* otherVar = ast.variables()->createTemporaryVariable();
  auto* keyVar = ast.variables()->createTemporaryVariable();
  std::string_view keyAttr = "_key";
  std::span<std::string_view> attribute(&keyAttr, 1);

  // INSERT doc: the whole document is needed
  InsertNode insert(&plan, ExecutionNodeId{1}, &collection, options, docVar,
                    nullptr, nullptr);
  insert.replaceAttributeAccess(&insert, docVar, attribute, keyVar, 0);
  EXPECT_EQ(docVar, insert.inVariable());

  // UPSERT: none of the operands can be replaced by the key
  UpsertNode upsert(&plan, ExecutionNodeId{2}, &collection, options, docVar,
                    docVar, docVar, nullptr, false, false);
  upsert.replaceAttributeAccess(&upsert, docVar, attribute, keyVar, 0);
  EXPECT_EQ(std::vector<Variable const*>{docVar},
            static_cast<ExecutionNode&>(upsert).getVariablesUsedHere());

  // UPDATE doc IN ...: the update document is needed as a whole
  UpdateNode updateDoc(&plan, ExecutionNodeId{3}, &collection, options, docVar,
                       nullptr, nullptr, nullptr);
  updateDoc.replaceAttributeAccess(&updateDoc, docVar, attribute, keyVar, 0);
  EXPECT_EQ(docVar, updateDoc.inDocVariable());

  // UPDATE doc WITH other IN ...: the lookup key can be replaced
  UpdateNode updateKey(&plan, ExecutionNodeId{4}, &collection, options,
                       otherVar, docVar, nullptr, nullptr);
  updateKey.replaceAttributeAccess(&updateKey, docVar, attribute, keyVar, 0);
  EXPECT_EQ(keyVar, updateKey.inKeyVariable());
  EXPECT_EQ(otherVar, updateKey.inDocVariable());
}

}  // namespace arangodb::tests::aql

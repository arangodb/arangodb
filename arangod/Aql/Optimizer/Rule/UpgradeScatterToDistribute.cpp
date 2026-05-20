////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
#include "UpgradeScatterToDistribute.h"

#include "ApplicationFeatures/ApplicationServer.h"

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/Collection.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/DistributeNode.h"
#include "Aql/ExecutionNode/EnumerateCollectionNode.h"
#include "Aql/ExecutionNode/IndexNode.h"
#include "Aql/Expression.h"
#include "Aql/Optimizer.h"
#include "Aql/OptimizerRule.h"

#include "Cluster/ServerState.h"
#include "Containers/SmallVector.h"

#include "Logger/LogMacros.h"

#define LOG_RULE LOG_DEVEL_IF(false) << "UpgradeScatterToDistribute: "

struct DistributeNodeDependency {
  std::unordered_map<std::string_view, arangodb::aql::AstNode const*>
      shardKeyAccessMap;
};

arangodb::aql::Collection const* getCollection(
    arangodb::aql::ExecutionNode const* node) {
  using namespace arangodb::aql;

  switch (node->getType()) {
    case ExecutionNode::NodeType::ENUMERATE_COLLECTION:
      return ExecutionNode::castTo<EnumerateCollectionNode const*>(node)
          ->collection();
    case ExecutionNode::NodeType::INDEX:
      return ExecutionNode::castTo<IndexNode const*>(node)->collection();
    default:
      break;
  }
  return nullptr;
}

arangodb::aql::Variable const* getVariableFromAttributeAccess(
    arangodb::aql::AstNode const* node) {
  using namespace arangodb::aql;
  if (node->numMembers() > 0) {
    node = node->getMember(0);
  }
  if (node->type != AstNodeType::NODE_TYPE_REFERENCE) {
    return nullptr;
  }
  return static_cast<Variable const*>(node->getData());
}

arangodb::aql::Variable const* getOutVariable(
    arangodb::aql::ExecutionNode const* node) {
  using namespace arangodb::aql;
  const auto nodeType = node->getType();
  if (nodeType == ExecutionNode::NodeType::INDEX ||
      nodeType == ExecutionNode::NodeType::ENUMERATE_COLLECTION) {
    auto const* n = dynamic_cast<DocumentProducingNode const*>(node);
    if (n != nullptr) {
      return n->outVariable();
    }
  }
  return nullptr;
}

bool checkIfAllShardKeysAreUsed(arangodb::aql::AstNode const* root,
                                arangodb::aql::ExecutionNode const* node,
                                DistributeNodeDependency& distDep) {
  using namespace arangodb::aql;
  if (root == nullptr) {
    return false;
  }
  if (root->type != AstNodeType::NODE_TYPE_OPERATOR_NARY_OR) {
    return false;
  }

  // number of ANDs
  size_t const numAnds = root->numMembers();
  if (numAnds != 1) {
    // The current implementation would only work if every branch has the same
    // expression that is used for the shardKeys, to keep the logic simpler we
    // just ignore that case and only allow one branch.
    LOG_RULE << "found more than one branch, stop evaluation";
    return false;
  }

  Variable const* var = getOutVariable(node);
  if (var == nullptr) {
    LOG_RULE << "no out variable for node, skip";
    return false;
  }
  Collection const* collection = getCollection(node);
  LOG_RULE << std::format("checking node {}({}) for var({}) and collection({})",
                          node->getTypeString(), node->id(), var->name,
                          collection->name());
  std::vector<std::string> shardKeys{collection->shardKeys(true)};

  AstNode const* andNode = root->getMemberUnchecked(0);
  if (andNode == nullptr) {
    return false;
  }
  TRI_ASSERT(andNode->type == arangodb::aql::NODE_TYPE_OPERATOR_NARY_AND);
  size_t const numConds = andNode->numMembers();
  LOG_RULE << "found " << numConds << " conditions. iterating";
  for (size_t j = 0; j < numConds; ++j) {
    AstNode const* condNode = andNode->getMember(j);
    if (condNode == nullptr ||
        condNode->type != AstNodeType::NODE_TYPE_OPERATOR_BINARY_EQ) {
      LOG_RULE << "condition not equal operator, skip.";
      continue;
    }
    auto const* lhs{condNode->getMember(0)};
    auto const* rhs{condNode->getMember(1)};
    if (lhs->type != AstNodeType::NODE_TYPE_ATTRIBUTE_ACCESS &&
        rhs->type != AstNodeType::NODE_TYPE_ATTRIBUTE_ACCESS) {
      // No side has attribute access, something else, we cant check for
      // shardKey access
      LOG_RULE << "condition has no attribute access, skip";
      continue;
    }

    Variable const* lhsVar = getVariableFromAttributeAccess(lhs);
    Variable const* rhsVar = getVariableFromAttributeAccess(rhs);
    if (lhsVar == nullptr && rhsVar == nullptr) {
      LOG_RULE << "lhsVar and rhsVar are null, skip";
      continue;
    }

    AstNode const* shardKeyNode{nullptr};
    AstNode const* expression{nullptr};
    if (lhsVar == var) {
      shardKeyNode = lhs;
      expression = rhs;
    } else if (rhsVar == var) {
      shardKeyNode = rhs;
      expression = lhs;
    }

    if (shardKeyNode == nullptr) {
      // Neither side is our var, ignore
      LOG_RULE << "var is neither on the left nor on the right side, skip";
      continue;
    }
    std::string const attrField = shardKeyNode->getString();
    bool const isShardKey = std::find(shardKeys.begin(), shardKeys.end(),
                                      attrField) != shardKeys.end();
    if (isShardKey > 0 && expression != nullptr) {
      distDep.shardKeyAccessMap[shardKeyNode->getStringView()] = expression;
    }
  }
  // Found shard-keys in all or-branches
  return distDep.shardKeyAccessMap.size() == shardKeys.size();
}

void replaceScatterWithDistribute(arangodb::aql::ExecutionPlan& plan,
                                  arangodb::aql::ExecutionNode* scatter,
                                  arangodb::aql::Collection const* coll,
                                  arangodb::aql::ExecutionNodeId targetNodeId,
                                  DistributeNodeDependency const& distDep) {
  using namespace arangodb::aql;

  Ast* ast = plan.getAst();
  Variable* shardInputVar = ast->variables()->createTemporaryVariable();
  AstNode* obj = ast->createNodeObject();

  for (auto const& [shardKey, accessExpr] : distDep.shardKeyAccessMap) {
    LOG_RULE << "Add expression for: " << shardKey;
    obj->addMember(ast->createNodeObjectElement(shardKey, accessExpr));
  }

  auto expr = std::make_unique<Expression>(ast, obj);
  auto* calc = plan.createNode<CalculationNode>(&plan, plan.nextId(),
                                                std::move(expr), shardInputVar);

  LOG_RULE << "Create DistributeNode for collection: " << coll->name()
           << " with TargetNodeId: " << targetNodeId;
  auto* distribution = plan.createNode<DistributeNode>(
      &plan, plan.nextId(), ScatterNode::ScatterType::SHARD, coll,
      shardInputVar, targetNodeId);

  plan.replaceNode(scatter, distribution);
  plan.insertBefore(distribution, calc);
}

namespace arangodb::aql {
void upgradeScatterToDistributeRule(Optimizer* opt,
                                    std::unique_ptr<ExecutionPlan> plan,
                                    OptimizerRule const& rule) {
  TRI_ASSERT(arangodb::ServerState::instance()->isCoordinator());
  bool wasModified = false;
  if (plan->isDisabledRule(static_cast<int>(
          OptimizerRule::removeUnnecessaryRemoteScatterRule))) {
    opt->addPlan(std::move(plan), rule, wasModified);
    return;
  }

  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes,
                        {
                            ExecutionNode::NodeType::SCATTER,
                        },
                        true);

  for (auto const node : nodes) {
    ExecutionNode* current = node->getFirstParent();
    while (current != nullptr) {
      Condition condition(plan->getAst());
      bool foundFirstIndexOrEnumerationNode = false;
      if (current->getType() == ExecutionNode::INDEX) {
        auto const indexNode = ExecutionNode::castTo<IndexNode const*>(current);
        auto const cond = indexNode->condition();
        if (cond != nullptr && cond->root() != nullptr) {
          condition.andCombine(cond->root());
        }
        auto const filter = indexNode->filter();
        if (filter != nullptr && filter->node() != nullptr) {
          condition.andCombine(filter->node());
        }
        foundFirstIndexOrEnumerationNode = true;
      } else if (current->getType() == ExecutionNode::ENUMERATE_COLLECTION) {
        auto const enumNode =
            ExecutionNode::castTo<EnumerateCollectionNode const*>(current);
        auto const filter = enumNode->filter();
        if (filter != nullptr && filter->node() != nullptr) {
          condition.andCombine(filter->node());
        }
        foundFirstIndexOrEnumerationNode = true;
      }

      if (condition.root() != nullptr) {
        condition.normalize(plan.get());

        DistributeNodeDependency distDep;
        if (checkIfAllShardKeysAreUsed(condition.root(), current, distDep)) {
          auto const scatterNode = ExecutionNode::castTo<ScatterNode*>(node);
          Collection const* coll{getCollection(current)};
          replaceScatterWithDistribute(*plan, scatterNode, coll, current->id(),
                                       distDep);
          wasModified = true;
        }
      }

      if (foundFirstIndexOrEnumerationNode) {
        // Only the first Index / Enumeration Parent-Node is relevant for us, we
        // can skip the rest
        break;
      }
      current = current->getFirstParent();
    }
  }
  if (wasModified) {
    plan->clearVarUsageComputed();
    plan->findVarUsage();
  }
  opt->addPlan(std::move(plan), rule, wasModified);
}
}  // namespace arangodb::aql

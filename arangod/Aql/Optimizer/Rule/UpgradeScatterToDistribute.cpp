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
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/IndexNode.h"
#include "Aql/Expression.h"
#include "Aql/Optimizer.h"
#include "Aql/OptimizerRule.h"
#include "Indexes/Index.h"

#include "Cluster/ServerState.h"
#include "Containers/SmallVector.h"

#include "Logger/LogMacros.h"
#include "Basics/StaticStrings.h"
#include "Transaction/Helpers.h"

#define LOG_RULE LOG_DEVEL_IF(false) << "UpgradeScatterToDistribute: "

namespace arangodb::aql {

namespace {
struct DistributeNodeDependency {
  std::unordered_map<std::string_view, AstNode const*> shardKeyAccessMap;
};

CollectionAccess const& getCollectionAccess(ExecutionNode const* node) {
  return ExecutionNode::castTo<CollectionAccessingNode const*>(node)
      ->collectionAccess();
}

Variable const* getVariableFromAttributeAccess(AstNode const* node) {
  if (node->numMembers() > 0) {
    node = node->getMember(0);
  }
  if (node->type != AstNodeType::NODE_TYPE_REFERENCE) {
    return nullptr;
  }
  return static_cast<Variable const*>(node->getData());
}

Variable const* getOutVariable(ExecutionNode const* node) {
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
arangodb::aql::AstNode* createParseKeyCall(
    arangodb::aql::Ast* ast, arangodb::aql::AstNode const* input) {
  using namespace arangodb::aql;
  AstNode* args = ast->createNodeArray();
  args->addMember(ast->clone(input));
  return ast->createNodeFunctionCall("PARSE_KEY", args, true);
}

bool checkIfAllShardKeysAreUsed(AstNode const* root, ExecutionNode const* node,
                                DistributeNodeDependency& distDep) {
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
  auto collection = getCollectionAccess(node).collection();
  LOG_RULE << std::format("checking node {}({}) for var({}) and collection({})",
                          node->getTypeString(), node->id(), var->name,
                          collection->name());
  std::vector<std::string> shardKeys{collection->shardKeys(true)};

  AstNode const* andNode = root->getMemberUnchecked(0);
  if (andNode == nullptr) {
    return false;
  }
  TRI_ASSERT(andNode->type == NODE_TYPE_OPERATOR_NARY_AND);
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

    std::string attrField = shardKeyNode->getString();
    if (attrField == arangodb::StaticStrings::IdString) {
      attrField = arangodb::StaticStrings::KeyString;
      Ast* ast = node->plan()->getAst();
      expression = createParseKeyCall(ast, expression);
    }

    bool isShardKey = std::find(shardKeys.begin(), shardKeys.end(),
                                attrField) != shardKeys.end();

    if (isShardKey && expression != nullptr) {
      std::string_view shardKey =
          shardKeyNode->getString() == arangodb::StaticStrings::IdString
              ? std::string_view{arangodb::StaticStrings::KeyString}
              : shardKeyNode->getStringView();
      // If a shard key is compared with multiple expressions, e.g.
      // `doc._key == "foo" AND doc._id == other._from`, prefer a constant
      // one. restrict-to-single-shard derives the target shard from constant
      // shard key values, so the distribute key has to agree with it.
      // Otherwise rows would be sent to shards that were restricted away.
      auto [it, inserted] =
          distDep.shardKeyAccessMap.try_emplace(shardKey, expression);
      if (!inserted && !it->second->isConstant()) {
        it->second = expression;
      }
    }
  }
  // Found shard-keys in all or-branches
  return distDep.shardKeyAccessMap.size() == shardKeys.size();
}

void replaceScatterWithDistribute(ExecutionPlan& plan, ExecutionNode* scatter,
                                  Collection const* coll,
                                  ExecutionNodeId targetNodeId,
                                  DistributeNodeDependency const& distDep) {
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

std::unique_ptr<Condition> getCondition(ExecutionPlan* plan,
                                        ExecutionNode* current) {
  auto condition = std::make_unique<Condition>(plan->getAst());
  if (current->getType() == ExecutionNode::INDEX) {
    auto const indexNode = ExecutionNode::castTo<IndexNode const*>(current);

    auto indexes = indexNode->getIndexes();
    for (auto&& i : indexes) {
      if (i->type() == arangodb::IndexType::Inverted) {
        LOG_RULE << "Inverted Indexes are unsupported for this rule";
        return nullptr;
      }
    }

    auto const cond = indexNode->condition();
    if (cond != nullptr && cond->root() != nullptr) {
      condition->andCombine(cond->root());
    }
    auto const filter = indexNode->filter();
    if (filter != nullptr && filter->node() != nullptr) {
      condition->andCombine(filter->node());
    }
    return condition;
  } else if (current->getType() == ExecutionNode::ENUMERATE_COLLECTION) {
    auto const enumNode =
        ExecutionNode::castTo<EnumerateCollectionNode const*>(current);
    auto const filter = enumNode->filter();
    if (filter != nullptr && filter->node() != nullptr) {
      condition->andCombine(filter->node());
    }
    return condition;
  }
  return nullptr;
}

}  // namespace

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
      if (current->getType() == ExecutionNode::INDEX ||
          current->getType() == ExecutionNode::ENUMERATE_COLLECTION) {
        auto collectionAccess = getCollectionAccess(current);
        if (collectionAccess.isUsedAsSatellite()) {
          LOG_RULE << "collection is used as satellite, skip";
          break;
        }

        auto condition = getCondition(plan.get(), current);

        if (condition != nullptr) {
          condition->normalize(plan.get());

          DistributeNodeDependency distDep;
          if (checkIfAllShardKeysAreUsed(condition->root(), current, distDep)) {
            auto const scatterNode = ExecutionNode::castTo<ScatterNode*>(node);
            replaceScatterWithDistribute(*plan, scatterNode,
                                         collectionAccess.collection(),
                                         current->id(), distDep);
            wasModified = true;
          }
        }
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

// TODO(listunov): disclaimer

#include "absl/strings/str_format.h"
#include "ApplicationFeatures/ApplicationServer.h"

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/Collection.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/DistributeNode.h"
#include "Aql/ExecutionNode/EnumerateCollectionNode.h"
#include "Aql/ExecutionNode/FilterNode.h"
#include "Aql/ExecutionNode/IndexNode.h"
#include "Aql/ExecutionNode/SubqueryNode.h"
#include "Aql/Expression.h"
#include "Aql/Optimizer.h"
#include "Aql/OptimizerRules.h"
#include "Aql/QueryContext.h"

#include "Basics/voc-errors.h"

#include "Cluster/ServerState.h"
#include "Containers/SmallVector.h"

#include "Logger/LogMacros.h"

#define LOG_RULE LOG_DEVEL_IF(true) << "UpgradeScatterToDistribute: "

using EN = arangodb::aql::ExecutionNode;

arangodb::aql::Collection const* getCollection(
    arangodb::aql::ExecutionNode const* node) {
  using arangodb::aql::ExecutionNode;

  switch (node->getType()) {
    case EN::ENUMERATE_COLLECTION:
      return ExecutionNode::castTo<
                 arangodb::aql::EnumerateCollectionNode const*>(node)
          ->collection();
    case EN::INDEX:
      return ExecutionNode::castTo<arangodb::aql::IndexNode const*>(node)
          ->collection();
    default:
      // note: modification nodes are not covered here yet
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "node type does not have a collection");
  }
}

arangodb::aql::Variable const* getVariableFromAttributeAccess(
    arangodb::aql::AstNode const* node) {
  if (node->type != arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS) {
    return nullptr;
  }

  node = node->getMember(0);

  if (node->type != arangodb::aql::NODE_TYPE_REFERENCE) {
    return nullptr;
  }

  return static_cast<arangodb::aql::Variable const*>(node->getData());
}

arangodb::aql::Variable const* getOutVariable(
    arangodb::aql::ExecutionNode const* node) {
  using arangodb::aql::ExecutionNode;

  switch (node->getType()) {
    case EN::INDEX:
    case EN::ENUMERATE_COLLECTION: {
      auto const* n =
          dynamic_cast<arangodb::aql::DocumentProducingNode const*>(node);
      if (n != nullptr) {
        return n->outVariable();
      }
    }
    default:
      break;
  }
  return nullptr;
}

struct DistributeNodeDep {
  arangodb::aql::Variable const* distVar{nullptr};
  std::set<std::string> members;
};
bool checkIfAllShardKeysAreUsed(arangodb::aql::AstNode const* root,
                                arangodb::aql::ExecutionNode const* node,
                                DistributeNodeDep& distDep) {
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
    // TODO(listunov): do we really only support one AND-branch ? or what is happening here ?
    LOG_RULE << "found more than one AND-branch, stop evaluation";
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
  std::vector<std::string> shardKeys{ collection->shardKeys(true) };

  uint32_t foundAllShardKeysCount{0};
  for (size_t i = 0; i < numAnds; ++i) {
    AstNode const* andNode = root->getMemberUnchecked(i);
    if (andNode == nullptr) {
      continue;
    }
    TRI_ASSERT(andNode->type == arangodb::aql::NODE_TYPE_OPERATOR_NARY_AND);
    size_t const numConds = andNode->numMembers();
    LOG_RULE << "found " << numConds << " conditions. iterating";
    std::set<std::string> shardKeySet{shardKeys.begin(), shardKeys.end()};
    for (size_t j = 0; j < numConds; ++j) {
      AstNode const* condNode = andNode->getMember(j);
      if (condNode == nullptr ||
          condNode->type != AstNodeType::NODE_TYPE_OPERATOR_BINARY_EQ) {
        LOG_RULE << "condition not equal operator, skip.";
        continue;
      }
      auto const* lhs{ condNode->getMember(0) };
      auto const* rhs{ condNode->getMember(1) };
      if (lhs->type != AstNodeType::NODE_TYPE_ATTRIBUTE_ACCESS &&
          rhs->type != AstNodeType::NODE_TYPE_ATTRIBUTE_ACCESS) {
        // No side has attribute access, something else, we cant check for shardKey access
        LOG_RULE << "condition has no attribute access, skip";
        continue;
      }

      // TODO(listunov): getVariableFromAttributeAccess can probably be removed but it does a check on NODE_TYPE_REFERENCE, maybe its easier to keep it in separate function
      Variable const* lhsVar = getVariableFromAttributeAccess(lhs);
      Variable const* rhsVar = getVariableFromAttributeAccess(rhs);

      // TODO(listunov): we could work if one of them is nullptr, so is this a check only for smartJoinsRule ?
      if (lhsVar == nullptr || rhsVar == nullptr) {
        // TODO(listunov): when can this happen?
        // TODO(listunov): when its not NODE_TYPE_REFERENCE, what is NODE_TYPE_REFERENCE?
        LOG_RULE << "lhsVar or rhsVar are null, skip";
        continue;
      }

      AstNode const* attribute{ nullptr };
      AstNode const* other{ nullptr };
      if (lhsVar == var) {
        distDep.distVar = rhsVar;
        attribute = lhs;
        other = rhs;
      } else if (rhsVar == var) {
        distDep.distVar = lhsVar;
        attribute = rhs;
        other = lhs;
      }

      if (attribute == nullptr) {
        // Neither side is our var, ignore
        LOG_RULE << "var is neither on the left nor on the right side, skip";
        continue;
      }
      std::string const shardField = attribute->getString();
      LOG_RULE << "Found attribute, remove from set: " << shardField;
      auto const numErased = shardKeySet.erase(shardField);
      if (numErased > 0 && other != nullptr) {
        distDep.members.insert(other->getString());
      }
    }
    if (shardKeySet.empty()) {
      LOG_RULE << "found all shardKeys for " << var->name;
      foundAllShardKeysCount++;
    } else {
      for (auto const& leftOver : shardKeySet) {
        LOG_RULE << "LeftOver ShardKey: " << leftOver;
      }
      // Some condition does not handle all shard-keys -> bail
      return false;
    }
  }
  // Found shard-keys in all and-branches
  return foundAllShardKeysCount == numAnds;
}

void replaceScatterWithDistribute(arangodb::aql::ExecutionPlan& plan,
  arangodb::aql::ExecutionNode* scatter, arangodb::aql::Collection const* coll,
  arangodb::aql::ExecutionNodeId targetNodeId, DistributeNodeDep const& distDep) {

  // TODO(listunov): targetNodeId IndexNode is wrong here, isn't it ? EnumerateCollectionNode sounds more plausible

  using namespace arangodb::aql;

  Ast* ast = plan.getAst();

  Variable* shardInputVar = ast->variables()->createTemporaryVariable();
  AstNode* obj = ast->createNodeObject();

  for (auto const& member : distDep.members) {
    LOG_RULE << "Add Member: " << distDep.distVar->name << "." << member;
    obj->addMember(ast->createNodeObjectElement(
      member,
      ast->createNodeAttributeAccess(ast->createNodeReference(distDep.distVar), member)));
  }

  auto expr = std::make_unique<Expression>(ast, obj);
  auto* calc = plan.createNode<CalculationNode>(
      &plan, plan.nextId(), std::move(expr), shardInputVar);

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
    // TODO(listunov): From the name, this should also be skipped when
    // removeUnnecessaryRemoteScatterRule is off
    opt->addPlan(std::move(plan), rule, wasModified);
    return;
  }

  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes,
                        {
                            ExecutionNode::NodeType::SCATTER,
                        },
                        true);

  for (auto& node : nodes) {
    ExecutionNode* current = node->getFirstParent();
    while (current != nullptr) {
      Condition condition(plan->getAst());
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
      } else if (current->getType() == ExecutionNode::ENUMERATE_COLLECTION) {
        auto const enumNode = ExecutionNode::castTo<EnumerateCollectionNode const*>(current);
        auto const filter = enumNode->filter();
        if (filter != nullptr && filter->node() != nullptr) {
          condition.andCombine(filter->node());
        }
      }

      if (condition.root() != nullptr) {
        condition.normalize(plan.get());

        DistributeNodeDep distDep;
        if (checkIfAllShardKeysAreUsed(condition.root(), current, distDep)) {
          auto const scatterNode = ExecutionNode::castTo<ScatterNode*>(node);
          Collection const* coll{ getCollection(current) };
          LOG_RULE << "--------------------------------------";
          plan->show();
          replaceScatterWithDistribute(*plan, scatterNode, coll, current->id(), distDep);
          wasModified = true;
          LOG_RULE << "--------------------------------------";
          plan->show();
        }
        // Only the first Index / Enumeration Parent-Node is relevant for us, we can skip the rest
        break;
      }
      current = current->getFirstParent();
    }
  }
  opt->addPlan(std::move(plan), rule, wasModified);
}
}  // namespace arangodb::aql

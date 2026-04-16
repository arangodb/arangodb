// TODO(listunov): disclaimer

#include "absl/strings/str_format.h"
#include "ApplicationFeatures/ApplicationServer.h"

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/Collection.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/EnumerateCollectionNode.h"
#include "Aql/ExecutionNode/FilterNode.h"
#include "Aql/ExecutionNode/IndexNode.h"
#include "Aql/Expression.h"
#include "Aql/Optimizer.h"
#include "Aql/OptimizerRules.h"
#include "Aql/QueryContext.h"

#include "Basics/voc-errors.h"

#include "Cluster/ServerState.h"
#include "Containers/SmallVector.h"

#include "Logger/LogMacros.h"

#define LOG_RULE LOG_DEVEL_IF(true)

using EN = arangodb::aql::ExecutionNode;

struct AttributeAccess {
  std::string attr;

  explicit AttributeAccess(arangodb::aql::AstNode const* node) {
    auto current = node;
    while (current->type == arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS) {
      attr += "." + current->getString();
      current = current->getMember(0);
    }
    if (current->type == arangodb::aql::NODE_TYPE_REFERENCE) {
      auto variable =
          static_cast<arangodb::aql::Variable const*>(current->getData());
      attr = variable->name + attr;
    } else {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL, "internal node for AttributeAccess struct");
    }
  }

  AttributeAccess(arangodb::aql::Variable const* variable, std::string const& a)
      : attr(a) {
    attr = variable->name + "." + attr;
  }

  std::string const& toString() const noexcept { return attr; }

  bool operator==(AttributeAccess const& other) const {
    return attr == other.attr;
  }

  bool operator!=(AttributeAccess const& other) const {
    return !(*this == other);
  }
};

namespace std {

template<>
struct hash<AttributeAccess> {
  size_t operator()(AttributeAccess const& x) const noexcept {
    return std::hash<std::string>()(x.toString());
  }
};

template<>
struct equal_to<AttributeAccess> {
  bool operator()(AttributeAccess const& a,
                  AttributeAccess const& b) const noexcept {
    return a == b;
  }
};

}  // namespace std

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

struct Cache {
  explicit Cache(arangodb::aql::ExecutionPlan* plan) : plan(plan) {}

  arangodb::aql::ExecutionPlan* plan;
  std::unordered_map<AttributeAccess, std::unordered_set<AttributeAccess>>
      aliases;
  std::unordered_map<arangodb::aql::Variable const*, std::vector<std::string>>
      shardKeys;

  arangodb::aql::Collection const* getCollectionForVariable(
      arangodb::aql::Variable const* variable) {
    auto setter = plan->getVarSetBy(variable->id);
    if (setter == nullptr ||
        (setter->getType() != EN::INDEX &&
         setter->getType() != EN::ENUMERATE_COLLECTION &&
         setter->getType() != EN::ENUMERATE_IRESEARCH_VIEW)) {
      return nullptr;
    }
    return getCollection(setter);
  }

  std::vector<std::string> const& getShardKeys(
      arangodb::aql::Variable const* variable) {
    auto it = shardKeys.find(variable);
    if (it == shardKeys.end()) {
      std::vector<std::string> keys;

      auto collection = getCollectionForVariable(variable);
      if (collection != nullptr) {
        keys = collection->shardKeys(true);
        if (!collection->smartJoinAttribute().empty()) {
          keys.clear();
          keys.emplace_back(collection->smartJoinAttribute());
        }
      }
      it = shardKeys.emplace(variable, std::move(keys)).first;
    }

    return (*it).second;
  }
};

arangodb::aql::Variable const* getVariableFromAttributeAccess(
    arangodb::aql::AstNode const* node) {
  TRI_ASSERT(node->type == arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS);

  // TODO(listunov): (what?) adjust for nested shard keys
  node = node->getMember(0);

  if (node->type != arangodb::aql::NODE_TYPE_REFERENCE) {
    return nullptr;
  }

  return static_cast<arangodb::aql::Variable const*>(node->getData());
}

bool checkAliasesForAllShardsKeysOfVar(arangodb::aql::AstNode const* lhs,
                                       arangodb::aql::AstNode const* rhs,
                                       arangodb::aql::Variable const* var,
                                       Cache& cache) {
  TRI_ASSERT(lhs->type == arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS);
  TRI_ASSERT(rhs->type == arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS);

  arangodb::aql::Variable const* lhsVar = getVariableFromAttributeAccess(lhs);
  arangodb::aql::Variable const* rhsVar = getVariableFromAttributeAccess(rhs);

  if (lhsVar == nullptr || rhsVar == nullptr) {
    return false;
  }

  // TODO (listunov): is this right ? what if condition also contains a third col ?
  if (lhsVar != var && rhsVar != var) {
    return false;
  }

  std::vector<std::string> const& v1Keys = cache.getShardKeys(var);
  for (auto const& key : v1Keys) {
    if (!cache.aliases.contains(AttributeAccess(var, key))) {
      return false;
    }
  }
  return true;
}

arangodb::aql::Variable const* getOutVariable(
    arangodb::aql::ExecutionNode const* node) {
  using arangodb::aql::ExecutionNode;

  switch (node->getType()) {
    case EN::CALCULATION:
      return ExecutionNode::castTo<arangodb::aql::CalculationNode const*>(node)
          ->outVariable();
    case EN::INDEX:
    case EN::ENUMERATE_COLLECTION: {
      auto const* n =
          dynamic_cast<arangodb::aql::DocumentProducingNode const*>(node);
      if (n != nullptr) {
        return n->outVariable();
      }
    }
    default: {
      return nullptr;
    }
  }
}

void buildAliases(arangodb::aql::AstNode const* root, Cache& cache) {
  if (root == nullptr ||
      root->type != arangodb::aql::NODE_TYPE_OPERATOR_NARY_OR ||
      root->numMembers() != 1) {
    return;
  }

  for (size_t i = 0; i < root->numMembers(); ++i) {
    arangodb::aql::AstNode const* andNode = root->getMemberUnchecked(i);

    if (andNode == nullptr) {
      continue;
    }

    TRI_ASSERT(andNode->type == arangodb::aql::NODE_TYPE_OPERATOR_NARY_AND);

    size_t numConds = andNode->numMembers();

    for (size_t j = 0; j < numConds; ++j) {
      arangodb::aql::AstNode const* condNode = andNode->getMember(j);

      if (condNode == nullptr ||
          condNode->type != arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ) {
        // something other than an equality join. we do not
        // support this
        continue;
      }

      // equality comparison
      auto const* lhs = condNode->getMember(0);
      auto const* rhs = condNode->getMember(1);

      if (lhs->type != arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS ||
          rhs->type != arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS) {
        // something else
        continue;
      }

      auto lhsVar = getVariableFromAttributeAccess(lhs);
      auto rhsVar = getVariableFromAttributeAccess(rhs);

      if (lhsVar == nullptr || rhsVar == nullptr) {
        // something else
        continue;
      }

      AttributeAccess one(lhs);
      AttributeAccess two(rhs);
      cache.aliases[one].emplace(two);
      cache.aliases[two].emplace(one);
    }
  }
}

bool checkIfAllShardKeysAreUsed(arangodb::aql::AstNode const* root,
                                arangodb::aql::ExecutionNode const* node,
                                Cache& cache) {
  if (root == nullptr) {
    return false;
  }

  if (root->type != arangodb::aql::NODE_TYPE_OPERATOR_NARY_OR) {
    return false;
  }
  // number of ANDs
  size_t const numAnds = root->numMembers();

  if (numAnds != 1) {
    return false;
  }

  arangodb::aql::Variable const* var = getOutVariable(node);

  // currently numAnds will always be one here.
  // however, when we support multiple shard keys, we actually may have
  // more than one condition to care about here.
  for (size_t i = 0; i < numAnds; ++i) {
    arangodb::aql::AstNode const* andNode = root->getMemberUnchecked(i);

    if (andNode == nullptr) {
      continue;
    }

    TRI_ASSERT(andNode->type == arangodb::aql::NODE_TYPE_OPERATOR_NARY_AND);

    size_t numConds = andNode->numMembers();

    for (size_t j = 0; j < numConds; ++j) {
      arangodb::aql::AstNode const* condNode = andNode->getMember(j);

      if (condNode == nullptr ||
          condNode->type != arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ) {
        // something other than an equality join. we do not
        // support this
        continue;
      }

      // equality comparison
      // now check if this comparison has the pattern
      // <variable from collection1>.<attribute from collection1> == <variable
      // from collection2>.<attribute from collection2>

      auto const* lhs = condNode->getMember(0);
      auto const* rhs = condNode->getMember(1);

      if (lhs->type != arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS ||
          rhs->type != arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS) {
        // something else
        continue;
      }

      if (checkAliasesForAllShardsKeysOfVar(lhs, rhs, var, cache)) {
        // all shard keys are accounted in the cache aliases
        return true;
      }
    }
  }

  return false;
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

  LOG_RULE << " ---- FOUND NODES: " << nodes.size();
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
        LOG_RULE << "----- NODE: " << current->getTypeString() << " CONDITION: " << condition.root()->toString();
        condition.normalize(plan.get());
        Cache cache(plan.get());
        buildAliases(condition.root(), cache);

        // TODO(listunov): SmartJoinsRule seems to be breaking here ? 
        if (checkIfAllShardKeysAreUsed(condition.root(), current, cache)) {
          LOG_RULE << "------ CHECK IF I CAN UPGRADE";
          if (!plan->shouldExcludeFromScatterGather(current)) {
            wasModified = true;
            LOG_RULE << "------ UPGRADE";
          }
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

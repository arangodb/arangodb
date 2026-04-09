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
/// @author Max Neunhoeffer
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "RestrictToSingleShard.h"

#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/CollectionAccessingNode.h"
#include "Aql/ExecutionNode/DocumentProducingNode.h"
#include "Aql/ExecutionNode/EnumerateCollectionNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/FilterNode.h"
#include "Aql/ExecutionNode/IndexNode.h"
#include "Aql/ExecutionNode/InsertNode.h"
#include "Aql/ExecutionNode/ModificationNode.h"
#include "Aql/ExecutionNode/RemoveNode.h"
#include "Aql/ExecutionNode/ReplaceNode.h"
#include "Aql/ExecutionNode/UpdateNode.h"
#include "Aql/ExecutionNode/UpsertNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Optimizer.h"
#include "Aql/OptimizerUtils.h"
#include "Aql/Query.h"
#include "Aql/TypedAstNodes.h"
#include "Aql/Variable.h"
#include "Aql/WalkerWorker.h"
#include "Aql/types.h"
#include "Basics/AttributeNameParser.h"
#include "Basics/StaticStrings.h"
#include "Basics/SupervisedBuffer.h"
#include "Cluster/ServerState.h"
#include "Containers/HashSet.h"
#include "Containers/SmallVector.h"
#include "Indexes/Index.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>

#include <map>
#include <optional>
#include <unordered_set>
#include <variant>

namespace {

using EN = arangodb::aql::ExecutionNode;

arangodb::aql::Variable const* getOutVariable(
    arangodb::aql::ExecutionNode const* node) {
  switch (node->getType()) {
    case EN::CALCULATION:
      return EN::castTo<arangodb::aql::CalculationNode const*>(node)
          ->outVariable();
    default: {
      auto const* n =
          dynamic_cast<arangodb::aql::DocumentProducingNode const*>(node);
      if (n != nullptr) {
        return n->outVariable();
      }
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "node type does not have an out variable");
    }
  }
}

void restrictToShard(arangodb::aql::ExecutionNode* node,
                     arangodb::ShardID const& shardId) {
  auto* n = dynamic_cast<arangodb::aql::CollectionAccessingNode*>(node);
  if (n != nullptr) {
    return n->restrictToShard(shardId);
  }
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_INTERNAL, "node type cannot be restricted to a single shard");
}

struct PairHash {
  template<class T1, class T2>
  size_t operator()(std::pair<T1, T2> const& pair) const noexcept {
    size_t first = std::hash<T1>()(pair.first);
    size_t second = std::hash<T2>()(pair.second);
    return first ^ second;
  }
};

void findShardKeyInComparison(arangodb::aql::AstNode const* root,
                              arangodb::aql::Variable const* inputVariable,
                              std::unordered_set<std::string>& toFind,
                              arangodb::velocypack::Builder& builder) {
  using arangodb::aql::AstNode;
  using arangodb::aql::Variable;
  TRI_ASSERT(root->type ==
             arangodb::aql::AstNodeType::NODE_TYPE_OPERATOR_BINARY_EQ);

  AstNode const* value = nullptr;
  std::pair<Variable const*, std::vector<arangodb::basics::AttributeName>> pair;

  arangodb::aql::ast::RelationalOperatorNode eqOp(root);
  auto lhs = eqOp.getLeft();
  auto rhs = eqOp.getRight();
  std::string result;

  if (lhs->isAttributeAccessForVariable(pair, false) &&
      pair.first == inputVariable && rhs->isConstant()) {
    TRI_AttributeNamesToString(pair.second, result, true);
    value = rhs;
  } else if (rhs->isAttributeAccessForVariable(pair, false) &&
             pair.first == inputVariable && lhs->isConstant()) {
    TRI_AttributeNamesToString(pair.second, result, true);
    value = lhs;
  }

  if (value != nullptr) {
    TRI_ASSERT(!result.empty());
    auto it = toFind.find(result);

    if (it != toFind.end()) {
      builder.add(VPackValue(result));
      value->toVelocyPackValue(builder);
      toFind.erase(it);
    }
  }
}

void findShardKeysInExpression(arangodb::aql::AstNode const* root,
                               arangodb::aql::Variable const* inputVariable,
                               std::unordered_set<std::string>& toFind,
                               arangodb::velocypack::Builder& builder) {
  if (root == nullptr) {
    return;
  }

  switch (root->type) {
    case arangodb::aql::AstNodeType::NODE_TYPE_OPERATOR_NARY_OR: {
      arangodb::aql::ast::NaryOperatorNode naryOr(root);
      auto operands = naryOr.getOperands();
      if (operands.size() != 1) {
        return;
      }
      root = operands[0];
      if (root == nullptr ||
          root->type !=
              arangodb::aql::AstNodeType::NODE_TYPE_OPERATOR_NARY_AND) {
        return;
      }
    }  // falls through
    case arangodb::aql::AstNodeType::NODE_TYPE_OPERATOR_BINARY_AND:
    case arangodb::aql::AstNodeType::NODE_TYPE_OPERATOR_NARY_AND: {
      for (auto* member : root->getMemberList()) {
        if (member != nullptr &&
            member->type ==
                arangodb::aql::AstNodeType::NODE_TYPE_OPERATOR_BINARY_EQ) {
          findShardKeyInComparison(member, inputVariable, toFind, builder);
        }
      }
      break;
    }
    case arangodb::aql::AstNodeType::NODE_TYPE_OPERATOR_BINARY_EQ: {
      findShardKeyInComparison(root, inputVariable, toFind, builder);
      break;
    }
    default:
      break;
  }
}

std::optional<arangodb::ShardID> getSingleShardId(
    arangodb::aql::ExecutionPlan const* plan,
    arangodb::aql::ExecutionNode const* node,
    arangodb::aql::Collection const* collection,
    arangodb::aql::Variable const* collectionVariable = nullptr) {
  if (collection->isSmart() &&
      collection->getCollection()->type() == TRI_COL_TYPE_EDGE) {
    return std::nullopt;
  }

  TRI_ASSERT(node->getType() == EN::INDEX ||
             node->getType() == EN::ENUMERATE_COLLECTION ||
             node->getType() == EN::FILTER || node->getType() == EN::INSERT ||
             node->getType() == EN::UPDATE || node->getType() == EN::REPLACE ||
             node->getType() == EN::REMOVE);

  arangodb::aql::Variable const* inputVariable = nullptr;
  if (node->getType() == EN::INDEX ||
      node->getType() == EN::ENUMERATE_COLLECTION) {
    inputVariable =
        EN::castTo<arangodb::aql::DocumentProducingNode const*>(node)
            ->outVariable();
  } else if (node->getType() == EN::FILTER) {
    inputVariable =
        EN::castTo<arangodb::aql::FilterNode const*>(node)->inVariable();
  } else if (node->getType() == EN::INSERT) {
    inputVariable =
        EN::castTo<arangodb::aql::InsertNode const*>(node)->inVariable();
  } else if (node->getType() == EN::REMOVE) {
    inputVariable =
        EN::castTo<arangodb::aql::RemoveNode const*>(node)->inVariable();
  } else if (node->getType() == EN::REPLACE || node->getType() == EN::UPDATE) {
    auto updateReplaceNode =
        EN::castTo<arangodb::aql::UpdateReplaceNode const*>(node);
    if (updateReplaceNode->inKeyVariable() != nullptr) {
      inputVariable = updateReplaceNode->inKeyVariable();
    } else {
      inputVariable = updateReplaceNode->inDocVariable();
    }
  } else {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "logic error");
  }

  TRI_ASSERT(inputVariable != nullptr);

  auto setter = plan->getVarSetBy(inputVariable->id);

  if (setter == nullptr) {
    TRI_ASSERT(false);
    return std::nullopt;
  }

  auto shardKeys = collection->shardKeys(true);
  std::unordered_set<std::string> toFind;
  for (auto const& it : shardKeys) {
    if (it.find('.') != std::string::npos) {
      return std::nullopt;
    }
    toFind.emplace(it);
  }

  auto sb = arangodb::velocypack::SupervisedBuffer(
      plan->getAst()->query().resourceMonitor());
  VPackBuilder builder(sb);
  builder.openObject();

  if (setter->getType() == EN::CALCULATION) {
    arangodb::aql::CalculationNode const* c =
        EN::castTo<arangodb::aql::CalculationNode const*>(setter);
    auto n = c->expression()->node();
    if (n == nullptr) {
      return std::nullopt;
    }

    if (n->isStringValue()) {
      if (!n->isConstant() || toFind.size() != 1 ||
          toFind.find(arangodb::StaticStrings::KeyString) == toFind.end()) {
        return std::nullopt;
      }

      builder.add(VPackValue(arangodb::StaticStrings::KeyString));
      n->toVelocyPackValue(builder);
      toFind.clear();
    } else if (n->isObject()) {
      for (size_t i = 0; i < n->numMembers(); ++i) {
        auto sub = n->getMember(i);

        if (sub->type != arangodb::aql::AstNodeType::NODE_TYPE_OBJECT_ELEMENT) {
          continue;
        }

        arangodb::aql::ast::ObjectElementNode objElem(sub);
        auto it = toFind.find(sub->getString());

        if (it != toFind.end()) {
          auto v = objElem.getValue();
          if (v->isConstant()) {
            builder.add(VPackValue(sub->getString()));
            v->toVelocyPackValue(builder);
            toFind.erase(it);
          }
        }
      }
    } else {
      if (nullptr != collectionVariable) {
        findShardKeysInExpression(n, collectionVariable, toFind, builder);
      } else {
        findShardKeysInExpression(n, inputVariable, toFind, builder);
      }
    }
  } else if (setter->getType() == EN::INDEX && setter == node) {
    auto const* c = EN::castTo<arangodb::aql::IndexNode const*>(setter);

    if (c->getIndexes().size() != 1) {
      return std::nullopt;
    }
    auto const* condition = c->condition();

    if (condition != nullptr) {
      arangodb::aql::AstNode const* root = condition->root();
      findShardKeysInExpression(root, inputVariable, toFind, builder);
    }
  }

  if (!toFind.empty() && (node->getType() == EN::INDEX ||
                          node->getType() == EN::ENUMERATE_COLLECTION)) {
    auto en = dynamic_cast<arangodb::aql::DocumentProducingNode const*>(node);
    TRI_ASSERT(en != nullptr);

    if (en->hasFilter()) {
      arangodb::aql::AstNode const* root = en->filter()->node();
      findShardKeysInExpression(root, inputVariable, toFind, builder);
    }
  }

  builder.close();

  if (!toFind.empty()) {
    return std::nullopt;
  }

  if (node->getType() == EN::INSERT && collection->numberOfShards() != 1 &&
      (shardKeys.size() != 1 ||
       shardKeys[0] != arangodb::StaticStrings::KeyString) &&
      builder.slice().get(arangodb::StaticStrings::KeyString).isNone()) {
    return std::nullopt;
  }

  std::string shardId;

  auto res =
      collection->getCollection()->getResponsibleShard(builder.slice(), true);

  if (res.fail()) {
    return std::nullopt;
  }

  TRI_ASSERT(res.get().isValid());
  return std::move(res.get());
}

/// WalkerWorker to track collection variable dependencies
class CollectionVariableTracker final
    : public arangodb::aql::WalkerWorker<
          arangodb::aql::ExecutionNode,
          arangodb::aql::WalkerUniqueness::NonUnique> {
  using DependencyPair = std::pair<arangodb::aql::Variable const*,
                                   arangodb::aql::Collection const*>;
  using DependencySet = std::unordered_set<DependencyPair, PairHash>;
  bool _stop;
  std::unordered_map<arangodb::aql::Variable const*, DependencySet>
      _dependencies;
  std::unordered_map<arangodb::aql::Collection const*, arangodb::aql::VarSet>
      _collectionVariables;

 private:
  template<class NodeType>
  void processSetter(arangodb::aql::ExecutionNode const* en,
                     arangodb::aql::Variable const* outVariable) {
    auto node = EN::castTo<NodeType const*>(en);
    try {
      arangodb::aql::VarSet inputVariables;
      node->getVariablesUsedHere(inputVariables);
      for (auto var : inputVariables) {
        for (auto dep : _dependencies[var]) {
          _dependencies[outVariable].emplace(dep);
        }
      }
    } catch (...) {
      _stop = true;
    }
  }

  template<class NodeType>
  void processModificationNode(arangodb::aql::ExecutionNode const* en) {
    auto node = EN::castTo<NodeType const*>(en);
    auto collection = node->collection();
    std::vector<arangodb::aql::Variable const*> outVariables{
        node->getOutVariableOld(), node->getOutVariableNew()};
    for (auto outVariable : outVariables) {
      if (nullptr != outVariable) {
        processSetter<NodeType>(node, outVariable);
        _collectionVariables[collection].emplace(outVariable);
      }
    }
  }

 public:
  explicit CollectionVariableTracker() : _stop{false} {}

  bool isSafeForOptimization() const { return !_stop; }

  DependencySet const& getDependencies(arangodb::aql::Variable const* var) {
    return _dependencies[var];
  }

  void after(arangodb::aql::ExecutionNode* en) override final {
    switch (en->getType()) {
      case EN::CALCULATION: {
        auto outVariable = getOutVariable(en);
        processSetter<arangodb::aql::CalculationNode>(en, outVariable);
        break;
      }

      case EN::INDEX:
      case EN::ENUMERATE_COLLECTION: {
        auto collection = arangodb::aql::utils::getCollection(en);
        auto variable = getOutVariable(en);

        try {
          _dependencies[variable].emplace(variable, collection);
          _collectionVariables[collection].emplace(variable);
        } catch (...) {
          _stop = true;
        }
        break;
      }

      case EN::UPDATE: {
        processModificationNode<arangodb::aql::UpdateNode>(en);
        break;
      }

      case EN::UPSERT: {
        processModificationNode<arangodb::aql::UpsertNode>(en);
        break;
      }

      case EN::INSERT: {
        processModificationNode<arangodb::aql::InsertNode>(en);
        break;
      }

      case EN::REMOVE: {
        processModificationNode<arangodb::aql::RemoveNode>(en);
        break;
      }

      case EN::REPLACE: {
        processModificationNode<arangodb::aql::ReplaceNode>(en);
        break;
      }

      default: {
        break;
      }
    }
  }
};

/// WalkerWorker for restrictToSingleShard
class RestrictToSingleShardChecker final
    : public arangodb::aql::WalkerWorker<
          arangodb::aql::ExecutionNode,
          arangodb::aql::WalkerUniqueness::NonUnique> {
  struct AllShards {};

  arangodb::aql::ExecutionPlan* _plan;
  CollectionVariableTracker& _tracker;
  std::unordered_map<
      arangodb::aql::Variable const*,
      std::variant<AllShards, std::unordered_set<arangodb::ShardID>>>
      _shardsUsed;
  std::unordered_map<
      arangodb::aql::Variable const*,
      std::variant<AllShards, std::unordered_set<arangodb::ShardID>>>
      _shardsCleared;
  bool _stop;
  std::map<arangodb::aql::Collection const*, bool> _unsafe;

 public:
  explicit RestrictToSingleShardChecker(arangodb::aql::ExecutionPlan* plan,
                                        CollectionVariableTracker& tracker)
      : _plan{plan}, _tracker{tracker}, _stop{false} {}

  bool isSafeForOptimization() const {
    return (!_stop && !_plan->getAst()->functionsMayAccessDocuments());
  }

  arangodb::ShardID getShard(arangodb::aql::Variable const* variable) const {
    auto const& it = _shardsCleared.find(variable);
    if (it == _shardsCleared.end()) {
      return arangodb::ShardID::invalidShard();
    }

    auto set = it->second;
    if (std::holds_alternative<AllShards>(set)) {
      return arangodb::ShardID::invalidShard();
    } else {
      auto const& shardList =
          std::get<std::unordered_set<arangodb::ShardID>>(set);
      return *shardList.begin();
    }
  }

  bool isSafeForOptimization(
      arangodb::aql::Collection const* collection) const {
    auto it = _unsafe.find(collection);
    if (it == _unsafe.end()) {
      return true;
    }
    return !it->second;
  }

  bool isSafeForOptimization(arangodb::aql::Variable const* variable) const {
    auto it = _shardsCleared.find(variable);
    if (it == _shardsCleared.end()) {
      return false;
    }

    if (std::holds_alternative<AllShards>(it->second)) {
      return false;
    } else {
      return std::get<std::unordered_set<arangodb::ShardID>>(it->second)
                 .size() == 1;
    }
  }

  bool enterSubquery(arangodb::aql::ExecutionNode*,
                     arangodb::aql::ExecutionNode*) override final {
    return true;
  }

  bool before(arangodb::aql::ExecutionNode* en) override final {
    switch (en->getType()) {
      case EN::TRAVERSAL:
      case EN::ENUMERATE_PATHS:
      case EN::SHORTEST_PATH: {
        _stop = true;
        return true;
      }

      case EN::FILTER: {
        auto node =
            EN::castTo<arangodb::aql::FilterNode const*>(en);
        arangodb::aql::Variable const* inputVariable = node->inVariable();
        handleInputVariable(en, inputVariable);
        break;
      }

      case EN::ENUMERATE_COLLECTION:
      case EN::INDEX: {
        handleDocumentNode(en);
        handleSourceNode(en);
        break;
      }

      case EN::INSERT:
      case EN::REPLACE:
      case EN::UPDATE:
      case EN::REMOVE: {
        auto node =
            EN::castTo<arangodb::aql::ModificationNode const*>(en);
        _shardsUsed.clear();
        auto shardId = getSingleShardId(_plan, en, node->collection());
        if (!shardId.has_value()) {
          _unsafe[node->collection()] = true;
        }
        break;
      }

      default: {
        break;
      }
    }

    return false;
  }

 private:
  void handleShardOutput(std::optional<arangodb::ShardID> shardId,
                         arangodb::aql::Variable const* variable) {
    if (!shardId.has_value()) {
      if (!_shardsUsed.contains(variable)) {
        _shardsUsed.emplace(variable, AllShards{});
      }
    } else {
      auto it = _shardsUsed.find(variable);
      if (it == _shardsUsed.end() ||
          std::holds_alternative<AllShards>(it->second)) {
        _shardsUsed[variable] =
            std::unordered_set<arangodb::ShardID>{std::move(shardId.value())};
      } else {
        std::get<std::unordered_set<arangodb::ShardID>>(it->second)
            .emplace(std::move(shardId.value()));
      }
    }
  }

  void handleInputVariable(arangodb::aql::ExecutionNode const* en,
                           arangodb::aql::Variable const* inputVariable) {
    auto dependencies = _tracker.getDependencies(inputVariable);
    for (auto dep : dependencies) {
      auto variable = dep.first;
      auto collection = dep.second;
      auto shardId = getSingleShardId(_plan, en, collection, variable);
      handleShardOutput(std::move(shardId), variable);
    }
  }

  void handleDocumentNode(arangodb::aql::ExecutionNode const* en) {
    TRI_ASSERT(en->getType() == EN::INDEX ||
               en->getType() == EN::ENUMERATE_COLLECTION);
    auto collection = arangodb::aql::utils::getCollection(en);
    auto variable = getOutVariable(en);
    auto shardId = getSingleShardId(_plan, en, collection, variable);
    handleShardOutput(std::move(shardId), variable);
  }

  void handleSourceNode(arangodb::aql::ExecutionNode const* en) {
    auto variable = getOutVariable(en);
    _shardsCleared[variable] = std::move(_shardsUsed[variable]);
  }
};

}  // namespace

namespace arangodb::aql {

/// @brief try to restrict fragments to a single shard if possible
void restrictToSingleShardRule(Optimizer* opt,
                               std::unique_ptr<ExecutionPlan> plan,
                               OptimizerRule const& rule) {
  TRI_ASSERT(arangodb::ServerState::instance()->isCoordinator());
  bool wasModified = false;

  CollectionVariableTracker tracker;
  plan->root()->walk(tracker);
  if (!tracker.isSafeForOptimization()) {
    opt->addPlan(std::move(plan), rule, wasModified);
    return;
  }

  RestrictToSingleShardChecker finder(plan.get(), tracker);
  plan->root()->walk(finder);
  if (!finder.isSafeForOptimization()) {
    opt->addPlan(std::move(plan), rule, wasModified);
    return;
  }

  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::REMOTE, true);

  ::arangodb::containers::HashSet<ExecutionNode*> toUnlink;
  std::map<Collection const*, std::unordered_set<ShardID>>
      modificationRestrictions;

  auto forwardRestrictionToPrototype = [&plan](ExecutionNode const* current,
                                               ShardID const& shardId) {
    auto collectionNode = dynamic_cast<CollectionAccessingNode const*>(current);
    if (collectionNode == nullptr) {
      return;
    }
    auto prototypeOutVariable = collectionNode->prototypeOutVariable();
    if (prototypeOutVariable == nullptr) {
      return;
    }

    auto setter = plan->getVarSetBy(prototypeOutVariable->id);
    if (setter == nullptr || (setter->getType() != EN::INDEX &&
                              setter->getType() != EN::ENUMERATE_COLLECTION)) {
      return;
    }

    auto s1 = utils::getCollection(current)->shardIds();
    auto s2 = utils::getCollection(setter)->shardIds();

    if (s1->size() != s2->size()) {
      return;
    }

    for (size_t i = 0; i < s1->size(); ++i) {
      if ((*s1)[i] == shardId) {
        restrictToShard(setter, (*s2)[i]);
        break;
      }
    }
  };

  for (auto& node : nodes) {
    TRI_ASSERT(node->getType() == ExecutionNode::REMOTE);
    ExecutionNode* current = node->getFirstDependency();

    while (current != nullptr) {
      auto const currentType = current->getType();
      if (currentType == ExecutionNode::INSERT ||
          currentType == ExecutionNode::UPDATE ||
          currentType == ExecutionNode::REPLACE ||
          currentType == ExecutionNode::REMOVE) {
        auto collection =
            ExecutionNode::castTo<ModificationNode const*>(current)
                ->collection();
        auto shardId = getSingleShardId(plan.get(), current, collection);

        if (shardId.has_value()) {
          TRI_ASSERT(shardId.value().isValid());
          wasModified = true;
          auto* modNode = ExecutionNode::castTo<ModificationNode*>(current);
          modNode->getOptions().ignoreDocumentNotFound = false;
          modNode->restrictToShard(shardId.value());
          modificationRestrictions[collection].emplace(shardId.value());

          auto const& deps = current->getDependencies();
          if (deps.size() && deps[0]->getType() == ExecutionNode::REMOTE) {
            ::arangodb::containers::HashSet<ExecutionNode*> toRemove;

            auto c = deps[0];
            toRemove.emplace(c);
            while (true) {
              if (c->getType() == EN::SCATTER ||
                  c->getType() == EN::DISTRIBUTE) {
                toRemove.emplace(c);
              }
              c = c->getFirstDependency();

              if (c == nullptr) {
                break;
              }

              if (c->getType() == EN::REMOTE || c->getType() == EN::SUBQUERY) {
                toRemove.clear();
                break;
              }

              if (c->getType() == EN::CALCULATION) {
                TRI_vocbase_t& vocbase = plan->getAst()->query().vocbase();
                auto cn = ExecutionNode::castTo<CalculationNode const*>(c);
                auto expr = cn->expression();
                if (!expr->canRunOnDBServer(vocbase.isOneShard())) {
                  toRemove.clear();
                  break;
                }
              }
            }

            for (auto const& it : toRemove) {
              toUnlink.emplace(it);
            }
          }
        }
      } else if (currentType == ExecutionNode::INDEX ||
                 currentType == ExecutionNode::ENUMERATE_COLLECTION) {
        bool disable = false;
        if (currentType == ExecutionNode::INDEX) {
          for (auto& index :
               ExecutionNode::castTo<aql::IndexNode*>(current)->getIndexes()) {
            if (Index::TRI_IDX_TYPE_INVERTED_INDEX == index->type()) {
              disable = true;
              break;
            }
          }
        }

        if (!disable) {
          auto collection = utils::getCollection(current);
          auto collectionVariable = getOutVariable(current);
          auto shardId = finder.getShard(collectionVariable);

          if (finder.isSafeForOptimization(collectionVariable) &&
              shardId.isValid()) {
            wasModified = true;
            restrictToShard(current, shardId);
            forwardRestrictionToPrototype(current, shardId);
          } else if (finder.isSafeForOptimization(collection)) {
            auto& shards = modificationRestrictions[collection];
            if (shards.size() == 1) {
              wasModified = true;
              shardId = *shards.begin();
              restrictToShard(current, shardId);
              forwardRestrictionToPrototype(current, shardId);
            }
          }
        }
      } else if (currentType == ExecutionNode::UPSERT ||
                 currentType == ExecutionNode::REMOTE ||
                 currentType == ExecutionNode::DISTRIBUTE ||
                 currentType == ExecutionNode::SINGLETON) {
        break;
      }

      current = current->getFirstDependency();
    }
  }

  if (!toUnlink.empty()) {
    plan->unlinkNodes(toUnlink);
  }

  opt->addPlan(std::move(plan), rule, wasModified);
}

}  // namespace arangodb::aql

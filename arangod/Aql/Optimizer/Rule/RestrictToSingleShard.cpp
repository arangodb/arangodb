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
#include "Aql/Optimizer/Utils/GetCollection.h"
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

namespace arangodb::aql {
namespace {

using EN = ExecutionNode;

Variable const* getOutVariable(ExecutionNode const* node) {
  switch (node->getType()) {
    case EN::CALCULATION:
      return EN::castTo<CalculationNode const*>(node)->outVariable();
    default: {
      auto const* n = dynamic_cast<DocumentProducingNode const*>(node);
      if (n != nullptr) {
        return n->outVariable();
      }
      // note: modification nodes are not covered here yet
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "node type does not have an out variable");
    }
  }
}

void restrictToShard(ExecutionNode* node, arangodb::ShardID const& shardId) {
  auto* n = dynamic_cast<CollectionAccessingNode*>(node);
  if (n != nullptr) {
    return n->restrictToShard(shardId);
  }
  // note: modification nodes are not covered here yet
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

void findShardKeyInComparison(AstNode const* root,
                              Variable const* inputVariable,
                              std::unordered_set<std::string>& toFind,
                              arangodb::velocypack::Builder& builder) {
  TRI_ASSERT(root->type == AstNodeType::NODE_TYPE_OPERATOR_BINARY_EQ);

  AstNode const* value = nullptr;
  std::pair<Variable const*, std::vector<arangodb::basics::AttributeName>> pair;

  ast::RelationalOperatorNode eqOp(root);
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

void findShardKeysInExpression(AstNode const* root,
                               Variable const* inputVariable,
                               std::unordered_set<std::string>& toFind,
                               arangodb::velocypack::Builder& builder) {
  if (root == nullptr) {
    return;
  }

  switch (root->type) {
    case AstNodeType::NODE_TYPE_OPERATOR_NARY_OR: {
      ast::NaryOperatorNode naryOr(root);
      auto operands = naryOr.getOperands();
      if (operands.size() != 1) {
        return;
      }
      root = operands[0];
      if (root == nullptr ||
          root->type != AstNodeType::NODE_TYPE_OPERATOR_NARY_AND) {
        return;
      }
    }  // falls through
    case AstNodeType::NODE_TYPE_OPERATOR_BINARY_AND:
    case AstNodeType::NODE_TYPE_OPERATOR_NARY_AND: {
      for (auto* member : root->getMemberList()) {
        if (member != nullptr &&
            member->type == AstNodeType::NODE_TYPE_OPERATOR_BINARY_EQ) {
          findShardKeyInComparison(member, inputVariable, toFind, builder);
        }
      }
      break;
    }
    case AstNodeType::NODE_TYPE_OPERATOR_BINARY_EQ: {
      findShardKeyInComparison(root, inputVariable, toFind, builder);
      break;
    }
    default:
      break;
  }
}

/// @brief find the single shard id for the node to restrict an operation to
/// this will check the conditions of an IndexNode or a data-modification node
/// (excluding UPSERT) and check if all shard keys are used in it. If all
/// shard keys are present and their values are fixed (constants), this
/// function will try to figure out the target shard. If the operation cannot
/// be restricted to a single shard, this function will return an empty string
std::optional<arangodb::ShardID> getSingleShardId(
    ExecutionPlan const* plan, ExecutionNode const* node,
    Collection const* collection,
    Variable const* collectionVariable = nullptr) {
  if (collection->isSmart() &&
      collection->getCollection()->type() == TRI_COL_TYPE_EDGE) {
    // no support for smart edge collections
    return std::nullopt;
  }

  TRI_ASSERT(node->getType() == EN::INDEX ||
             node->getType() == EN::ENUMERATE_COLLECTION ||
             node->getType() == EN::FILTER || node->getType() == EN::INSERT ||
             node->getType() == EN::UPDATE || node->getType() == EN::REPLACE ||
             node->getType() == EN::REMOVE);

  Variable const* inputVariable = nullptr;
  if (node->getType() == EN::INDEX ||
      node->getType() == EN::ENUMERATE_COLLECTION) {
    inputVariable =
        EN::castTo<DocumentProducingNode const*>(node)->outVariable();
  } else if (node->getType() == EN::FILTER) {
    inputVariable = EN::castTo<FilterNode const*>(node)->inVariable();
  } else if (node->getType() == EN::INSERT) {
    inputVariable = EN::castTo<InsertNode const*>(node)->inVariable();
  } else if (node->getType() == EN::REMOVE) {
    inputVariable = EN::castTo<RemoveNode const*>(node)->inVariable();
  } else if (node->getType() == EN::REPLACE || node->getType() == EN::UPDATE) {
    auto updateReplaceNode = EN::castTo<UpdateReplaceNode const*>(node);
    if (updateReplaceNode->inKeyVariable() != nullptr) {
      inputVariable = updateReplaceNode->inKeyVariable();
    } else {
      inputVariable = updateReplaceNode->inDocVariable();
    }
  } else {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "logic error");
  }

  TRI_ASSERT(inputVariable != nullptr);

  // check if we can easily find out the setter of the input variable
  // (and if we can find it, check if the data is constant so we can look
  // up the shard key attribute values)
  auto setter = plan->getVarSetBy(inputVariable->id);

  if (setter == nullptr) {
    // oops!
    TRI_ASSERT(false);
    return std::nullopt;
  }

  // note for which shard keys we need to look for
  auto shardKeys = collection->shardKeys(true);
  std::unordered_set<std::string> toFind;
  for (auto const& it : shardKeys) {
    if (it.find('.') != std::string::npos) {
      // shard key containing a "." (sub-attribute). this is not yet supported
      return std::nullopt;
    }
    toFind.emplace(it);
  }

  auto sb = arangodb::velocypack::SupervisedBuffer(
      plan->getAst()->query().resourceMonitor());
  VPackBuilder builder(sb);
  builder.openObject();

  if (setter->getType() == EN::CALCULATION) {
    CalculationNode const* c = EN::castTo<CalculationNode const*>(setter);
    auto n = c->expression()->node();
    if (n == nullptr) {
      return std::nullopt;
    }

    if (n->isStringValue()) {
      if (!n->isConstant() || toFind.size() != 1 ||
          toFind.find(arangodb::StaticStrings::KeyString) == toFind.end()) {
        return std::nullopt;
      }

      // the lookup value is a string, and the only shard key is _key: so we
      // can use it
      builder.add(VPackValue(arangodb::StaticStrings::KeyString));
      n->toVelocyPackValue(builder);
      toFind.clear();
    } else if (n->isObject()) {
      // go through the input object attribute by attribute
      // and look for our shard keys
      for (size_t i = 0; i < n->numMembers(); ++i) {
        auto sub = n->getMember(i);

        if (sub->type != AstNodeType::NODE_TYPE_OBJECT_ELEMENT) {
          continue;
        }

        ast::ObjectElementNode objElem(sub);
        auto it = toFind.find(sub->getString());

        if (it != toFind.end()) {
          // we found one of the shard keys!
          auto v = objElem.getValue();
          if (v->isConstant()) {
            // if the attribute value is a constant, we copy it into our
            // builder
            builder.add(VPackValue(sub->getString()));
            v->toVelocyPackValue(builder);
            // remove the attribute from our to-do list
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
    auto const* c = EN::castTo<IndexNode const*>(setter);

    if (c->getIndexes().size() != 1) {
      // we can only handle a single index here
      return std::nullopt;
    }
    auto const* condition = c->condition();

    if (condition != nullptr) {
      AstNode const* root = condition->root();
      findShardKeysInExpression(root, inputVariable, toFind, builder);
    }
  }

  if (!toFind.empty() && (node->getType() == EN::INDEX ||
                          node->getType() == EN::ENUMERATE_COLLECTION)) {
    auto en = dynamic_cast<DocumentProducingNode const*>(node);
    TRI_ASSERT(en != nullptr);

    if (en->hasFilter()) {
      AstNode const* root = en->filter()->node();
      findShardKeysInExpression(root, inputVariable, toFind, builder);
    }
  }

  builder.close();

  if (!toFind.empty()) {
    return std::nullopt;
  }

  // all shard keys found!!

  if (node->getType() == EN::INSERT && collection->numberOfShards() != 1 &&
      (shardKeys.size() != 1 ||
       shardKeys[0] != arangodb::StaticStrings::KeyString) &&
      builder.slice().get(arangodb::StaticStrings::KeyString).isNone()) {
    // insert into a collection with more than one shard or custom shard keys,
    // and _key is not given in inputs.
    return std::nullopt;
  }

  // find the responsible shard for the data
  std::string shardId;

  auto res =
      collection->getCollection()->getResponsibleShard(builder.slice(), true);

  if (res.fail()) {
    // some error occurred. better do not use the
    // single shard optimization here
    return std::nullopt;
  }

  // we will only need a single shard!
  TRI_ASSERT(res.get().isValid());
  return std::move(res.get());
}

/// WalkerWorker to track collection variable dependencies
class CollectionVariableTracker final
    : public WalkerWorker<ExecutionNode, WalkerUniqueness::NonUnique> {
  using DependencyPair = std::pair<Variable const*, Collection const*>;
  using DependencySet = std::unordered_set<DependencyPair, PairHash>;
  bool _stop;
  std::unordered_map<Variable const*, DependencySet> _dependencies;
  std::unordered_map<Collection const*, VarSet> _collectionVariables;

 private:
  template<class NodeType>
  void processSetter(ExecutionNode const* en, Variable const* outVariable) {
    auto node = EN::castTo<NodeType const*>(en);
    try {
      VarSet inputVariables;
      node->getVariablesUsedHere(inputVariables);
      for (auto var : inputVariables) {
        for (auto dep : _dependencies[var]) {
          _dependencies[outVariable].emplace(dep);
        }
      }
    } catch (...) {
      _stop = true;  // won't be able to recover correctly
    }
  }

  template<class NodeType>
  void processModificationNode(ExecutionNode const* en) {
    auto node = EN::castTo<NodeType const*>(en);
    auto collection = node->collection();
    std::vector<Variable const*> outVariables{node->getOutVariableOld(),
                                              node->getOutVariableNew()};
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

  DependencySet const& getDependencies(Variable const* var) {
    return _dependencies[var];
  }

  void after(ExecutionNode* en) override final {
    switch (en->getType()) {
      case EN::CALCULATION: {
        auto outVariable = getOutVariable(en);
        processSetter<CalculationNode>(en, outVariable);
        break;
      }

      case EN::INDEX:
      case EN::ENUMERATE_COLLECTION: {
        auto collection = optimizer::getCollection(en);
        auto variable = getOutVariable(en);

        // originates the collection variable, direct dependence
        try {
          _dependencies[variable].emplace(variable, collection);
          _collectionVariables[collection].emplace(variable);
        } catch (...) {
          _stop = true;  // we won't be able to figure it out
        }
        break;
      }

      case EN::UPDATE: {
        processModificationNode<UpdateNode>(en);
        break;
      }

      case EN::UPSERT: {
        processModificationNode<UpsertNode>(en);
        break;
      }

      case EN::INSERT: {
        processModificationNode<InsertNode>(en);
        break;
      }

      case EN::REMOVE: {
        processModificationNode<RemoveNode>(en);
        break;
      }

      case EN::REPLACE: {
        processModificationNode<ReplaceNode>(en);
        break;
      }

      default: {
        // we don't support other node types yet
        break;
      }
    }
  }
};

/// WalkerWorker for restrictToSingleShard
class RestrictToSingleShardChecker final
    : public WalkerWorker<ExecutionNode, WalkerUniqueness::NonUnique> {
  struct AllShards {};

  ExecutionPlan* _plan;
  CollectionVariableTracker& _tracker;
  std::unordered_map<
      Variable const*,
      std::variant<AllShards, std::unordered_set<arangodb::ShardID>>>
      _shardsUsed;
  std::unordered_map<
      Variable const*,
      std::variant<AllShards, std::unordered_set<arangodb::ShardID>>>
      _shardsCleared;
  bool _stop;
  std::map<Collection const*, bool> _unsafe;

 public:
  explicit RestrictToSingleShardChecker(ExecutionPlan* plan,
                                        CollectionVariableTracker& tracker)
      : _plan{plan}, _tracker{tracker}, _stop{false} {}

  bool isSafeForOptimization() const {
    // we have found something in the execution plan that will
    // render the optimization unsafe
    return (!_stop && !_plan->getAst()->functionsMayAccessDocuments());
  }

  arangodb::ShardID getShard(Variable const* variable) const {
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

  bool isSafeForOptimization(Collection const* collection) const {
    auto it = _unsafe.find(collection);
    if (it == _unsafe.end()) {
      return true;
    }
    return !it->second;
  }

  bool isSafeForOptimization(Variable const* variable) const {
    auto it = _shardsCleared.find(variable);
    if (it == _shardsCleared.end()) {
      return false;
    }

    // check for "all" marker
    if (std::holds_alternative<AllShards>(it->second)) {
      // We do have ALL
      return false;
    } else {
      // If we have exactly one shard, we can optimize
      return std::get<std::unordered_set<arangodb::ShardID>>(it->second)
                 .size() == 1;
    }
  }

  bool enterSubquery(ExecutionNode*, ExecutionNode*) override final {
    return true;
  }

  bool before(ExecutionNode* en) override final {
    switch (en->getType()) {
      case EN::TRAVERSAL:
      case EN::ENUMERATE_PATHS:
      case EN::SHORTEST_PATH: {
        _stop = true;
        return true;  // abort enumerating, we are done already!
      }

      case EN::FILTER: {
        auto node = EN::castTo<FilterNode const*>(en);
        Variable const* inputVariable = node->inVariable();
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
        auto node = EN::castTo<ModificationNode const*>(en);
        // make sure we don't restrict this collection via a lower filter
        _shardsUsed.clear();
        auto shardId = getSingleShardId(_plan, en, node->collection());
        if (!shardId.has_value()) {
          // mark the collection unsafe to restrict
          _unsafe[node->collection()] = true;
        }
        // no need to track the shardId, we'll find it again later
        break;
      }

      default: {
        // we don't care about other execution node types here
        break;
      }
    }

    return false;  // go on
  }

 private:
  void handleShardOutput(std::optional<arangodb::ShardID> shardId,
                         Variable const* variable) {
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

  void handleInputVariable(ExecutionNode const* en,
                           Variable const* inputVariable) {
    auto dependencies = _tracker.getDependencies(inputVariable);
    for (auto dep : dependencies) {
      auto variable = dep.first;
      auto collection = dep.second;
      auto shardId = getSingleShardId(_plan, en, collection, variable);
      handleShardOutput(std::move(shardId), variable);
    }
  }

  void handleDocumentNode(ExecutionNode const* en) {
    TRI_ASSERT(en->getType() == EN::INDEX ||
               en->getType() == EN::ENUMERATE_COLLECTION);
    auto collection = optimizer::getCollection(en);
    auto variable = getOutVariable(en);
    auto shardId = getSingleShardId(_plan, en, collection, variable);
    handleShardOutput(std::move(shardId), variable);
  }

  void handleSourceNode(ExecutionNode const* en) {
    auto variable = getOutVariable(en);
    // now move all shards for this variable to the cleared list
    _shardsCleared[variable] = std::move(_shardsUsed[variable]);
  }
};

}  // namespace

/// @brief try to restrict fragments to a single shard if possible
void restrictToSingleShardRule(Optimizer* opt,
                               std::unique_ptr<ExecutionPlan> plan,
                               OptimizerRule const& rule) {
  TRI_ASSERT(arangodb::ServerState::instance()->isCoordinator());
  bool wasModified = false;

  CollectionVariableTracker tracker;
  plan->root()->walk(tracker);
  if (!tracker.isSafeForOptimization()) {
    // encountered errors while working on optimization, do not continue
    opt->addPlan(std::move(plan), rule, wasModified);
    return;
  }

  RestrictToSingleShardChecker finder(plan.get(), tracker);
  plan->root()->walk(finder);
  if (!finder.isSafeForOptimization()) {
    // found something in the execution plan that renders the optimization
    // unsafe, so do not optimize
    opt->addPlan(std::move(plan), rule, wasModified);
    return;
  }

  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::REMOTE, true);

  ::arangodb::containers::HashSet<ExecutionNode*> toUnlink;
  std::map<Collection const*, std::unordered_set<ShardID>>
      modificationRestrictions;

  // forward a shard key restriction from one collection to the other if the two
  // collections are used in a SmartJoin (and use distributeShardsLike on each
  // other)
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

    auto s1 = optimizer::getCollection(current)->shardIds();
    auto s2 = optimizer::getCollection(setter)->shardIds();

    if (s1->size() != s2->size()) {
      // different number of shard ids... should not happen if we have a
      // prototype
      return;
    }

    // find matching shard key
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
          // we are on a single shard. we must not ignore not-found documents
          // now
          auto* modNode = ExecutionNode::castTo<ModificationNode*>(current);
          modNode->getOptions().ignoreDocumentNotFound = false;
          modNode->restrictToShard(shardId.value());
          modificationRestrictions[collection].emplace(shardId.value());

          auto const& deps = current->getDependencies();
          if (deps.size() && deps[0]->getType() == ExecutionNode::REMOTE) {
            // if we can apply the single-shard optimization, but still have a
            // REMOTE node in front of us, we can probably move the remote
            // parts of the query to our side. this is only the case if the
            // remote part does not call any remote parts itself
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
                // reached the end
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
                  // found something that must not run on a DB server,
                  // but that must run on a coordinator. stop optimization here!
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
          // Custom analyzer on inverted indexes might be incompatible with
          // shard key distribution.
          for (auto& index :
               ExecutionNode::castTo<aql::IndexNode*>(current)->getIndexes()) {
            if (IndexType::Inverted == index->type()) {
              disable = true;
              break;
            }
          }
        }

        if (!disable) {
          auto collection = optimizer::getCollection(current);
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
        // we reached a new snippet or the end of the plan - we can abort
        // searching now. additionally, we cannot yet handle UPSERT well
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

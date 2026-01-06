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
/// @author Julia Puget
/// @author Koichi Nakata
////////////////////////////////////////////////////////////////////////////////

#include "OptimizerRulesClusterDistribution.h"

#include "Aql/Aggregator.h"
#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/CollectNode.h"
#include "Aql/ExecutionNode/DistributeNode.h"
#include "Aql/ExecutionNode/EnumeratePathsNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/FilterNode.h"
#include "Aql/ExecutionNode/GatherNode.h"
#include "Aql/ExecutionNode/IndexNode.h"
#include "Aql/ExecutionNode/InsertNode.h"
#include "Aql/ExecutionNode/IResearchViewNode.h"
#include "Aql/ExecutionNode/RemoteNode.h"
#include "Aql/ExecutionNode/RemoveNode.h"
#include "Aql/ExecutionNode/ReplaceNode.h"
#include "Aql/ExecutionNode/ShortestPathNode.h"
#include "Aql/ExecutionNode/SortNode.h"
#include "Aql/ExecutionNode/SubqueryNode.h"
#include "Aql/ExecutionNode/TraversalNode.h"
#include "Aql/ExecutionNode/UpdateNode.h"
#include "Aql/ExecutionNode/UpsertNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Function.h"
#include "Aql/Optimizer.h"
#include "Aql/OptimizerUtils.h"
#include "Aql/TypedAstNodes.h"
#include "Aql/Variable.h"
#include "Basics/StaticStrings.h"
#include "Containers/SmallVector.h"
#include "Indexes/Index.h"

using namespace arangodb;
using namespace arangodb::aql;
using EN = arangodb::aql::ExecutionNode;

namespace {

static constexpr std::initializer_list<arangodb::aql::ExecutionNode::NodeType>
    undistributeNodeTypes{arangodb::aql::ExecutionNode::UPDATE,
                          arangodb::aql::ExecutionNode::REPLACE,
                          arangodb::aql::ExecutionNode::REMOVE};

std::optional<arangodb::ShardID> getSingleShardId(
    arangodb::aql::ExecutionPlan const* plan,
    arangodb::aql::ExecutionNode const* node,
    arangodb::aql::Collection const* collection,
    arangodb::aql::Variable const* collectionVariable = nullptr);

arangodb::aql::Variable const* getOutVariable(
    arangodb::aql::ExecutionNode const* node) {
  using EN = arangodb::aql::ExecutionNode;
  using arangodb::aql::ExecutionNode;

  switch (node->getType()) {
    case EN::CALCULATION:
      return ExecutionNode::castTo<arangodb::aql::CalculationNode const*>(node)
          ->outVariable();
    default: {
      auto const* n =
          dynamic_cast<arangodb::aql::DocumentProducingNode const*>(node);
      if (n != nullptr) {
        return n->outVariable();
      }
      // note: modification nodes are not covered here yet
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "node type does not have an out variable");
    }
  }
}

void replaceGatherNodeVariables(
    arangodb::aql::ExecutionPlan* plan, arangodb::aql::GatherNode* gatherNode,
    std::unordered_map<arangodb::aql::Variable const*,
                       arangodb::aql::Variable const*> const& replacements) {
  using EN = arangodb::aql::ExecutionNode;

  std::string cmp;
  std::string buffer;

  // look for all sort elements in the GatherNode and replace them
  // if they match what we have changed
  arangodb::aql::SortElementVector& elements = gatherNode->elements();
  for (auto& it : elements) {
    // replace variables
    auto it2 = replacements.find(it.var);

    if (it2 != replacements.end()) {
      // match with our replacement table
      it.resetTo((*it2).second);
    } else {
      // no match. now check all our replacements and compare how
      // their sources are actually calculated (e.g. #2 may mean
      // "foo.bar")
      cmp = it.toVarAccessString();
      for (auto const& it3 : replacements) {
        auto setter = plan->getVarSetBy(it3.first->id);
        if (setter == nullptr || setter->getType() != EN::CALCULATION) {
          continue;
        }
        auto* expr = arangodb::aql::ExecutionNode::castTo<
                         arangodb::aql::CalculationNode const*>(setter)
                         ->expression();
        buffer.clear();
        expr->stringify(buffer);
        if (cmp == buffer) {
          // finally a match!
          it.resetTo(it3.second);
          break;
        }
      }
    }
  }
}

void restrictToShard(arangodb::aql::ExecutionNode* node,
                     arangodb::ShardID const& shardId) {
  auto* n = dynamic_cast<arangodb::aql::CollectionAccessingNode*>(node);
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

/// WalkerWorker to track collection variable dependencies
class CollectionVariableTracker final
    : public arangodb::aql::WalkerWorker<
          arangodb::aql::ExecutionNode,
          arangodb::aql::WalkerUniqueness::NonUnique> {
  using DependencyPair = std::pair<arangodb::aql::Variable const*,
                                   arangodb::aql::Collection const*>;
  using DependencySet = std::unordered_set<DependencyPair, ::PairHash>;
  bool _stop;
  std::unordered_map<arangodb::aql::Variable const*, DependencySet>
      _dependencies;
  std::unordered_map<arangodb::aql::Collection const*, arangodb::aql::VarSet>
      _collectionVariables;

 private:
  template<class NodeType>
  void processSetter(arangodb::aql::ExecutionNode const* en,
                     arangodb::aql::Variable const* outVariable) {
    auto node = arangodb::aql::ExecutionNode::castTo<NodeType const*>(en);
    try {
      arangodb::aql::VarSet inputVariables;
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
  void processModificationNode(arangodb::aql::ExecutionNode const* en) {
    auto node = arangodb::aql::ExecutionNode::castTo<NodeType const*>(en);
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
    using EN = arangodb::aql::ExecutionNode;
    using arangodb::aql::ExecutionNode;

    switch (en->getType()) {
      case EN::CALCULATION: {
        auto outVariable = ::getOutVariable(en);
        processSetter<arangodb::aql::CalculationNode>(en, outVariable);
        break;
      }

      case EN::INDEX:
      case EN::ENUMERATE_COLLECTION: {
        auto collection = arangodb::aql::utils::getCollection(en);
        auto variable = ::getOutVariable(en);

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
        // we don't support other node types yet
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
    // we have found something in the execution plan that will
    // render the optimization unsafe
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

  bool enterSubquery(arangodb::aql::ExecutionNode*,
                     arangodb::aql::ExecutionNode*) override final {
    return true;
  }

  bool before(arangodb::aql::ExecutionNode* en) override final {
    using EN = arangodb::aql::ExecutionNode;
    using arangodb::aql::ExecutionNode;

    switch (en->getType()) {
      case EN::TRAVERSAL:
      case EN::ENUMERATE_PATHS:
      case EN::SHORTEST_PATH: {
        _stop = true;
        return true;  // abort enumerating, we are done already!
      }

      case EN::FILTER: {
        auto node = ExecutionNode::castTo<arangodb::aql::FilterNode const*>(en);
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
            ExecutionNode::castTo<arangodb::aql::ModificationNode const*>(en);
        // make sure we don't restrict this collection via a lower filter
        _shardsUsed.clear();
        auto shardId = ::getSingleShardId(_plan, en, node->collection());
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
      auto shardId = ::getSingleShardId(_plan, en, collection, variable);
      handleShardOutput(std::move(shardId), variable);
    }
  }

  void handleDocumentNode(arangodb::aql::ExecutionNode const* en) {
    TRI_ASSERT(en->getType() == arangodb::aql::ExecutionNode::INDEX ||
               en->getType() ==
                   arangodb::aql::ExecutionNode::ENUMERATE_COLLECTION);
    auto collection = arangodb::aql::utils::getCollection(en);
    auto variable = ::getOutVariable(en);
    auto shardId = ::getSingleShardId(_plan, en, collection, variable);
    handleShardOutput(std::move(shardId), variable);
  }

  void handleSourceNode(arangodb::aql::ExecutionNode const* en) {
    auto variable = ::getOutVariable(en);
    // now move all shards for this variable to the cleared list
    _shardsCleared[variable] = std::move(_shardsUsed[variable]);
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
      for (size_t i = 0; i < root->numMembers(); ++i) {
        auto member = root->getMemberUnchecked(i);
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

/// @brief find the single shard id for the node to restrict an operation to
/// this will check the conditions of an IndexNode or a data-modification node
/// (excluding UPSERT) and check if all shard keys are used in it. If all
/// shard keys are present and their values are fixed (constants), this
/// function will try to figure out the target shard. If the operation cannot
/// be restricted to a single shard, this function will return an empty string
std::optional<arangodb::ShardID> getSingleShardId(
    arangodb::aql::ExecutionPlan const* plan,
    arangodb::aql::ExecutionNode const* node,
    arangodb::aql::Collection const* collection,
    arangodb::aql::Variable const* collectionVariable) {
  using EN = arangodb::aql::ExecutionNode;
  using arangodb::aql::ExecutionNode;

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

  arangodb::aql::Variable const* inputVariable = nullptr;
  if (node->getType() == EN::INDEX ||
      node->getType() == EN::ENUMERATE_COLLECTION) {
    inputVariable =
        ExecutionNode::castTo<arangodb::aql::DocumentProducingNode const*>(node)
            ->outVariable();
  } else if (node->getType() == EN::FILTER) {
    inputVariable =
        ExecutionNode::castTo<arangodb::aql::FilterNode const*>(node)
            ->inVariable();
  } else if (node->getType() == EN::INSERT) {
    inputVariable =
        ExecutionNode::castTo<arangodb::aql::InsertNode const*>(node)
            ->inVariable();
  } else if (node->getType() == EN::REMOVE) {
    inputVariable =
        ExecutionNode::castTo<arangodb::aql::RemoveNode const*>(node)
            ->inVariable();
  } else if (node->getType() == EN::REPLACE || node->getType() == EN::UPDATE) {
    auto updateReplaceNode =
        ExecutionNode::castTo<arangodb::aql::UpdateReplaceNode const*>(node);
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
    arangodb::aql::CalculationNode const* c =
        ExecutionNode::castTo<arangodb::aql::CalculationNode const*>(setter);
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
        auto sub = n->getMemberUnchecked(i);

        if (sub->type != arangodb::aql::AstNodeType::NODE_TYPE_OBJECT_ELEMENT) {
          continue;
        }

        arangodb::aql::ast::ObjectElementNode objElem(sub);
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
        ::findShardKeysInExpression(n, collectionVariable, toFind, builder);
      } else {
        ::findShardKeysInExpression(n, inputVariable, toFind, builder);
      }
    }
  } else if (setter->getType() == ExecutionNode::INDEX && setter == node) {
    auto const* c =
        ExecutionNode::castTo<arangodb::aql::IndexNode const*>(setter);

    if (c->getIndexes().size() != 1) {
      // we can only handle a single index here
      return std::nullopt;
    }
    auto const* condition = c->condition();

    if (condition != nullptr) {
      arangodb::aql::AstNode const* root = condition->root();
      ::findShardKeysInExpression(root, inputVariable, toFind, builder);
    }
  }

  if (!toFind.empty() && (node->getType() == EN::INDEX ||
                          node->getType() == EN::ENUMERATE_COLLECTION)) {
    auto en = dynamic_cast<arangodb::aql::DocumentProducingNode const*>(node);
    TRI_ASSERT(en != nullptr);

    if (en->hasFilter()) {
      arangodb::aql::AstNode const* root = en->filter()->node();
      ::findShardKeysInExpression(root, inputVariable, toFind, builder);
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
}  // namespace

// Create a new DistributeNode for the ExecutionNode passed in node, and
// register it with the plan
auto arangodb::aql::createDistributeNodeFor(ExecutionPlan& plan,
                                            ExecutionNode* node)
    -> DistributeNode* {
  auto collection = static_cast<Collection const*>(nullptr);
  auto inputVariable = static_cast<Variable const*>(nullptr);

  bool isTraversalNode = false;
  // TODO: this seems a bit verbose, but is at least local & simple
  //       the modification nodes are all collectionaccessing, the graph nodes
  //       are currently assumed to be disjoint, and hence smart, so all
  //       collections are sharded the same way!
  switch (node->getType()) {
    case ExecutionNode::INSERT: {
      auto const* insertNode = ExecutionNode::castTo<InsertNode const*>(node);
      collection = insertNode->collection();
      inputVariable = insertNode->inVariable();
    } break;
    case ExecutionNode::REMOVE: {
      auto const* removeNode = ExecutionNode::castTo<RemoveNode const*>(node);
      collection = removeNode->collection();
      inputVariable = removeNode->inVariable();
    } break;
    case ExecutionNode::UPDATE:
    case ExecutionNode::REPLACE: {
      auto const* updateReplaceNode =
          ExecutionNode::castTo<UpdateReplaceNode const*>(node);
      collection = updateReplaceNode->collection();
      if (updateReplaceNode->inKeyVariable() != nullptr) {
        inputVariable = updateReplaceNode->inKeyVariable();
      } else {
        inputVariable = updateReplaceNode->inDocVariable();
      }
    } break;
    case ExecutionNode::UPSERT: {
      auto upsertNode = ExecutionNode::castTo<UpsertNode const*>(node);
      collection = upsertNode->collection();
      inputVariable = upsertNode->inDocVariable();
    } break;
    case ExecutionNode::TRAVERSAL: {
      auto traversalNode = ExecutionNode::castTo<TraversalNode const*>(node);
      TRI_ASSERT(traversalNode->isDisjoint());
      collection = traversalNode->collection();
      inputVariable = traversalNode->inVariable();
      isTraversalNode = true;
    } break;
    case ExecutionNode::ENUMERATE_PATHS: {
      auto pathsNode = ExecutionNode::castTo<EnumeratePathsNode const*>(node);
      TRI_ASSERT(pathsNode->isDisjoint());
      collection = pathsNode->collection();
      // Subtle: EnumeratePathsNode uses a reference when returning
      // startInVariable
      inputVariable = &pathsNode->startInVariable();
    } break;
    case ExecutionNode::SHORTEST_PATH: {
      auto shortestPathNode =
          ExecutionNode::castTo<ShortestPathNode const*>(node);
      TRI_ASSERT(shortestPathNode->isDisjoint());
      collection = shortestPathNode->collection();
      inputVariable = shortestPathNode->startInVariable();
    } break;
    default: {
      TRI_ASSERT(false);
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL,
          absl::StrCat("Cannot distribute ", node->getTypeString(), "."));
    }
  }

  TRI_ASSERT(collection != nullptr);
  TRI_ASSERT(inputVariable != nullptr);

  // The DistributeNode needs specially prepared input, but we do not want to
  // insert the calculation for that just yet, because it would interfere with
  // some optimizations, in particular those that might completely remove the
  // DistributeNode (which would) also render the calculation pointless. So
  // instead we insert this calculation in a post-processing step when
  // finalizing the plan in the Optimizer.
  auto distNode = plan.createNode<DistributeNode>(
      &plan, plan.nextId(), ScatterNode::ScatterType::SHARD, collection,
      inputVariable, node->id());

  if (isTraversalNode) {
#ifdef USE_ENTERPRISE
    // Only relevant for Disjoint Smart Graphs that can only be part of the
    // Enterprise version
    // ShortestPath, and K_SHORTEST_PATH will handle satellites differently.
    auto graphNode = ExecutionNode::castTo<GraphNode const*>(node);
    auto vertices = graphNode->vertexColls();
    for (auto const& it : vertices) {
      if (it->isSatellite()) {
        distNode->addSatellite(it);
      }
    }
#endif
  }
  TRI_ASSERT(distNode != nullptr);
  return distNode;
}

// Create a new GatherNode for the DistributeNode passed in node, and
// register it with the plan
//
// TODO: Really Scatter/Gather and Distribute/Gather should be created in pairs.
auto arangodb::aql::createGatherNodeFor(ExecutionPlan& plan,
                                        DistributeNode* node) -> GatherNode* {
  auto const collection = node->collection();

  auto const sortMode =
      GatherNode::evaluateSortMode(collection->numberOfShards());
  auto const parallelism = GatherNode::Parallelism::Undefined;
  return plan.createNode<GatherNode>(&plan, plan.nextId(), sortMode,
                                     parallelism);
}

//
// for a node `at` of type
//  - INSERT, REMOVE, UPDATE, REPLACE, UPSERT
//  - TRAVERSAL, SHORTEST_PATH, K_SHORTEST_PATHS,
// we transform
//
// parents[0] -> `node` -> deps[0]
//
// into
//
// parents[0] -> GATHER -> REMOTE -> `node` -> REMOTE -> DISTRIBUTE -> deps[0]
//
// We can only handle the above mentioned node types, because the setup of
// distribute and gather requires knowledge from these nodes.
//
// Note that parents[0] might be `nullptr` if `node` is the root of the plan,
// and we handle this case in here as well by resetting the root to the
// inserted GATHER node.
//
auto arangodb::aql::insertDistributeGatherSnippet(ExecutionPlan& plan,
                                                  ExecutionNode* at,
                                                  SubqueryNode* snode)
    -> DistributeNode* {
  auto const parents = at->getParents();
  auto const deps = at->getDependencies();

  // This transforms `parents[0] -> node -> deps[0]` into `parents[0] ->
  // deps[0]`
  plan.unlinkNode(at, true);

  // create, and register a distribute node
  DistributeNode* distNode = createDistributeNodeFor(plan, at);
  TRI_ASSERT(distNode != nullptr);
  TRI_ASSERT(deps.size() == 1);
  distNode->addDependency(deps[0]);

  // TODO: This dance is only needed to extract vocbase for
  //       creating the remote node. The vocbase parameter for
  //       the remote node does not seem to be really needed, since
  //       the vocbase is stored in plan (and this variable is actually used in)
  //       some code, so maybe this parameter could be removed?
  auto const* collection = distNode->collection();
  TRI_vocbase_t* vocbase = collection->vocbase();

  // insert a remote node
  ExecutionNode* remoteNode =
      plan.createNode<RemoteNode>(&plan, plan.nextId(), vocbase, "", "", "");
  remoteNode->addDependency(distNode);

  // re-link with the remote node
  at->addDependency(remoteNode);

  // insert another remote node
  remoteNode =
      plan.createNode<RemoteNode>(&plan, plan.nextId(), vocbase, "", "", "");
  remoteNode->addDependency(at);

  // insert a gather node matching the distribute node
  auto* gatherNode = createGatherNodeFor(plan, distNode);
  gatherNode->addDependency(remoteNode);

  TRI_ASSERT(parents.size() < 2);
  // Song and dance to deal with at being the root of a plan or a subquery
  if (parents.empty()) {
    if (snode) {
      if (snode->getSubquery() == at) {
        snode->setSubquery(gatherNode, true);
      }
    } else {
      plan.root(gatherNode, true);
    }
  } else {
    // This is correct: Since we transformed `parents[0] -> node -> deps[0]`
    // into `parents[0] -> deps[0]` above, created
    //
    // gather -> remote -> node -> remote -> distribute -> deps[0]
    // and now make the plan consistent again by splicing in our snippet.
    parents[0]->replaceDependency(deps[0], gatherNode);
  }
  return ExecutionNode::castTo<DistributeNode*>(distNode);
}

auto extractSmartnessAndCollection(ExecutionNode* node)
    -> std::tuple<bool, bool, Collection const*> {
  auto nodeType = node->getType();
  auto collection = static_cast<Collection const*>(nullptr);
  auto isSmart = bool{false};
  auto isDisjoint = bool{false};

  if (nodeType == ExecutionNode::TRAVERSAL ||
      nodeType == ExecutionNode::SHORTEST_PATH ||
      nodeType == ExecutionNode::ENUMERATE_PATHS) {
    auto const* graphNode = ExecutionNode::castTo<GraphNode*>(node);

    isSmart = graphNode->isSmart();
    isDisjoint = graphNode->isDisjoint();

    // Note that here we are in the Disjoint SmartGraph case and "collection()"
    // will give us any collection in the graph, but they're all sharded the
    // same way.
    collection = graphNode->collection();

  } else {
    auto const* collectionAccessingNode =
        dynamic_cast<CollectionAccessingNode*>(node);
    TRI_ASSERT(collectionAccessingNode != nullptr);

    collection = collectionAccessingNode->collection();
    isSmart = collection->isSmart();
  }

  return std::tuple<bool, bool, Collection const*>{isSmart, isDisjoint,
                                                   collection};
}

/// @brief distribute operations in cluster
///
/// this rule inserts distribute, remote nodes so operations on sharded
/// collections actually work, this differs from scatterInCluster in that every
/// incoming row is only sent to one shard and not all as in scatterInCluster
///
/// it will change plans in place

auto isGraphNode(ExecutionNode::NodeType nodeType) noexcept -> bool {
  return nodeType == ExecutionNode::TRAVERSAL ||
         nodeType == ExecutionNode::SHORTEST_PATH ||
         nodeType == ExecutionNode::ENUMERATE_PATHS;
}

auto isModificationNode(ExecutionNode::NodeType nodeType) noexcept -> bool {
  return nodeType == ExecutionNode::INSERT ||
         nodeType == ExecutionNode::REMOVE ||
         nodeType == ExecutionNode::UPDATE ||
         nodeType == ExecutionNode::REPLACE ||
         nodeType == ExecutionNode::UPSERT;
}

auto nodeEligibleForDistribute(ExecutionNode::NodeType nodeType) noexcept
    -> bool {
  return isModificationNode(nodeType) || isGraphNode(nodeType);
}

void arangodb::aql::distributeInClusterRule(Optimizer* opt,
                                            std::unique_ptr<ExecutionPlan> plan,
                                            OptimizerRule const& rule) {
  TRI_ASSERT(arangodb::ServerState::instance()->isCoordinator());
  bool wasModified = false;
  // we are a coordinator, we replace the root if it is a modification node

  // only replace if it is the last node in the plan
  containers::SmallVector<ExecutionNode*, 8> subqueryNodes;
  // inspect each return node and work upwards to SingletonNode
  subqueryNodes.push_back(plan->root());
  plan->findNodesOfType(subqueryNodes, ExecutionNode::SUBQUERY, true);

  for (ExecutionNode* subqueryNode : subqueryNodes) {
    SubqueryNode* snode = nullptr;
    ExecutionNode* root = nullptr;  // only used for asserts
    bool reachedEnd = false;
    if (subqueryNode == plan->root()) {
      snode = nullptr;
      root = plan->root();
    } else {
      snode = ExecutionNode::castTo<SubqueryNode*>(subqueryNode);
      root = snode->getSubquery();
    }
    ExecutionNode* node = root;
    TRI_ASSERT(node != nullptr);

    // TODO: we might be able to use a walker here?
    while (node != nullptr) {
      auto nodeType = node->getType();

      // loop until we find a modification node or the end of the plan
      while (node != nullptr) {
        // update type
        nodeType = node->getType();

        // check if there is a node type that needs distribution
        if (nodeEligibleForDistribute(nodeType)) {
          // found a node!
          break;
        }

        // there is nothing above us
        if (!node->hasDependency()) {
          // reached the end
          reachedEnd = true;
          break;
        }

        // go further up the tree
        node = node->getFirstDependency();
      }

      if (reachedEnd) {
        // break loop for subquery
        break;
      }

      TRI_ASSERT(node != nullptr);
      if (node == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "logic error");
      }

      // when we get here, we have found a matching data-modification or
      // traversal/shortest_path/k_shortest_paths node!
      TRI_ASSERT(nodeEligibleForDistribute(nodeType));

      auto const [isSmart, isDisjoint, collection] =
          extractSmartnessAndCollection(node);

#ifdef USE_ENTERPRISE
      if (isSmart) {
        node =
            distributeInClusterRuleSmart(plan.get(), snode, node, wasModified);
        // TODO: MARKUS CHECK WHEN YOU NEED TO CONTINUE HERE!
        //       We want to just handle all smart collections here, so we
        //       probably just want to always continue
        // continue;
      }
#endif

      TRI_ASSERT(collection != nullptr);
      bool const defaultSharding = collection->usesDefaultSharding();

      // If the collection does not use default sharding, we have to use a
      // scatter node this is because we might only have a _key for REMOVE or
      // UPDATE
      if (nodeType == ExecutionNode::REMOVE ||
          nodeType == ExecutionNode::UPDATE) {
        if (!defaultSharding) {
          // We have to use a ScatterNode.
          node = node->getFirstDependency();
          continue;
        }
      }

      // For INSERT, REPLACE,
      if (isModificationNode(nodeType) ||
          (isGraphNode(nodeType) && isSmart && isDisjoint)) {
        node = insertDistributeGatherSnippet(*plan, node, snode);
        wasModified = true;
      } else {
        node = node->getFirstDependency();
      }
    }  // for node in subquery
  }  // for end subquery in plan
  opt->addPlan(std::move(plan), rule, wasModified);
}

void arangodb::aql::collectInClusterRule(Optimizer* opt,
                                         std::unique_ptr<ExecutionPlan> plan,
                                         OptimizerRule const& rule) {
  TRI_ASSERT(arangodb::ServerState::instance()->isCoordinator());
  bool wasModified = false;

  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::COLLECT, true);

  VarSet allUsed;
  VarSet used;

  for (auto& node : nodes) {
    allUsed.clear();
    used.clear();
    node->getVariablesUsedHere(used);

    // found a node we need to replace in the plan
    TRI_ASSERT(node->getDependencies().size() == 1);

    auto collectNode = ExecutionNode::castTo<CollectNode*>(node);
    // look for next remote node
    GatherNode* gatherNode = nullptr;
    auto current = node->getFirstDependency();

    while (current != nullptr) {
      if (current->getType() == EN::LIMIT) {
        break;
      }

      // check if any of the nodes we pass use a variable that will not be
      // available after we insert a new COLLECT on top of it (note: COLLECT
      // will eliminate all variables from the scope but its own)
      if (current->getType() != EN::GATHER) {
        // Gather nodes are taken care of separately below
        current->getVariablesUsedHere(allUsed);
      }

      bool eligible = true;
      for (auto const& it : current->getVariablesSetHere()) {
        if (used.contains(it)) {
          eligible = false;
          break;
        }
      }

      if (!eligible) {
        break;
      }

      if (current->getType() == ExecutionNode::GATHER) {
        gatherNode = ExecutionNode::castTo<GatherNode*>(current);
      } else if (current->getType() == ExecutionNode::REMOTE) {
        auto previous = current->getFirstDependency();
        // now we are on a DB server

        {
          // check if we will deal with more than one shard
          // if the remote one has one shard, the optimization will actually
          // be a pessimization and shouldn't be applied
          bool hasFoundMultipleShards = false;
          auto p = previous;
          while (p != nullptr) {
            switch (p->getType()) {
              case ExecutionNode::REMOTE: {
                hasFoundMultipleShards = true;
                break;
              }
              case ExecutionNode::ENUMERATE_COLLECTION:
              case ExecutionNode::INDEX: {
                auto col = utils::getCollection(p);
                if (col->numberOfShards() > 1 ||
                    (col->type() == TRI_COL_TYPE_EDGE && col->isSmart())) {
                  hasFoundMultipleShards = true;
                }
                break;
              }
              case ExecutionNode::TRAVERSAL: {
                hasFoundMultipleShards = true;
                break;
              }
              case ExecutionNode::ENUMERATE_IRESEARCH_VIEW: {
                auto& viewNode =
                    *ExecutionNode::castTo<iresearch::IResearchViewNode*>(p);
                auto collections = viewNode.collections();
                auto const collCount = collections.size();
                TRI_ASSERT(collCount > 0);
                hasFoundMultipleShards = collCount > 0;
                if (collCount == 1) {
                  hasFoundMultipleShards =
                      collections.front().first.get().numberOfShards() > 1;
                }
                break;
              }
              default:
                break;
            }

            if (hasFoundMultipleShards) {
              break;
            }
            p = p->getFirstDependency();
          }
          if (!hasFoundMultipleShards) {
            // only a single shard will be contacted - abort the optimization
            // attempt to not make it a pessimization
            break;
          }
        }

        // we may have moved another CollectNode here already. if so, we need
        // to move the new CollectNode to the front of multiple CollectNodes
        ExecutionNode* target = current;
        while (previous != nullptr &&
               previous->getType() == ExecutionNode::COLLECT) {
          target = previous;
          previous = previous->getFirstDependency();
        }

        TRI_ASSERT(eligible);

        if (previous != nullptr) {
          for (auto const& otherVariable : allUsed) {
            auto const setHere = collectNode->getVariablesSetHere();
            if (std::find(setHere.begin(), setHere.end(), otherVariable) ==
                setHere.end()) {
              eligible = false;
              break;
            }
          }

          if (!eligible) {
            break;
          }

          bool removeGatherNodeSort = false;

          if (collectNode->aggregationMethod() ==
              CollectOptions::CollectMethod::kCount) {
            TRI_ASSERT(collectNode->aggregateVariables().size() == 1);
            TRI_ASSERT(collectNode->hasOutVariable() == false);
            // clone a COLLECT AGGREGATE var=LENGTH(_) operation from the
            // coordinator to the DB server(s), and leave an aggregate COLLECT
            // node on the coordinator for total aggregation

            // add a new CollectNode on the DB server to do the actual counting
            auto outVariable =
                plan->getAst()->variables()->createTemporaryVariable();
            std::vector<AggregateVarInfo> aggregateVariables;
            aggregateVariables.emplace_back(AggregateVarInfo{
                outVariable, collectNode->aggregateVariables()[0].inVar,
                "LENGTH"});
            auto dbCollectNode = plan->createNode<CollectNode>(
                plan.get(), plan->nextId(), collectNode->getOptions(),
                collectNode->groupVariables(), aggregateVariables, nullptr,
                nullptr, std::vector<std::pair<Variable const*, std::string>>{},
                collectNode->variableMap());

            dbCollectNode->addDependency(previous);
            target->replaceDependency(previous, dbCollectNode);

            dbCollectNode->aggregationMethod(collectNode->aggregationMethod());

            // re-use the existing CollectNode on the coordinator to aggregate
            // the counts of the DB servers
            collectNode->aggregateVariables()[0].type = "SUM";
            collectNode->aggregateVariables()[0].inVar = outVariable;
            collectNode->aggregationMethod(
                CollectOptions::CollectMethod::kSorted);

            removeGatherNodeSort = true;
          } else if (collectNode->aggregationMethod() ==
                     CollectOptions::CollectMethod::kDistinct) {
            // clone a COLLECT DISTINCT operation from the coordinator to the
            // DB server(s), and leave an aggregate COLLECT node on the
            // coordinator for total aggregation

            // create a new result variable
            auto const& groupVars = collectNode->groupVariables();
            TRI_ASSERT(!groupVars.empty());
            auto out = plan->getAst()->variables()->createTemporaryVariable();

            std::vector<GroupVarInfo> const groupVariables{
                GroupVarInfo{out, groupVars[0].inVar}};

            auto dbCollectNode = plan->createNode<CollectNode>(
                plan.get(), plan->nextId(), collectNode->getOptions(),
                groupVariables, collectNode->aggregateVariables(), nullptr,
                nullptr, std::vector<std::pair<Variable const*, std::string>>{},
                collectNode->variableMap());

            dbCollectNode->addDependency(previous);
            target->replaceDependency(previous, dbCollectNode);

            dbCollectNode->aggregationMethod(collectNode->aggregationMethod());

            // will set the input of the coordinator's collect node to the new
            // variable produced on the DB servers
            auto copy = collectNode->groupVariables();
            TRI_ASSERT(!copy.empty());
            std::unordered_map<Variable const*, Variable const*> replacements;
            replacements.try_emplace(copy[0].inVar, out);
            copy[0].inVar = out;
            collectNode->groupVariables(copy);

            replaceGatherNodeVariables(plan.get(), gatherNode, replacements);
          } else if (!collectNode->hasOutVariable() ||
                     collectNode->getOptions()
                         .aggregateIntoExpressionOnDBServers) {
            // clone a COLLECT v1 = expr, v2 = expr ... operation from the
            // coordinator to the DB server(s), and leave an aggregate COLLECT
            // node on the coordinator for total aggregation

            std::vector<AggregateVarInfo> dbServerAggVars;
            for (auto const& it : collectNode->aggregateVariables()) {
              std::string_view func = Aggregator::pushToDBServerAs(it.type);
              if (func.empty()) {
                eligible = false;
                break;
              }
              // eligible!
              auto outVariable =
                  plan->getAst()->variables()->createTemporaryVariable();
              dbServerAggVars.emplace_back(
                  AggregateVarInfo{outVariable, it.inVar, std::string(func)});
            }

            if (!eligible) {
              break;
            }

            // create new group variables
            auto const& groupVars = collectNode->groupVariables();
            std::vector<GroupVarInfo> outVars;
            outVars.reserve(groupVars.size());
            std::unordered_map<Variable const*, Variable const*> replacements;

            for (auto const& it : groupVars) {
              // create new out variables
              auto out = plan->getAst()->variables()->createTemporaryVariable();
              replacements.try_emplace(it.inVar, out);
              outVars.emplace_back(GroupVarInfo{out, it.inVar});
            }

            Variable const* expressionVariable = nullptr;
            Variable const* outVariable = nullptr;
            std::vector<std::pair<Variable const*, std::string>> keepVariables;

            bool const aggregateOutVariablesOnDBServers =
                collectNode->getOptions().aggregateIntoExpressionOnDBServers &&
                collectNode->hasOutVariable();

            if (aggregateOutVariablesOnDBServers) {
              outVariable =
                  plan->getAst()->variables()->createTemporaryVariable();
              expressionVariable = collectNode->expressionVariable();
              keepVariables = collectNode->keepVariables();
            }

            auto dbCollectNode = plan->createNode<CollectNode>(
                plan.get(), plan->nextId(), collectNode->getOptions(), outVars,
                dbServerAggVars, expressionVariable, outVariable,
                std::move(keepVariables), collectNode->variableMap());

            dbCollectNode->addDependency(previous);
            target->replaceDependency(previous, dbCollectNode);

            dbCollectNode->aggregationMethod(collectNode->aggregationMethod());

            std::vector<GroupVarInfo> copy;
            size_t i = 0;
            for (GroupVarInfo const& it : collectNode->groupVariables()) {
              // replace input variables
              copy.emplace_back(GroupVarInfo{/*outVar*/ it.outVar,
                                             /*inVar*/ outVars[i].outVar});
              ++i;
            }
            collectNode->groupVariables(copy);

            size_t j = 0;
            for (AggregateVarInfo& it : collectNode->aggregateVariables()) {
              it.inVar = dbServerAggVars[j].outVar;
              it.type = Aggregator::runOnCoordinatorAs(it.type);
              ++j;
            }

            if (aggregateOutVariablesOnDBServers) {
              TRI_ASSERT(outVariable != nullptr);
              collectNode->setMergeListsAggregation(outVariable);
            }

            removeGatherNodeSort = (dbCollectNode->aggregationMethod() !=
                                    CollectOptions::CollectMethod::kSorted);

            // in case we need to keep the sortedness of the GatherNode,
            // we may need to replace some variable references in it due
            // to the changes we made to the COLLECT node
            if (gatherNode != nullptr && !removeGatherNodeSort &&
                !replacements.empty() && !gatherNode->elements().empty()) {
              replaceGatherNodeVariables(plan.get(), gatherNode, replacements);
            }
          } else {
            // all other cases cannot be optimized
            break;
          }

          if (gatherNode != nullptr && removeGatherNodeSort) {
            // remove sort(s) from GatherNode if we can
            gatherNode->elements().clear();
          }

          wasModified = true;
        }
        break;
      }

      current = current->getFirstDependency();
    }
  }

  opt->addPlan(std::move(plan), rule, wasModified);
}

/// @brief move filters up into the cluster distribution part of the plan
/// this rule modifies the plan in place
/// filters are moved as far up in the plan as possible to make result sets
/// as small as possible as early as possible
void arangodb::aql::distributeFilterCalcToClusterRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const& rule) {
  bool modified = false;

  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::GATHER, true);

  VarSet varsSetHere;

  for (auto& n : nodes) {
    auto const& remoteNodeList = n->getDependencies();
    TRI_ASSERT(remoteNodeList.size() > 0);
    auto rn = remoteNodeList[0];

    if (!n->hasParent()) {
      continue;
    }

    bool allowOnlyFilterAndCalculation = false;

    varsSetHere.clear();
    auto parents = n->getParents();
    TRI_ASSERT(!parents.empty());

    while (true) {
      TRI_ASSERT(!parents.empty());
      bool stopSearching = false;
      auto inspectNode = parents[0];
      TRI_ASSERT(inspectNode != nullptr);

      auto type = inspectNode->getType();
      if (allowOnlyFilterAndCalculation && type != EN::FILTER &&
          type != EN::CALCULATION) {
        stopSearching = true;
        break;
      }

      switch (type) {
        case EN::ENUMERATE_LIST:
        case EN::SINGLETON:
        case EN::INSERT:
        case EN::REMOVE:
        case EN::REPLACE:
        case EN::UPDATE:
        case EN::UPSERT:
        case EN::SORT: {
          for (auto& v : inspectNode->getVariablesSetHere()) {
            varsSetHere.emplace(v);
          }
          parents = inspectNode->getParents();
          if (type == EN::SORT) {
            allowOnlyFilterAndCalculation = true;
          }
          continue;
        }

        case EN::COLLECT:
        case EN::RETURN:
        case EN::NORESULTS:
        case EN::SCATTER:
        case EN::DISTRIBUTE:
        case EN::GATHER:
        case EN::REMOTE:
        case EN::LIMIT:
        case EN::INDEX:
        case EN::ENUMERATE_COLLECTION:
        case EN::ENUMERATE_NEAR_VECTORS:
        case EN::TRAVERSAL:
        case EN::ENUMERATE_PATHS:
        case EN::SHORTEST_PATH:
        case EN::SUBQUERY:
        case EN::ENUMERATE_IRESEARCH_VIEW:
        case EN::WINDOW:
        case EN::MATERIALIZE:
          // do break
          stopSearching = true;
          break;

        case EN::OFFSET_INFO_MATERIALIZE:
        case EN::CALCULATION:
        case EN::FILTER: {
          if (inspectNode->getType() == EN::CALCULATION) {
            // check if the expression can be executed on a DB server safely
            TRI_vocbase_t& vocbase = plan->getAst()->query().vocbase();
            if (!ExecutionNode::castTo<CalculationNode const*>(inspectNode)
                     ->expression()
                     ->canRunOnDBServer(vocbase.isOneShard())) {
              stopSearching = true;
              break;
            }
            // intentionally falls through
          }
          // no special handling for filters here

          TRI_ASSERT(inspectNode->getType() == EN::SUBQUERY ||
                     inspectNode->getType() == EN::CALCULATION ||
                     inspectNode->getType() == EN::FILTER);

          VarSet used;
          inspectNode->getVariablesUsedHere(used);
          for (auto& v : used) {
            if (varsSetHere.find(v) != varsSetHere.end()) {
              // do not move over the definition of variables that we need
              stopSearching = true;
              break;
            }
          }

          if (!stopSearching) {
            // remember our cursor...
            parents = inspectNode->getParents();
            // then unlink the filter/calculator from the plan
            plan->unlinkNode(inspectNode);
            // and re-insert into plan in front of the remoteNode
            plan->insertDependency(rn, inspectNode);

            modified = true;
            // ready to rumble!
          }
          break;
        }

        default: {
          // should not reach this point
          TRI_ASSERT(false)
              << "Unhandled node type " << inspectNode->getTypeString();
        }
      }

      if (stopSearching) {
        break;
      }
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}

/// @brief move sorts up into the cluster distribution part of the plan
/// this rule modifies the plan in place
/// sorts are moved as far up in the plan as possible to make result sets
/// as small as possible as early as possible
///
/// filters are not pushed beyond limits
void arangodb::aql::distributeSortToClusterRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  VarSet usedBySort;
  plan->findNodesOfType(nodes, EN::GATHER, true);

  bool modified = false;

  for (auto& n : nodes) {
    auto const remoteNodeList = n->getDependencies();
    TRI_ASSERT(remoteNodeList.size() > 0);
    auto rn = remoteNodeList[0];

    if (!n->hasParent()) {
      continue;
    }

    auto gatherNode = ExecutionNode::castTo<GatherNode*>(n);

    auto parents = n->getParents();

    while (true) {
      TRI_ASSERT(!parents.empty());
      bool stopSearching = false;
      auto inspectNode = parents[0];
      TRI_ASSERT(inspectNode != nullptr);

      switch (inspectNode->getType()) {
        case EN::SINGLETON:
        case EN::ENUMERATE_COLLECTION:
        case EN::ENUMERATE_LIST:
        case EN::ENUMERATE_NEAR_VECTORS:
        case EN::COLLECT:
        case EN::INDEX_COLLECT:
        case EN::INSERT:
        case EN::REMOVE:
        case EN::REPLACE:
        case EN::UPDATE:
        case EN::UPSERT:
        case EN::CALCULATION:
        case EN::FILTER:
        case EN::SUBQUERY:
        case EN::RETURN:
        case EN::NORESULTS:
        case EN::SCATTER:
        case EN::DISTRIBUTE:
        case EN::GATHER:
        case EN::REMOTE:
        case EN::LIMIT:
        case EN::INDEX:
        case EN::JOIN:
        case EN::TRAVERSAL:
        case EN::ENUMERATE_PATHS:
        case EN::SHORTEST_PATH:
        case EN::REMOTE_SINGLE:
        case EN::REMOTE_MULTIPLE:
        case EN::ENUMERATE_IRESEARCH_VIEW:
        case EN::WINDOW:
        case EN::MATERIALIZE:
        case EN::OFFSET_INFO_MATERIALIZE:

          // For all these, we do not want to pull a SortNode further down
          // out to the DBservers, note that potential FilterNodes and
          // CalculationNodes that can be moved to the DBservers have
          // already been moved over by the distribute-filtercalc-to-cluster
          // rule which is done first.
          stopSearching = true;
          break;

        case EN::SORT: {
          auto thisSortNode = ExecutionNode::castTo<SortNode*>(inspectNode);
          usedBySort.clear();
          thisSortNode->getVariablesUsedHere(usedBySort);
          // remember our cursor...
          parents = inspectNode->getParents();
          // then unlink the filter/calculator from the plan
          plan->unlinkNode(inspectNode);
          // and re-insert into plan in front of the remoteNode
          if (thisSortNode->reinsertInCluster()) {
            // let's look for the best place for that SORT.
            // We could skip over several calculations if
            // they are not needed for our sort. So we could calculate
            // more lazily and even make late materialization possible
            ExecutionNode* insertPoint = rn;
            auto current = insertPoint->getFirstDependency();
            while (current != nullptr &&
                   current->getType() == EN::CALCULATION) {
              auto nn = ExecutionNode::castTo<CalculationNode*>(current);
              if (!nn->expression()->isDeterministic()) {
                // let's not touch non-deterministic calculation
                // as results may depend on calls count and sort could change
                // this
                break;
              }
              auto variable = nn->outVariable();
              if (usedBySort.find(variable) == usedBySort.end()) {
                insertPoint = current;
              } else {
                break;  // first node used by sort. We should stop here.
              }
              current = current->getFirstDependency();
            }
            plan->insertDependency(insertPoint, inspectNode);
          }

          gatherNode->elements(thisSortNode->elements());
          modified = true;
          // ready to rumble!
          break;
        }
        // we should not encounter this kind of nodes for now
        case EN::SUBQUERY_START:
        case EN::SUBQUERY_END:
        case EN::DISTRIBUTE_CONSUMER:
        case EN::ASYNC:
        case EN::MUTEX:
        case EN::MAX_NODE_TYPE_VALUE: {
          // should not reach this point
          stopSearching = true;
          TRI_ASSERT(false);
          break;
        }
      }

      if (stopSearching) {
        break;
      }
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}

/// @brief try to get rid of a RemoteNode->ScatterNode combination which has
/// only a SingletonNode and possibly some CalculationNodes as dependencies
void arangodb::aql::removeUnnecessaryRemoteScatterRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::REMOTE,
                        false /* do not go into Subqueries */);

  ::arangodb::containers::HashSet<ExecutionNode*> toUnlink;

  for (auto& n : nodes) {
    // check if the remote node is preceded by a scatter node and any number of
    // calculation and singleton nodes. if yes, remove remote and scatter
    if (!n->hasDependency()) {
      continue;
    }

    auto const dep = n->getFirstDependency();
    if (dep->getType() != EN::SCATTER) {
      continue;
    }

    bool canOptimize = true;
    auto node = dep;
    while (node != nullptr) {
      auto const& d = node->getDependencies();

      if (d.size() != 1) {
        break;
      }

      node = d[0];
      if (!plan->shouldExcludeFromScatterGather(node)) {
        if (node->getType() != EN::SINGLETON &&
            node->getType() != EN::CALCULATION &&
            node->getType() != EN::FILTER) {
          // found some other node type...
          // this disqualifies the optimization
          canOptimize = false;
          break;
        }

        if (node->getType() == EN::CALCULATION) {
          auto calc = ExecutionNode::castTo<CalculationNode const*>(node);
          // check if the expression can be executed on a DB server safely
          TRI_vocbase_t& vocbase = plan->getAst()->query().vocbase();
          if (!calc->expression()->canRunOnDBServer(vocbase.isOneShard())) {
            canOptimize = false;
            break;
          }
        }
      }
    }

    if (canOptimize) {
      toUnlink.emplace(n);
      toUnlink.emplace(dep);
    }
  }

  if (!toUnlink.empty()) {
    plan->unlinkNodes(toUnlink);
  }

  opt->addPlan(std::move(plan), rule, !toUnlink.empty());
}

/// @brief try to restrict fragments to a single shard if possible
void arangodb::aql::restrictToSingleShardRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
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

    auto s1 = utils::getCollection(current)->shardIds();
    auto s2 = utils::getCollection(setter)->shardIds();

    if (s1->size() != s2->size()) {
      // different number of shard ids... should not happen if we have a
      // prototype
      return;
    }

    // find matching shard key
    for (size_t i = 0; i < s1->size(); ++i) {
      if ((*s1)[i] == shardId) {
        ::restrictToShard(setter, (*s2)[i]);
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
        auto shardId = ::getSingleShardId(plan.get(), current, collection);

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
            if (Index::TRI_IDX_TYPE_INVERTED_INDEX == index->type()) {
              disable = true;
              break;
            }
          }
        }

        if (!disable) {
          auto collection = utils::getCollection(current);
          auto collectionVariable = ::getOutVariable(current);
          auto shardId = finder.getShard(collectionVariable);

          if (finder.isSafeForOptimization(collectionVariable) &&
              shardId.isValid()) {
            wasModified = true;
            ::restrictToShard(current, shardId);
            forwardRestrictionToPrototype(current, shardId);
          } else if (finder.isSafeForOptimization(collection)) {
            auto& shards = modificationRestrictions[collection];
            if (shards.size() == 1) {
              wasModified = true;
              shardId = *shards.begin();
              ::restrictToShard(current, shardId);
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

/// WalkerWorker for undistributeRemoveAfterEnumColl
class RemoveToEnumCollFinder final
    : public WalkerWorker<ExecutionNode, WalkerUniqueness::NonUnique> {
  ExecutionPlan* _plan;
  ::arangodb::containers::HashSet<ExecutionNode*>& _toUnlink;
  bool _foundModification;
  bool _foundScatter;
  bool _foundGather;
  ExecutionNode* _enumColl;
  ExecutionNode* _setter;
  Variable const* _variable;

 public:
  RemoveToEnumCollFinder(
      ExecutionPlan* plan,
      ::arangodb::containers::HashSet<ExecutionNode*>& toUnlink)
      : _plan(plan),
        _toUnlink(toUnlink),
        _foundModification(false),
        _foundScatter(false),
        _foundGather(false),
        _enumColl(nullptr),
        _setter(nullptr),
        _variable(nullptr) {}

  bool before(ExecutionNode* en) override final {
    switch (en->getType()) {
      case EN::UPDATE:
      case EN::REPLACE:
      case EN::REMOVE: {
        if (_foundModification) {
          break;
        }

        // find the variable we are removing . . .
        auto rn = ExecutionNode::castTo<ModificationNode*>(en);
        Variable const* toRemove = nullptr;

        if (en->getType() == EN::REPLACE) {
          toRemove =
              ExecutionNode::castTo<ReplaceNode const*>(en)->inKeyVariable();
        } else if (en->getType() == EN::UPDATE) {
          // first try if we have the pattern UPDATE <key> WITH <doc> IN
          // collection. if so, then toRemove will contain <key>.
          toRemove =
              ExecutionNode::castTo<UpdateNode const*>(en)->inKeyVariable();

          if (toRemove == nullptr) {
            // if we don't have that pattern, we can if instead have
            // UPDATE <doc> IN collection.
            // in this case toRemove will contain <doc>.
            toRemove =
                ExecutionNode::castTo<UpdateNode const*>(en)->inDocVariable();
          }
        } else if (en->getType() == EN::REMOVE) {
          toRemove = ExecutionNode::castTo<RemoveNode const*>(en)->inVariable();
        } else {
          TRI_ASSERT(false);
        }

        if (toRemove == nullptr) {
          // abort
          break;
        }

        _setter = _plan->getVarSetBy(toRemove->id);
        TRI_ASSERT(_setter != nullptr);
        auto enumColl = _setter;

        if (_setter->getType() == EN::CALCULATION) {
          // this should be an attribute access for _key
          auto cn = ExecutionNode::castTo<CalculationNode*>(_setter);

          auto expr = cn->expression();
          if (expr->isAttributeAccess()) {
            // check the variable is the same as the remove variable
            if (cn->outVariable() != toRemove) {
              break;  // abort . . .
            }
            // check that the modification node's collection is sharded over
            // _key
            std::vector<std::string> shardKeys =
                rn->collection()->shardKeys(false);
            if (shardKeys.size() != 1 ||
                shardKeys[0] != arangodb::StaticStrings::KeyString) {
              break;  // abort . . .
            }

            // set the varsToRemove to the variable in the expression of this
            // node and also define enumColl
            VarSet varsToRemove;
            cn->getVariablesUsedHere(varsToRemove);
            TRI_ASSERT(varsToRemove.size() == 1);
            toRemove = *(varsToRemove.begin());
            enumColl = _plan->getVarSetBy(toRemove->id);
            TRI_ASSERT(_setter != nullptr);
          } else if (expr->node() && expr->node()->isObject()) {
            auto n = expr->node();

            if (n == nullptr) {
              break;
            }

            // note for which shard keys we need to look for
            auto shardKeys = rn->collection()->shardKeys(false);
            std::unordered_set<std::string> toFind;
            for (auto const& it : shardKeys) {
              toFind.emplace(it);
            }
            // for UPDATE/REPLACE/REMOVE, we must also know the _key value,
            // otherwise they will not work.
            toFind.emplace(arangodb::StaticStrings::KeyString);

            // go through the input object attribute by attribute
            // and look for our shard keys
            Variable const* lastVariable = nullptr;
            bool doOptimize = true;

            for (size_t i = 0; i < n->numMembers(); ++i) {
              auto sub = n->getMemberUnchecked(i);

              if (sub->type != NODE_TYPE_OBJECT_ELEMENT) {
                continue;
              }

              ast::ObjectElementNode objElem(sub);
              std::string attributeName = sub->getString();
              auto it = toFind.find(attributeName);

              if (it != toFind.end()) {
                // we found one of the shard keys!
                // remove the attribute from our to-do list
                auto value = objElem.getValue();

                // check if we have something like: { key: source.key }
                if (value->type == NODE_TYPE_ATTRIBUTE_ACCESS &&
                    value->getStringView() == attributeName) {
                  // check if all values for the shard keys are referring to
                  // the same FOR loop variable
                  ast::AttributeAccessNode attrAccess(value);
                  auto var = attrAccess.getObject();
                  if (var->type == NODE_TYPE_REFERENCE) {
                    ast::ReferenceNode ref(var);
                    auto accessedVariable = ref.getVariable();

                    if (lastVariable == nullptr) {
                      lastVariable = accessedVariable;
                    } else if (lastVariable != accessedVariable) {
                      doOptimize = false;
                      break;
                    }

                    toFind.erase(it);
                  }
                }
              }
            }

            if (!toFind.empty() || !doOptimize || lastVariable == nullptr) {
              // not all shard keys covered, or different source variables in
              // use
              break;
            }

            TRI_ASSERT(lastVariable != nullptr);
            enumColl = _plan->getVarSetBy(lastVariable->id);
          } else {
            // cannot optimize this type of input
            break;
          }
        }

        if (enumColl->getType() != EN::ENUMERATE_COLLECTION &&
            enumColl->getType() != EN::INDEX) {
          break;  // abort . . .
        }

        auto const& projections =
            dynamic_cast<DocumentProducingNode const*>(enumColl)->projections();
        if (projections.isSingle(arangodb::StaticStrings::KeyString)) {
          // cannot handle projections
          break;
        }

        _enumColl = enumColl;

        if (utils::getCollection(_enumColl) != rn->collection()) {
          break;  // abort . . .
        }

        _variable = toRemove;  // the variable we'll remove
        _foundModification = true;
        return false;  // continue . . .
      }
      case EN::REMOTE: {
        _toUnlink.emplace(en);
        return false;  // continue . . .
      }
      case EN::DISTRIBUTE:
      case EN::SCATTER: {
        if (_foundScatter) {  // met more than one scatter node
          break;              // abort . . .
        }
        _foundScatter = true;
        _toUnlink.emplace(en);
        return false;  // continue . . .
      }
      case EN::GATHER: {
        if (_foundGather) {  // met more than one gather node
          break;             // abort . . .
        }
        _foundGather = true;
        _toUnlink.emplace(en);
        return false;  // continue . . .
      }
      case EN::FILTER: {
        return false;  // continue . . .
      }
      case EN::CALCULATION: {
        TRI_vocbase_t& vocbase = _plan->getAst()->query().vocbase();
        auto calculationNode = ExecutionNode::castTo<CalculationNode*>(en);
        auto expr = calculationNode->expression();

        // If we find an expression that is not allowed to run on a DBServer,
        // we cannot undistribute (as then the expression *would* run on a
        // dbserver)
        if (!expr->canRunOnDBServer(vocbase.isOneShard())) {
          break;
        }
        return false;  // continue . . .
      }
      case EN::WINDOW: {
        return false;  // continue . . .
      }
      case EN::ENUMERATE_COLLECTION:
      case EN::INDEX: {
        // check that we are enumerating the variable we are to remove
        // and that we have already seen a remove node
        TRI_ASSERT(_enumColl != nullptr);
        if (en->id() != _enumColl->id()) {
          break;
        }
        return true;  // reached the end!
      }
      case EN::SINGLETON:
      case EN::ENUMERATE_LIST:
      case EN::ENUMERATE_IRESEARCH_VIEW:
      case EN::SUBQUERY:
      case EN::COLLECT:
      case EN::INSERT:
      case EN::UPSERT:
      case EN::RETURN:
      case EN::NORESULTS:
      case EN::LIMIT:
      case EN::SORT:
      case EN::TRAVERSAL:
      case EN::ENUMERATE_PATHS:
      case EN::SHORTEST_PATH: {
        // if we meet any of the above, then we abort . . .
        break;
      }

      default: {
        // should not reach this point
        TRI_ASSERT(false);
      }
    }

    _toUnlink.clear();
    return true;
  }
};

/// @brief recognizes that a RemoveNode can be moved to the shards.
void arangodb::aql::undistributeRemoveAfterEnumCollRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, ::undistributeNodeTypes, true);

  ::arangodb::containers::HashSet<ExecutionNode*> toUnlink;

  for (auto& n : nodes) {
    RemoveToEnumCollFinder finder(plan.get(), toUnlink);
    n->walk(finder);
  }

  bool modified = false;
  if (!toUnlink.empty()) {
    plan->unlinkNodes(toUnlink);
    modified = true;
  }

  opt->addPlan(std::move(plan), rule, modified);
}
////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "OptimizerRules.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/ClusterNodes.h"
#include "Aql/CollectNode.h"
#include "Aql/CollectOptions.h"
#include "Aql/Collection.h"
#include "Aql/ConditionFinder.h"
#include "Aql/DocumentProducingNode.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Function.h"
#include "Aql/IndexNode.h"
#include "Aql/ModificationNodes.h"
#include "Aql/Optimizer.h"
#include "Aql/Query.h"
#include "Aql/ShortestPathNode.h"
#include "Aql/SortCondition.h"
#include "Aql/SortNode.h"
#include "Aql/TraversalConditionFinder.h"
#include "Aql/TraversalNode.h"
#include "Aql/Variable.h"
#include "Aql/types.h"
#include "Basics/AttributeNameParser.h"
#include "Basics/NumberUtils.h"
#include "Basics/SmallVector.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringBuffer.h"
#include "Cluster/ClusterInfo.h"
#include "Geo/GeoParams.h"
#include "GeoIndex/Index.h"
#include "Graph/TraverserOptions.h"
#include "Indexes/Index.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/Methods.h"
#include "VocBase/Methods/Collections.h"

#ifdef USE_IRESEARCH
#include "IResearch/IResearchViewNode.h"
#endif

#include <boost/optional.hpp>
#include <tuple>

namespace {
         
bool accessesCollectionVariable(arangodb::aql::ExecutionPlan const* plan,
                                arangodb::aql::CalculationNode const* node, 
                                std::unordered_set<arangodb::aql::Variable const*>& vars) {
  using EN = arangodb::aql::ExecutionNode;

  vars.clear();
  arangodb::aql::Ast::getReferencedVariables(node->expression()->node(), vars);
  for (auto const& it : vars) {
    auto setter = plan->getVarSetBy(it->id);
    if (setter == nullptr) {
      continue;
    }
    if (setter->getType() == EN::INDEX ||
        setter->getType() == EN::ENUMERATE_COLLECTION ||
#ifdef USE_IRESEARCH
        setter->getType() == EN::ENUMERATE_IRESEARCH_VIEW ||
#endif
        setter->getType() == EN::SUBQUERY ||
        setter->getType() == EN::TRAVERSAL ||
        setter->getType() == EN::SHORTEST_PATH) {
      return true;
    }
  }

  return false;
}

std::string getSingleShardId(
    arangodb::aql::ExecutionPlan const* plan,
    arangodb::aql::ExecutionNode const* node,
    arangodb::aql::Collection const* collection,
    arangodb::aql::Variable const* collectionVariable = nullptr);

arangodb::aql::Collection const* getCollection(
    arangodb::aql::ExecutionNode const* node) {
  using EN = arangodb::aql::ExecutionNode;
  using arangodb::aql::ExecutionNode;

  switch (node->getType()) {
    case EN::ENUMERATE_COLLECTION:
      return ExecutionNode::castTo<
                 arangodb::aql::EnumerateCollectionNode const*>(node)
          ->collection();
    case EN::INDEX:
      return ExecutionNode::castTo<arangodb::aql::IndexNode const*>(node)
          ->collection();
    case EN::TRAVERSAL:
    case EN::SHORTEST_PATH:
      return ExecutionNode::castTo<arangodb::aql::GraphNode const*>(node)
          ->collection();
    default:
      // note: modification nodes are not covered here yet
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "node type does not have a collection");
  }
}

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

void restrictToShard(arangodb::aql::ExecutionNode* node, std::string shardId) {
  auto* n = dynamic_cast<arangodb::aql::CollectionAccessingNode*>(node);
  if (n != nullptr) {
    return n->restrictToShard(shardId);
  }
  // note: modification nodes are not covered here yet
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_INTERNAL, "node type cannot be restricted to a single shard");
}

struct PairHash {
  template <class T1, class T2>
  size_t operator()(std::pair<T1, T2> const& pair) const noexcept {
    size_t first = std::hash<T1>()(pair.first);
    size_t second = std::hash<T2>()(pair.second);

    return first ^ second;
  }
};

/// WalkerWorker to track collection variable dependencies
class CollectionVariableTracker final
    : public arangodb::aql::WalkerWorker<arangodb::aql::ExecutionNode> {
  using DependencyPair = std::pair<arangodb::aql::Variable const*,
                                   arangodb::aql::Collection const*>;
  using DependencySet = std::unordered_set<DependencyPair, ::PairHash>;
  using VariableSet = std::unordered_set<arangodb::aql::Variable const*>;
  bool _stop;
  std::unordered_map<arangodb::aql::Variable const*, DependencySet>
      _dependencies;
  std::unordered_map<arangodb::aql::Collection const*, VariableSet>
      _collectionVariables;

 private:
  template <class NodeType>
  void processSetter(arangodb::aql::ExecutionNode const* en,
                     arangodb::aql::Variable const* outVariable) {
    auto node = arangodb::aql::ExecutionNode::castTo<NodeType const*>(en);
    try {
      auto inputVariables = node->getVariablesUsedHere();
      for (auto var : inputVariables) {
        for (auto dep : _dependencies[var]) {
          _dependencies[outVariable].emplace(dep);
        }
      }
    } catch (...) {
      _stop = true;  // won't be able to recover correctly
    }
  }

  template <class NodeType>
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

  VariableSet const& getCollectionVariables(
      arangodb::aql::Collection const* collection) {
    return _collectionVariables[collection];
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
        auto collection = ::getCollection(en);
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
    : public arangodb::aql::WalkerWorker<arangodb::aql::ExecutionNode> {
  arangodb::aql::ExecutionPlan* _plan;
  CollectionVariableTracker& _tracker;
  std::unordered_map<arangodb::aql::Variable const*,
                     std::unordered_set<std::string>>
      _shardsUsed;
  std::unordered_map<arangodb::aql::Variable const*,
                     std::unordered_set<std::string>>
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

  std::string getShard(arangodb::aql::Variable const* variable) const {
    auto const& it = _shardsCleared.find(variable);
    if (it == _shardsCleared.end()) {
      return "";
    }

    auto set = it->second;
    if (set.size() != 1 || *set.begin() == "all") {
      return "";
    }

    return *set.begin();
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

    if ((*it).second.size() != 1) {
      // more than one shard
      return false;
    }

    // check for "all" marker
    auto it2 = (*it).second.find("all");
    if (it2 != (*it).second.end()) {
      // "all" included
      return false;
    }

    // all good -> safe to optimize
    return true;
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
      case EN::SHORTEST_PATH: {
        _stop = true;
        return true;  // abort enumerating, we are done already!
      }

      case EN::FILTER: {
        auto node = ExecutionNode::castTo<arangodb::aql::FilterNode const*>(en);
        auto v = node->getVariablesUsedHere();
        TRI_ASSERT(v.size() == 1);
        arangodb::aql::Variable const* inputVariable = v[0];
        handleInputVariable(en, inputVariable);
        break;
      }

      case EN::ENUMERATE_COLLECTION: {
        handleSourceNode(en);
        break;
      }

      case EN::INDEX: {
        handleIndexNode(en);
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
        std::string shardId = ::getSingleShardId(_plan, en, node->collection());
        if (shardId.empty()) {
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
  void handleInputVariable(arangodb::aql::ExecutionNode const* en,
                           arangodb::aql::Variable const* inputVariable) {
    auto dependencies = _tracker.getDependencies(inputVariable);
    for (auto dep : dependencies) {
      auto variable = dep.first;
      auto collection = dep.second;
      auto shardId = ::getSingleShardId(_plan, en, collection, variable);
      if (shardId.empty()) {
        if (_shardsUsed[variable].empty()) {
          _shardsUsed[variable].emplace("all");
        }
      } else {
        if (1 == _shardsUsed[variable].size() &&
            "all" == *_shardsUsed[variable].begin()) {
          _shardsUsed[variable].clear();
        }
        _shardsUsed[variable].emplace(shardId);
      }
    }
  }

  void handleIndexNode(arangodb::aql::ExecutionNode const* en) {
    auto collection = ::getCollection(en);
    auto variable = ::getOutVariable(en);
    auto shardId = ::getSingleShardId(_plan, en, collection, variable);
    if (shardId.empty()) {
      if (_shardsUsed[variable].empty()) {
        _shardsUsed[variable].emplace("all");
      }
    } else {
      if (1 == _shardsUsed[variable].size() &&
          "all" == *_shardsUsed[variable].begin()) {
        _shardsUsed[variable].clear();
      }
      _shardsUsed[variable].emplace(shardId);
    }
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

  auto lhs = root->getMember(0);
  auto rhs = root->getMember(1);
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
      if (root->numMembers() != 1) {
        return;
      }
      root = root->getMember(0);
      if (root == nullptr ||
          root->type !=
              arangodb::aql::AstNodeType::NODE_TYPE_OPERATOR_NARY_AND) {
        return;
      }
    } // falls through
    case arangodb::aql::AstNodeType::NODE_TYPE_OPERATOR_BINARY_AND:
    case arangodb::aql::AstNodeType::NODE_TYPE_OPERATOR_NARY_AND: {
      for (size_t i = 0; i < root->numMembers(); ++i) {
        if (root->getMember(i) != nullptr &&
            root->getMember(i)->type ==
                arangodb::aql::AstNodeType::NODE_TYPE_OPERATOR_BINARY_EQ) {
          findShardKeyInComparison(root->getMember(i), inputVariable, toFind,
                                   builder);
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

// static node types used by some optimizer rules
// having them statically available avoids having to build the vectors over
// and over for each AQL query
std::vector<arangodb::aql::ExecutionNode::NodeType> const
    removeUnnecessaryCalculationsNodeTypes{
        arangodb::aql::ExecutionNode::CALCULATION,
        arangodb::aql::ExecutionNode::SUBQUERY};
std::vector<arangodb::aql::ExecutionNode::NodeType> const
    interchangeAdjacentEnumerationsNodeTypes{
        arangodb::aql::ExecutionNode::ENUMERATE_COLLECTION,
        arangodb::aql::ExecutionNode::ENUMERATE_LIST};
std::vector<arangodb::aql::ExecutionNode::NodeType> const
    scatterInClusterNodeTypes{
        arangodb::aql::ExecutionNode::ENUMERATE_COLLECTION,
        arangodb::aql::ExecutionNode::INDEX,
        arangodb::aql::ExecutionNode::INSERT,
        arangodb::aql::ExecutionNode::UPDATE,
        arangodb::aql::ExecutionNode::REPLACE,
        arangodb::aql::ExecutionNode::REMOVE,
        arangodb::aql::ExecutionNode::UPSERT};
std::vector<arangodb::aql::ExecutionNode::NodeType> const
    removeDataModificationOutVariablesNodeTypes{
        arangodb::aql::ExecutionNode::REMOVE,
        arangodb::aql::ExecutionNode::INSERT,
        arangodb::aql::ExecutionNode::UPDATE,
        arangodb::aql::ExecutionNode::REPLACE,
        arangodb::aql::ExecutionNode::UPSERT};
std::vector<arangodb::aql::ExecutionNode::NodeType> const
    patchUpdateStatementsNodeTypes{arangodb::aql::ExecutionNode::UPDATE,
                                   arangodb::aql::ExecutionNode::REPLACE};
std::vector<arangodb::aql::ExecutionNode::NodeType> const
    patchUpdateRemoveStatementsNodeTypes{arangodb::aql::ExecutionNode::UPDATE,
                                         arangodb::aql::ExecutionNode::REPLACE,
                                         arangodb::aql::ExecutionNode::REMOVE};

int indexOf(std::vector<std::string> const& haystack,
            std::string const& needle) {
  for (size_t i = 0; i < haystack.size(); ++i) {
    if (haystack[i] == needle) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

/// @brief find the single shard id for the node to restrict an operation to
/// this will check the conditions of an IndexNode or a data-modification node
/// (excluding UPSERT) and check if all shard keys are used in it. If all
/// shard keys are present and their values are fixed (constants), this
/// function will try to figure out the target shard. If the operation cannot
/// be restricted to a single shard, this function will return an empty string
std::string getSingleShardId(
    arangodb::aql::ExecutionPlan const* plan,
    arangodb::aql::ExecutionNode const* node,
    arangodb::aql::Collection const* collection,
    arangodb::aql::Variable const* collectionVariable) {
  using EN = arangodb::aql::ExecutionNode;
  using arangodb::aql::ExecutionNode;

  if (collection->isSmart() &&
      collection->getCollection()->type() == TRI_COL_TYPE_EDGE) {
    // no support for smart edge collections
    return std::string();
  }

  TRI_ASSERT(node->getType() == EN::INDEX || node->getType() == EN::FILTER ||
             node->getType() == EN::INSERT || node->getType() == EN::UPDATE ||
             node->getType() == EN::REPLACE || node->getType() == EN::REMOVE);

  arangodb::aql::Variable const* inputVariable = nullptr;
  if (node->getType() == EN::INDEX) {
    inputVariable = node->getVariablesSetHere()[0];
  } else {
    std::vector<arangodb::aql::Variable const*> v =
        node->getVariablesUsedHere();
    if (v.size() > 1) {
      // If there is a key variable:
      inputVariable = v[1];
    } else {
      inputVariable = v[0];
    }
  }

  // check if we can easily find out the setter of the input variable
  // (and if we can find it, check if the data is constant so we can look
  // up the shard key attribute values)
  auto setter = plan->getVarSetBy(inputVariable->id);

  if (setter == nullptr) {
    // oops!
    TRI_ASSERT(false);
    return std::string();
  }

  // note for which shard keys we need to look for
  auto shardKeys = collection->shardKeys();
  std::unordered_set<std::string> toFind;
  for (auto const& it : shardKeys) {
    if (it.find('.') != std::string::npos) {
      // shard key containing a "." (sub-attribute). this is not yet supported
      return std::string();
    }
    toFind.emplace(it);
  }

  VPackBuilder builder;
  builder.openObject();

  if (setter->getType() == EN::CALCULATION) {
    arangodb::aql::CalculationNode const* c =
        ExecutionNode::castTo<arangodb::aql::CalculationNode const*>(setter);
    auto ex = c->expression();

    if (ex == nullptr) {
      return std::string();
    }

    auto n = ex->node();
    if (n == nullptr) {
      return std::string();
    }

    if (n->isStringValue()) {
      if (!n->isConstant() || toFind.size() != 1 ||
          toFind.find(arangodb::StaticStrings::KeyString) == toFind.end()) {
        return std::string();
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

        if (sub->type != arangodb::aql::AstNodeType::NODE_TYPE_OBJECT_ELEMENT) {
          continue;
        }

        auto it = toFind.find(sub->getString());

        if (it != toFind.end()) {
          // we found one of the shard keys!
          auto v = sub->getMember(0);
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
      return std::string();
    }
    auto const* condition = c->condition();

    if (condition == nullptr) {
      return std::string();
    }

    arangodb::aql::AstNode const* root = condition->root();
    ::findShardKeysInExpression(root, inputVariable, toFind, builder);
  }

  builder.close();

  if (!toFind.empty()) {
    return std::string();
  }

  // all shard keys found!!

  // find the responsible shard for the data
  std::string shardId;

  int res = collection->getCollection()->getResponsibleShard(builder.slice(),
                                                             true, shardId);

  if (res != TRI_ERROR_NO_ERROR) {
    // some error occurred. better do not use the
    // single shard optimization here
    return std::string();
  }

  // we will only need a single shard!
  return shardId;
}

}  // namespace

using namespace arangodb;
using namespace arangodb::aql;
using EN = arangodb::aql::ExecutionNode;

namespace arangodb {
namespace aql {

// TODO cleanup this f-ing aql::Collection(s) mess
Collection* addCollectionToQuery(Query* query, std::string const& cname,
                                 bool assert) {

  aql::Collection* coll = nullptr;

  if (!cname.empty()) {
    coll = query->addCollection(cname, AccessMode::Type::READ);

    if (!ServerState::instance()->isCoordinator()) {
      TRI_ASSERT(coll != nullptr);
      auto cptr = query->trx()->vocbase().lookupCollection(cname);

      coll->setCollection(cptr.get());
      query->trx()->addCollectionAtRuntime(cname);
    }
  }

  if (assert) {
    TRI_ASSERT(coll != nullptr);
  }

  return coll;
}

}  // namespace aql
}  // namespace arangodb

/// @brief adds a SORT operation for IN right-hand side operands
void arangodb::aql::sortInValuesRule(Optimizer* opt,
                                     std::unique_ptr<ExecutionPlan> plan,
                                     OptimizerRule const* rule) {
  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, EN::FILTER, true);

  bool modified = false;

  for (auto const& n : nodes) {
    // filter nodes always have one input variable
    auto varsUsedHere = n->getVariablesUsedHere();
    TRI_ASSERT(varsUsedHere.size() == 1);

    // now check who introduced our variable
    auto variable = varsUsedHere[0];
    auto setter = plan->getVarSetBy(variable->id);

    if (setter == nullptr || setter->getType() != EN::CALCULATION) {
      // filter variable was not introduced by a calculation.
      continue;
    }

    // filter variable was introduced a CalculationNode. now check the
    // expression
    auto s = ExecutionNode::castTo<CalculationNode*>(setter);
    auto filterExpression = s->expression();
    auto* inNode = filterExpression->nodeForModification();

    TRI_ASSERT(inNode != nullptr);

    // check the filter condition
    if ((inNode->type != NODE_TYPE_OPERATOR_BINARY_IN &&
         inNode->type != NODE_TYPE_OPERATOR_BINARY_NIN) ||
        !inNode->isDeterministic()) {
      // we better not tamper with this filter
      continue;
    }

    auto rhs = inNode->getMember(1);

    if (rhs->type != NODE_TYPE_REFERENCE && rhs->type != NODE_TYPE_ARRAY) {
      continue;
    }

    auto loop = n->getLoop();

    if (loop == nullptr) {
      // FILTER is not used inside a loop. so it will be used at most once
      // not need to sort the IN values then
      continue;
    }

    if (rhs->type == NODE_TYPE_ARRAY) {
      if (rhs->numMembers() < AstNode::SortNumberThreshold || rhs->isSorted()) {
        // number of values is below threshold or array is already sorted
        continue;
      }

      auto ast = plan->getAst();
      auto args = ast->createNodeArray();
      args->addMember(rhs);
      auto sorted = ast->createNodeFunctionCall(
          TRI_CHAR_LENGTH_PAIR("SORTED_UNIQUE"), args);
      inNode->changeMember(1, sorted);
      modified = true;
      continue;
    }

    variable = static_cast<Variable const*>(rhs->getData());
    setter = plan->getVarSetBy(variable->id);

    if (setter == nullptr || (setter->getType() != EN::CALCULATION &&
                              setter->getType() != EN::SUBQUERY)) {
      // variable itself was not introduced by a calculation.
      continue;
    }

    if (loop == setter->getLoop()) {
      // the FILTER and its value calculation are contained in the same loop
      // this means the FILTER will be executed as many times as its value
      // calculation. sorting the IN values will not provide a benefit here
      continue;
    }

    auto ast = plan->getAst();
    AstNode const* originalArg = nullptr;

    if (setter->getType() == EN::CALCULATION) {
      AstNode const* originalNode =
          ExecutionNode::castTo<CalculationNode*>(setter)->expression()->node();
      TRI_ASSERT(originalNode != nullptr);

      AstNode const* testNode = originalNode;

      if (originalNode->type == NODE_TYPE_FCALL &&
          static_cast<Function const*>(originalNode->getData())->name ==
              "NOOPT") {
        // bypass NOOPT(...)
        TRI_ASSERT(originalNode->numMembers() == 1);
        auto args = originalNode->getMember(0);

        if (args->numMembers() > 0) {
          testNode = args->getMember(0);
        }
      }

      if (testNode->type == NODE_TYPE_VALUE ||
          testNode->type == NODE_TYPE_OBJECT) {
        // not really usable...
        continue;
      }

      if (testNode->type == NODE_TYPE_ARRAY &&
          testNode->numMembers() < AstNode::SortNumberThreshold) {
        // number of values is below threshold
        continue;
      }

      if (testNode->isSorted()) {
        // already sorted
        continue;
      }

      originalArg = originalNode;
    } else {
      TRI_ASSERT(setter->getType() == EN::SUBQUERY);
      auto sub = ExecutionNode::castTo<SubqueryNode*>(setter);

      // estimate items in subquery
      CostEstimate estimate = sub->getSubquery()->getCost();

      if (estimate.estimatedNrItems < AstNode::SortNumberThreshold) {
        continue;
      }

      originalArg = ast->createNodeReference(sub->outVariable());
    }

    TRI_ASSERT(originalArg != nullptr);

    auto args = ast->createNodeArray();
    args->addMember(originalArg);
    auto sorted = ast->createNodeFunctionCall(
        TRI_CHAR_LENGTH_PAIR("SORTED_UNIQUE"), args);

    auto outVar = ast->variables()->createTemporaryVariable();
    ExecutionNode* calculationNode = nullptr;
    auto expression = new Expression(plan.get(), ast, sorted);
    try {
      calculationNode =
          new CalculationNode(plan.get(), plan->nextId(), expression, outVar);
    } catch (...) {
      delete expression;
      throw;
    }
    plan->registerNode(calculationNode);

    // make the new node a parent of the original calculation node
    TRI_ASSERT(setter != nullptr);
    calculationNode->addDependency(setter);
    auto oldParent = setter->getFirstParent();
    TRI_ASSERT(oldParent != nullptr);
    calculationNode->addParent(oldParent);

    oldParent->removeDependencies();
    oldParent->addDependency(calculationNode);
    setter->setParent(calculationNode);

    AstNode* clone = ast->clone(inNode);
    // set sortedness bit for the IN operator
    clone->setBoolValue(true);
    // finally adjust the variable inside the IN calculation
    clone->changeMember(1, ast->createNodeReference(outVar));
    filterExpression->replaceNode(clone);

    modified = true;
  }

  opt->addPlan(std::move(plan), rule, modified);
}

/// @brief remove redundant sorts
/// this rule modifies the plan in place:
/// - sorts that are covered by earlier sorts will be removed
void arangodb::aql::removeRedundantSortsRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const* rule) {
  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, EN::SORT, true);

  if (nodes.empty()) {
    // quick exit
    opt->addPlan(std::move(plan), rule, false);
    return;
  }

  std::unordered_set<ExecutionNode*> toUnlink;
  arangodb::basics::StringBuffer buffer;

  for (auto const& n : nodes) {
    if (toUnlink.find(n) != toUnlink.end()) {
      // encountered a sort node that we already deleted
      continue;
    }

    auto const sortNode = ExecutionNode::castTo<SortNode*>(n);

    auto sortInfo = sortNode->getSortInformation(plan.get(), &buffer);

    if (sortInfo.isValid && !sortInfo.criteria.empty()) {
      // we found a sort that we can understand
      std::vector<ExecutionNode*> stack;

      sortNode->dependencies(stack);

      int nodesRelyingOnSort = 0;

      while (!stack.empty()) {
        auto current = stack.back();
        stack.pop_back();

        if (current->getType() == EN::SORT) {
          // we found another sort. now check if they are compatible!

          auto other =
              ExecutionNode::castTo<SortNode*>(current)->getSortInformation(
                  plan.get(), &buffer);

          switch (sortInfo.isCoveredBy(other)) {
            case SortInformation::unequal: {
              // different sort criteria
              if (nodesRelyingOnSort == 0) {
                // a sort directly followed by another sort: now remove one of
                // them

                if (!other.isDeterministic) {
                  // if the sort is non-deterministic, we must not remove it
                  break;
                }

                if (sortNode->isStable()) {
                  // we should not optimize predecessors of a stable sort (used
                  // in a COLLECT node)
                  // the stable sort is for a reason, and removing any
                  // predecessors sorts might
                  // change the result
                  break;
                }

                // remove sort that is a direct predecessor of a sort
                toUnlink.emplace(current);
              }
              break;
            }

            case SortInformation::otherLessAccurate: {
              toUnlink.emplace(current);
              break;
            }

            case SortInformation::ourselvesLessAccurate: {
              // the sort at the start of the pipeline makes the sort at the end
              // superfluous, so we'll remove it
              toUnlink.emplace(n);
              break;
            }

            case SortInformation::allEqual: {
              // the sort at the end of the pipeline makes the sort at the start
              // superfluous, so we'll remove it
              toUnlink.emplace(current);
              break;
            }
          }
        } else if (current->getType() == EN::FILTER) {
          // ok: a filter does not depend on sort order
        } else if (current->getType() == EN::CALCULATION) {
          // ok: a calculation does not depend on sort order only if it is
          // deterministic
          if (!current->isDeterministic()) {
            ++nodesRelyingOnSort;
          }
        } else if (current->getType() == EN::ENUMERATE_LIST ||
                   current->getType() == EN::ENUMERATE_COLLECTION ||
                   current->getType() == EN::TRAVERSAL ||
                   current->getType() == EN::SHORTEST_PATH) {
          // ok, but we cannot remove two different sorts if one of these node
          // types is between them
          // example: in the following query, the one sort will be optimized
          // away:
          //   FOR i IN [ { a: 1 }, { a: 2 } , { a: 3 } ] SORT i.a ASC SORT i.a
          //   DESC RETURN i
          // but in the following query, the sorts will stay:
          //   FOR i IN [ { a: 1 }, { a: 2 } , { a: 3 } ] SORT i.a ASC LET a =
          //   i.a SORT i.a DESC RETURN i
          ++nodesRelyingOnSort;
        } else {
          // abort at all other type of nodes. we cannot remove a sort beyond
          // them
          // this includes COLLECT and LIMIT
          break;
        }

        if (!current->hasDependency()) {
          // node either has no or more than one dependency. we don't know what
          // to do and must abort
          // note: this will also handle Singleton nodes
          break;
        }

        current->dependencies(stack);
      }

      if (toUnlink.find(n) == toUnlink.end() &&
          sortNode->simplify(plan.get())) {
        // sort node had only constant expressions. it will make no difference
        // if we execute it or not
        // so we can remove it
        toUnlink.emplace(n);
      }
    }
  }

  if (!toUnlink.empty()) {
    plan->unlinkNodes(toUnlink);
  }

  opt->addPlan(std::move(plan), rule, !toUnlink.empty());
}

/// @brief remove all unnecessary filters
/// this rule modifies the plan in place:
/// - filters that are always true are removed completely
/// - filters that are always false will be replaced by a NoResults node
void arangodb::aql::removeUnnecessaryFiltersRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const* rule) {
  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, EN::FILTER, true);

  bool modified = false;
  std::unordered_set<ExecutionNode*> toUnlink;

  for (auto const& n : nodes) {
    // filter nodes always have one input variable
    auto varsUsedHere = n->getVariablesUsedHere();
    TRI_ASSERT(varsUsedHere.size() == 1);

    // now check who introduced our variable
    auto variable = varsUsedHere[0];
    auto setter = plan->getVarSetBy(variable->id);

    if (setter == nullptr || setter->getType() != EN::CALCULATION) {
      // filter variable was not introduced by a calculation.
      continue;
    }

    // filter variable was introduced a CalculationNode. now check the
    // expression
    auto s = ExecutionNode::castTo<CalculationNode*>(setter);
    auto root = s->expression()->node();

    TRI_ASSERT(root != nullptr);

    if (!root->isDeterministic()) {
      // we better not tamper with this filter
      continue;
    }

    // filter expression is constant and thus cannot throw
    // we can now evaluate it safely

    if (root->isTrue()) {
      // filter is always true
      // remove filter node and merge with following node
      toUnlink.emplace(n);
      modified = true;
    } else if (root->isFalse()) {
      // filter is always false
      // now insert a NoResults node below it
      auto noResults = new NoResultsNode(plan.get(), plan->nextId());
      plan->registerNode(noResults);
      plan->replaceNode(n, noResults);
      modified = true;
    }
  }

  if (!toUnlink.empty()) {
    plan->unlinkNodes(toUnlink);
  }

  opt->addPlan(std::move(plan), rule, modified);
}

/// @brief remove INTO of a COLLECT if not used
/// additionally remove all unused aggregate calculations from a COLLECT
void arangodb::aql::removeCollectVariablesRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const* rule) {
  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, EN::COLLECT, true);

  bool modified = false;

  for (auto const& n : nodes) {
    auto collectNode = ExecutionNode::castTo<CollectNode*>(n);
    TRI_ASSERT(collectNode != nullptr);

    auto const& varsUsedLater = n->getVarsUsedLater();
    auto outVariable = collectNode->outVariable();

    if (outVariable != nullptr &&
        varsUsedLater.find(outVariable) == varsUsedLater.end()) {
      // outVariable not used later
      if (!collectNode->count()) {
        collectNode->clearOutVariable();
      }
      modified = true;
    } else if (outVariable != nullptr && !collectNode->count() &&
               !collectNode->hasExpressionVariable() &&
               !collectNode->hasKeepVariables()) {
      // outVariable used later, no count, no INTO expression, no KEEP
      // e.g. COLLECT something INTO g
      // we will now check how many part of "g" are used later
      std::unordered_set<std::string> keepAttributes;

      bool stop = false;
      auto p = collectNode->getFirstParent();
      while (p != nullptr) {
        if (p->getType() == EN::CALCULATION) {
          auto cc = ExecutionNode::castTo<CalculationNode const*>(p);
          Expression const* exp = cc->expression();
          if (exp != nullptr && exp->node() != nullptr) {
            bool isSafeForOptimization;
            auto usedThere = Ast::getReferencedAttributesForKeep(
                exp->node(), outVariable, isSafeForOptimization);
            if (isSafeForOptimization) {
              for (auto const& it : usedThere) {
                keepAttributes.emplace(it);
              }
            } else {
              stop = true;
            }
          }
        }
        if (stop) {
          break;
        }
        p = p->getFirstParent();
      }

      if (!stop) {
        std::vector<Variable const*> keepVariables;
        // we are allowed to do the optimization
        auto current = n->getFirstDependency();
        while (current != nullptr) {
          for (auto const& var : current->getVariablesSetHere()) {
            for (auto it = keepAttributes.begin(); it != keepAttributes.end();
                 /* no hoisting */) {
              if ((*it) == var->name) {
                keepVariables.emplace_back(var);
                it = keepAttributes.erase(it);
              } else {
                ++it;
              }
            }
          }
          if (keepAttributes.empty()) {
            // done
            break;
          }
          current = current->getFirstDependency();
        }

        if (keepAttributes.empty() && !keepVariables.empty()) {
          collectNode->setKeepVariables(std::move(keepVariables));
          modified = true;
        }
      }
    }

    collectNode->clearAggregates(
        [&varsUsedLater, &modified](
            std::pair<Variable const*,
                      std::pair<Variable const*, std::string>> const& aggregate)
            -> bool {
          if (varsUsedLater.find(aggregate.first) == varsUsedLater.end()) {
            // result of aggregate function not used later
            modified = true;
            return true;
          }
          return false;
        });
  }

  opt->addPlan(std::move(plan), rule, modified);
}

class PropagateConstantAttributesHelper {
 public:
  PropagateConstantAttributesHelper() : _constants(), _modified(false) {}

  bool modified() const { return _modified; }

  /// @brief inspects a plan and propages constant values in expressions
  void propagateConstants(ExecutionPlan* plan) {
    SmallVector<ExecutionNode*>::allocator_type::arena_type a;
    SmallVector<ExecutionNode*> nodes{a};
    plan->findNodesOfType(nodes, EN::FILTER, true);

    for (auto const& node : nodes) {
      auto fn = ExecutionNode::castTo<FilterNode*>(node);

      auto inVar = fn->getVariablesUsedHere();
      TRI_ASSERT(inVar.size() == 1);

      auto setter = plan->getVarSetBy(inVar[0]->id);
      if (setter != nullptr && setter->getType() == EN::CALCULATION) {
        auto cn = ExecutionNode::castTo<CalculationNode*>(setter);
        auto expression = cn->expression();

        if (expression != nullptr) {
          collectConstantAttributes(const_cast<AstNode*>(expression->node()));
        }
      }
    }

    if (!_constants.empty()) {
      for (auto const& node : nodes) {
        auto fn = ExecutionNode::castTo<FilterNode*>(node);

        auto inVar = fn->getVariablesUsedHere();
        TRI_ASSERT(inVar.size() == 1);

        auto setter = plan->getVarSetBy(inVar[0]->id);
        if (setter != nullptr && setter->getType() == EN::CALCULATION) {
          auto cn = ExecutionNode::castTo<CalculationNode*>(setter);
          auto expression = cn->expression();

          if (expression != nullptr) {
            insertConstantAttributes(const_cast<AstNode*>(expression->node()));
          }
        }
      }
    }
  }

 private:
  AstNode const* getConstant(Variable const* variable,
                             std::string const& attribute) const {
    auto it = _constants.find(variable);

    if (it == _constants.end()) {
      return nullptr;
    }

    auto it2 = (*it).second.find(attribute);

    if (it2 == (*it).second.end()) {
      return nullptr;
    }

    return (*it2).second;
  }

  /// @brief inspects an expression (recursively) and notes constant attribute
  /// values so they can be propagated later
  void collectConstantAttributes(AstNode* node) {
    if (node == nullptr) {
      return;
    }

    if (node->type == NODE_TYPE_OPERATOR_BINARY_AND) {
      auto lhs = node->getMember(0);
      auto rhs = node->getMember(1);

      collectConstantAttributes(lhs);
      collectConstantAttributes(rhs);
    } else if (node->type == NODE_TYPE_OPERATOR_BINARY_EQ) {
      auto lhs = node->getMember(0);
      auto rhs = node->getMember(1);

      if (lhs->isConstant() && rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
        inspectConstantAttribute(rhs, lhs);
      } else if (rhs->isConstant() && lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
        inspectConstantAttribute(lhs, rhs);
      }
    }
  }

  /// @brief traverses an AST part recursively and patches it by inserting
  /// constant values
  void insertConstantAttributes(AstNode* node) {
    if (node == nullptr) {
      return;
    }

    if (node->type == NODE_TYPE_OPERATOR_BINARY_AND) {
      auto lhs = node->getMember(0);
      auto rhs = node->getMember(1);

      insertConstantAttributes(lhs);
      insertConstantAttributes(rhs);
    } else if (node->type == NODE_TYPE_OPERATOR_BINARY_EQ) {
      auto lhs = node->getMember(0);
      auto rhs = node->getMember(1);

      if (!lhs->isConstant() && rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
        insertConstantAttribute(node, 1);
      }
      if (!rhs->isConstant() && lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
        insertConstantAttribute(node, 0);
      }
    }
  }

  /// @brief extract an attribute and its variable from an attribute access
  /// (e.g. `a.b.c` will return variable `a` and attribute name `b.c.`.
  bool getAttribute(AstNode const* attribute, Variable const*& variable,
                    std::string& name) {
    TRI_ASSERT(attribute != nullptr &&
               attribute->type == NODE_TYPE_ATTRIBUTE_ACCESS);
    TRI_ASSERT(name.empty());

    while (attribute->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
      name = std::string(".") + attribute->getString() + name;
      attribute = attribute->getMember(0);
    }

    if (attribute->type != NODE_TYPE_REFERENCE) {
      return false;
    }

    variable = static_cast<Variable const*>(attribute->getData());
    TRI_ASSERT(variable != nullptr);

    return true;
  }

  /// @brief inspect the constant value assigned to an attribute
  /// the attribute value will be stored so it can be inserted for the attribute
  /// later
  void inspectConstantAttribute(AstNode const* attribute,
                                AstNode const* value) {
    Variable const* variable = nullptr;
    std::string name;

    if (!getAttribute(attribute, variable, name)) {
      return;
    }

    auto it = _constants.find(variable);

    if (it == _constants.end()) {
      _constants.emplace(
          variable,
          std::unordered_map<std::string, AstNode const*>{{name, value}});
      return;
    }

    auto it2 = (*it).second.find(name);

    if (it2 == (*it).second.end()) {
      // first value for the attribute
      (*it).second.emplace(name, value);
    } else {
      auto previous = (*it2).second;

      if (previous == nullptr) {
        // we have multiple different values for the attribute. better not use
        // this attribute
        return;
      }

      if (!value->computeValue().equals(previous->computeValue())) {
        // different value found for an already tracked attribute. better not
        // use this attribute
        (*it2).second = nullptr;
      }
    }
  }

  /// @brief patches an AstNode by inserting a constant value into it
  void insertConstantAttribute(AstNode* parentNode, size_t accessIndex) {
    Variable const* variable = nullptr;
    std::string name;

    if (!getAttribute(parentNode->getMember(accessIndex), variable, name)) {
      return;
    }

    auto constantValue = getConstant(variable, name);

    if (constantValue != nullptr) {
      parentNode->changeMember(accessIndex,
                               const_cast<AstNode*>(constantValue));
      _modified = true;
    }
  }

  std::unordered_map<Variable const*,
                     std::unordered_map<std::string, AstNode const*>>
      _constants;

  bool _modified;
};

/// @brief propagate constant attributes in FILTERs
void arangodb::aql::propagateConstantAttributesRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const* rule) {
  PropagateConstantAttributesHelper helper;
  helper.propagateConstants(plan.get());

  opt->addPlan(std::move(plan), rule, helper.modified());
}

/// @brief move calculations up in the plan
/// this rule modifies the plan in place
/// it aims to move up calculations as far up in the plan as possible, to
/// avoid redundant calculations in inner loops
void arangodb::aql::moveCalculationsUpRule(Optimizer* opt,
                                           std::unique_ptr<ExecutionPlan> plan,
                                           OptimizerRule const* rule) {
  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, EN::CALCULATION, true);

  bool modified = false;
  std::unordered_set<Variable const*> neededVars;
  std::unordered_set<Variable const*> vars;

  for (auto const& n : nodes) {
    auto nn = ExecutionNode::castTo<CalculationNode*>(n);

    if (!nn->expression()->isDeterministic()) {
      // we will only move expressions up that cannot throw and that are
      // deterministic
      continue;
    }

    neededVars.clear();
    n->getVariablesUsedHere(neededVars);

    auto current = n->getFirstDependency();

    while (current != nullptr) {
      auto dep = current->getFirstDependency();

      if (dep == nullptr) {
        // node either has no or more than one dependency. we don't know what to
        // do and must abort
        // note: this will also handle Singleton nodes
        break;
      }

      if (current->setsVariable(neededVars)) {
        // shared variable, cannot move up any more
        // done with optimizing this calculation node
        break;
      }
      
      if (current->getType() == EN::LIMIT) { 
        if (!arangodb::ServerState::instance()->isCoordinator()) {
          // do not move calculations beyond a LIMIT on a single server,
          // as this would mean carrying out potentially unnecessary calculations
          break;
        }

        // coordinator case
        // now check if the calculation uses data from any collection. if so,
        // we expect that it is cheaper to execute the calculation close to the origin
        // of data (e.g. IndexNode, EnumerateCollectionNode) on a DB server than on
        // a coordinator. though executing the calculation will have the same costs
        // on DB server and coordinator, the assumption is that we can reduce the 
        // amount of data we need to transfer between the two if we can execute the
        // calculation on the DB server and only transfer the calculation result to
        // the coordinator instead of the full documents 

        if (!::accessesCollectionVariable(plan.get(), nn, vars)) {
          // not accessing any collection data
          break;
        }
        // accessing collection data.
        // allow the calculation to be moved beyond the LIMIT,
        // in the hope that this reduces the amount of data we have
        // to transfer between the DB server and the coordinator
      }

      // first, unlink the calculation from the plan
      plan->unlinkNode(n);

      // and re-insert into before the current node
      plan->insertDependency(current, n);

      modified = true;
      current = dep;
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}

/// @brief move calculations down in the plan
/// this rule modifies the plan in place
/// it aims to move calculations as far down in the plan as possible, beyond
/// FILTER and LIMIT operations
void arangodb::aql::moveCalculationsDownRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const* rule) {
  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, EN::CALCULATION, true);

  std::vector<ExecutionNode*> stack;
  std::unordered_set<Variable const*> vars;
  std::unordered_set<Variable const*> usedHere;
  bool modified = false;

  for (auto const& n : nodes) {
    auto nn = ExecutionNode::castTo<CalculationNode*>(n);
    if (!nn->expression()->isDeterministic()) {
      // we will only move expressions down that cannot throw and that are
      // deterministic
      continue;
    }

    // this is the variable that the calculation will set
    auto variable = nn->outVariable();

    stack.clear();
    n->parents(stack);

    ExecutionNode* lastNode = nullptr;

    while (!stack.empty()) {
      auto current = stack.back();
      stack.pop_back();

      bool done = false;

      usedHere.clear();
      current->getVariablesUsedHere(usedHere);
      for (auto const& v : usedHere) {
        if (v == variable) {
          // the node we're looking at needs the variable we're setting.
          // can't push further!
          done = true;
          break;
        }
      }

      if (done) {
        // done with optimizing this calculation node
        break;
      }

      auto const currentType = current->getType();

      if (currentType == EN::FILTER || currentType == EN::SORT ||
          currentType == EN::LIMIT || currentType == EN::SUBQUERY) {
        // we found something interesting that justifies moving our node down
        if (currentType == EN::LIMIT && 
            arangodb::ServerState::instance()->isCoordinator()) {
          // in a cluster, we do not want to move the calculations as far down
          // as possible, because this will mean we may need to transfer a lot
          // more data between DB servers and the coordinator

          // assume first that we want to move the node past the LIMIT

          // however, if our calculation uses any data from a
          // collection/index/view, it probably makes sense to not move it,
          // because the result set may be huge
          if (::accessesCollectionVariable(plan.get(), nn, vars)) {
            done = true;
            break;
          }
        } 
        
        if (!done) {
          lastNode = current;
        }
      } else if (currentType == EN::INDEX ||
                 currentType == EN::ENUMERATE_COLLECTION ||
#ifdef USE_IRESEARCH
                 currentType == EN::ENUMERATE_IRESEARCH_VIEW ||
#endif
                 currentType == EN::ENUMERATE_LIST ||
                 currentType == EN::TRAVERSAL ||
                 currentType == EN::SHORTEST_PATH ||
                 currentType == EN::COLLECT || currentType == EN::NORESULTS) {
        // we will not push further down than such nodes
        done = true;
        break;
      }
      
      if (done || !current->hasParent()) {
        break;
      }

      current->parents(stack);
    }

    if (lastNode != nullptr && lastNode->getFirstParent() != nullptr) {
      // first, unlink the calculation from the plan
      plan->unlinkNode(n);

      // and re-insert into after the last "good" node
      plan->insertDependency(lastNode->getFirstParent(), n);
      modified = true;
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}

/// @brief determine the "right" type of CollectNode and
/// add a sort node for each COLLECT (note: the sort may be removed later)
/// this rule cannot be turned off (otherwise, the query result might be wrong!)
void arangodb::aql::specializeCollectRule(Optimizer* opt,
                                          std::unique_ptr<ExecutionPlan> plan,
                                          OptimizerRule const* rule) {
  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, EN::COLLECT, true);

  bool modified = false;

  for (auto const& n : nodes) {
    auto collectNode = ExecutionNode::castTo<CollectNode*>(n);

    if (collectNode->isSpecialized()) {
      // already specialized this node
      continue;
    }

    auto const& groupVariables = collectNode->groupVariables();

    // test if we can use an alternative version of COLLECT with a hash table
    bool const canUseHashAggregation =
        (!groupVariables.empty() &&
         (!collectNode->hasOutVariable() || collectNode->count()) &&
         collectNode->getOptions().canUseMethod(
             CollectOptions::CollectMethod::HASH));

    if (canUseHashAggregation && !opt->runOnlyRequiredRules(1)) {
      if (collectNode->getOptions().shouldUseMethod(
              CollectOptions::CollectMethod::HASH)) {
        // user has explicitly asked for hash method
        // specialize existing the CollectNode so it will become a
        // HashedCollectBlock later. additionally, add a SortNode BEHIND the
        // CollectNode (to sort the final result)
        collectNode->aggregationMethod(CollectOptions::CollectMethod::HASH);
        collectNode->specialized();

        if (!collectNode->isDistinctCommand()) {
          // add the post-SORT
          SortElementVector sortElements;
          for (auto const& v : collectNode->groupVariables()) {
            sortElements.emplace_back(v.first, true);
          }

          auto sortNode =
              new SortNode(plan.get(), plan->nextId(), sortElements, false);
          plan->registerNode(sortNode);

          TRI_ASSERT(collectNode->hasParent());
          auto parent = collectNode->getFirstParent();
          TRI_ASSERT(parent != nullptr);

          sortNode->addDependency(collectNode);
          parent->replaceDependency(collectNode, sortNode);
        }

        modified = true;
        continue;
      }

      // create a new plan with the adjusted COLLECT node
      std::unique_ptr<ExecutionPlan> newPlan(plan->clone());

      // use the cloned COLLECT node
      auto newCollectNode = ExecutionNode::castTo<CollectNode*>(
          newPlan->getNodeById(collectNode->id()));
      TRI_ASSERT(newCollectNode != nullptr);

      // specialize the CollectNode so it will become a HashedCollectBlock
      // later
      // additionally, add a SortNode BEHIND the CollectNode (to sort the
      // final result)
      newCollectNode->aggregationMethod(CollectOptions::CollectMethod::HASH);
      newCollectNode->specialized();

      if (!collectNode->isDistinctCommand()) {
        // add the post-SORT
        SortElementVector sortElements;
        for (auto const& v : newCollectNode->groupVariables()) {
          sortElements.emplace_back(v.first, true);
        }

        auto sortNode =
            new SortNode(newPlan.get(), newPlan->nextId(), sortElements, false);
        newPlan->registerNode(sortNode);

        TRI_ASSERT(newCollectNode->hasParent());
        auto parent = newCollectNode->getFirstParent();
        TRI_ASSERT(parent != nullptr);

        sortNode->addDependency(newCollectNode);
        parent->replaceDependency(newCollectNode, sortNode);
      }

      if (nodes.size() > 1) {
        // this will tell the optimizer to optimize the cloned plan with this
        // specific rule again
        opt->addPlan(std::move(newPlan), rule, true,
                     static_cast<int>(rule->level - 1));
      } else {
        // no need to run this specific rule again on the cloned plan
        opt->addPlan(std::move(newPlan), rule, true);
      }
    } else if (groupVariables.empty() &&
               collectNode->aggregateVariables().empty() &&
               collectNode->count()) {
      collectNode->aggregationMethod(CollectOptions::CollectMethod::COUNT);
      collectNode->specialized();
      modified = true;
      continue;
    }

    // mark node as specialized, so we do not process it again
    collectNode->specialized();

    // finally, adjust the original plan and create a sorted version of COLLECT

    // specialize the CollectNode so it will become a SortedCollectBlock
    // later
    collectNode->aggregationMethod(CollectOptions::CollectMethod::SORTED);

    // insert a SortNode IN FRONT OF the CollectNode
    if (!groupVariables.empty()) {
      SortElementVector sortElements;
      for (auto const& v : groupVariables) {
        sortElements.emplace_back(v.second, true);
      }

      auto sortNode =
          new SortNode(plan.get(), plan->nextId(), sortElements, true);
      plan->registerNode(sortNode);

      TRI_ASSERT(collectNode->hasDependency());
      auto dep = collectNode->getFirstDependency();
      TRI_ASSERT(dep != nullptr);
      sortNode->addDependency(dep);
      collectNode->replaceDependency(dep, sortNode);

      modified = true;
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}

/// @brief split and-combined filters and break them into smaller parts
void arangodb::aql::splitFiltersRule(Optimizer* opt,
                                     std::unique_ptr<ExecutionPlan> plan,
                                     OptimizerRule const* rule) {
  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, EN::FILTER, true);

  bool modified = false;

  for (auto const& n : nodes) {
    auto inVars(n->getVariablesUsedHere());
    TRI_ASSERT(inVars.size() == 1);
    auto setter = plan->getVarSetBy(inVars[0]->id);

    if (setter == nullptr || setter->getType() != EN::CALCULATION) {
      continue;
    }

    auto cn = ExecutionNode::castTo<CalculationNode*>(setter);
    auto const expression = cn->expression();

    if (!expression->isDeterministic() ||
        expression->node()->type != NODE_TYPE_OPERATOR_BINARY_AND) {
      continue;
    }

    std::vector<AstNode*> stack{expression->nodeForModification()};

    while (!stack.empty()) {
      auto current = stack.back();
      stack.pop_back();

      if (current->type == NODE_TYPE_OPERATOR_BINARY_AND) {
        stack.emplace_back(current->getMember(0));
        stack.emplace_back(current->getMember(1));
      } else {
        modified = true;

        ExecutionNode* calculationNode = nullptr;
        auto outVar = plan->getAst()->variables()->createTemporaryVariable();
        auto expression = new Expression(plan.get(), plan->getAst(), current);
        try {
          calculationNode = new CalculationNode(plan.get(), plan->nextId(),
                                                expression, outVar);
        } catch (...) {
          delete expression;
          throw;
        }
        plan->registerNode(calculationNode);

        plan->insertDependency(n, calculationNode);

        auto filterNode = new FilterNode(plan.get(), plan->nextId(), outVar);
        plan->registerNode(filterNode);

        plan->insertDependency(n, filterNode);
      }
    }

    if (modified) {
      plan->unlinkNode(n, false);
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}

/// @brief move filters up in the plan
/// this rule modifies the plan in place
/// filters are moved as far up in the plan as possible to make result sets
/// as small as possible as early as possible
/// filters are not pushed beyond limits
void arangodb::aql::moveFiltersUpRule(Optimizer* opt,
                                      std::unique_ptr<ExecutionPlan> plan,
                                      OptimizerRule const* rule) {
  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, EN::FILTER, true);

  bool modified = false;

  for (auto const& n : nodes) {
    auto neededVars = n->getVariablesUsedHere();
    TRI_ASSERT(neededVars.size() == 1);

    std::vector<ExecutionNode*> stack;
    n->dependencies(stack);

    while (!stack.empty()) {
      auto current = stack.back();
      stack.pop_back();

      if (current->getType() == EN::LIMIT) {
        // cannot push a filter beyond a LIMIT node
        break;
      }

      if (!current->isDeterministic()) {
        // TODO: validate if this is actually necessary
        // must not move a filter beyond a node that is non-deterministic
        break;
      }

      if (current->isModificationNode()) {
        // must not move a filter beyond a modification node
        break;
      }

      if (current->getType() == EN::CALCULATION) {
        // must not move a filter beyond a node with a non-deterministic result
        auto calculation =
            ExecutionNode::castTo<CalculationNode const*>(current);
        if (!calculation->expression()->isDeterministic()) {
          break;
        }
      }

      bool found = false;

      for (auto const& v : current->getVariablesSetHere()) {
        for (auto it = neededVars.begin(); it != neededVars.end(); ++it) {
          if ((*it)->id == v->id) {
            // shared variable, cannot move up any more
            found = true;
            break;
          }
        }
      }

      if (found) {
        // done with optimizing this calculation node
        break;
      }

      if (!current->hasDependency()) {
        // node either has no or more than one dependency. we don't know what to
        // do and must abort
        // note: this will also handle Singleton nodes
        break;
      }

      current->dependencies(stack);

      // first, unlink the filter from the plan
      plan->unlinkNode(n);
      // and re-insert into plan in front of the current node
      plan->insertDependency(current, n);
      modified = true;
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}

class arangodb::aql::RedundantCalculationsReplacer final
    : public WalkerWorker<ExecutionNode> {
 public:
  explicit RedundantCalculationsReplacer(Ast* ast,
      std::unordered_map<VariableId, Variable const*> const& replacements)
      : _ast(ast), 
        _replacements(replacements) {}

  template <typename T>
  void replaceStartTargetVariables(ExecutionNode* en) {
    auto node = static_cast<T*>(en);
    if (node->_inStartVariable != nullptr) {
      node->_inStartVariable =
          Variable::replace(node->_inStartVariable, _replacements);
    }
    if (node->_inTargetVariable != nullptr) {
      node->_inTargetVariable =
          Variable::replace(node->_inTargetVariable, _replacements);
    }
  }

  template <typename T>
  void replaceInVariable(ExecutionNode* en) {
    auto node = ExecutionNode::castTo<T*>(en);
    node->_inVariable = Variable::replace(node->_inVariable, _replacements);
  }

  void replaceInCalculation(ExecutionNode* en) {
    auto node = ExecutionNode::castTo<CalculationNode*>(en);
    std::unordered_set<Variable const*> variables;
    node->expression()->variables(variables);

    // check if the calculation uses any of the variables that we want to
    // replace
    for (auto const& it : variables) {
      if (_replacements.find(it->id) != _replacements.end()) {
        // calculation uses a to-be-replaced variable
        node->expression()->replaceVariables(_replacements);
        return;
      }
    }
  }

#ifdef USE_IRESEARCH
  void replaceInView(ExecutionNode* en) {
    auto view = ExecutionNode::castTo<arangodb::iresearch::IResearchViewNode*>(en);
    if (view->filterConditionIsEmpty()) {
      // nothing to do
      return;
    }
    AstNode const& search = view->filterCondition();
    std::unordered_set<Variable const*> variables;
    Ast::getReferencedVariables(&search, variables);

    // check if the search condition uses any of the variables that we want to
    // replace
    AstNode* cloned = nullptr;
    for (auto const& it : variables) {
      if (_replacements.find(it->id) != _replacements.end()) {
        if (cloned == nullptr) {
          // only clone the original search condition once
          cloned = _ast->clone(&search);
        }
        // calculation uses a to-be-replaced variable
        _ast->replaceVariables(cloned, _replacements);
      }
    }

    if (cloned != nullptr) {
      // exchange the filter condition
      view->filterCondition(cloned);
    }
  }
#endif

  bool before(ExecutionNode* en) override final {
    switch (en->getType()) {
      case EN::ENUMERATE_LIST: {
        replaceInVariable<EnumerateListNode>(en);
        break;
      }

#ifdef USE_IRESEARCH
      case EN::ENUMERATE_IRESEARCH_VIEW: {
        replaceInView(en);
        break;
      }
#endif

      case EN::RETURN: {
        replaceInVariable<ReturnNode>(en);
        break;
      }

      case EN::CALCULATION: {
        replaceInCalculation(en);
        break;
      }

      case EN::FILTER: {
        replaceInVariable<FilterNode>(en);
        break;
      }

      case EN::TRAVERSAL: {
        replaceInVariable<TraversalNode>(en);
        break;
      }

      case EN::SHORTEST_PATH: {
        replaceStartTargetVariables<ShortestPathNode>(en);
        break;
      }

      case EN::COLLECT: {
        auto node = ExecutionNode::castTo<CollectNode*>(en);
        for (auto& variable : node->_groupVariables) {
          variable.second = Variable::replace(variable.second, _replacements);
        }
        for (auto& variable : node->_keepVariables) {
          auto old = variable;
          variable = Variable::replace(old, _replacements);
        }
        for (auto& variable : node->_aggregateVariables) {
          variable.second.first =
              Variable::replace(variable.second.first, _replacements);
        }
        if (node->_expressionVariable != nullptr) {
          node->_expressionVariable =
              Variable::replace(node->_expressionVariable, _replacements);
        }
        for (auto const& it : _replacements) {
          node->_variableMap.emplace(it.second->id, it.second->name);
        }
        // node->_keepVariables does not need to be updated at the moment as the
        // "remove-redundant-calculations" rule will stop when it finds a
        // COLLECT with an INTO, and the "inline-subqueries" rule will abort
        // there as well
        break;
      }

      case EN::SORT: {
        auto node = ExecutionNode::castTo<SortNode*>(en);
        for (auto& variable : node->_elements) {
          variable.var = Variable::replace(variable.var, _replacements);
        }
        break;
      }

      case EN::GATHER: {
        auto node = ExecutionNode::castTo<GatherNode*>(en);
        for (auto& variable : node->_elements) {
          auto v = Variable::replace(variable.var, _replacements);
          if (v != variable.var) {
            variable.var = v;
          }
          variable.attributePath.clear();
        }
        break;
      }

      case EN::DISTRIBUTE: {
        auto node = ExecutionNode::castTo<DistributeNode*>(en);
        node->_variable = Variable::replace(node->_variable, _replacements);
        node->_alternativeVariable =
            Variable::replace(node->_alternativeVariable, _replacements);
        break;
      }

      case EN::REMOVE: {
        replaceInVariable<RemoveNode>(en);
        break;
      }

      case EN::INSERT: {
        replaceInVariable<InsertNode>(en);
        break;
      }

      case EN::UPSERT: {
        auto node = ExecutionNode::castTo<UpsertNode*>(en);

        if (node->_inDocVariable != nullptr) {
          node->_inDocVariable =
              Variable::replace(node->_inDocVariable, _replacements);
        }
        if (node->_insertVariable != nullptr) {
          node->_insertVariable =
              Variable::replace(node->_insertVariable, _replacements);
        }
        if (node->_updateVariable != nullptr) {
          node->_updateVariable =
              Variable::replace(node->_updateVariable, _replacements);
        }
        break;
      }

      case EN::UPDATE: {
        auto node = ExecutionNode::castTo<UpdateNode*>(en);

        if (node->_inDocVariable != nullptr) {
          node->_inDocVariable =
              Variable::replace(node->_inDocVariable, _replacements);
        }
        if (node->_inKeyVariable != nullptr) {
          node->_inKeyVariable =
              Variable::replace(node->_inKeyVariable, _replacements);
        }
        break;
      }

      case EN::REPLACE: {
        auto node = ExecutionNode::castTo<ReplaceNode*>(en);

        if (node->_inDocVariable != nullptr) {
          node->_inDocVariable =
              Variable::replace(node->_inDocVariable, _replacements);
        }
        if (node->_inKeyVariable != nullptr) {
          node->_inKeyVariable =
              Variable::replace(node->_inKeyVariable, _replacements);
        }
        break;
      }
     
#if 0 
      // TODO: figure out if this does any harm
      case EN::REMOTESINGLE: {
        replaceInVariable<SingleRemoteOperationNode>(en);
        break;
      }
#endif
      default: {
        // ignore all other types of nodes
      }
    }

    // always continue
    return false;
  }

 private:
  Ast* _ast;
  std::unordered_map<VariableId, Variable const*> const& _replacements;
};

/// @brief simplify conditions in CalculationNodes
void arangodb::aql::simplifyConditionsRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const* rule) {
  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, EN::CALCULATION, true);
 
  if (nodes.empty()) {
    opt->addPlan(std::move(plan), rule, false);
    return;
  }

  auto p = plan.get();

  auto visitor = [p](AstNode* node) {
again:
    if (node->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
      auto const* accessed = node->getMemberUnchecked(0);

      if (accessed->type == NODE_TYPE_REFERENCE) {
        Variable const* v = static_cast<Variable const*>(accessed->getData());
        TRI_ASSERT(v != nullptr);

        auto setter = p->getVarSetBy(v->id);
        
        if (setter == nullptr || setter->getType() != EN::CALCULATION) {
          return node;
        }

        accessed = ExecutionNode::castTo<CalculationNode*>(setter)->expression()->node();
        if (accessed == nullptr) {
          return node;
        }
      }

      TRI_ASSERT(accessed != nullptr);

      if (accessed->type == NODE_TYPE_OBJECT) {
        StringRef const attributeName(node->getStringValue(), node->getStringLength());
        bool isDynamic = false;
        size_t const n = accessed->numMembers();
        for (size_t i = 0; i < n; ++i) {
          auto member = accessed->getMemberUnchecked(i);

          if (member->type == NODE_TYPE_OBJECT_ELEMENT &&
              StringRef(member->getStringValue(), member->getStringLength()) == attributeName) {
            // found the attribute!
            node = member->getMember(0);
            // now try optimizing the simplified condition
            // time for a goto...!
            goto again;
          } else if (member->type == NODE_TYPE_CALCULATED_OBJECT_ELEMENT) {
            // dynamic attribute name
            isDynamic = true;
          }
        }

        // attribute not found 
        if (!isDynamic) {
          return Ast::createNodeValueNull(); 
        }
      }
    } else if (node->type == NODE_TYPE_INDEXED_ACCESS) {
      auto const* accessed = node->getMember(0);

      if (accessed->type == NODE_TYPE_REFERENCE) {
        Variable const* v = static_cast<Variable const*>(accessed->getData());
        TRI_ASSERT(v != nullptr);

        auto setter = p->getVarSetBy(v->id);
        
        if (setter == nullptr || setter->getType() != EN::CALCULATION) {
          return node;
        }

        accessed = ExecutionNode::castTo<CalculationNode*>(setter)->expression()->node();
        if (accessed == nullptr) {
          return node;
        }
      }
      
      auto indexValue = node->getMember(1);

      if (!indexValue->isConstant() || !(indexValue->isStringValue() || indexValue->isNumericValue())) {
        // cant handle this type of index statically
        return node;
      }
      
      if (accessed->type == NODE_TYPE_OBJECT) {
        StringRef attributeName;
        std::string indexString;

        if (indexValue->isStringValue()) {
          // string index, e.g. ['123']
          attributeName = StringRef(indexValue->getStringValue(), indexValue->getStringLength());
        } else {
          // numeric index, e.g. [123]
          TRI_ASSERT(indexValue->isNumericValue());
          // convert the numeric index into a string
          indexString = std::to_string(indexValue->getIntValue());
          attributeName = StringRef(indexString);
        }

        bool isDynamic = false;
        size_t const n = accessed->numMembers();
        for (size_t i = 0; i < n; ++i) {
          auto member = accessed->getMemberUnchecked(i);

          if (member->type == NODE_TYPE_OBJECT_ELEMENT &&
              StringRef(member->getStringValue(), member->getStringLength()) == attributeName) {
            // found the attribute!
            node = member->getMember(0);
            // now try optimizing the simplified condition
            // time for a goto...!
            goto again;
          } else if (member->type == NODE_TYPE_CALCULATED_OBJECT_ELEMENT) {
            // dynamic attribute name
            isDynamic = true;
          }
        } 

        // attribute not found
        if (!isDynamic) {
          return Ast::createNodeValueNull(); 
        }
      } else if (accessed->type == NODE_TYPE_ARRAY) {
        int64_t position;
        if (indexValue->isStringValue()) {
          // string index, e.g. ['123'] -> convert to a numeric index
          bool valid;
          position = NumberUtils::atoi<int64_t>(indexValue->getStringValue(), indexValue->getStringValue() + indexValue->getStringLength(), valid);
          if (!valid) {
            // invalid index
            return Ast::createNodeValueNull(); 
          }
        } else {
          // numeric index, e.g. [123]
          TRI_ASSERT(indexValue->isNumericValue());
          position = indexValue->getIntValue();
        }
        int64_t const n = accessed->numMembers();
        if (position < 0) {
          // a negative position is allowed
          position = n + position;
        }
        if (position >= 0 && position < n) {
          node = accessed->getMember(static_cast<size_t>(position));
          // now try optimizing the simplified condition
          // time for a goto...!
          goto again;
        }
        
        // index out of bounds    
        return Ast::createNodeValueNull(); 
      }
    }

    return node;
  };
   
  bool modified = false;
  for (auto const& n : nodes) {
    auto nn = ExecutionNode::castTo<CalculationNode*>(n);

    if (!nn->expression()->isDeterministic()) {
      // If this node is non-deterministic, we must not touch it!
      continue;
    }

    AstNode* root = nn->expression()->nodeForModification();

    if (root != nullptr) {
      AstNode* simplified = plan->getAst()->traverseAndModify(root, visitor);
      if (simplified != root) {
        nn->expression()->replaceNode(simplified);
        modified = true;
      }
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}

/// @brief fuse filter conditions that follow each other
void arangodb::aql::fuseFiltersRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const* rule) {
  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, EN::FILTER, true);
 
  if (nodes.size() < 2) {
    opt->addPlan(std::move(plan), rule, false);
    return;
  }

  std::unordered_set<ExecutionNode*> seen;
  // candidates of CalculationNode, FilterNode
  std::vector<std::pair<ExecutionNode*, ExecutionNode*>> candidates;

  bool modified = false;
  
  for (auto const& n : nodes) {
    if (seen.find(n) != seen.end()) {
      // already processed
      continue;
    }

    Variable const* nextExpectedVariable = nullptr;
    ExecutionNode* lastFilter = nullptr;
    candidates.clear();

    ExecutionNode* current = n;
    while (current != nullptr) {
      if (current->getType() == EN::CALCULATION) {
        auto cn = ExecutionNode::castTo<CalculationNode*>(current);
        if (!cn->isDeterministic() || cn->outVariable() != nextExpectedVariable) {
          break;
        }
        TRI_ASSERT(lastFilter != nullptr);
        candidates.emplace_back(current, lastFilter);
        nextExpectedVariable = nullptr;
      } else if (current->getType() == EN::FILTER) {
        seen.emplace(current);

        if (nextExpectedVariable != nullptr) {
          // an unexpected order of nodes
          break;
        }
        nextExpectedVariable = ExecutionNode::castTo<FilterNode const*>(current)->inVariable();
        TRI_ASSERT(nextExpectedVariable != nullptr);
        if (current->isVarUsedLater(nextExpectedVariable)) {
          // filter input variable is also used for other things. we must not
          // remove it or the corresponding calculation
          break;
        }
        lastFilter = current;
      } else {
        // all other types of nodes we cannot optimize
        break;
      }
      current = current->getFirstDependency();
    }

    if (candidates.size() >= 2) {
      modified = true;
      AstNode* root = ExecutionNode::castTo<CalculationNode*>(candidates[0].first)->expression()->nodeForModification();
      for (size_t i = 1; i < candidates.size(); ++i) {
        root = plan->getAst()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_AND, ExecutionNode::castTo<CalculationNode const*>(candidates[i].first)->expression()->node(), root); 
        
        // throw away all now-unused filters and calculations
        plan->unlinkNode(candidates[i - 1].second);
        plan->unlinkNode(candidates[i - 1].first);
      }

      ExecutionNode* en = candidates.back().first;
      TRI_ASSERT(en->getType() == EN::CALCULATION);
      ExecutionNode::castTo<CalculationNode*>(en)->expression()->replaceNode(root);
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}

/// @brief remove CalculationNode(s) that are repeatedly used in a query
/// (i.e. common expressions)
void arangodb::aql::removeRedundantCalculationsRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const* rule) {
  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, EN::CALCULATION, true);

  if (nodes.size() < 2) {
    // quick exit
    opt->addPlan(std::move(plan), rule, false);
    return;
  }

  arangodb::basics::StringBuffer buffer; 
  std::unordered_map<VariableId, Variable const*> replacements;

  for (auto const& n : nodes) {
    auto nn = ExecutionNode::castTo<CalculationNode*>(n);

    if (!nn->expression()->isDeterministic()) {
      // If this node is non-deterministic, we must not touch it!
      continue;
    }

    auto outvar = n->getVariablesSetHere();
    TRI_ASSERT(outvar.size() == 1);

    try {
      nn->expression()->stringifyIfNotTooLong(&buffer);
    } catch (...) {
      // expression could not be stringified (maybe because not all node types
      // are supported). this is not an error, we just skip the optimization
      buffer.reset();
      continue;
    }

    std::string const referenceExpression(buffer.c_str(), buffer.length());
    buffer.reset();

    std::vector<ExecutionNode*> stack;
    n->dependencies(stack);

    while (!stack.empty()) {
      auto current = stack.back();
      stack.pop_back();

      if (current->getType() == EN::CALCULATION) {
        try {
          // ExecutionNode::castTo<CalculationNode*>(current)->expression()->node()->dump(0);
          ExecutionNode::castTo<CalculationNode*>(current)
              ->expression()
              ->stringifyIfNotTooLong(&buffer);
        } catch (...) {
          // expression could not be stringified (maybe because not all node
          // types are supported). this is not an error, we just skip the
          // optimization
          buffer.reset();
          continue;
        }

        bool const isEqual =
            (buffer.length() == referenceExpression.size() &&
             memcmp(buffer.c_str(), referenceExpression.c_str(),
                    buffer.length()) == 0);
        buffer.reset();

        if (isEqual) {
          // expressions are identical
          auto outvars = current->getVariablesSetHere();
          TRI_ASSERT(outvars.size() == 1);

          // check if target variable is already registered as a replacement
          // this covers the following case:
          // - replacements is set to B => C
          // - we're now inserting a replacement A => B
          // the goal now is to enter a replacement A => C instead of A => B
          auto target = outvars[0];
          while (target != nullptr) {
            auto it = replacements.find(target->id);

            if (it != replacements.end()) {
              target = (*it).second;
            } else {
              break;
            }
          }
          replacements.emplace(outvar[0]->id, target);

          // also check if the insertion enables further shortcuts
          // this covers the following case:
          // - replacements is set to A => B
          // - we have just inserted a replacement B => C
          // the goal now is to change the replacement A => B to A => C
          for (auto it = replacements.begin(); it != replacements.end(); ++it) {
            if ((*it).second == outvar[0]) {
              (*it).second = target;
            }
          }
        }
      }

      if (current->getType() == EN::COLLECT) {
        if (ExecutionNode::castTo<CollectNode*>(current)->hasOutVariable()) {
          // COLLECT ... INTO is evil (tm): it needs to keep all already defined
          // variables
          // we need to abort optimization here
          break;
        }
      }

      if (!current->hasDependency()) {
        // node either has no or more than one dependency. we don't know what to
        // do and must abort
        // note: this will also handle Singleton nodes
        break;
      }

      current->dependencies(stack);
    }
  }

  if (!replacements.empty()) {
    // finally replace the variables
    RedundantCalculationsReplacer finder(plan->getAst(), replacements);
    plan->root()->walk(finder);
  }

  opt->addPlan(std::move(plan), rule, !replacements.empty());
}

/// @brief remove CalculationNodes and SubqueryNodes that are never needed
/// this modifies an existing plan in place
void arangodb::aql::removeUnnecessaryCalculationsRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const* rule) {
  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, ::removeUnnecessaryCalculationsNodeTypes, true);

  std::unordered_set<ExecutionNode*> toUnlink;

  for (auto const& n : nodes) {
    if (n->getType() == EN::CALCULATION) {
      auto nn = ExecutionNode::castTo<CalculationNode*>(n);

      if (!nn->isDeterministic()) {
        // If this node is non-deterministic, we must not optimize it away!
        continue;
      }
      // will remove calculation when we get here
    } else if (n->getType() == EN::SUBQUERY) {
      auto nn = ExecutionNode::castTo<SubqueryNode*>(n);

      if (!nn->isDeterministic()) {
        // subqueries that are non-deterministic must not be optimized away
        continue;
      }

      if (nn->isModificationSubquery()) {
        // subqueries that modify data must not be optimized away
        continue;
      }
      // will remove subquery when we get here
    }

    auto outvars = n->getVariablesSetHere();
    TRI_ASSERT(outvars.size() == 1);

    if (!n->isVarUsedLater(outvars[0])) {
      // The variable whose value is calculated here is not used at
      // all further down the pipeline! We remove the whole
      // calculation node,
      toUnlink.emplace(n);
    } else if (n->getType() == EN::CALCULATION) {
      // variable is still used later, but...
      // ...if it's used exactly once later by another calculation,
      // it's a temporary variable that we can fuse with the other
      // calculation easily

      if (!ExecutionNode::castTo<CalculationNode*>(n)
               ->expression()
               ->isDeterministic()) {
        continue;
      }

      AstNode const* rootNode =
          ExecutionNode::castTo<CalculationNode*>(n)->expression()->node();

      if (rootNode->type == NODE_TYPE_REFERENCE) {
        // if the LET is a simple reference to another variable, e.g. LET a = b
        // then replace all references to a with references to b
        bool hasCollectWithOutVariable = false;
        auto current = n->getFirstParent();

        // check first if we have a COLLECT with an INTO later in the query
        // in this case we must not perform the replacements
        while (current != nullptr) {
          if (current->getType() == EN::COLLECT) {
            if (ExecutionNode::castTo<CollectNode const*>(current)
                    ->hasOutVariableButNoCount()) {
              hasCollectWithOutVariable = true;
              break;
            }
          }
          current = current->getFirstParent();
        }

        if (!hasCollectWithOutVariable) {
          // no COLLECT found, now replace
          std::unordered_map<VariableId, Variable const*> replacements;
          replacements.emplace(outvars[0]->id, static_cast<Variable const*>(
                                                   rootNode->getData()));

          RedundantCalculationsReplacer finder(plan->getAst(), replacements);
          plan->root()->walk(finder);
          toUnlink.emplace(n);
          continue;
        }
      }

      std::unordered_set<Variable const*> vars;

      size_t usageCount = 0;
      CalculationNode* other = nullptr;
      auto current = n->getFirstParent();

      while (current != nullptr) {
        current->getVariablesUsedHere(vars);
        if (vars.find(outvars[0]) != vars.end()) {
          if (current->getType() == EN::COLLECT) {
            if (ExecutionNode::castTo<CollectNode const*>(current)
                    ->hasOutVariableButNoCount()) {
              // COLLECT with an INTO variable will collect all variables from
              // the scope, so we shouldn't try to remove or change the meaning
              // of variables
              usageCount = 0;
              break;
            }
          }
          if (current->getType() != EN::CALCULATION) {
            // don't know how to replace the variable in a non-LET node
            // abort the search
            usageCount = 0;
            break;
          }

          // got a LET. we can replace the variable reference in it by
          // something else
          ++usageCount;
          other = ExecutionNode::castTo<CalculationNode*>(current);
        }

        if (usageCount > 1) {
          break;
        }

        current = current->getFirstParent();
        vars.clear();
      }

      if (usageCount == 1) {
        // our variable is used by exactly one other calculation
        // now we can replace the reference to our variable in the other
        // calculation with the variable's expression directly
        auto otherExpression = other->expression();
        TRI_ASSERT(otherExpression != nullptr);

        if (rootNode->type != NODE_TYPE_ATTRIBUTE_ACCESS &&
            Ast::countReferences(otherExpression->node(), outvars[0]) > 1) {
          // used more than once... better give up
          continue;
        }

        if (rootNode->isSimple() != otherExpression->node()->isSimple()) {
          // expression types (V8 vs. non-V8) do not match. give up
          continue;
        }

        if (!n->isInInnerLoop() && rootNode->callsFunction() &&
            other->isInInnerLoop()) {
          // original expression calls a function and is not contained in a loop
          // we're about to move this expression into a loop, but we don't want
          // to move (expensive) function calls into loops
          continue;
        }

        TRI_ASSERT(other != nullptr);
        otherExpression->replaceVariableReference(outvars[0], rootNode);

        toUnlink.emplace(n);
      }
    }
  }

  if (!toUnlink.empty()) {
    plan->unlinkNodes(toUnlink);
  }

  opt->addPlan(std::move(plan), rule, !toUnlink.empty());
}

/// @brief useIndex, try to use an index for filtering
void arangodb::aql::useIndexesRule(Optimizer* opt,
                                   std::unique_ptr<ExecutionPlan> plan,
                                   OptimizerRule const* rule) {
  // These are all the nodes where we start traversing (including all
  // subqueries)
  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};
  plan->findEndNodes(nodes, true);

  std::unordered_map<size_t, ExecutionNode*> changes;

  auto cleanupChanges = [&changes]() -> void {
    for (auto& v : changes) {
      delete v.second;
    }
    changes.clear();
  };

  TRI_DEFER(cleanupChanges());
  bool hasEmptyResult = false;
  for (auto const& n : nodes) {
    ConditionFinder finder(plan.get(), &changes, &hasEmptyResult, false);
    n->walk(finder);
  }

  if (!changes.empty()) {
    for (auto& it : changes) {
      plan->registerNode(it.second);
      plan->replaceNode(plan->getNodeById(it.first), it.second);

      // prevent double deletion by cleanupChanges()
      it.second = nullptr;
    }
    opt->addPlan(std::move(plan), rule, true);
  } else {
    opt->addPlan(std::move(plan), rule, hasEmptyResult);
  }
}

struct SortToIndexNode final : public WalkerWorker<ExecutionNode> {
  ExecutionPlan* _plan;
  SortNode* _sortNode;
  std::vector<std::pair<Variable const*, bool>> _sorts;
  std::unordered_map<VariableId, AstNode const*> _variableDefinitions;
  bool _modified;

 public:
  explicit SortToIndexNode(ExecutionPlan* plan)
      : _plan(plan),
        _sortNode(nullptr),
        _sorts(),
        _variableDefinitions(),
        _modified(false) {}

  bool handleEnumerateCollectionNode(
      EnumerateCollectionNode* enumerateCollectionNode) {
    if (_sortNode == nullptr) {
      return true;
    }

    if (enumerateCollectionNode->isInInnerLoop()) {
      // index node contained in an outer loop. must not optimize away the sort!
      return true;
    }

    SortCondition sortCondition(
        _plan, _sorts,
        std::vector<std::vector<arangodb::basics::AttributeName>>(),
        _variableDefinitions);

    if (!sortCondition.isEmpty() && sortCondition.isOnlyAttributeAccess() &&
        sortCondition.isUnidirectional()) {
      // we have found a sort condition, which is unidirectionl
      // now check if any of the collection's indexes covers it

      Variable const* outVariable = enumerateCollectionNode->outVariable();
      std::vector<transaction::Methods::IndexHandle> usedIndexes;
      auto trx = _plan->getAst()->query()->trx();
      size_t coveredAttributes = 0;
      auto resultPair = trx->getIndexForSortCondition(
          enumerateCollectionNode->collection()->name(), &sortCondition,
          outVariable, enumerateCollectionNode->collection()->count(trx),
          usedIndexes, coveredAttributes);
      if (resultPair.second) {
        // If this bit is set, then usedIndexes has length exactly one
        // and contains the best index found.
        auto condition = std::make_unique<Condition>(_plan->getAst());
        condition->normalize(_plan);

        IndexIteratorOptions opts;
        opts.ascending = sortCondition.isAscending();
        auto newNode = std::make_unique<IndexNode>(
            _plan, _plan->nextId(), enumerateCollectionNode->collection(),
            outVariable, usedIndexes, std::move(condition), opts);

        auto n = newNode.release();

        _plan->registerNode(n);
        _plan->replaceNode(enumerateCollectionNode, n);
        _modified = true;

        if (coveredAttributes == sortCondition.numAttributes()) {
          // if the index covers the complete sort condition, we can also remove
          // the sort node
          n->needsGatherNodeSort(true);
          _plan->unlinkNode(_plan->getNodeById(_sortNode->id()));
        }
      }
    }

    return true;  // always abort further searching here
  }

  bool handleIndexNode(IndexNode* indexNode) {
    if (_sortNode == nullptr) {
      return true;
    }

    if (indexNode->isInInnerLoop()) {
      // index node contained in an outer loop. must not optimize away the sort!
      return true;
    }

    auto const& indexes = indexNode->getIndexes();
    auto cond = indexNode->condition();
    TRI_ASSERT(cond != nullptr);

    Variable const* outVariable = indexNode->outVariable();
    TRI_ASSERT(outVariable != nullptr);

    auto index = indexes[0];
    transaction::Methods* trx = _plan->getAst()->query()->trx();
    bool isSorted = false;
    bool isSparse = false;
    std::vector<std::vector<arangodb::basics::AttributeName>> fields =
        trx->getIndexFeatures(index, isSorted, isSparse);
    if (indexes.size() != 1) {
      // can only use this index node if it uses exactly one index or multiple
      // indexes on exactly the same attributes

      if (!cond->isSorted()) {
        // index conditions do not guarantee sortedness
        return true;
      }

      if (isSparse) {
        return true;
      }

      for (auto& idx : indexes) {
        if (idx != index) {
          // Can only be sorted iff only one index is used.
          return true;
        }
      }

      // all indexes use the same attributes and index conditions guarantee
      // sorted output
    }

    TRI_ASSERT(indexes.size() == 1 || cond->isSorted());

    // if we get here, we either have one index or multiple indexes on the same
    // attributes
    bool handled = false;

    if (indexes.size() == 1 && isSorted) {
      // if we have just a single index and we can use it for the filtering
      // condition, then we can use the index for sorting, too. regardless of it
      // the index is sparse or not. because the index would only return
      // non-null attributes anyway, so we do not need to care about null values
      // when sorting here
      isSparse = false;
    }

    SortCondition sortCondition(
        _plan, _sorts, cond->getConstAttributes(outVariable, !isSparse),
        _variableDefinitions);

    bool const isOnlyAttributeAccess =
        (!sortCondition.isEmpty() && sortCondition.isOnlyAttributeAccess());

    if (isOnlyAttributeAccess && isSorted && !isSparse &&
        sortCondition.isUnidirectional() &&
        sortCondition.isAscending() == indexNode->options().ascending) {
      // we have found a sort condition, which is unidirectional and in the same
      // order as the IndexNode...
      // now check if the sort attributes match the ones of the index
      size_t const numCovered =
          sortCondition.coveredAttributes(outVariable, fields);

      if (numCovered >= sortCondition.numAttributes()) {
        // sort condition is fully covered by index... now we can remove the
        // sort node from the plan
        _plan->unlinkNode(_plan->getNodeById(_sortNode->id()));
        // we need to have a sorted result later on, so we will need a sorted
        // GatherNode in the cluster
        indexNode->needsGatherNodeSort(true);
        _modified = true;
        handled = true;
      }
    }

    if (!handled && isOnlyAttributeAccess && indexes.size() == 1) {
      // special case... the index cannot be used for sorting, but we only
      // compare with equality
      // lookups. now check if the equality lookup attributes are the same as
      // the index attributes
      auto root = cond->root();

      if (root != nullptr) {
        auto condNode = root->getMember(0);

        if (condNode->isOnlyEqualityMatch()) {
          // now check if the index fields are the same as the sort condition
          // fields e.g. FILTER c.value1 == 1 && c.value2 == 42 SORT c.value1,
          // c.value2
          auto i = index.getIndex();
          // some special handling for the MMFiles edge index here, which to the
          // outside world is an index on attributes _from and _to at the same
          // time, but only one can be queried at a time this special handling
          // is required in order to prevent lookups by one of the index
          // attributes (e.g. _from) and a sort clause on the other index
          // attribte (e.g. _to) to be treated as the same index attribute, e.g.
          //     FOR doc IN edgeCol FILTER doc._from == ... SORT doc._to ...
          // can use the index either for lookup or for sorting, but not for
          // both at the same time. this is because if we do the lookup by
          // _from, the results will be sorted by _from, and not by _to.
          if (i->type() ==
                  arangodb::Index::IndexType::TRI_IDX_TYPE_EDGE_INDEX &&
              fields.size() == 2) {
            // looks like MMFiles edge index
            if (condNode->type == NODE_TYPE_OPERATOR_NARY_AND) {
              // check all conditions of the index node, and check if we can
              // find _from or _to
              for (size_t j = 0; j < condNode->numMembers(); ++j) {
                auto sub = condNode->getMemberUnchecked(j);
                if (sub->type != NODE_TYPE_OPERATOR_BINARY_EQ) {
                  continue;
                }
                auto lhs = sub->getMember(0);
                if (lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS &&
                    lhs->getMember(0)->type == NODE_TYPE_REFERENCE &&
                    lhs->getMember(0)->getData() == outVariable) {
                  // check if this is either _from or _to
                  std::string attr = lhs->getString();
                  if (attr == StaticStrings::FromString ||
                      attr == StaticStrings::ToString) {
                    // reduce index fields to just the attribute we found in the
                    // index lookup condition
                    fields = {{arangodb::basics::AttributeName(attr, false)}};
                  }
                }

                auto rhs = sub->getMember(1);
                if (rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS &&
                    rhs->getMember(0)->type == NODE_TYPE_REFERENCE &&
                    rhs->getMember(0)->getData() == outVariable) {
                  // check if this is either _from or _to
                  std::string attr = rhs->getString();
                  if (attr == StaticStrings::FromString ||
                      attr == StaticStrings::ToString) {
                    // reduce index fields to just the attribute we found in the
                    // index lookup condition
                    fields = {{arangodb::basics::AttributeName(attr, false)}};
                  }
                }
              }
            }
          }

          size_t const numCovered =
              sortCondition.coveredAttributes(outVariable, fields);

          if (numCovered == sortCondition.numAttributes() &&
              sortCondition.isUnidirectional() &&
              (isSorted || fields.size() >= sortCondition.numAttributes())) {
            // no need to sort
            _plan->unlinkNode(_plan->getNodeById(_sortNode->id()));
            indexNode->setAscending(sortCondition.isAscending());
            // we need to have a sorted result later on, so we will need a
            // sorted GatherNode in the cluster
            indexNode->needsGatherNodeSort(true);
            _modified = true;
          } else if (numCovered > 0 && sortCondition.isUnidirectional()) {
            // remove the first few attributes if they are constant
            SortNode* sortNode = ExecutionNode::castTo<SortNode*>(
                _plan->getNodeById(_sortNode->id()));
            sortNode->removeConditions(numCovered);
            _modified = true;
          }
        }
      }
    }

    return true;  // always abort after we found an IndexNode
  }

  bool enterSubquery(ExecutionNode*, ExecutionNode*) override final {
    return false;
  }

  bool before(ExecutionNode* en) override final {
    switch (en->getType()) {
      case EN::TRAVERSAL:
      case EN::SHORTEST_PATH:
      case EN::ENUMERATE_LIST:
#ifdef USE_IRESEARCH
      case EN::ENUMERATE_IRESEARCH_VIEW:
#endif
        // found some other FOR loop
        return true;

      case EN::SUBQUERY:
      case EN::FILTER:
        return false;  // skip. we don't care.

      case EN::CALCULATION: {
        auto outvars = en->getVariablesSetHere();
        TRI_ASSERT(outvars.size() == 1);

        _variableDefinitions.emplace(
            outvars[0]->id, ExecutionNode::castTo<CalculationNode const*>(en)
                                ->expression()
                                ->node());
        return false;
      }

      case EN::SINGLETON:
      case EN::COLLECT:
      case EN::INSERT:
      case EN::REMOVE:
      case EN::REPLACE:
      case EN::UPDATE:
      case EN::UPSERT:
      case EN::RETURN:
      case EN::NORESULTS:
      case EN::SCATTER:
      case EN::DISTRIBUTE:
      case EN::GATHER:
      case EN::REMOTE:
      case EN::LIMIT:  // LIMIT is criterion to stop
        return true;   // abort.

      case EN::SORT:  // pulling two sorts together is done elsewhere.
        if (!_sorts.empty() || _sortNode != nullptr) {
          return true;  // a different SORT node. abort
        }
        _sortNode = ExecutionNode::castTo<SortNode*>(en);
        for (auto& it : _sortNode->elements()) {
          _sorts.emplace_back(it.var, it.ascending);
        }
        return false;

      case EN::INDEX:
        return handleIndexNode(ExecutionNode::castTo<IndexNode*>(en));

      case EN::ENUMERATE_COLLECTION:
        return handleEnumerateCollectionNode(
            ExecutionNode::castTo<EnumerateCollectionNode*>(en));

      default: {
        // should not reach this point
        TRI_ASSERT(false);
      }
    }
    return true;
  }
};

void arangodb::aql::useIndexForSortRule(Optimizer* opt,
                                        std::unique_ptr<ExecutionPlan> plan,
                                        OptimizerRule const* rule) {
  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, EN::SORT, true);

  bool modified = false;

  for (auto const& n : nodes) {
    auto sortNode = ExecutionNode::castTo<SortNode*>(n);

    SortToIndexNode finder(plan.get());
    sortNode->walk(finder);

    if (finder._modified) {
      modified = true;
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}

/// @brief try to remove filters which are covered by indexes
void arangodb::aql::removeFiltersCoveredByIndexRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const* rule) {
  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, EN::FILTER, true);

  std::unordered_set<ExecutionNode*> toUnlink;
  bool modified = false;
  // this rule may modify the plan in place, but the new plan
  // may not yet be optimal. so we may pass it into this same
  // rule again. the default is to continue with the next rule
  // however
  int newLevel = 0;

  for (auto const& node : nodes) {
    auto fn = ExecutionNode::castTo<FilterNode const*>(node);
    // find the node with the filter expression
    auto inVar = fn->getVariablesUsedHere();
    TRI_ASSERT(inVar.size() == 1);

    auto setter = plan->getVarSetBy(inVar[0]->id);

    if (setter == nullptr || setter->getType() != EN::CALCULATION) {
      continue;
    }

    auto calculationNode = ExecutionNode::castTo<CalculationNode*>(setter);
    auto conditionNode = calculationNode->expression()->node();

    // build the filter condition
    Condition condition(plan->getAst());
    condition.andCombine(conditionNode);
    condition.normalize(plan.get());

    if (condition.root() == nullptr) {
      continue;
    }

    size_t const n = condition.root()->numMembers();

    if (n != 1) {
      // either no condition or multiple ORed conditions...
      continue;
    }

    bool handled = false;
    auto current = node;
    while (current != nullptr) {
      if (current->getType() == EN::INDEX) {
        auto indexNode = ExecutionNode::castTo<IndexNode const*>(current);

        // found an index node, now check if the expression is covered by the
        // index
        auto indexCondition = indexNode->condition();

        if (indexCondition != nullptr && !indexCondition->isEmpty()) {
          auto const& indexesUsed = indexNode->getIndexes();

          if (indexesUsed.size() == 1) {
            // single index. this is something that we can handle
            auto newNode = condition.removeIndexCondition(
                plan.get(), indexNode->outVariable(), indexCondition->root(),
                indexesUsed[0].getIndex()->sparse());

            if (newNode == nullptr) {
              // no condition left...
              // FILTER node can be completely removed
              toUnlink.emplace(node);
              // note: we must leave the calculation node intact, in case it is
              // still used by other nodes in the plan
              modified = true;
              handled = true;
            } else if (newNode != condition.root()) {
              // some condition is left, but it is a different one than
              // the one from the FILTER node
              auto expr = std::make_unique<Expression>(plan.get(),
                                                       plan->getAst(), newNode);
              CalculationNode* cn =
                  new CalculationNode(plan.get(), plan->nextId(), expr.get(),
                                      calculationNode->outVariable());
              expr.release();
              plan->registerNode(cn);
              plan->replaceNode(setter, cn);
              modified = true;
              handled = true;
              // pass the new plan into this rule again, to optimize even
              // further
              newLevel = static_cast<int>(rule->level - 1);
            }
          }
        }

        if (handled) {
          break;
        }
      }

      if (handled || current->getType() == EN::LIMIT ||
          !current->hasDependency()) {
        break;
      }

      current = current->getFirstDependency();
    }
  }

  if (!toUnlink.empty()) {
    plan->unlinkNodes(toUnlink);
  }

  opt->addPlan(std::move(plan), rule, modified, newLevel);
}

/// @brief helper to compute lots of permutation tuples
/// a permutation tuple is represented as a single vector together with
/// another vector describing the boundaries of the tuples.
/// Example:
/// data:   0,1,2, 3,4, 5,6
/// starts: 0,     3,   5,      (indices of starts of sections)
/// means a tuple of 3 permutations of 3, 2 and 2 points respectively
/// This function computes the next permutation tuple among the
/// lexicographically sorted list of all such tuples. It returns true
/// if it has successfully computed this and false if the tuple is already
/// the lexicographically largest one. If false is returned, the permutation
/// tuple is back to the beginning.
static bool NextPermutationTuple(std::vector<size_t>& data,
                                 std::vector<size_t>& starts) {
  auto begin = data.begin();  // a random access iterator

  for (size_t i = starts.size(); i-- != 0;) {
    std::vector<size_t>::iterator from = begin + starts[i];
    std::vector<size_t>::iterator to;
    if (i == starts.size() - 1) {
      to = data.end();
    } else {
      to = begin + starts[i + 1];
    }
    if (std::next_permutation(from, to)) {
      return true;
    }
  }

  return false;
}

/// @brief interchange adjacent EnumerateCollectionNodes in all possible ways
void arangodb::aql::interchangeAdjacentEnumerationsRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const* rule) {
  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};

  plan->findNodesOfType(nodes, ::interchangeAdjacentEnumerationsNodeTypes,
                        true);

  std::unordered_set<ExecutionNode*> nodesSet;
  for (auto const& n : nodes) {
    TRI_ASSERT(nodesSet.find(n) == nodesSet.end());
    nodesSet.emplace(n);
  }

  std::vector<ExecutionNode*> nodesToPermute;
  std::vector<size_t> permTuple;
  std::vector<size_t> starts;
  std::vector<ExecutionNode*> nn;

  // We use that the order of the nodes is such that a node B that is among the
  // recursive dependencies of a node A is later in the vector.
  for (auto const& n : nodes) {
    if (nodesSet.find(n) != nodesSet.end()) {
      nn.clear();
      nn.emplace_back(n);
      nodesSet.erase(n);

      // Now follow the dependencies as long as we see further such nodes:
      auto nwalker = n;

      while (true) {
        if (!nwalker->hasDependency()) {
          break;
        }

        auto dep = nwalker->getFirstDependency();

        if (dep->getType() != EN::ENUMERATE_COLLECTION &&
            dep->getType() != EN::ENUMERATE_LIST) {
          break;
        }

        if (n->getType() == EN::ENUMERATE_LIST &&
            dep->getType() == EN::ENUMERATE_LIST) {
          break;
        }

        nwalker = dep;
        nn.emplace_back(nwalker);
        nodesSet.erase(nwalker);
      }

      if (nn.size() > 1) {
        // Move it into the permutation tuple:
        starts.emplace_back(permTuple.size());

        for (auto const& nnn : nn) {
          nodesToPermute.emplace_back(nnn);
          permTuple.emplace_back(permTuple.size());
        }
      }
    }
  }

  // Now we have collected all the runs of EnumerateCollectionNodes in the
  // plan, we need to compute all possible permutations of all of them,
  // independently. This is why we need to compute all permutation tuples.

  if (!starts.empty()) {
    NextPermutationTuple(permTuple, starts);  // will never return false

    do {
      // check if we already have enough plans (plus the one plan that we will
      // add at the end of this function)
      if (opt->runOnlyRequiredRules(1)) {
        // have enough plans. stop permutations
        break;
      }

      // Clone the plan:
      std::unique_ptr<ExecutionPlan> newPlan(plan->clone());

      // Find the nodes in the new plan corresponding to the ones in the
      // old plan that we want to permute:
      std::vector<ExecutionNode*> newNodes;
      newNodes.reserve(nodesToPermute.size());
      for (size_t j = 0; j < nodesToPermute.size(); j++) {
        newNodes.emplace_back(newPlan->getNodeById(nodesToPermute[j]->id()));
      }

      // Now get going with the permutations:
      for (size_t i = 0; i < starts.size(); i++) {
        size_t lowBound = starts[i];
        size_t highBound =
            (i < starts.size() - 1) ? starts[i + 1] : permTuple.size();
        // We need to remove the nodes
        // newNodes[lowBound..highBound-1] in newPlan and replace
        // them by the same ones in a different order, given by
        // permTuple[lowBound..highBound-1].
        auto parent = newNodes[lowBound]->getFirstParent();

        TRI_ASSERT(parent != nullptr);

        // Unlink all those nodes:
        for (size_t j = lowBound; j < highBound; j++) {
          newPlan->unlinkNode(newNodes[j]);
        }

        // And insert them in the new order:
        for (size_t j = highBound; j-- != lowBound;) {
          newPlan->insertDependency(parent, newNodes[permTuple[j]]);
        }
      }

      // OK, the new plan is ready, let's report it:
      opt->addPlan(std::move(newPlan), rule, true);
    } while (NextPermutationTuple(permTuple, starts));
  }

  opt->addPlan(std::move(plan), rule, false);
}

/// @brief optimize queries in the cluster so that the entire query gets pushed
/// to a single server
void arangodb::aql::optimizeClusterSingleShardRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const* rule) {
  TRI_ASSERT(arangodb::ServerState::instance()->isCoordinator());
  bool wasModified = false;
  bool done = false;

  std::unordered_set<std::string> responsibleServers;
  auto collections = plan->getAst()->query()->collections();

  for (auto const& it : *(collections->collections())) {
    Collection* c = it.second;
    TRI_ASSERT(c != nullptr);

    if (c->numberOfShards() != 1) {
      // more than one shard for this collection
      done = true;
      break;
    }

    size_t n = c->responsibleServers(responsibleServers);

    if (n != 1) {
      // more than one responsible server for this collection
      done = true;
      break;
    }
  }

  if (done || responsibleServers.size() != 1) {
    opt->addPlan(std::move(plan), rule, wasModified);
    return;
  }

  // we only found a single responsible server, and all collections involved
  // have exactly one shard
  // that means we can move the entire query onto that server

  // TODO: handle Traversals and ShortestPaths here!
  // TODO: properly handle subqueries here
  SmallVector<ExecutionNode*>::allocator_type::arena_type s;
  SmallVector<ExecutionNode*> nodes{s};
  std::vector<ExecutionNode::NodeType> types = {ExecutionNode::TRAVERSAL,
                                                ExecutionNode::SHORTEST_PATH,
                                                ExecutionNode::SUBQUERY};
  plan->findNodesOfType(nodes, types, true);

  bool hasIncompatibleNodes = !nodes.empty();

  nodes.clear();
  types = {ExecutionNode::INDEX, ExecutionNode::ENUMERATE_COLLECTION,
           ExecutionNode::TRAVERSAL};
  plan->findNodesOfType(nodes, types, false);

  if (!nodes.empty() && !hasIncompatibleNodes) {
    // turn off all other cluster optimization rules now as they are superfluous
    opt->disableRule(OptimizerRule::optimizeClusterJoinsRule);
    opt->disableRule(OptimizerRule::distributeInClusterRule);
    opt->disableRule(OptimizerRule::scatterInClusterRule);
    opt->disableRule(OptimizerRule::distributeFilternCalcToClusterRule);
    opt->disableRule(OptimizerRule::distributeSortToClusterRule);
    opt->disableRule(OptimizerRule::removeUnnecessaryRemoteScatterRule);
#ifdef USE_ENTERPRISE
    opt->disableRule(OptimizerRule::removeSatelliteJoinsRule);
#endif
    opt->disableRule(OptimizerRule::undistributeRemoveAfterEnumCollRule);

    // get first collection from query
    Collection const* c = ::getCollection(nodes[0]);
    TRI_ASSERT(c != nullptr);

    auto& vocbase = plan->getAst()->query()->vocbase();
    ExecutionNode* rootNode = plan->root();

    // insert a remote node
    ExecutionNode* remoteNode =
        new RemoteNode(plan.get(), plan->nextId(), &vocbase, "", "", "");

    plan->registerNode(remoteNode);
    remoteNode->addDependency(rootNode);

    // insert a gather node
    auto const sortMode = GatherNode::evaluateSortMode(c->numberOfShards());

    auto* gatherNode = new GatherNode(plan.get(), plan->nextId(), sortMode);

    plan->registerNode(gatherNode);
    gatherNode->addDependency(remoteNode);
    plan->root(gatherNode, true);
    wasModified = true;
  }

  opt->addPlan(std::move(plan), rule, wasModified);
}

void arangodb::aql::optimizeClusterJoinsRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const* rule) {
  TRI_ASSERT(arangodb::ServerState::instance()->isCoordinator());
  bool wasModified = false;

  SmallVector<ExecutionNode*>::allocator_type::arena_type s;
  SmallVector<ExecutionNode*> nodes{s};
  std::vector<ExecutionNode::NodeType> const types = {
      ExecutionNode::ENUMERATE_COLLECTION, ExecutionNode::INDEX};
  plan->findNodesOfType(nodes, types, true);

  for (auto& n : nodes) {
    ExecutionNode* current = n->getFirstDependency();
    while (current != nullptr) {
      if (current->getType() == ExecutionNode::ENUMERATE_COLLECTION ||
          current->getType() == ExecutionNode::INDEX) {
        Collection const* c1 = ::getCollection(n);
        Collection const* c2 = ::getCollection(current);

        bool qualifies = false;

        // check how many (different) responsible servers we have for this
        // collection
        std::unordered_set<std::string> responsibleServers;
        size_t n1 = c1->responsibleServers(responsibleServers);
        size_t n2 = c2->responsibleServers(responsibleServers);

        if (responsibleServers.size() == 1 && c1->numberOfShards() == 1 &&
            c2->numberOfShards() == 1) {
          // a single responsible server. so we can use a shard-local access
          qualifies = true;
        } else if ((c1->isSatellite() &&
                    (c2->numberOfShards() == 1 || n2 == 1)) ||
                   (c2->isSatellite() &&
                    (c1->numberOfShards() == 1 || n1 == 1))) {
          // a satellite collection and another collection with a single shard
          // or single responsible server
          qualifies = true;
        }

        if (!qualifies && n->getType() == EN::INDEX) {
          Variable const* indexVariable = ::getOutVariable(n);
          Variable const* otherVariable = ::getOutVariable(current);

          std::string dist1 = c1->distributeShardsLike();
          std::string dist2 = c2->distributeShardsLike();

          // convert cluster collection names into proper collection names
          if (!dist1.empty()) {
            auto trx = plan->getAst()->query()->trx();
            dist1 = trx->resolver()->getCollectionNameCluster(
                static_cast<TRI_voc_cid_t>(basics::StringUtils::uint64(dist1)));
          }
          if (!dist2.empty()) {
            auto trx = plan->getAst()->query()->trx();
            dist2 = trx->resolver()->getCollectionNameCluster(
                static_cast<TRI_voc_cid_t>(basics::StringUtils::uint64(dist2)));
          }

          if (dist1 == c2->name() || dist2 == c1->name() ||
              (!dist1.empty() && dist1 == dist2)) {
            // collections have the same "distributeShardsLike" values
            // so their shards are distributed to the same servers for the
            // same shardKey values
            // now check if the number of shardKeys match
            auto keys1 = c1->shardKeys();
            auto keys2 = c2->shardKeys();

            if (keys1.size() == keys2.size()) {
              // same number of shard keys... now check if the shard keys are
              // all used and whether we only have equality joins
              Condition const* condition =
                  ExecutionNode::castTo<IndexNode const*>(n)->condition();

              if (condition != nullptr) {
                AstNode const* root = condition->root();

                if (root != nullptr &&
                    root->type == NODE_TYPE_OPERATOR_NARY_OR) {
                  size_t found = 0;
                  size_t numAnds = root->numMembers();

                  for (size_t i = 0; i < numAnds; ++i) {
                    AstNode const* andNode = root->getMember(i);

                    if (andNode == nullptr) {
                      continue;
                    }

                    TRI_ASSERT(andNode->type == NODE_TYPE_OPERATOR_NARY_AND);

                    std::unordered_set<int> shardKeysFound;
                    size_t numConds = andNode->numMembers();

                    if (numConds < keys1.size()) {
                      // too few join conditions, so we will definitely not
                      // cover all shardKeys
                      break;
                    }

                    for (size_t j = 0; j < numConds; ++j) {
                      AstNode const* condNode = andNode->getMember(j);

                      if (condNode == nullptr ||
                          condNode->type != NODE_TYPE_OPERATOR_BINARY_EQ) {
                        // something other than an equality join. we do not
                        // support this
                        continue;
                      }

                      // equality comparison
                      // now check if this comparison has the pattern
                      // <variable from collection1>.<attribute from
                      // collection1> ==
                      //   <variable from collection2>.<attribute from
                      //   collection2>

                      auto const* lhs = condNode->getMember(0);
                      auto const* rhs = condNode->getMember(1);

                      if (lhs->type != NODE_TYPE_ATTRIBUTE_ACCESS ||
                          rhs->type != NODE_TYPE_ATTRIBUTE_ACCESS) {
                        // something else
                        continue;
                      }

                      AstNode const* lhsData = lhs->getMember(0);
                      AstNode const* rhsData = rhs->getMember(0);

                      if (lhsData->type != NODE_TYPE_REFERENCE ||
                          rhsData->type != NODE_TYPE_REFERENCE) {
                        // something else
                        continue;
                      }

                      Variable const* lhsVar =
                          static_cast<Variable const*>(lhsData->getData());
                      Variable const* rhsVar =
                          static_cast<Variable const*>(rhsData->getData());

                      std::string leftString = lhs->getString();
                      std::string rightString = rhs->getString();

                      int pos = -1;
                      if (lhsVar == indexVariable && rhsVar == otherVariable &&
                          indexOf(keys1, leftString) ==
                              indexOf(keys2, rightString)) {
                        pos = indexOf(keys1, leftString);
                        // indexedCollection.shardKeyAttribute ==
                        // otherCollection.shardKeyAttribute
                      } else if (lhsVar == otherVariable &&
                                 rhsVar == indexVariable &&
                                 indexOf(keys2, leftString) ==
                                     indexOf(keys1, rightString)) {
                        // otherCollection.shardKeyAttribute ==
                        // indexedCollection.shardKeyAttribute
                        pos = indexOf(keys2, leftString);
                      }

                      // we found a shardKeys match
                      if (pos != -1) {
                        shardKeysFound.emplace(pos);
                      }
                    }

                    // conditions match
                    if (shardKeysFound.size() >= keys1.size()) {
                      // all shard keys covered
                      ++found;
                    } else {
                      // not all shard keys covered
                      break;
                    }
                  }

                  qualifies = (found > 0 && found == numAnds);
                }
              }
            }
          }
        }
        // everything else does not qualify

        if (qualifies) {
          wasModified = true;

          plan->excludeFromScatterGather(current);
          break;  // done for this pair
        }

      } else if (current->getType() != ExecutionNode::FILTER &&
                 current->getType() != ExecutionNode::CALCULATION &&
                 current->getType() != ExecutionNode::LIMIT) {
        // we allow just these nodes in between and ignore them
        // we need to stop for all other types of nodes
        break;
      }

      current = current->getFirstDependency();
    }
  }

  opt->addPlan(std::move(plan), rule, wasModified);
}

/// @brief scatter operations in cluster
/// this rule inserts scatter, gather and remote nodes so operations on sharded
/// collections actually work
/// it will change plans in place
void arangodb::aql::scatterInClusterRule(Optimizer* opt,
                                         std::unique_ptr<ExecutionPlan> plan,
                                         OptimizerRule const* rule) {
  TRI_ASSERT(arangodb::ServerState::instance()->isCoordinator());
  bool wasModified = false;

  // find subqueries
  std::unordered_map<ExecutionNode*, ExecutionNode*> subqueries;

  SmallVector<ExecutionNode*>::allocator_type::arena_type s;
  SmallVector<ExecutionNode*> subs{s};
  plan->findNodesOfType(subs, ExecutionNode::SUBQUERY, true);

  for (auto& it : subs) {
    subqueries.emplace(
        ExecutionNode::castTo<SubqueryNode const*>(it)->getSubquery(), it);
  }

  // we are a coordinator. now look in the plan for nodes of type
  // EnumerateCollectionNode, IndexNode and modification nodes
  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, ::scatterInClusterNodeTypes, true);

  TRI_ASSERT(plan->getAst() && plan->getAst()->query() &&
             plan->getAst()->query()->trx());
  auto* resolver = plan->getAst()->query()->trx()->resolver();
  TRI_ASSERT(resolver);

  for (auto& node : nodes) {
    // found a node we need to replace in the plan

    auto const& parents = node->getParents();
    // intentional copy of the dependencies, as we will be modifying 
    // dependencies later on
    auto const deps = node->getDependencies();
    TRI_ASSERT(deps.size() == 1);

    // don't do this if we are already distributing!
    if (deps[0]->getType() == ExecutionNode::REMOTE &&
        deps[0]->getFirstDependency()->getType() == ExecutionNode::DISTRIBUTE) {
      continue;
    }

    if (plan->shouldExcludeFromScatterGather(node)) {
      continue;
    }

    bool const isRootNode = plan->isRoot(node);
    plan->unlinkNode(node, true);

    auto const nodeType = node->getType();

    // extract database and collection from plan node
    TRI_vocbase_t* vocbase = nullptr;
    Collection const* collection = nullptr;

    SortElementVector elements;

    if (nodeType == ExecutionNode::ENUMERATE_COLLECTION) {
      vocbase = ExecutionNode::castTo<EnumerateCollectionNode const*>(node)
                    ->vocbase();
      collection = ExecutionNode::castTo<EnumerateCollectionNode const*>(node)
                       ->collection();
    } else if (nodeType == ExecutionNode::INDEX) {
      auto idxNode = ExecutionNode::castTo<IndexNode const*>(node);
      vocbase = idxNode->vocbase();
      collection = idxNode->collection();
      TRI_ASSERT(collection != nullptr);
      auto outVars = idxNode->getVariablesSetHere();
      TRI_ASSERT(outVars.size() == 1);
      Variable const* sortVariable = outVars[0];
      bool isSortAscending = idxNode->options().ascending;
      auto allIndexes = idxNode->getIndexes();
      TRI_ASSERT(!allIndexes.empty());

      // Using Index for sort only works if all indexes are equal.
      auto first = allIndexes[0].getIndex();
      // also check if we actually need to bother about the sortedness of the
      // result, or if we use the index for filtering only
      if (first->isSorted() && idxNode->needsGatherNodeSort()) {
        for (auto const& path : first->fieldNames()) {
          elements.emplace_back(sortVariable, isSortAscending, path);
        }
        for (auto const& it : allIndexes) {
          if (first != it.getIndex()) {
            elements.clear();
            break;
          }
        }
      }
    } else if (nodeType == ExecutionNode::INSERT ||
               nodeType == ExecutionNode::UPDATE ||
               nodeType == ExecutionNode::REPLACE ||
               nodeType == ExecutionNode::REMOVE ||
               nodeType == ExecutionNode::UPSERT) {
      vocbase = ExecutionNode::castTo<ModificationNode*>(node)->vocbase();
      collection = ExecutionNode::castTo<ModificationNode*>(node)->collection();
      if (nodeType == ExecutionNode::REMOVE ||
          nodeType == ExecutionNode::UPDATE) {
        // Note that in the REPLACE or UPSERT case we are not getting here,
        // since the distributeInClusterRule fires and a DistributionNode is
        // used.
        auto* modNode = ExecutionNode::castTo<ModificationNode*>(node);
        modNode->getOptions().ignoreDocumentNotFound = true;
      }
    } else {
      TRI_ASSERT(false);
    }

    // insert a scatter node
    auto* scatterNode = new ScatterNode(plan.get(), plan->nextId());
    plan->registerNode(scatterNode);
    TRI_ASSERT(!deps.empty());
    scatterNode->addDependency(deps[0]);

    // insert a remote node
    ExecutionNode* remoteNode =
        new RemoteNode(plan.get(), plan->nextId(), vocbase, "", "", "");
    plan->registerNode(remoteNode);
    TRI_ASSERT(scatterNode);
    remoteNode->addDependency(scatterNode);

    // re-link with the remote node
    node->addDependency(remoteNode);

    // insert another remote node
    remoteNode =
        new RemoteNode(plan.get(), plan->nextId(), vocbase, "", "", "");
    plan->registerNode(remoteNode);
    TRI_ASSERT(node);
    remoteNode->addDependency(node);

    // insert a gather node
    auto const sortMode =
        GatherNode::evaluateSortMode(collection->numberOfShards());
    auto* gatherNode = new GatherNode(plan.get(), plan->nextId(), sortMode);
    plan->registerNode(gatherNode);
    TRI_ASSERT(remoteNode);
    gatherNode->addDependency(remoteNode);
    // On SmartEdge collections we have 0 shards and we need the elements
    // to be injected here as well. So do not replace it with > 1
    if (!elements.empty() && collection->numberOfShards() != 1) {
      gatherNode->elements(elements);
    }

    // and now link the gather node with the rest of the plan
    if (parents.size() == 1) {
      parents[0]->replaceDependency(deps[0], gatherNode);
    }

    // check if the node that we modified was at the end of a subquery
    auto it = subqueries.find(node);

    if (it != subqueries.end()) {
      ExecutionNode::castTo<SubqueryNode*>((*it).second)
          ->setSubquery(gatherNode, true);
    }

    if (isRootNode) {
      // if we replaced the root node, set a new root node
      plan->root(gatherNode);
    }
    wasModified = true;
  }

  opt->addPlan(std::move(plan), rule, wasModified);
}

/// @brief distribute operations in cluster
///
/// this rule inserts distribute, remote nodes so operations on sharded
/// collections actually work, this differs from scatterInCluster in that every
/// incoming row is only sent to one shard and not all as in scatterInCluster
///
/// it will change plans in place
void arangodb::aql::distributeInClusterRule(Optimizer* opt,
                                            std::unique_ptr<ExecutionPlan> plan,
                                            OptimizerRule const* rule) {
  TRI_ASSERT(arangodb::ServerState::instance()->isCoordinator());
  bool wasModified = false;
  // we are a coordinator, we replace the root if it is a modification node

  // only replace if it is the last node in the plan
  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> subqueryNodes{a};
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

    while (node != nullptr) {
      auto nodeType = node->getType();

      // loop until we find a modification node or the end of the plan
      while (node != nullptr) {
        // update type
        nodeType = node->getType();

        // check if there is a node type that needs distribution
        if (nodeType == ExecutionNode::INSERT ||
            nodeType == ExecutionNode::REMOVE ||
            nodeType == ExecutionNode::UPDATE ||
            nodeType == ExecutionNode::REPLACE ||
            nodeType == ExecutionNode::UPSERT) {
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
        // break loop for subqyery
        break;
      }

      TRI_ASSERT(node != nullptr);
      if (node == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "logic error");
      }

      // when we get here, we have found a matching data-modification node!
      TRI_ASSERT(nodeType == ExecutionNode::INSERT ||
                 nodeType == ExecutionNode::REMOVE ||
                 nodeType == ExecutionNode::UPDATE ||
                 nodeType == ExecutionNode::REPLACE ||
                 nodeType == ExecutionNode::UPSERT);

      ExecutionNode* originalParent = nullptr;
      if (node->hasParent()) {
        auto const& parents = node->getParents();
        originalParent = parents[0];
        TRI_ASSERT(originalParent != nullptr);
        TRI_ASSERT(node != root);
      } else {
        TRI_ASSERT(node == root);
      }

      Collection const* collection =
          ExecutionNode::castTo<ModificationNode*>(node)->collection();

#ifdef USE_ENTERPRISE
      auto ci = ClusterInfo::instance();
      auto collInfo =
          ci->getCollection(collection->vocbase()->name(), collection->name());
      // Throws if collection is not found!
      if (collInfo->isSmart() && collInfo->type() == TRI_COL_TYPE_EDGE) {
        node = distributeInClusterRuleSmartEdgeCollection(
            plan.get(), snode, node, originalParent, wasModified);
        continue;
      }
#endif
      bool const defaultSharding = collection->usesDefaultSharding();

      if (nodeType == ExecutionNode::REMOVE ||
          nodeType == ExecutionNode::UPDATE) {
        if (!defaultSharding) {
          // We have to use a ScatterNode.
          node = node->getFirstDependency();  // advance node
          continue;
        }
      }

      // In the INSERT and REPLACE cases we use a DistributeNode...

      TRI_ASSERT(node->hasDependency());
      // intentional copy of the dependencies, as we will be modifying 
      // dependencies later on
      auto const deps = node->getDependencies();

      bool haveAdjusted = false;
      if (originalParent != nullptr) {
        // nodes below removed node
        originalParent->removeDependency(node);
        plan->unlinkNode(node, true);
        if (snode) {
          if (snode->getSubquery() == node) {
            snode->setSubquery(originalParent, true);
            haveAdjusted = true;
          }
        }
      } else {
        // no nodes below unlinked node
        plan->unlinkNode(node, true);
        if (snode) {
          snode->setSubquery(deps[0], true);
          haveAdjusted = true;
        } else {
          plan->root(deps[0], true);
        }
      }

      // extract database from plan node
      TRI_vocbase_t* vocbase =
          ExecutionNode::castTo<ModificationNode*>(node)->vocbase();

      // insert a distribute node
      ExecutionNode* distNode = nullptr;
      Variable const* inputVariable;
      if (nodeType == ExecutionNode::INSERT ||
          nodeType == ExecutionNode::REMOVE) {
        TRI_ASSERT(node->getVariablesUsedHere().size() == 1);

        // in case of an INSERT, the DistributeNode is responsible for
        // generating keys if none present
        bool const createKeys = (nodeType == ExecutionNode::INSERT);
        inputVariable = node->getVariablesUsedHere()[0];
        distNode =
            new DistributeNode(plan.get(), plan->nextId(), collection,
                               inputVariable, inputVariable, createKeys, true);
      } else if (nodeType == ExecutionNode::REPLACE) {
        std::vector<Variable const*> v = node->getVariablesUsedHere();
        if (defaultSharding && v.size() > 1) {
          // We only look into _inKeyVariable
          inputVariable = v[1];
        } else {
          // We only look into _inDocVariable
          inputVariable = v[0];
        }
        distNode = new DistributeNode(plan.get(), plan->nextId(), collection,
                                      inputVariable, inputVariable, false,
                                      v.size() > 1);
      } else if (nodeType == ExecutionNode::UPDATE) {
        std::vector<Variable const*> v = node->getVariablesUsedHere();
        if (v.size() > 1) {
          // If there is a key variable:
          inputVariable = v[1];
          // This is the _inKeyVariable! This works, since we use a ScatterNode
          // for non-default-sharding attributes.
        } else {
          // was only UPDATE <doc> IN <collection>
          inputVariable = v[0];
        }
        distNode = new DistributeNode(plan.get(), plan->nextId(), collection,
                                      inputVariable, inputVariable, false,
                                      v.size() > 1);
      } else if (nodeType == ExecutionNode::UPSERT) {
        // an UPSERT node has two input variables!
        std::vector<Variable const*> v(node->getVariablesUsedHere());
        TRI_ASSERT(v.size() >= 2);

        auto d = new DistributeNode(plan.get(), plan->nextId(), collection,
                                    v[0], v[1], true, true);
        d->setAllowSpecifiedKeys(true);
        distNode = ExecutionNode::castTo<ExecutionNode*>(d);
      } else {
        TRI_ASSERT(false);
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "logic error");
      }

      TRI_ASSERT(distNode != nullptr);

      plan->registerNode(distNode);
      distNode->addDependency(deps[0]);

      // insert a remote node
      ExecutionNode* remoteNode =
          new RemoteNode(plan.get(), plan->nextId(), vocbase, "", "", "");
      plan->registerNode(remoteNode);
      remoteNode->addDependency(distNode);

      // re-link with the remote node
      node->addDependency(remoteNode);

      // insert another remote node
      remoteNode =
          new RemoteNode(plan.get(), plan->nextId(), vocbase, "", "", "");
      plan->registerNode(remoteNode);
      remoteNode->addDependency(node);

      // insert a gather node
      auto const sortMode =
          GatherNode::evaluateSortMode(collection->numberOfShards());
      auto* gatherNode = new GatherNode(plan.get(), plan->nextId(), sortMode);
      plan->registerNode(gatherNode);
      gatherNode->addDependency(remoteNode);

      if (originalParent != nullptr) {
        // we did not replace the root node
        TRI_ASSERT(gatherNode);
        originalParent->addDependency(gatherNode);
      } else {
        // we replaced the root node, set a new root node
        if (snode) {
          if (snode->getSubquery() == node || haveAdjusted) {
            snode->setSubquery(gatherNode, true);
          }
        } else {
          plan->root(gatherNode, true);
        }
      }
      wasModified = true;
      node = distNode;  // will be gatherNode or nulltpr
    }                   // for node in subquery
  }                     // for end subquery in plan
  opt->addPlan(std::move(plan), rule, wasModified);
}

void arangodb::aql::collectInClusterRule(Optimizer* opt,
                                         std::unique_ptr<ExecutionPlan> plan,
                                         OptimizerRule const* rule) {
  TRI_ASSERT(arangodb::ServerState::instance()->isCoordinator());
  bool wasModified = false;

  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, EN::COLLECT, true);

  std::unordered_set<Variable const*> allUsed;

  for (auto& node : nodes) {
    allUsed.clear();
    auto used = node->getVariablesUsedHere();

    // found a node we need to replace in the plan
    TRI_ASSERT(node->getDependencies().size() == 1);

    auto collectNode = ExecutionNode::castTo<CollectNode*>(node);
    // look for next remote node
    GatherNode* gatherNode = nullptr;
    auto current = node->getFirstDependency();

    while (current != nullptr) {
      bool eligible = true;

      // check if any of the nodes we pass use a variable that will not be
      // available after we insert a new COLLECT on top of it (note: COLLECT
      // will eliminate all variables from the scope but its own)
      for (auto const& it : current->getVariablesUsedHere()) {
        if (current->getType() != EN::GATHER) {
          // Gather nodes are taken care of separately below
          allUsed.emplace(it);
        }
      }

      // check if any of the nodes we pass use a variable that will not be
      // available after we insert a new COLLECT on top of it (note: COLLECT
      // will eliminate all variables from the scope but its own)
      for (auto const& it : current->getVariablesUsedHere()) {
        if (current->getType() != EN::GATHER) {
          // Gather nodes are taken care of separately below
          allUsed.emplace(it);
        }
      }

      for (auto const& it : current->getVariablesSetHere()) {
        if (std::find(used.begin(), used.end(), it) != used.end()) {
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

        // we may have moved another CollectNode here already. if so, we need to
        // move the new CollectNode to the front of multiple CollectNodes
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
              CollectOptions::CollectMethod::COUNT) {
            // clone a COLLECT WITH COUNT operation from the coordinator to the
            // DB server(s), and leave an aggregate COLLECT node on the
            // coordinator for total aggregation

            // add a new CollectNode on the DB server to do the actual counting
            auto outVariable =
                plan->getAst()->variables()->createTemporaryVariable();
            auto dbCollectNode = new CollectNode(
                plan.get(), plan->nextId(), collectNode->getOptions(),
                collectNode->groupVariables(),
                collectNode->aggregateVariables(), nullptr, outVariable,
                std::vector<Variable const*>(), collectNode->variableMap(),
                true, false);

            plan->registerNode(dbCollectNode);

            dbCollectNode->addDependency(previous);
            target->replaceDependency(previous, dbCollectNode);

            dbCollectNode->aggregationMethod(collectNode->aggregationMethod());
            dbCollectNode->specialized();

            // re-use the existing CollectNode on the coordinator to aggregate
            // the counts of the DB servers
            std::vector<std::pair<Variable const*,
                                  std::pair<Variable const*, std::string>>>
                aggregateVariables;
            aggregateVariables.emplace_back(
                std::make_pair(collectNode->outVariable(),
                               std::make_pair(outVariable, "SUM")));

            collectNode->aggregationMethod(
                CollectOptions::CollectMethod::SORTED);
            collectNode->count(false);
            collectNode->setAggregateVariables(aggregateVariables);
            collectNode->clearOutVariable();

            removeGatherNodeSort = true;
          } else if (collectNode->aggregationMethod() ==
                     CollectOptions::CollectMethod::DISTINCT) {
            // clone a COLLECT DISTINCT operation from the coordinator to the DB
            // server(s), and leave an aggregate COLLECT node on the coordinator
            // for total aggregation

            // create a new result variable
            auto const& groupVars = collectNode->groupVariables();
            TRI_ASSERT(!groupVars.empty());
            auto out = plan->getAst()->variables()->createTemporaryVariable();

            std::vector<std::pair<Variable const*, Variable const*>> const
                groupVariables{std::make_pair(out, groupVars[0].second)};

            auto dbCollectNode = new CollectNode(
                plan.get(), plan->nextId(), collectNode->getOptions(),
                groupVariables, collectNode->aggregateVariables(), nullptr,
                nullptr, std::vector<Variable const*>(),
                collectNode->variableMap(), false, true);

            plan->registerNode(dbCollectNode);

            dbCollectNode->addDependency(previous);
            target->replaceDependency(previous, dbCollectNode);

            dbCollectNode->aggregationMethod(collectNode->aggregationMethod());
            dbCollectNode->specialized();

            // will set the input of the coordinator's collect node to the new
            // variable produced on the DB servers
            auto copy = collectNode->groupVariables();
            TRI_ASSERT(!copy.empty());
            copy[0].second = out;
            collectNode->groupVariables(copy);

            removeGatherNodeSort = true;
          } else if (  //! collectNode->groupVariables().empty() &&
              (!collectNode->hasOutVariable() || collectNode->count())) {
            // clone a COLLECT v1 = expr, v2 = expr ... operation from the
            // coordinator to the DB server(s), and leave an aggregate COLLECT
            // node on the coordinator for total aggregation

            std::vector<std::pair<Variable const*,
                                  std::pair<Variable const*, std::string>>>
                aggregateVariables;
            if (!collectNode->aggregateVariables().empty()) {
              for (auto const& it : collectNode->aggregateVariables()) {
                std::string func =
                    Aggregator::pushToDBServerAs(it.second.second);
                if (func.empty()) {
                  eligible = false;
                  break;
                }
                // eligible!
                auto outVariable =
                    plan->getAst()->variables()->createTemporaryVariable();
                aggregateVariables.emplace_back(std::make_pair(
                    outVariable, std::make_pair(it.second.first, func)));
              }
            }

            if (!eligible) {
              break;
            }

            Variable const* outVariable = nullptr;
            if (collectNode->count()) {
              outVariable =
                  plan->getAst()->variables()->createTemporaryVariable();
            }

            // create new group variables
            auto const& groupVars = collectNode->groupVariables();
            std::vector<std::pair<Variable const*, Variable const*>> outVars;
            outVars.reserve(groupVars.size());
            std::unordered_map<Variable const*, Variable const*> replacements;

            for (auto const& it : groupVars) {
              // create new out variables
              auto out = plan->getAst()->variables()->createTemporaryVariable();
              replacements.emplace(it.second, out);
              outVars.emplace_back(out, it.second);
            }

            auto dbCollectNode = new CollectNode(
                plan.get(), plan->nextId(), collectNode->getOptions(), outVars,
                aggregateVariables, nullptr, outVariable,
                std::vector<Variable const*>(), collectNode->variableMap(),
                collectNode->count(), false);

            plan->registerNode(dbCollectNode);

            dbCollectNode->addDependency(previous);
            target->replaceDependency(previous, dbCollectNode);

            dbCollectNode->aggregationMethod(collectNode->aggregationMethod());
            dbCollectNode->specialized();

            std::vector<std::pair<Variable const*, Variable const*>> copy;
            size_t i = 0;
            for (auto const& it : collectNode->groupVariables()) {
              // replace input variables
              copy.emplace_back(std::make_pair(it.first, outVars[i].first));
              ++i;
            }
            collectNode->groupVariables(copy);

            if (collectNode->count()) {
              std::vector<std::pair<Variable const*,
                                    std::pair<Variable const*, std::string>>>
                  aggregateVariables;
              aggregateVariables.emplace_back(
                  std::make_pair(collectNode->outVariable(),
                                 std::make_pair(outVariable, "SUM")));

              collectNode->count(false);
              collectNode->setAggregateVariables(aggregateVariables);
              collectNode->clearOutVariable();
            } else {
              size_t i = 0;
              for (auto& it : collectNode->aggregateVariables()) {
                it.second.first = aggregateVariables[i].first;
                it.second.second =
                    Aggregator::runOnCoordinatorAs(it.second.second);
                ++i;
              }
            }

            removeGatherNodeSort = (dbCollectNode->aggregationMethod() !=
                                    CollectOptions::CollectMethod::SORTED);

            // in case we need to keep the sortedness of the GatherNode,
            // we may need to replace some variable references in it due
            // to the changes we made to the COLLECT node
            if (gatherNode != nullptr) {
              SortElementVector& elements = gatherNode->elements();
              if (!removeGatherNodeSort && !replacements.empty() &&
                  !elements.empty()) {
                std::string cmp;
                std::string other;
                basics::StringBuffer buffer(128, false);

                // look for all sort elements in the GatherNode and replace them
                // if they match what we have changed
                for (auto& it : elements) {
                  // replace variables
                  auto it2 = replacements.find(it.var);

                  if (it2 != replacements.end()) {
                    // match with our replacement table
                    it.var = (*it2).second;
                    it.attributePath.clear();
                  } else {
                    // no match. now check all our replacements and compare how
                    // their sources are actually calculated (e.g. #2 may mean
                    // "foo.bar")
                    cmp = it.toString();
                    for (auto const& it3 : replacements) {
                      auto setter = plan->getVarSetBy(it3.first->id);
                      if (setter == nullptr ||
                          setter->getType() != EN::CALCULATION) {
                        continue;
                      }
                      auto* expr =
                          ExecutionNode::castTo<CalculationNode const*>(setter)
                              ->expression();
                      if (expr == nullptr) {
                        continue;
                      }
                      other.clear();
                      try {
                        buffer.clear();
                        expr->stringify(&buffer);
                        other = std::string(buffer.c_str(), buffer.size());
                      } catch (...) {
                      }
                      if (other == cmp) {
                        // finally a match!
                        it.var = it3.second;
                        it.attributePath.clear();
                        break;
                      }
                    }
                  }
                }
              }
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
void arangodb::aql::distributeFilternCalcToClusterRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const* rule) {
  bool modified = false;

  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, EN::GATHER, true);
    
  std::unordered_set<Variable const*> varsSetHere;

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
      if (allowOnlyFilterAndCalculation &&
          type != EN::FILTER && 
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
        case EN::TRAVERSAL:
        case EN::SHORTEST_PATH:
        case EN::SUBQUERY:
#ifdef USE_IRESEARCH
        case EN::ENUMERATE_IRESEARCH_VIEW:
#endif
          // do break
          stopSearching = true;
          break;

        case EN::CALCULATION:
        case EN::FILTER:
          if (inspectNode->getType() == EN::CALCULATION) {
            // check if the expression can be executed on a DB server safely
            if (!ExecutionNode::castTo<CalculationNode const*>(inspectNode)
                     ->expression()
                     ->canRunOnDBServer()) {
              stopSearching = true;
              break;
            }
            // intentionally falls through
          }
#if 0
          // TODO: this is already prepared to push subqueries on the DB servers.
          // However, the ExecutionEngine's instanciator cannot yet handle subqueries
          // on DB servers. Once it can do this, this part can be finished
          else if (inspectNode->getType() == EN::SUBQUERY) {
            // check if the subquery can be executed on a DB server safely
            SubqueryNode* s = ExecutionNode::castTo<SubqueryNode*>(inspectNode);
            if (!s->isDeterministic() || s->mayAccessCollections()) {
              stopSearching = true;
              break;
            }
            // intentionally falls through
          }
#endif
          // no special handling for filters here

          TRI_ASSERT(inspectNode->getType() == EN::SUBQUERY ||
                     inspectNode->getType() == EN::CALCULATION ||
                     inspectNode->getType() == EN::FILTER);

          for (auto& v : inspectNode->getVariablesUsedHere()) {
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

        default: {
          // should not reach this point
          TRI_ASSERT(false);
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
    OptimizerRule const* rule) {
  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};
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
        case EN::COLLECT:
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
        case EN::TRAVERSAL:
        case EN::SHORTEST_PATH:
        case EN::REMOTESINGLE:
#ifdef USE_IRESEARCH
        case EN::ENUMERATE_IRESEARCH_VIEW:
#endif

          // For all these, we do not want to pull a SortNode further down
          // out to the DBservers, note that potential FilterNodes and
          // CalculationNodes that can be moved to the DBservers have
          // already been moved over by the distribute-filtercalc-to-cluster
          // rule which is done first.
          stopSearching = true;
          break;

        case EN::SORT: {
          auto thisSortNode = ExecutionNode::castTo<SortNode*>(inspectNode);

          // remember our cursor...
          parents = inspectNode->getParents();
          // then unlink the filter/calculator from the plan
          plan->unlinkNode(inspectNode);
          // and re-insert into plan in front of the remoteNode
          if (thisSortNode->_reinsertInCluster) {
            plan->insertDependency(rn, inspectNode);
          }

          auto const* collection = GatherNode::findCollection(*gatherNode);

          // For views (when 'collection == nullptr') we don't need
          // to check number of shards
          // On SmartEdge collections we have 0 shards and we need the elements
          // to be injected here as well. So do not replace it with > 1
          if (!collection || collection->numberOfShards() != 1) {
            gatherNode->elements(thisSortNode->elements());
          }

          modified = true;
          // ready to rumble!
          break;
        }

        case EN::MAX_NODE_TYPE_VALUE: {
          // should not reach this point
          TRI_ASSERT(false);
          stopSearching = true;
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
    OptimizerRule const* rule) {
  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, EN::REMOTE, true);

  std::unordered_set<ExecutionNode*> toUnlink;

  for (auto& n : nodes) {
    // check if the remote node is preceeded by a scatter node and any number of
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
          if (!calc->expression()->canRunOnDBServer()) {
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
    OptimizerRule const* rule) {
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

  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, EN::REMOTE, true);

  std::unordered_set<ExecutionNode*> toUnlink;
  std::map<Collection const*, std::unordered_set<std::string>>
      modificationRestrictions;

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
        std::string shardId =
            ::getSingleShardId(plan.get(), current, collection);

        if (!shardId.empty()) {
          wasModified = true;
          // we are on a single shard. we must not ignore not-found documents
          // now
          auto* modNode = ExecutionNode::castTo<ModificationNode*>(current);
          modNode->getOptions().ignoreDocumentNotFound = false;
          modNode->restrictToShard(shardId);
          modificationRestrictions[collection].emplace(shardId);

          auto const& deps = current->getDependencies();
          if (deps.size() && deps[0]->getType() == ExecutionNode::REMOTE) {
            // if we can apply the single-shard optimization, but still have a
            // REMOTE node in front of us, we can probably move the remote parts
            // of the query to our side. this is only the case if the remote
            // part does not call any remote parts itself
            std::unordered_set<ExecutionNode*> toRemove;

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
                auto cn = ExecutionNode::castTo<CalculationNode const*>(c);
                auto expr = cn->expression();
                if (expr != nullptr && !expr->canRunOnDBServer()) {
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
        auto collection = ::getCollection(current);
        auto collectionVariable = ::getOutVariable(current);
        std::string shardId = finder.getShard(collectionVariable);

        if (finder.isSafeForOptimization(collectionVariable) &&
            !shardId.empty()) {
          wasModified = true;
          ::restrictToShard(current, shardId);
        } else if (finder.isSafeForOptimization(collection)) {
          auto& shards = modificationRestrictions[collection];
          if (shards.size() == 1) {
            wasModified = true;
            shardId = *shards.begin();
            ::restrictToShard(current, shardId);
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
class RemoveToEnumCollFinder final : public WalkerWorker<ExecutionNode> {
  ExecutionPlan* _plan;
  std::unordered_set<ExecutionNode*>& _toUnlink;
  bool _remove;
  bool _scatter;
  bool _gather;
  ExecutionNode* _enumColl;
  ExecutionNode* _setter;
  const Variable* _variable;
  ExecutionNode* _lastNode;

 public:
  RemoveToEnumCollFinder(ExecutionPlan* plan,
                         std::unordered_set<ExecutionNode*>& toUnlink)
      : _plan(plan),
        _toUnlink(toUnlink),
        _remove(false),
        _scatter(false),
        _gather(false),
        _enumColl(nullptr),
        _setter(nullptr),
        _variable(nullptr),
        _lastNode(nullptr) {}

  ~RemoveToEnumCollFinder() {}

  bool before(ExecutionNode* en) override final {
    switch (en->getType()) {
      case EN::REMOVE: {
        if (_remove) {
          break;
        }

        // find the variable we are removing . . .
        auto rn = ExecutionNode::castTo<RemoveNode*>(en);
        auto varsToRemove = rn->getVariablesUsedHere();

        // remove nodes always have one input variable
        TRI_ASSERT(varsToRemove.size() == 1);

        _setter = _plan->getVarSetBy(varsToRemove[0]->id);
        TRI_ASSERT(_setter != nullptr);
        auto enumColl = _setter;

        if (_setter->getType() == EN::CALCULATION) {
          // this should be an attribute access for _key
          auto cn = ExecutionNode::castTo<CalculationNode*>(_setter);

          auto expr = cn->expression();
          if (expr->isAttributeAccess()) {
            // check the variable is the same as the remove variable
            auto vars = cn->getVariablesSetHere();
            if (vars.size() != 1 || vars[0]->id != varsToRemove[0]->id) {
              break;  // abort . . .
            }
            // check the remove node's collection is sharded over _key
            std::vector<std::string> shardKeys = rn->collection()->shardKeys();
            if (shardKeys.size() != 1 ||
                shardKeys[0] != StaticStrings::KeyString) {
              break;  // abort . . .
            }

            // set the varsToRemove to the variable in the expression of this
            // node and also define enumColl
            varsToRemove = cn->getVariablesUsedHere();
            TRI_ASSERT(varsToRemove.size() == 1);
            enumColl = _plan->getVarSetBy(varsToRemove[0]->id);
            TRI_ASSERT(_setter != nullptr);
          } else if (expr->node() && expr->node()->isObject()) {
            auto n = expr->node();

            if (n == nullptr) {
              break;
            }

            // note for which shard keys we need to look for
            auto shardKeys = rn->collection()->shardKeys();
            std::unordered_set<std::string> toFind;
            for (auto const& it : shardKeys) {
              toFind.emplace(it);
            }
            // for REMOVE, we must also know the _key value, otherwise
            // REMOVE will not work
            toFind.emplace(StaticStrings::KeyString);

            // go through the input object attribute by attribute
            // and look for our shard keys
            Variable const* lastVariable = nullptr;
            bool doOptimize = true;

            for (size_t i = 0; i < n->numMembers(); ++i) {
              auto sub = n->getMember(i);

              if (sub->type != NODE_TYPE_OBJECT_ELEMENT) {
                continue;
              }

              auto it = toFind.find(sub->getString());

              if (it != toFind.end()) {
                // we found one of the shard keys!
                // remove the attribute from our to-do list
                auto value = sub->getMember(0);

                if (value->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
                  // check if all values for the shard keys are referring to the
                  // same FOR loop variable
                  auto var = value->getMember(0);
                  if (var->type == NODE_TYPE_REFERENCE) {
                    auto accessedVariable =
                        static_cast<Variable const*>(var->getData());

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

        if (enumColl->getType() == EN::ENUMERATE_COLLECTION &&
            !dynamic_cast<DocumentProducingNode const*>(enumColl)
                 ->projections()
                 .empty()) {
          // cannot handle projections yet
          break;
        }

        _enumColl = enumColl;

        if (::getCollection(_enumColl) != rn->collection()) {
          break;  // abort . . .
        }

        _variable = varsToRemove[0];  // the variable we'll remove
        _remove = true;
        _lastNode = en;
        return false;  // continue . . .
      }
      case EN::REMOTE: {
        _toUnlink.emplace(en);
        _lastNode = en;
        return false;  // continue . . .
      }
      case EN::DISTRIBUTE:
      case EN::SCATTER: {
        if (_scatter) {  // met more than one scatter node
          break;         // abort . . .
        }
        _scatter = true;
        _toUnlink.emplace(en);
        _lastNode = en;
        return false;  // continue . . .
      }
      case EN::GATHER: {
        if (_gather) {  // met more than one gather node
          break;        // abort . . .
        }
        _gather = true;
        _toUnlink.emplace(en);
        _lastNode = en;
        return false;  // continue . . .
      }
      case EN::FILTER: {
        _lastNode = en;
        return false;  // continue . . .
      }
      case EN::CALCULATION: {
        TRI_ASSERT(_setter != nullptr);
        if (_setter->getType() == EN::CALCULATION &&
            _setter->id() == en->id()) {
          _lastNode = en;
          return false;  // continue . . .
        }
        if (_lastNode == nullptr || _lastNode->getType() != EN::FILTER) {
          // doesn't match the last filter node
          break;  // abort . . .
        }
        auto cn = ExecutionNode::castTo<CalculationNode*>(en);
        auto fn = ExecutionNode::castTo<FilterNode*>(_lastNode);

        // check these are a Calc-Filter pair
        if (cn->getVariablesSetHere()[0]->id !=
            fn->getVariablesUsedHere()[0]->id) {
          break;  // abort . . .
        }

        // check that we are filtering/calculating something with the variable
        // we are to remove
        auto varsUsedHere = cn->getVariablesUsedHere();

        if (varsUsedHere.size() != 1) {
          break;  // abort . . .
        }
        if (varsUsedHere[0]->id != _variable->id) {
          break;
        }
        _lastNode = en;
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
#ifdef USE_IRESEARCH
      case EN::ENUMERATE_IRESEARCH_VIEW:
#endif
      case EN::SUBQUERY:
      case EN::COLLECT:
      case EN::INSERT:
      case EN::REPLACE:
      case EN::UPDATE:
      case EN::UPSERT:
      case EN::RETURN:
      case EN::NORESULTS:
      case EN::LIMIT:
      case EN::SORT:
      case EN::TRAVERSAL:
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
    OptimizerRule const* rule) {
  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, EN::REMOVE, true);

  std::unordered_set<ExecutionNode*> toUnlink;

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

/// @brief auxilliary struct for finding common nodes in OR conditions
struct CommonNodeFinder {
  std::vector<AstNode const*> possibleNodes;

  bool find(AstNode const* node, AstNodeType condition,
            AstNode const*& commonNode, std::string& commonName) {
    if (node->type == NODE_TYPE_OPERATOR_BINARY_OR) {
      return (find(node->getMember(0), condition, commonNode, commonName) &&
              find(node->getMember(1), condition, commonNode, commonName));
    }

    if (node->type == NODE_TYPE_VALUE) {
      possibleNodes.clear();
      return true;
    }

    if (node->type == condition ||
        (condition != NODE_TYPE_OPERATOR_BINARY_EQ &&
         (node->type == NODE_TYPE_OPERATOR_BINARY_LE ||
          node->type == NODE_TYPE_OPERATOR_BINARY_LT ||
          node->type == NODE_TYPE_OPERATOR_BINARY_GE ||
          node->type == NODE_TYPE_OPERATOR_BINARY_GT ||
          node->type == NODE_TYPE_OPERATOR_BINARY_IN))) {
      auto lhs = node->getMember(0);
      auto rhs = node->getMember(1);

      bool const isIn =
          (node->type == NODE_TYPE_OPERATOR_BINARY_IN && rhs->isArray());

      if (node->type == NODE_TYPE_OPERATOR_BINARY_IN &&
          rhs->type == NODE_TYPE_EXPANSION) {
        // ooh, cannot optimize this (yet)
        possibleNodes.clear();
        return false;
      }

      if (!isIn && lhs->isConstant()) {
        commonNode = rhs;
        commonName = commonNode->toString();
        possibleNodes.clear();
        return true;
      }

      if (rhs->isConstant()) {
        commonNode = lhs;
        commonName = commonNode->toString();
        possibleNodes.clear();
        return true;
      }

      if (rhs->type == NODE_TYPE_FCALL || rhs->type == NODE_TYPE_FCALL_USER ||
          rhs->type == NODE_TYPE_REFERENCE) {
        commonNode = lhs;
        commonName = commonNode->toString();
        possibleNodes.clear();
        return true;
      }

      if (!isIn &&
          (lhs->type == NODE_TYPE_FCALL || lhs->type == NODE_TYPE_FCALL_USER ||
           lhs->type == NODE_TYPE_REFERENCE)) {
        commonNode = rhs;
        commonName = commonNode->toString();
        possibleNodes.clear();
        return true;
      }

      if (!isIn && (lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS ||
                    lhs->type == NODE_TYPE_INDEXED_ACCESS)) {
        if (possibleNodes.size() == 2) {
          for (size_t i = 0; i < 2; i++) {
            if (lhs->toString() == possibleNodes[i]->toString()) {
              commonNode = possibleNodes[i];
              commonName = commonNode->toString();
              possibleNodes.clear();
              return true;
            }
          }
          // don't return, must consider the other side of the condition
        } else {
          possibleNodes.emplace_back(lhs);
        }
      }
      if (rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS ||
          rhs->type == NODE_TYPE_INDEXED_ACCESS) {
        if (possibleNodes.size() == 2) {
          for (size_t i = 0; i < 2; i++) {
            if (rhs->toString() == possibleNodes[i]->toString()) {
              commonNode = possibleNodes[i];
              commonName = commonNode->toString();
              possibleNodes.clear();
              return true;
            }
          }
          return false;
        } else {
          possibleNodes.emplace_back(rhs);
          return true;
        }
      }
    }
    possibleNodes.clear();
    return (!commonName.empty());
  }
};

/// @brief auxilliary struct for the OR-to-IN conversion
struct OrSimplifier {
  Ast* ast;
  ExecutionPlan* plan;

  OrSimplifier(Ast* ast, ExecutionPlan* plan) : ast(ast), plan(plan) {}

  std::string stringifyNode(AstNode const* node) const {
    try {
      return node->toString();
    } catch (...) {
    }
    return std::string();
  }

  bool qualifies(AstNode const* node, std::string& attributeName) const {
    if (node->isConstant()) {
      return false;
    }

    if (node->type == NODE_TYPE_ATTRIBUTE_ACCESS ||
        node->type == NODE_TYPE_INDEXED_ACCESS ||
        node->type == NODE_TYPE_REFERENCE) {
      attributeName = stringifyNode(node);
      return true;
    }

    return false;
  }

  bool detect(AstNode const* node, bool preferRight, std::string& attributeName,
              AstNode const*& attr, AstNode const*& value) const {
    attributeName.clear();

    if (node->type == NODE_TYPE_OPERATOR_BINARY_EQ) {
      auto lhs = node->getMember(0);
      auto rhs = node->getMember(1);
      if (!preferRight && qualifies(lhs, attributeName)) {
        if (rhs->isDeterministic()) {
          attr = lhs;
          value = rhs;
          return true;
        }
      }

      if (qualifies(rhs, attributeName)) {
        if (lhs->isDeterministic()) {
          attr = rhs;
          value = lhs;
          return true;
        }
      }
      // intentionally falls through
    } else if (node->type == NODE_TYPE_OPERATOR_BINARY_IN) {
      auto lhs = node->getMember(0);
      auto rhs = node->getMember(1);
      if (rhs->isArray() && qualifies(lhs, attributeName)) {
        if (rhs->isDeterministic()) {
          attr = lhs;
          value = rhs;
          return true;
        }
      }
      // intentionally falls through
    }

    return false;
  }

  AstNode* buildValues(AstNode const* attr, AstNode const* lhs,
                       bool leftIsArray, AstNode const* rhs,
                       bool rightIsArray) const {
    auto values = ast->createNodeArray();
    if (leftIsArray) {
      size_t const n = lhs->numMembers();
      for (size_t i = 0; i < n; ++i) {
        values->addMember(lhs->getMemberUnchecked(i));
      }
    } else {
      values->addMember(lhs);
    }

    if (rightIsArray) {
      size_t const n = rhs->numMembers();
      for (size_t i = 0; i < n; ++i) {
        values->addMember(rhs->getMemberUnchecked(i));
      }
    } else {
      values->addMember(rhs);
    }

    return ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_IN, attr,
                                         values);
  }

  AstNode* simplify(AstNode const* node) const {
    if (node == nullptr) {
      return nullptr;
    }

    if (node->type == NODE_TYPE_OPERATOR_BINARY_OR) {
      auto lhs = node->getMember(0);
      auto rhs = node->getMember(1);

      auto lhsNew = simplify(lhs);
      auto rhsNew = simplify(rhs);

      if (lhs != lhsNew || rhs != rhsNew) {
        // create a modified node
        node = ast->createNodeBinaryOperator(node->type, lhsNew, rhsNew);
      }

      if ((lhsNew->type == NODE_TYPE_OPERATOR_BINARY_EQ ||
           lhsNew->type == NODE_TYPE_OPERATOR_BINARY_IN) &&
          (rhsNew->type == NODE_TYPE_OPERATOR_BINARY_EQ ||
           rhsNew->type == NODE_TYPE_OPERATOR_BINARY_IN)) {
        std::string leftName;
        std::string rightName;
        AstNode const* leftAttr = nullptr;
        AstNode const* rightAttr = nullptr;
        AstNode const* leftValue = nullptr;
        AstNode const* rightValue = nullptr;

        for (size_t i = 0; i < 4; ++i) {
          if (detect(lhsNew, i >= 2, leftName, leftAttr, leftValue) &&
              detect(rhsNew, i % 2 == 0, rightName, rightAttr, rightValue) &&
              leftName == rightName) {
            std::pair<Variable const*,
                      std::vector<arangodb::basics::AttributeName>>
                tmp1;

            if (leftValue->isAttributeAccessForVariable(tmp1)) {
              bool qualifies = false;
              auto setter = plan->getVarSetBy(tmp1.first->id);
              if (setter != nullptr &&
                  setter->getType() == EN::ENUMERATE_COLLECTION) {
                qualifies = true;
              }

              std::pair<Variable const*,
                        std::vector<arangodb::basics::AttributeName>>
                  tmp2;

              if (qualifies && rightValue->isAttributeAccessForVariable(tmp2)) {
                auto setter = plan->getVarSetBy(tmp2.first->id);
                if (setter != nullptr &&
                    setter->getType() == EN::ENUMERATE_COLLECTION) {
                  if (tmp1.first != tmp2.first || tmp1.second != tmp2.second) {
                    continue;
                  }
                }
              }
            }

            return buildValues(leftAttr, leftValue,
                               lhsNew->type == NODE_TYPE_OPERATOR_BINARY_IN,
                               rightValue,
                               rhsNew->type == NODE_TYPE_OPERATOR_BINARY_IN);
          }
        }
      }

      // return node as is
      return const_cast<AstNode*>(node);
    }

    if (node->type == NODE_TYPE_OPERATOR_BINARY_AND) {
      auto lhs = node->getMember(0);
      auto rhs = node->getMember(1);

      auto lhsNew = simplify(lhs);
      auto rhsNew = simplify(rhs);

      if (lhs != lhsNew || rhs != rhsNew) {
        // return a modified node
        return ast->createNodeBinaryOperator(node->type, lhsNew, rhsNew);
      }

      // intentionally falls through
    }

    return const_cast<AstNode*>(node);
  }
};

/// @brief this rule replaces expressions of the type:
///   x.val == 1 || x.val == 2 || x.val == 3
//  with
//    x.val IN [1,2,3]
//  when the OR conditions are present in the same FILTER node, and refer to the
//  same (single) attribute.
void arangodb::aql::replaceOrWithInRule(Optimizer* opt,
                                        std::unique_ptr<ExecutionPlan> plan,
                                        OptimizerRule const* rule) {
  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, EN::FILTER, true);

  bool modified = false;
  for (auto const& n : nodes) {
    TRI_ASSERT(n->hasDependency());

    auto const dep = n->getFirstDependency();

    if (dep->getType() != EN::CALCULATION) {
      continue;
    }

    auto fn = ExecutionNode::castTo<FilterNode*>(n);
    auto inVar = fn->getVariablesUsedHere();

    auto cn = ExecutionNode::castTo<CalculationNode*>(dep);
    auto outVar = cn->getVariablesSetHere();

    if (outVar.size() != 1 || outVar[0]->id != inVar[0]->id) {
      continue;
    }

    auto root = cn->expression()->node();

    OrSimplifier simplifier(plan->getAst(), plan.get());
    auto newRoot = simplifier.simplify(root);

    if (newRoot != root) {
      ExecutionNode* newNode = nullptr;
      Expression* expr = new Expression(plan.get(), plan->getAst(), newRoot);

      try {
        TRI_IF_FAILURE("OptimizerRules::replaceOrWithInRuleOom") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }

        newNode =
            new CalculationNode(plan.get(), plan->nextId(), expr, outVar[0]);
      } catch (...) {
        delete expr;
        throw;
      }

      plan->registerNode(newNode);
      plan->replaceNode(cn, newNode);
      modified = true;
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}

struct RemoveRedundantOr {
  AstNode const* bestValue = nullptr;
  AstNodeType comparison;
  bool inclusive;
  bool isComparisonSet = false;
  CommonNodeFinder finder;
  AstNode const* commonNode = nullptr;
  std::string commonName;

  bool hasRedundantCondition(AstNode const* node) {
    try {
      if (finder.find(node, NODE_TYPE_OPERATOR_BINARY_LT, commonNode,
                      commonName)) {
        return hasRedundantConditionWalker(node);
      }
    } catch (...) {
      // ignore errors and simply return false
    }
    return false;
  }

  AstNode* createReplacementNode(Ast* ast) {
    TRI_ASSERT(commonNode != nullptr);
    TRI_ASSERT(bestValue != nullptr);
    TRI_ASSERT(isComparisonSet == true);
    return ast->createNodeBinaryOperator(comparison, commonNode->clone(ast),
                                         bestValue);
  }

 private:
  bool isInclusiveBound(AstNodeType type) {
    return (type == NODE_TYPE_OPERATOR_BINARY_GE ||
            type == NODE_TYPE_OPERATOR_BINARY_LE);
  }

  int isCompatibleBound(AstNodeType type, AstNode const* value) {
    if ((comparison == NODE_TYPE_OPERATOR_BINARY_LE ||
         comparison == NODE_TYPE_OPERATOR_BINARY_LT) &&
        (type == NODE_TYPE_OPERATOR_BINARY_LE ||
         type == NODE_TYPE_OPERATOR_BINARY_LT)) {
      return -1;  // high bound
    } else if ((comparison == NODE_TYPE_OPERATOR_BINARY_GE ||
                comparison == NODE_TYPE_OPERATOR_BINARY_GT) &&
               (type == NODE_TYPE_OPERATOR_BINARY_GE ||
                type == NODE_TYPE_OPERATOR_BINARY_GT)) {
      return 1;  // low bound
    }
    return 0;  // incompatible bounds
  }

  // returns false if the existing value is better and true if the input value
  // is better
  bool compareBounds(AstNodeType type, AstNode const* value, int lowhigh) {
    int cmp = CompareAstNodes(bestValue, value, true);

    if (cmp == 0 && (isInclusiveBound(comparison) != isInclusiveBound(type))) {
      return (isInclusiveBound(type) ? true : false);
    }
    return (cmp * lowhigh == 1);
  }

  bool hasRedundantConditionWalker(AstNode const* node) {
    AstNodeType type = node->type;

    if (type == NODE_TYPE_OPERATOR_BINARY_OR) {
      return (hasRedundantConditionWalker(node->getMember(0)) &&
              hasRedundantConditionWalker(node->getMember(1)));
    }

    if (type == NODE_TYPE_OPERATOR_BINARY_LE ||
        type == NODE_TYPE_OPERATOR_BINARY_LT ||
        type == NODE_TYPE_OPERATOR_BINARY_GE ||
        type == NODE_TYPE_OPERATOR_BINARY_GT) {
      auto lhs = node->getMember(0);
      auto rhs = node->getMember(1);

      if (hasRedundantConditionWalker(rhs) &&
          !hasRedundantConditionWalker(lhs) && lhs->isConstant()) {
        if (!isComparisonSet) {
          comparison = Ast::ReverseOperator(type);
          bestValue = lhs;
          isComparisonSet = true;
          return true;
        }

        int lowhigh = isCompatibleBound(Ast::ReverseOperator(type), lhs);
        if (lowhigh == 0) {
          return false;
        }

        if (compareBounds(type, lhs, lowhigh)) {
          comparison = Ast::ReverseOperator(type);
          bestValue = lhs;
        }
        return true;
      }
      if (hasRedundantConditionWalker(lhs) &&
          !hasRedundantConditionWalker(rhs) && rhs->isConstant()) {
        if (!isComparisonSet) {
          comparison = type;
          bestValue = rhs;
          isComparisonSet = true;
          return true;
        }

        int lowhigh = isCompatibleBound(type, rhs);
        if (lowhigh == 0) {
          return false;
        }

        if (compareBounds(type, rhs, lowhigh)) {
          comparison = type;
          bestValue = rhs;
        }
        return true;
      }
      // if hasRedundantConditionWalker(lhs) and
      // hasRedundantConditionWalker(rhs), then one of the conditions in the OR
      // statement is of the form x == x intentionally falls through
    } else if (type == NODE_TYPE_REFERENCE ||
               type == NODE_TYPE_ATTRIBUTE_ACCESS ||
               type == NODE_TYPE_INDEXED_ACCESS) {
      // get a string representation of the node for comparisons
      return (node->toString() == commonName);
    }

    return false;
  }
};

void arangodb::aql::removeRedundantOrRule(Optimizer* opt,
                                          std::unique_ptr<ExecutionPlan> plan,
                                          OptimizerRule const* rule) {
  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, EN::FILTER, true);

  bool modified = false;
  for (auto const& n : nodes) {
    TRI_ASSERT(n->hasDependency());

    auto const dep = n->getFirstDependency();

    if (dep->getType() != EN::CALCULATION) {
      continue;
    }

    auto fn = ExecutionNode::castTo<FilterNode*>(n);
    auto inVar = fn->getVariablesUsedHere();

    auto cn = ExecutionNode::castTo<CalculationNode*>(dep);
    auto outVar = cn->getVariablesSetHere();

    if (outVar.size() != 1 || outVar[0]->id != inVar[0]->id) {
      continue;
    }
    if (cn->expression()->node()->type != NODE_TYPE_OPERATOR_BINARY_OR) {
      continue;
    }

    RemoveRedundantOr remover;
    if (remover.hasRedundantCondition(cn->expression()->node())) {
      ExecutionNode* newNode = nullptr;
      auto astNode = remover.createReplacementNode(plan->getAst());

      Expression* expr = new Expression(plan.get(), plan->getAst(), astNode);

      try {
        newNode =
            new CalculationNode(plan.get(), plan->nextId(), expr, outVar[0]);
      } catch (...) {
        delete expr;
        throw;
      }

      plan->registerNode(newNode);
      plan->replaceNode(cn, newNode);
      modified = true;
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}

/// @brief remove $OLD and $NEW variables from data-modification statements
/// if not required
void arangodb::aql::removeDataModificationOutVariablesRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const* rule) {
  bool modified = false;

  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, ::removeDataModificationOutVariablesNodeTypes, true);

  for (auto const& n : nodes) {
    auto node = ExecutionNode::castTo<ModificationNode*>(n);
    TRI_ASSERT(node != nullptr);

    Variable const* old = node->getOutVariableOld();
    if (!n->isVarUsedLater(old)) {
      // "$OLD" is not used later
      node->clearOutVariableOld();
      modified = true;
    } else {
      switch (n->getType()) {
        case EN::UPDATE:
        case EN::REPLACE: {
          Variable const* inVariable = ExecutionNode::castTo<UpdateReplaceNode const*>(n)->inKeyVariable();
          if (inVariable != nullptr) {
            auto setter = plan->getVarSetBy(inVariable->id);
            if (setter != nullptr && (setter->getType() == EN::ENUMERATE_COLLECTION || setter->getType() == EN::INDEX)) {
              std::unordered_map<VariableId, Variable const*> replacements;
              replacements.emplace(old->id, inVariable);
              RedundantCalculationsReplacer finder(plan->getAst(), replacements);
              plan->root()->walk(finder);
              modified = true;
            }
          }
          break;
        }
        case EN::REMOVE: {
          Variable const* inVariable = ExecutionNode::castTo<RemoveNode const*>(n)->inVariable();
          TRI_ASSERT(inVariable != nullptr);
          auto setter = plan->getVarSetBy(inVariable->id);
          if (setter != nullptr && (setter->getType() == EN::ENUMERATE_COLLECTION || setter->getType() == EN::INDEX)) {
            std::unordered_map<VariableId, Variable const*> replacements;
            replacements.emplace(old->id, inVariable);
            RedundantCalculationsReplacer finder(plan->getAst(), replacements);
            plan->root()->walk(finder);
            modified = true;
          }
          break;
        }
        default: {
          // do nothing
        }
      }
    }

    if (!n->isVarUsedLater(node->getOutVariableNew())) {
      // "$NEW" is not used later
      node->clearOutVariableNew();
      modified = true;
    }

    if (!n->hasParent()) {
      node->producesResults(false);
      modified = true;
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}

/// @brief patch UPDATE statement on single collection that iterates over the
/// entire collection to operate in batches
void arangodb::aql::patchUpdateStatementsRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const* rule) {
  // no need to dive into subqueries here
  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};

  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  if (engine->typeName() == "mmfiles") {
    // MMFiles: we can update UPDATE/REPLACE but not REMOVE
    // this is because in MMFiles the iteration over a collection may
    // use the primary index, but a REMOVE may at the same time remove
    // the documents from this index. this would not be safe
    plan->findNodesOfType(nodes, ::patchUpdateStatementsNodeTypes, false);
  } else {
    // other engines: we can update UPDATE/REPLACE as well as REMOVE
    plan->findNodesOfType(nodes, ::patchUpdateRemoveStatementsNodeTypes, false);
  }

  bool modified = false;

  for (auto const& n : nodes) {
    auto node = ExecutionNode::castTo<ModificationNode*>(n);
    TRI_ASSERT(node != nullptr);

    auto& options = node->getOptions();
    if (!options.readCompleteInput) {
      // already ok
      continue;
    }

    auto const collection = node->collection();

    auto dep = n->getFirstDependency();

    while (dep != nullptr) {
      auto const type = dep->getType();

      if (type == EN::ENUMERATE_LIST ||
#ifdef USE_IRESEARCH
          type == EN::ENUMERATE_IRESEARCH_VIEW ||
#endif
          type == EN::SUBQUERY) {
        // not suitable
        modified = false;
        break;
      }

      if (type == EN::ENUMERATE_COLLECTION || type == EN::INDEX) {
        if (::getCollection(dep) == collection) {
          if (modified) {
            // already saw the collection... that means we have seen the same
            // collection two times in two FOR loops
            modified = false;
            // abort
            break;
          }
          TRI_ASSERT(!modified);
          // saw the same collection in FOR as in UPDATE
          if (n->isVarUsedLater(::getOutVariable(dep))) {
            // must abort, because the variable produced by the FOR loop is
            // read after it is updated
            break;
          }
          modified = true;
        }
      } else if (type == EN::TRAVERSAL || type == EN::SHORTEST_PATH) {
        // unclear what will be read by the traversal
        modified = false;
        break;
      }

      dep = dep->getFirstDependency();
    }

    if (modified) {
      options.readCompleteInput = false;
    }
  }

  // always re-add the original plan, be it modified or not
  // only a flag in the plan will be modified
  opt->addPlan(std::move(plan), rule, modified);
}

/// @brief optimizes away unused traversal output variables and
/// merges filter nodes into graph traversal nodes
void arangodb::aql::optimizeTraversalsRule(Optimizer* opt,
                                           std::unique_ptr<ExecutionPlan> plan,
                                           OptimizerRule const* rule) {
  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> tNodes{a};
  plan->findNodesOfType(tNodes, EN::TRAVERSAL, true);

  if (tNodes.empty()) {
    // no traversals present
    opt->addPlan(std::move(plan), rule, false);
    return;
  }

  bool modified = false;

  // first make a pass over all traversal nodes and remove unused
  // variables from them
  for (auto const& n : tNodes) {
    TraversalNode* traversal = ExecutionNode::castTo<TraversalNode*>(n);

    // note that we can NOT optimize away the vertex output variable
    // yet, as many traversal internals depend on the number of vertices
    // found/built
    auto outVariable = traversal->edgeOutVariable();
    if (outVariable != nullptr && !n->isVarUsedLater(outVariable)) {
      // traversal edge outVariable not used later
      traversal->setEdgeOutput(nullptr);
      modified = true;
    }

    outVariable = traversal->pathOutVariable();
    if (outVariable != nullptr && !n->isVarUsedLater(outVariable)) {
      // traversal path outVariable not used later
      traversal->setPathOutput(nullptr);
      modified = true;
    }
  }

  if (!tNodes.empty()) {
    // These are all the end nodes where we start
    SmallVector<ExecutionNode*>::allocator_type::arena_type a;
    SmallVector<ExecutionNode*> nodes{a};
    plan->findEndNodes(nodes, true);

    for (auto const& n : nodes) {
      TraversalConditionFinder finder(plan.get(), &modified);
      n->walk(finder);
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}

// remove filter nodes already covered by a traversal
void arangodb::aql::removeFiltersCoveredByTraversal(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const* rule) {
  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> fNodes{a};
  plan->findNodesOfType(fNodes, EN::FILTER, true);
  if (fNodes.empty()) {
    // no filters present
    opt->addPlan(std::move(plan), rule, false);
    return;
  }

  bool modified = false;
  std::unordered_set<ExecutionNode*> toUnlink;

  for (auto const& node : fNodes) {
    auto fn = ExecutionNode::castTo<FilterNode const*>(node);
    // find the node with the filter expression
    auto inVar = fn->getVariablesUsedHere();
    TRI_ASSERT(inVar.size() == 1);

    auto setter = plan->getVarSetBy(inVar[0]->id);
    if (setter == nullptr || setter->getType() != EN::CALCULATION) {
      continue;
    }

    auto calculationNode = ExecutionNode::castTo<CalculationNode*>(setter);
    auto conditionNode = calculationNode->expression()->node();

    // build the filter condition
    Condition condition(plan->getAst());
    condition.andCombine(conditionNode);
    condition.normalize(plan.get());

    if (condition.root() == nullptr) {
      continue;
    }

    size_t const n = condition.root()->numMembers();

    if (n != 1) {
      // either no condition or multiple ORed conditions...
      continue;
    }

    bool handled = false;
    auto current = node;
    while (current != nullptr) {
      if (current->getType() == EN::TRAVERSAL) {
        auto traversalNode =
            ExecutionNode::castTo<TraversalNode const*>(current);

        // found a traversal node, now check if the expression
        // is covered by the traversal
        auto traversalCondition = traversalNode->condition();

        if (traversalCondition != nullptr && !traversalCondition->isEmpty()) {
          /*auto const& indexesUsed = traversalNode->get
          //indexNode->getIndexes();

          if (indexesUsed.size() == 1) {*/
          // single index. this is something that we can handle
          Variable const* outVariable = traversalNode->pathOutVariable();
          std::unordered_set<Variable const*> varsUsedByCondition;
          Ast::getReferencedVariables(condition.root(), varsUsedByCondition);
          if (outVariable != nullptr && varsUsedByCondition.find(outVariable) !=
                                            varsUsedByCondition.end()) {
            auto newNode = condition.removeTraversalCondition(
                plan.get(), outVariable, traversalCondition->root());
            if (newNode == nullptr) {
              // no condition left...
              // FILTER node can be completely removed
              toUnlink.emplace(node);
              // note: we must leave the calculation node intact, in case it is
              // still used by other nodes in the plan
              modified = true;
              handled = true;
            } else if (newNode != condition.root()) {
              // some condition is left, but it is a different one than
              // the one from the FILTER node
              auto expr = std::make_unique<Expression>(plan.get(),
                                                       plan->getAst(), newNode);
              CalculationNode* cn =
                  new CalculationNode(plan.get(), plan->nextId(), expr.get(),
                                      calculationNode->outVariable());
              expr.release();
              plan->registerNode(cn);
              plan->replaceNode(setter, cn);
              modified = true;
              handled = true;
            }
          }
        }

        if (handled) {
          break;
        }
      }

      if (handled || current->getType() == EN::LIMIT ||
          !current->hasDependency()) {
        break;
      }
      current = current->getFirstDependency();
    }
  }

  if (!toUnlink.empty()) {
    plan->unlinkNodes(toUnlink);
  }

  opt->addPlan(std::move(plan), rule, modified);
}

/// @brief removes redundant path variables, after applying
/// `removeFiltersCoveredByTraversal`. Should significantly reduce overhead
void arangodb::aql::removeTraversalPathVariable(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const* rule) {
  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> tNodes{a};
  plan->findNodesOfType(tNodes, EN::TRAVERSAL, true);

  bool modified = false;
  // first make a pass over all traversal nodes and remove unused
  // variables from them
  for (auto const& n : tNodes) {
    TraversalNode* traversal = ExecutionNode::castTo<TraversalNode*>(n);

    auto outVariable = traversal->pathOutVariable();
    if (outVariable != nullptr && !n->isVarUsedLater(outVariable)) {
      // traversal path outVariable not used later
      traversal->setPathOutput(nullptr);
      modified = true;
    }
  }
  opt->addPlan(std::move(plan), rule, modified);
}

/// @brief prepares traversals for execution (hidden rule)
void arangodb::aql::prepareTraversalsRule(Optimizer* opt,
                                          std::unique_ptr<ExecutionPlan> plan,
                                          OptimizerRule const* rule) {
  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> tNodes{a};
  plan->findNodesOfType(tNodes, EN::TRAVERSAL, true);
  plan->findNodesOfType(tNodes, EN::SHORTEST_PATH, true);

  if (tNodes.empty()) {
    // no traversals present
    opt->addPlan(std::move(plan), rule, false);
    return;
  }

  // first make a pass over all traversal nodes and remove unused
  // variables from them
  for (auto const& n : tNodes) {
    if (n->getType() == EN::TRAVERSAL) {
      TraversalNode* traversal = ExecutionNode::castTo<TraversalNode*>(n);
      traversal->prepareOptions();
    } else {
      TRI_ASSERT(n->getType() == EN::SHORTEST_PATH);
      ShortestPathNode* spn = ExecutionNode::castTo<ShortestPathNode*>(n);
      spn->prepareOptions();
    }
  }

  opt->addPlan(std::move(plan), rule, true);
}

/// @brief pulls out simple subqueries and merges them with the level above
///
/// For example, if we have the input query
///
/// FOR x IN (
///     FOR y IN collection FILTER y.value >= 5 RETURN y.test
///   )
///   RETURN x.a
///
/// then this rule will transform it into:
///
/// FOR tmp IN collection
///   FILTER tmp.value >= 5
///   LET x = tmp.test
///   RETURN x.a
void arangodb::aql::inlineSubqueriesRule(Optimizer* opt,
                                         std::unique_ptr<ExecutionPlan> plan,
                                         OptimizerRule const* rule) {
  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, EN::SUBQUERY, true);

  if (nodes.empty()) {
    opt->addPlan(std::move(plan), rule, false);
    return;
  }

  bool modified = false;
  std::vector<ExecutionNode*> subNodes;

  for (auto const& n : nodes) {
    auto subqueryNode = ExecutionNode::castTo<SubqueryNode*>(n);

    if (subqueryNode->isModificationSubquery()) {
      // can't modify modifying subqueries
      continue;
    }

    if (!subqueryNode->isDeterministic()) {
      // can't inline non-deterministic subqueries
      continue;
    }

    // check if subquery contains a COLLECT node with an INTO variable
    bool eligible = true;
    bool containsLimitOrSort = false;
    auto current = subqueryNode->getSubquery();
    TRI_ASSERT(current != nullptr);

    while (current != nullptr) {
      if (current->getType() == EN::COLLECT) {
        if (ExecutionNode::castTo<CollectNode const*>(current)
                ->hasOutVariable()) {
          eligible = false;
          break;
        }
      } else if (current->getType() == EN::LIMIT ||
                 current->getType() == EN::SORT) {
        containsLimitOrSort = true;
      }
      current = current->getFirstDependency();
    }

    if (!eligible) {
      continue;
    }

    Variable const* out = subqueryNode->outVariable();
    TRI_ASSERT(out != nullptr);
    // the subquery outvariable and all its aliases
    std::unordered_set<Variable const*> subqueryVars;
    subqueryVars.emplace(out);

    // the potential calculation nodes that produce the aliases
    std::vector<ExecutionNode*> aliasNodesToRemoveLater;

    std::unordered_set<Variable const*> varsUsed;

    current = n->getFirstParent();
    // now check where the subquery is used
    while (current->hasParent()) {
      if (current->getType() == EN::ENUMERATE_LIST) {
        if (current->isInInnerLoop() && containsLimitOrSort) {
          // exit the loop
          current = nullptr;
          break;
        }

        // we're only interested in FOR loops...
        auto listNode = ExecutionNode::castTo<EnumerateListNode*>(current);

        // ...that use our subquery as its input
        if (subqueryVars.find(listNode->inVariable()) != subqueryVars.end()) {
          // bingo!

          // check if the subquery result variable or any of the aliases are
          // used after the FOR loop
          bool mustAbort = false;
          for (auto const& itSub : subqueryVars) {
            if (listNode->isVarUsedLater(itSub)) {
              // exit the loop
              current = nullptr;
              mustAbort = true;
              break;
            }
          }
          if (mustAbort) {
            break;
          }

          for (auto const& toRemove : aliasNodesToRemoveLater) {
            plan->unlinkNode(toRemove, false);
          }

          subNodes.clear();
          subNodes.reserve(4);
          subqueryNode->getSubquery()->getDependencyChain(subNodes, true);
          TRI_ASSERT(!subNodes.empty());
          auto returnNode = ExecutionNode::castTo<ReturnNode*>(subNodes[0]);
          TRI_ASSERT(returnNode->getType() == EN::RETURN);

          modified = true;
          auto queryVariables = plan->getAst()->variables();
          auto previous = n->getFirstDependency();
          auto insert = n->getFirstParent();
          TRI_ASSERT(insert != nullptr);

          // unlink the original SubqueryNode
          plan->unlinkNode(n, false);

          for (auto& it : subNodes) {
            // first unlink them all
            plan->unlinkNode(it, true);

            if (it->getType() == EN::SINGLETON) {
              // reached the singleton node already. that means we can stop
              break;
            }

            // and now insert them one level up
            if (it != returnNode) {
              // we skip over the subquery's return node. we don't need it
              // anymore
              insert->removeDependencies();
              TRI_ASSERT(it != nullptr);
              insert->addDependency(it);
              insert = it;

              // additionally rename the variables from the subquery so they
              // cannot conflict with the ones from the top query
              for (auto const& variable : it->getVariablesSetHere()) {
                queryVariables->renameVariable(variable->id);
              }
            }
          }

          // link the top node in the subquery with the original plan
          if (previous != nullptr) {
            insert->addDependency(previous);
          }

          // remove the list node from the plan
          plan->unlinkNode(listNode, false);

          queryVariables->renameVariable(returnNode->inVariable()->id,
                                         listNode->outVariable()->name);

          // finally replace the variables
          std::unordered_map<VariableId, Variable const*> replacements;
          replacements.emplace(listNode->outVariable()->id,
                               returnNode->inVariable());
          RedundantCalculationsReplacer finder(plan->getAst(), replacements);
          plan->root()->walk(finder);

          plan->clearVarUsageComputed();
          plan->findVarUsage();

          // abort optimization
          current = nullptr;
        }
      } else if (current->getType() == EN::CALCULATION) {
        auto rootNode = ExecutionNode::castTo<CalculationNode*>(current)
                            ->expression()
                            ->node();
        if (rootNode->type == NODE_TYPE_REFERENCE) {
          if (subqueryVars.find(static_cast<Variable const*>(
                  rootNode->getData())) != subqueryVars.end()) {
            // found an alias for the subquery variable
            subqueryVars.emplace(
                ExecutionNode::castTo<CalculationNode*>(current)
                    ->outVariable());
            aliasNodesToRemoveLater.emplace_back(current);
            current = current->getFirstParent();

            continue;
          }
        }
      }

      if (current == nullptr) {
        break;
      }

      varsUsed.clear();
      current->getVariablesUsedHere(varsUsed);

      bool mustAbort = false;
      for (auto const& itSub : subqueryVars) {
        if (varsUsed.find(itSub) != varsUsed.end()) {
          // we found another node that uses the subquery variable
          // we need to stop the optimization attempts here
          mustAbort = true;
          break;
        }
      }
      if (mustAbort) {
        break;
      }

      current = current->getFirstParent();
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}

static bool isValueOrReference(AstNode const* node) {
  return node->type == NODE_TYPE_VALUE || node->type == NODE_TYPE_REFERENCE;
}

/// Essentially mirrors the geo::QueryParams struct, but with
/// abstracts AstNode value objects
struct GeoIndexInfo {
  operator bool() const {
    return collectionNodeToReplace != nullptr && collectionNodeOutVar &&
           collection && index && valid;
  }
  void invalidate() { valid = false; }

  /// node that will be replaced by (geo) IndexNode
  ExecutionNode* collectionNodeToReplace = nullptr;
  Variable const* collectionNodeOutVar = nullptr;

  /// accessed collection
  aql::Collection const* collection = nullptr;
  /// selected index
  std::shared_ptr<Index> index;

  /// Filter calculations to modify
  std::map<ExecutionNode*, Expression*> exesToModify;
  std::set<AstNode const*> nodesToRemove;

  // ============ Distance ============
  AstNode const* distCenterExpr = nullptr;
  AstNode const* distCenterLatExpr = nullptr;
  AstNode const* distCenterLngExpr = nullptr;
  // Expression representing minimum distance
  AstNode const* minDistanceExpr = nullptr;
  // Was operator < or <= used
  bool minInclusive = true;
  // Expression representing maximum distance
  AstNode const* maxDistanceExpr = nullptr;
  // Was operator > or >= used
  bool maxInclusive = true;
  /// for WITHIN, we know we need to scan the full range, so do it in one pass
  bool fullRange = false;

  // ============ Near Info ============
  bool sorted = false;
  /// Default order is from closest to farthest
  bool ascending = true;

  // ============ Filter Info ===========
  geo::FilterType filterMode = geo::FilterType::NONE;
  /// variable using the filter mask
  AstNode const* filterExpr = nullptr;

  // ============ Accessed Fields ============
  AstNode const* locationVar = nullptr;   // access to location field
  AstNode const* latitudeVar = nullptr;   // access path to latitude
  AstNode const* longitudeVar = nullptr;  // access path to longitude

  /// contains this node a valid condition
  bool valid = true;
};

// checks 2 parameters of distance function if they represent a valid access to
// latitude and longitude attribute of the geo index.
// disance(a,b,c,d) - possible pairs are (a,b) and (c,d)
static bool distanceFuncArgCheck(ExecutionPlan* plan, AstNode const* latArg,
                                 AstNode const* lngArg, bool supportLegacy,
                                 GeoIndexInfo& info) {
  std::pair<Variable const*, std::vector<arangodb::basics::AttributeName>>
      attributeAccess1;
  std::pair<Variable const*, std::vector<arangodb::basics::AttributeName>>
      attributeAccess2;
  // first and second should be based on the same document - need to provide the
  // document in order to see which collection is bound to it and if that
  // collections supports geo-index
  if (!latArg->isAttributeAccessForVariable(attributeAccess1, true) ||
      !lngArg->isAttributeAccessForVariable(attributeAccess2, true)) {
    return false;
  }
  TRI_ASSERT(attributeAccess1.first != nullptr);
  TRI_ASSERT(attributeAccess2.first != nullptr);

  ExecutionNode* setter1 = plan->getVarSetBy(attributeAccess1.first->id);
  ExecutionNode* setter2 = plan->getVarSetBy(attributeAccess2.first->id);
  if (setter1 == nullptr || setter1 != setter2 ||
      setter1->getType() != EN::ENUMERATE_COLLECTION) {
    return false;  // expect access of doc.lat, doc.lng or doc.loc[0],
                   // doc.loc[1]
  }

  // get logical collection
  auto collNode = reinterpret_cast<EnumerateCollectionNode*>(setter1);
  if (info.collectionNodeToReplace != nullptr &&
      info.collectionNodeToReplace != collNode) {
    return false;  // should probably never happen
  }
  info.collectionNodeToReplace = collNode;
  info.collectionNodeOutVar = collNode->outVariable();
  info.collection = collNode->collection();

  // we should not access the LogicalCollection directly
  Query* query = plan->getAst()->query();
  auto indexes = query->trx()->indexesForCollection(info.collection->name());
  // check for suitiable indexes
  for (std::shared_ptr<Index> idx : indexes) {
    // check if current index is a geo-index
    std::size_t fieldNum = idx->fields().size();
    bool isGeo1 = idx->type() == Index::IndexType::TRI_IDX_TYPE_GEO1_INDEX &&
                  supportLegacy;
    bool isGeo2 = idx->type() == Index::IndexType::TRI_IDX_TYPE_GEO2_INDEX &&
                  supportLegacy;
    bool isGeo = idx->type() == Index::IndexType::TRI_IDX_TYPE_GEO_INDEX;

    if ((isGeo2 || isGeo) && fieldNum == 2) {  // individual fields
      // check access paths of attributes in ast and those in index match
      if (idx->fields()[0] == attributeAccess1.second &&
          idx->fields()[1] == attributeAccess2.second) {
        if (info.index != nullptr && info.index != idx) {
          return false;
        }
        info.index = idx;
        info.latitudeVar = latArg;
        info.longitudeVar = lngArg;
        return true;
      }
    } else if ((isGeo1 || isGeo) && fieldNum == 1) {
      std::vector<basics::AttributeName> fields1 = idx->fields()[0];
      std::vector<basics::AttributeName> fields2 = idx->fields()[0];

      VPackBuilder builder;
      idx->toVelocyPack(builder, Index::makeFlags(Index::Serialize::Basics));
      bool geoJson = basics::VelocyPackHelper::getBooleanValue(
          builder.slice(), "geoJson", false);

      fields1.back().name += geoJson ? "[1]" : "[0]";
      fields2.back().name += geoJson ? "[0]" : "[1]";
      if (fields1 == attributeAccess1.second &&
          fields2 == attributeAccess2.second) {
        if (info.index != nullptr && info.index != idx) {
          return false;
        }
        info.index = idx;
        info.latitudeVar = latArg;
        info.longitudeVar = lngArg;
        return true;
      }
    }  // if isGeo 1 or 2
  }    // for index in collection
  return false;
}

// checks parameter of GEO_* function
static bool geoFuncArgCheck(ExecutionPlan* plan, AstNode const* args,
                            bool supportLegacy, GeoIndexInfo& info) {
  std::pair<Variable const*, std::vector<arangodb::basics::AttributeName>>
      attributeAccess;
  // "arg" is either `[doc.lat, doc.lng]` or `doc.geometry`
  if (args->isArray() && args->numMembers() == 2) {
    return distanceFuncArgCheck(plan, /*lat*/ args->getMemberUnchecked(1),
                                /*lng*/ args->getMemberUnchecked(0),
                                supportLegacy, info);
  } else if (!args->isAttributeAccessForVariable(attributeAccess, true)) {
    return false;  // no attribute access, no index check
  }
  TRI_ASSERT(attributeAccess.first != nullptr);
  ExecutionNode* setter = plan->getVarSetBy(attributeAccess.first->id);
  if (setter == nullptr || setter->getType() != EN::ENUMERATE_COLLECTION) {
    return false;  // expected access of the for doc.attribute
  }

  // get logical collection
  auto collNode = reinterpret_cast<EnumerateCollectionNode*>(setter);
  if (info.collectionNodeToReplace != nullptr &&
      info.collectionNodeToReplace != collNode) {
    return false;  // should probably never happen
  }
  info.collectionNodeToReplace = collNode;
  info.collectionNodeOutVar = collNode->outVariable();
  info.collection = collNode->collection();
  std::shared_ptr<LogicalCollection> coll =
      collNode->collection()->getCollection();

  // check for suitable indexes
  for (std::shared_ptr<arangodb::Index> idx : coll->getIndexes()) {
    // check if current index is a geo-index
    bool isGeo =
        idx->type() == arangodb::Index::IndexType::TRI_IDX_TYPE_GEO_INDEX;
    if (isGeo && idx->fields().size() == 1) {  // individual fields
      // check access paths of attributes in ast and those in index match
      if (idx->fields()[0] == attributeAccess.second) {
        if (info.index != nullptr && info.index != idx) {
          return false;  // different index
        }
        info.index = idx;
        info.locationVar = args;
        return true;
      }
    }
  }  // for index in collection
  return false;
}

/// returns true if left side is same as right or lhs is null
static bool isValidGeoArg(AstNode const* lhs, AstNode const* rhs) {
  if (lhs == nullptr) {  // lhs is from the GeoIndexInfo struct
    return true;         // if geoindex field is null everything is valid
  } else if (lhs->type != rhs->type) {
    return false;
  } else if (lhs->isArray()) {  // expect `[doc.lng, doc.lat]`
    if (lhs->numMembers() >= 2 && rhs->numMembers() >= 2) {
      return isValidGeoArg(lhs->getMemberUnchecked(0),
                           rhs->getMemberUnchecked(0)) &&
             isValidGeoArg(lhs->getMemberUnchecked(1),
                           rhs->getMemberUnchecked(1));
    }
    return false;
  } else if (lhs->type == NODE_TYPE_REFERENCE) {
    return static_cast<Variable const*>(lhs->getData())->id ==
           static_cast<Variable const*>(rhs->getData())->id;
  }
  // CompareAstNodes does not handle non const attribute access
  std::pair<Variable const*, std::vector<arangodb::basics::AttributeName>> res1,
      res2;
  bool acc1 = lhs->isAttributeAccessForVariable(res1, true);
  bool acc2 = rhs->isAttributeAccessForVariable(res2, true);
  if (acc1 || acc2) {
    return acc1 && acc2 && res1 == res2;  // same variable same path
  }
  return aql::CompareAstNodes(lhs, rhs, false) == 0;
}

static bool checkDistanceFunc(ExecutionPlan* plan, AstNode const* funcNode,
                              bool legacy, GeoIndexInfo& info) {
  if (funcNode->type == NODE_TYPE_REFERENCE) {
    // FOR x IN cc LET d = DISTANCE(...) FILTER d > 10 RETURN x
    Variable const* var = static_cast<Variable const*>(funcNode->getData());
    TRI_ASSERT(var != nullptr);
    ExecutionNode* setter = plan->getVarSetBy(var->id);
    if (setter == nullptr || setter->getType() != EN::CALCULATION) {
      return false;
    }
    funcNode =
        ExecutionNode::castTo<CalculationNode*>(setter)->expression()->node();
  }
  // get the ast node of the expression
  if (!funcNode || funcNode->type != NODE_TYPE_FCALL ||
      funcNode->numMembers() != 1) {
    return false;
  }
  AstNode* fargs = funcNode->getMemberUnchecked(0);
  auto func = static_cast<Function const*>(funcNode->getData());
  if (fargs->numMembers() >= 4 &&
      func->name == "DISTANCE") {  // allow DISTANCE(a,b,c,d)
    if (info.distCenterExpr != nullptr) {
      return false;  // do not allow mixing of DISTANCE and GEO_DISTANCE
    }
    if (isValidGeoArg(info.distCenterLatExpr, fargs->getMemberUnchecked(2)) &&
        isValidGeoArg(info.distCenterLngExpr, fargs->getMemberUnchecked(3)) &&
        distanceFuncArgCheck(plan, fargs->getMemberUnchecked(0),
                             fargs->getMemberUnchecked(1), legacy, info)) {
      info.distCenterLatExpr = fargs->getMemberUnchecked(2);
      info.distCenterLngExpr = fargs->getMemberUnchecked(3);
      return true;
    } else if (isValidGeoArg(info.distCenterLatExpr,
                             fargs->getMemberUnchecked(0)) &&
               isValidGeoArg(info.distCenterLngExpr,
                             fargs->getMemberUnchecked(1)) &&
               distanceFuncArgCheck(plan, fargs->getMemberUnchecked(2),
                                    fargs->getMemberUnchecked(3), legacy,
                                    info)) {
      info.distCenterLatExpr = fargs->getMemberUnchecked(0);
      info.distCenterLngExpr = fargs->getMemberUnchecked(1);
      return true;
    }
  } else if (fargs->numMembers() == 2 && func->name == "GEO_DISTANCE") {
    if (info.distCenterLatExpr || info.distCenterLngExpr) {
      return false;  // do not allow mixing of DISTANCE and GEO_DISTANCE
    }
    if (isValidGeoArg(info.distCenterExpr, fargs->getMemberUnchecked(1)) &&
        geoFuncArgCheck(plan, fargs->getMemberUnchecked(0), legacy, info)) {
      info.distCenterExpr = fargs->getMemberUnchecked(1);
      return true;
    } else if (isValidGeoArg(info.distCenterExpr,
                             fargs->getMemberUnchecked(0)) &&
               geoFuncArgCheck(plan, fargs->getMemberUnchecked(1), legacy,
                               info)) {
      info.distCenterExpr = fargs->getMemberUnchecked(0);
      return true;
    }
  }
  return false;
}

// contains the AstNode* a supported function?
static bool checkGeoFilterFunction(ExecutionPlan* plan, AstNode const* funcNode,
                                   GeoIndexInfo& info) {
  // the expression must exist and it must be a function call
  if (funcNode->type != NODE_TYPE_FCALL || funcNode->numMembers() != 1 ||
      info.filterMode != geo::FilterType::NONE) {  // can't handle more than one
    return false;
  }

  auto func = static_cast<Function const*>(funcNode->getData());
  AstNode* fargs = funcNode->getMemberUnchecked(0);
  bool contains = func->name == "GEO_CONTAINS";
  bool intersect = func->name == "GEO_INTERSECTS";
  if ((!contains && !intersect) || fargs->numMembers() != 2) {
    return false;
  }

  AstNode* arg = fargs->getMemberUnchecked(1);
  if (geoFuncArgCheck(plan, arg, /*legacy*/ true, info)) {
    TRI_ASSERT(contains || intersect);
    info.filterMode =
        contains ? geo::FilterType::CONTAINS : geo::FilterType::INTERSECTS;
    info.filterExpr = fargs->getMemberUnchecked(0);
    TRI_ASSERT(info.index);
    return true;
  }
  return false;
}

// checks if a node contanis a geo index function a valid operator
// to use within a filter condition
bool checkGeoFilterExpression(ExecutionPlan* plan, AstNode const* node,
                              GeoIndexInfo& info) {
  // checks @first `smaller` @second
  auto eval = [&](AstNode const* first, AstNode const* second,
                  bool lessequal) -> bool {
    if (isValueOrReference(second) &&       // no attribute access
        info.maxDistanceExpr == nullptr &&  // max distance is not yet set
        checkDistanceFunc(plan, first, /*legacy*/ true, info)) {
      TRI_ASSERT(info.index);
      info.maxDistanceExpr = second;
      info.maxInclusive = info.maxInclusive && lessequal;
      info.nodesToRemove.insert(node);
      return true;
    } else if (isValueOrReference(first) &&  // no attribute access
               info.minDistanceExpr ==
                   nullptr &&  // min distance is not yet set
               checkDistanceFunc(plan, second, /*legacy*/ true, info)) {
      info.minDistanceExpr = first;
      info.minInclusive = info.minInclusive && lessequal;
      info.nodesToRemove.insert(node);
      return true;
    }
    return false;
  };

  switch (node->type) {
    case NODE_TYPE_FCALL:
      if (checkGeoFilterFunction(plan, node, info)) {
        info.nodesToRemove.insert(node);
        return true;
      }
      return false;
      break;
    // only DISTANCE is allowed with <=, <, >=, >
    case NODE_TYPE_OPERATOR_BINARY_LE:
      TRI_ASSERT(node->numMembers() == 2);
      return eval(node->getMember(0), node->getMember(1), true);
      break;
    case NODE_TYPE_OPERATOR_BINARY_LT:
      TRI_ASSERT(node->numMembers() == 2);
      return eval(node->getMember(0), node->getMember(1), false);
      break;
    case NODE_TYPE_OPERATOR_BINARY_GE:
      TRI_ASSERT(node->numMembers() == 2);
      return eval(node->getMember(1), node->getMember(0), true);
    case NODE_TYPE_OPERATOR_BINARY_GT:
      TRI_ASSERT(node->numMembers() == 2);
      return eval(node->getMember(1), node->getMember(0), false);
      break;
    default:
      return false;
  }
}

static bool optimizeSortNode(ExecutionPlan* plan, SortNode* sort,
                             GeoIndexInfo& info) {
  TRI_ASSERT(sort->getType() == EN::SORT);
  // we're looking for "SORT DISTANCE(x,y,a,b)"
  SortElementVector const& elements = sort->elements();
  if (elements.size() != 1) {  // can't do it
    return false;
  }
  TRI_ASSERT(elements[0].var != nullptr);

  // find the expression that is bound to the variable
  // get the expression node that holds the calculation
  ExecutionNode* setter = plan->getVarSetBy(elements[0].var->id);
  if (setter == nullptr || setter->getType() != EN::CALCULATION) {
    return false;  // setter could be enumerate list node e.g.
  }
  CalculationNode* calc = ExecutionNode::castTo<CalculationNode*>(setter);
  Expression* expr = calc->expression();
  if (expr == nullptr || expr->node() == nullptr) {
    return false;  // the expression must exist and must have an astNode
  }

  bool legacy = elements[0].ascending;  // DESC is only supported on S2 index
  if (!info.sorted && checkDistanceFunc(plan, expr->node(), legacy, info)) {
    info.sorted = true;  // do not parse another SORT
    info.ascending = elements[0].ascending;
    if (!ServerState::instance()->isCoordinator()) {
      // we must not remove a sort in the cluster... the results from each
      // shard will be sorted by using the index, however we still need to
      // establish a cross-shard sortedness by distance.
      info.exesToModify.emplace(sort, expr);
      info.nodesToRemove.emplace(expr->node());
    }
    return true;
  }
  return false;
}

// checks a single sort or filter node
static void optimizeFilterNode(ExecutionPlan* plan, FilterNode* fn,
                               GeoIndexInfo& info) {
  TRI_ASSERT(fn->getType() == EN::FILTER);

  // filter nodes always have one input variable
  auto varsUsedHere = fn->getVariablesUsedHere();
  TRI_ASSERT(varsUsedHere.size() == 1);  // does the optimizer do this?
  // now check who introduced our variable
  ExecutionNode* setter = plan->getVarSetBy(varsUsedHere[0]->id);
  if (setter == nullptr || setter->getType() != EN::CALCULATION) {
    return;  // setter could be enumerate list node e.g.
  }
  CalculationNode* calc = ExecutionNode::castTo<CalculationNode*>(setter);
  Expression* expr = calc->expression();
  if (expr == nullptr || expr->node() == nullptr) {
    return;  // the expression must exist and must have an astNode
  }

  std::vector<AstNodeType> parents;  // parents and current node
  size_t orsInBranch = 0;
  Ast::traverseReadOnly(
      expr->node(),
      [&](AstNode const* node) {  // pre
        parents.push_back(node->type);
        if (Ast::IsOrOperatorType(node->type)) {
          orsInBranch++;
          return false;
        }
        return true;
      },
      [&](AstNode const* node) {  // post
        size_t pl = parents.size();
        if (orsInBranch == 0 &&
            (pl == 1 || Ast::IsAndOperatorType(parents[pl - 2]))) {
          // do not visit below OR or into <=, <, >, >= expressions
          if (checkGeoFilterExpression(plan, node, info)) {
            info.exesToModify.emplace(fn, expr);
          }
        }
        parents.pop_back();
        if (Ast::IsOrOperatorType(node->type)) {
          orsInBranch--;
        }
      });
}

// modify plan

// builds a condition that can be used with the index interface and
// contains all parameters required by the MMFilesGeoIndex
static std::unique_ptr<Condition> buildGeoCondition(ExecutionPlan* plan,
                                                    GeoIndexInfo const& info) {
  Ast* ast = plan->getAst();
  // shared code to add symbolic `doc.geometry` or `[doc.lng, doc.lat]`
  auto addLocationArg = [ast, &info](AstNode* args) {
    if (info.locationVar) {
      args->addMember(info.locationVar);
    } else if (info.latitudeVar && info.longitudeVar) {
      AstNode* array = ast->createNodeArray(2);
      array->addMember(info.longitudeVar);  // GeoJSON ordering
      array->addMember(info.latitudeVar);
      args->addMember(array);
    } else {
      TRI_ASSERT(false);
      THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
    }
  };

  TRI_ASSERT(info.index);
  auto cond = std::make_unique<Condition>(ast);
  bool hasCenter = info.distCenterLatExpr || info.distCenterExpr;
  bool hasDistLimit = info.maxDistanceExpr || info.minDistanceExpr;
  TRI_ASSERT(!hasCenter || hasDistLimit || info.sorted);
  if (hasCenter && (hasDistLimit || info.sorted)) {
    // create GEO_DISTANCE(...) [<|<=|>=|>] Var
    AstNode* args = ast->createNodeArray(2);
    if (info.distCenterLatExpr && info.distCenterLngExpr) {  // legacy
      TRI_ASSERT(!info.distCenterExpr);
      // info.sorted && info.ascending &&
      AstNode* array = ast->createNodeArray(2);
      array->addMember(info.distCenterLngExpr);  // GeoJSON ordering
      array->addMember(info.distCenterLatExpr);
      args->addMember(array);
    } else {
      TRI_ASSERT(info.distCenterExpr);
      TRI_ASSERT(!info.distCenterLatExpr && !info.distCenterLngExpr);
      args->addMember(info.distCenterExpr);  // center location
    }

    addLocationArg(args);
    AstNode* func =
        ast->createNodeFunctionCall(TRI_CHAR_LENGTH_PAIR("GEO_DISTANCE"), args);

    TRI_ASSERT(info.maxDistanceExpr || info.minDistanceExpr || info.sorted);
    if (info.minDistanceExpr != nullptr) {
      AstNodeType t = info.minInclusive ? NODE_TYPE_OPERATOR_BINARY_GE
                                        : NODE_TYPE_OPERATOR_BINARY_GT;
      cond->andCombine(
          ast->createNodeBinaryOperator(t, func, info.minDistanceExpr));
    }
    if (info.maxDistanceExpr != nullptr) {
      AstNodeType t = info.maxInclusive ? NODE_TYPE_OPERATOR_BINARY_LE
                                        : NODE_TYPE_OPERATOR_BINARY_LT;
      cond->andCombine(
          ast->createNodeBinaryOperator(t, func, info.maxDistanceExpr));
    }
    if (info.minDistanceExpr == nullptr && info.maxDistanceExpr == nullptr &&
        info.sorted) {
      // hack to pass on the sort-to-point info
      AstNodeType t = NODE_TYPE_OPERATOR_BINARY_LT;
      std::string const& u = StaticStrings::Unlimited;
      AstNode* cc = ast->createNodeValueString(u.c_str(), u.length());
      cond->andCombine(ast->createNodeBinaryOperator(t, func, cc));
    }
  }
  if (info.filterMode != geo::FilterType::NONE) {
    // create GEO_CONTAINS / GEO_INTERSECTS
    TRI_ASSERT(info.filterExpr);
    TRI_ASSERT(info.locationVar || (info.longitudeVar && info.latitudeVar));

    AstNode* args = ast->createNodeArray(2);
    args->addMember(info.filterExpr);
    addLocationArg(args);
    if (info.filterMode == geo::FilterType::CONTAINS) {
      cond->andCombine(ast->createNodeFunctionCall("GEO_CONTAINS", args));
    } else if (info.filterMode == geo::FilterType::INTERSECTS) {
      cond->andCombine(ast->createNodeFunctionCall("GEO_INTERSECTS", args));
    } else {
      TRI_ASSERT(false);
    }
  }

  cond->normalize(plan);
  return cond;
}

// applys the optimization for a candidate
static bool applyGeoOptimization(ExecutionPlan* plan, LimitNode* ln,
                                 GeoIndexInfo const& info) {
  TRI_ASSERT(info.collection != nullptr);
  TRI_ASSERT(info.collectionNodeToReplace != nullptr);
  TRI_ASSERT(info.index);

  // verify that all vars used in the index condition are valid
  auto const& valid = info.collectionNodeToReplace->getVarsValid();
  auto checkVars = [&valid](AstNode const* expr) {
    if (expr != nullptr) {
      std::unordered_set<Variable const*> varsUsed;
      Ast::getReferencedVariables(expr, varsUsed);
      for (Variable const* v : varsUsed) {
        if (valid.find(v) == valid.end()) {
          return false;  // invalid variable foud
        }
      }
    }
    return true;
  };
  if (!checkVars(info.distCenterExpr) || !checkVars(info.distCenterLatExpr) ||
      !checkVars(info.distCenterLngExpr) || !checkVars(info.filterExpr)) {
    return false;
  }

  size_t limit = 0;
  if (ln != nullptr) {
    limit = ln->offset() + ln->limit();
    TRI_ASSERT(limit != SIZE_MAX);
  }

  IndexIteratorOptions opts;
  opts.sorted = info.sorted;
  opts.ascending = info.ascending;
  // opts.fullRange = info.fullRange;
  opts.limit = limit;
  opts.evaluateFCalls = false;  // workaround to avoid evaluating "doc.geo"
  std::unique_ptr<Condition> condition(buildGeoCondition(plan, info));
  auto inode = new IndexNode(plan, plan->nextId(), info.collection,
                             info.collectionNodeOutVar,
                             std::vector<transaction::Methods::IndexHandle>{
                                 transaction::Methods::IndexHandle{info.index}},
                             std::move(condition), opts);
  plan->registerNode(inode);
  plan->replaceNode(info.collectionNodeToReplace, inode);

  // remove expressions covered by our index
  Ast* ast = plan->getAst();
  for (std::pair<ExecutionNode*, Expression*> pair : info.exesToModify) {
    AstNode* root = pair.second->nodeForModification();
    auto pre = [&](AstNode const* node) -> bool {
      return node == root || Ast::IsAndOperatorType(node->type);
    };
    auto visitor = [&](AstNode* node) -> AstNode* {
      if (Ast::IsAndOperatorType(node->type)) {
        std::vector<AstNode*> keep;  // always shallow copy node
        for (std::size_t i = 0; i < node->numMembers(); i++) {
          AstNode* child = node->getMemberUnchecked(i);
          if (info.nodesToRemove.find(child) == info.nodesToRemove.end()) {
            keep.push_back(child);
          }
        }

        if (keep.size() > 2) {
          AstNode* n = ast->createNodeNaryOperator(NODE_TYPE_OPERATOR_NARY_AND);
          for (size_t i = 0; i < keep.size(); i++) {
            n->addMember(keep[i]);
          }
          return n;
        } else if (keep.size() == 2) {
          return ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_AND,
                                               keep[0], keep[1]);
        } else if (keep.size() == 1) {
          return keep[0];
        }
        return node == root ? nullptr : ast->createNodeValueBool(true);
      } else if (info.nodesToRemove.find(node) != info.nodesToRemove.end()) {
        return node == root ? nullptr : ast->createNodeValueBool(true);
      }
      return node;
    };
    auto post = [](AstNode const*) {};
    AstNode* newNode = Ast::traverseAndModify(root, pre, visitor, post);
    if (newNode == nullptr) {  // if root was removed, unlink FILTER or SORT
      plan->unlinkNode(pair.first);
    } else if (newNode != root) {
      pair.second->replaceNode(newNode);
    }
  }

  // signal that plan has been changed
  return true;
}

void arangodb::aql::geoIndexRule(Optimizer* opt,
                                 std::unique_ptr<ExecutionPlan> plan,
                                 OptimizerRule const* rule) {
  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};
  bool mod = false;

  plan->findNodesOfType(nodes, EN::ENUMERATE_COLLECTION, true);
  for (ExecutionNode* node : nodes) {
    GeoIndexInfo info;
    ExecutionNode* current = node->getFirstParent();
    LimitNode* limit = nullptr;

    while (current) {
      if (current->getType() == EN::SORT) {
        if (!optimizeSortNode(
                plan.get(), ExecutionNode::castTo<SortNode*>(current), info)) {
          // 1. EnumerateCollectionNode x
          // 2. SortNode x.abc ASC
          // 3. LimitNode n,m  <-- cannot reuse LIMIT node here
          // limit = nullptr;
          break;  // stop parsing on non-optimizable SORT
        }
      } else if (current->getType() == EN::FILTER) {
        optimizeFilterNode(plan.get(),
                           ExecutionNode::castTo<FilterNode*>(current), info);
      } else if (current->getType() == EN::LIMIT) {
        limit = ExecutionNode::castTo<LimitNode*>(current);
        break;  // stop parsing after first LIMIT
      } else if (current->getType() == EN::RETURN) {
        break;  // stop parsing on return
      } else if (current->getType() == EN::INDEX ||
                 current->getType() == EN::COLLECT) {
        info.invalidate();
        break;  // unsupported
      }
      current = current->getFirstParent();  // inspect next node
    }

    // if info is valid we try to optimize ENUMERATE_COLLECTION
    if (info && info.collectionNodeToReplace == node) {
      mod = mod || applyGeoOptimization(plan.get(), limit, info);
    }
  }

  opt->addPlan(std::move(plan), rule, mod);
}

void arangodb::aql::optimizeSubqueriesRule(Optimizer* opt,
                                           std::unique_ptr<ExecutionPlan> plan,
                                           OptimizerRule const* rule) {
  bool modified = false;

  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, EN::CALCULATION, true);

  std::unordered_map<
      ExecutionNode*,
      std::tuple<int64_t, std::unordered_set<ExecutionNode const*>, bool>>
      subqueryAttributes;

  for (auto const& n : nodes) {
    auto cn = ExecutionNode::castTo<CalculationNode*>(n);
    auto expr = cn->expression();
    if (expr == nullptr) {
      continue;
    }

    AstNode const* root = expr->node();
    if (root == nullptr) {
      continue;
    }

    auto visitor = [&subqueryAttributes, &plan,
                    n](AstNode const* node) -> bool {
      std::pair<ExecutionNode*, int64_t> found{nullptr, 0};
      bool usedForCount = false;

      if (node->type == NODE_TYPE_REFERENCE) {
        Variable const* v = static_cast<Variable const*>(node->getData());
        auto setter = plan->getVarSetBy(v->id);
        if (setter != nullptr && setter->getType() == EN::SUBQUERY) {
          // we found a subquery result being used somehow in some
          // way that will make the optimization produce wrong results
          found.first = setter;
          found.second = -1;  // negative values will disable the optimization
        }
      } else if (node->type == NODE_TYPE_INDEXED_ACCESS) {
        auto sub = node->getMemberUnchecked(0);
        if (sub->type == NODE_TYPE_REFERENCE) {
          Variable const* v = static_cast<Variable const*>(sub->getData());
          auto setter = plan->getVarSetBy(v->id);
          auto index = node->getMemberUnchecked(1);
          if (index->type == NODE_TYPE_VALUE && index->isNumericValue() &&
              setter != nullptr && setter->getType() == EN::SUBQUERY) {
            found.first = setter;
            found.second = index->getIntValue() + 1;  // x[0] => LIMIT 1
            if (found.second <= 0) {
              // turn optimization off
              found.second = -1;
            }
          }
        }
      } else if (node->type == NODE_TYPE_FCALL && node->numMembers() > 0) {
        auto func = static_cast<Function const*>(node->getData());
        auto args = node->getMember(0);
        if (func->name == "FIRST" || func->name == "LENGTH" ||
            func->name == "COUNT") {
          if (args->numMembers() > 0 &&
              args->getMember(0)->type == NODE_TYPE_REFERENCE) {
            Variable const* v =
                static_cast<Variable const*>(args->getMember(0)->getData());
            auto setter = plan->getVarSetBy(v->id);
            if (setter != nullptr && setter->getType() == EN::SUBQUERY) {
              found.first = setter;
              if (func->name == "FIRST") {
                found.second = 1;  // FIRST(x) => LIMIT 1
              } else {
                found.second = -1;
                usedForCount = true;
              }
            }
          }
        }
      }

      if (found.first != nullptr) {
        auto it = subqueryAttributes.find(found.first);
        if (it == subqueryAttributes.end()) {
          subqueryAttributes.emplace(
              found.first,
              std::make_tuple(found.second,
                              std::unordered_set<ExecutionNode const*>{n},
                              usedForCount));
        } else {
          auto& sq = (*it).second;
          if (usedForCount) {
            // COUNT + LIMIT together will turn off the optimization
            std::get<2>(sq) = (std::get<0>(sq) <= 0);
            std::get<0>(sq) = -1;
            std::get<1>(sq).clear();
          } else {
            if (found.second <= 0 || std::get<0>(sq) < 0) {
              // negative value will turn off the optimization
              std::get<0>(sq) = -1;
              std::get<1>(sq).clear();
            } else {
              // otherwise, use the maximum of the limits needed, and insert
              // current node into our "safe" list
              std::get<0>(sq) = std::max(std::get<0>(sq), found.second);
              std::get<1>(sq).emplace(n);
            }
            std::get<2>(sq) = false;
          }
        }
        // don't descend further
        return false;
      }

      // descend further
      return true;
    };

    Ast::traverseReadOnly(root, visitor, [](AstNode const*) {});
  }

  for (auto const& it : subqueryAttributes) {
    ExecutionNode* node = it.first;
    TRI_ASSERT(node->getType() == EN::SUBQUERY);
    auto sn = ExecutionNode::castTo<SubqueryNode const*>(node);

    if (sn->isModificationSubquery()) {
      // cannot push a LIMIT into data-modification subqueries
      continue;
    }

    auto const& sq = it.second;
    int64_t limitValue = std::get<0>(sq);
    bool usedForCount = std::get<2>(sq);
    if (limitValue <= 0 && !usedForCount) {
      // optimization turned off
      continue;
    }

    // scan from the subquery node to the bottom of the ExecutionPlan to check
    // if any of the following nodes also use the subquery result
    auto out = sn->outVariable();
    std::unordered_set<Variable const*> used;
    bool invalid = false;

    auto current = node->getFirstParent();
    while (current != nullptr) {
      auto const& referencedBy = std::get<1>(sq);
      if (referencedBy.find(current) == referencedBy.end()) {
        // node not found in "safe" list
        // now check if it uses the subquery's out variable
        used.clear();
        current->getVariablesUsedHere(used);
        if (used.find(out) != used.end()) {
          invalid = true;
          break;
        }
      }
      // continue iteration
      current = current->getFirstParent();
    }

    if (invalid) {
      continue;
    }

    auto root = sn->getSubquery();
    if (root != nullptr && root->getType() == EN::RETURN) {
      // now inject a limit
      auto f = root->getFirstDependency();
      TRI_ASSERT(f != nullptr);

      if (std::get<2>(sq)) {
        // used for count, e.g. COUNT(FOR doc IN collection RETURN ...)
        // this will be turned into
        // COUNT(FOR doc IN collection RETURN 1)
        Ast* ast = plan->getAst();
        // generate a calculation node that only produces "true"
        auto expr = std::make_unique<Expression>(
            plan.get(), ast, Ast::createNodeValueBool(true));
        Variable* outVariable = ast->variables()->createTemporaryVariable();
        auto calcNode = new CalculationNode(plan.get(), plan->nextId(),
                                            expr.get(), nullptr, outVariable);
        plan->registerNode(calcNode);
        expr.release();
        plan->insertAfter(f, calcNode);
        // change the result value of the existing Return node
        TRI_ASSERT(root->getType() == EN::RETURN);
        ExecutionNode::castTo<ReturnNode*>(root)->inVariable(outVariable);
        modified = true;
        continue;
      }

      if (f->getType() == EN::LIMIT) {
        // subquery already has a LIMIT node at its end
        // no need to do anything
        continue;
      }

      auto limitNode = new LimitNode(plan.get(), plan->nextId(), 0, limitValue);
      plan->registerNode(limitNode);
      plan->insertAfter(f, limitNode);
      modified = true;
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}
